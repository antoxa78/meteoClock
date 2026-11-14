/*
  Скетч к проекту "Домашняя метеостанция"
  Страница проекта (схемы, описания): https://alexgyver.ru/meteoclock/
  Исходники на GitHub: https://github.com/AlexGyver/MeteoClock
  Нравится, как написан и закомментирован код? Поддержи автора! https://alexgyver.ru/support_alex/
  Автор: AlexGyver Technologies, 2018
  http://AlexGyver.ru/
  Доработал Роман Новиков (с)НР
  обновления здесь: https://github.com/Norovl/meteoClock
*/

/*
  Время и дата устанавливаются автоматически при загрузке прошивки (время компиляции, как на компьютере в момент сборки).
  В модуле реального времени стоит батарейка, которая продолжает отсчёт времени после выключения/сброса питания.
  Время компиляции отстаёт от реального на время сборки и загрузки прошивки (обычно 10-30 сек).
  Для точной синхронизации с ПК отправьте в последовательный порт (9600) строку вида:
    T2026-08-17 14:23:45
  Пример для Linux (сразу после загрузки прошивки):
    stty -F /dev/ttyUSB0 9600 raw -echo; printf 'T%s\n' "$(date '+%Y-%m-%d %H:%M:%S')" > /dev/ttyUSB0
  При RESET_CLOCK = 1 устройство в течение 10 секунд после запуска ждёт эту строку и, если она не пришла, ставит время компиляции.
  Команда работает и в рабочем режиме в любой момент.
  Как настроить время на часах:
	- Ставим настройку RESET_CLOCK на 1
  - Прошиваемся
  - Сразу ставим RESET_CLOCK 0
  - И прошиваемся ещё раз
  - Всё
*/

/* Версия 1.8
  - Полностью удалены графики и пункты меню их отображения; удалены неиспользуемые массивы и таймеры
  - Удалены высота над уровнем моря и индикатор питания/заряда батареи
  - Меню и все экранные строки переведены на английский; русские символы и WEEK_LANG удалены
  - Новая логика кнопки: одиночное нажатие меняет значение, двойное/тройное — вход в настройки
  - Калибровка нуля CO2 (350-500 ppm) с датой последней калибровки в EEPROM
  - CO2: прочерки "---" при прогреве, ошибке или перегрузе датчика
  - Улучшено расположение показаний на главном экране (T/H, отступы, крупная температура)
  - Добавлены экранные сообщения после сохранения настроек и калибровки CO2
  - Mhz19 2.0.2: убрана задержка прогрева, ожидание ответа 200 мс (патч в libraries/Mhz19)
  - Итог: flash 24706 байт (80%), RAM 1086 байт, свободно 962 байта
*/

// ------------------------- НАСТРОЙКИ --------------------
#define RESET_CLOCK 0     // сброс часов на время загрузки прошивки (для модуля с несъёмной батарейкой). Не забудь поставить 0 и прошить ещё раз!
#define SENS_TIME 10000   // время обновления показаний сенсоров на экране, миллисекунд
#define LED_MODE 0        // тип RGB светодиода: 0 - главный катод, 1 - главный анод

// управление яркостью
byte LED_BRIGHT = 10;         // при отсутствии сохранения в EEPROM: яркость светодиода СО2 (0 - 10) (коэффициент настраиваемой яркости индикатора по умолчанию, если нет сохранения и не автоматическая регулировка (с)НР)
byte LCD_BRIGHT = 10;         // при отсутствии сохранения в EEPROM: яркость экрана (0 - 10) (коэффициент настраиваемой яркости экрана по умолчанию, если нет сохранения и не автоматическая регулировка (с)НР)

#define BRIGHT_CONTROL 1      // 0/1 - запретить/разрешить управление яркостью (при отключении яркость всегда будет макс.)
#define BRIGHT_THRESHOLD 350  // величина сигнала, ниже которой яркость переключится на минимум (0-1023)
#define LED_BRIGHT_MAX 255    // макс яркость светодиода СО2 (0 - 255)
#define LED_BRIGHT_MIN 10     // мин яркость светодиода СО2 (0 - 255)
#define LCD_BRIGHT_MAX 255    // макс яркость подсветки дисплея (0 - 255)
#define LCD_BRIGHT_MIN 10     // мин яркость подсветки дисплея (0 - 255)

#define DISP_MODE 1       // в правом верхнем углу отображать: 0 - год, 1 - день недели и секунды
#define DEBUG 0           // вывод на дисплей лог инициализации датчиков при запуске. Для дисплея 1602 не работает! Но дублируется через порт!
#define CO2_SENSOR 1      // включить или выключить поддержку/вывод с датчика СО2 (1 вкл, 0 выкл)
#define DISPLAY_TYPE 1    // тип дисплея: 1 - 2004 (большой), 0 - 1602 (маленький)
#define DISPLAY_ADDR 0x27 // адрес платы дисплея: 0x27 или 0x3f. Если дисплей не работает - смени адрес! На самом дисплее адрес не указан

// пределы для индикатора (с)НР
#define normCO2 800       // и выше - желтый
#define maxCO2 1200       // и выше - красный
#define blinkLEDCO2 1500  // Значение СО2, при превышении которого будет мигать индикатор

#define minTemp 21        // и ниже для синего индикатора
#define normTemp 26       // и выше - желтый
#define maxTemp 28        // и выше - красный
#define blinkLEDTemp 35   // Значение температуры, при превышении которой будет мигать индикатор

#define maxHum 90         // и выше - синий
#define normHum 30        // и ниже - желтый
#define minHum 20         // и ниже - красный
#define blinkLEDHum 15    // Значение влажности, при показаниях ниже порога которого будет мигать индикатор

#define normPress 733     // и ниже - желтый
#define minPress 728      // и ниже - красный   // может, переделать на синий?

#define minRain -50       // и ниже - красный   
#define normRain -20      // и ниже - желтый
#define maxRain 50        // и выше - синий


byte LEDType = 0;         //  при отсутствии сохранения в EEPROM: привязка индикатора к датчикам: 0 - СО2, 1 - Влажность, 2 - Температура, 3 - Осадки

#include <EEPROM.h>


/* 1 - для графика СО2 часового, 2 - для графика СО2 суточного (с)НР
   4 - для графика влажности часовой, 8 - для графика влажности суточной (с)НР
   16 - для графика температуры часовой, 32 - для графика температуры суточной (с)НР
   64 - для графика давления/дождя часового, 128 - для графика давления/дождя суточного (с)НР
   256 - для графика высоты часового, 512 - для графика высоты суточного (с)НР
   для выборочных графиков значения нужно сложить (с)НР
   например: для изменения пределов у графиков суточной температуры и суточного СО2 складываем 2 + 32 и устанавливаем значение 34 (можно ставить сумму) (с)НР
*/

// пределы отображения для графиков

// адрес BME280 жёстко задан в файле библиотеки Adafruit_BME280.h
// адрес BME280: у стоковых модулей 0x77, у китайских 0x76.
// Если температура/влажность/давление показывают 0 - смените адрес! (с)НР
#define BME280_ADDR 0x76

// если дисплей не заводится - поменяйте адрес (строка 54)

// пины
#define BACKLIGHT 10
#define PHOTO A3

#define MHZ_RX 2
#define MHZ_TX 3

#define LED_COM 7
#define LED_R 9
#define LED_G 6
#define LED_B 5
#define BTN_PIN 4

// библиотеки
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

#if (DISPLAY_TYPE == 1)
LiquidCrystal_I2C lcd(DISPLAY_ADDR, 20, 4);
#else
LiquidCrystal_I2C lcd(DISPLAY_ADDR, 16, 2);
#endif

#include "RTClib.h"
RTC_DS3231 rtc;
DateTime now;

#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>
#define TEMP_CORRECTION -0.6        // поправка температуры (°C): показания станции - среднее эталонных термометров
Adafruit_BME280 bme;

#if (CO2_SENSOR == 1)
#include <SoftwareSerial.h>
#include <Mhz19.h>
SoftwareSerial co2Serial(MHZ_RX, MHZ_TX);   // RX = D2 (sensor Tx), TX = D3 (sensor Rx)
Mhz19 mhz19;
#endif

unsigned long sensorsTimer = SENS_TIME;
unsigned long drawSensorsTimer = SENS_TIME;
unsigned long clockTimer = 500;

#if (DISPLAY_TYPE == 1)           // для дисплея 2004 график "длиннее", поэтому интервалы времени на сегмент короче (с)НР
#else
#endif

unsigned long predictTimer = ((long)10 * 60 * 1000);        // 10 минут
unsigned long brightTimer = (2000);

unsigned long sensorsTimerD = 0;
unsigned long drawSensorsTimerD = 0;
unsigned long clockTimerD = 0;
unsigned long predictTimerD = 0;
unsigned long brightTimerD = 0;

#include "GyverButton.h"
GButton button(BTN_PIN, LOW_PULL, NORM_OPEN);

int8_t hrs, mins, secs;
byte mode = 0;
/*
  0 часы и данные
  1 график углекислого за час
  2 график углекислого за сутки
  3 график влажности за час
  4 график влажности за сутки
  5 график температуры за час
  6 график температуры за сутки
  7 график дождя/давления за час
  8 график дождя/давления за сутки
  9 график высоты за час
  10 график высоты за сутки
*/

byte podMode = 1; // подрежим меню(с)НР
byte mode0scr = 0;
/* (с)НР
  0 - Крупно время
  1 - Крупно содержание СО2
  2 - Крупно температура
  3 - Крупно давление
  4 - Крупно влажность
  5 - Крупно высота
*/
boolean bigDig = false;   // true - цифры на главном экране на все 4 строки (для LCD 2004) (с)НР

// переменные для вывода
float dispTemp;
byte dispHum;
int dispPres;
int dispCO2 = -1;
boolean co2Gate = true;              // true = показывать "---" (прогрев/ошибка/перегрузка)
#define CO2_WARMUP_MS 300000UL       // 5 минут прогрева после включения
int dispRain;

// массивы графиков (byte + коэффициент, чтобы показания влезли в байт) (с)НР
int delta;
uint32_t pressure_array[6];
uint32_t sumX, sumY, sumX2, sumXY;
float a, b;
//byte time_array[6];

/*
  Характеристики датчика BME:
  Температура: от-40 до + 85 °C
  Влажность: 0-100%
  Давление: 300-1100 hPa (225-825 ммРтСт)
  Разрешение:
  Температура: 0,01 °C
  Влажность: 0.008%
  Давление: 0,18 Pa
  Точность:
  Температура: +-1 °C
  Влажность: +-3%
  Давление: +-1 Па
*/

// символы
// график
byte row5[8] = {0b00000,  0b00000,  0b00000,  0b11111,  0b11111,  0b11111,  0b11111,  0b11111};   // в т.ч. для четырехстрочных цифр 2, 3, 4, 5, 6, 8, 9, 0 (с)НР
byte row3[8] = {0b00000,  0b00000,  0b00000,  0b00000,  0b00000,  0b11111,  0b11111,  0b11111};   // в т.ч. для двустрочной цифры 0, для четырехстрочных цифр 2, 3, 4, 5, 6, 8, 9 (с)НР
byte row2[8] = {0b00000,  0b00000,  0b00000,  0b00000,  0b00000,  0b00000,  0b11111,  0b11111};   // в т.ч. для двустрочной цифры 4 (с)НР

// цифры //  (с)НР
uint8_t UB[8] = {0b11111,  0b11111,  0b11111,  0b00000,  0b00000,  0b00000,  0b00000,  0b00000};   // для двустрочных 7, 0   // для четырехстрочных 2, 3, 4, 5, 6, 8, 9
uint8_t UMB[8] = {0b11111,  0b11111,  0b11111,  0b00000,  0b00000,  0b00000,  0b11111,  0b11111};  // для двустрочных 2, 3, 5, 6, 8, 9
uint8_t LMB[8] = {0b11111,  0b00000,  0b00000,  0b00000,  0b00000,  0b11111,  0b11111,  0b11111};  // для двустрочных 2, 3, 5, 6, 8, 9
uint8_t LM2[8] = {0b11111,  0b00000,  0b00000,  0b00000,  0b00000,  0b00000,  0b00000,  0b00000};  // для двустрочной 4
uint8_t UT[8] = {0b11111,  0b11111,  0b11111,  0b11111,  0b11111,  0b00000,  0b00000,  0b00000};   // для четырехстрочных 2, 3, 4, 5, 6, 7, 8, 9, 0

uint8_t KU[8] = {0b00000,  0b00000,  0b00000,  0b00001,  0b00010,  0b00100,  0b01000,  0b10000};   // для верхней части %
uint8_t KD[8] = {0b00001,  0b00010,  0b00100,  0b01000,  0b10000,  0b00000,  0b00000,  0b00000};   // для нижней части %

// русские буквы (с)НР

void digSeg(byte x, byte y, byte z1, byte z2, byte z3, byte z4, byte z5, byte z6) {   // отображение двух строк по три символа с указанием кодов символов (с)НР
  lcd.setCursor(x, y);
  lcd.write(z1); lcd.write(z2); lcd.write(z3);
  if (x <= 11) lcd.print(" ");
  lcd.setCursor(x, y + 1);
  lcd.write(z4); lcd.write(z5); lcd.write(z6);
  if (x <= 11) lcd.print(" ");
}

void drawDig(byte dig, byte x, byte y) {        // рисуем цифры (с)НР ---------------------------------------
  if (bigDig && DISPLAY_TYPE == 1) {
    switch (dig) {            // четырехстрочные цифры (с)НР
      case 0:
        digSeg(x, y, 255, 0, 255, 255, 32, 255);
        digSeg(x, y + 2, 255, 32, 255, 255, 3, 255);
        break;
      case 1:
        digSeg(x, y, 32, 255, 32, 32, 255, 32);
        digSeg(x, y + 2, 32, 255, 32, 32, 255, 32);
        break;
      case 2:
        digSeg(x, y, 0, 0, 255, 1, 1, 255);
        digSeg(x, y + 2, 255, 2, 2, 255, 3, 3);
        break;
      case 3:
        digSeg(x, y, 0, 0, 255, 1, 1, 255);
        digSeg(x, y + 2, 2, 2, 255, 3, 3, 255);
        break;
      case 4:
        digSeg(x, y, 255, 32, 255, 255, 1, 255);
        digSeg(x, y + 2, 2, 2, 255, 32, 32, 255);
        break;
      case 5:
        digSeg(x, y, 255, 0, 0, 255, 1, 1);
        digSeg(x, y + 2, 2, 2, 255, 3, 3, 255);
        break;
      case 6:
        digSeg(x, y, 255, 0, 0, 255, 1, 1);
        digSeg(x, y + 2, 255, 2, 255, 255, 3, 255);
        break;
      case 7:
        digSeg(x, y, 0, 0, 255, 32, 32, 255);
        digSeg(x, y + 2, 32, 255, 32, 32, 255, 32);
        break;
      case 8:
        digSeg(x, y, 255, 0, 255, 255, 1, 255);
        digSeg(x, y + 2, 255, 2, 255, 255, 3, 255);
        break;
      case 9:
        digSeg(x, y, 255, 0, 255, 255, 1, 255);
        digSeg(x, y + 2, 2, 2, 255, 3, 3, 255);
        break;
      case 10:
        digSeg(x, y, 32, 32, 32, 32, 32, 32);
        digSeg(x, y + 2, 32, 32, 32, 32, 32, 32);
        break;
    }
  }
  else {
    switch (dig) {            // двухстрочные цифры
      case 0:
        digSeg(x, y, 255, 1, 255, 255, 2, 255);
        break;
      case 1:
        digSeg(x, y, 32, 255, 32, 32, 255, 32);
        break;
      case 2:
        digSeg(x, y, 3, 3, 255, 255, 4, 4);
        break;
      case 3:
        digSeg(x, y, 3, 3, 255, 4, 4, 255);
        break;
      case 4:
        digSeg(x, y, 255, 0, 255, 5, 5, 255);
        break;
      case 5:
        digSeg(x, y, 255, 3, 3, 4, 4, 255);
        break;
      case 6:
        digSeg(x, y, 255, 3, 3, 255, 4, 255);
        break;
      case 7:
        digSeg(x, y, 1, 1, 255, 32, 255, 32);
        break;
      case 8:
        digSeg(x, y, 255, 3, 255, 255, 4, 255);
        break;
      case 9:
        digSeg(x, y, 255, 3, 255, 4, 4, 255);
        break;
      case 10:
        digSeg(x, y, 32, 32, 32, 32, 32, 32);
        break;
    }
  }
}

void drawPPM(int dispCO2, byte x, byte y) {     // Уровень СО2 крупно на главном экране (с)НР ----------------------------
  if (co2Gate) {
    lcd.setCursor(x, y);
    lcd.print(F("---"));
    lcd.setCursor(15, 0);
#if (DISPLAY_TYPE == 1)
    lcd.print(F("ppm"));
#else
    lcd.print(F("p"));
#endif
    return;
  }
  if (dispCO2 / 1000 == 0) drawDig(10, x, y);
  else drawDig(dispCO2 / 1000, x, y);
  drawDig((dispCO2 % 1000) / 100, x + 4, y);
  drawDig((dispCO2 % 100) / 10, x + 8, y);
  drawDig(dispCO2 % 10 , x + 12, y);
  lcd.setCursor(15, 0);
#if (DISPLAY_TYPE == 1)
  lcd.print("ppm");
#else
  lcd.print("p");
#endif
}

void drawPres(int dispPres, byte x, byte y) {   // Давление крупно на главном экране (с)НР ----------------------------
  drawDig((dispPres % 1000) / 100, x , y);
  drawDig((dispPres % 100) / 10, x + 4, y);
  drawDig(dispPres % 10 , x + 8, y);
  lcd.setCursor(x + 11, 1);
  if (bigDig) lcd.setCursor(x + 11, 3);
  lcd.print("mm");
}

void drawTemp(float dispTemp, byte x, byte y) { // Температура крупно на главном экране (с)НР ----------------------------
  boolean neg = (dispTemp < 0);
  if (neg) dispTemp = -dispTemp;
  byte shift = (neg && dispTemp >= 10) ? 1 : 0;
  if (dispTemp / 10 == 0) {
    if (neg) {
      lcd.setCursor(x, y + 1);
      lcd.write(45);          // знак минус для отрицательной температуры (с)НР
    } else drawDig(10, x, y);
    drawDig(int(dispTemp) % 10, x + 4, y);
  } else {
    if (neg) {
      lcd.setCursor(x, y + 1);
      lcd.write(45);
      drawDig(dispTemp / 10, x + 1, y);
    } else {
      drawDig(dispTemp / 10, x, y);
    }
    drawDig(int(dispTemp) % 10, x + 4 + shift, y);
  }
  drawDig(int(dispTemp * 10.0) % 10, x + 9, y);

  byte px = x + 7 + shift;
  if (bigDig && DISPLAY_TYPE == 1) {
    lcd.setCursor(px, y + 3);
    lcd.write(1);             // десятичная точка
  }
  else {
    lcd.setCursor(px, y + 1);
    lcd.write(0B10100001);    // десятичная точка
  }
  lcd.setCursor(x + 13, y);
  lcd.write(223);             // градусы
}

void drawHum(int dispHum, byte x, byte y) {   // Влажность крупно на главном экране (с)НР ----------------------------
  if (dispHum / 100 == 0) drawDig(10, x, y);
  else drawDig(dispHum / 100, x, y);
  if ((dispHum % 100) / 10 == 0) drawDig(0, x + 4, y);
  else drawDig(dispHum / 10, x + 4, y);
  drawDig(int(dispHum) % 10, x + 8, y);
  if (bigDig && DISPLAY_TYPE == 1) {
    lcd.setCursor(x + 12, y + 1);
    lcd.print("\245\4");
    lcd.setCursor(x + 12, y + 2);
    lcd.print("\5\245");
  }
  else {
    lcd.setCursor(x + 12, y + 1);
    lcd.print("%");
  }
}

void drawClock(byte hours, byte minutes, byte x, byte y) {    // рисуем время крупными цифрами -------------------------------------------
  if (hours > 23 || minutes > 59) return;
  if (hours / 10 == 0) drawDig(10, x, y);
  else drawDig(hours / 10, x, y);
  drawDig(hours % 10, x + 4, y);
  // тут должны быть точки. Отдельной функцией
  drawDig(minutes / 10, x + 8, y);
  drawDig(minutes % 10, x + 12, y);
}

static const char *dayNames[] = {"Su","Mo","Tu","We","Th","Fr","Sa"};


void drawData() {                     // выводим дату -------------------------------------------------------------
  int Y = 0;
  if (DISPLAY_TYPE == 1 && mode0scr == 1) Y = 2;
  if (!bigDig) {              // если 4-х строчные цифры, то дату, день недели (и секунды) не пишем - некуда (с)НР
    lcd.setCursor(15, 0 + Y);
    if (now.day() < 10) lcd.print(0);
    lcd.print(now.day());
    lcd.print(".");
    if (now.month() < 10) lcd.print(0);
    lcd.print(now.month());

    if (DISP_MODE == 0) {
      lcd.setCursor(16, 1);
      lcd.print(now.year());
    } else {
      loadClock();              // принудительно обновляем знаки, т.к. есть жалобы на необновление знаков в днях недели (с)НР
      lcd.setCursor(15, 1);
      if (secs < 10) lcd.print('0');
      lcd.print(secs);
      lcd.setCursor(18, 1);
      int dayofweek = now.dayOfTheWeek();
      lcd.print(dayNames[dayofweek]);
      // if (hrs == 0 && mins == 0 && secs <= 1) loadClock();   // Обновляем знаки, чтобы русские буквы в днях недели тоже обновились. (с)НР
    }
  }
}

void printValRight(int val, byte field) {  // вывод значения с выравниванием по правому краю поля (с)НР
  boolean neg = val < 0;
  int v = abs(val);
  byte digits = 1;
  if (v >= 1000) digits = 4;
  else if (v >= 100) digits = 3;
  else if (v >= 10) digits = 2;
  if (neg) digits++;
  while (digits < field) {
    lcd.print(" ");
    digits++;
  }
  lcd.print(val);
}

void printTemp(float t) {          // вывод температуры с одним знаком после запятой без String (с)НР
  lcd.print((int)t);
  lcd.print(".");
  int tenths = (int)(t * 10.0 + (t < 0 ? -0.5 : 0.5));
  if (tenths < 0) tenths = -tenths;
  lcd.print(tenths % 10);
}

void loadClock() {
  if (bigDig && (DISPLAY_TYPE == 1)) {              // для четырехстрочных цифр (с)НР
    lcd.createChar(0, UT);
    lcd.createChar(1, row3);
    lcd.createChar(2, UB);
    lcd.createChar(3, row5);
    lcd.createChar(4, KU);
    lcd.createChar(5, KD);
  }
  else {                                            // для двустрочных цифр (с)НР
    lcd.createChar(0, row2);
    lcd.createChar(1, UB);
    lcd.createChar(2, row3);
    lcd.createChar(3, UMB);
    lcd.createChar(4, LMB);
    lcd.createChar(5, LM2);
  }


}

#if (LED_MODE == 0)
byte LED_ON = (LED_BRIGHT_MAX);
byte LED_OFF = (0);
#else
byte LED_ON = (255 - LED_BRIGHT_MAX);
byte LED_OFF = (255);
#endif

void setLEDcolor(byte color) {                    // цвет индикатора задается двумя битами на каждый цвет (с)НР
#if (LED_MODE == 0)
  // главный катод: 0 - выключен, LED_ON - полная яркость (с)НР
  analogWrite(LED_R, LED_ON * (color & 3) / 3);
  analogWrite(LED_G, LED_ON * ((color & 12) >> 2) / 3);
  analogWrite(LED_B, LED_ON * ((color & 48) >> 4) / 3);
#else
  // главный анод: 0 - полная яркость, 255 - выключен (с)НР
  analogWrite(LED_R, LED_ON + (255 - LED_ON) * (3 - (color & 3)) / 3);
  analogWrite(LED_G, LED_ON + (255 - LED_ON) * (3 - ((color & 12) >> 2)) / 3);
  analogWrite(LED_B, LED_ON + (255 - LED_ON) * (3 - ((color & 48) >> 4)) / 3);
#endif
}

void setLED() {

#if (BRIGHT_CONTROL == 1)
  if (LED_BRIGHT < 11) {                                     // если ручные установки яркости
    LED_ON = map(LED_BRIGHT, 0, 10, 0, LED_BRIGHT_MAX);
  } else {
    checkBrightness();
  }
#else
  LED_ON = LED_BRIGHT_MAX;                                   // управление яркостью отключено - всегда максимум
#endif
  if (LED_MODE != 0) LED_ON = 255 - LED_ON;

  // Во время прогрева/ошибки/перегруза CO2 не показываем красный/жёлтый индикатор
  if (co2Gate && LEDType == 0) {
    setLEDcolor(12);   // зелёный
    return;
  }

  // ниже задается цвет индикатора в зависимости от назначенного сенсора: красный, желтый, зеленый, синий (с)НР

  if ((dispCO2 >= maxCO2) && LEDType == 0 || (dispHum <= minHum) && LEDType == 1 || (dispTemp >= maxTemp) && LEDType == 2 || (dispRain <= minRain) && LEDType == 3 || (dispPres <= minPress) && LEDType == 4) setLEDcolor(3);   // красный
  else if ((dispCO2 >= normCO2) && LEDType == 0 || (dispHum <= normHum) && LEDType == 1 || (dispTemp >= normTemp) && LEDType == 2 || (dispRain <= normRain) && LEDType == 3 || (dispPres <= normPress) && LEDType == 4) setLEDcolor(3 + 8);   // желтый
  else if (LEDType == 0 || (dispHum <= maxHum) && LEDType == 1 || (dispTemp >= minTemp) && LEDType == 2 || (dispRain <= maxRain) && LEDType == 3 || LEDType == 4) setLEDcolor(12);    // зеленый
  else setLEDcolor(48);   // синий (если влажность превышает заданный максимум, температура ниже минимума, вероятность осадков выше maxRain)
}

boolean readSerialLine(char* buf, byte size) {   // неблокирующее чтение строки из порта; true - строка получена (с)НР
  static byte idx = 0;
  while (Serial.available()) {
    char c = Serial.read();
    if (c == '\n' || c == '\r') {
      if (idx > 0) {
        buf[idx] = 0;
        idx = 0;
        return true;
      }
    } else if (idx < size - 1) {
      buf[idx++] = c;
    }
  }
  return false;
}

boolean setTimeSerial(char* str) {     // разбор команды установки времени с ПК: TYYYY-MM-DD HH:MM:SS (с)НР
  char* p = str;
  if (*p == 'T' || *p == 't') p++;
  if (strlen(p) < 19) return false;
  if (p[4] != '-' || p[7] != '-' || p[10] != ' ' || p[13] != ':' || p[16] != ':') return false;
  for (byte i = 0; i < 19; i++) {
    if (i == 4 || i == 7 || i == 10 || i == 13 || i == 16) continue;
    if (p[i] < '0' || p[i] > '9') return false;
  }
  uint16_t y = (p[0] - '0') * 1000 + (p[1] - '0') * 100 + (p[2] - '0') * 10 + (p[3] - '0');
  uint8_t mo = (p[5] - '0') * 10 + (p[6] - '0');
  uint8_t d = (p[8] - '0') * 10 + (p[9] - '0');
  uint8_t h = (p[11] - '0') * 10 + (p[12] - '0');
  uint8_t mi = (p[14] - '0') * 10 + (p[15] - '0');
  uint8_t s = (p[17] - '0') * 10 + (p[18] - '0');
  if (y < 2000 || y > 2099 || mo < 1 || mo > 12 || d < 1 || d > 31 || h > 23 || mi > 59 || s > 59) return false;
  rtc.adjust(DateTime(y, mo, d, h, mi, s));
  now = rtc.now();
  secs = now.second();
  mins = now.minute();
  hrs = now.hour();
  Serial.println(F("TIME SET"));
  return true;
}

void serialTick() {                     // проверка команд из порта в рабочем режиме (с)НР
  char buf[24];
  if (readSerialLine(buf, sizeof(buf))) setTimeSerial(buf);
}

void setup() {
  Serial.begin(9600);

  rtc.begin();
  if (RESET_CLOCK) {                    // ждём точное время с ПК до 10 сек, иначе ставим время компиляции (с)НР
    char buf[24];
    unsigned long t = millis();
    boolean got = false;
    while (millis() - t < 10000) {
      if (readSerialLine(buf, sizeof(buf)) && setTimeSerial(buf)) {
        got = true;
        break;
      }
    }
    if (!got) rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  } else if (rtc.lostPower()) {
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }

  pinMode(BACKLIGHT, OUTPUT);
  pinMode(LED_COM, OUTPUT);
  pinMode(LED_R, OUTPUT);
  pinMode(LED_G, OUTPUT);
  pinMode(LED_B, OUTPUT);
  setLEDcolor(0);

  digitalWrite(LED_COM, LED_MODE);
  analogWrite(BACKLIGHT, LCD_BRIGHT_MAX);

  if (EEPROM.read(0) == 124) {      // если было сохранение настроек, то восстанавливаем их (с)НР
    mode0scr = EEPROM.read(6);
    bigDig = EEPROM.read(7);
    LED_BRIGHT = EEPROM.read(8);
    LCD_BRIGHT = EEPROM.read(9);
    LEDType = EEPROM.read(10);
  }

  lcd.init();
  lcd.backlight();
  lcd.clear();
  lcd.setCursor(0, 0); lcd.print(F("  MeteoClock"));
  lcd.setCursor(0, 1); lcd.print(F("     v1.8"));
  delay(1200);
  lcd.clear();

#if (DEBUG == 1 && DISPLAY_TYPE == 1)
  boolean status = true;

  setLEDcolor(3);

#if (CO2_SENSOR == 1)
  lcd.setCursor(0, 0);
  lcd.print(F("MHZ-19... "));
  Serial.print(F("MHZ-19... "));
  co2Serial.begin(9600);
  mhz19.begin(&co2Serial);
  mhz19.disableAutoBaseCalibration();
  int co2ppm = mhz19.getCarbonDioxide();
  delay(500);
  co2ppm = mhz19.getCarbonDioxide();
  if (co2ppm >= 0) {
    lcd.print(F("OK"));
    Serial.println(F("OK"));
  } else {
    lcd.print(F("ERROR"));
    Serial.println(F("ERROR"));
    status = false;
  }
#endif

  setLEDcolor(3 + 12);
  lcd.setCursor(0, 1);
  lcd.print(F("RTC... "));
  Serial.print(F("RTC... "));
  delay(50);
  if (rtc.begin()) {
    lcd.print(F("OK"));
    Serial.println(F("OK"));
  } else {
    lcd.print(F("ERROR"));
    Serial.println(F("ERROR"));
    status = false;
  }

  setLEDcolor(12);
  lcd.setCursor(0, 2);
  lcd.print(F("BME280... "));
  Serial.print(F("BME280... "));
  delay(50);
  if (bme.begin(BME280_ADDR, &Wire)) {
    lcd.print(F("OK"));
    Serial.println(F("OK"));
  } else {
    lcd.print(F("ERROR"));
    Serial.println(F("ERROR"));
    status = false;
  }

  setLEDcolor(0);
  lcd.setCursor(0, 3);
  if (status) {
    lcd.print(F("All good"));
    Serial.println(F("All good"));
  } else {
    lcd.print(F("Check wires!"));
    Serial.println(F("Check wires!"));
  }

  for (byte i = 1; i < 20; i++) { // убрал бесконечный цикл, сделал 5-ти секундное ожидание (с)НР
    lcd.setCursor(14, 1);
    lcd.print("P:    ");
    lcd.setCursor(16, 1);
    lcd.print(analogRead(PHOTO), 1);
    Serial.println(analogRead(PHOTO));
    delay(250);
  }
#else

#if (CO2_SENSOR == 1)
  co2Serial.begin(9600);
  mhz19.begin(&co2Serial);
  mhz19.disableAutoBaseCalibration();
#endif
  rtc.begin();
  bme.begin(BME280_ADDR, &Wire);
#endif

  bme.setSampling(Adafruit_BME280::MODE_FORCED,
                  Adafruit_BME280::SAMPLING_X1, // temperature
                  Adafruit_BME280::SAMPLING_X1, // pressure
                  Adafruit_BME280::SAMPLING_X1, // humidity
                  Adafruit_BME280::FILTER_OFF);

  if (RESET_CLOCK) rtc.adjust(DateTime(F(__DATE__), F(__TIME__))); // подстраховка, если по какой-то причине время не было установлено в начале setup() (с)НР
  lcd.clear();
  now = rtc.now();
  secs = now.second();
  mins = now.minute();
  hrs = now.hour();

  bme.takeForcedMeasurement();
  uint32_t Pressure = bme.readPressure();
  for (byte i = 0; i < 6; i++) {          // счётчик от 0 до 5
    pressure_array[i] = Pressure; // забить весь массив текущим давлением
    //time_array[i] = i;                    // забить массив времени числами 0 - 5
  }


  // заполняем графики текущим значением
  readSensors();

  if (DISPLAY_TYPE == 1) drawData();
  loadClock();
  // readSensors();
  drawSensors();
}

void loop() {
  serialTick();                                       // приём команд установки времени с ПК (с)НР
  if (testTimer(brightTimerD, brightTimer)) checkBrightness();  // яркость
  if (testTimer(sensorsTimerD, sensorsTimer)) readSensors();    // читаем показания датчиков с периодом SENS_TIME
#if (DEBUG == 1)
  Serial.println(dispTemp);
#endif

  if (testTimer(clockTimerD, clockTimer)) clockTick();          // два раза в секунду пересчитываем время и мигаем точками
  plotSensorsTick();                                // тут внутри несколько таймеров для пересчёта графиков (за час, за день и прогноз)
  modesTick();                                      // тут ловим нажатия на кнопку и переключаем режимы
  if (mode == 0) {                                  // в режиме "главного экрана"
    if (testTimer(drawSensorsTimerD, drawSensorsTimer)) drawSensors();  // обновляем показания датчиков на дисплее с периодом SENS_TIME
  }
}
