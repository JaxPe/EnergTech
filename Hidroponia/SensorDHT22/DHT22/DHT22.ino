#include <Adafruit_Sensor.h>
#include <DHT.h>
#include <DHT_U.h>

#define DHTTYPE DHT22
#define DHTPIN 2
DHT_Unified dht(DHTPIN, DHTTYPE);
uint32_t delayMS;

String comandoRecebido = "";

void setup()
{
  Serial.begin(9600);
  dht.begin();
  Serial.println("Inicializando sensor DHT");
  sensor_t sensor;
  dht.temperature().getSensor(&sensor);
  Serial.println("------------------------------------");
  Serial.println("Temperatura");
  Serial.print  ("Sensor:       "); Serial.println(sensor.name);
  Serial.print  ("Valor max:    "); Serial.print(sensor.max_value); Serial.println(" *C");
  Serial.print  ("Valor min:    "); Serial.print(sensor.min_value); Serial.println(" *C");
  Serial.print  ("Resolucao:   "); Serial.print(sensor.resolution); Serial.println(" *C");
  Serial.println("------------------------------------");
  dht.humidity().getSensor(&sensor);
  Serial.println("------------------------------------");
  Serial.println("Umidade");
  Serial.print  ("Sensor:       "); Serial.println(sensor.name);
  Serial.print  ("Valor max:    "); Serial.print(sensor.max_value); Serial.println("%");
  Serial.print  ("Valor min:    "); Serial.print(sensor.min_value); Serial.println("%");
  Serial.print  ("Resolucao:   "); Serial.print(sensor.resolution); Serial.println("%");
  Serial.println("------------------------------------");
  delayMS = sensor.min_delay / 1000;
}

void loop()
{
  delay(delayMS);
  float T, H;
  leituraSensores(T, H);
  Serial.print("T:");   Serial.print(T);
  Serial.print(";U:");  Serial.println(H);
}

void leituraSensores(float &temperture, float &humidity) {
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
