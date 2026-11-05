#define button 8
#define pulPin 3
#define dirPin 2

unsigned long startTime = 0;
const long requiredTime = 800;
bool motorRunning = false;
const int pulseDelay = 2;
const int holdDelay = 20;

void setup() {
  pinMode(pulPin, OUTPUT);
  pinMode(button, INPUT_PULLUP);
  pinMode(dirPin, OUTPUT);
  Serial.begin(9600);
  Serial.println("Iniciado");

  digitalWrite(dirPin, HIGH);
}

void loop() {
  if (digitalRead(button) == LOW) {
    if (startTime == 0) {
      startTime = millis();
    }

    if ((millis() - startTime >= requiredTime) && (motorRunning == false)) {
      motorRunning = true;
      Serial.println("Toque longo");
    }

    if (motorRunning == true) {
      digitalWrite(pulPin, HIGH);
      delayMicroseconds(holdDelay);
      digitalWrite(pulPin, LOW);
      delayMicroseconds(holdDelay);
    }
  } else {

    // Se houve um toque anterior (startTime > 0)
    if (startTime > 0) {
      unsigned long pressDuration = millis() - startTime;

      // VERIFICAÇÃO DO TOQUE RÁPIDO (< 0.8s)
      // Se a duração da pressão foi menor que o tempo exigido E o motor não estava girando
      if (pressDuration < requiredTime && motorRunning == false) {

        Serial.println("Toque rápido detectado. Enviando pulso único (1/32 passo).");

        // ENVIA O PULSO ÚNICO (Para o seu 1/32 de micro-passo)
        digitalWrite(pulPin, HIGH);
        delay(pulseDelay);  // Duração mínima do pulso
        digitalWrite(pulPin, LOW);
      }

      // Se o motor estava rodando, ele simplesmente para aqui.
      if (motorRunning == true) {
        Serial.println("Botão solto. Parando o giro contínuo.");
      }
    }

    // RESETA TODAS AS VARIÁVEIS para o próximo toque
    motorRunning = false;
    startTime = 0;
    digitalWrite(pulPin, LOW);  // Garante que o pino PUL esteja seguro
  }
}
