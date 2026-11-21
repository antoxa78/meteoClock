/*
  Buzzer diagnostic for meteoClock.

  Use this if the frequency test is silent.
  Buzzer between D8 and GND (or move it to D7 and change BUZZER_PIN below).

  1. Open Serial Monitor at 9600. If lines do not appear, the sketch is not
     running (upload/port problem), not a buzzer problem.
  2. DC test: D8 goes HIGH 1 s, LOW 1 s, three times.
     - Active buzzer: continuous sound on HIGH or LOW.
       HIGH -> BUZZER_ACTIVE 1, LOW -> BUZZER_ACTIVE 2.
     - Passive piezo: only a click at each transition, no continuous sound.
     - Magnetic buzzer: may buzz on DC; use a transistor, not direct drive.
  3. Square-wave sweep: software toggling, no tone()/Timer2.
     If this is silent but DC test clicks, the pin works but the buzzer is
     likely magnetic/needs a transistor, or is not connected to D8.
*/

const uint8_t BUZZER_PIN = 8;
const uint16_t freqs[] = { 2700, 3500, 4000, 4500 };

void playTone(uint8_t pin, uint16_t freq, uint16_t durationMs) {
  uint16_t halfPeriod = 500000UL / freq;
  uint32_t start = millis();
  while (millis() - start < durationMs) {
    digitalWrite(pin, HIGH);
    delayMicroseconds(halfPeriod);
    digitalWrite(pin, LOW);
    delayMicroseconds(halfPeriod);
  }
}

void setup() {
  Serial.begin(9600);
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);
  delay(500);

  Serial.println(F("Buzzer diagnostic"));
  Serial.println(F("Serial is working."));
  Serial.println(F("DC test: HIGH 1s / LOW 1s x3"));

  for (byte i = 0; i < 3; i++) {
    Serial.print(F("D"));
    Serial.print(BUZZER_PIN);
    Serial.println(F(" = HIGH"));
    digitalWrite(BUZZER_PIN, HIGH);
    delay(1000);
    Serial.print(F("D"));
    Serial.print(BUZZER_PIN);
    Serial.println(F(" = LOW"));
    digitalWrite(BUZZER_PIN, LOW);
    delay(1000);
  }

  Serial.println(F("DC test done."));
  Serial.println(F("Square-wave sweep (software, no Timer2)"));
}

void loop() {
  for (byte i = 0; i < sizeof(freqs) / sizeof(freqs[0]); i++) {
    Serial.print(freqs[i]);
    Serial.println(F(" Hz"));
    playTone(BUZZER_PIN, freqs[i], 700);
    delay(400);
  }
  Serial.println(F("--- repeat ---"));
  delay(1000);
}
