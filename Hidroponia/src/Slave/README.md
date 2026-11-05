# Sistema Hidropônico - Dispositivo Escravo - Documentação

## Visão Geral

Este é o firmware do **Nó Escravo** para o **Sistema de Medição Hidropônico**, executando em um microcontrolador **Arduino NANO**. O dispositivo escravo monitora o fluxo de água através de 9 saídas de gotejamento (flautas) em uma cama de cultivo hidropônico, mede drenagem de reservatórios, detecta entupimentos e coleta amostras contínuas de temperatura ambiente.

O escravo comunica com um controlador mestre via barramento serial **RS485** para receber comandos de medição e transmitir dados de sensores.

---

## Configuração de Hardware

### Microcontrolador
- **Plataforma**: Arduino NANO (ATmega328P @ 16MHz)
- **Comunicação Serial**: SoftwareSerial para RS485 em 9600 baud

### Mapeamento de Pinos

| Pino | Nome | Função | Tipo |
|------|------|--------|------|
| D2 (05) | TEMP_PIN | Sensor de temperatura OneWire DS18B20 | Entrada |
| D3-D13 (06-14) | FLT_PINS[0-8] | Interruptores de nível de fluxo/drenagem (9 flautas) | Entrada, Pull-up |
| D11 (01) | RS485 TXD | Comunicação com mestre - envio | Saída |
| D12 (02) | RS485 RXD | Comunicação com mestre - recepção | Entrada |
| A0-A4 (19-23) | ADDR_PINs[0-4] | Seleção de endereço do dispositivo (interruptor DIP) | Entrada, Pull-up |
| A5 (24) | MODE_PIN | Seleção de modo de operação (interruptor DIP) | Entrada, Pull-up |

### Configuração do Interruptor DIP

#### Seleção de Endereço (A0-A4)
Um interruptor DIP de 5 posições define o endereço único do dispositivo no barramento RS485:
- **Fechado (BAIXO)** = 1
- **Aberto (ALTO)** = 0
- Permite 32 endereços únicos (0-31)
- O endereço é lido durante a inicialização

#### Modo de Operação (A5)
- **Aberto (ALTO / Lógica 1)** = **Modo Completo** - Mede fluxo + temperatura (9 flautas)
- **Fechado (BAIXO / Lógica 0)** = **Modo Apenas Temperatura** - Apenas amostragem de temperatura

---

## Modos de Operação

### Modo 0: Apenas Temperatura
- Amostra continuamente a temperatura na taxa configurada
- Não realiza medições de fluxo ou drenagem
- Ignora comandos "StartAll" e "StopAll"
- Responde a solicitações de dados com: `<ID>,0,<TEMP>\r\n`

### Modo 1: Modo Completo
- Mede status de entupimento de saída via verificação inicial de sensores
- Mede tempo de ativação de fluxo de água através de cada flauta
- Mede tempo de drenagem de reservatório após parada da bomba
- Amostra continuamente a temperatura (background)
- Responde a solicitações de dados com: `<ID>,1,<TEMP>,<ENTUPIMENTO>,<VAZAO>,<PURGA>\r\n`

---

## Interface de Comandos (RS485)

Todos os comandos são terminados com retorno de carro (`\r`). O dispositivo responde em 9600 baud.

### Comandos Disponíveis

#### `StartAll\r`
Inicia o ciclo de medição de fluxo:
1. Verifica todas as 9 saídas quanto a entupimentos (estado inicial dos pinos)
2. Se todas entupidas → retorna status de erro
3. Caso contrário → começa a medir tempo de ativação de fluxo de água
4. Executa por até `_flowTimeMax` milissegundos
5. Registra quando cada pino de saída vai para BAIXO

#### `StopAll\r`
Para a bomba e inicia o ciclo de medição de purga:
1. Monitora drenagem de reservatório através das saídas de purga
2. Rastreia quando cada interruptor de nível libera (pino vai para ALTO)
3. Mede tempo de drenagem para cada flauta
4. Executa por até `_purgeTimeMax` milissegundos

#### `Send <addr>\r`
Solicita dados de medição do escravo no endereço `<addr>`:
- Se o endereço estiver correto, escravo responde com leituras de sensores atuais
- Formato de resposta depende do modo de operação
- Status: Retorna `BUSY\r\n` se medições estão em progresso

#### `TimeVMax <val>\r`
Define a duração da janela de medição de fluxo (milissegundos):
- Padrão: 2000 ms
- Intervalo: Qualquer inteiro positivo
- Exemplo: `TimeVMax 3000\r` → 3 segundos de janela de medição

#### `TimeVMin <val>\r`
Define o limiar mínimo de detecção de fluxo (milissegundos):
- Padrão: 500 ms
- Ativações de fluxo mais rápidas que este tempo são consideradas erros
- Exemplo: `TimeVMin 300\r` → ignora ativações < 300 ms

#### `TimePMax <val>\r`
Define a duração da janela de medição de purga (milissegundos):
- Padrão: 1000 ms
- Timeout para medição de drenagem de reservatório

#### `TimePMin <val>\r`
Define o limiar mínimo de detecção de purga (milissegundos):
- Padrão: 300 ms
- Drenagem mais rápida que este tempo pode indicar problemas

#### `TempRate <val>\r`
Define a frequência de amostragem de temperatura (amostras por minuto):
- Padrão: 30 amostras/minuto (uma amostra a cada 2 segundos)
- Exemplo: `TempRate 12\r` → 12 amostras/minuto (uma a cada 5 segundos)

#### `TempAdj <addr> <offset>\r`
Define offset de calibração de temperatura (°C) para o sensor:
- Aplica-se apenas ao escravo no endereço especificado
- Offset é adicionado a todas as leituras de temperatura
- Exemplo: `TempAdj 22 -0.5\r` → Dispositivo 22 subtrai 0.5°C das leituras

---

## Formato de Resposta de Dados

### Resposta Modo Apenas Temperatura
```
<ID>,0,<TEMP>
```

Exemplo: `22,0,25.3\r\n`

### Resposta Modo Completo
```
<ID>,1,<TEMP>,<HEX_ENTUPIMENTO>,<HEX_VAZAO>,<HEX_PURGA>
```

Exemplo: `22,1,25.3,01FF,0000,01FF\r\n`

### Descrição dos Campos

#### ID
- Endereço do dispositivo (00-31)
- Identifica qual escravo enviou os dados

#### MODO
- 0 = Apenas temperatura
- 1 = Modo completo de medição

#### TEMP
- Temperatura atual em °C com 1 casa decimal
- Exemplo: `25.3`, `18.5`, `-5.2`
- Valor de erro: `ERR` (sensor não detectado ou conexão perdida)

#### HEX_ENTUPIMENTO (Modo Completo Apenas)
- Valor hexadecimal de 16-bit representando status de entupimento de saída
- Cada bit corresponde a uma flauta (bit 0 = flauta 1, bit 8 = flauta 9)
- **Bit valor 1** = Saída OK (não entupida)
- **Bit valor 0** = Saída ENTUPIDA (bloqueada, BAIXA na medição inicial)

Exemplo: `01FF` em binário = `0000000111111111`
- Flautas 1-8 estão OK (bits 0-7 = 1)
- Flauta 9 está entupida (bit 8 = 0)

#### HEX_VAZAO (Modo Completo Apenas)
- Valor hexadecimal de 16-bit representando resultados de medição de fluxo de água
- Cada bit = status de ativação de fluxo de uma flauta
- **Bit valor 1** = PROBLEMA de Fluxo
  - Nunca foi ativada (água não atinge saída), ou
  - Foi ativada muito rápido (< TimeVMin), ou
  - Saída estava pré-entupida
- **Bit valor 0** = Fluxo OK (ativada normalmente)

Exemplo: `0000` = Todas as flautas com bom fluxo
Exemplo: `0003` = Flautas 1-2 com problemas de fluxo, 3-9 estão OK

#### HEX_PURGA (Modo Completo Apenas)
- Valor hexadecimal de 16-bit representando resultados de drenagem de reservatório
- Cada bit = status de drenagem de uma flauta
- **Bit valor 1** = PROBLEMA de Drenagem
  - Nunca drenagem (válvula travada), ou
  - Drenou muito lentamente (> TimePMax)
- **Bit valor 0** = Drenagem OK (completada normalmente)

Exemplo: `0080` = Flauta 8 com problema de drenagem, outras OK

---

## Sistema Interno de Amostragem de Temperatura

### Amostragem Contínua em Background
- Amostras executam **independentemente** das medições de fluxo/purga
- Não-bloqueante: usa comparação de tempo decorrido, não atrasos
- Taxa configurável: 12-60 amostras/minuto (padrão: 30)

### Filtro de Média Móvel
- Mantém buffer circular das últimas 10 leituras
- Rejeita valores discrepantes desviando > 2°C da média atual
- Suaviza ruído do sensor e transientes térmicos

### Requisitos do Sensor
- Sensor de temperatura **OneWire DS18B20**
- Conectado ao pino D2
- Suporta múltiplos sensores, usa primeiro endereço detectado

### Tratamento de Erro de Temperatura
- Se sensor não for detectado: reporta `ERR` na resposta de dados
- Sensores ausentes ou desconectados retornam -99.0 internamente
- Sem alarme ou indicação especial, apenas valor de erro reportado

### Calibração
Use comando `TempAdj` para ajustar leituras:
- Aplicado como offset a todas as medições
- Persiste entre ciclos de medição
- Exemplo: Sensor lê 1°C acima do normal → use `TempAdj <addr> -1.0`

---

## Arquitetura da Máquina de Estados

### Máquina de Estados de Medição de Fluxo

```
FLOW_IDLE
    ↓ (comando StartAll)
FLOW_CLOG_CHECK
    └─ Verifica estados de todos os pinos de saída (ALTO=OK, BAIXO=entupido)
    ├─→ Todos entupidos? → Define erro, retorna a FLOW_IDLE
    ├─→ Modo apenas temperatura? → Retorna a FLOW_IDLE
    └─→ Saídas boas? → Continua para FLOW_RUNNING
    
FLOW_RUNNING
    └─ Pesquisa pinos, aguardando transições para BAIXO
    ├─ Registra ms decorridos para cada ativação
    └─ Sai quando: todos os pinos ativados OU timeout atingido
    
FLOW_DONE
    └─ Avalia tempo de cada ativação
    ├─ Resultado: < TimeVMin → bit de erro definido (1)
    ├─ Resultado: > TimeVMin → bit OK definido (0)
    └─ Retorna a FLOW_IDLE
```

Chamado: A cada iteração do loop principal (máx ~1-2ms por estado)

### Máquina de Estados de Medição de Purga

```
PURGE_IDLE
    ↓ (comando StopAll)
PURGE_RUNNING
    └─ Pesquisa pinos, aguardando transições para ALTO
    ├─ Registra ms decorridos para cada pino ALTO
    └─ Sai quando: todos os pinos ALTOS OU timeout atingido
    
PURGE_DONE
    └─ Avalia tempos de drenagem
    ├─ Resultado: Timeout ou muito lento → bit de erro definido (1)
    ├─ Resultado: Drenagem normal → bit OK definido (0)
    └─ Retorna a PURGE_IDLE
```

Chamado: A cada iteração do loop principal

---

## Estrutura do Código

### Funções Principais

| Função | Propósito |
|--------|-----------|
| `setup()` | Inicialização de hardware, configuração de pinos, inicialização de sensores |
| `loop()` | Loop principal de eventos, chama todos os subsistemas |
| `readSerialCommands()` | Acumula dados de RS485, distribui para gerenciador |
| `handleCommand()` | Analisa e executa comandos |
| `readAddress()` | Lê endereço de 5-bit do interruptor DIP |
| `startFlowMeasurement()` | Inicia ciclo de medição de fluxo |
| `runFlowStateMachine()` | Máquina de estados para medição de fluxo |
| `startPurgeMeasurement()` | Inicia ciclo de purga de drenagem |
| `runPurgeStateMachine()` | Máquina de estados para medição de purga |
| `sendData()` | Transmite resultados de medição |
| `sampleTemperatureContinuous()` | Gatilho não-bloqueante de amostra de temperatura |
| `updateTemperature()` | Lê sensor, aplica filtro, atualiza média |
| `computeAverage()` | Calcula média das leituras armazenadas |
| `initTemperatureBuffer()` | Pré-preenche buffer na inicialização |
| `isBusy()` | Verifica se medições estão sendo executadas |

### Variáveis Globais

**Parâmetros de Tempo:**
- `_flowTimeMax`, `_flowTimeMin` - Janela de medição de fluxo e limiar
- `_purgeTimeMax`, `_purgeTimeMin` - Janela de medição de purga e limiar
- `_tempSampleRate`, `_tempSampleInterval` - Frequência de amostragem de temperatura

**Resultados de Medição:**
- `_clogStatus`, `_flowStatus`, `_purgeStatus` - Palavras de status de resultado (16-bit)
- `_temperature` - Temperatura atual (°C)

**Rastreamento de Estado:**
- `_flowState`, `_purgeState` - Estados atuais das máquinas de estados
- `_flowPinDone[]`, `_purgePinDone[]` - Flags de conclusão por pino
- `_flowSwitchTime[]`, `_purgeSwitchTime[]` - Tempos de ativação (ms)
- `_flowStartTime`, `_purgeStartTime` - Timestamps de início de ciclo

**Buffer de Temperatura:**
- `_tempBuffer[]` - Buffer circular (10 amostras)
- `_tempIndex` - Posição de escrita atual
- `_tempCount` - Amostras válidas no buffer
- `_tempAdj` - Offset de calibração

---

## Detalhes do Protocolo de Comunicação

### Configuração Serial
- **Velocidade**: 9600 baud
- **Bits**: 8 bits de dados
- **Paridade**: Nenhuma
- **Bits de parada**: 1
- **Controle de fluxo**: Nenhum

### Estrutura de Comandos
- Comandos devem terminar com retorno de carro `\r` (ASCII 13)
- Quebras de linha `\n` são ignoradas
- Feedback de eco é enviado durante acumulação de comandos (saída de debug)
- Palavras-chave de comando sensíveis a maiúsculas/minúsculas

### Estrutura de Resposta
- Respostas terminam com `\r\n` (CR+LF)
- Campos de dados separados por vírgulas
- Valores hex são minúsculos, 4 dígitos com zeros à esquerda

### Tratamento de Erro
- Se `Send` recebido durante medição: retorna `BUSY\r\n`
- Comandos inválidos: ignorados silenciosamente
- Erros de sensor: campo de temperatura mostra `ERR`

---

## Solução de Problemas

### Sem Resposta a Comandos
- Verificar fiação RS485 (par diferencial A/B)
- Verificar configuração de taxa de baud (deve ser 9600)
- Confirmar terminação CR em comandos
- Verificar se endereço do dispositivo corresponde à configuração do interruptor DIP

### Medições de Fluxo Sempre Mostram Erro
- Verificar se fios de saída de flauta conectados aos pinos corretos (D3-D13)
- Verificar se configuração de pino é INPUT_PULLUP
- Testar com HIGH/BAIXO manual do pino para verificar fiação
- Confirmar se limiares de fluxo são razoáveis (TimeVMin < TimeVMax)

### Temperatura Lê "ERR"
- Verificar conexão do sensor OneWire ao D2
- Verificar se biblioteca DallasTemperature está instalada
- Confirmar conexões de alimentação e terra do DS18B20
- Testar sensor com sketch de diagnóstico separado

### Medições Nunca Completam
- Verificar se janelas de medição são razoáveis (TimeVMax > 100ms)
- Verificar se não há comandos sobrepostos de Start/Stop
- Confirmar se mudanças de estado de pino realmente ocorrem
- Verificar se relógio/timing do Arduino não está travado

### Medições de Purga Sempre Expiram
- Verificar se válvula de drenagem é funcional (abre e fecha)
- Verificar fiação e operação do interruptor de nível
- Confirmar se limiares de tempo de purga são apropriados
- Verificar se comando de parada de bomba realmente para o fluxo

---

## Notas de Desempenho

- **Tempo de Loop**: Tipicamente 1-2 ms por iteração (RS485 9600 baud limitante)
- **Memória**: ~2.5 KB código + 300 bytes RAM (variáveis + buffers)
- **Precisão de Tempo**: ±5-10 ms (resolução de timing do Arduino)
- **Taxa de Atualização de Temperatura**: Configurável, padrão uma amostra a cada 2 segundos

---

## Revisão de Hardware
- **Versão do Firmware Escravo**: F (2026-05-13)
- **Projetado para**: Arduino NANO com placa de sensores de flauta LED customizada

---

## Dependências

### Bibliotecas Necessárias
- `SoftwareSerial` (built-in) - Comunicação RS485
- `OneWire` (externa) - Protocolo DS18B20
- `DallasTemperature` (externa) - Driver do sensor de temperatura

### Instalação
```bash
# No Gerenciador de Bibliotecas da Arduino IDE:
Procurar por "OneWire" de Jim Studt et al. → Instalar
Procurar por "DallasTemperature" de Miles Burton → Instalar
```

---

## Exemplo de Integração do Sistema

```
Dispositivo Mestre (RS485)
        ↓ (Comandos)
        ↓
   Barramento RS485 (diferencial A/B)
        ↓
   [Dispositivo Escravo] ← Este firmware
        ├─ Avaliação de Fluxo → 9 sensores de flauta
        ├─ Amostragem de Temperatura → DS18B20
        └─ Resposta (Dados)
        ↓
   Barramento RS485
        ↓
        ↓ (Dados de sensores)
   Dispositivo Mestre
```

---

## Licença e Notas

Este firmware faz parte do projeto **Sistema de Medição Hidropônico EnergTech** do INMETRO.

Para esquemáticos de hardware detalhados, veja: `docs/fluxograma_slave*.mmd`
