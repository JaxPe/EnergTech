// ============================================================
//  SLAVE
//  Hardware: Arduino NANO
//  Rev: E  |  Date: 2026-03-30
//
//  MAPEAMENTO DE CONEXÕES (apartir da esquemática V6):
//    D2  (05) - OneWire DS18B20 sensor temperatura
//    D3  (06) - FLT 1
//    D4  (07) - FLT 2
//    D5  (08) - FLT 3
//    D6  (09) - FLT 4
//    D7  (10) - FLT 5
//    D8  (11) - FLT 6
//    D9  (12) - FLT 7
//    D10 (13) - FLT 8
//    D13 (14) - FLT 9
//    A0  (19) - ENDEREÇO bit 0
//    A1  (20) - ENDEREÇO bit 1
//    A2  (21) - ENDEREÇO bit 2
//    A3  (22) - ENDEREÇO bit 3
//    A4  (23) - ENDEREÇO bit 4
//    A5  (24) - MODO DE OPERAÇÃO (open=full, shorted=temp only)
//    TX  (01) - conversor RS485 TXD
//    RX  (02) - conversor RS485 RXD
//
//  MODOS OPERACIONAIS (DIP switch A5 / MOD OP):
//    0 = Full: medição de vazão (9 flautas) + temperatura
//    1 = Somente temperatura
//
//  COMANDOS (RS485 bus, <CR>):
//    READ A<CR>        - TODOS os dispositivos executam a rotina de medição de vazão
//    SEND <addr><CR>   - dispositivo <addr> envia temperatura + flow data
//    Tmax <val><CR>    - Define janela de tempo máxima para medição de vazão (ms)  Default: 2000
//    Tmin <val><CR>    - Define tempo mínimo para vazão (ms)                       Default: 500
//    TRATE <val><CR>   - Define amostras de temperatura por minuto (padrão = 12)
//
//  RESPOSTA para "SEND <addr>":
//    <temperatura>,<obstruido_HEX>,<vazao_HEX><CR><LF>
//    Example:  22.50,01FF,0000
//
//    obstruido_HEX (16-bit):
//      posição do Bit = número da flauta (0=FLT1 ... 8=FLT9)
//      Bit 0 = saída OBSTRUIDA
//      Bit 1 = saída OK
//
//    vazao_HEX (16-bit):
//      posição do Bit = número da flauta (0=FLT1 ... 8=FLT9)
//      Bit 1 = vazao BAIXA (não acionou dentro do Tmax)
//      Bit 0 = vazao OK
//
//  TEMPERATURA:
//    Amostras coletadas continuamente em backgroun em uma frequencia de TRATE por segundo.
//    Média móvel das últimas 10 medições.
//    Leituras que desviam mais que 2°C da média atual são decartadas.
//    Completamente independente do rotina de vazão.
// ============================================================

#include <SoftwareSerial.h>
#include <OneWire.h>
#include <DallasTemperature.h>

// ==== Definição de pins ========================
#define TEMP_PIN 2
#define NUM_FLAUTAS 9
const uint8_t FLT_PINS[NUM_FLAUTAS] = {3, 4, 5, 6, 7, 8, 9, 10, 13};

#define ADDR_PIN_0 A0
#define ADDR_PIN_1 A1
#define ADDR_PIN_2 A2
#define ADDR_PIN_3 A3
#define ADDR_PIN_4 A4
#define MODE_PIN A5

// ==== Tempo padrão (ms) ====================
#define DEFAULT_FLOW_TIME_MAX 2000
#define DEFAULT_FLOW_TIME_MIN 500
#define DEFAULT_PURGE_TIME_MAX 1000
#define DEFAULT_PURGE_TIME_MIN 300

// ==== Temperature moving average =============
#define DEFAULT_TEMP_SAMPLE_RATE 30 // leituras por minuto
#define TEMP_AVG_SIZE 10
#define TEMP_MAX_DELTA 2.0f // °C - descarta leituras fora desta margem
#define DEFAULT_TEMP_ADJ 0.0f

#define STATUS_UNINIT 0xFFF

OneWire oneWire(TEMP_PIN);
DallasTemperature tempSensor(&oneWire);
DeviceAddress tempAddress;

SoftwareSerial masterSerial(12, 11);

int _device_id = 0;
int _op_mode = 0; // 0=completo, 1=temp somente

// Tempo de vazão
unsigned long _flowTimeMax = DEFAULT_FLOW_TIME_MAX;
unsigned long _flowTimeMin = DEFAULT_FLOW_TIME_MIN;

// Tempo de purga
unsigned long _purgeTimeMax = DEFAULT_PURGE_TIME_MAX;
unsigned long _purgeTimeMin = DEFAULT_PURGE_TIME_MIN;

// Amostragem de temperatura
unsigned long _tempSampleRate = DEFAULT_TEMP_SAMPLE_RATE;
unsigned long _tempSampleInterval = 60000UL / DEFAULT_TEMP_SAMPLE_RATE;
unsigned long _lastTempSample = 0;

// Buffer circular de temperatura
float _tempBuffer[TEMP_AVG_SIZE];
int _tempIndex = 0;
int _tempCount = 0;
float _temperature = -99.0;

float _tempAdj = DEFAULT_TEMP_ADJ;

// Resultado de medições
uint16_t _clogStatus = STATUS_UNINIT;
uint16_t _flowStatus = STATUS_UNINIT;
uint16_t _purgeStatus = STATUS_UNINIT;

// Máquina de estado de vazão
enum FLowState
{
  FLOW_IDLE,
  FLOW_CLOG_CHECK,
  FLOW_RUNNING,
  FLOW_DONE
};

FLowState _flowState = FLOW_IDLE;
unsigned long _flowStartTime = 0;
bool _flowPinDone[NUM_FLAUTAS] = {false};
bool _flowClogged[NUM_FLAUTAS] = {false};
unsigned long _flowSwitchTime[NUM_FLAUTAS] = {0};

// Máquina de estado da purga
enum PurgeState
{
  PURGE_IDLE,
  PURGE_RUNNING,
  PURGE_DONE
};

PurgeState _purgeState = PURGE_IDLE;
unsigned long _purgeStartTime = 0;
bool _purgePinDone[NUM_FLAUTAS] = {false};
unsigned long _purgeSwitchTime[NUM_FLAUTAS] = {0};

// Buffer de serial recebido
String _rxBuffer = "";

bool isBusy()
{
  return _flowState != FLOW_IDLE || _purgeState != PURGE_IDLE;
}

void setup()
{
  pinMode(ADDR_PIN_0, INPUT_PULLUP);
  pinMode(ADDR_PIN_1, INPUT_PULLUP);
  pinMode(ADDR_PIN_2, INPUT_PULLUP);
  pinMode(ADDR_PIN_3, INPUT_PULLUP);
  pinMode(ADDR_PIN_4, INPUT_PULLUP);
  pinMode(MODE_PIN, INPUT_PULLUP);

  for (int i = 0; i < NUM_FLAUTAS; i++)
  {
    pinMode(FLT_PINS[i], INPUT_PULLUP);
  }

  tempSensor.begin();
  Serial.begin(9600);
  masterSerial.begin(9600);

  _device_id = readAddress();
  Serial.print("Endereço: ");
  Serial.println(_device_id);
  _op_mode = !digitalRead(MODE_PIN);
  Serial.print("Modo de operação: ");
  Serial.println(_op_mode);

  Serial.println("Iniciado! Coletando amostra inicial de temperatura");
  initTemperatureBuffer();
}

void loop()
{
  readSerialCommands();
  runFlowStateMachine();
  runPurgeStateMachine();
  sampleTemperatureContinuous();
}

// Lê endereço 5-bit do DIP switch (A0-A4)
int readAddress()
{
  int b0 = !digitalRead(ADDR_PIN_4);
  int b1 = !digitalRead(ADDR_PIN_3);
  int b2 = !digitalRead(ADDR_PIN_2);
  int b3 = !digitalRead(ADDR_PIN_1);
  int b4 = !digitalRead(ADDR_PIN_0);
  return b0 + (b1 << 1) + (b2 << 2) + (b3 << 3) + (b4 << 4);
}

// Acumula caracteres do RS485 até <CR>
void readSerialCommands()
{
  while (Serial.available() > 0)
  {
    char c = (char)Serial.read();
    if (c == '\r')
    {
      _rxBuffer.trim();
      if (_rxBuffer.length() > 0)
      {
        handleCommand(_rxBuffer);
      }
      _rxBuffer = "";
    }
    else if (c != '\n')
    {
      _rxBuffer += c;
      Serial.println(_rxBuffer);
    }
    else if (c == '\n')
    {
      _rxBuffer = "";
    }
  }
}

// lida com os comandos
void handleCommand(String cmd)
{
  if (cmd == "StartAll")
  {
    Serial.println("Start");
    startFlowMeasurement();
  }
  else if (cmd == "StopAll")
  {
    startPurgeMeasurement();
  }

  else if (cmd.startsWith("Send "))
  {
    int addr = cmd.substring(5).toInt();
    if (addr == _device_id)
    {
      sendData();
    }
  }
  else if (cmd.startsWith("TimeVMax "))
  {
    _flowTimeMax = (unsigned long)cmd.substring(9).toInt();
    Serial.print("Tmax: ");
    Serial.println(_flowTimeMax);
  }
  else if (cmd.startsWith("TimeVMin "))
  {
    _flowTimeMin = (unsigned long)cmd.substring(9).toInt();
    Serial.print("Tmin: ");
    Serial.println(_flowTimeMin);
  }
  else if (cmd.startsWith("TimePMax"))
  {
    _purgeTimeMax = (unsigned long)cmd.substring(9).toInt();
  }
  else if (cmd.startsWith("TimePMin"))
  {
    _purgeTimeMin = (unsigned long)cmd.substring(9).toInt();
  }
  else if (cmd.startsWith("TempRate "))
  {
    unsigned long rate = (unsigned long)cmd.substring(6).toInt();
    if (rate > 0)
    {
      _tempSampleRate = rate;
      Serial.print("TRate: ");
      Serial.println(_tempSampleRate);
      _tempSampleInterval = 60000UL / _tempSampleRate;
    }
  }
  else if (cmd.startsWith("TempAdj "))
  {
    int addr = cmd.substring(9).toInt();
    if (addr == _device_id)
    {
      float adjust = (float)cmd.substring(11).toFloat();
      _tempAdj = adjust;
    }
  }
}

// Inicia a medição da vazão - acionado por "StartAll".
// Reseta entupimento + vazão e inicia a máquina de estado de vazão
void startFlowMeasurement()
{
  _clogStatus = 0xFFFF;
  _flowStatus = 0x0000;

  for (int i = 0; i < NUM_FLAUTAS; i++)
  {
    _flowPinDone[i] = false;
    _flowClogged[i] = false;
    _flowSwitchTime[i] = 0;
  }

  _flowState = FLOW_CLOG_CHECK;
}

// ============================================================
//  Flow state machine — called every loop().
//
//  FLOW_CLOG_CHECK : reads all pins once.
//                    LOW at start = clogged outlet (bit=0 in clogStatus).
//                    HIGH at start = outlet OK (bit=1 in clogStatus).
//                    Aborts to FLOW_IDLE if all clogged.
//  FLOW_RUNNING    : polls pending pins each loop() call.
//                    Records elapsed ms when pin goes LOW.
//                    Exits when all done or _flowTimeMax elapsed.
//  FLOW_DONE       : evaluates results once, sets _flowStatus,
//                    transitions to FLOW_IDLE.
// ============================================================
void runFlowStateMachine()
{
  switch (_flowState)
  {
  case FLOW_IDLE:
    break;
  case FLOW_CLOG_CHECK:
  {
    _clogStatus = 0x0000;
    int clogCount = 0;

    for (int i = 0; i < NUM_FLAUTAS; i++)
    {
      if (digitalRead(FLT_PINS[i]) == LOW)
      {
        _flowPinDone[i] = true;
        _flowClogged[i] = true;
        clogCount++;
      }
      else
      {
        _clogStatus |= (1 << i);
      }
    }

    if (clogCount == NUM_FLAUTAS)
    {
      _flowStatus = 0x01FF;
      _flowState = FLOW_IDLE;
    }
    else if (_op_mode == 0)
    {
      _flowStatus = 0x0000;
      _flowState = FLOW_IDLE;
    }
    else
    {
      _flowStartTime = millis();
      _flowState = FLOW_RUNNING;
      Serial.println("sem entupimento");
    }
    break;
  }

  case FLOW_RUNNING:
  {
    unsigned long elapsed = millis() - _flowStartTime;
    bool allDone = true;

    for (int i = 0; i < NUM_FLAUTAS; i++)
    {
      if (!_flowPinDone[i])
      {
        if (digitalRead(FLT_PINS[i]) == LOW)
        {
          _flowSwitchTime[i] = elapsed;
          _flowPinDone[i] = true;
          Serial.println("ativou");
        }
        else
        {
          allDone = false;
        }
      }
    }

    if (allDone || elapsed >= _flowTimeMax)
    {
      _flowState = FLOW_DONE;
      Serial.println("tempo limite");
    }

    break;
  }

  case FLOW_DONE:
  {
    _flowStatus = 0x0000;
    bool anyTriggeredOK = false;

    for (int i = 0; i < NUM_FLAUTAS; i++)
    {
      if (_flowClogged[i])
        continue;

      if (!_flowPinDone[i])
      {
        _flowStatus |= (1 << i);
      }
      else if (_flowSwitchTime[i] < _flowTimeMin)
      {
        Serial.println("Acionado antes do tempo");
        _flowStatus |= (1 << i);
      }
      else
      {
        anyTriggeredOK = true;
      }
    }

    if (!anyTriggeredOK)
    {
      _flowStatus = 0x01FF;
    }

    _flowState = FLOW_IDLE;
    break;
  }
  }
}

void startPurgeMeasurement()
{
  _purgeStatus = STATUS_UNINIT;

  for (int i = 0; i < NUM_FLAUTAS; i++)
  {
    _purgePinDone[i] = false;
    _purgeSwitchTime[i] = 0;
  }
}

// ============================================================
//  Purge state machine — called every loop().
//
//  After StopAll the pump stops. Reservoirs drain through the
//  purge outlets. Each flauta pin is LOW while the reservoir
//  is full (level switch triggered). As it drains the pin
//  goes HIGH (level switch released = liquid level dropped).
//
//  PURGE_RUNNING : polls all pending pins each loop() call.
//                  Records elapsed ms when pin goes HIGH.
//                  Exits when all done or _purgeTimeMax elapsed.
//  PURGE_DONE    : evaluates results once, sets _purgeStatus,
//                  transitions to PURGE_IDLE.
// ============================================================
void runPurgeStateMachine()
{
  switch (_purgeState)
  {
  case PURGE_IDLE:
    break;

  case PURGE_RUNNING:
  {
    unsigned long elapsed = millis() - _purgeStartTime;
    bool allDone = true;

    for (int i = 0; i < NUM_FLAUTAS; i++)
    {
      if (!_purgePinDone[i])
      {
        if (digitalRead(FLT_PINS[i]) == HIGH)
        {
          _purgeSwitchTime[i] = elapsed;
          _purgePinDone[i] = true;
        }
        else
        {
          allDone = false;
        }
      }
    }

    if (allDone || elapsed >= _purgeTimeMax)
    {
      _purgeState = PURGE_DONE;
    }

    break;
  }
  case PURGE_DONE:
  {
    _purgeStatus = 0x0000;

    for (int i = 0; i < NUM_FLAUTAS; i++)
    {
      if (!_purgePinDone[i])
      {
        _purgeStatus |= (1 << i);
      }
      else if (_purgeSwitchTime[i] < _purgeTimeMin)
      {
        _purgeStatus |= (1 << i);
      }
    }

    _purgeState = PURGE_IDLE;
    break;
  }
  }
}

void sendData()
{
  if (isBusy())
  {
    Serial.print("BUSY\r\n");
    return;
  }

  char buf[5];

  sprintf(buf, "%02d", _device_id);
  Serial.print(buf);

  Serial.print(",");
  Serial.print(_op_mode);

  Serial.print(",");
  if (_temperature <= -98.0)
    Serial.print("ERR");
  else
    Serial.print(_temperature, 1);

  if (_op_mode == 1)
  {
    Serial.print(",");
    sprintf(buf, "%04x", _clogStatus);
    Serial.print(buf);

    Serial.print(",");
    sprintf(buf, "%04x", _flowStatus);
    Serial.print(buf);

    Serial.print(",");
    sprintf(buf, "%04x", _purgeStatus);
    Serial.print(buf);
  }

  Serial.print("\r\n");
}

void sampleTemperatureContinuous()
{
  unsigned long now = millis();
  if (now - _lastTempSample >= _tempSampleInterval)
  {
    _lastTempSample = now;
    updateTemperature();
  }
}

void updateTemperature()
{
  tempSensor.requestTemperatures();

  if (!tempSensor.getAddress(tempAddress, 0))
  {
    _temperature = -99.0;
    return;
  }

  float reading = tempSensor.getTempC(tempAddress);

  if (_tempCount > 0)
  {
    float avg = computeAverage();
    if (fabsf(reading - avg) > TEMP_MAX_DELTA)
    {
      return;
    }
  }

  _tempBuffer[_tempIndex] = reading;
  _tempIndex = (_tempIndex + 1) % TEMP_AVG_SIZE;
  if (_tempCount < TEMP_AVG_SIZE)
  {
    _tempCount++;
  }
  _temperature = computeAverage();
}

float computeAverage()
{
  if (_tempCount == 0)
  {
    return 0.0;
  }
  float sum = 0.0;
  for (int i = 0; i < _tempCount; i++)
  {
    sum += _tempBuffer[i];
  }
  return sum / _tempCount;
}

void initTemperatureBuffer()
{
  tempSensor.requestTemperatures();

  if (!tempSensor.getAddress(tempAddress, 0))
  {
    return;
  }

  float first = tempSensor.getTempC(tempAddress);

  for (int i = 0; i < TEMP_AVG_SIZE; i++)
  {
    _tempBuffer[i] = first;
  }

  _tempCount = TEMP_AVG_SIZE;
  _tempIndex = 0;
  _temperature = first;
  Serial.println(_temperature);
}
