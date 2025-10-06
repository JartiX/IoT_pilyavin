
const int base_period = 15;
const int number_of_leds = 5;

volatile unsigned int ledCounters[number_of_leds] = {0};

void setup() {
  cli();
  DDRB |= 0b00011111;

  TCCR1A = 0;
  TCCR1B = 0;

  TCCR1B |= (1 << WGM12);
  TCCR1B |= (1 << CS12);
  OCR1A = 624;
  TIMSK1 |= (1 << OCIE1A);
  sei();
}

ISR(TIMER1_COMPA_vect) {
  for (int i = 0; i < number_of_leds; i++) {
    ledCounters[i]++;
    unsigned int targetPeriod = (i + 1) * base_period;

    if (ledCounters[i] >= targetPeriod) {
      PORTB ^= (1 << i);
      ledCounters[i] = 0;
    }
  }
}

void loop() {
}