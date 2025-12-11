#define COMMON_ANODE false // выбор анода или катода у дисплеев

#define DATA_PIN 8   // PB0 - DS (Data Serial)
#define CLOCK_PIN 9  // PB1 - SHCP (Shift Clock)
#define LATCH_PIN 10 // PB2 - STCP (Storage Clock/Latch)

#define DATA_MASK  (1 << 0)  // PB0
#define CLOCK_MASK (1 << 1)  // PB1
#define LATCH_MASK (1 << 2)  // PB2

const uint8_t SEGMENT_MAP_CATHODE[10] = {
  0b00111111,  // 0
  0b00000110,  // 1
  0b01011011,  // 2
  0b01001111,  // 3
  0b01100110,  // 4
  0b01101101,  // 5
  0b01111101,  // 6
  0b00000111,  // 7
  0b01111111,  // 8
  0b01101111   // 9
};

const uint8_t SEGMENT_MAP_ANODE[10] = {
  0b11000000,  // 0
  0b11111001,  // 1
  0b10100100,  // 2
  0b10110000,  // 3
  0b10011001,  // 4
  0b10010010,  // 5
  0b10000010,  // 6
  0b11111000,  // 7
  0b10000000,  // 8
  0b10010000   // 9
};

const uint8_t* SEGMENT_MAP = COMMON_ANODE ? SEGMENT_MAP_ANODE : SEGMENT_MAP_CATHODE;

volatile uint8_t currentCounter = 0;      // Текущее значение счетчика (0-99)
volatile bool hasNewValue = false;        // Флаг нового значения от пользователя
volatile uint8_t newUserValue = 0;        // Новое значение от пользователя
volatile bool counterInitialized = false; // Флаг инициализации счетчика

inline void setPinHigh(uint8_t mask) {
  PORTB |= mask;
}

inline void setPinLow(uint8_t mask) {
  PORTB &= ~mask;
}

inline void writeDataBit(bool bit) {
  if (bit) {
    setPinHigh(DATA_MASK);
  } else {
    setPinLow(DATA_MASK);
  }
}

inline void clockPulse() {
  setPinHigh(CLOCK_MASK);

  asm volatile("nop\n\t");
  asm volatile("nop\n\t");
  setPinLow(CLOCK_MASK);
}

inline void latchPulse() {
  setPinLow(LATCH_MASK);
  asm volatile("nop\n\t");
  setPinHigh(LATCH_MASK);
  asm volatile("nop\n\t");
  asm volatile("nop\n\t");
  setPinLow(LATCH_MASK);
}

void shiftOut16(uint16_t data) {
  setPinLow(LATCH_MASK);
  
  for (int8_t i = 15; i >= 0; i--) {
    // Записать бит данных
    writeDataBit((data >> i) & 0x01);
    
    clockPulse();
  }
  
  latchPulse();
}

void updateDisplay(uint8_t value) {
  if (value > 99) value = 0;
  
  uint8_t tens = value / 10;
  uint8_t ones = value % 10;
  
  uint8_t tensSegments = SEGMENT_MAP[tens];
  uint8_t onesSegments = SEGMENT_MAP[ones];
  
  // При каскадном соединении первый переданный байт попадает во второй регистр
  // Поэтому сначала единицы (для второго регистра), затем десятки (для первого)
  uint16_t displayData = ((uint16_t)onesSegments << 8) | tensSegments;
  
  shiftOut16(displayData);
}

ISR(TIMER1_COMPA_vect) {
  static bool initialized = false;
  
  if (!counterInitialized) return;
  
  if (hasNewValue) {
    currentCounter = newUserValue;
    hasNewValue = false;
  } else if (initialized) {
    currentCounter++;
    if (currentCounter > 99) {
      currentCounter = 0;
    }
  }
  
  updateDisplay(currentCounter);
  
  initialized = true;
}

void setupTimer1() {
  cli();
  
  TCCR1A = 0;
  TCCR1B = 0;
  TCNT1 = 0;
  
  // Расчет для 1 секунды:
  // Частота процессора = 16 МГц
  // Прескалер = 256
  // Частота таймера = 16,000,000 / 256 = 62.500 Гц
  // Для 1 секунды: 62.500 тактов
  OCR1A = 62499;
  
  TCCR1B |= (1 << WGM12);
  
  TCCR1B |= (1 << CS12);
  
  TIMSK1 |= (1 << OCIE1A);
  
  sei();
}

void processSerialInput() {
  if (Serial.available() > 0) {
    String input = Serial.readStringUntil('\n');
    input.trim();
    
    if (input.length() > 0) {
      int value = input.toInt();
      
      if (value >= 0 && value <= 99) {
        cli();
        
        if (!counterInitialized) {
          currentCounter = value;
          counterInitialized = true;
          updateDisplay(currentCounter);
          Serial.print(F("Counter initialized: "));
          Serial.println(value);
        } else {
          newUserValue = value;
          hasNewValue = true;
          Serial.print(F("Next value set to: "));
          Serial.println(value);
        }
        
        sei();
      } else {
        Serial.println(F("Error: Value must be 0-99"));
      }
    }
  }
}

void setup() {
  Serial.begin(9600);
  Serial.println(F("================================="));
  Serial.println(F("Cascading Shift Register Counter"));
  Serial.println(F("================================="));
  Serial.print(F("Display type: "));
  Serial.println(COMMON_ANODE ? F("Common Anode") : F("Common Cathode"));
  Serial.println(F("Send a value (0-99) to start"));
  Serial.println(F("---------------------------------"));
  
  DDRB |= DATA_MASK | CLOCK_MASK | LATCH_MASK;
  
  PORTB &= ~(DATA_MASK | CLOCK_MASK | LATCH_MASK);
  
  shiftOut16(0x0000);
  
  setupTimer1();
}

void loop() {
  processSerialInput();
}