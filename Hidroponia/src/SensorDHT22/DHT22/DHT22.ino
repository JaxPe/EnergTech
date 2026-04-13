#include <Adafruit_Sensor.h>
#include <DHT.h>
#include <DHT_U.h>
#include <SoftwareSerial.h>

#define DHTTYPE DHT22
#define DHTPIN 2
DHT_Unified dht(DHTPIN, DHTTYPE);
uint32_t delayMS;

SoftwareSerial masterSerial(5, 6);

String receivedCommand = "";
const String ID = "1";  // alterar ID aqui

void setup()
{
  Serial.begin(9600);
  masterSerial.begin(9600);
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
  while (masterSerial.available() > 0)
  {
    char character = masterSerial.read();

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
  String target = "Send " + ID;
  command.trim();

  if (command == target)
  {
    float t = 0.0;
    float h = 0.0;

    sensorsRead(t, h);

    masterSerial.print("T:");   masterSerial.print(t);
    masterSerial.print(";H:");  masterSerial.println(h);
  }
}
