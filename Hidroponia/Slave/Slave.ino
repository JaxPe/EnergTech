#include <Adafruit_Sensor.h>
#include <DHT.h>
#include <DHT_U.h>
#include <SoftwareSerial.h>

#define DHTTYPE DHT22
#define DHTPIN 2
#define ADDR_BIT0 A1
#define ADDR_BIT1 A2
#define ADDR_BIT2 A3
#define ADDR_BIT3 A4

DHT_Unified dht(DHTPIN, DHTTYPE);

int _device_id = 0; // alterar ID aqui
SoftwareSerial masterSerial(5, 6);
String receivedCommand = "";

void setup()
{
  pinMode(A1, INPUT_PULLUP);
  pinMode(A2, INPUT_PULLUP);
  pinMode(A3, INPUT_PULLUP);
  pinMode(A4, INPUT_PULLUP);

  Serial.begin(9600);
  masterSerial.begin(9600);

  dht.begin();
  _device_id = readAdressFromPins()
}

void loop()
{
  getCommands();
}

int readAdressFromPins()
{
  int b0 = !digitalRead(ADDR_BIT0);
  int b1 = !digitalRead(ADDR_BIT1);
  int b2 = !digitalRead(ADDR_BIT2);
  int b3 = !digitalRead(ADDR_BIT3);

  return (b0 * 1) + (b1 * 2) + (b2 * 4) + (b3 * 8);
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
  command.trim();
  String target = "Send " + _device_id;

  if (command == target)
  {
    float t = 0.0;
    float h = 0.0;

    sensorsRead(t, h);

    masterSerial.print("Slave ");
    masterSerial.print(_device_id);
    masterSerial.print(",");
    masterSerial.print(t);
    masterSerial.print(",");
    masterSerial.println(h);
  }
}

void sensorsRead(float &temperture, float &humidity)
{
  sensors_event_t event;

  dht.temperature().getEvent(&event);

  if (isnan(event.temperature))
  {
    temperture = -99.0;
  }
  else
  {
    temperture = event.temperature;
  }

  dht.humidity().getEvent(&event);

  if (isnan(event.relative_humidity))
  {
    humidity = -99.0;
  }
  else
  {
    humidity = event.relative_humidity;
  }
}
