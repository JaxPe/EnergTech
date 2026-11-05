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
  DEBUG_SERIAL.println("Exemplos:");
  DEBUG_SERIAL.println("  Send 4");
  DEBUG_SERIAL.println("  StartAll");
  DEBUG_SERIAL.println("  StopAll");
  DEBUG_SERIAL.println("  TimeVMax 2000");
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
    }
  }
}
