// =============================================================
//  CATRACA AVÍCOLA — Dois Motores de Passo
//  Motor 1: Eixo Central
//  Motor 2: Trava
// =============================================================
//
//  MAPEAMENTO DE PINOS
//  -----------------------------------------------------------
//  MOTOR 1 (Eixo Central)
//    PUL  → pino 3
//    DIR  → pino 2
//    ENA  → pino 4
//
//  MOTOR 2 (Trava)
//    PUL  → pino 6
//    DIR  → pino 5
//    ENA  → pino 7
//
//  BOTÕES — todos com INPUT_PULLUP (conectar entre o pino e GND)
//    Horário Motor 1      → pino 8
//    Anti-horário Motor 1 → pino 9
//    Disable Motor 1      → pino 10  (toggle: ativa / desativa o driver)
//
//    Horário Motor 2      → pino 11
//    Anti-horário Motor 2 → pino 12
//    Disable Motor 2      → pino A0  (toggle: ativa / desativa o driver)
//  -----------------------------------------------------------
//
//  LÓGICA DOS BOTÕES DE GIRO:
//    Toque curto (< 800 ms) → envia 1 pulso (micro-passo único)
//    Toque longo (≥ 800 ms) → giro contínuo enquanto mantiver pressionado
//
//  NOTA SOBRE ENA:
//    Na maioria dos drivers (TB6600, DM542, etc.)
//      ENA = LOW  → motor ATIVO
//      ENA = HIGH → motor DESATIVADO
//    Se o seu driver funcionar ao contrário, troque ENA_ON / ENA_OFF abaixo.
// =============================================================

// ---------- PINOS MOTOR 1 ----------
#define M1_PUL 3
#define M1_DIR 2
#define M1_ENA 4

// ---------- PINOS MOTOR 2 ----------
#define M2_PUL 6
#define M2_DIR 5
#define M2_ENA 7

// ---------- PINOS DOS BOTÕES ----------
#define BTN_M1_CW 8   // Horário Motor 1
#define BTN_M1_CCW 9  // Anti-horário Motor 1
#define BTN_M1_ENA 10 // Disable Motor 1

#define BTN_M2_CW 11  // Horário Motor 2
#define BTN_M2_CCW 12 // Anti-horário Motor 2
#define BTN_M2_ENA A0 // Disable Motor 2

// ---------- NÍVEL LÓGICO DO ENA ----------
#define ENA_ON LOW   // driver ATIVO
#define ENA_OFF HIGH // driver DESATIVADO

// ---------- DIREÇÕES ----------
#define DIR_CW HIGH // Horário
#define DIR_CCW LOW // Anti-horário

// ---------- TEMPORIZAÇÃO ----------
const unsigned long REQUIRED_TIME = 800; // ms para toque longo
const int PULSE_DELAY = 2;               // ms — duração do pulso único
const int HOLD_DELAY = 20;               // µs — semi-período do pulso contínuo

// =============================================================
//  ESTRUTURA DE ESTADO DE CADA MOTOR
// =============================================================
struct Motor
{
  // Pinos de sinal
  uint8_t pinPUL;
  uint8_t pinDIR;
  uint8_t pinENA;

  // Pinos dos botões
  uint8_t btnCW;
  uint8_t btnCCW;
  uint8_t btnEna;

  // Estado interno de giro
  unsigned long startTime;
  bool motorRunning;
  int activeBtn; // qual botão está sendo segurado (btnCW, btnCCW ou -1)

  // Estado do driver
  bool enabled;

  // Debounce do botão de disable
  bool lastBtnEna;
};

Motor m1, m2;

// =============================================================
//  INICIALIZA UMA ESTRUTURA Motor
// =============================================================
void initMotor(Motor &m,
               uint8_t pul, uint8_t dir, uint8_t ena,
               uint8_t bCW, uint8_t bCCW, uint8_t bEna)
{
  m.pinPUL = pul;
  m.pinDIR = dir;
  m.pinENA = ena;
  m.btnCW = bCW;
  m.btnCCW = bCCW;
  m.btnEna = bEna;

  m.startTime = 0;
  m.motorRunning = false;
  m.activeBtn = -1;
  m.enabled = true;
  m.lastBtnEna = HIGH;

  pinMode(pul, OUTPUT);
  pinMode(dir, OUTPUT);
  pinMode(ena, OUTPUT);
  pinMode(bCW, INPUT_PULLUP);
  pinMode(bCCW, INPUT_PULLUP);
  pinMode(bEna, INPUT_PULLUP);

  digitalWrite(pul, LOW);
  digitalWrite(dir, DIR_CW);
  digitalWrite(ena, ENA_ON);
}

// =============================================================
//  SETUP
// =============================================================
void setup()
{
  Serial.begin(9600);

  initMotor(m1,
            M1_PUL, M1_DIR, M1_ENA,
            BTN_M1_CW, BTN_M1_CCW, BTN_M1_ENA);

  initMotor(m2,
            M2_PUL, M2_DIR, M2_ENA,
            BTN_M2_CW, BTN_M2_CCW, BTN_M2_ENA);

  Serial.println("Catraca Avicola iniciada.");
  Serial.println("Motor 1 (Eixo Central) e Motor 2 (Trava) prontos.");
}

// =============================================================
//  LOOP PRINCIPAL
// =============================================================
void loop()
{
  // --- Motor 1: Eixo Central ---
  processEna(m1);
  processMotor(m1);

  // --- Motor 2: Trava ---
  processEna(m2);
  processMotor(m2);
}

// =============================================================
//  PROCESSA OS DOIS BOTÕES DE GIRO DE UM MOTOR
// =============================================================
void processMotor(Motor &m)
{
  bool cwActive = processGiroBtn(m, m.btnCW, DIR_CW);
  if (!cwActive)
  {
    processGiroBtn(m, m.btnCCW, DIR_CCW);
  }
}

// =============================================================
//  PROCESSA O BOTÃO DE ENABLE (toggle na borda de descida)
// =============================================================
void processEna(Motor &m)
{
  bool btn = digitalRead(m.btnEna);

  if (btn == LOW && m.lastBtnEna == HIGH)
  {
    m.enabled = !m.enabled;
    digitalWrite(m.pinENA, m.enabled ? ENA_ON : ENA_OFF);

    delay(20);
    // Ao desativar, para qualquer giro em andamento
    if (!m.enabled)
    {
      m.motorRunning = false;
      m.startTime = 0;
      m.activeBtn = -1;
      digitalWrite(m.pinPUL, LOW);
    }
  }

  m.lastBtnEna = btn;
}

// =============================================================
//  PROCESSA UM BOTÃO DE GIRO (CW ou CCW)
// =============================================================
bool processGiroBtn(Motor &m, uint8_t btn, uint8_t dirValue)
{
  bool pressed = (digitalRead(btn) == LOW);

  // Outro botão já está sendo segurado → ignora este
  if (m.activeBtn != -1 && m.activeBtn != btn)
    return false;

  if (pressed)
  {
    if (!m.enabled)
      return false;

    m.activeBtn = btn;
    digitalWrite(m.pinDIR, dirValue);

    if (m.startTime == 0)
      m.startTime = millis();

    // Toque longo: ativa giro contínuo
    if ((millis() - m.startTime >= REQUIRED_TIME) && !m.motorRunning)
    {
      m.motorRunning = true;
    }

    // Giro contínuo
    if (m.motorRunning)
    {
      digitalWrite(m.pinPUL, HIGH);
      delayMicroseconds(HOLD_DELAY);
      digitalWrite(m.pinPUL, LOW);
      delayMicroseconds(HOLD_DELAY);
    }

    return true;
  }
  else
  {
    // Botão solto — só trata se era este o botão ativo
    if (m.activeBtn == btn)
    {
      unsigned long dur = millis() - m.startTime;

      // Toque curto: envia um único pulso
      if (dur < REQUIRED_TIME && !m.motorRunning && m.enabled)
      {
        digitalWrite(m.pinDIR, dirValue);
        digitalWrite(m.pinPUL, HIGH);
        delay(PULSE_DELAY);
        digitalWrite(m.pinPUL, LOW);
      }

      // Reset
      m.motorRunning = false;
      m.startTime = 0;
      m.activeBtn = -1;
      digitalWrite(m.pinPUL, LOW);
    }
    return false;
  }
}
