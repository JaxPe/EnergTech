#include <Adafruit_Sensor.h>
#include <DHT.h>
#include <DHT_U.h>

#define DHTTYPE DHT22
#define DHTPIN 2
DHT_Unified dht(DHTPIN, DHTTYPE);
uint32_t delayMS;

String receivedCommand = "";

void setup()
{
  Serial.begin(9600);
  dht.begin();
  Serial.println("Inicializando sensor DHT");
  sensor_t sensor;
  dht.temperature().getSensor(&sensor);
  delayMS = sensor.min_delay / 1000;
}

void loop()
{
  getCommands();
}

void sensorsRead(float &temperture, float &humidity) {
  sensors_event_t event;

  dht.temperature().getEvent(&event);

  if (isnan(event.temperature)) {
    temperture = -99.0;
  } else {
    temperture = event.temperature;
  }

  dht.humidity().getEvent(&event);

  if (isnan(event.relative_humidity))
  {
    humidity = -99.0;
  } else
  {
  humidity = event.relative_humidity;
  }
}

void getCommands()
{
  while (Serial.available() > 0)
  {
    char character = Serial.read();

    if (character == '\r')
    {
      executeCommand(receivedCommand);
      receivedCommand = "";
    }
    else if (character != '\n')
    {
      receivedCommand += character;
    }
  }
}

void executeCommand(String command)
{
  command.trim();

  if (command == "Send 1")
  {
    float t = 0.0;
    float h = 0.0;

    sensorsRead(t, h);

    Serial.print("T:");   Serial.print(t);
    Serial.print(";H:");  Serial.println(h);
  }
}
