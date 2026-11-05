// ============================================================
//  SLAVE
//  Hardware: Arduino NANO
//  Rev: F  |  Date: 2026-05-13
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
//    D11 (01) - conversor RS485 TXD
//    D12 (02) - conversor RS485 RXD
//    D13 (14) - FLT 9
//    A0  (19) - ENDEREÇO bit 0
//    A1  (20) - ENDEREÇO bit 1
//    A2  (21) - ENDEREÇO bit 2
//    A3  (22) - ENDEREÇO bit 3
//    A4  (23) - ENDEREÇO bit 4
//    A5  (24) - MODO DE OPERAÇÃO (open=full, shorted=temp only)
//
//  MODOS OPERACIONAIS (DIP switch A5 / MOD OP):
//    1 = Full: medição de vazão (9 flautas) + temperatura
//    0 = Somente temperatura
//
//  COMANDOS (RS485 bus, <CR>):
//    StartAll<CR>              - TODOS os dispositivos executam a rotina de medição de vazão
//    StopAll<CR>               - TODOS os dispositivos executam a rotina de medição de purga
//    SEND <addr><CR>           - dispositivo <addr> envia string de dados
//    TimeVMax <val><CR>        - Define janela de tempo para medição de vazão (ms)  Default: 2000
//    TimeVMin <val><CR>        - Define tempo mínimo para vazão (ms)                       Default: 500
//    TimePMax <Val><CR>        - Define janela de tempo para medição de purga (ms)
//    TimePMin <val><CR>        - Define tempo mínimo para purga (ms)
//    TempRate <val><CR>        - Define amostras de temperatura por minuto (padrão = 12)
//    TempAdj <addr><val><CR>   - Define variável de ajuste do sensor de temperatura
//
//  RESPOSTA para "SEND <addr>":
//    <id>,<op_mode>,<temperatura>,<entupimento_HEX>,<vazao_HEX>,<purga_HEX><CR><LF>
//    Example:  22.50,01FF,0000,01FF
//
//    obstruido_HEX (16-bit):
//      posição do Bit = número da flauta (0=FLT1 ... 8=FLT9)
//      Bit 0 = saída OBSTRUIDA
//      Bit 1 = saída OK
//
//    vazao_HEX (16-bit):
//      posição do Bit = número da flauta (0=FLT1 ... 8=FLT9)
//      Bit 1 = vazao de entrada BAIXA (não acionou dentro do Tmax)
//      Bit 0 = vazao de entrada OK
//
//    purga_HEX (16-bit):
//      posição do Bit = número da flauta (0=FLT1 ... 8=FLT9)
//      Bit 1 = vazao de purga BAIXA (não desacionou dentro do Tmax)
//      Bit 0 = vazao de purga OK
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

#define STATUS_UNINIT 0xFFFF

#define RX_TIMEOUT_ms 100

OneWire oneWire(TEMP_PIN);
DallasTemperature tempSensor(&oneWire);
DeviceAddress tempAddress;

SoftwareSerial masterSerial(12, 11);

int _device_id = 0;
int _op_mode = 0; // 1=completo, 0=temp somente

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
unsigned long _rxLastCharTime = 0;

/**
 * @brief Verifica se o dispositivo está executando medições.
 *
 * @return true se a medição de vazão ou purga estiver em progresso
 * @return false se o dispositivo estiver ocioso e pronto para novos comandos
 */
bool isBusy()
{
  return _flowState != FLOW_IDLE || _purgeState != PURGE_IDLE;
}

/**
 * @brief Inicializa o hardware, configura os pinos, comunicação serial e sensor de temperatura.
 *
 * Configura:
 * - Pinos de seleção de endereço (A0-A4) como entradas com pull-ups do interruptor DIP
 * - Pino de modo de operação (A5) como entrada com pull-up
 * - Todos os 9 pinos de sensor de flauta (D3-D13) como entradas com pull-ups
 * - Sensor de temperatura OneWire no pino D2
 * - Comunicação serial em 9600 baud (debug e comunicação com mestre)
 * - Lê o endereço do dispositivo da configuração do interruptor DIP
 * - Lê o modo de operação do pino MODE_PIN
 * - Inicializa o buffer de temperatura com leituras iniciais
 */
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

/**
 * @brief Loop principal - monitora e atualiza continuamente todos os subsistemas.
 *
 * Executa em sequência a cada iteração:
 * 1. Processa comandos recebidos do mestre via RS485
 * 2. Atualiza a máquina de estados de medição de vazão
 * 3. Atualiza a máquina de estados de medição de purga
 * 4. Amostra a temperatura em taxa configurada (em background)
 */
void loop()
{
  readSerialCommands();
  runFlowStateMachine();
  runPurgeStateMachine();
  sampleTemperatureContinuous();
}

/**
 * @brief Lê o endereço de 5 bits do dispositivo a partir da configuração do interruptor DIP.
 *
 * O endereço é definido via interruptor DIP de 5 posições nos pinos A0-A4 (MSB para LSB).
 * Cada interruptor: BAIXO (fechado) = 1, ALTO (aberto) = 0
 * Permite até 32 endereços únicos (0-31) no barramento RS485.
 *
 * @return int Endereço do dispositivo (0-31) baseado nas posições do interruptor DIP
 */
int readAddress()
{
  int b0 = !digitalRead(ADDR_PIN_4);
  int b1 = !digitalRead(ADDR_PIN_3);
  int b2 = !digitalRead(ADDR_PIN_2);
  int b3 = !digitalRead(ADDR_PIN_1);
  int b4 = !digitalRead(ADDR_PIN_0);
  return b0 + (b1 << 1) + (b2 << 2) + (b3 << 3) + (b4 << 4);
}

/**
 * @brief Lê dados de série recebidos e processa comandos terminados por retorno de carro.
 *
 * Acumula caracteres do barramento RS485 em um buffer. Quando um retorno de carro (\r)
 * é recebido, a string de comando completa é passada para handleCommand() para processamento.
 * Quebras de linha (\n) são ignoradas. A saída de debug por eco é enviada para caracteres recebidos.
 */
void readSerialCommands()
{
  unsigned long now = millis();

  if (_rxBuffer.length() > 0 && (now - _rxLastCharTime) >= RX_TIMEOUT_ms)
  {
    Serial.print("Descartando (timeout): ");
    Serial.println(_rxBuffer);
    _rxBuffer = "";
  }

  while (masterSerial.available() > 0)
  {
    char c = (char)masterSerial.read();
    _rxLastCharTime = millis();

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
  }
}

/**
 * @brief Analisa e executa comandos recebidos do dispositivo mestre via RS485.
 *
 * Comandos suportados:
 * - "StartAll" - Inicia ciclo de medição de vazão
 * - "StopAll" - Inicia ciclo de medição de purga
 * - "Send <addr>" - Envia dados de medição se endereço corresponder
 * - "TimeVMax <ms>" - Define janela de medição de vazão
 * - "TimeVMin <ms>" - Define tempo mínimo de detecção de vazão
 * - "TimePMax <ms>" - Define janela de medição de purga
 * - "TimePMin <ms>" - Define tempo mínimo de detecção de purga
 * - "TempRate <amostras/min>" - Define frequência de amostragem de temperatura
 * - "TempAdj <addr> <offset>" - Define offset de calibração do sensor de temperatura
 */
void handleCommand(String cmd)
{
  if (cmd == "StartAll")
  {
    Serial.println("Start");
    startFlowMeasurement();
  }
  else if (cmd == "StopAll")
  {
    Serial.println("Stop");
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
    unsigned long rate = (unsigned long)cmd.substring(9).toInt();
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
    int addr = cmd.substring(8).toInt();
    if (addr == _device_id)
    {
      float adjust = (float)cmd.substring(10).toFloat();
      _tempAdj = adjust;
    }
    Serial.print("TempAdj: ");
    Serial.println(_tempAdj);
  }
}

/**
 * @brief Inicia o ciclo de medição de vazão acionado pelo comando "StartAll".
 *
 * Redefine todos os registros de status relacionados com vazão e variáveis de estado,
 * depois faz a transição da máquina de estados para a fase FLOW_CLOG_CHECK.
 * Este é o ponto de entrada para medir as taxas de fluxo de água através de cada flauta
 * (saída de gotejamento de flor).
 *
 * Define valores de status inicial:
 * - _clogStatus redefinido para 0xFFFF (assume todas as saídas desentupidas)
 * - _flowStatus redefinido para 0x0000 (sem vazão detectada ainda)
 * - Todas as variáveis de rastreamento por flauta limpas
 */
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

/**
 * @brief Máquina de estados que controla o ciclo de medição de vazão.
 *
 * Funciona em três fases:
 *
 * **FLOW_CLOG_CHECK**: Verificação inicial de saída
 * - Lê todos os 9 pinos de flauta uma vez para detectar saídas entupidas
 * - Se o pino for BAIXO = saída entupida (flauta fechada, bloqueada)
 * - Se o pino for ALTO = saída OK (flauta aberta, limpa)
 * - Armazena status de entupimento em _clogStatus (1=OK, 0=entupido)
 * - Se todas as 9 saídas estão entupidas, aborta medição para FLOW_IDLE
 * - Se todas passam ou op_mode=0 (temperatura apenas), transição para FLOW_IDLE
 * - Caso contrário move para FLOW_RUNNING
 *
 * **FLOW_RUNNING**: Medir tempo de ativação
 * - Pesquisa continuamente pinos esperando que fiquem BAIXOS (água fluindo)
 * - Quando BAIXO detectado, registra milissegundos decorridos desde o início
 * - Continua até que todos os pinos tenham se ativado OU tempo limite (_flowTimeMax) atingido
 * - Transição para FLOW_DONE quando completo
 *
 * **FLOW_DONE**: Avaliar e armazenar resultados
 * - Analisa tempos de ativação armazenados contra limiar mínimo (_flowTimeMin)
 * - Define bits de _flowStatus: 1=vazão OK, 0=problema de vazão (muito lento ou nunca acionado)
 * - Transição de volta para FLOW_IDLE
 *
 * Chamado a cada iteração do main loop().
 */
void runFlowStateMachine()
{
  switch (_flowState)
  {
  case FLOW_IDLE:
    break;
  case FLOW_CLOG_CHECK:
  {
    Serial.println("Checking");
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
      Serial.println("sem entupimento total");
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
          Serial.print(i);
          Serial.println(" ativou");
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

/**
 * @brief Inicia o ciclo de medição de purga acionado pelo comando "StopAll".
 *
 * Quando a bomba para após medição de vazão, água residual drena através das saídas de purga.
 * Esta função redefine variáveis de rastreamento de purga e começa a monitorar quanto tempo leva
 * para cada flauta drenagem completamente via saída de purga.
 *
 * Define status inicial como STATUS_UNINIT e limpa rastreamento de purga por flauta.
 */
void startPurgeMeasurement()
{
  _purgeStatus = STATUS_UNINIT;

  for (int i = 0; i < NUM_FLAUTAS; i++)
  {
    _purgePinDone[i] = false;
    _purgeSwitchTime[i] = 0;
  }

  _purgeStartTime = millis();
  _purgeState = PURGE_RUNNING;
}

/**
 * @brief Máquina de estados que controla o ciclo de medição de purga de drenagem.
 *
 * Depois que a bomba para, líquido residual em cada reservatório de flauta drena através de uma saída de purga.
 * Isto monitora tempo de drenagem para detectar bloqueios ou anomalias do sistema.
 *
 * **PURGE_RUNNING**: Medir tempo de drenagem de reservatório
 * - Pesquisa continuamente todos os 9 pinos de flauta procurando transição para ALTO
 * - Cada flauta tem um interruptor de nível que dispara BAIXO quando reservatório está cheio
 * - À medida que líquido drena, nível cai e pino vai para ALTO (interruptor de nível libera)
 * - Registra ms decorridos desde parada de bomba até transição para ALTO para cada flauta
 * - Continua até que todos os reservatórios estejam vazios OU tempo limite (_purgeTimeMax) atingido
 * - Transição para PURGE_DONE quando completo
 *
 * **PURGE_DONE**: Avaliar desempenho de drenagem
 * - Para cada flauta que drenou, verifica se tempo excede limiar mínimo (_purgeTimeMin)
 * - Se drenagem muito lenta ou nunca ocorreu, define bit de erro em _purgeStatus
 * - Define bits de _purgeStatus: 1=problema de drenagem/timeout, 0=drenou OK
 * - Transição de volta para PURGE_IDLE
 *
 * Chamado a cada iteração do main loop().
 */
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

/**
 * @brief Transmite dados de medição para o dispositivo mestre via RS485.
 *
 * Envia string de dados formatada contendo ID, modo de operação, temperatura e resultados de medição.
 * Formato: ID,MODO,TEMP[,HEX_ENTUPIMENTO,HEX_VAZAO,HEX_PURGA]\r\n
 *
 * Exemplo de saída (Modo completo):
 *   22,1,25.3,01FF,0000,01FF\r\n
 * Exemplo de saída (Modo temperatura apenas):
 *   22,0,25.3\r\n
 *
 * Recusa enviar se dispositivo está ocupado (medições em progresso).
 * No modo temperatura apenas, envia apenas ID, modo e temperatura.
 * No modo completo, também inclui três palavras de status de 16-bit:
 *   - HEX_ENTUPIMENTO: Status de entupimento de saída (bit por flauta, 1=OK, 0=entupido)
 *   - HEX_VAZAO: Resultados de medição de vazão (bit por flauta, 1=problema, 0=OK)
 *   - HEX_PURGA: Resultados de drenagem de purga (bit por flauta, 1=problema, 0=OK)
 */
void sendData()
{
  if (isBusy())
  {
    Serial.print("BUSY\r\n");
    return;
  }

  char buf[5];

  sprintf(buf, "%02d", _device_id);
  masterSerial.print(buf);

  masterSerial.print(",");
  masterSerial.print(_op_mode);

  masterSerial.print(",");
  if (_temperature <= -98.0)
    masterSerial.print("ERR");
  else
    masterSerial.print(_temperature, 1);

  if (_op_mode == 1)
  {
    masterSerial.print(",");
    sprintf(buf, "%04x", _clogStatus);
    masterSerial.print(buf);

    masterSerial.print(",");
    sprintf(buf, "%04x", _flowStatus);
    masterSerial.print(buf);

    masterSerial.print(",");
    sprintf(buf, "%04x", _purgeStatus);
    masterSerial.print(buf);
  }

  masterSerial.print("\r\n");
}

/**
 * @brief Verifica se é hora de amostrar temperatura e dispara atualização se necessário.
 *
 * Esta função implementa amostragem periódica não-bloqueante na taxa configurada
 * (_tempSampleRate, padrão 30 amostras/minuto = a cada 2 segundos).
 * Usa comparação de tempo decorrido em vez de atrasos bloqueadores.
 *
 * Chamado a cada iteração de loop mas realiza medições reais apenas nos limites de intervalo.
 */
void sampleTemperatureContinuous()
{
  unsigned long now = millis();
  if (now - _lastTempSample >= _tempSampleInterval)
  {
    _lastTempSample = now;
    updateTemperature();
  }
}

/**
 * @brief Lê a temperatura atual do sensor DS18B20 e atualiza a média móvel.
 *
 * Implementa filtro de média móvel com rejeição de valores discrepantes:
 * 1. Solicita conversão de temperatura ao sensor
 * 2. Recupera leitura de temperatura bruta em °C
 * 3. Compara contra média móvel:
 *    - Se desvio > TEMP_MAX_DELTA (2°C), leitura é rejeitada como valor discrepante
 *    - Caso contrário, leitura é adicionada ao buffer circular
 * 4. Atualiza _temperature com nova média das últimas N leituras
 *
 * Tamanho do buffer: TEMP_AVG_SIZE (10 amostras)
 * Filtro rejeita ruído do sensor e transientes térmicos
 * Se sensor não é detectado, define _temperature para -99.0 (indicador de erro)
 */
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
  _temperature = computeAverage() + _tempAdj;
}

/**
 * @brief Calcula a média aritmética das leituras de temperatura armazenadas em buffer.
 *
 * Computa média de todas as leituras válidas atualmente no buffer circular.
 * Usada pelo filtro de rejeição de valores discrepantes e reportada na saída de dados do sensor.
 *
 * @return float Temperatura média em °C, ou 0.0 se buffer está vazio
 */
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

/**
 * @brief Inicializa o buffer de temperatura com leituras iniciais do sensor na inicialização.
 *
 * Lê o sensor DS18B20 e preenche todo o buffer circular com a mesma leitura inicial.
 * Isto permite que o filtro de detecção de valores discrepantes funcione corretamente
 * a partir da primeira medição estabelecendo uma linha de base válida e média.
 *
 * Chamado durante setup() para garantir que medições de temperatura estejam disponíveis
 * imediatamente após boot (sem período de aquecimento necessário).
 */
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
