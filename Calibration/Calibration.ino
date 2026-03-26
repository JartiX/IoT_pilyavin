#include <SoftwareSerial.h>
#include <EEPROM.h>

// ─── Пины ────────────────────────────────────────────────────
const uint8_t DIR1 = 4;
const uint8_t SPD1 = 5;   // PWM
const uint8_t SPD2 = 6;   // PWM
const uint8_t DIR2 = 7;

SoftwareSerial bt(2, 3);  // RX, TX

// ─── Состояния ───────────────────────────────────────────────
enum Phase : uint8_t { PH_DIR, PH_BAL, PH_TURN, PH_DRIVE };
const char* PH_NAME[] = { "DIRECTION", "BALANCE", "TURNS", "DRIVE" };

enum Move : uint8_t { MV_FWD, MV_BWD, MV_LFT, MV_RGT };
const char* MV_NAME[] = { "FORWARD", "BACKWARD", "LEFT", "RIGHT" };
const char  MV_KEY[]  = { 'F', 'B', 'L', 'R' };

// Все комбинации DIR1, DIR2
const bool COMBOS[4][2] = {
  {LOW,  LOW },
  {HIGH, LOW },
  {LOW,  HIGH},
  {HIGH, HIGH}
};

// ─── Переменные калибровки ───────────────────────────────────
Phase    phase      = PH_DIR;
uint8_t  curMove    = 0;
uint8_t  tryIdx     = 0;
bool     isTesting  = false;

int8_t   comboMap[4]   = {-1, -1, -1, -1};
bool     comboUsed[4]  = {false, false, false, false};

uint8_t  baseSpeed  = 180;
int8_t   balance    = 0;      // -50…+50
const int8_t BAL_STEP = 5;

uint16_t turnMs[4]  = {400, 850, 1350, 1900};
uint8_t  turnSel    = 0;
const uint16_t T_STEP = 25;
const char* ANGLE_NAME[] = { "90", "180", "270", "360" };

// ─── EEPROM ──────────────────────────────────────────────────
const uint8_t EE_MAGIC_ADDR = 0;
const uint8_t EE_DATA_ADDR  = 1;
const uint8_t MAGIC_BYTE    = 0xCA;

void saveCalibration() {
  uint8_t a = EE_DATA_ADDR;
  EEPROM.update(EE_MAGIC_ADDR, MAGIC_BYTE);
  for (uint8_t i = 0; i < 4; i++) EEPROM.update(a++, (uint8_t)comboMap[i]);
  EEPROM.update(a++, (uint8_t)(balance + 50));
  for (uint8_t i = 0; i < 4; i++) { EEPROM.put(a, turnMs[i]); a += 2; }
  bt.println(F("[OK] Saved to EEPROM"));
}

bool loadCalibration() {
  if (EEPROM.read(EE_MAGIC_ADDR) != MAGIC_BYTE) return false;
  uint8_t a = EE_DATA_ADDR;
  for (uint8_t i = 0; i < 4; i++) {
    int8_t v = (int8_t)EEPROM.read(a++);
    if (v < 0 || v > 3) return false;
    comboMap[i] = v;
    comboUsed[v] = true;
  }
  balance = (int8_t)(EEPROM.read(a++) - 50);
  for (uint8_t i = 0; i < 4; i++) { EEPROM.get(a, turnMs[i]); a += 2; }
  return true;
}

// ─── Управление моторами ─────────────────────────────────────
void stopMotors() {
  analogWrite(SPD1, 0);
  analogWrite(SPD2, 0);
  isTesting = false;
}

void runWithCombo(uint8_t ci) {
  uint8_t s1 = constrain((int16_t)baseSpeed + balance, 0, 255);
  uint8_t s2 = constrain((int16_t)baseSpeed - balance, 0, 255);
  digitalWrite(DIR1, COMBOS[ci][0]);
  digitalWrite(DIR2, COMBOS[ci][1]);
  analogWrite(SPD1, s1);
  analogWrite(SPD2, s2);
  isTesting = true;
}

void runMove(uint8_t mv) {
  if (comboMap[mv] < 0) return;
  runWithCombo(comboMap[mv]);
}

void timedMove(uint8_t mv, uint16_t ms) {
  if (comboMap[mv] < 0) return;
  runWithCombo(comboMap[mv]);
  delay(ms);
  stopMotors();
}

uint8_t nextFreeCombo(uint8_t from) {
  for (uint8_t i = 0; i < 4; i++) {
    uint8_t idx = (from + i) % 4;
    if (!comboUsed[idx]) return idx;
  }
  return 255;
}

bool allDirectionsMapped() {
  for (uint8_t i = 0; i < 4; i++) {
    if (comboMap[i] < 0) return false;
  }
  return true;
}

// ─── Вывод справки ───────────────────────────────────────────
void printPhaseHeader() {
  bt.println();
  bt.print(F("=== Phase: "));
  bt.print(PH_NAME[phase]);
  bt.println(F(" ==="));
}

void printHelp() {
  bt.println(F("--- HELP ---"));
  switch (phase) {
    case PH_DIR:
      bt.println(F("D-pad = choose move"));
      bt.println(F("[T] tri = next combo"));
      bt.println(F("[X] cross = confirm"));
      bt.println(F("[S] sq = stop motors"));
      bt.println(F("[A] start = next phase"));
      break;
    case PH_BAL:
      bt.println(F("[^] = test forward"));
      bt.println(F("[<] = boost left motor"));
      bt.println(F("[>] = boost right motor"));
      bt.println(F("[S] sq = stop motors"));
      bt.println(F("[X] cross = reset bal"));
      bt.println(F("[A] start = next phase"));
      break;
    case PH_TURN:
      bt.println(F("[^][v] = select angle"));
      bt.println(F("[<] = time - 25ms"));
      bt.println(F("[>] = time + 25ms"));
      bt.println(F("[X] cross = test RIGHT"));
      bt.println(F("[T] tri = test LEFT"));
      bt.println(F("[S] sq = stop motors"));
      bt.println(F("[A] start = next phase"));
      break;
    case PH_DRIVE:
      bt.println(F("D-pad = drive"));
      bt.println(F("[S]/release = stop"));
      bt.println(F("[C] circle = save"));
      bt.println(F("[P] pause = recalib"));
      break;
  }
}

void printStatus() {
  switch (phase) {
    case PH_DIR:
      bt.print(F("Mapped: "));
      for (uint8_t i = 0; i < 4; i++) {
        bt.print(MV_NAME[i]);
        bt.print('=');
        if (comboMap[i] >= 0) bt.print(comboMap[i]);
        else bt.print('?');
        bt.print(' ');
      }
      bt.println();
      break;
    case PH_BAL:
      bt.print(F("Bal="));
      bt.print(balance);
      bt.print(F(" SPD1="));
      bt.print(constrain((int16_t)baseSpeed + balance, 0, 255));
      bt.print(F(" SPD2="));
      bt.println(constrain((int16_t)baseSpeed - balance, 0, 255));
      break;
    case PH_TURN:
      for (uint8_t i = 0; i < 4; i++) {
        bt.print(ANGLE_NAME[i]);
        bt.print("d=");
        bt.print(turnMs[i]);
        bt.print("ms ");
      }
      bt.println();
      bt.print(F("> "));
      bt.print(ANGLE_NAME[turnSel]);
      bt.println(F(" deg"));
      break;
    case PH_DRIVE:
      bt.println(F("Free drive mode"));
      break;
  }
}

// ─── Обработчики фаз ────────────────────────────────────────

void handlePH_DIR(char c) {
  // D-pad: выбор движения
  for (uint8_t i = 0; i < 4; i++) {
    if (c == MV_KEY[i]) {
      if (comboMap[i] >= 0) {
        bt.print(MV_NAME[i]);
        bt.println(F(" already set"));
        return;
      }
      curMove = i;
      tryIdx = nextFreeCombo(0);
      if (tryIdx == 255) {
        bt.println(F("No free combos!"));
        return;
      }
      bt.print(F("Test "));
      bt.print(MV_NAME[i]);
      bt.print(F(" -> combo "));
      bt.print(tryIdx);
      bt.print(F(" (DIR1="));
      bt.print(COMBOS[tryIdx][0] ? "H" : "L");
      bt.print(F(" DIR2="));
      bt.print(COMBOS[tryIdx][1] ? "H" : "L");
      bt.println(')');
      runWithCombo(tryIdx);
      return;
    }
  }

  switch (c) {
    case 'T': // △ — следующая комбинация
      stopMotors();
      {
        uint8_t nxt = nextFreeCombo(tryIdx + 1);
        if (nxt == 255) nxt = nextFreeCombo(0);
        if (nxt == tryIdx) bt.println(F("Only 1 combo left"));
        tryIdx = nxt;
        bt.print(F("Try combo "));
        bt.print(tryIdx);
        bt.print(F(" (DIR1="));
        bt.print(COMBOS[tryIdx][0] ? "H" : "L");
        bt.print(F(" DIR2="));
        bt.print(COMBOS[tryIdx][1] ? "H" : "L");
        bt.println(')');
        runWithCombo(tryIdx);
      }
      break;

    case 'X': // ✕ — подтвердить
      stopMotors();
      if (comboMap[curMove] >= 0) {
        bt.println(F("Already confirmed"));
        break;
      }
      comboMap[curMove] = tryIdx;
      comboUsed[tryIdx] = true;
      bt.print(F("[OK] "));
      bt.print(MV_NAME[curMove]);
      bt.print(F(" = combo "));
      bt.println(tryIdx);
      printStatus();
      if (allDirectionsMapped()) {
        bt.println(F("All mapped! Press [A]"));
      }
      break;

    case 'S': // □ — стоп
      stopMotors();
      bt.println(F("Stopped"));
      break;

    case '0': // отпускание
      stopMotors();
      break;

    case 'A': // Start — следующая фаза
      stopMotors();
      if (!allDirectionsMapped()) {
        bt.println(F("Map all 4 first!"));
        printStatus();
      } else {
        phase = PH_BAL;
        printPhaseHeader();
        printHelp();
        printStatus();
      }
      break;
  }
}

void handlePH_BAL(char c) {
  switch (c) {
    case 'F': // ↑ — тест вперёд
      runMove(MV_FWD);
      bt.println(F("FWD..."));
      printStatus();
      break;

    case 'L': // ← — усилить левый
      stopMotors();
      balance = constrain(balance - BAL_STEP, -50, 50);
      printStatus();
      break;

    case 'R': // → — усилить правый
      stopMotors();
      balance = constrain(balance + BAL_STEP, -50, 50);
      printStatus();
      break;

    case 'X': // ✕ — сброс
      balance = 0;
      bt.println(F("Balance reset"));
      printStatus();
      break;

    case 'S':
    case '0':
      stopMotors();
      break;

    case 'A': // Start — следующая фаза
      stopMotors();
      phase = PH_TURN;
      printPhaseHeader();
      printHelp();
      printStatus();
      break;
  }
}

void handlePH_TURN(char c) {
  switch (c) {
    case 'F': // ↑ — следующий угол
      turnSel = (turnSel + 1) % 4;
      bt.print(F("> "));
      bt.print(ANGLE_NAME[turnSel]);
      bt.print(F("d = "));
      bt.print(turnMs[turnSel]);
      bt.println(F("ms"));
      break;

    case 'B': // ↓ — предыдущий угол
      turnSel = (turnSel + 3) % 4;
      bt.print(F("> "));
      bt.print(ANGLE_NAME[turnSel]);
      bt.print(F("d = "));
      bt.print(turnMs[turnSel]);
      bt.println(F("ms"));
      break;

    case 'L': // ← — меньше
      turnMs[turnSel] = max(50, turnMs[turnSel] - T_STEP);
      bt.print(ANGLE_NAME[turnSel]);
      bt.print(F("d = "));
      bt.print(turnMs[turnSel]);
      bt.println(F("ms"));
      break;

    case 'R': // → — больше
      turnMs[turnSel] = min(5000, turnMs[turnSel] + T_STEP);
      bt.print(ANGLE_NAME[turnSel]);
      bt.print(F("d = "));
      bt.print(turnMs[turnSel]);
      bt.println(F("ms"));
      break;

    case 'X': // ✕ — тест ВПРАВО
      bt.print(F("RIGHT "));
      bt.print(ANGLE_NAME[turnSel]);
      bt.print(F("d ("));
      bt.print(turnMs[turnSel]);
      bt.println(F("ms)"));
      timedMove(MV_RGT, turnMs[turnSel]);
      bt.println(F("Done"));
      break;

    case 'T': // △ — тест ВЛЕВО
      bt.print(F("LEFT "));
      bt.print(ANGLE_NAME[turnSel]);
      bt.print(F("d ("));
      bt.print(turnMs[turnSel]);
      bt.println(F("ms)"));
      timedMove(MV_LFT, turnMs[turnSel]);
      bt.println(F("Done"));
      break;

    case 'S':
    case '0':
      stopMotors();
      break;

    case 'A': // Start — следующая фаза
      stopMotors();
      phase = PH_DRIVE;
      printPhaseHeader();
      printHelp();
      break;
  }
}

void handlePH_DRIVE(char c) {
  switch (c) {
    case 'F': runMove(MV_FWD); break;
    case 'B': runMove(MV_BWD); break;
    case 'L': runMove(MV_LFT); break;
    case 'R': runMove(MV_RGT); break;

    case 'S':
    case '0':
      stopMotors();
      break;

    case 'C': // ○ — сохранить
      stopMotors();
      saveCalibration();
      break;

    case 'P': // Pause — перекалибровка
      stopMotors();
      for (uint8_t i = 0; i < 4; i++) {
        comboMap[i] = -1;
        comboUsed[i] = false;
      }
      balance = 0;
      turnMs[0] = 400; turnMs[1] = 850;
      turnMs[2] = 1350; turnMs[3] = 1900;
      turnSel = 0;
      phase = PH_DIR;
      printPhaseHeader();
      printHelp();
      break;
  }
}

// ─── setup / loop ────────────────────────────────────────────

void setup() {
  pinMode(DIR1, OUTPUT);
  pinMode(DIR2, OUTPUT);
  pinMode(SPD1, OUTPUT);
  pinMode(SPD2, OUTPUT);
  stopMotors();

  Serial.begin(9600);
  bt.begin(9600);

  bt.println(F("=== Motor Calibrator ==="));

  if (loadCalibration()) {
    bt.println(F("Loaded EEPROM data"));
    phase = PH_DRIVE;
    printPhaseHeader();
    printHelp();
    printStatus();
  } else {
    bt.println(F("No saved data"));
    phase = PH_DIR;
    printPhaseHeader();
    printHelp();
  }
}

void loop() {
  if (!bt.available()) return;

  char c = bt.read();
  if (c == '\n' || c == '\r' || c == ' ') return;

  Serial.print(F("Recv: "));
  Serial.println(c);

  switch (phase) {
    case PH_DIR:   handlePH_DIR(c);   break;
    case PH_BAL:   handlePH_BAL(c);   break;
    case PH_TURN:  handlePH_TURN(c);  break;
    case PH_DRIVE: handlePH_DRIVE(c); break;
  }
}
