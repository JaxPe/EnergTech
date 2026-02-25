#include <SoftwareSerial.h>

SoftwareSerial slaveSerial(5, 6);

unsigned long lastCommandTime = 0;
const long pollingInterval = 2000;
const long responseInterval = 1000;

const int totalSlaves = 2;
int currentSlave = 1;

bool awaitingResponse = false;

void setup()
{
  Serial.begin(9600);
  slaveSerial.begin(9600);

  Serial.println("Master Inicializado");
}

void loop()
{
  if (!awaitingResponse && (millis() - lastCommandTime >= pollingInterval))
  {
    lastCommandTime = millis();
    awaitingResponse = true;

    Serial.print("\n[MASTER] Requesting data -> 'Send ");
    Serial.print(currentSlave);
    Serial.println("<CR>'");

    slaveSerial.print("Send ");
    slaveSerial.print(currentSlave);
    slaveSerial.println("\r");
  }

  if (slaveSerial.available() > 0)
  {
    awaitingResponse = false;
    Serial.print("[RESPONSE] ");

    while (slaveSerial.available() > 0)
    {
      char responseChar = slaveSerial.read();
      Serial.print(responseChar);
      delay(2);
    }

    prepareNextSlave();
  }

  if (awaitingResponse && (millis() - lastCommandTime >= responseInterval))
  {
    Serial.print("[TIMEOUT] Slave ");
    Serial.print(currentSlave);
    Serial.println(" failed to respond!");

    awaitingResponse = false;
    prepareNextSlave();
  }
}

void prepareNextSlave()
{
  currentSlave++;
  if (currentSlave > totalSlaves)
    currentSlave = 1;
}
