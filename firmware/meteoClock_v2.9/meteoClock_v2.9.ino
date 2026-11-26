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
  Время хранится в модуле реального времени DS3231. Батарейка RTC продолжает
  отсчёт после выключения питания; если она разрядилась, RTC сообщает об этом
  флагом OSF. В этом случае прошивка НЕ подставляет время компиляции: на экране
  появляется SET TIME, и часы ждут синхронизации с ПК.
  Для точной синхронизации проще всего запустить ./set_time.sh, либо отправить
  в последовательный порт (9600) строку вида:
    T2026-08-17 14:23:45
  Пример для Linux (без перепрошивки, в любой момент):
    stty -F /dev/ttyUSB0 9600 raw -echo; printf 'T%s\n' "$(date '+%Y-%m-%d %H:%M:%S')" > /dev/ttyUSB0
  Команда работает и в рабочем режиме, и сразу после включения.
  Принудительная установка времени компиляции:
  - Ставим RESET_CLOCK 1
  - Прошиваемся
  - Если время с ПК не пришло за 10 секунд, в RTC запишется время компиляции
  - Возвращаем RESET_CLOCK 0 и прошиваемся ещё раз
*/

/* Версия 2.9
  - Добавлен сигнал 18 — Nokia tune (~5 секунд)
  - Chime 4 (Für Elise) и 17 (third) удалены; оставшиеся перенумерованы 0..17
  - Мелодии 2 (Happy Birthday) и 3 (Amazing Grace) удлинены до 8 секунд
  - Остальные мелодии длятся около 5 секунд
  - Chime: режим Random — каждый час случайный вариант из 19
  - Названия сигналов заменены номерами 0..18; используются 66 частот
    (1000–10000 Гц), разные ритмы и регистры
  - На экране запуска показываются дата и время компиляции прошивки
  - Поправка температуры сброшена в 0; сохранённое в EEPROM значение
    температуры больше не читается
  - Alarm SET применяет время и возвращается в меню настроек (не на главный
    экран); удержание на поле SET делает то же самое
  - При запуске играет короткая мелодия (скрытый вариант, не в меню;
    в сборке RESET_CLOCK 1 отключена для экономии flash)
  - В меню у пунктов Alarm и Chime есть иконки (колокольчик и нота)
  - В обычной прошивке экраны настроек и редакторы сами возвращаются на
    главный экран после 2 минут без нажатий (в сборке RESET_CLOCK 1 таймаут
    отключён)
  - Остальное — как в v2.3
  - Меню переупорядочено: Alarm — 1, Chime — 2, далее Save/Exit,
    яркость, LED Mode, CO2 Calibration, Altitude — 9
  - Flash 30 566 байт (99%), RAM 916 байт (для 2004)

  Версия 2.3
  - Редактор будильника: вход сразу в EDIT; 1x меняет поле, удержание — следующее
    поле; 2x/3x игнорируются; поле SET применяет и выходит на главный экран
  - Пункты Temp Offset / Hum Offset удалены из меню; поправки T/H по-прежнему
    применяются из EEPROM (значения по умолчанию заданы в скетче)
  - Chime: 51 частота (1000–7000 Гц), в каждом из 21 варианта 6–8 разных
    тонов; звучание более плавное. Удержание кнопки проигрывает выбранный
    сигнал, применяет его и выходит в меню настроек
  - CO2 Calibration в меню (на 1602 — CO2 Calib)
  - Flash 30 124 байта (98%), RAM 896 байт (для 2004)

  Версия 2.2
  - Пункт меню Chime объединяет включение/выключение и выбор варианта
    почасового сигнала: OFF или один из 21 варианта
  - При выборе звука сразу играет превью; выбор сохраняется в EEPROM
  - OFF отключает только почасовой сигнал, будильник остаётся независимым
  - Добавлены частоты 1600/1800/1500/1200/1000 Гц для вариантов сигнала
  - RGB-индикатор мигает синхронно с секундными точками на главном экране
  - Цвет индикатора сразу обновляется при смене LED Mode и показаний датчика
  - Flash 30 112 байт (98%), RAM 897 байт (для 2004)

  Версия 2.1
  - Табличное рисование крупных цифр: сегменты вынесены в PROGMEM
  - Названия пунктов меню и дни недели хранятся в PROGMEM
  - Одиночные символы выводятся как char, а не как строки
  - Функциональность и экраны v2.0 сохранены

  Версия 2.0
  - Точка росы и индекс жары удалены для экономии flash (~1 КБ)
  - Добавлены суточные минимумы/максимумы T/H/P/CO2 с автосбросом в полночь
  - Добавлено давление, приведённое к уровню моря (QNH), с настройкой высоты
  - Поправки температуры и влажности вынесены в меню (EEPROM), без перепрошивки
  - Добавлены двухтональный будильник и почасовой сигнал «та-дам» (зуммер D8);
    во время звука RGB-индикатор мигает синим, будильник автостоп через 60 с
  - Экран статистики: T/H на 1602, P/CO2 на 1602; на 2004 все четыре строки
  - Исправлено: drawPres больше не пишет в несуществующую строку 3 на 1602
  - Исправлен вывод отрицательных температур от -1 до 0 °C
  - В меню показывается текущее значение пункта.
  - Меню: 1x — следующий пункт, 2x — предыдущий, удержание — вход.
  - Редактор: 1x/2x — +/−, удержание — автоповтор с ускорением, значения
    зациклены, 3x — назад.
  - Для экономии flash удалены прогноз осадков и дата калибровки CO2
  - Оформление: меню с маркером `>`, позицией `n/11` и значениями пунктов;
    выровненные показания и подписи, шкала яркости, подсказки редакторов;
    экран QNH с давлением/высотой, LED Mode с текущим показанием
  - Экраны калибровки обновляются без полной очистки (без мерцания)
  - Локальная библиотека BME280 переведена в I2C-only: освобождено ~1 КБ flash
  - Итог: см. README (flash/RAM)

  Версия 1.9
  - При потере питания RTC (села батарейка) время компиляции больше не
    подставляется: на экране отображается SET TIME до синхронизации с ПК
  - В мелких часах секунды всегда выводятся двумя цифрами (05, а не " 5")
  - Поправка температуры применяется драйвером BME280, поэтому давление и
    влажность рассчитываются по скорректированной температуре
  - set_time.sh ждёт READY до 12 секунд (учтено сообщение о потере питания RTC)
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
  - Калибровка CO2 выполняется только после подтверждения (двойное нажатие),
    чтобы её нельзя было запустить случайно
  - Исправлена установка времени: время компиляции больше не перезаписывает
    точное время, полученное с ПК (раньше часы отставали на 10-30 секунд)
  - Часы сверяются с RTC раз в минуту, а не раз в час: экран больше не
    накапливает отставание в несколько секунд из-за задержек цикла
  - Прошивка печатает READY, когда готова принять время; set_time.sh ждёт
    этот сигнал и отправляет метку один раз, не откатывая часы назад
  - После включения/перезагрузки главный экран по умолчанию показывает
    часы крупными цифрами (START_BIG_CLOCK в настройках)
  - Mhz19 2.0.2: убрана задержка прогрева, ожидание ответа 200 мс (патч в libraries/Mhz19)
  - Итог: flash 24706 байт (80%), RAM 1086 байт, свободно 962 байта
*/

// ------------------------- НАСТРОЙКИ --------------------
#define RESET_CLOCK 0     // 1 помещается (~29.8 КБ), но после установки времени верните 0
#define SENS_TIME 10000   // время обновления показаний сенсоров на экране, миллисекунд
#define LED_MODE 0        // тип RGB светодиода: 0 - главный катод, 1 - главный анод
#define START_BIG_CLOCK 1 // после включения/перезагрузки показывать часы крупными цифрами

// -------- будильник / почасовой сигнал --------
#define BUZZER_PIN 8          // свободный пин D8; см. README, раздел "Buzzer"
#define BUZZER_PORT PORTB     // D8 = PB0 на ATmega328P (для пассивного пьезо)
#define BUZZER_BIT PB0
#define BUZZER_ACTIVE 0       // 0 - пассивный пьезо (таймер; в покое D8 = high-Z)
                              // 1 - активный, включается HIGH, 2 - активный, включается LOW
#define BUZZER_BRIDGE 0       // 1 - пьезо между D8 и D11 (противофаза, ~+6 дБ); 0 - D8 и GND
#define BUZZER_BRIDGE_PIN 11  // D11 = PB3; не использовать вместе с SPI
#define BUZZER_BRIDGE_PORT PORTB
#define BUZZER_BRIDGE_BIT PB3
#define BUZZER_FREQ_IDX 5     // 5 = 4000 Гц (резонанс пьезо); меню частоты удалено для flash
#define BUZZER_NOTE_COUNT 71  // 7 штатных + 64 дополнительных частот для сигналов
#define ALARM_MAX_MS 60000UL  // автоотключение будильника, мс
#define ALARM_STEP_MS 220     // длительность одной фазы двухтонального будильника
#define TOUCH_BEEP 1          // 1 - короткий писк при нажатии кнопки
#define TOUCH_BEEP_MS 40      // длительность писка, мс

// -------- значения по умолчанию (используются, если в EEPROM ничего нет) --------
#define TEMP_CORRECTION 0    // поправка температуры, °C (0 = без поправки; EEPROM для неё не используется)
#define HUMIDITY_CORRECTION 0 // поправка влажности, %
#define ALTITUDE_DEFAULT 0    // высота над уровнем моря для QNH, м
#define ALARM_HOUR_DEFAULT 7
#define ALARM_MIN_DEFAULT 0
#define CHIME_SOUND_DEFAULT 1    // 1 = "дин-дон" (вариант A)
#define CHIME_MODE_COUNT 19      // число вариантов в таблице chimePatterns
#define CHIME_RANDOM CHIME_MODE_COUNT  // псевдоиндекс режима Random
#define STARTUP_CHIME CHIME_MODE_COUNT // скрытый вариант для мелодии запуска

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
                          // ВНИМАНИЕ: DEBUG 1 не помещается в 30 720 байт (Nano); для отладки используйте Uno или оставьте 0
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


byte LEDType = 0;         //  при отсутствии сохранения в EEPROM: 0 - СО2, 1 - Влажность, 2 - Температура, 3 - Давление

#include <EEPROM.h>
#include <math.h>

// EEPROM layout (v2.0+; addresses 0..13 совместимы с v1.9)
#define EE_MAGIC 0
#define EE_MODE0 6
#define EE_BIGDIG 7
#define EE_LED_BRIGHT 8
#define EE_LCD_BRIGHT 9
#define EE_LED_TYPE 10
#define EE_TEMP_OFF 14        // int8: десятые °C, -50..50
#define EE_HUM_OFF 15         // int8: %, -20..20
#define EE_ALTITUDE 16        // uint16: метры, 0..3000 (16,17)
#define EE_ALARM_HOUR 18
#define EE_ALARM_MIN 19
#define EE_FLAGS 20           // bit0 - будильник, bit1 - почасовой сигнал
#define EE_VERSION 21
#define EE_CHIME_SOUND 22     // выбранный вариант почасового сигнала (0..20; 21 = Random)
#define EE_MAGIC_VALUE 124
#define EE_VERSION_VALUE 3
#define FLAG_ALARM 1
#define FLAG_CHIME 2

// Настройки, сохраняемые в EEPROM. int16_t выбран специально: все они
// редактируются одним универсальным редактором через указатель.
int16_t tempOffset = (int16_t)(TEMP_CORRECTION * 10); // десятые °C
int16_t humOffset = HUMIDITY_CORRECTION;              // %
int16_t altitude = ALTITUDE_DEFAULT;                  // м
int16_t alarmHour = ALARM_HOUR_DEFAULT;
int16_t alarmMin = ALARM_MIN_DEFAULT;
byte alarmEditH = 0, alarmEditM = 0;             // временные часы/минуты редактора Alarm
byte alarmField = 0;                             // 0=часы, 1=минуты, 2=ON/OFF, 3=SET
int16_t alarmEnabled = 0;
int16_t chimeEnabled = 0;
int16_t chimeSound = CHIME_SOUND_DEFAULT;        // выбранный вариант сигнала (0..18 или CHIME_RANDOM)
int16_t chimeSel = 0;                            // редактор Chime: 0=OFF, 1..19 = сигнал 0..18, 20 = Random
uint16_t rngState = 0xACE1;                      // состояние генератора для Chime → Random
byte buzzerPlayIdx = BUZZER_FREQ_IDX;            // текущая нота (будильник/сигнал)

// Универсальный редактор пунктов меню 7..9
int16_t *editTarget = NULL;
int16_t editMin = 0, editMax = 0, editStep = 1, editBigStep = 1;
int8_t editDirection = 1;       // +1: 1x, -1: 2x; hold repeats this direction
byte editRepeatCount = 0;


// адрес BME280: у стоковых модулей 0x77, у китайских 0x76.
// Если температура/влажность/давление показывают 0 — смените BME280_ADDR.
#define BME280_ADDR 0x76

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
// Локальная копия библиотеки собрана в I2C-only режиме (экономия ~1 КБ flash).
#include <Adafruit_BME280.h>
// Поправка температуры BME280 (°C) хранится в EEPROM и применяется постоянно
// (в меню пункта нет). Отрицательная — если станция завышает.
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
unsigned long brightTimer = (2000);

unsigned long sensorsTimerD = 0;
unsigned long drawSensorsTimerD = 0;
unsigned long clockTimerD = 0;
unsigned long brightTimerD = 0;
unsigned long menuActivity = 0;          // последнее нажатие в настройках

#include "GyverButton.h"
GButton button(BTN_PIN, LOW_PULL, NORM_OPEN);

int8_t hrs, mins, secs;
byte mode = 0;
boolean timeValid = true;   // false после потери питания RTC, пока время не выставлено с ПК
boolean lcdReady = false;   // LCD инициализирован, можно перерисовывать экран из setTimeSerial()
byte podMode = 1; // пункт меню 1..9
byte mode0scr = 0;
/* (с)НР
  0 - Крупно время
  1 - Крупно содержание СО2
  2 - Крупно температура
  3 - Крупно давление
  4 - Крупно влажность
  7 - Суточные min/max (2004: все; 1602: T/H)
  8 - Суточные min/max P/CO2 (только 1602)
  9 - Крупно QNH (давление на уровне моря)
*/
boolean bigDig = false;   // true - цифры на главном экране на все 4 строки (для LCD 2004) (с)НР

// переменные для вывода
float dispTemp;
byte dispHum;
int dispPres;
int dispCO2 = -1;
boolean co2Gate = true;              // true = показывать "---" (прогрев/ошибка/перегрузка)
#define CO2_WARMUP_MS 300000UL       // 5 минут прогрева после включения

// computed weather values
float stationHpa = 0;                // абсолютное давление, hPa
int dispQnh = 0;                     // давление на уровне моря, mmHg
boolean bmeOk = false;               // последнее чтение BME280 достоверно

// суточные минимумы/максимумы (сбрасываются при смене даты)
byte statsDay = 0;
boolean statTempOk = false, statHumOk = false, statPresOk = false, statCO2Ok = false;
float statTempMin = 0, statTempMax = 0;
byte statHumMin = 0, statHumMax = 0;
int statPresMin = 0, statPresMax = 0;
int statCO2Min = 0, statCO2Max = 0;

// будильник / почасовой сигнал
boolean alarmActive = false;
boolean chimeActive = false;
byte alarmPhase = 0;
byte chimeStep = 0;
byte chimePos = 0;                     // позиция в таблице chimePatterns
uint16_t chimeStepMs = 0;              // длительность текущего шага сигнала
boolean touchBeepActive = false;
unsigned long touchBeepStart = 0;
boolean ignoreClick = false;          // проглотить клик, которым остановили будильник
unsigned long alarmStart = 0;
unsigned long chimeStart = 0;
byte alarmDoneDay = 0;
byte lastChimeHour = 255;

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

// символы для крупных цифр
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

// Иконки меню (свободные ячейки 6 и 7)
uint8_t alarmIcon[8] = {0b00100, 0b01110, 0b01110, 0b01110, 0b11111, 0b00100, 0b00000, 0b00000};
uint8_t chimeIcon[8] = {0b00110, 0b00101, 0b00100, 0b00100, 0b01100, 0b11100, 0b11100, 0b00000};

// русские буквы (с)НР

// Сегменты крупных цифр: по 3 байта на строку.
static const uint8_t dig4[11][12] PROGMEM = {
  {255, 0, 255, 255, 32, 255, 255, 32, 255, 255, 3, 255},
  {32, 255, 32, 32, 255, 32, 32, 255, 32, 32, 255, 32},
  {0, 0, 255, 1, 1, 255, 255, 2, 2, 255, 3, 3},
  {0, 0, 255, 1, 1, 255, 2, 2, 255, 3, 3, 255},
  {255, 32, 255, 255, 1, 255, 2, 2, 255, 32, 32, 255},
  {255, 0, 0, 255, 1, 1, 2, 2, 255, 3, 3, 255},
  {255, 0, 0, 255, 1, 1, 255, 2, 255, 255, 3, 255},
  {0, 0, 255, 32, 32, 255, 32, 255, 32, 32, 255, 32},
  {255, 0, 255, 255, 1, 255, 255, 2, 255, 255, 3, 255},
  {255, 0, 255, 255, 1, 255, 2, 2, 255, 3, 3, 255},
  {32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32},
};

static const uint8_t dig2[11][6] PROGMEM = {
  {255, 1, 255, 255, 2, 255},
  {32, 255, 32, 32, 255, 32},
  {3, 3, 255, 255, 4, 4},
  {3, 3, 255, 4, 4, 255},
  {255, 0, 255, 5, 5, 255},
  {255, 3, 3, 4, 4, 255},
  {255, 3, 3, 255, 4, 255},
  {1, 1, 255, 32, 255, 32},
  {255, 3, 255, 255, 4, 255},
  {255, 3, 255, 4, 4, 255},
  {32, 32, 32, 32, 32, 32},
};

void drawDig(byte dig, byte x, byte y) {        // рисуем цифры (с)НР ---------------------------------------
  if (dig > 10) return;
  byte rows;
  const uint8_t *p;
  if (bigDig && DISPLAY_TYPE == 1) {
    rows = 4;
    p = &dig4[dig][0];
  } else {
    rows = 2;
    p = &dig2[dig][0];
  }
  for (byte r = 0; r < rows; r++) {
    lcd.setCursor(x, y + r);
    for (byte i = 0; i < 3; i++) lcd.write(pgm_read_byte(p++));
    if (x <= 11) lcd.print(' ');
  }
}

void drawPPM(int dispCO2, byte x, byte y) {     // Уровень СО2 крупно на главном экране (с)НР ----------------------------
  if (co2Gate) {
    lcd.setCursor(x, y);
    lcd.print(F("---"));
    for (byte i = 4; i < 16; i += 4) drawDig(10, x + i, y);
#if (DISPLAY_TYPE == 0)
    lcd.setCursor(15, 0);
    lcd.print(F("p"));
#endif
    return;
  }
  if (dispCO2 / 1000 == 0) drawDig(10, x, y);
  else drawDig(dispCO2 / 1000, x, y);
  drawDig((dispCO2 % 1000) / 100, x + 4, y);
  drawDig((dispCO2 % 100) / 10, x + 8, y);
  drawDig(dispCO2 % 10 , x + 12, y);
#if (DISPLAY_TYPE == 0)
  lcd.setCursor(15, 0);
  lcd.print(F("p"));
#endif
}

void drawPres(int dispPres, byte x, byte y) {   // Давление крупно на главном экране (с)НР ----------------------------
  drawDig((dispPres % 1000) / 100, x , y);
  drawDig((dispPres % 100) / 10, x + 4, y);
  drawDig(dispPres % 10 , x + 8, y);
  lcd.setCursor(x + 11, 1);
  if (bigDig && DISPLAY_TYPE == 1) lcd.setCursor(x + 11, 3);
  lcd.print(F("mmHg"));
}

void drawPresQnh(int dispPres, byte x, byte y) { // QNH: 4 цифры, чтобы 1000+ mmHg не обрезалось
#if (DISPLAY_TYPE == 1)
  drawDig(dispPres < 1000 ? 10 : (dispPres / 1000) % 10, x, y);
  drawDig((dispPres / 100) % 10, x + 4, y);
  drawDig((dispPres / 10) % 10, x + 8, y);
  drawDig(dispPres % 10, x + 12, y);
  lcd.setCursor(16, bigDig ? 3 : 1);
  lcd.print(F("QNH"));
#else
  drawDig(dispPres < 1000 ? 10 : (dispPres / 1000) % 10, x, y);
  drawDig((dispPres / 100) % 10, x + 3, y);
  drawDig((dispPres / 10) % 10, x + 6, y);
  drawDig(dispPres % 10, x + 9, y);
  lcd.setCursor(12, 1);
  lcd.print(F("QNH"));
#endif
}

void drawTemp(float dispTemp, byte x, byte y) { // Температура крупно на главном экране (с)НР ----------------------------
  if (isnan(dispTemp) || dispTemp > 900 || dispTemp < -900) {
    lcd.setCursor(x, y);
    lcd.print(F("--.-"));
    return;
  }
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
    if (!shift) { lcd.setCursor(px + 1, y + 3); lcd.print(' '); }
  }
  else {
    lcd.setCursor(px, y + 1);
    lcd.write(0B10100001);    // десятичная точка
    if (!shift) { lcd.setCursor(px + 1, y + 1); lcd.print(' '); }
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
    lcd.print('%');
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

static const char dayNames[] PROGMEM = "SuMoTuWeThFrSa";


void drawData() {                     // выводим дату -------------------------------------------------------------
  if (mode0scr >= 7) return;          // экраны статистики и QNH занимают весь дисплей
  if (!bigDig) {              // если 4-х строчные цифры, то дату, день недели (и секунды) не пишем - некуда (с)НР
    if (!timeValid) {         // после потери питания RTC дата недостоверна
      lcd.setCursor(15, 0); lcd.print(F("     "));
      lcd.setCursor(15, 1); lcd.print(F("     "));
      return;
    }
    lcd.setCursor(15, 0);
    if (now.day() < 10) lcd.print(0);
    lcd.print(now.day());
    lcd.print('.');
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
      byte d = now.dayOfTheWeek() * 2;
      lcd.write(pgm_read_byte(&dayNames[d]));
      lcd.write(pgm_read_byte(&dayNames[d + 1]));
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
    lcd.print(' ');
    digits++;
  }
  lcd.print(val);
}

void printTempField(float t, byte width) { // температура с одним знаком, опционально по правому краю
  if (isnan(t) || t > 900 || t < -900) {
    byte n = 4;                            // "--.-"
    while (n < width) { lcd.print(' '); n++; }
    lcd.print(F("--.-"));
    return;
  }
  boolean neg = (t < 0);
  if (neg) t = -t;
  int whole = (int)t;
  int tenths = (int)((t - whole) * 10.0 + 0.5);
  if (tenths > 9) { whole++; tenths = 0; }
  byte len = (whole >= 100 ? 3 : whole >= 10 ? 2 : 1) + 2;
  if (neg) len++;
  while (len < width) { lcd.print(' '); len++; }
  if (neg) lcd.print('-');
  lcd.print(whole);
  lcd.print('.');
  lcd.print(tenths);
}

void printTemp(float t) {          // вывод температуры с одним знаком после запятой без String (с)НР
  printTempField(t, 0);
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
#else
byte LED_ON = (255 - LED_BRIGHT_MAX);
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

  if (((dispCO2 >= maxCO2) && LEDType == 0) || ((dispHum <= minHum) && LEDType == 1) || ((dispTemp >= maxTemp) && LEDType == 2) || ((dispPres <= minPress) && LEDType == 3)) setLEDcolor(3);   // красный
  else if (((dispCO2 >= normCO2) && LEDType == 0) || ((dispHum <= normHum) && LEDType == 1) || ((dispTemp >= normTemp) && LEDType == 2) || ((dispPres <= normPress) && LEDType == 3)) setLEDcolor(3 + 8);   // желтый
  else if ((LEDType == 0) || ((dispHum <= maxHum) && LEDType == 1) || ((dispTemp >= minTemp) && LEDType == 2) || (LEDType == 3)) setLEDcolor(12);    // зеленый
  else setLEDcolor(48);   // синий (влажность выше максимума или температура ниже минимума)
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
  timeValid = true;
  lastChimeHour = hrs;        // не играем сигнал сразу после установки времени
  alarmDoneDay = 0;
  statsReset();                         // суточные min/max начинаем от нового времени
  if (lcdReady && mode == 0) {          // сразу убираем "SET TIME" с экрана
    loadClock();
    drawSensors();
    if (DISPLAY_TYPE == 1) drawData();
  }
  Serial.println(F("TIME SET"));
  return true;
}

void serialTick() {                     // проверка команд из порта в рабочем режиме (с)НР
  char buf[24];
  if (readSerialLine(buf, sizeof(buf))) setTimeSerial(buf);
}

void playChimeSound(byte target);

void setup() {
  Serial.begin(9600);
  rngState = analogRead(A0) ^ (uint16_t)micros();   // зерно для Chime → Random
  if (!rngState) rngState = 0xACE1;
  menuActivity = millis();
  button.setTimeout(350);       // hold enters/repeats a bit faster
  button.setStepTimeout(80);    // auto-repeat interval
#if (RESET_CLOCK == 1)
  Serial.println(F("READY"));           // можно присылать время: setup его ждёт
#endif

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
    // Батарейка RTC разрядилась или модуль долго был без питания.
    // Не подставляем время компиляции: оно вводит в заблуждение.
    // Показываем SET TIME, пока время не придёт с ПК через set_time.sh.
    timeValid = false;
    Serial.println(F("RTC lost power: time is not set"));
  }

  pinMode(BACKLIGHT, OUTPUT);
  pinMode(LED_COM, OUTPUT);
  pinMode(LED_R, OUTPUT);
  pinMode(LED_G, OUTPUT);
  pinMode(LED_B, OUTPUT);
  setLEDcolor(0);

  pinMode(BUZZER_PIN, OUTPUT);
  buzzerOff();

  digitalWrite(LED_COM, LED_MODE);
  analogWrite(BACKLIGHT, LCD_BRIGHT_MAX);

  loadSettings();                    // EEPROM: старые адреса совместимы с v1.9

#if (START_BIG_CLOCK == 1)
  mode0scr = 0;                     // после включения всегда показываем крупные часы
  bigDig = true;
#endif

  lcd.init();
  lcd.backlight();
  lcd.createChar(6, alarmIcon);
  lcd.createChar(7, chimeIcon);
  lcd.clear();
  lcdReady = true;
#if (DISPLAY_TYPE == 1)
  lcd.setCursor(2, 0); lcd.print(F("MeteoClock v2.9"));
  lcd.setCursor(0, 1); lcd.print(F("Compiled ")); lcd.print(F(__DATE__));
  lcd.setCursor(3, 2); lcd.print(F("Time ")); lcd.print(F(__TIME__));
#else
  lcd.setCursor(0, 0); lcd.print(F("v2.9 ")); lcd.print(F(__DATE__));
  lcd.setCursor(4, 1); lcd.print(F(__TIME__));
#endif
#if (RESET_CLOCK == 0)
  playChimeSound(STARTUP_CHIME);      // короткая мелодия запуска
#endif
  delay(1800);
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
    lcd.print(F("P:    "));
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
  // rtc.begin() уже вызван выше — повторный вызов не нужен
  bme.begin(BME280_ADDR, &Wire);
#endif

  bme.setSampling(Adafruit_BME280::MODE_FORCED,
                  Adafruit_BME280::SAMPLING_X1, // temperature
                  Adafruit_BME280::SAMPLING_X1, // pressure
                  Adafruit_BME280::SAMPLING_X1, // humidity
                  Adafruit_BME280::FILTER_OFF);
  // Поправка применяется драйвером, поэтому давление и влажность тоже
  // рассчитываются по скорректированной температуре. Значение берётся из EEPROM.
  applyTempOffset();

  // Здесь нельзя повторно ставить время компиляции: это перезаписывало точное
  // время, полученное из порта в начале setup(), и часы отставали на 10-30 секунд.
  if (!timeValid) {                   // RTC потерял питание: объясняем, что время надо выставить
    lcd.clear();
#if (DISPLAY_TYPE == 1)
    lcd.setCursor(0, 0); lcd.print(F("RTC lost power"));
    lcd.setCursor(0, 1); lcd.print(F("Time is not set"));
    lcd.setCursor(0, 2); lcd.print(F("Run set_time.sh"));
    lcd.setCursor(0, 3); lcd.print(F("or send T... via USB"));
#else
    lcd.setCursor(0, 0); lcd.print(F("RTC lost power"));
    lcd.setCursor(0, 1); lcd.print(F("Set time via USB"));
#endif
    delay(2500);
  }
  lcd.clear();
  now = rtc.now();
  secs = now.second();
  mins = now.minute();
  hrs = now.hour();
  lastChimeHour = hrs;        // не играем сигнал сразу после включения посреди часа

  readSensors();

  if (DISPLAY_TYPE == 1) drawData();
  loadClock();
  drawSensors();

  Serial.println(F("READY"));           // инициализация завершена, можно присылать время
}

void loop() {
  serialTick();                                       // приём команд установки времени с ПК (с)НР
  tickBuzzer();                                       // будильник / почасовой сигнал
  if (testTimer(brightTimerD, brightTimer)) checkBrightness();  // яркость
  if (testTimer(sensorsTimerD, sensorsTimer)) readSensors();    // читаем показания датчиков с периодом SENS_TIME
#if (DEBUG == 1)
  Serial.println(dispTemp);
#endif

  if (testTimer(clockTimerD, clockTimer)) clockTick();          // два раза в секунду пересчитываем время и мигаем точками
  modesTick();                                      // тут ловим нажатия на кнопку и переключаем режимы
  if (mode == 0) {                                  // в режиме "главного экрана"
    if (testTimer(drawSensorsTimerD, drawSensorsTimer)) drawSensors();  // обновляем показания датчиков на дисплее с периодом SENS_TIME
  }
}
