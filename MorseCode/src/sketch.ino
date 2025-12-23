/*
 * Morse Code Communication System
 * Half-duplex communication between two Arduino microcontrollers
 */

// ==================== PIN CONFIGURATION ====================
const int SENSOR_PIN = 2;
const int RX_PIN = 3;
const int TX_PIN = 5;
const int DATA_PIN = 8;
const int CLOCK_PIN = 9;
const int LATCH_PIN = 10;
const int LED_TX = 11;
const int LED_RX = 12;

// ==================== TIMING PARAMETERS ====================
const unsigned long DOT_DURATION = 200;
const unsigned long DASH_DURATION = 600;
const unsigned long SYMBOL_GAP = 200;
const unsigned long LETTER_GAP = 600;
const unsigned long WORD_GAP = 1400;
const unsigned long DOT_MIN = 120;
const unsigned long DOT_MAX = 350;
const unsigned long DASH_MIN = 400;
const unsigned long DASH_MAX = 900;
const unsigned long LETTER_GAP_MIN = 400;
const unsigned long WORD_GAP_MIN = 1000;
const unsigned long RAW_AUTO_START_TIMEOUT = 3000;
const unsigned long AUTO_START_TIMEOUT = 3000;

// ==================== OPERATING MODES ====================
enum Mode { MODE_AUTO, MODE_MANUAL, MODE_RAW };
Mode currentMode = MODE_AUTO;

// ==================== TIMER MANAGEMENT ====================
volatile bool displayTimerActive = false;
volatile unsigned int displayTimerTicks = 0;
const unsigned int DISPLAY_HOLD_TICKS = 5;  // 5 × 100ms = 500ms

volatile bool rxResetTimerActive = false;
volatile unsigned int rxResetTimerTicks = 0;
const unsigned int RX_RESET_TICKS = 20;  // 20 × 100ms = 2000ms

volatile bool txCompleteTimerActive = false;
volatile unsigned int txCompleteTimerTicks = 0;
const unsigned int TX_COMPLETE_TICKS = 1;  // 1 × 100ms = 100ms

volatile bool pendingReceiveInit = false;
volatile bool pendingTxIdle = false;
volatile bool pendingDisplayClear = false;

// ==================== MORSE TABLE IN PROGMEM ====================
const char morse_A[] PROGMEM = ".-";
const char morse_B[] PROGMEM = "-...";
const char morse_C[] PROGMEM = "-.-.";
const char morse_D[] PROGMEM = "-..";
const char morse_E[] PROGMEM = ".";
const char morse_F[] PROGMEM = "..-.";
const char morse_G[] PROGMEM = "--.";
const char morse_H[] PROGMEM = "....";
const char morse_I[] PROGMEM = "..";
const char morse_J[] PROGMEM = ".---";
const char morse_K[] PROGMEM = "-.-";
const char morse_L[] PROGMEM = ".-..";
const char morse_M[] PROGMEM = "--";
const char morse_N[] PROGMEM = "-.";
const char morse_O[] PROGMEM = "---";
const char morse_P[] PROGMEM = ".--.";
const char morse_Q[] PROGMEM = "--.-";
const char morse_R[] PROGMEM = ".-.";
const char morse_S[] PROGMEM = "...";
const char morse_T[] PROGMEM = "-";
const char morse_U[] PROGMEM = "..-";
const char morse_V[] PROGMEM = "...-";
const char morse_W[] PROGMEM = ".--";
const char morse_X[] PROGMEM = "-..-";
const char morse_Y[] PROGMEM = "-.--";
const char morse_Z[] PROGMEM = "--..";
const char morse_0[] PROGMEM = "-----";
const char morse_1[] PROGMEM = ".----";
const char morse_2[] PROGMEM = "..---";
const char morse_3[] PROGMEM = "...--";
const char morse_4[] PROGMEM = "....-";
const char morse_5[] PROGMEM = ".....";
const char morse_6[] PROGMEM = "-....";
const char morse_7[] PROGMEM = "--...";
const char morse_8[] PROGMEM = "---..";
const char morse_9[] PROGMEM = "----.";
const char morse_SPACE[] PROGMEM = "/";

const char* const morseTable[] PROGMEM = {
  morse_A, morse_B, morse_C, morse_D, morse_E, morse_F, morse_G, morse_H,
  morse_I, morse_J, morse_K, morse_L, morse_M, morse_N, morse_O, morse_P,
  morse_Q, morse_R, morse_S, morse_T, morse_U, morse_V, morse_W, morse_X,
  morse_Y, morse_Z, morse_0, morse_1, morse_2, morse_3, morse_4, morse_5,
  morse_6, morse_7, morse_8, morse_9, morse_SPACE
};

const char morseChars[] PROGMEM = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789 ";

// ==================== 7-SEGMENT CODES IN PROGMEM ====================
const byte segmentCodes[] PROGMEM = {
  0b00111111, 0b00000110, 0b01011011, 0b01001111, 0b01100110, 0b01101101,
  0b01111101, 0b00000111, 0b01111111, 0b01101111, 0b01110111, 0b01111100,
  0b00111001, 0b01011110, 0b01111001, 0b01110001, 0b00111101, 0b01110110,
  0b00000110, 0b00011110, 0b01110110, 0b00111000, 0b01010101, 0b01010100,
  0b00111111, 0b01110011, 0b01100111, 0b01010000, 0b01101101, 0b01111000,
  0b00111110, 0b00111110, 0b01111110, 0b01110110, 0b01101110, 0b01011011,
  0b00000000
};

// ==================== STATE MACHINES ====================
enum TxState { TX_IDLE, TX_SENDING, TX_COMPLETE };
enum RxState { RX_IDLE, RX_RECEIVING, RX_COMPLETE, RX_ERROR };

// ==================== GLOBAL VARIABLES ====================
TxState txState = TX_IDLE;
char txBuffer[64];
int txIndex = 0;
char currentMorseCode[8];
int morseIndex = 0;
unsigned long txLastTime = 0;
bool txPinState = LOW;

RxState rxState = RX_IDLE;
char rxBuffer[64];
char currentMorseReceived[8];
volatile unsigned long pulseStartTime = 0;
volatile unsigned long pulseDuration = 0;
volatile bool newPulseComplete = false;
volatile unsigned long lastSignalTime = 0;
volatile bool signalActive = false;

volatile unsigned long buttonPressTime = 0;
volatile bool buttonPressed = false;
volatile bool buttonReleased = false;
char manualMorseCode[8];
unsigned long lastManualActivity = 0;
char manualBuffer[32];

volatile bool rawModeActive = false;
bool rawRxMode = false;
unsigned long rawLastReceiveTime = 0;
bool rawStarted = false;
bool autoStarted = false;
unsigned long autoLastReceiveTime = 0;
char autoMessage[64];

// ==================== TIMER INTERRUPT ====================
ISR(TIMER1_COMPA_vect) {
  // Display hold timer
  if (displayTimerActive) {
    displayTimerTicks++;
    if (displayTimerTicks >= DISPLAY_HOLD_TICKS) {
      displayTimerActive = false;
      displayTimerTicks = 0;
      pendingDisplayClear = true;
    }
  }
  
  // RX reset timer
  if (rxResetTimerActive) {
    rxResetTimerTicks++;
    if (rxResetTimerTicks >= RX_RESET_TICKS) {
      rxResetTimerActive = false;
      rxResetTimerTicks = 0;
      pendingReceiveInit = true;
    }
  }
  
  // TX complete timer
  if (txCompleteTimerActive) {
    txCompleteTimerTicks++;
    if (txCompleteTimerTicks >= TX_COMPLETE_TICKS) {
      txCompleteTimerActive = false;
      txCompleteTimerTicks = 0;
      pendingTxIdle = true;
    }
  }
}

// ==================== TIMER SETUP ====================
void setupTimer1() {
  // Stop timer
  TCCR1A = 0;
  TCCR1B = 0;
  TCNT1 = 0;
  
  // Setup for 100ms interval (10 Hz)
  // For 16MHz Arduino: (16000000 / (prescaler * Hz)) - 1
  // prescaler = 256, Hz = 10
  OCR1A = 6249;  // (16000000 / (256 * 10)) - 1
  
  // CTC mode
  TCCR1B |= (1 << WGM12);
  
  // Prescaler 256
  TCCR1B |= (1 << CS12);
  
  TIMSK1 |= (1 << OCIE1A);
}

// ==================== MORSE ENCODING/DECODING ====================
void encodeToMorse(char c, char* output) {
  c = toupper(c);
  output[0] = '\0';
  
  for (int i = 0; i < 37; i++) {
    char ch = pgm_read_byte(&morseChars[i]);
    if (ch == c) {
      strcpy_P(output, (char*)pgm_read_ptr(&morseTable[i]));
      return;
    }
  }
}

char decodeFromMorse(const char* morse) {
  char buffer[8];
  for (int i = 0; i < 37; i++) {
    strcpy_P(buffer, (char*)pgm_read_ptr(&morseTable[i]));
    if (strcmp(buffer, morse) == 0) {
      return pgm_read_byte(&morseChars[i]);
    }
  }
  return '?';
}

// ==================== 7-SEGMENT DISPLAY ====================
void displayChar(char c) {
  byte code = 0;
  c = toupper(c);
  if (c >= '0' && c <= '9') {
    code = pgm_read_byte(&segmentCodes[c - '0']);
  } else if (c >= 'A' && c <= 'Z') {
    code = pgm_read_byte(&segmentCodes[10 + (c - 'A')]);
  }
  digitalWrite(LATCH_PIN, LOW);
  shiftOut(DATA_PIN, CLOCK_PIN, MSBFIRST, code);
  digitalWrite(LATCH_PIN, HIGH);
  
  displayTimerActive = true;
  displayTimerTicks = 0;
}

void clearDisplay() {
  digitalWrite(LATCH_PIN, LOW);
  shiftOut(DATA_PIN, CLOCK_PIN, MSBFIRST, 0);
  digitalWrite(LATCH_PIN, HIGH);
}

// ==================== INTERRUPT SERVICE ROUTINES ====================
void buttonISR() {
  unsigned long now = millis();
  bool state = digitalRead(SENSOR_PIN);
  if (state == HIGH && !buttonPressed) {
    buttonPressTime = now;
    buttonPressed = true;
    buttonReleased = false;
  } else if (state == LOW && buttonPressed) {
    buttonPressed = false;
    buttonReleased = true;
  }
}

void rxISR() {
  unsigned long now = millis();
  bool state = digitalRead(RX_PIN);
  if (state == HIGH && !signalActive) {
    pulseStartTime = now;
    signalActive = true;
    digitalWrite(LED_RX, HIGH);
  } else if (state == LOW && signalActive) {
    pulseDuration = now - pulseStartTime;
    signalActive = false;
    newPulseComplete = true;
    lastSignalTime = now;
    digitalWrite(LED_RX, LOW);
  }
}

// ==================== TRANSMITTER ====================
void transmitInit(const char* message) {
  strncpy(txBuffer, message, 63);
  txBuffer[63] = '\0';
  txIndex = 0;
  txState = TX_SENDING;
  if (strlen(txBuffer) > 0) {
    encodeToMorse(txBuffer[0], currentMorseCode);
  } else {
    currentMorseCode[0] = '\0';
  }
  morseIndex = 0;
  txLastTime = millis();
  txPinState = LOW;
  digitalWrite(TX_PIN, LOW);
  digitalWrite(LED_TX, HIGH);
  Serial.println(F("\n=== TX: Sending ==="));
  Serial.print(F("TX: \""));
  Serial.print(message);
  Serial.println(F("\""));
}

void transmitUpdate() {
  if (txState == TX_IDLE || txState == TX_COMPLETE) return;
  unsigned long now = millis();
  unsigned long elapsed = now - txLastTime;
  
  if (morseIndex < strlen(currentMorseCode)) {
    char symbol = currentMorseCode[morseIndex];
    if (txPinState == LOW) {
      digitalWrite(TX_PIN, HIGH);
      txPinState = HIGH;
      txLastTime = now;
    } else {
      unsigned long duration = (symbol == '.') ? DOT_DURATION : DASH_DURATION;
      if (elapsed >= duration) {
        digitalWrite(TX_PIN, LOW);
        txPinState = LOW;
        txLastTime = now;
        morseIndex++;
      }
    }
  } else if (txPinState == LOW && elapsed >= SYMBOL_GAP) {
    if (elapsed >= LETTER_GAP) {
      txIndex++;
      if (txIndex < strlen(txBuffer)) {
        encodeToMorse(txBuffer[txIndex], currentMorseCode);
        morseIndex = 0;
        txLastTime = now;
      } else {
        txState = TX_COMPLETE;
        digitalWrite(LED_TX, LOW);
        Serial.println(F("TX: Done\n"));
      }
    }
  }
}

// ==================== RECEIVER ====================
void receiveInit() {
  rxState = RX_IDLE;
  rxBuffer[0] = '\0';
  currentMorseReceived[0] = '\0';
  newPulseComplete = false;
  signalActive = false;
  lastSignalTime = millis();
  autoStarted = false;
  rawStarted = false;
  digitalWrite(LED_RX, LOW);
  Serial.println(F("RX: Ready"));
}

void receiveUpdate() {
  unsigned long now = millis();
  
  if (newPulseComplete) {
    noInterrupts();
    unsigned long duration = pulseDuration;
    newPulseComplete = false;
    lastSignalTime = now;
    interrupts();
    
    if (rawRxMode && !rawStarted) {
      rawStarted = true;
      rawLastReceiveTime = now;
      Serial.println(F("\n=== RAW: Auto Start ==="));
    }
    
    if (!rawRxMode && !autoStarted) {
      autoStarted = true;
      autoLastReceiveTime = now;
      rxState = RX_RECEIVING;
      Serial.println(F("\n=== AUTO: Receiving ==="));
    }
    
    int len = strlen(currentMorseReceived);
    if (len < 7) {
      if (duration >= DOT_MIN && duration <= DOT_MAX) {
        currentMorseReceived[len] = '.';
        currentMorseReceived[len + 1] = '\0';
        Serial.print('.');
      } else if (duration >= DASH_MIN && duration <= DASH_MAX) {
        currentMorseReceived[len] = '-';
        currentMorseReceived[len + 1] = '\0';
        Serial.print('-');
      } else if (duration > 50 && duration < DOT_MIN) {
        currentMorseReceived[len] = '.';
        currentMorseReceived[len + 1] = '\0';
        Serial.print('.');
      } else if (duration > DOT_MAX && duration < DASH_MIN) {
        currentMorseReceived[len] = '.';
        currentMorseReceived[len + 1] = '\0';
        Serial.print('.');
      }
    }
    rawLastReceiveTime = now;
    autoLastReceiveTime = now;
  }
  
  if (rawRxMode && !signalActive && strlen(currentMorseReceived) > 0) {
    unsigned long silenceDuration = now - lastSignalTime;
    
    if (silenceDuration >= LETTER_GAP_MIN) {
      Serial.print(F(" ["));
      Serial.print(currentMorseReceived);
      Serial.print(F("] "));
      
      char decoded = decodeFromMorse(currentMorseReceived);
      Serial.print(decoded);
      displayChar(decoded);
      currentMorseReceived[0] = '\0';
      
      if (silenceDuration >= WORD_GAP_MIN) {
        Serial.print(F(" [SP] "));
      }
    }
    return;
  }
  
  if (rawRxMode && rawStarted && (now - rawLastReceiveTime) > RAW_AUTO_START_TIMEOUT) {
    rawStarted = false;
    Serial.println(F("\n=== RAW: Timeout - Ready for next ==="));
  }
  
  if (!rawRxMode && !signalActive && strlen(currentMorseReceived) > 0) {
    unsigned long silenceDuration = now - lastSignalTime;
    
    if (silenceDuration >= LETTER_GAP_MIN && silenceDuration < WORD_GAP_MIN) {
      Serial.print(F(" ["));
      Serial.print(currentMorseReceived);
      Serial.print(F("] "));
      
      char decoded = decodeFromMorse(currentMorseReceived);
      Serial.print(decoded);
      
      int len = strlen(rxBuffer);
      if (len < 63) {
        rxBuffer[len] = decoded;
        rxBuffer[len + 1] = '\0';
      }
      displayChar(decoded);
      currentMorseReceived[0] = '\0';
    } else if (silenceDuration >= WORD_GAP_MIN) {
      if (strlen(currentMorseReceived) > 0) {
        Serial.print(F(" ["));
        Serial.print(currentMorseReceived);
        Serial.print(F("] "));
        
        char decoded = decodeFromMorse(currentMorseReceived);
        Serial.print(decoded);
        
        int len = strlen(rxBuffer);
        if (len < 63) {
          rxBuffer[len] = decoded;
          rxBuffer[len + 1] = '\0';
        }
        displayChar(decoded);
        currentMorseReceived[0] = '\0';
      }
      
      int len = strlen(rxBuffer);
      if (len < 63) {
        rxBuffer[len] = ' ';
        rxBuffer[len + 1] = '\0';
      }
      Serial.print(F(" [SP] "));
    }
  }
  
  if (!rawRxMode && autoStarted && (now - autoLastReceiveTime) > AUTO_START_TIMEOUT) {
    if (strlen(rxBuffer) > 0) {
      rxState = RX_COMPLETE;
      Serial.println(F("\n=== AUTO: Complete ==="));
      Serial.print(F("RX: \""));
      Serial.print(rxBuffer);
      Serial.println(F("\""));
    }
    autoStarted = false;
  }
}

// ==================== MANUAL MODE ====================
void manualModeUpdate() {
  unsigned long now = millis();
  if (buttonReleased) {
    buttonReleased = false;
    unsigned long duration = now - buttonPressTime;
    int len = strlen(manualMorseCode);
    if (len < 7) {
      if (duration >= DOT_MIN && duration <= DOT_MAX) {
        manualMorseCode[len] = '.';
        manualMorseCode[len + 1] = '\0';
        Serial.print('.');
      } else if (duration >= DASH_MIN && duration <= DASH_MAX) {
        manualMorseCode[len] = '-';
        manualMorseCode[len + 1] = '\0';
        Serial.print('-');
      }
    }
    lastManualActivity = now;
  }
  
  if (strlen(manualMorseCode) > 0 && (now - lastManualActivity) > LETTER_GAP) {
    Serial.print(F(" ["));
    Serial.print(manualMorseCode);
    Serial.print(F("] "));
    char decoded = decodeFromMorse(manualMorseCode);
    Serial.print(decoded);
    int len = strlen(manualBuffer);
    if (len < 31) {
      manualBuffer[len] = decoded;
      manualBuffer[len + 1] = '\0';
    }
    displayChar(decoded);
    manualMorseCode[0] = '\0';
    lastManualActivity = now;
  }
  
  if (strlen(manualBuffer) > 0 && (now - lastManualActivity) > WORD_GAP) {
    if (txState == TX_IDLE) {
      Serial.print(F("\n[TX: \""));
      Serial.print(manualBuffer);
      Serial.println(F("\"]"));
      transmitInit(manualBuffer);
      manualBuffer[0] = '\0';
    }
  }
}

// ==================== RAW MODE ====================
void rawModeUpdate() {
  bool sensorState = digitalRead(SENSOR_PIN);
  digitalWrite(TX_PIN, sensorState);
  digitalWrite(LED_TX, sensorState ? HIGH : LOW);
}

// ==================== SETUP ====================
void setup() {
  Serial.begin(115200);
  while (!Serial) { delay(10); }
  Serial.println(F("\n========================="));
  Serial.println(F("Morse Code System"));
  Serial.println(F("SMART COMMANDS"));
  Serial.println(F("========================="));
  Serial.println(F("Commands:"));
  Serial.println(F("  a - AUTO mode"));
  Serial.println(F("  m - MANUAL mode"));
  Serial.println(F("  r - RAW mode"));
  Serial.println(F("=========================\n"));
  
  pinMode(SENSOR_PIN, INPUT_PULLUP);
  pinMode(TX_PIN, OUTPUT);
  pinMode(RX_PIN, INPUT);
  pinMode(LED_TX, OUTPUT);
  pinMode(LED_RX, OUTPUT);
  pinMode(DATA_PIN, OUTPUT);
  pinMode(CLOCK_PIN, OUTPUT);
  pinMode(LATCH_PIN, OUTPUT);
  
  digitalWrite(TX_PIN, LOW);
  digitalWrite(LED_TX, LOW);
  digitalWrite(LED_RX, LOW);
  clearDisplay();
  
  setupTimer1();
  
  attachInterrupt(digitalPinToInterrupt(SENSOR_PIN), buttonISR, CHANGE);
  attachInterrupt(digitalPinToInterrupt(RX_PIN), rxISR, CHANGE);
  
  txBuffer[0] = '\0';
  rxBuffer[0] = '\0';
  currentMorseReceived[0] = '\0';
  manualMorseCode[0] = '\0';
  manualBuffer[0] = '\0';
  autoMessage[0] = '\0';
  
  receiveInit();
  Serial.println(F("Mode: AUTO"));
}

// ==================== MAIN LOOP ====================
void loop() {
  if (pendingDisplayClear) {
    pendingDisplayClear = false;
  }
  
  if (pendingReceiveInit) {
    pendingReceiveInit = false;
    receiveInit();
  }
  
  if (pendingTxIdle) {
    pendingTxIdle = false;
    txState = TX_IDLE;
  }
  
  if (Serial.available()) {
    char cmd = Serial.read();
    
    if (currentMode == MODE_AUTO) {
      if (cmd == '\n' || cmd == '\r') {
        if (strlen(autoMessage) == 1) {
          char singleChar = autoMessage[0];
          
          if (singleChar == 'a') {
            currentMode = MODE_AUTO;
            rawModeActive = false;
            rawRxMode = false;
            autoMessage[0] = '\0';
            Serial.println(F("\n>>> AUTO (already in AUTO)"));
            return;
          } else if (singleChar == 'm') {
            currentMode = MODE_MANUAL;
            rawModeActive = false;
            rawRxMode = false;
            autoMessage[0] = '\0';
            manualBuffer[0] = '\0';
            Serial.println(F("\n>>> MANUAL (wait 1.4s)"));
            return;
          } else if (singleChar == 'r') {
            currentMode = MODE_RAW;
            rawModeActive = true;
            rawRxMode = true;
            autoMessage[0] = '\0';
            rawLastReceiveTime = millis();
            Serial.println(F("\n>>> RAW"));
            return;
          }
        }
        
        if (strlen(autoMessage) > 0 && txState == TX_IDLE) {
          transmitInit(autoMessage);
          autoMessage[0] = '\0';
        }
      } else if (cmd >= 32 && cmd <= 126) {
        int len = strlen(autoMessage);
        if (len < 63) {
          autoMessage[len] = cmd;
          autoMessage[len + 1] = '\0';
          Serial.print(cmd);
        }
      }
    }
    else if (cmd == 'a') {
      currentMode = MODE_AUTO;
      rawModeActive = false;
      rawRxMode = false;
      autoMessage[0] = '\0';
      manualBuffer[0] = '\0';
      Serial.println(F("\n>>> AUTO"));
    } else if (cmd == 'm') {
      currentMode = MODE_MANUAL;
      rawModeActive = false;
      rawRxMode = false;
      autoMessage[0] = '\0';
      manualBuffer[0] = '\0';
      Serial.println(F("\n>>> MANUAL"));
    } else if (cmd == 'r') {
      currentMode = MODE_RAW;
      rawModeActive = true;
      rawRxMode = true;
      autoMessage[0] = '\0';
      rawLastReceiveTime = millis();
      Serial.println(F("\n>>> RAW"));
    }
  }
  
  if (rawModeActive) {
    rawModeUpdate();
    receiveUpdate();
  } else {
    switch (currentMode) {
      case MODE_AUTO:
        transmitUpdate();
        receiveUpdate();
        break;
      case MODE_MANUAL:
        manualModeUpdate();
        transmitUpdate();
        receiveUpdate();
        break;
      default:
        break;
    }
  }
  
  if (rxState == RX_COMPLETE || rxState == RX_ERROR) {
    if (!rxResetTimerActive) {
      rxResetTimerActive = true;
      rxResetTimerTicks = 0;
    }
  }
  
  if (txState == TX_COMPLETE) {
    if (!txCompleteTimerActive) {
      txCompleteTimerActive = true;
      txCompleteTimerTicks = 0;
    }
  }
}