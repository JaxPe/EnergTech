#include <Adafruit_Sensor.h>
#include <DHT.h>
#include <DHT_U.h>

#define DHTTYPE DHT22
#define DHTPIN 2
DHT_Unified dht(DHTPIN, DHTTYPE);
uint32_t delayMS;

void setup()
{
  Serial.begin(9600);
  dht.begin();
  Serial.println("Inicializando sensor DHT");
  sensor_t sensor;
  dht.temperature().getSensor(&sensor);
  dht.humidity().getSensor(&sensor);
  delayMS = sensor.min_delay / 1000;
}

void loop()
{
  delay(delayMS);
  sensors_event_t event;
  dht.temperature().getEvent(&event);
  if(isnan(event.temperature))
  {
      Serial.println("Erro na leitura da Temperatura!");
  }
  else
  {
    Serial.print("T: ");
    Serial.print(event.temperature);
    Serial.println(" °C");
  }
  dht.humidity().getEvent(&event);
  if(isnan(event.relative_humidity))
  {
      Serial.println("Erro na leitura da Umidade!");
  }
  else
  {
    Serial.print("U: ");
    Serial.print(event.relative_humidity);
    Serial.println("%");
  }
}
