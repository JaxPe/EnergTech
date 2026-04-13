#include <SoftwareSerial.h>

SoftwareSerial slaveSerial(5, 6);

unsigned long lastActionTime = 0;
const long pollingInterval = 2000;
const long responseTimeout = 1000;

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
  unsigned long currentTime = millis();

  if (!awaitingResponse && (currentTime - lastActionTime >= pollingInterval))
  {
    sendRequest();
  }
  if (awaitingResponse && slaveSerial.available() > 0)
  {
    handleResponse();
  }
  if (awaitingResponse && (currentTime - lastActionTime >= responseTimeout))
  {
    handleTimeout();
  }
}

void sendRequest()
{
  lastActionTime = millis();
  awaitingResponse = true;

  Serial.print("\n[POLLING] Slave ");
  Serial.print(currentSlave);

  slaveSerial.print("Send ");
  slaveSerial.print(currentSlave);
  slaveSerial.print("\r");
}

void handleResponse()
{
  String rawData = slaveSerial.readStringUntil('\n');
  rawData.trim();

  int rxId;
  float rxTemp, rxHum;

  int fields = sscanf(rawData.c_str(), "%d,%f,%f", &rxId, &rxTemp, &rxHum);

  if (fields == 3 && rxId == currentSlave)
  {
    Serial.print(" -> SUCCESS: ");
    Serial.print("T:");
    Serial.print(rxTemp);
    Serial.print(" H:");
    Serial.println(rxHum);
  }
  else
  {
    Serial.print(" -> DATA ERROR: Received invalid format or wrong ID: ");
    Serial.println(rawData);
  }

  finishCycle();
}

void handleTimeout()
{
  Serial.println(" -> TIMEOUT: Slave failed to respond.");
  finishCycle();
}

void finishCycle()
{
  awaitingResponse = false;
  lastActionTime = millis();

  currentSlave++;
  if (currentSlave > totalSlaves)
  {
    currentSlave = 1;
  }
}
