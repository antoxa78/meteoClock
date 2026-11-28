#include <math.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>

void playChimeSound(byte target);
#if (RESET_CLOCK == 0)
uint8_t randomChime();
#endif
void playRandomChime();
void setLEDcolor(byte color);
void drawEditValueLine();

boolean editGuard = false;      // true: кнопка, которой вошли в редактор, ещё не отпущена (не даём автоповтору сработать)
byte alarmStepCnt = 0;          // номер шага автоповтора (минуты ускоряются)

// ---------------------------------------------------------------------------
// Звуковой движок (v2.22).
//  * Ноты задаются номерами MIDI (C4 = 60, A4 = 440 Гц), строй равномерно
//    темперированный. Период считается в тактах CPU из таблицы одной октавы,
//    поэтому ошибка высоты не больше ~7 центов (раньше — до 50).
//  * Пьезо питается меандром с Timer2 (CTC). Предделитель подбирается под ноту,
//    а длительности «верхней» и «нижней» фазы можно менять: скважность меньше
//    50% снижает амплитуду основного тона. Так получается спад громкости,
//    как у колокольчика или струны (5 ступеней, см. buzzerDuty).
// ---------------------------------------------------------------------------
// Период ноты C4..B4 в тактах CPU (F_CPU / частота); для верхних октав сдвиг вправо.
static const uint16_t buzzerPeriod[12] PROGMEM = {
  61156, 57724, 54484, 51426, 48540, 45815, 43244, 40817, 38526, 36364, 34323, 32396
};
static const uint8_t buzzerPsShift[5] PROGMEM = {3, 5, 6, 7, 8};   // предделители 8, 32, 64, 128, 256
static const uint8_t buzzerPsBits[5] PROGMEM = {2, 3, 4, 5, 6};    // биты CS22:0 Timer2
static const uint8_t buzzerDuty[5] PROGMEM = {8, 5, 3, 2, 1};      // «верхняя» фаза в 1/16 периода; амплитуда 1, .83, .56, .38, .20

volatile uint8_t bzOcrHi = 100, bzOcrLo = 100;   // OCR2A для верхней и нижней фазы
volatile boolean bzPhase = false;
boolean bzRunning = false;

#if (BUZZER_ACTIVE == 0)
ISR(TIMER2_COMPA_vect) {
  bzPhase = !bzPhase;
  if (bzPhase) {
    BUZZER_PORT |= _BV(BUZZER_BIT);
#if (BUZZER_BRIDGE == 1)
    BUZZER_BRIDGE_PORT &= ~_BV(BUZZER_BRIDGE_BIT);
#endif
    OCR2A = bzOcrHi;
  } else {
    BUZZER_PORT &= ~_BV(BUZZER_BIT);
#if (BUZZER_BRIDGE == 1)
    BUZZER_BRIDGE_PORT |= _BV(BUZZER_BRIDGE_BIT);
#endif
    OCR2A = bzOcrLo;
  }
}
#endif

// Включить ноту note (MIDI 60..119) с уровнем громкости level (0 = максимум .. 4).
// Если звук уже идёт (legato или смена уровня), поток не прерывается.
void buzzerTone(byte note, byte level) {
#if (BUZZER_ACTIVE == 1)
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, HIGH);
#elif (BUZZER_ACTIVE == 2)
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);
#else
  if (note < 60) note = 60;
  if (note > 119) note = 119;
  byte s = note / 12 - 5;                                       // число октав выше четвёртой
  uint16_t cycles = pgm_read_word(&buzzerPeriod[note % 12]);
  while (s--) cycles = (cycles + 1) >> 1;                      // каждая октава вверх — вдвое короче период
  byte i = 0;                                                   // минимальный предделитель, при котором период <= 273 тиков
  while (i < 4 && (cycles >> pgm_read_byte(&buzzerPsShift[i])) > 273) i++;
  byte sh = pgm_read_byte(&buzzerPsShift[i]);
  uint16_t ticks = (cycles + (1 << (sh - 1))) >> sh;
  if (level > 4) level = 4;
  uint16_t hi = (ticks * pgm_read_byte(&buzzerDuty[level]) + 8) >> 4;
  uint8_t sreg = SREG;
  cli();
  bzOcrHi = hi - 1;
  bzOcrLo = ticks - hi - 1;
  if (!bzRunning) {
    pinMode(BUZZER_PIN, OUTPUT);
    digitalWrite(BUZZER_PIN, LOW);
#if (BUZZER_BRIDGE == 1)
    pinMode(BUZZER_BRIDGE_PIN, OUTPUT);
    digitalWrite(BUZZER_BRIDGE_PIN, HIGH);   // старт в противофазе
#endif
    bzPhase = false;
    TCCR2A = _BV(WGM21);                     // CTC
    OCR2A = bzOcrLo;
    TCNT2 = 0;
    TCCR2B = pgm_read_byte(&buzzerPsBits[i]);
    TIMSK2 |= _BV(OCIE2A);
    bzRunning = true;
  } else {
    byte cs = pgm_read_byte(&buzzerPsBits[i]);
    if (TCCR2B != cs) { TCCR2B = cs; TCNT2 = 0; }
  }
  SREG = sreg;
#endif
}

void buzzerOn() {
  buzzerTone(buzzerPlayIdx, 0);
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
  bzRunning = false;
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

  // Поля, совместимые с v1.9. Последний экран читаем только при START_BIG_CLOCK 0.
  byte v;
#if (START_BIG_CLOCK == 0)
  v = EEPROM.read(EE_MODE0);
  if (v <= 8) mode0scr = v;
  if (mode0scr == 5 || mode0scr == 6) mode0scr = 7;  // старые экраны dew/heat
#if (DISPLAY_TYPE == 1)
  if (mode0scr == 8) mode0scr = 7;       // 8-й экран есть только на 1602
#endif
#if (CO2_SENSOR == 0)
  if (mode0scr == 1) mode0scr = 2;
#endif

  bigDig = EEPROM.read(EE_BIGDIG) != 0;
#endif

  v = EEPROM.read(EE_LED_BRIGHT);
  if (v <= 11) LED_BRIGHT = v;
  v = EEPROM.read(EE_LED_TYPE);
  if (v <= 3) LEDType = v;
  else if (v == 4) LEDType = 3;          // v1.9: 4 = давление, теперь 3

  // Новые поля читаем только если они были сохранены v2.0+.
  // Иначе в EEPROM v1.9 там лежат 0xFF, которые выглядели бы как -1/-1%.
  byte ver = EEPROM.read(EE_VERSION);
  if (ver < 2 || ver > EE_VERSION_VALUE) return;

  int8_t t = (int8_t)EEPROM.read(EE_HUM_OFF);
  if (t >= -20 && t <= 20) humOffset = t;

  v = EEPROM.read(EE_ALARM_HOUR);
  if (v <= 23) alarmHour = v;
  v = EEPROM.read(EE_ALARM_MIN);
  if (v <= 59) alarmMin = v;

  v = EEPROM.read(EE_FLAGS);
  alarmEnabled = (v & FLAG_ALARM) ? 1 : 0;
  chimeEnabled = (v & FLAG_CHIME) ? 1 : 0;
  co2BeepEnabled = !(v & FLAG_NO_CO2_BEEP);

  v = EEPROM.read(EE_CHIME_SOUND);
  if (ver < 4 && v == 13) v = 21;   // в v2.18 значение 13 означало Random
  if (ver < 5 && v == 17) v = 21;   // в v2.21 значение 17 означало Random
  if (ver < 6 && v == 19) v = 21;   // в v2.21 (без Morning/Carol) 19 = Random
  if (ver < 7) v = (v == 21) ? CHIME_RANDOM : CHIME_SOUND_DEFAULT;  // нумерация изменилась
  else if (ver < 8 && v == 18) v = CHIME_RANDOM;   // в v2.23 значение 18 означало Random
  else if (ver < 9 && v == 19) v = CHIME_RANDOM;   // в v2.23 с Ode to Joy 19 = Random
  else if (ver == 9 && v == 20) v = CHIME_RANDOM;   // в v2.23 (ver 9) 20 = Random
  else if (ver == 10 && v == 22) v = CHIME_RANDOM;  // в v2.24 (ver 10) 22 = Random
  if (v <= CHIME_RANDOM) chimeSound = v;
}

void saveSettings() {
  EEPROM.update(EE_MAGIC, EE_MAGIC_VALUE);
#if (START_BIG_CLOCK == 0)
  EEPROM.update(EE_MODE0, mode0scr);
  EEPROM.update(EE_BIGDIG, bigDig ? 1 : 0);
#endif
  EEPROM.update(EE_LED_BRIGHT, LED_BRIGHT);
  EEPROM.update(EE_LED_TYPE, LEDType);
  EEPROM.update(EE_HUM_OFF, (uint8_t)humOffset);
  EEPROM.update(EE_ALARM_HOUR, (uint8_t)alarmHour);
  EEPROM.update(EE_ALARM_MIN, (uint8_t)alarmMin);
  byte flags = (alarmEnabled ? FLAG_ALARM : 0) | (chimeEnabled ? FLAG_CHIME : 0);
  if (!co2BeepEnabled) flags |= FLAG_NO_CO2_BEEP;
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

// Экраны min/max: во всех строках одинаковая ширина полей, поэтому
// разделитель "/" стоит в одной колонке (2004: поля по 5, 1602: P/CO2 по 4).
void drawStatsAll() {
  lcd.setCursor(0, 0);
  lcd.print(F("T "));
  printStatTemp(statTempMin, 5, statTempOk); lcd.print('/');
  printStatTemp(statTempMax, 5, statTempOk); lcd.print(' '); lcd.write(223); lcd.print('C');
  lcd.setCursor(0, 1);
  lcd.print(F("H "));
  printStatInt(statHumMin, 5, statHumOk); lcd.print('/');
  printStatInt(statHumMax, 5, statHumOk); lcd.print(F(" %"));
  lcd.setCursor(0, 2);
  lcd.print(F("P "));
  printStatInt(statPresMin, 5, statPresOk); lcd.print('/');
  printStatInt(statPresMax, 5, statPresOk); lcd.print(F(" mmHg"));
  lcd.setCursor(0, 3);
  lcd.print(F("C "));
  printStatInt(statCO2Min, 5, statCO2Ok); lcd.print('/');
  printStatInt(statCO2Max, 5, statCO2Ok); lcd.print(F(" ppm"));
}

void drawStatsTH() {
  lcd.setCursor(0, 0);
  lcd.print(F("T "));
  printStatTemp(statTempMin, 5, statTempOk); lcd.print('/');
  printStatTemp(statTempMax, 5, statTempOk); lcd.write(223);
  lcd.setCursor(0, 1);
  lcd.print(F("H "));
  printStatInt(statHumMin, 5, statHumOk); lcd.print('/');
  printStatInt(statHumMax, 5, statHumOk); lcd.print(F(" %"));
}

void drawStatsPC() {
  lcd.setCursor(0, 0);
  lcd.print(F("P "));
  printStatInt(statPresMin, 4, statPresOk); lcd.print('/');
  printStatInt(statPresMax, 4, statPresOk); lcd.print(F(" mmHg"));
  lcd.setCursor(0, 1);
  lcd.print(F("C "));
  printStatInt(statCO2Min, 4, statCO2Ok); lcd.print('/');
  printStatInt(statCO2Max, 4, statCO2Ok); lcd.print(F(" ppm"));
}

void beginEdit(byte item) {
  mode = 240;
  switch (item) {
    case 1:  alarmEditH = alarmHour; alarmEditM = alarmMin; alarmField = 0;
             editGuard = true; alarmStepCnt = 0; break;
    case 2:  chimeSel = chimeEnabled ? (chimeSound + 1) : 0; break;
  }
}

// Один шаг редактора Alarm: +1 к часам/минутам или переключение ON/OFF.
// held = шаг от удержания кнопки: после 8 шагов минуты идут по 5.
void alarmEditStep(boolean held) {
  if (alarmField == 0) {
    alarmEditH = (alarmEditH + 1) % 24;
  } else if (alarmField == 1) {
    if (held && alarmStepCnt >= 8) alarmEditM = (alarmEditM / 5 * 5 + 5) % 60;
    else alarmEditM = (alarmEditM + 1) % 60;
  } else {
    alarmEnabled = alarmEnabled ? 0 : 1;
    if (!alarmEnabled) stopAlarm();
  }
  if (alarmField < 2) alarmEnabled = 1;      // поменяли время — значит, будильник нужен
  alarmDoneDay = 0;
  if (held && alarmStepCnt < 255) alarmStepCnt++;
  alarmHour = alarmEditH;
  alarmMin = alarmEditM;
}

void finishEdit() {
  if (podMode == 1) {
    alarmHour = alarmEditH;
    alarmMin = alarmEditM;
    alarmDoneDay = 0;
    saveSettings();                          // будильник сохраняется сразу, отдельный Save не нужен
  }
}

void editApply(int16_t delta) {
  int16_t v = chimeSel + delta;
  if (v < 0) v = CHIME_MODE_COUNT + 1;      // wrap: после конца — начало
  if (v > CHIME_MODE_COUNT + 1) v = 0;
  chimeSel = v;
  chimeEnabled = (chimeSel > 0) ? 1 : 0;
  if (chimeEnabled) chimeSound = chimeSel - 1;
  else chimeActive = false;
  drawEditValueLine();          // сначала показать номер, затем играть превью
  if (!alarmActive) {
    if (chimeEnabled) {
      if (chimeSound == CHIME_RANDOM) playRandomChime();       // превью случайного варианта
      else playChimeSound((byte)chimeSound);                  // превью выбранного варианта
    } else {
      buzzerOff();
    }
  }
}

#if (DISPLAY_TYPE == 1)
static const char menuNames[] PROGMEM =
  "Alarm\0Chime\0Save\0Exit\0LED Br\0LED Mode\0CO2 Calib";
#else
static const char menuNames[] PROGMEM =
  "Alarm\0Chime\0Save\0Exit\0LED Br\0LED Mode\0CO2 Calib";
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
  if (!chimeEnabled) lcd.print(F("OFF"));
  else if (chimeSound >= CHIME_MODE_COUNT) lcd.print(F("Random"));
  else { lcd.print(F("Sound ")); lcd.print(chimeSound); }
}

void drawEditValueLine() {
#if (DISPLAY_TYPE == 1)
  clearLine(1, 20);
#else
  clearLine(1, 16);
#endif
  lcd.setCursor(2, 1);                 // та же колонка, что и в меню
  printEditValue();
}

// Скобка вокруг выбранного поля; у остальных на её месте пробел, поэтому
// строка имеет фиксированную ширину (14 знаков) и её можно перерисовывать
// без очистки экрана.
void alarmMark(byte f, char c) {
  lcd.print(alarmField == f ? c : ' ');
}

void drawAlarmFields() {
  lcd.setCursor(0, 0);
  alarmMark(0, '['); printTwoDigits(alarmEditH); alarmMark(0, ']');
  lcd.print(':');
  alarmMark(1, '['); printTwoDigits(alarmEditM); alarmMark(1, ']');
  alarmMark(2, '['); lcd.print(alarmEnabled ? F("ON ") : F("OFF")); alarmMark(2, ']');
}

void drawAlarmHint() {                 // подсказка зависит от выбранного поля
#if (DISPLAY_TYPE == 1)
  lcd.setCursor(0, 2);
  lcd.print(alarmField < 2 ? F("1x +1   hold = fast ") : F("1x = switch ON/OFF  "));
  lcd.setCursor(0, 3);
  lcd.print(alarmField < 2 ? F("2x = next field ") : F("2x = save & exit"));
#endif
}

void drawAlarmEdit() {
  drawAlarmFields();
  lcd.setCursor(0, 1);
#if (DISPLAY_TYPE == 1)
  lcd.write(6);                        // иконка колокольчика, как в меню
  lcd.print(F(" Alarm EDIT"));
  drawAlarmHint();
#else
  lcd.print(F("1x+ hold++ 2x->"));
#endif
}

void drawEdit() {
  if (podMode == 1) { drawAlarmEdit(); return; }
  lcd.setCursor(0, 0);
  lcd.write(7);                        // иконка ноты (редактор есть только у Alarm и Chime)
  lcd.print(' ');
#if (DISPLAY_TYPE == 1)
  printMenuItemName();
  lcd.setCursor(16, 0);
  lcd.print(F("EDIT"));
  drawEditValueLine();
  lcd.setCursor(0, 3);
  lcd.print(F("1x+ 2x- hold=OK"));
#else
  printMenuItemName();
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
    case 6: printSensorName(); break;
    case 7: lcd.print(F("400 ppm")); break;
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
  lcd.print(F("/7"));
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
    case 252: podMode = 6; break;   // LED Mode
  }
  mode = 255;
}

void stopAlarm() {
  alarmActive = false;
  chimeActive = false;
  touchBeepActive = false;
  co2WarnBeepActive = false;
  buzzerOff();
}


// ---- варианты почасового сигнала ----
// Таблица используется и для превью в меню, и для сигнала в начале часа.
// Шаг: {нота, длительность в 10 мс}. Нота — номер MIDI (nC4 = 60 ... nB8 = 119),
// флаг CT_LEGATO — без паузы после ноты (связно), либо служебный код:
//   CT_END   — конец мелодии
//   CT_REST  — пауза
//   CT_VOICE — «тембр» следующих нот: длительность = шаг спада громкости в 10 мс
//              (0 — ровный звук без спада)
// Между нотами без CT_LEGATO автоматически делается короткая пауза (1/8 ноты,
// не больше 25 мс), поэтому повторяющиеся ноты звучат раздельно.
#define CT_END 0
#define CT_REST 1
#define CT_VOICE 2
#define CT_LEGATO 0x80

#if (RESET_CLOCK == 0)
enum : uint8_t {
  nC4 = 60, nCs4, nD4, nDs4, nE4, nF4, nFs4, nG4, nGs4, nA4, nAs4, nB4,
  nC5, nCs5, nD5, nDs5, nE5, nF5, nFs5, nG5, nGs5, nA5, nAs5, nB5,
  nC6, nCs6, nD6, nDs6, nE6, nF6, nFs6, nG6, nGs6, nA6, nAs6, nB6,
  nC7, nCs7, nD7, nDs7, nE7, nF7, nFs7, nG7, nGs7, nA7, nAs7, nB7,
  nC8, nCs8, nD8, nDs8, nE8, nF8, nFs8, nG8, nGs8, nA8, nAs8, nB8
};

struct ChimeStep {
  uint8_t note;
  uint8_t dur10;   // длительность в 10 мс
};

static const ChimeStep chimePatterns[] PROGMEM = {
  {CT_VOICE, 8}, {nA7, 30}, {nF7, 60}, {CT_END, 0}, // 0 low doorbell
  {CT_VOICE, 6}, {nD7, 24}, {nD7, 8}, {nE7, 32}, {nD7, 32}, {nG7, 32}, {nFs7, 64}, {nD7, 24}, {nD7, 8}, {nE7, 32}, {nD7, 32}, {nA7, 32}, {nG7, 64}, {nD7, 24}, {nD7, 8}, {nD8, 32}, {nB7, 32}, {nG7, 32}, {nFs7, 32}, {nE7, 64}, {nC8, 24}, {nC8, 8}, {nB7, 32}, {nG7, 32}, {nA7, 32}, {nG7, 80}, {CT_END, 0}, // 1 Happy Birthday
  {CT_VOICE, 4}, {nAs6, 25}, {nAs6, 25}, {nAs6, 50}, {nAs6, 25}, {nAs6, 25}, {nAs6, 50}, {nAs6, 25}, {nCs7, 25}, {nFs7, 38}, {nGs7, 12}, {nAs7, 100}, {nB7, 25}, {nB7, 25}, {nB7, 38}, {nB7, 12}, {nB7, 25}, {nAs7, 25}, {nAs7, 25}, {nAs7, 12}, {nAs7, 12}, {nAs7, 25}, {nGs7, 25}, {nGs7, 25}, {nAs7, 25}, {nGs7, 50}, {nCs7, 50}, {CT_END, 0}, // 2 Jingle Bells
  {nE6|CT_LEGATO, 20}, {nC7|CT_LEGATO, 20}, {nE6|CT_LEGATO, 20}, {nC7|CT_LEGATO, 20}, {nE6|CT_LEGATO, 20}, {nC7|CT_LEGATO, 20}, {nE6|CT_LEGATO, 20}, {nC7|CT_LEGATO, 30}, {CT_REST, 5}, {nCs7|CT_LEGATO, 12}, {nFs6|CT_LEGATO, 12}, {nD7|CT_LEGATO, 12}, {nB5|CT_LEGATO, 12}, {nCs7|CT_LEGATO, 9}, {nFs6|CT_LEGATO, 9}, {nD7|CT_LEGATO, 9}, {nB5|CT_LEGATO, 9}, {nAs7|CT_LEGATO, 7}, {nC7|CT_LEGATO, 7}, {nAs7|CT_LEGATO, 7}, {nC7|CT_LEGATO, 7}, {nCs7|CT_LEGATO, 5}, {nFs6|CT_LEGATO, 5}, {nD7|CT_LEGATO, 5}, {nB5|CT_LEGATO, 5}, {nAs7|CT_LEGATO, 4}, {nC7|CT_LEGATO, 4}, {nAs7|CT_LEGATO, 4}, {nC7|CT_LEGATO, 4}, {nCs7|CT_LEGATO, 3}, {nFs6|CT_LEGATO, 3}, {nD7|CT_LEGATO, 3}, {nB5|CT_LEGATO, 3}, {nAs7|CT_LEGATO, 2}, {nC7|CT_LEGATO, 2}, {nAs7|CT_LEGATO, 2}, {nC7|CT_LEGATO, 2}, {CT_REST, 5}, {nB5|CT_LEGATO, 25}, {CT_END, 0}, // 3 old modem sound (~3.7 s, beginning cut)
  {CT_VOICE, 4}, {nG6, 4}, {nB6, 4}, {nD7, 4}, {nG7, 4}, {nB7, 4}, {nD8, 38}, {CT_END, 0}, // 4 rising run
  {CT_VOICE, 4}, {nC7, 6}, {nC8, 6}, {nG6, 6}, {nG7, 6}, {nE7, 38}, {CT_END, 0}, // 5 wide leap
  {CT_VOICE, 8}, {nG7, 15}, {nE7, 15}, {nG7, 42}, {CT_END, 0}, // 6 ding-dong-ding
  {CT_VOICE, 6}, {nG7, 6}, {nG7, 6}, {nC7, 46}, {CT_END, 0}, // 7 double strike
  {CT_VOICE, 0}, {nD8, 15}, {nC8, 15}, {nE7, 30}, {nFs7, 30}, {nB7, 15}, {nA7, 15}, {nC7, 30}, {nD7, 30}, {nA7, 15}, {nG7, 15}, {nB6, 30}, {nD7, 30}, {nG7, 90}, {CT_END, 0}, // 8 Grande Valse (Nokia)
  {CT_VOICE, 0}, {nC7, 13}, {nC7, 80}, {nB7, 10}, {nG7, 10}, {nA7, 10}, {nG7, 10}, {nE7, 10}, {nC7, 10}, {nG7, 10}, {nC7, 10}, {nF7, 10}, {nC7, 10}, {nE7, 10}, {nC7, 10}, {nD7, 10}, {nC7, 20}, {CT_END, 0}, // 9 Jumping (Nokia)
  {CT_VOICE, 10}, {nGs7, 33}, {nFs7, 33}, {nE7, 33}, {nB6, 70}, {CT_END, 0}, // 10 Westminster short
  {CT_VOICE, 10}, {nE7, 33}, {nGs7, 33}, {nFs7, 33}, {nB6, 56}, {nE7, 33}, {nFs7, 33}, {nGs7, 33}, {nE7, 56}, {nGs7, 33}, {nE7, 33}, {nFs7, 33}, {nB6, 56}, {nB6, 33}, {nFs7, 33}, {nGs7, 33}, {nE7, 100}, {CT_END, 0}, // 11 Westminster full (Big Ben)
  {CT_VOICE, 0}, {nG7, 18}, {nE7, 34}, {CT_REST, 38}, {nG7, 18}, {nE7, 34}, {CT_REST, 38}, {nG7, 18}, {nE7, 34}, {CT_REST, 38}, {nG7, 18}, {nE7, 56}, {CT_END, 0}, // 12 Cuckoo
  {CT_VOICE, 12}, {nG7, 15}, {nG7, 15}, {nG7, 15}, {nDs7, 83}, {nF7, 15}, {nF7, 15}, {nF7, 15}, {nD7, 103}, {nG7, 15}, {nG7, 15}, {nG7, 15}, {nDs7, 83}, {nF7, 15}, {nF7, 15}, {nF7, 15}, {nD7, 190}, {CT_END, 0}, // 13 Beethoven 5th opening
  {CT_VOICE, 0}, {nB6|CT_LEGATO, 10}, {CT_REST, 90}, {nB6|CT_LEGATO, 10}, {CT_REST, 90}, {nB6|CT_LEGATO, 10}, {CT_REST, 90}, {nB6|CT_LEGATO, 10}, {CT_REST, 90}, {nB6|CT_LEGATO, 10}, {CT_REST, 90}, {nB6|CT_LEGATO, 50}, {CT_END, 0}, // 14 Time signal (six pips)
  {CT_VOICE, 4}, {nE7, 22}, {nCs7, 22}, {nE7, 22}, {nDs7, 22}, {nC8, 22}, {nCs7, 22}, {nB7, 22}, {nB7, 22}, {nB7, 48}, {nAs7, 22}, {nAs7, 22}, {nAs7, 48}, {nA7, 22}, {nA7, 22}, {nA7, 48}, {nGs7, 22}, {nCs7, 22}, {nA7, 22}, {nCs7, 22}, {nGs7, 45}, {CT_END, 0}, // 15 Sugar Plum Fairy
  {CT_VOICE, 0}, {nA7|CT_LEGATO, 20}, {nFs7|CT_LEGATO, 20}, {nE7|CT_LEGATO, 20}, {nD7|CT_LEGATO, 20}, {nE7|CT_LEGATO, 20}, {nFs7, 36}, {CT_REST, 6}, {nA7|CT_LEGATO, 20}, {nFs7|CT_LEGATO, 20}, {nE7|CT_LEGATO, 20}, {nD7|CT_LEGATO, 20}, {nE7|CT_LEGATO, 20}, {nFs7|CT_LEGATO, 20}, {nE7|CT_LEGATO, 20}, {nFs7, 36}, {CT_REST, 6}, {nA7|CT_LEGATO, 20}, {nFs7|CT_LEGATO, 20}, {nA7|CT_LEGATO, 20}, {nB7|CT_LEGATO, 20}, {nFs7|CT_LEGATO, 20}, {nB7, 36}, {CT_REST, 6}, {nA7|CT_LEGATO, 20}, {nFs7|CT_LEGATO, 20}, {nE7|CT_LEGATO, 20}, {nD7, 50}, {CT_END, 0}, // 16 Morning Mood
  {CT_VOICE, 4}, {nDs7, 17}, {nD7, 17}, {nDs7, 17}, {nC7, 19}, {nDs7, 17}, {nD7, 17}, {nDs7, 17}, {nC7, 19}, {nG7, 17}, {nF7, 17}, {nG7, 17}, {nDs7, 19}, {nG7, 17}, {nF7, 17}, {nG7, 17}, {nDs7, 19}, {nC7, 17}, {nC7, 17}, {nC7, 17}, {nAs7, 17}, {nGs7, 17}, {nG7, 17}, {nG7, 17}, {nG7, 19}, {nF7, 17}, {nDs7, 17}, {nF7, 17}, {nF7, 17}, {nF7, 17}, {nG7, 17}, {nF7, 17}, {nC7, 30}, {CT_END, 0}, // 17 Carol of the Bells
  {CT_VOICE, 4},
  {nFs7, 35}, {nFs7|CT_LEGATO, 35}, {nG7|CT_LEGATO, 35}, {nA7, 35},
  {nA7|CT_LEGATO, 35}, {nG7|CT_LEGATO, 35}, {nFs7|CT_LEGATO, 35}, {nE7|CT_LEGATO, 35},
  {nD7, 35}, {nD7|CT_LEGATO, 35}, {nE7|CT_LEGATO, 35}, {nFs7, 35},
  {nFs7|CT_LEGATO, 52}, {nE7, 18}, {nE7|CT_LEGATO, 70},
  {CT_END, 0}, // 18 Ode to Joy (Beethoven, opening phrase)
  {CT_VOICE, 4},
  {nC7|CT_LEGATO, 110}, {nB6|CT_LEGATO, 83}, {nFs6|CT_LEGATO, 28}, {nA6|CT_LEGATO, 55},
  {nG6|CT_LEGATO, 55}, {nF6|CT_LEGATO, 28}, {nD6, 28}, {nC6|CT_LEGATO, 110},
  {CT_END, 0}, // 19 Wedding March (Mendelssohn, main theme)
  {CT_VOICE, 6},
  {nE7, 15}, {CT_REST, 27},
  {nE7|CT_LEGATO, 62}, {nGs7|CT_LEGATO, 22}, {nF7|CT_LEGATO, 19}, {nE7|CT_LEGATO, 21},
  {nGs7, 23}, {CT_REST, 21},
  {nGs7|CT_LEGATO, 59}, {nB7|CT_LEGATO, 20}, {nA7|CT_LEGATO, 20}, {nGs7|CT_LEGATO, 21},
  {nA7, 11}, {CT_REST, 31},
  {nA7|CT_LEGATO, 63}, {nC8|CT_LEGATO, 22}, {nB7|CT_LEGATO, 19}, {nA7|CT_LEGATO, 23},
  {nGs7|CT_LEGATO, 41}, {nF7, 22}, {nF7|CT_LEGATO, 20}, {nE7, 63}, {CT_REST, 20},
  {nGs7|CT_LEGATO, 42}, {nF7|CT_LEGATO, 21}, {nE7, 20}, {nE7, 21}, {nE7|CT_LEGATO, 41},
  {nF7, 22}, {nF7|CT_LEGATO, 41}, {nE7|CT_LEGATO, 19}, {nD7, 20}, {nD7, 22},
  {nD7, 43}, {nD7|CT_LEGATO, 42}, {nF7|CT_LEGATO, 35}, {nE7|CT_LEGATO, 9}, {nD7, 20},
  {nD7|CT_LEGATO, 21}, {nA7|CT_LEGATO, 42}, {nGs7|CT_LEGATO, 39}, {nF7, 21}, {nF7|CT_LEGATO, 22},
  {nA7, 84}, {CT_END, 0}, // 20 Hava Nagila (main theme + second phrase)
  {CT_VOICE, 16},
  {nA6|CT_LEGATO, 38}, {nB6|CT_LEGATO, 38}, {nC7|CT_LEGATO, 38}, {nD7|CT_LEGATO, 38}, {nE7, 75},
  {nE7|CT_LEGATO, 75}, {nF7|CT_LEGATO, 38}, {nE7|CT_LEGATO, 38}, {nF7|CT_LEGATO, 38}, {nA7|CT_LEGATO, 38},
  {nE7|CT_LEGATO, 150}, {nD7, 75}, {nD7|CT_LEGATO, 38}, {nB6|CT_LEGATO, 38}, {nC7, 75},
  {nC7|CT_LEGATO, 75}, {nB6|CT_LEGATO, 38}, {nA6|CT_LEGATO, 38}, {nB6|CT_LEGATO, 38}, {nC7|CT_LEGATO, 38},
  {nA6, 113}, {CT_REST, 20},
  {nA6, 75}, {nA7, 75}, {nA7, 75}, {nA7|CT_LEGATO, 75}, {nG7|CT_LEGATO, 38},
  {nA7|CT_LEGATO, 38}, {nG7|CT_LEGATO, 38}, {nF7|CT_LEGATO, 38}, {nE7, 150}, {CT_END, 0}, // 21 Hatikvah (opening + refrain)
  {CT_VOICE, 24},
  {nB6|CT_LEGATO, 46}, {nD7|CT_LEGATO, 46}, {nFs7|CT_LEGATO, 46}, {nD7|CT_LEGATO, 46},
  {nE7|CT_LEGATO, 91}, {nD7|CT_LEGATO, 46}, {nCs7|CT_LEGATO, 46},
  {nFs7|CT_LEGATO, 91}, {nE7|CT_LEGATO, 91},
  {nB6|CT_LEGATO, 182},
  {nD7|CT_LEGATO, 46}, {nFs7|CT_LEGATO, 46}, {nA7, 46}, {nA7|CT_LEGATO, 46},
  {nB7|CT_LEGATO, 91}, {nA7|CT_LEGATO, 46}, {nG7|CT_LEGATO, 46},
  {nFs7, 182}, {CT_END, 0}, // 22 Podmoskovnie Vechera (opening phrase)
};

uint16_t chimeModeStart(byte target) {
  if (target >= CHIME_MODE_COUNT) target = 0;
  uint16_t pos = 0;
  byte mode = 0;
  while (mode < target) {
    if (pgm_read_byte(&chimePatterns[pos].note) == CT_END) mode++;
    pos++;
  }
  return pos;
}

static const byte ledColors[] PROGMEM = {3, 11, 12, 60, 48, 51, 63, 15};

byte randomLEDcolor() {
  static byte idx = 0, rnd = 0xA5;
  rnd = rnd * 5 + 1;
  idx = (idx + 1 + ((rnd >> 4) & 6)) & 7;   // нечётный шаг: цвет не повторяется подряд
  return pgm_read_byte(&ledColors[idx]);
}

void startupLEDtick() {
  static byte ci = 0;
  static byte skip = 0;
  if (!startupLED) return;
  if (++skip < 8) return;              // менять цвет примерно раз в 8 нот
  skip = 0;
  setLEDcolor(pgm_read_byte(&ledColors[ci]));
  if (++ci >= sizeof(ledColors)) ci = 0;
}

// ---- секвенсор ----
// Время шагов считается по «идеальной» шкале (seqT0 += длительность), а не от
// момента, когда до шага дошёл loop(): задержки основного цикла не накапливаются
// и не сбивают темп.
static uint8_t seqVoice;          // шаг спада громкости, 10 мс (0 - без спада)
static uint8_t seqNote;           // текущая нота
static uint8_t seqLevel;          // текущая ступень громкости 0..4
static boolean seqSound;          // нота ещё звучит
static uint16_t seqDur;           // длительность текущего шага, мс
static uint16_t seqGate;          // через сколько мс после начала шага звук выключается
static unsigned long seqT0;       // начало текущего шага

void seqStart(byte target, unsigned long ms) {
  chimePos = chimeModeStart(target);
  seqVoice = 0;
  seqSound = false;
  seqDur = 0;
  seqT0 = ms;
}

boolean seqLoad() {                // загрузить следующий звучащий шаг или паузу; false - конец
  for (;;) {
    uint8_t note = pgm_read_byte(&chimePatterns[chimePos].note);
    uint8_t dur10 = pgm_read_byte(&chimePatterns[chimePos].dur10);
    chimePos++;
    if (note == CT_END) return false;
    if (note == CT_VOICE) { seqVoice = dur10; continue; }
    seqDur = (uint16_t)dur10 * 10;
    if (note == CT_REST) {
      if (seqSound) buzzerOff();
      seqSound = false;
      return true;
    }
    seqNote = note & 0x7F;
    uint16_t gap = 0;
    if (!(note & CT_LEGATO)) {
      gap = seqDur >> 3;
      if (gap > 25) gap = 25;
    }
    seqGate = seqDur - gap;
    seqLevel = 0;
    seqSound = true;
    buzzerTone(seqNote, 0);
    if (startupLED) startupLEDtick();
    else setLEDcolor(randomLEDcolor());   // случайный цвет на каждую ноту
    return true;
  }
}

boolean seqTick(unsigned long ms) {   // вызывать как можно чаще; false - мелодия закончилась
  uint16_t el = (uint16_t)(ms - seqT0);
  if (el >= seqDur) {
    seqT0 += seqDur;
    if (!seqLoad()) return false;
    el = (uint16_t)(ms - seqT0);
  }
  if (seqSound) {
    if (el >= seqGate) {
      buzzerOff();
      seqSound = false;
    } else if (seqVoice) {
      uint16_t lv = el / ((uint16_t)seqVoice * 10);
      if (lv > 4) lv = 4;
      if (lv != seqLevel) {
        seqLevel = lv;
        buzzerTone(seqNote, seqLevel);
      }
    }
  }
  return true;
}

void playChimeSound(byte target) {
  stopAlarm();
  if (!startupLED) setLED();
  seqStart(target, millis());
  while (seqTick(millis())) { }
  buzzerOff();
  if (!startupLED) setLED();
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
  if (!TOUCH_BEEP || alarmActive || chimeActive || co2WarnBeepActive) return;
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
  co2WarnBeepActive = false;
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
  seqStart(snd, millis());
  chimeActive = true;
  co2WarnBeepActive = false;
  setLED();                         // обновить яркость и взять текущий цвет
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
        buzzerPlayIdx = BUZZER_FREQ_IDX - 9;       // вторая нота (D7): большая секста вниз
        buzzerOn();
      } else {
        buzzerOff();
      }
    }
  }
#if (RESET_CLOCK == 0)
  else if (chimeActive) {
    if (!seqTick(ms)) {
      buzzerOff();
      chimeActive = false;
      setLED();                     // вернуть цвет по датчику
    }
  }
#endif
  else if (touchBeepActive) {
    if (ms - touchBeepStart >= TOUCH_BEEP_MS) {
      buzzerOff();
      touchBeepActive = false;
    }
  }
  else if (co2WarnBeepActive) {
    // Серия из CO2_WARN_BEEPS писков с паузами CO2_WARN_GAP_MS.
    uint16_t dur = (co2WarnPhase & 1) ? CO2_WARN_GAP_MS : CO2_WARN_BEEP_MS;
    if (ms - co2WarnBeepStart >= dur) {
      if (co2WarnPhase >= (CO2_WARN_BEEPS - 1) * 2) {
        buzzerOff();
        co2WarnBeepActive = false;
      } else {
        co2WarnPhase++;
        co2WarnBeepStart = ms;
        if (co2WarnPhase & 1) buzzerOff();
        else {
          buzzerPlayIdx = BUZZER_FREQ_IDX;
          buzzerOn();
        }
      }
    }
  }
}

void checkCO2Warning() {
  if (!co2BeepEnabled || co2Gate || dispCO2 < CO2_WARN_LEVEL) {
    co2WarnTimer = 0;
    return;
  }
  if (alarmActive || chimeActive || touchBeepActive || co2WarnBeepActive) return;
  unsigned long ms = millis();
  if (co2WarnTimer == 0 || (unsigned long)(ms - co2WarnTimer) >= CO2_WARN_PERIOD_MS) {
    co2WarnTimer = ms;
    co2WarnPhase = 0;
    co2WarnBeepActive = true;
    co2WarnBeepStart = ms;
    buzzerPlayIdx = BUZZER_FREQ_IDX;
    buzzerOn();
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
    showMessage(F("Need 400 ppm"), 12);
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

#else
void drawCurrentCTH() {
  lcd.print('C');
  if (dispCO2 >= 0) printValRight(dispCO2, 4); else lcd.print(F(" ---"));
  lcd.print(F(" T"));
  printValRight(tempInt(), 3);
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
  lcd.print(F("400ppm?  2x=Y  1x=N"));
#else
  lcd.setCursor(0, 0);
  drawCurrentCTH();
  lcd.setCursor(0, 1);
  lcd.print(F("2x=YES 1x=NO"));
#endif
}

void drawBrightnessBar(byte b, byte row) {
  lcd.setCursor(2, row);                // та же колонка, что и у значения
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
  lcd.setCursor(10, 0);
  lcd.print(co2BeepEnabled ? F("B:ON") : F("B:OFF"));
#if (DISPLAY_TYPE == 1)
  lcd.setCursor(16, 0);
  lcd.print(F("EDIT"));
#endif
  lcd.setCursor(2, 1);
  printSensorName();
#if (DISPLAY_TYPE == 1)
  lcd.setCursor(2, 2);                  // текущее значение (название датчика уже в строке выше)
  switch (LEDType) {
    case 0:
      if (co2Gate) lcd.print(F("---"));
      else lcd.print(dispCO2);
      lcd.print(F(" ppm"));
      break;
    case 1: lcd.print(dispHum); lcd.print('%'); break;
    case 2: printTemp(dispTemp); lcd.write(223); lcd.print('C'); break;
    case 3: lcd.print(dispPres); lcd.print(F(" mmHg")); break;
  }
  lcd.setCursor(0, 3);
  lcd.print(F("1x chg 2x back H=bip"));
#endif
}

void drawBrightness(byte b) {
  lcd.setCursor(0, 0);
  lcd.print(F("LED Bright"));
#if (DISPLAY_TYPE == 1)
  lcd.setCursor(16, 0);
  lcd.print(F("EDIT"));
  lcd.setCursor(2, 1);
  printBrightness(b);
  drawBrightnessBar(b, 2);
  lcd.setCursor(0, 3);
  lcd.print(F("1x change  2x back"));
#else
  lcd.setCursor(12, 0);
  printBrightness(b);
  drawBrightnessBar(b, 1);
#endif
}

void nextMainScreen() {
  mode0scr++;
#if (CO2_SENSOR == 0)
  if (mode0scr == 1) mode0scr = 2;
#endif
  if (mode0scr == 5 || mode0scr == 6) mode0scr = 7;   // dew/heat удалены
#if (DISPLAY_TYPE == 1)
  if (mode0scr > 7) mode0scr = 0;
#else
  if (mode0scr > 8) mode0scr = 0;
#endif
}

void enterMenuItem() {
  if (podMode == 3) {                         // Save
    saveSettings();
    showMessage(F("Settings saved"), 14);     // подтверждение, как у калибровки CO2
    delay(800);
    mode = 0;
  } else if (podMode == 4) {                  // Exit
    mode = 0;
  } else if (podMode >= 5 && podMode <= 6) {  // LED brightness, LED mode
    if (podMode == 5) { mode = 254; podMode = LED_BRIGHT; }
    else { mode = 252; podMode = LEDType; }
  } else if (podMode == 7) {                  // CO2 zero calibration
#if (CO2_SENSOR == 1)
    mode = 251;                               // сначала спрашиваем подтверждение
#endif
  } else if (podMode == 1 || podMode == 2) { // Alarm, Chime
    beginEdit(podMode);
  }
  // Не даём тому же удержанию, которым вошли в пункт, сразу начать
  // авто-повтор и изменить значение. Отпустите и нажмите снова.
  button.resetStates();
}

/*
  mode:
  0 - main screen
  240 - generic editor (podMode 1,2; 1 = alarm)
  251 - CO2 calibration confirmation (yes/no)
  252 - indicator mode (podMode 0-3: CO2, humidity, temperature, pressure)
  254 - indicator brightness (podMode 0-11: 0-100% or auto)
  255 - setup menu (podMode 1..7)
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

  if (button.isRelease()) editGuard = false;      // кнопку, которой вошли в редактор, отпустили
  if (pressed) { touchBeep(); alarmStepCnt = 0; }
  if (pressed && mode >= 240) menuActivity = millis();
  if (mode >= 240 && button.isHold()) menuActivity = millis();   // не считаем удержание простоем

  boolean changeFlag = false;

  if (button.isSingle()) {    // одинарное нажатие: меняем значение / пункт меню
    if (mode == 251) {        // отмена калибровки CO2
      showMessage(F("Cancelled"), 9);
      delay(800);
      mode = 255;
      podMode = 7;
      changeFlag = true;
    } else if (mode == 0) {
      nextMainScreen();
      changeFlag = true;
    } else if (mode == 240) {
      if (podMode == 1) {
        alarmEditStep(false);           // 1x: +1 / переключить ON-OFF
        drawAlarmFields();              // перерисовываем только строку полей, без мерцания
      } else {
        editApply(1);
        changeFlag = true;
      }
    } else if (mode >= 252) {
      if (mode == 252) {
        podMode++;
        if (podMode > 3) podMode = 0;
        LEDType = podMode;
        setLED();
        changeFlag = true;
      } else {
        podMode++;
        switch (mode) {
          case 254:             // яркость LED индикатора
            if (podMode > 11) podMode = 0;
            LED_BRIGHT = podMode;
            setLED();
            changeFlag = true;
            break;
          case 255:             // пункт главного меню
            if (podMode > 7) podMode = 1;
            changeFlag = true;
            break;
        }
      }
    }
  }

  if (button.isDouble()) {                    // 2x: меню — назад, редактор — минус
    if (mode == 251) {                        // подтверждение калибровки CO2
#if (CO2_SENSOR == 1)
      calibrateCo2();
#endif
      mode = 255;
      podMode = 7;
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
        podMode = (podMode <= 1) ? 7 : podMode - 1;   // 2x: предыдущий пункт
      }
      changeFlag = true;
    } else if (mode == 240) {
      if (podMode == 1) {
        if (alarmField < 2) {           // 2x: следующее поле
          alarmField++;
          drawAlarmFields();
          drawAlarmHint();
        } else {                        // 2x на ON/OFF: готово, сохранить и выйти в меню
          finishEdit();
          mode = 255;
          menuActivity = millis();
          changeFlag = true;
        }
      } else {
        editApply(-1);
        changeFlag = true;
      }
    } else if (mode == 252 || mode == 254) {
      returnToMenu();
      changeFlag = true;
    }
  }

  if (button.isTriple()) {
    if (mode == 0) {                          // тройное нажатие: вход в настройки
      mode = 255;
      podMode = 1;
      menuActivity = millis();
      changeFlag = true;
    }
  }

  if (button.isHolded() && mode != 251) {     // удержание кнопки (не подтверждает калибровку)
    boolean holdStep = false;
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
        } else if (podMode == 1 && !editGuard && alarmField < 2) {    // удержание: быстрое изменение (автоповтор)
          alarmEditStep(true);
          drawAlarmFields();
          holdStep = true;
        }
        break;
      case 252:       // реж. индикатора: удержание переключает писк CO2
        co2BeepEnabled = !co2BeepEnabled;
        changeFlag = true;
        break;
      case 254:       // ярк. индикатора
        returnToMenu();
        break;
      case 255:       // главное меню
        enterMenuItem();
        break;
    }
    if (!holdStep) changeFlag = true;       // шаг в редакторе Alarm не требует полной перерисовки
  }

  if (mode == 240 && podMode == 1 && !editGuard && alarmField < 2 && button.isStep()) {   // удержание: автоповтор
    alarmEditStep(true);
    drawAlarmFields();
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
    if (mode >= 240) {
      lcd.clear();
      lcd.setCursor(0, 0);
    }
    if (mode == 255) drawMenu();
    else if (mode == 251) drawCalibrationConfirm();
    else if (mode == 252) drawIndicatorMode();
    else if (mode == 254) drawBrightness(LED_BRIGHT);
    else if (mode == 240) drawEdit();
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
    dispPres = (int)(p * 0.00750062 + 0.5);
  }

#if (CO2_SENSOR == 1)
  dispCO2 = mhz19.getCarbonDioxide();
  if (dispCO2 < 0) dispCO2 = mhz19.getCarbonDioxide();   // повторный запрос при сбое UART (с)НР
#else
  dispCO2 = 0;
#endif

  if (millis() > CO2_WARMUP_MS) co2Gate = false;
  if (dispCO2 < 0 || dispCO2 > 4000) co2Gate = true;

  checkCO2Warning();
  statsTick();

#if (DEBUG == 1)
  Serial.print(F("CO2=")); Serial.print(dispCO2);
  Serial.print(F(" T=")); Serial.print(dispTemp, 1);
  Serial.print(F(" H=")); Serial.print(dispHum, 1);
  Serial.print(F(" P=")); Serial.println(dispPres);
#endif

  // Живое обновление значения на экране калибровки.
  if (lcdReady) {
    if (mode == 251) {
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

  if (bigDig) {
    // Крупное значение слева, справа — ровная колонка остальных датчиков.
    byte row = 0;
    if (mode0scr != 2) {
      lcd.setCursor(16, row++);
      lcd.print('T');
      printValRight(tempInt(), 3);
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
      if (mode0scr == 1) {
        lcd.setCursor(16, 3);                // ppm в колонке подписей T/H/P
        lcd.print(F("ppm"));
#if (CO2_SENSOR == 1)
        drawPPM(dispCO2, 0, 0);
#endif
      } else {
        lcd.setCursor(15, 3);
        if (timeValid) printTime(hrs * 60 + mins);
        else lcd.print(F("-:--"));
        if (mode0scr == 2) drawTemp(dispTemp, 0, 0);
        else if (mode0scr == 3) drawPres(dispPres, 0, 0);
        else if (mode0scr == 4) drawHum(dispHum, 0, 0);
      }
    }
    return;
  }

  if (mode0scr != 2) {                        // Температура ----------------------------
    lcd.setCursor(0, 2);
    lcd.print('T');                           // T/H сдвинуты влево: место для пробела перед CO2
    printTempField(dispTemp, 5);
    lcd.write(223);
  } else {
    drawTemp(dispTemp, 0, 0);
  }

  if (mode0scr != 4) {                        // Влажность ----------------------------
    lcd.setCursor(7, 2);
    lcd.print('H');
    printValRight(dispHum, 3);
    lcd.print('%');
  } else {
    drawHum(dispHum, 0, 0);
  }

#if (CO2_SENSOR == 1)
  if (mode0scr != 1) {                       // СО2 ----------------------------
    lcd.setCursor(12, 2);                    // пробел между % и ppm
    lcd.print(' ');
    if (co2Gate) lcd.print(F("---ppm"));
    else { printValRight(dispCO2, dispCO2 >= 1000 ? 4 : 3); lcd.print(F("ppm")); }
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
        drawTemp(dispTemp, 1, 0);   // °C занимает колонки 14-15
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
      if (bigDig && DISPLAY_TYPE == 1) {
        // Крупное двоеточие: верхняя точка в верхней половине, нижняя — в нижней.
        lcd.setCursor(7, 1);
        lcd.write(dotFlag ? 5 : 32);
        lcd.setCursor(7, 2);
        lcd.write(dotFlag ? 4 : 32);
      } else {
        lcd.setCursor(7, 0);
        lcd.write(code);
        lcd.setCursor(7, 1);
        lcd.write(code);
      }
    }
    else {
      // На крупном экране CO2 времени нет (там ppm), поэтому его двоеточие
      // не должно мигать поверх единицы измерения.
#if (DISPLAY_TYPE == 1)
      if (!(bigDig && mode0scr == 1)) {
        if (code == 165) code = 58;
        lcd.setCursor(17, 3);
        lcd.write(code);
      }
#endif
    }
  }

  if (alarmActive) {
    setLED();                       // обновить LED_ON (яркость/авто)
    if (dotFlag) setLEDcolor(48);   // синий
    else setLEDcolor(0);
  } else if (chimeActive) {
    // цвет уже меняется на каждой ноте в tickBuzzer()/playChimeSound()
  } else if ((((dispCO2 >= blinkLEDCO2) && LEDType == 0) || ((dispHum <= blinkLEDHum) && LEDType == 1) || ((dispTemp >= blinkLEDTemp) && LEDType == 2)) && !dotFlag) {
    setLEDcolor(0);                 // мигание индикатора при выходе за порог
  } else {
    setLED();
    // Мигать индикатором синхронно с секундными точками на главном экране.
    boolean dotsVisible = timeValid && mode == 0 && mode0scr <= 4;
#if (DISPLAY_TYPE == 0)
    if (!(mode0scr == 0 && bigDig)) dotsVisible = false;   // на 1602 точки только в крупных часах
#endif
    // На 2004 индикатор мигает с секундами и на крупном CO2; двоеточие
    // на этом экране не перерисовывается, чтобы не портить «ppm».
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
