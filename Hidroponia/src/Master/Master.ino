#include <Arduino.h>

#define DEBUG_SERIAL Serial
#define RS485_SERIAL Serial1

// Se o módulo RS485 tiver pino DE/RE, descomente e configure o pino correto.
// #define RS485_DE_RE_PIN 2

void setRs485Tx()
{
#ifdef RS485_DE_RE_PIN
  digitalWrite(RS485_DE_RE_PIN, HIGH);
#endif
}

void setRs485Rx()
{
#ifdef RS485_DE_RE_PIN
  digitalWrite(RS485_DE_RE_PIN, LOW);
#endif
}

void setup()
{
  DEBUG_SERIAL.begin(9600);
  RS485_SERIAL.begin(9600);

#ifdef RS485_DE_RE_PIN
  pinMode(RS485_DE_RE_PIN, OUTPUT);
  setRs485Rx();
#endif

  DEBUG_SERIAL.println("MasterTeste pronto.");
  DEBUG_SERIAL.println("Use o monitor serial para enviar comandos.");
  DEBUG_SERIAL.println("-----------------------------");
}

void loop()
{
  if (DEBUG_SERIAL.available() > 0)
  {
    String cmd = DEBUG_SERIAL.readStringUntil('\n');
    cmd.trim();

    if (cmd.length() > 0)
    {
      DEBUG_SERIAL.print("-> Enviando: ");
      DEBUG_SERIAL.println(cmd);

      setRs485Tx();
      RS485_SERIAL.print(cmd);
      RS485_SERIAL.print("\r");
      delay(20);
      setRs485Rx();
    }
  }

  if (RS485_SERIAL.available() > 0)
  {
    String response = RS485_SERIAL.readStringUntil('\n');
    response.trim();

    if (response.length() > 0)
    {
      DEBUG_SERIAL.print("<- Resposta: ");
      DEBUG_SERIAL.println(response);
      byteDecypher(response);
    }
  }
}

void decifrarStatus(const char *nome, uint16_t status, bool ehEntupimento)
{
  DEBUG_SERIAL.print(nome);
  DEBUG_SERIAL.print(": ");

  for (uint8_t i = 0; i < 9; i++)
  {
    bool bitAtivo = (status & (1u << i)) != 0;

    DEBUG_SERIAL.print("F");
    DEBUG_SERIAL.print(i + 1);
    DEBUG_SERIAL.print("=");

    if (ehEntupimento)
      DEBUG_SERIAL.print(bitAtivo ? "OK" : "ENTUPIDA");
    else
      DEBUG_SERIAL.print(bitAtivo ? "PROBLEMA" : "OK");

    if (i < 8)
      DEBUG_SERIAL.print(", ");
  }

  DEBUG_SERIAL.println();
}

String obterCampo(String response, uint8_t indice)
{
  int inicio = 0;

  for (uint8_t i = 0; i < indice; i++)
  {
    inicio = response.indexOf(',', inicio);
    if (inicio == -1)
      return "";
    inicio++;
  }

  int fim = response.indexOf(',', inicio);
  if (fim == -1)
    return response.substring(inicio);

  return response.substring(inicio, fim);
}

void byteDecypher(String response)
{
  response.trim();

  String id = obterCampo(response, 0);
  String modo = obterCampo(response, 1);
  String temperatura = obterCampo(response, 2);

  if (id.length() == 0)
  {
    DEBUG_SERIAL.println("Resposta invalida do slave.");
    return;
  }

  id.trim();
  modo.trim();
  temperatura.trim();

  DEBUG_SERIAL.println("Decodificando resposta:");
  DEBUG_SERIAL.print("ID: ");
  DEBUG_SERIAL.println(id);
  DEBUG_SERIAL.print("Modo: ");
  DEBUG_SERIAL.println(modo);
  DEBUG_SERIAL.print("Temperatura: ");
  DEBUG_SERIAL.println(temperatura);

  if (modo == "1")
  {
    String entupimentoHex = obterCampo(response, 3);
    String vazaoHex = obterCampo(response, 4);
    String purgaHex = obterCampo(response, 5);

    entupimentoHex.trim();
    vazaoHex.trim();
    purgaHex.trim();

    uint16_t entupimento = (uint16_t)strtoul(entupimentoHex.c_str(), NULL, 16);
    uint16_t vazao = (uint16_t)strtoul(vazaoHex.c_str(), NULL, 16);
    uint16_t purga = (uint16_t)strtoul(purgaHex.c_str(), NULL, 16);

    DEBUG_SERIAL.print("Entupimento HEX: ");
    DEBUG_SERIAL.println(entupimentoHex);
    decifrarStatus("Entupimento", entupimento, true);

    DEBUG_SERIAL.print("Vazao HEX: ");
    DEBUG_SERIAL.println(vazaoHex);
    decifrarStatus("Vazao", vazao, false);

    DEBUG_SERIAL.print("Purga HEX: ");
    DEBUG_SERIAL.println(purgaHex);
    decifrarStatus("Purga", purga, false);
  }
}
