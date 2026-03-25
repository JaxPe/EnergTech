// ============================================================
//  SLAVE
//  Hardware: Arduino NANO
//  Rev: C  |  Date: 2026-03-11
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
//    D11 (14) - FLT 9
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
//    Tmax <val><CR>    - Define janela de tempo máxima para medição de vazão (ms)
//    Tmin <val><CR>    - Define tempo mínimo para vazão (ms)
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
#define FLT_FIRST_PIN 3
#define NUM_FLAUTAS 1

#define ADDR_PIN_0 A0
#define ADDR_PIN_1 A1
#define ADDR_PIN_2 A2
#define ADDR_PIN_3 A3
#define ADDR_PIN_4 A4
#define MODE_PIN A5

// ==== Tempo padrão (ms) ====================
#define DEFAULT_TMAX 2000
#define DEFAULT_TMIN 500

// ==== Temperature moving average =============
#define DEFAULT_TRATE 12 // leituras por minuto
#define TEMP_AVG_SIZE 10
#define TEMP_MAX_DELTA 2.0f // °C - descarta leituras fora desta margem

OneWire oneWire(TEMP_PIN);
DallasTemperature tempSensor(&oneWire);
DeviceAddress tempAddress;

int _device_id = 0;
int _op_mode = 0; // 0=completo, 1=temp somente

unsigned long _tmax = DEFAULT_TMAX;
unsigned long _tmin = DEFAULT_TMIN;
unsigned long _trate = DEFAULT_TRATE;

unsigned long _tempSampleInterval = 60000UL / DEFAULT_TRATE;
unsigned long _lastTempSample = 0;

// Resultado de medições
float _temperature = -99.0;
uint16_t _clog_status = 0xFFFF;
uint16_t _flow_status = 0x0000;
bool _measurement_done = false;

// Vazão
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

float _tempBuffer[TEMP_AVG_SIZE];
int _tempIndex = 0;
int _tempCount = 0;

String _rxBuffer = "";

bool isMonitoringFlow = 0;
unsigned long finalFlowDuration = 0;

float tempAdj = 0.0;

SoftwareSerial masterSerial(5, 6);

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
    pinMode(FLT_FIRST_PIN + i, INPUT_PULLUP);
  }

  tempSensor.begin();
  Serial.begin(9600);
  masterSerial.begin(9600);

  _device_id = readAddress();
  _op_mode = !digitalRead(MODE_PIN);

  initTemperatureBuffer();
}

void loop()
{
  readSerialCommands();
}

// Lê endereço 5-bit do DIP switch (A0-A4)
int readAddress()
{
  int b0 = !digitalRead(ADDR_PIN_0);
  int b1 = !digitalRead(ADDR_PIN_1);
  int b2 = !digitalRead(ADDR_PIN_2);
  int b3 = !digitalRead(ADDR_PIN_3);
  int b4 = !digitalRead(ADDR_PIN_4);
  return b0 + (b1 << 1) + (b2 << 2) + (b3 << 3) + (b4 << 4);
}

// Acumula caracteres do RS485 até <CR>
void readSerialCommands()
{
  while (masterSerial.available() > 0)
  {
    char c = (char)masterSerial.read();
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
    }
  }
}

void handleCommand(String cmd)
{
  if (cmd == "READ A")
  {
    startMeasurement();
  }
  else if (cmd.startsWith("SEND "))
  {
    int addr = cmd.substring(5).toInt();
    if (addr == _device_id)
    {
      sendData();
    }
  }
  else if (cmd.startsWith("Tmax "))
  {
    _tmax = (unsigned long)cmd.substring(5).toInt();
  }
  else if (cmd.startsWith("Tmin "))
  {
    _tmin = (unsigned long)cmd.substring(5).toInt();
  }
  else if (cmd.startsWith("TRATE "))
  {
    unsigned long rate = (unsigned long)cmd.substring(6).toInt();
    if (rate > 0)
    {
      _trate = rate;
      _tempSampleInterval = 60000UL / _trate;
    }
  }
}

void startMeasurement()
{
  _clog_status = 0xFFFF;
  _flow_status = 0x0000;
  _measurement_done = false;

  for (int i = 0; i < NUM_FLAUTAS; i++)
  {
    _flowPinDone[i] = false;
    _flowClogged[i] = false;
    _flowSwitchTime[i] = 0;
  }

  _flowState = FLOW_CLOG_CHECK;
}

void runFlowStateMachine()
{
  switch (_flowState)
  {
  case FLOW_IDLE:
    break;
  case FLOW_CLOG_CHECK:
  {
    int clogCount = 0;
    for (int i = 0; i < NUM_FLAUTAS; i++)
    {
      if (digitalRead(FLT_FIRST_PIN + i) == LOW)
      {
        _flowPinDone[i] = true;
        _flowClogged[i] = true;
        clogCount++;
        _clog_status &= ~(1 << i); // bit=0 → entupido
      }
    }

    if (clogCount == NUM_FLAUTAS)
    {
      _flow_status = 0x01FF;
      _measurement_done = true;
      _flowState = FLOW_IDLE;
    }
    else if (_op_mode == 1)
    {
      _measurement_done = true;
      _flowState = FLOW_IDLE;
    }
    else
    {
      _flowStartTime = millis();
      _flowState = FLOW_RUNNING;
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
        if (digitalRead(FLT_FIRST_PIN + i) == LOW)
        {
          _flowSwitchTime[i] = elapsed;
          _flowPinDone[i] = true;
        }
        else
        {
          allDone = false;
        }
      }
    }

    if (allDone || elapsed >= _tmax)
    {
      _flowState = FLOW_DONE;
    }

    break;
  }

  case FLOW_DONE:
  {
    bool anyTriggeredOK = false;

    for (int i = 0; i < NUM_FLAUTAS; i++)
    {
      if (_flowClogged[i])
        continue;

      if (!_flowPinDone[i])
      {
        _flow_status |= (1 << i);
      }
      else if (_flowSwitchTime[i] < _tmin)
      {
        _flow_status |= (1 << i);
      }
      else
      {
        anyTriggeredOK = true;
      }
    }

    if (!anyTriggeredOK)
    {
      _flow_status = 0x01FF;
    }

    _measurement_done = true;
    _flowState = FLOW_IDLE;
    break;
  }
  }
}

void sendData()
{
  if (_flowState != FLOW_IDLE)
  {
    Serial.print("BUSY\r\n");
    return;
  }

  if (!_measurement_done)
  {
    Serial.print("NODATA\r\n");
  }

  if (_temperature <= -98.0)
  {
    Serial.print("ERR");
  }
  else
  {
    Serial.print(_temperature, 1);
  }

  char buf[5];
  Serial.print(",");
  sprintf(buf, "%04x", _clog_status);
  Serial.print(buf);

  Serial.print(",");
  sprintf(buf, "%04x", _flow_status);
  Serial.print(buf);

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

  if (reading == DEVICE_DISCONNECTED_C)
  {
    _temperature = -99.0;
    return;
  }

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
  if (first == DEVICE_DISCONNECTED_C)
  {
    return;
  }
  for (int i = 0; i < TEMP_AVG_SIZE; i++)
  {
    _tempBuffer[i] = first;
  }

  _tempCount = TEMP_AVG_SIZE;
  _tempIndex = 0;
  _temperature = first;
}
