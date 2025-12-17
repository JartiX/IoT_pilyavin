#define F_CPU 16000000UL


#define NROWS 2
#define NCOLS 2
#define SCAN_INTERVAL_MS 10

const uint8_t row_masks[NROWS] = {
  (1 << 2),  // PD2 - ROW1
  (1 << 3)   // PD3 - ROW2
};

const uint8_t col_masks[NCOLS] = {
  (1 << 5),  // PD5 - COL1
  (1 << 6)   // PD6 - COL2
};

bool current_state[NROWS * NCOLS] = {false};
bool previous_state[NROWS * NCOLS] = {false};
unsigned long press_start_time[NROWS * NCOLS] = {0};

volatile uint8_t current_row = 0;
volatile bool scan_complete = false;

void setup() {
  Serial.begin(9600);
  
  DDRD |= (1 << 2) | (1 << 3);
  
  DDRD &= ~((1 << 5) | (1 << 6));
  PORTD |= (1 << 5) | (1 << 6);
  
  setup_timer();
}

void setup_timer() {
  cli();
  
  TCCR1A = 0;
  TCCR1B = 0;
  TCNT1 = 0;
  
  uint16_t compare_value = (F_CPU / (64UL * (1000UL / SCAN_INTERVAL_MS))) - 1;
  OCR1A = compare_value;
  
  TCCR1B |= (1 << WGM12);
  TCCR1B |= (1 << CS11) | (1 << CS10);
  TIMSK1 |= (1 << OCIE1A);
  
  sei();
}

ISR(TIMER1_COMPA_vect) {
  PORTD |= (1 << 2) | (1 << 3);
  
  PORTD &= ~row_masks[current_row];
  
  asm volatile("nop\n\t");
  asm volatile("nop\n\t");
  asm volatile("nop\n\t");
  asm volatile("nop\n\t");
  
  uint8_t pin_state = PIND;
  
  for (uint8_t icol = 0; icol < NCOLS; icol++) {
    uint8_t btn_index = current_row * NCOLS + icol;
    current_state[btn_index] = !(pin_state & col_masks[icol]);
  }
  
  current_row++;
  if (current_row >= NROWS) {
    current_row = 0;
    scan_complete = true;
  }
}

void loop() {
  if (scan_complete) {
    scan_complete = false;
    process_button_states();
  }
}

void process_button_states() {
  bool state_changed = false;
  unsigned long current_time = millis();
  
  for (uint8_t i = 0; i < NROWS * NCOLS; i++) {
    if (current_state[i] && !previous_state[i]) {
      press_start_time[i] = current_time;
      state_changed = true;
    }
    else if (!current_state[i] && previous_state[i]) {
      unsigned long duration = current_time - press_start_time[i];
      Serial.print("Длительность нажатия кнопки ");
      Serial.print(i + 1);
      Serial.print(" - ");
      Serial.print(duration);
      Serial.print(" мс, начало в ");
      Serial.print(press_start_time[i]);
      Serial.println(" мс");
      state_changed = true;
    }
  }
  
  if (state_changed) {
    report_pressed_buttons();
    for (uint8_t i = 0; i < NROWS * NCOLS; i++) {
      previous_state[i] = current_state[i];
    }
  }
}

void report_pressed_buttons() {
  bool first = true;
  bool any_pressed = false;
  
  Serial.print("Нажаты кнопки ");
  
  for (uint8_t i = 0; i < NROWS * NCOLS; i++) {
    if (current_state[i]) {
      any_pressed = true;
      if (!first) {
        Serial.print(", ");
      }
      Serial.print(i + 1);
      first = false;
    }
  }
  
  if (!any_pressed) {
    Serial.print("(нет)");
  }
  
  Serial.println();
}