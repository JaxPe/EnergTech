/* 10 min on ou 1 min on dependendo do nivel lógico de 1 porta
 * botão para start
 * LED on enquanto está em funcionamento
 */

#include <Wire.h>
#include <LiquidCrystal_I2C.h>

#define BUTTONSTART 2
#define LED 3
#define WATERDISPENSER 4
#define SENSOR 5
#define BUTTONSTOP 7
#define SWITCHTIME 12

#define BOUNCINGTIME 400

#define COLUMN 20
#define LINE 4
#define ADDR 0x27

LiquidCrystal_I2C lcd(ADDR, COLUMN, LINE);

int lastStartButtonState = HIGH;
int startButtonState;
int lastStopButtonState = HIGH;
int stopButtonState;
unsigned long lastStartDebounceTime = 0;
unsigned long lastStopDebounceTime = 0;
unsigned long debounceTime = 50;

bool lastCalibrationState = false;
bool calibrationRunning = false;
unsigned long calibrationStartTime = 0;
unsigned long calibrationDuration = 0;

unsigned long lastDisplayedTime = 0;

int lastSensorState = HIGH;
unsigned long sensorTriggerTime = 0;

const unsigned long SHORT_TIME = 60000;
const unsigned long LONG_TIME = 600000;

void setup()
{
  Wire.begin();
  Serial.begin(9600);

  lcd.init();
  lcd.backlight();
  lcd.home();

  pinMode(LED, OUTPUT);
  digitalWrite(LED, LOW);
  pinMode(WATERDISPENSER, OUTPUT);
  digitalWrite(WATERDISPENSER, LOW);
  pinMode(SWITCHTIME, INPUT_PULLUP);
  pinMode(BUTTONSTART, INPUT_PULLUP);
  pinMode(BUTTONSTOP, INPUT_PULLUP);

  Serial.println("CLEARDATA");
  Serial.println("LABEL, Data, Hora, tempo (s)");

  updateDisplay(false, 0, digitalRead(SWITCHTIME) == LOW);
}

void loop()
{
  int startButtonReading = digitalRead(BUTTONSTART);
  int stopButtonReading = digitalRead(BUTTONSTOP);

  if (startButtonReading != lastStartButtonState)
  {
    lastStartDebounceTime = millis();
  }

  if ((millis() - lastStartDebounceTime) > debounceTime)
  {
    if (startButtonReading != startButtonState)
    {
      startButtonState = startButtonReading;
      if (startButtonState == LOW)
      {
        Serial.println("click");

        if (!calibrationRunning)
        {
          startCalibration();
        }
      }
    }
  }

  lastStartButtonState = startButtonReading;

  if (stopButtonReading != lastStopButtonState)
  {
    lastStopDebounceTime = millis();
  }

  if ((millis() - lastStopDebounceTime) > debounceTime)
  {
    if (stopButtonReading != stopButtonState)
    {
      stopButtonState = stopButtonReading;

      if (stopButtonState == LOW)
      {
        endCalibration();
      }
    }
  }

  lastStopButtonState = stopButtonReading;

  if (calibrationRunning)
  {
    int sensorReading = digitalRead(SENSOR);

    if (sensorReading == LOW && lastSensorState == HIGH)
    {
      sensorTriggerTime = millis() - calibrationStartTime;

      Serial.print("DATA,DATE,TIME,");
      Serial.println(sensorTriggerTime / 1000.0);

      endCalibration();
    }
    lastSensorState = sensorReading;

    if (millis() - calibrationStartTime >= calibrationDuration)
    {
      endCalibration();
    }
  }

  bool stateChanged = (calibrationRunning != lastCalibrationState);
  bool timeChanged = (calibrationRunning && sensorTriggerTime != lastDisplayedTime);

  if (stateChanged || timeChanged)
  {
    lastCalibrationState = calibrationRunning;
    lastDisplayedTime = sensorTriggerTime;
    updateDisplay(calibrationRunning, sensorTriggerTime, digitalRead(SWITCHTIME) == LOW);
  }
}

void startCalibration()
{
  int configTime = digitalRead(SWITCHTIME);

  if (configTime == HIGH)
  {
    calibrationDuration = SHORT_TIME;
  }
  else
  {
    calibrationDuration = LONG_TIME;
  }

  calibrationRunning = true;
  calibrationStartTime = millis();
  digitalWrite(LED, HIGH);
  digitalWrite(WATERDISPENSER, HIGH);
  sensorTriggerTime = 0;
}

void endCalibration()
{
  calibrationRunning = false;
  digitalWrite(WATERDISPENSER, LOW);
  digitalWrite(LED, LOW);
}

void updateDisplay(bool isCalibrating, unsigned long activationTime, bool isShort)
{
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("  Calibrador Vazao");
  lcd.setCursor(0, 1);
  lcd.print(isCalibrating ? "Calibrando" : "OFF");
  lcd.setCursor(0, 2);
  lcd.print("Acionamento: ");
  lcd.setCursor(13, 2);
  lcd.print(activationTime / 1000.0, 1);
  lcd.setCursor(19, 2);
  lcd.print("s");
  lcd.setCursor(0, 3);
  lcd.print(isShort ? "Duracao: 10 min" : "Duracao: 1 min");
}
