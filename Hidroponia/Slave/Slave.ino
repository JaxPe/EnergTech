#include <Adafruit_Sensor.h>
#include <SoftwareSerial.h>
#include <OneWire.h>
#include <DallasTemperature.h>

#define ADDR_BIT0 A1
#define ADDR_BIT1 A2
#define ADDR_BIT2 A3
#define ADDR_BIT3 A4
#define ONE_WIRE_BUS 2
#define FLOW_SWITCH 3

OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensor(&oneWire);
DeviceAddress tempDeviceAddress;

unsigned long flowStartTime = 0;
bool isMonitoringFlow = 0;
unsigned long finalFlowDuration = 0;

int _device_id = 0; // alterar ID aqui
SoftwareSerial masterSerial(5, 6);
String receivedCommand = "";

void setup()
{
  pinMode(A1, INPUT_PULLUP);
  pinMode(A2, INPUT_PULLUP);
  pinMode(A3, INPUT_PULLUP);
  pinMode(A4, INPUT_PULLUP);

  sensor.begin();
  Serial.begin(9600);
  masterSerial.begin(9600);

  _device_id = readAdressFromPins();
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
  String sendCommand = "Send " + _device_id;
  String startCommand = "Start All";

  if (command == sendCommand)
  {
    float t = 0.0;

    temperatureRead(t);
    flowRead();

    masterSerial.print("Slave ");
    masterSerial.print(_device_id);
    masterSerial.print(",");
    masterSerial.print(t);
    masterSerial.print(",");
    masterSerial.print(isMonitoringFlow);
    masterSerial.print(",");
    masterSerial.println(finalFlowDuration);
  }
  if (command == startCommand)
  {
    flowStartTime = millis();
    isMonitoringFlow = true;
    finalFlowDuration = 0;
  }
}

void flowRead()
{
  if (!isMonitoringFlow)
    return;

  if (digitalRead(FLOW_SWITCH) == LOW)
  {
    finalFlowDuration = millis() - flowStartTime;
    isMonitoringFlow = false;
  }
}

void temperatureRead(float &temperature)
{
  sensor.requestTemperatures();
  if (!sensor.getAddress(tempDeviceAddress, 0))
  {
    temperature = -99.0;
  }
  else
  {
    temperature = sensor.getTempC(tempDeviceAddress);
  }
}
