/*
  Buzzer frequency test for meteoClock.
  Passive piezo between D8 and GND.

  Uses a software square wave (no tone()/Timer2), so it also works if the
  board core or Timer2 is the problem.

  Upload, open Serial Monitor at 9600, listen and note the loudest frequency.
  Then re-upload firmware/meteoClock_v2.0 and select that frequency in
  Setup Menu -> Buzzer Freq (or tell me which one and I'll change the default).
*/

const uint8_t BUZZER_PIN = 8;
const uint8_t BUZZER_PIN2 = 11;
const bool BRIDGE = false;   // true: piezo between D8 and D11; false: D8 and GND
const uint16_t freqs[] = { 2000, 2400, 2700, 3000, 3500, 4000, 4500 };
const uint8_t FREQ_COUNT = sizeof(freqs) / sizeof(freqs[0]);

void playTone(uint8_t pin, uint16_t freq, uint16_t durationMs) {
  uint16_t halfPeriod = 500000UL / freq;   // microseconds
  uint32_t start = millis();
  while (millis() - start < durationMs) {
    if (BRIDGE) {
      digitalWrite(pin, HIGH);
      digitalWrite(BUZZER_PIN2, LOW);
      delayMicroseconds(halfPeriod);
      digitalWrite(pin, LOW);
      digitalWrite(BUZZER_PIN2, HIGH);
      delayMicroseconds(halfPeriod);
    } else {
      digitalWrite(pin, HIGH);
      delayMicroseconds(halfPeriod);
      digitalWrite(pin, LOW);
      delayMicroseconds(halfPeriod);
    }
  }
}

void setup() {
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);
  if (BRIDGE) {
    pinMode(BUZZER_PIN2, OUTPUT);
    digitalWrite(BUZZER_PIN2, LOW);
  }
  Serial.begin(9600);
  delay(500);
  Serial.println(F("Buzzer frequency test (software square wave)"));
  Serial.println(F("Listen and note the loudest:"));
}

void loop() {
  for (uint8_t i = 0; i < FREQ_COUNT; i++) {
    Serial.print(i);
    Serial.print(F(": "));
    Serial.print(freqs[i]);
    Serial.println(F(" Hz"));
    playTone(BUZZER_PIN, freqs[i], 800);
    delay(400);
  }
  Serial.println(F("--- repeat ---"));
  delay(1000);
}
