#include <math.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>

void playChimeSound(byte target);
#if (RESET_CLOCK == 0)
uint8_t randomChime();
#endif
void playRandomChime();
void drawEditValueLine();

// Частоты для пассивного пьезо и готовые значения OCR2A для Timer2:
// f = F_CPU / (2 * 32 * (OCR2A + 1)), F_CPU = 16 МГц.
// 0..6 — штатные; 7..70 — дополнительные частоты для вариантов сигнала.
static const uint8_t  buzzerOcr[BUZZER_NOTE_COUNT] PROGMEM = {
  124, 103, 92, 82, 70, 62, 55,   // 2000..4500 Гц
  155, 166, 207, 249,             // 1600, 1500, 1200, 1000 Гц
  99, 95, 77,                     // 2500, 2600, 3200 Гц
  45, 41, 191,                    // 5400, 6000, 1300 Гц
  67, 49, 35, 237,                // 3700, 5000, 7000, 1050 Гц
  221, 212, 195, 181,             // 1125, 1175, 1275, 1375 Гц
  158, 132,                       // 1575, 1875 Гц
  121, 110,                       // 2050, 2250 Гц
  90, 76, 32,                     // 2750, 3250, 7500 Гц
  30, 28, 27, 24,                 // 8000, 8500, 9000, 10000 Гц
  138, 113, 118, 88, 65, 59,      // 1800, 2200, 2100, 2800, 3800, 4200 Гц
  51, 178, 226, 108, 85, 80,      // 4800, 1400, 1100, 2300, 2900, 3100 Гц
  75, 73, 68, 63, 60, 57,         // 3300, 3400, 3600, 3900, 4100, 4300 Гц
  56, 53, 52, 50, 47, 44,         // 4400, 4600, 4700, 4900, 5200, 5600 Гц
  42, 39, 37,                     // 5800, 6200, 6500 Гц
  131, 146, 199, 216,             // 1900, 1700, 1250, 1150 Гц
  168, 140, 105, 81               // 1475, 1775, 2350, 3050 Гц
};

#if (BUZZER_ACTIVE == 0)
ISR(TIMER2_COMPA_vect) {
#if (BUZZER_BRIDGE == 1)
  static bool phase;
  phase = !phase;
  if (phase) {
    BUZZER_PORT |= _BV(BUZZER_BIT);
    BUZZER_BRIDGE_PORT &= ~_BV(BUZZER_BRIDGE_BIT);
  } else {
    BUZZER_PORT &= ~_BV(BUZZER_BIT);
    BUZZER_BRIDGE_PORT |= _BV(BUZZER_BRIDGE_BIT);
  }
#else
  BUZZER_PORT ^= _BV(BUZZER_BIT);
#endif
}
#endif

void buzzerOn() {
#if (BUZZER_ACTIVE == 1)
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, HIGH);
#elif (BUZZER_ACTIVE == 2)
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);
#else
  byte idx = buzzerPlayIdx;
  if (idx >= BUZZER_NOTE_COUNT) idx = BUZZER_FREQ_IDX;
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);
#if (BUZZER_BRIDGE == 1)
  pinMode(BUZZER_BRIDGE_PIN, OUTPUT);
  digitalWrite(BUZZER_BRIDGE_PIN, HIGH);   // старт в противофазе
#endif
  TCCR2A = _BV(WGM21);                 // CTC
  TCCR2B = _BV(CS21) | _BV(CS20);      // /32
  OCR2A = pgm_read_byte(&buzzerOcr[idx]);
  TCNT2 = 0;
  TIMSK2 |= _BV(OCIE2A);
#endif
}

void buzzerOff() {
#if (BUZZER_ACTIVE == 1)
  // active-high: низкий уровень = выключено
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);
#elif (BUZZER_ACTIVE == 2)
  // active-low: высокий уровень = выключено
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, HIGH);
#else
  // Пассивный пьезо: останавливаем Timer2 и отпускаем пин в high-Z.
  // Это важно для активных модулей, случайно оставленных в режиме 0:
  // низкий уровень у active-low модуля = постоянный звук.
  TIMSK2 &= ~_BV(OCIE2A);
  TCCR2B = 0;
  digitalWrite(BUZZER_PIN, LOW);
  pinMode(BUZZER_PIN, INPUT);
#if (BUZZER_BRIDGE == 1)
  digitalWrite(BUZZER_BRIDGE_PIN, LOW);
  pinMode(BUZZER_BRIDGE_PIN, INPUT);
#endif
#endif
}

void applyTempOffset() {
  bme.setTemperatureCompensation(tempOffset / 10.0);
}

void loadSettings() {
  if (EEPROM.read(EE_MAGIC) != EE_MAGIC_VALUE) return;

  // Поля v1.9 читаем всегда — они совместимы.
  byte v = EEPROM.read(EE_MODE0);
  if (v <= 9) mode0scr = v;
  if (mode0scr == 5 || mode0scr == 6) mode0scr = 7;  // старые экраны dew/heat
#if (DISPLAY_TYPE == 1)
  if (mode0scr == 8) mode0scr = 9;       // 8-й экран есть только на 1602
#endif
#if (CO2_SENSOR == 0)
  if (mode0scr == 1) mode0scr = 2;
#endif

  bigDig = EEPROM.read(EE_BIGDIG) != 0;

  v = EEPROM.read(EE_LED_BRIGHT);
  if (v <= 11) LED_BRIGHT = v;
  v = EEPROM.read(EE_LCD_BRIGHT);
  if (v <= 11) LCD_BRIGHT = v;
  v = EEPROM.read(EE_LED_TYPE);
  if (v <= 3) LEDType = v;
  else if (v == 4) LEDType = 3;          // v1.9: 4 = давление, теперь 3

  // Новые поля читаем только если они были сохранены v2.0+.
  // Иначе в EEPROM v1.9 там лежат 0xFF, которые выглядели бы как -1/-1%.
  byte ver = EEPROM.read(EE_VERSION);
  if (ver < 2 || ver > EE_VERSION_VALUE) return;

  int8_t t = (int8_t)EEPROM.read(EE_HUM_OFF);
  if (t >= -20 && t <= 20) humOffset = t;

  uint16_t alt = EEPROM.read(EE_ALTITUDE) | ((uint16_t)EEPROM.read(EE_ALTITUDE + 1) << 8);
  if (alt <= 3000) altitude = alt;

  v = EEPROM.read(EE_ALARM_HOUR);
  if (v <= 23) alarmHour = v;
  v = EEPROM.read(EE_ALARM_MIN);
  if (v <= 59) alarmMin = v;

  v = EEPROM.read(EE_FLAGS);
  alarmEnabled = (v & FLAG_ALARM) ? 1 : 0;
  chimeEnabled = (v & FLAG_CHIME) ? 1 : 0;

  v = EEPROM.read(EE_CHIME_SOUND);
  if (v <= CHIME_RANDOM) chimeSound = v;
}

void saveSettings() {
  EEPROM.update(EE_MAGIC, EE_MAGIC_VALUE);
  EEPROM.update(EE_MODE0, mode0scr);
  EEPROM.update(EE_BIGDIG, bigDig ? 1 : 0);
  EEPROM.update(EE_LED_BRIGHT, LED_BRIGHT);
  EEPROM.update(EE_LCD_BRIGHT, LCD_BRIGHT);
  EEPROM.update(EE_LED_TYPE, LEDType);
  EEPROM.update(EE_TEMP_OFF, (uint8_t)tempOffset);
  EEPROM.update(EE_HUM_OFF, (uint8_t)humOffset);
  EEPROM.update(EE_ALTITUDE, (uint8_t)(altitude & 0xFF));
  EEPROM.update(EE_ALTITUDE + 1, (uint8_t)((altitude >> 8) & 0xFF));
  EEPROM.update(EE_ALARM_HOUR, (uint8_t)alarmHour);
  EEPROM.update(EE_ALARM_MIN, (uint8_t)alarmMin);
  byte flags = (alarmEnabled ? FLAG_ALARM : 0) | (chimeEnabled ? FLAG_CHIME : 0);
  EEPROM.update(EE_FLAGS, flags);
  EEPROM.update(EE_CHIME_SOUND, (uint8_t)chimeSound);
  EEPROM.update(EE_VERSION, EE_VERSION_VALUE);
}

void printTwoDigits(int v) {
  if (v < 10) lcd.print('0');
  lcd.print(v);
}

void printDashes(byte width) {
  while (width--) lcd.print('-');
}

void clearLine(byte row, byte width) {
  lcd.setCursor(0, row);
  while (width--) lcd.print(' ');
}

void printStatTemp(float v, byte width, boolean ok) {
  if (ok) printTempField(v, width);
  else printDashes(width);
}

void printStatInt(int v, byte width, boolean ok) {
  if (ok) printValRight(v, width);
  else printDashes(width);
}

void updateQnh() {
  if (!bmeOk) return;
  // (1 - 2.25577e-5*h)^-5.25588, ряд Тейлора до h^3.
  // Ошибка < 0.1% на высотах до 3000 м, без pow().
  float x = altitude * 2.25577e-5;
  float inv = 1.0 + x * (5.25588 + x * (16.4395 + x * 39.76));
  dispQnh = (int)(stationHpa * inv * 0.750062 + 0.5);
}

void statsReset() {
  statTempOk = statHumOk = statPresOk = statCO2Ok = false;
  if (bmeOk) {
    statTempMin = statTempMax = dispTemp;
    statHumMin = statHumMax = dispHum;
    statPresMin = statPresMax = dispPres;
    statTempOk = statHumOk = statPresOk = true;
  }
#if (CO2_SENSOR == 1)
  if (!co2Gate && dispCO2 >= 0) {
    statCO2Min = statCO2Max = dispCO2;
    statCO2Ok = true;
  }
#endif
  statsDay = timeValid ? now.day() : 0;
}

void statsUpdate() {
  if (bmeOk) {
    if (!statTempOk) {
      statTempMin = statTempMax = dispTemp;
      statTempOk = true;
    } else {
      if (dispTemp < statTempMin) statTempMin = dispTemp;
      if (dispTemp > statTempMax) statTempMax = dispTemp;
    }
    if (!statHumOk) {
      statHumMin = statHumMax = dispHum;
      statHumOk = true;
    } else {
      if (dispHum < statHumMin) statHumMin = dispHum;
      if (dispHum > statHumMax) statHumMax = dispHum;
    }
    if (!statPresOk) {
      statPresMin = statPresMax = dispPres;
      statPresOk = true;
    } else {
      if (dispPres < statPresMin) statPresMin = dispPres;
      if (dispPres > statPresMax) statPresMax = dispPres;
    }
  }
#if (CO2_SENSOR == 1)
  if (!co2Gate && dispCO2 >= 0) {
    if (!statCO2Ok) {
      statCO2Min = statCO2Max = dispCO2;
      statCO2Ok = true;
    } else {
      if (dispCO2 < statCO2Min) statCO2Min = dispCO2;
      if (dispCO2 > statCO2Max) statCO2Max = dispCO2;
    }
  }
#endif
}

void statsTick() {
  if (timeValid && statsDay != now.day()) statsReset();
  statsUpdate();
}

void drawStatsAll() {
  lcd.setCursor(0, 0);
  lcd.print(F("T "));
  printStatTemp(statTempMin, 5, statTempOk); lcd.print('/');
  printStatTemp(statTempMax, 5, statTempOk); lcd.print(F(" C"));
  lcd.setCursor(0, 1);
  lcd.print(F("H "));
  printStatInt(statHumMin, 3, statHumOk); lcd.print('/');
  printStatInt(statHumMax, 3, statHumOk); lcd.print(F(" %"));
  lcd.setCursor(0, 2);
  lcd.print(F("P "));
  printStatInt(statPresMin, 3, statPresOk); lcd.print('/');
  printStatInt(statPresMax, 3, statPresOk); lcd.print(F(" mm"));
  lcd.setCursor(0, 3);
  lcd.print(F("C "));
  printStatInt(statCO2Min, 4, statCO2Ok); lcd.print('/');
  printStatInt(statCO2Max, 4, statCO2Ok); lcd.print(F(" ppm"));
}

void drawStatsTH() {
  lcd.setCursor(0, 0);
  lcd.print(F("T "));
  printStatTemp(statTempMin, 5, statTempOk); lcd.print('/');
  printStatTemp(statTempMax, 5, statTempOk);
  lcd.setCursor(0, 1);
  lcd.print(F("H "));
  printStatInt(statHumMin, 3, statHumOk); lcd.print('/');
  printStatInt(statHumMax, 3, statHumOk); lcd.print(F(" %"));
}

void drawStatsPC() {
  lcd.setCursor(0, 0);
  lcd.print(F("P "));
  printStatInt(statPresMin, 3, statPresOk); lcd.print('/');
  printStatInt(statPresMax, 3, statPresOk); lcd.print(F(" mmHg"));
  lcd.setCursor(0, 1);
  lcd.print(F("C "));
  printStatInt(statCO2Min, 4, statCO2Ok); lcd.print('/');
  printStatInt(statCO2Max, 4, statCO2Ok); lcd.print(F(" ppm"));
}

void drawQnh() {
  drawPresQnh(dispQnh, 0, 0);
#if (DISPLAY_TYPE == 1)
  if (!bigDig) {
    lcd.setCursor(0, 2);
    lcd.print(F("P "));
    printValRight(dispPres, 3);
    lcd.print(F(" mmHg"));
    lcd.setCursor(0, 3);
    lcd.print(F("Alt "));
    printValRight(altitude, 4);
    lcd.print(F(" m"));
  }
#endif
}

void beginEdit(byte item) {
  mode = 240;
  editDirection = 1;
  editRepeatCount = 0;
  switch (item) {
    case 9:  editTarget = &altitude;    editMin = 0;    editMax = 3000; editStep = 1;  editBigStep = 10;  break;
    case 1:  alarmEditH = alarmHour; alarmEditM = alarmMin; alarmField = 0;
             editTarget = NULL; break;
    case 2:  chimeSel = chimeEnabled ? (chimeSound + 1) : 0;
             editTarget = &chimeSel; editMin = 0; editMax = CHIME_MODE_COUNT + 1; editStep = 1; editBigStep = 1; break;
  }
}

void alarmEditStep() {
  if (alarmField == 0) {
    alarmEditH = (alarmEditH + 1) % 24;
  } else if (alarmField == 1) {
    alarmEditM = (alarmEditM + 1) % 60;
  } else if (alarmField == 2) {
    alarmEnabled = alarmEnabled ? 0 : 1;
    alarmDoneDay = 0;
    if (!alarmEnabled) stopAlarm();
  }
  alarmHour = alarmEditH;
  alarmMin = alarmEditM;
}

void finishEdit() {
  if (podMode == 9) {
    updateQnh();
  } else if (podMode == 1) {
    alarmHour = alarmEditH;
    alarmMin = alarmEditM;
    alarmDoneDay = 0;
  }
}

void editApply(int16_t delta) {
  if (!editTarget) return;
  int16_t v = *editTarget + delta;
  if (v < editMin) v = editMax;     // wrap: после конца — начало
  if (v > editMax) v = editMin;
  *editTarget = v;
  if (podMode == 2) {                          // Chime: OFF, вариант или Random
    chimeEnabled = (chimeSel > 0) ? 1 : 0;
    if (chimeEnabled) {
      chimeSound = chimeSel - 1;
      drawEditValueLine();        // сначала показать номер, затем играть превью
      if (!alarmActive) {
        if (chimeSound == CHIME_RANDOM) playRandomChime();       // превью случайного варианта
        else playChimeSound((byte)chimeSound);                  // превью выбранного варианта
      }
    } else {
      chimeActive = false;
      drawEditValueLine();
      if (!alarmActive) buzzerOff();
    }
    return;
  }
  if (podMode == 9) updateQnh();
}

#if (DISPLAY_TYPE == 1)
static const char menuNames[] PROGMEM =
  "Alarm\0Chime\0Save\0Exit\0LED Br\0LCD Br\0LED Mode\0CO2 Calibration\0Altitude";
#else
static const char menuNames[] PROGMEM =
  "Alarm\0Chime\0Save\0Exit\0LED Br\0LCD Br\0LED Mode\0CO2 Calib\0Altitude";
#endif

void printMenuItemName() {
  const char *p = menuNames;
  for (byte i = 1; i < podMode; i++) {
    while (pgm_read_byte(p++)) {}
  }
  byte c;
  while ((c = pgm_read_byte(p++))) lcd.write(c);
}

void printSensorName() {
  switch (LEDType) {
    case 0: lcd.print(F("CO2")); break;
    case 1: lcd.print(F("Humidity")); break;
    case 2: lcd.print(F("Temperature")); break;
    case 3: lcd.print(F("Pressure")); break;
  }
}

void printTime(int m) {
  printTwoDigits(m / 60);
  lcd.print(':');
  printTwoDigits(m % 60);
}

void printEditValue() {
  switch (podMode) {
    case 9:  lcd.print(altitude); lcd.print(F(" m")); break;
    case 2:
      if (!chimeEnabled) lcd.print(F("OFF"));
      else if (chimeSound >= CHIME_MODE_COUNT) lcd.print(F("Random"));
      else lcd.print(chimeSound);
      break;
  }
}

void drawEditValueLine() {
#if (DISPLAY_TYPE == 1)
  clearLine(1, 20);
  lcd.setCursor(2, 1);
#else
  clearLine(1, 16);
  lcd.setCursor(0, 1);
  if (podMode == 9) {
    printMenuItemName();
    lcd.print(' ');
  }
#endif
  printEditValue();
}

void drawAlarmFields() {
  lcd.setCursor(0, 0);
  if (alarmField == 0) lcd.print('[');
  printTwoDigits(alarmEditH);
  if (alarmField == 0) lcd.print(']');
  lcd.print(':');
  if (alarmField == 1) lcd.print('[');
  printTwoDigits(alarmEditM);
  if (alarmField == 1) lcd.print(']');
  lcd.print(' ');
  if (alarmField == 2) lcd.print('[');
  lcd.print(alarmEnabled ? F("ON") : F("OFF"));
  if (alarmField == 2) lcd.print(']');
  lcd.print(' ');
  if (alarmField == 3) lcd.print('[');
  lcd.print(F("SET"));
  if (alarmField == 3) lcd.print(']');
}

void drawAlarmEdit() {
  drawAlarmFields();
#if (DISPLAY_TYPE == 1)
  lcd.setCursor(0, 1);
  lcd.print(F("Alarm EDIT"));
  lcd.setCursor(0, 2);
  lcd.print(F("1x change  hold next"));
  lcd.setCursor(0, 3);
  lcd.print(F("SET exits to menu"));
#else
  lcd.setCursor(0, 1);
  lcd.print(F("EDIT 1x HOLD>"));
#endif
}

void drawEdit() {
  if (podMode == 1) { drawAlarmEdit(); return; }
  lcd.setCursor(0, 0);
#if (DISPLAY_TYPE == 1)
  printMenuItemName();
  lcd.setCursor(16, 0);
  lcd.print(F("EDIT"));
  drawEditValueLine();
  if (podMode != 2) {                         // Chime: без строки текущих показаний
    clearLine(2, 20);
    lcd.setCursor(0, 2);
    if (podMode == 9) drawCurrentPQ();
  }
  lcd.setCursor(0, 3);
  if (podMode == 2) lcd.print(F("1x+ 2x- hold=OK"));
  else lcd.print(F("1x+ 2x- 3x back"));
#else
  if (podMode == 9) {
    lcd.print('P');
    printValRight(dispPres, 3);
    lcd.print(F(" Q"));
    printValRight(dispQnh, 4);
  } else {
    printMenuItemName();
  }
  drawEditValueLine();
#endif
}

void printBrightness(byte b) {
  if (b == 11) lcd.print(F("Auto"));
  else { lcd.print(b * 10); lcd.print('%'); }
}

void printMenuItemValue() {
  switch (podMode) {
    case 1:
      printTime(alarmHour * 60 + alarmMin);
      lcd.print(' ');
      lcd.print(alarmEnabled ? F("ON") : F("OFF"));
      break;
    case 2: printEditValue(); break;
    case 3: lcd.print(F("save EEPROM")); break;
    case 4: lcd.print(F("no save")); break;
    case 5: printBrightness(LED_BRIGHT); break;
    case 6: printBrightness(LCD_BRIGHT); break;
    case 7: printSensorName(); break;
    case 8: lcd.print(F("400 ppm")); break;
    case 9: printEditValue(); break;
  }
}

void drawMenu() {
  lcd.setCursor(0, 0);
  if (podMode == 1) {             // Alarm
    lcd.write(6);
    lcd.print(' ');
  } else if (podMode == 2) {      // Chime
    lcd.write(7);
    lcd.print(' ');
  } else {
    lcd.print(F("> "));
  }
  printMenuItemName();
#if (DISPLAY_TYPE == 1)
  lcd.setCursor(17, 0);
#else
  lcd.setCursor(13, 0);
#endif
  lcd.print(podMode);
  lcd.print(F("/9"));
  lcd.setCursor(2, 1);
  printMenuItemValue();
#if (DISPLAY_TYPE == 1)
  lcd.setCursor(0, 2);
  lcd.print(F("1x next  2x prev"));
  lcd.setCursor(0, 3);
  lcd.print(F("hold = enter"));
#endif
}

void returnToMenu() {
  switch (mode) {
    case 254: podMode = 5; break;   // LED Br
    case 253: podMode = 6; break;   // LCD Br
    case 252: podMode = 7; break;   // LED Mode
  }
  mode = 255;
}

void stopAlarm() {
  alarmActive = false;
  chimeActive = false;
  touchBeepActive = false;
  buzzerOff();
}

// ---- варианты почасового сигнала ----
// Таблица используется и для превью в меню, и для сигнала в начале часа.
#define CT_GAP 0xFF
#define CT_END 0xFE

#if (RESET_CLOCK == 0)
struct ChimeStep {
  uint8_t note;    // индекс частоты или CT_GAP / CT_END
  uint8_t dur10;   // длительность в 10 мс
};

// Индексы частот: 0=2000, 1=2400, 2=2700, 3=3000, 4=3500, 5=4000, 6=4500,
// 7=1600, 8=1500, 9=1200, 10=1000, 11=2500, 12=2600, 13=3200, 14=5400,
// 15=6000, 16=1300, 17=3700, 18=5000, 19=7000, 20=1050, 21=1125, 22=1175,
// 23=1275, 24=1375, 25=1575, 26=1875, 27=2050, 28=2250, 29=2750, 30=3250,
// 31=7500, 32=8000, 33=8500, 34=9000, 35=10000, 36=1800, 37=2200, 38=2100,
// 39=2800, 40=3800, 41=4200, 42=4800, 43=1400, 44=1100, 45=2300, 46=2900,
// 47=3100, 48=3300, 49=3400, 50=3600, 51=3900, 52=4100, 53=4300, 54=4400,
// 55=4600, 56=4700, 57=4900, 58=5200, 59=5600, 60=5800, 61=6200, 62=6500,
// 63=1900, 64=1700, 65=1250, 66=1150, 67=1475, 68=1775, 69=2350, 70=3050 Гц.
static const ChimeStep chimePatterns[] PROGMEM = {
  {33, 2}, {35, 2}, {62, 2}, {15, 5},  {CT_GAP, 3}, {54, 4}, {57, 3}, {18, 8}, {CT_GAP, 4}, {47, 4}, {0, 12}, {CT_END, 0}, // 0 high ping
  {1, 6},  {63, 3}, {36, 3}, {64, 3}, {CT_GAP, 2}, {7, 9}, {CT_GAP, 4}, {0, 10}, {CT_GAP, 4}, {10, 20}, {CT_END, 0}, // 1 low doorbell
  {3, 16}, {3, 16}, {49, 32}, {3, 32}, {5, 32}, {40, 64}, {CT_GAP, 4}, {3, 16}, {3, 16}, {49, 32}, {3, 32}, {6, 32}, {5, 64}, {CT_GAP, 4}, {3, 16}, {3, 16}, {15, 32}, {18, 32}, {5, 32}, {40, 32}, {49, 64}, {CT_GAP, 4}, {14, 16}, {14, 16}, {18, 32}, {5, 32}, {6, 32}, {5, 64}, {CT_END, 0}, // 2 Happy Birthday
  {48, 36}, {48, 36}, {48, 72}, {48, 36}, {48, 36}, {48, 72}, {48, 36}, {51, 36}, {58, 36}, {60, 36}, {62, 144}, {CT_END, 0}, // 3 Jingle Bells
  {38, 50}, {16, 50}, {36, 25}, {CT_GAP, 5}, {37, 40}, {8, 30}, {10, 60}, {63, 25}, {9, 20}, {38, 40}, {1, 20}, {64, 20}, {36, 30}, {16, 30}, {10, 55}, {CT_END, 0}, // 4 old modem sound
  {10, 5}, {20, 6}, {21, 7},  {66, 6},  {22, 8},  {23, 9},  {43, 6}, {9, 22}, {CT_END, 0}, // 5 low gong
  {8, 3},  {25, 3}, {26, 3},  {27, 3},  {28, 3},  {69, 3},  {29, 10}, {46, 6}, {30, 10}, {CT_END, 0}, // 6 rising run
  {19, 3}, {15, 3}, {14, 3},  {18, 3},  {6, 3},   {4, 3},   {50, 3},  {48, 3}, {3, 16}, {CT_END, 0}, // 7 falling run
  {31, 2}, {32, 2}, {33, 2},  {34, 2},  {35, 2},  {59, 2},  {60, 2}, {61, 2}, {CT_GAP, 3}, {31, 3}, {35, 12}, {15, 6}, {CT_END, 0}, // 8 sparkle
  {10, 6}, {0, 3},  {15, 4},  {70, 4},  {10, 6},  {49, 4},  {34, 4},  {34, 14}, {CT_END, 0}, // 9 wide leap
  {32, 2}, {1, 6},  {CT_GAP, 3}, {68, 3}, {7, 8}, {CT_GAP, 3}, {37, 3}, {0, 8}, {CT_GAP, 3}, {1, 18}, {CT_END, 0}, // 10 ding-dong-ding
  {32, 2}, {55, 2}, {5, 2},  {CT_GAP, 2}, {5, 2}, {CT_GAP, 7}, {4, 2}, {CT_GAP, 2}, {4, 2}, {CT_GAP, 7}, {3, 14}, {CT_GAP, 3}, {9, 8}, {CT_END, 0}, // 11 double strike
  {18, 34}, {18, 34}, {14, 34}, {15, 52}, {15, 34}, {14, 34}, {18, 34}, {6, 52}, {5, 34}, {5, 34}, {6, 34}, {39, 17}, {18, 73}, {CT_END, 0}, // 12 Ode to Joy
  {62, 20}, {60, 20}, {17, 40}, {52, 40}, {CT_GAP, 2}, {59, 20}, {57, 20}, {46, 40}, {48, 40}, {CT_GAP, 2}, {57, 20}, {54, 20}, {39, 40}, {48, 40}, {CT_GAP, 2}, {54, 80}, {CT_END, 0}, // 13 Nokia tune
  {3, 7},  {CT_GAP, 2}, {5, 7}, {CT_GAP, 2}, {18, 7}, {CT_GAP, 2}, {15, 24}, {CT_END, 0}, // 14 startup (hidden)
};

byte chimeModeStart(byte target) {
  if (target > CHIME_MODE_COUNT) target = 0;
  byte pos = 0;
  byte mode = 0;
  while (mode < target) {
    if (pgm_read_byte(&chimePatterns[pos].note) == CT_END) mode++;
    pos++;
  }
  return pos;
}

void playChimeSound(byte target) {
  stopAlarm();
  byte pos = chimeModeStart(target);
  for (;;) {
    uint8_t note = pgm_read_byte(&chimePatterns[pos].note);
    uint8_t dur10 = pgm_read_byte(&chimePatterns[pos].dur10);
    pos++;
    if (note == CT_END) break;
    if (note == CT_GAP) buzzerOff();
    else {
      buzzerPlayIdx = note;
      buzzerOn();
    }
    delay((uint16_t)dur10 * 10);
  }
  buzzerOff();
}

void playRandomChime() {
  playChimeSound(randomChime());
}

uint8_t randomChime() {
  uint8_t r;
  do {
    rngState ^= rngState << 7;
    rngState ^= rngState >> 9;
    rngState ^= rngState << 8;
    r = (uint8_t)rngState;
  } while (r >= CHIME_MODE_COUNT);
  return r;
}
#else
void playChimeSound(byte target) { (void)target; }
void playRandomChime() {}
#endif

void touchBeep() {
  if (!TOUCH_BEEP || alarmActive || chimeActive) return;
  touchBeepActive = true;
  touchBeepStart = millis();
  buzzerPlayIdx = BUZZER_FREQ_IDX;
  buzzerOn();
}

void checkAlarm() {
  if (!timeValid || !alarmEnabled || alarmActive) return;
  if (hrs != alarmHour || mins != alarmMin) return;
  if (alarmDoneDay == now.day()) return;
  alarmDoneDay = now.day();
  alarmActive = true;
  alarmPhase = 0;
  alarmStart = millis();
  buzzerPlayIdx = BUZZER_FREQ_IDX;
  buzzerOn();
}

#if (RESET_CLOCK == 0)
void checkChime() {
  if (!timeValid || !chimeEnabled || alarmActive || chimeActive) return;
  // Не требуем secs == 0: сверка с RTC раз в минуту может прийти уже на 1-2-й
  // секунде, и тогда почасовой сигнал пропускался. Достаточно первой минуты
  // часа, а lastChimeHour не даст сыграть его повторно.
  if (mins != 0) return;
  if (lastChimeHour == hrs) return;
  lastChimeHour = hrs;
  byte snd = (byte)chimeSound;
  if (snd == CHIME_RANDOM) snd = randomChime();
  else if (snd >= CHIME_MODE_COUNT) snd = 0;
  chimePos = chimeModeStart(snd);
  chimeActive = true;
  chimeStep = 0;
  chimeStepMs = 0;
  chimeStart = millis();
}
#else
void checkChime() {}
#endif

void tickBuzzer() {
  unsigned long ms = millis();
  if (alarmActive) {
    if (ms - alarmStart >= ALARM_MAX_MS) {
      stopAlarm();
      return;
    }
    byte phase = ((ms - alarmStart) / ALARM_STEP_MS) & 3;
    if (phase != alarmPhase) {
      alarmPhase = phase;
      if (phase == 0) {
        buzzerPlayIdx = BUZZER_FREQ_IDX;
        buzzerOn();
      } else if (phase == 2) {
        byte f2 = BUZZER_FREQ_IDX + 3;
        if (f2 > 6) f2 -= 7;
        buzzerPlayIdx = f2;                        // вторая частота
        buzzerOn();
      } else {
        buzzerOff();
      }
    }
  }
#if (RESET_CLOCK == 0)
  else if (chimeActive) {
    if (chimeStep == 0) {
      uint8_t note = pgm_read_byte(&chimePatterns[chimePos].note);
      uint8_t dur10 = pgm_read_byte(&chimePatterns[chimePos].dur10);
      chimePos++;
      if (note == CT_END) {
        buzzerOff();
        chimeActive = false;
        return;
      }
      if (note == CT_GAP) {
        buzzerOff();
      } else {
        buzzerPlayIdx = note;
        buzzerOn();
      }
      chimeStepMs = (uint16_t)dur10 * 10;
      chimeStart = ms;
      chimeStep = 1;
    } else if (ms - chimeStart >= chimeStepMs) {
      chimeStep = 0;
    }
  }
#endif
  else if (touchBeepActive) {
    if (ms - touchBeepStart >= TOUCH_BEEP_MS) {
      buzzerOff();
      touchBeepActive = false;
    }
  }
}

void checkBrightness() {
#if (BRIGHT_CONTROL == 0)                         // управление яркостью отключено - всегда максимум
  analogWrite(BACKLIGHT, LCD_BRIGHT_MAX);
#else
  int photo = analogRead(PHOTO);                  // одно чтение фоторезистора на оба канала (с)НР
  if (LCD_BRIGHT == 11) {                         // если установлен автоматический режим для экрана (с)НР
    if (photo < BRIGHT_THRESHOLD) {               // если темно
      analogWrite(BACKLIGHT, LCD_BRIGHT_MIN);
    } else {                                      // если светло
      analogWrite(BACKLIGHT, LCD_BRIGHT_MAX);
    }
  } else {
    analogWrite(BACKLIGHT, map(LCD_BRIGHT, 0, 10, 0, LCD_BRIGHT_MAX));
  }

  // Immediate on/off fallback for I2C LCD backpacks: 0% = backlight off.
  // Real PWM dimming still requires D10 wired to the backlight transistor.
  static byte lastLcdOn = 255;
  byte lcdOn = (LCD_BRIGHT != 0);
  if (lcdOn != lastLcdOn) {
    lastLcdOn = lcdOn;
    if (lcdOn) lcd.backlight();
    else lcd.noBacklight();
  }

  if (LED_BRIGHT == 11) {                         // если установлен автоматический режим для индикатора (с)НР
    // LED_ON задаётся в "катодной" логике, инверсия для общего анода выполняется в setLED() (с)НР
    if (photo < BRIGHT_THRESHOLD) {               // если темно
      LED_ON = LED_BRIGHT_MIN;
    } else {                                      // если светло
      LED_ON = LED_BRIGHT_MAX;
    }
  }
#endif
}

void showMessage(const __FlashStringHelper *msg, byte len) {
  lcd.clear();
#if (DISPLAY_TYPE == 1)
  lcd.setCursor((len < 20) ? (20 - len) / 2 : 0, 1);
#else
  lcd.setCursor((len < 16) ? (16 - len) / 2 : 0, 0);
#endif
  lcd.print(msg);
}

#if (CO2_SENSOR == 1)
void calibrateCo2() {                         // zero calibration CO2 with result message
  readSensors();                              // перед калибровкой берём свежее показание
  if (dispCO2 >= 350 && dispCO2 <= 500) {
    boolean ok = mhz19.calibrateToZeroPoint();
#if (DISPLAY_TYPE == 1)
    showMessage(ok ? F("Calibration Success") : F("Calibration Failed"),
                ok ? 19 : 18);
#else
    showMessage(ok ? F("Calib OK") : F("Calib ERR"), ok ? 8 : 9);
#endif
  } else {
    showMessage(F("Need 400 PPM"), 12);
  }
  delay(1500);
}
#endif

#if (DISPLAY_TYPE == 1)
void drawCurrentTHP() {
  lcd.print('T');
  printTemp(dispTemp);
  lcd.write(223);
  lcd.print(F(" H"));
  lcd.print((int)dispHum);
  lcd.print('%');
  lcd.print(F(" P"));
  lcd.print(dispPres);
}

void drawCurrentPQ() {
  lcd.print('P');
  lcd.print(dispPres);
  lcd.print(F(" Q"));
  lcd.print(dispQnh);
}
#else
void drawCurrentCTH() {
  lcd.print('C');
  if (dispCO2 >= 0) printValRight(dispCO2, 4); else lcd.print(F(" ---"));
  lcd.print(F(" T"));
  printValRight((int)dispTemp, 3);
  lcd.print(F(" H"));
  printValRight(dispHum, 3);
}
#endif

void drawCalibrationConfirm() {
#if (DISPLAY_TYPE == 1)
  lcd.setCursor(2, 0);
  lcd.print(F("CO2 Calibration"));
  clearLine(1, 20);
  lcd.setCursor(0, 1);
  lcd.print(F("Now: "));
  if (dispCO2 >= 0) {
    lcd.print(dispCO2);
    lcd.print(F(" ppm"));
  } else {
    lcd.print(F("--- ppm"));
  }
  clearLine(2, 20);
  lcd.setCursor(0, 2);
  drawCurrentTHP();
  lcd.setCursor(0, 3);
  lcd.print(F("400ppm 2x=Y 1x=N"));
#else
  lcd.setCursor(0, 0);
  drawCurrentCTH();
  lcd.setCursor(0, 1);
  lcd.print(F("2x=YES 1x=NO"));
#endif
}

void drawBrightnessBar(byte b, byte row) {
#if (DISPLAY_TYPE == 1)
  lcd.setCursor(4, row);
#else
  lcd.setCursor(2, row);
#endif
  if (b == 11) {
    lcd.print(F("[   AUTO   ]"));
    return;
  }
  lcd.print('[');
  for (byte i = 0; i < 10; i++) lcd.write(i < b ? 255 : ' ');
  lcd.print(']');
}

void drawIndicatorMode() {
  LEDType = podMode;
  if (!alarmActive && !chimeActive) setLED();   // сразу показать цвет нового режима
  lcd.setCursor(0, 0);
  lcd.print(F("LED Mode"));
#if (DISPLAY_TYPE == 1)
  lcd.setCursor(16, 0);
  lcd.print(F("SET"));
#endif
  lcd.setCursor(2, 1);
  printSensorName();
#if (DISPLAY_TYPE == 1)
  lcd.setCursor(2, 2);
  switch (LEDType) {
    case 0:
      lcd.print(F("CO2 "));
      if (co2Gate) lcd.print(F("---"));
      else lcd.print(dispCO2);
      lcd.print(F(" ppm"));
      break;
    case 1: lcd.print(dispHum); lcd.print('%'); break;
    case 2: printTemp(dispTemp); lcd.write(223); lcd.print('C'); break;
    case 3: lcd.print(dispPres); lcd.print(F(" mmHg")); break;
  }
  lcd.setCursor(0, 3);
  lcd.print(F("1x change  2x back"));
#endif
}

void drawLcdBrightness() {
  lcd.setCursor(0, 0);
  lcd.print(F("LCD Bright"));
#if (DISPLAY_TYPE == 1)
  lcd.setCursor(16, 0);
  lcd.print(F("SET"));
  lcd.setCursor(2, 1);
  printBrightness(LCD_BRIGHT);
  drawBrightnessBar(LCD_BRIGHT, 2);
  lcd.setCursor(0, 3);
  lcd.print(F("1x change  2x back"));
#else
  lcd.setCursor(12, 0);
  printBrightness(LCD_BRIGHT);
  drawBrightnessBar(LCD_BRIGHT, 1);
#endif
}

void drawLedBrightness() {
  lcd.setCursor(0, 0);
  lcd.print(F("LED Bright"));
#if (DISPLAY_TYPE == 1)
  lcd.setCursor(16, 0);
  lcd.print(F("SET"));
  lcd.setCursor(2, 1);
  printBrightness(LED_BRIGHT);
  drawBrightnessBar(LED_BRIGHT, 2);
  lcd.setCursor(0, 3);
  lcd.print(F("1x change  2x back"));
#else
  lcd.setCursor(12, 0);
  printBrightness(LED_BRIGHT);
  drawBrightnessBar(LED_BRIGHT, 1);
#endif
}

void nextMainScreen() {
  mode0scr++;
#if (CO2_SENSOR == 0)
  if (mode0scr == 1) mode0scr = 2;
#endif
  if (mode0scr == 5 || mode0scr == 6) mode0scr = 7;   // dew/heat удалены
#if (DISPLAY_TYPE == 1)
  if (mode0scr == 8) mode0scr = 9;      // 8-й экран только для 1602
#endif
  if (mode0scr > 9) mode0scr = 0;
}

void enterMenuItem() {
  if (podMode == 3) {                         // Save
    saveSettings();
    mode = 0;
  } else if (podMode == 4) {                  // Exit
    mode = 0;
  } else if (podMode >= 5 && podMode <= 7) {  // indicator/LCD brightness, indicator mode
    if (podMode == 5) mode = 254;             // LED Br
    else if (podMode == 6) mode = 253;        // LCD Br
    else mode = 252;                          // LED Mode
    if (mode == 254) podMode = LED_BRIGHT;
    else if (mode == 253) podMode = LCD_BRIGHT;
    else podMode = LEDType;
  } else if (podMode == 8) {                  // CO2 zero calibration
#if (CO2_SENSOR == 1)
    mode = 251;                               // сначала спрашиваем подтверждение
#endif
  } else if (podMode == 1 || podMode == 2 || podMode == 9) { // Alarm, Chime, Altitude
    beginEdit(podMode);
  }
  // Не даём тому же удержанию, которым вошли в пункт, сразу начать
  // авто-повтор и изменить значение. Отпустите и нажмите снова.
  button.resetStates();
}

/*
  mode:
  0 - main screen
  240 - generic editor (podMode 1,2,9; 1 = alarm)
  251 - CO2 calibration confirmation (yes/no)
  252 - indicator mode (podMode 0-3: CO2, humidity, temperature, pressure)
  253 - LCD brightness (podMode 0-11: 0-100% or auto)
  254 - indicator brightness (podMode 0-11: 0-100% or auto)
  255 - setup menu (podMode 1..9)
*/
void modesTick() {
  button.tick();
  boolean pressed = button.isPress();

  // Любое нажатие во время будильника останавливает его и не меняет режим.
  if (alarmActive && pressed) {
    ignoreClick = true;
    stopAlarm();
    button.resetStates();
    return;
  }

  // После остановки будильника не даём этому же нажатию переключить экран.
  if (ignoreClick) {
    if (button.isSingle() || button.isDouble() || button.isTriple() || button.isHolded()) {
      ignoreClick = false;
    }
    return;
  }

  if (pressed) touchBeep();
  if (pressed && mode >= 240) menuActivity = millis();
  if (mode >= 240 && button.isHold()) menuActivity = millis();   // не считаем удержание простоем

  boolean changeFlag = false;

  if (button.isSingle()) {    // одинарное нажатие: меняем значение / пункт меню
    if (mode == 251) {        // отмена калибровки CO2
      showMessage(F("Cancelled"), 9);
      delay(800);
      mode = 255;
      podMode = 8;
      changeFlag = true;
    } else if (mode == 0) {
      nextMainScreen();
      changeFlag = true;
    } else if (mode == 240) {
      if (podMode == 1) {
        if (alarmField == 3) {          // SET: применить и выйти в меню настроек
          finishEdit();
          mode = 255;
          menuActivity = millis();
        } else {
          alarmEditStep();
        }
      } else {
        editDirection = 1;
        editRepeatCount = 0;
        editApply(editStep);
      }
      changeFlag = true;
    } else if (mode >= 252) {
      podMode++;
      switch (mode) {
        case 252:             // режим LED индикатора
          if (podMode > 3) podMode = 0;
          LEDType = podMode;
          changeFlag = true;
          break;
        case 253:             // яркость LCD экрана
          if (podMode > 11) podMode = 0;
          LCD_BRIGHT = podMode;
          checkBrightness();
          changeFlag = true;
          break;
        case 254:             // яркость LED индикатора
          if (podMode > 11) podMode = 0;
          LED_BRIGHT = podMode;
          setLED();
          changeFlag = true;
          break;
        case 255:             // пункт главного меню
          if (podMode > 9) podMode = 1;
          changeFlag = true;
          break;
      }
    }
  }

  if (button.isDouble()) {                    // 2x: меню — назад, редактор — минус
    if (mode == 251) {                        // подтверждение калибровки CO2
#if (CO2_SENSOR == 1)
      calibrateCo2();
#endif
      mode = 255;
      podMode = 8;
      changeFlag = true;
    } else if (mode == 0) {
      mode = 255;                             // вход в меню
      podMode = 1;
      menuActivity = millis();
      changeFlag = true;
    } else if (mode == 255) {
      if (podMode == 3 || podMode == 4) {
        enterMenuItem();                     // Save/Exit — можно и 2x
      } else {
        podMode = (podMode <= 1) ? 9 : podMode - 1;   // 2x: предыдущий пункт
      }
      changeFlag = true;
    } else if (mode == 240) {
      if (podMode != 1) {
        editDirection = -1;
        editRepeatCount = 0;
        editApply(-editStep);
      }
      changeFlag = true;
    } else if (mode >= 252 && mode <= 254) {
      returnToMenu();
      changeFlag = true;
    }
  }

  if (button.isTriple()) {
    if (mode == 240 && podMode == 9) {         // 3x: назад в меню (только Altitude)
      finishEdit();
      mode = 255;
      changeFlag = true;
    } else if (mode == 0) {                   // тройное нажатие: вход в настройки
      mode = 255;
      podMode = 1;
      menuActivity = millis();
      changeFlag = true;
    }
  }

  if (button.isHolded() && mode != 251) {     // удержание кнопки (не подтверждает калибровку)
    switch (mode) {
      case 0:
        bigDig = !bigDig;
        break;
      case 240:       // в редакторе: шаг
        if (podMode == 2) {                     // Chime: проиграть, применить и выйти в меню
          editApply(0);
          mode = 255;
          menuActivity = millis();
          button.resetStates();                 // не даём этому же удержанию уйти дальше
        } else if (podMode == 1) {
          if (alarmField == 3) {                    // SET: применить и выйти в меню
            finishEdit();
            mode = 255;
            menuActivity = millis();
          } else {
            alarmField = (alarmField + 1) % 4;      // удержание: следующее поле
          }
        } else {
          editRepeatCount = 0;
          editApply(editDirection > 0 ? editStep : -editStep);
        }
        break;
      case 252:       // реж. индикатора
      case 253:       // ярк. экрана
      case 254:       // ярк. индикатора
        returnToMenu();
        break;
      case 255:       // главное меню
        enterMenuItem();
        break;
    }
    changeFlag = true;
  }

  // Авто-повтор удержания только для числового редактора Altitude.
  if (mode == 240 && podMode == 9 && button.isStep()) {
    if (editRepeatCount < 255) editRepeatCount++;
    int16_t step = (editRepeatCount > 10) ? editBigStep : editStep;
    editApply(editDirection > 0 ? step : -step);
    changeFlag = true;
  }

#if (RESET_CLOCK == 0)
  // В меню настроек и редакторах: при простое вернуться на главный экран.
  if (mode >= 240 && (unsigned long)(millis() - menuActivity) >= 120000UL) {
    if (mode == 240) finishEdit();
    menuActivity = millis();
    mode = 0;
    changeFlag = true;
  }
#endif

  if (changeFlag) {
    boolean repeatOnly = (mode == 240 && editRepeatCount > 0);
    if (mode >= 240 && !repeatOnly) {
      lcd.clear();
      lcd.setCursor(0, 0);
    }
    if (mode == 255) drawMenu();
    else if (mode == 251) drawCalibrationConfirm();
    else if (mode == 252) drawIndicatorMode();
    else if (mode == 253) drawLcdBrightness();
    else if (mode == 254) drawLedBrightness();
    else if (mode == 240) {
      if (repeatOnly) drawEditValueLine();
      else drawEdit();
    }
    else if (mode == 0) {
      lcd.clear();
      loadClock();
      drawSensors();
      if (DISPLAY_TYPE == 1) drawData();
    }
  }
}

void readSensors() {
  bme.takeForcedMeasurement();
  float t = bme.readTemperature();
  float h = bme.readHumidity();
  float p = bme.readPressure();
  bmeOk = !isnan(t) && !isnan(h) && !isnan(p) && p > 0;

  if (bmeOk) {
    dispTemp = t;
    int hh = (int)(h + humOffset + 0.5);
    if (hh < 0) hh = 0;
    if (hh > 100) hh = 100;
    dispHum = (byte)hh;
    stationHpa = p / 100.0;
    dispPres = (int)(stationHpa * 0.750062 + 0.5);
    updateQnh();
  }

#if (CO2_SENSOR == 1)
  dispCO2 = mhz19.getCarbonDioxide();
  if (dispCO2 < 0) dispCO2 = mhz19.getCarbonDioxide();   // повторный запрос при сбое UART (с)НР
#else
  dispCO2 = 0;
#endif

  if (millis() > CO2_WARMUP_MS) co2Gate = false;
  if (dispCO2 < 0 || dispCO2 > 4000) co2Gate = true;

  statsTick();

#if (DEBUG == 1)
  Serial.print(F("CO2=")); Serial.print(dispCO2);
  Serial.print(F(" T=")); Serial.print(dispTemp, 1);
  Serial.print(F(" H=")); Serial.print(dispHum, 1);
  Serial.print(F(" P=")); Serial.print(dispPres);
  Serial.print(F(" QNH=")); Serial.println(dispQnh);
#endif

  // Живое обновление значения на экране Altitude.
  if (lcdReady) {
    if (mode == 240 && podMode == 9) {
      drawEdit();
    } else if (mode == 251) {
      drawCalibrationConfirm();
    }
  }

  // Обновить цвет индикатора сразу после новых показаний. На главном экране
  // этим занимается clockTick, чтобы не сбить синхронное мигание с точками.
  if (mode != 0 && !alarmActive && !chimeActive) setLED();
}

void drawTimeNotSet() {               // RTC потерял питание: вместо времени показываем сообщение
#if (DISPLAY_TYPE == 1)
  byte rows = bigDig ? 4 : 2;
  for (byte r = 0; r < rows; r++) {
    lcd.setCursor(0, r);
    lcd.print(F("               "));  // 15 знакомест под крупные цифры
  }
  byte y = bigDig ? 1 : 0;
  lcd.setCursor(3, y);
  lcd.print(F("SET TIME"));
  lcd.setCursor(4, y + 1);
  lcd.print(F("VIA USB"));
#else
  for (byte r = 0; r < 2; r++) {
    lcd.setCursor(0, r);
    lcd.print(F("               "));
  }
  lcd.setCursor(4, 0);
  lcd.print(F("SET TIME"));
  lcd.setCursor(3, 1);
  lcd.print(F("VIA USB"));
#endif
}

void drawSensors() {
#if (DISPLAY_TYPE == 1)
  // дисплей 2004 ----------------------------------
  if (mode0scr == 7) { drawStatsAll(); return; }
  if (mode0scr == 9) { drawQnh(); return; }

  if (bigDig) {
    // Крупное значение слева, справа — ровная колонка остальных датчиков.
    byte row = 0;
    if (mode0scr != 2) {
      lcd.setCursor(16, row++);
      lcd.print('T');
      printValRight((int)dispTemp, 3);
    }
    if (mode0scr != 4) {
      lcd.setCursor(16, row++);
      lcd.print('H');
      printValRight(dispHum, 3);
    }
#if (CO2_SENSOR == 1)
    if (mode0scr != 1) {
      if (co2Gate || dispCO2 < 1000) {
        // слева остаётся пробел-разделитель
        lcd.setCursor(15, row++);
        lcd.print(' ');
        lcd.print('C');
        if (co2Gate) lcd.print(F("---"));
        else printValRight(dispCO2, 3);
      } else {
        // 4 цифры CO2 занимают разделитель, чтобы не потерять точность
        lcd.setCursor(15, row++);
        lcd.print('C');
        printValRight(dispCO2, 4);
      }
    }
#endif
    if (mode0scr != 3) {
      lcd.setCursor(16, row++);
      lcd.print('P');
      printValRight(dispPres, 3);
    }

    if (mode0scr == 0) {
      if (timeValid) drawClock(hrs, mins, 0, 0);
      else drawTimeNotSet();
    } else {
      lcd.setCursor(15, 3);
      if (timeValid) printTime(hrs * 60 + mins);
      else lcd.print(F("-:--"));
#if (CO2_SENSOR == 1)
      if (mode0scr == 1) drawPPM(dispCO2, 0, 0);
      else
#endif
      if (mode0scr == 2) drawTemp(dispTemp, 0, 0);
      else if (mode0scr == 3) drawPres(dispPres, 0, 0);
      else if (mode0scr == 4) drawHum(dispHum, 0, 0);
    }
    return;
  }

  if (mode0scr != 2) {                        // Температура ----------------------------
    lcd.setCursor(0, 2);
    lcd.print(F("T "));
    printTempField(dispTemp, 5);
    lcd.write(223);
  } else {
    drawTemp(dispTemp, 0, 0);
  }

  if (mode0scr != 4) {                        // Влажность ----------------------------
    lcd.setCursor(8, 2);
    lcd.print('H');
    printValRight(dispHum, 3);
    lcd.print('%');
  } else {
    drawHum(dispHum, 0, 0);
  }

#if (CO2_SENSOR == 1)
  if (mode0scr != 1) {                       // СО2 ----------------------------
    lcd.setCursor(13, 2);
    if (co2Gate) lcd.print(F(" ---ppm"));
    else { printValRight(dispCO2, 4); lcd.print(F("ppm")); }
  } else {
    drawPPM(dispCO2, 0, 0);
    lcd.setCursor(14, 2);
    lcd.print(F("ppm"));
  }
#endif

  if (mode0scr != 3) {                      // Давление ---------------------------
    lcd.setCursor(0, 3);
    lcd.print(F("P "));
    printValRight(dispPres, 3);
    lcd.print(F(" mmHg"));
  } else {
    drawPres(dispPres, 0, 0);
  }

  if (mode0scr == 0) {                      // время ----------------------------
    if (timeValid) drawClock(hrs, mins, 0, 0);
    else drawTimeNotSet();
  } else {
    lcd.setCursor(15, 3);
    if (timeValid) printTime(hrs * 60 + mins);
    else lcd.print(F("-:--"));
  }
#else

  // дисплей 1602 ----------------------------------
  if (mode0scr == 7) { drawStatsTH(); return; }
  if (mode0scr == 8) { drawStatsPC(); return; }
  if (mode0scr == 9) { drawQnh(); return; }

  if (!bigDig) {              // если только мелкими цифрами (с)НР
    lcd.setCursor(0, 0);
    lcd.print('T');
    printTempField(dispTemp, 5);
    lcd.write(223);
    lcd.setCursor(8, 0);
    lcd.print('H');
    printValRight(dispHum, 3);
    lcd.print('%');

    lcd.setCursor(0, 1);
    lcd.print('P');
    lcd.print(dispPres);
    lcd.print(' ');
#if (CO2_SENSOR == 1)
    lcd.print('C');
    if (co2Gate) lcd.print(F(" ---")); else printValRight(dispCO2, 4);
    lcd.print(' ');
#endif
    lcd.setCursor(11, 1);
    if (timeValid) printTime(hrs * 60 + mins);
    else lcd.print(F("-:-- "));
  } else {                    // для крупных цифр (с)НР
    switch (mode0scr) {
      case 0:
        if (timeValid) drawClock(hrs, mins, 0, 0);
        else drawTimeNotSet();
        break;
      case 1:
#if (CO2_SENSOR == 1)
        drawPPM(dispCO2, 0, 0);
#endif
        break;
      case 2:
        drawTemp(dispTemp, 2, 0);
        break;
      case 3:
        drawPres(dispPres, 2, 0);
        break;
      case 4:
        drawHum(dispHum, 0, 0);
        break;
    }
  }
#endif
}

boolean dotFlag;
void clockTick() {
  dotFlag = !dotFlag;
  if (dotFlag) {            // каждую секунду пересчёт времени
    secs++;
    if (secs > 59) {        // каждую минуту сверяемся с RTC
      byte oldHrs = hrs;
      now = rtc.now();      // убираем накопленный дрейф millis(): раньше сверка была раз в час
      secs = now.second();
      mins = now.minute();
      hrs = now.hour();
      if (hrs > 23) hrs = 0;
      if (mode == 0) {
        drawSensors();      // (с)НР
        if (DISPLAY_TYPE && hrs != oldHrs) drawData();
      }
    }

    checkAlarm();           // будильник проверяем раз в секунду, в любом режиме
    checkChime();

    if (timeValid && (DISP_MODE != 0 && mode == 0) && DISPLAY_TYPE == 1 && !bigDig && mode0scr <= 4) {   // секунды только на обычных экранах
      lcd.setCursor(15, 1);
      if (secs < 10) lcd.print('0');
      lcd.print(secs);
    }
  }

  if (timeValid && mode == 0 && mode0scr <= 4) {   // Точки (с)НР ---------------------------------------------------
    byte code;
    if (dotFlag) code = 165;
    else code = 32;
    if (mode0scr == 0 && ((bigDig && DISPLAY_TYPE == 0) || DISPLAY_TYPE == 1)) {          // мигание большими точками только в нулевом режиме главного экрана (с)НР
      if (bigDig && DISPLAY_TYPE == 1) lcd.setCursor(7, 2);
      else lcd.setCursor(7, 0);
      lcd.write(code);
      lcd.setCursor(7, 1);
      lcd.write(code);
    }
    else {
#if (DISPLAY_TYPE == 1)
      if (code == 165) code = 58;
      lcd.setCursor(17, 3);
      lcd.write(code);
#endif
    }
  }

  if (alarmActive || chimeActive) {
    setLED();                       // обновить LED_ON (яркость/авто)
    if (alarmActive) {
      if (dotFlag) setLEDcolor(48); // синий
      else setLEDcolor(0);
    } else {
      setLEDcolor(48);              // короткий сигнал — синяя вспышка
    }
  } else if ((((dispCO2 >= blinkLEDCO2) && LEDType == 0) || ((dispHum <= blinkLEDHum) && LEDType == 1) || ((dispTemp >= blinkLEDTemp) && LEDType == 2)) && !dotFlag) {
    setLEDcolor(0);                 // мигание индикатора при выходе за порог
  } else {
    setLED();
    // Мигать индикатором синхронно с секундными точками на главном экране.
    boolean dotsVisible = timeValid && mode == 0 && mode0scr <= 4;
#if (DISPLAY_TYPE == 0)
    if (!(mode0scr == 0 && bigDig)) dotsVisible = false;   // на 1602 точки только в крупных часах
#endif
    if (dotsVisible && !dotFlag) setLEDcolor(0);
  }
}

boolean testTimer(unsigned long & dataTimer, unsigned long setTimer) {   // Проверка таймеров (с)НР
  if (millis() - dataTimer >= setTimer) {
    dataTimer = millis();
    return true;
  } else {
    return false;
  }
}
