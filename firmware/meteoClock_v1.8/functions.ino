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

void enterMenuItem() {
  if (podMode == 1) {                         // Save
    if (EEPROM.read(6) != mode0scr) EEPROM.write(6, mode0scr);
    if (EEPROM.read(7) != bigDig) EEPROM.write(7, bigDig);
    if (EEPROM.read(8) != LED_BRIGHT) EEPROM.write(8, LED_BRIGHT);
    if (EEPROM.read(9) != LCD_BRIGHT) EEPROM.write(9, LCD_BRIGHT);
    if (EEPROM.read(10) != LEDType) EEPROM.write(10, LEDType);
    if (EEPROM.read(0) != 124) EEPROM.write(0, 124);
    showMessage(F("Settings Saved"), 14);
    delay(1200);
    mode = 0;
  } else if (podMode == 2) {                  // Exit
    mode = 0;
  } else if (podMode >= 3 && podMode <= 5) {  // Indicator Brightness / LCD Brightness / Indicator Mode
    mode = 255 - podMode + 2;
    if (mode == 252) podMode = LEDType;
    else if (mode == 253) podMode = LCD_BRIGHT;
    else if (mode == 254) podMode = LED_BRIGHT;
  } else if (podMode == 6) {                  // CO2 zero calibration
#if (CO2_SENSOR == 1)
    if (dispCO2 >= 350 && dispCO2 <= 500) {
      boolean ok = mhz19.calibrateToZeroPoint();
      showMessage(ok ? F("Calibration Success") : F("Calibration Failed"),
                  ok ? 19 : 18);
      if (ok) {                               // сохраняем дату калибровки
        DateTime cal = rtc.now();
        EEPROM.update(11, cal.day());
        EEPROM.update(12, cal.month());
        EEPROM.update(13, cal.year() % 100);
      }
    } else {
      showMessage(F("Need 400 PPM"), 12);
    }
    delay(1500);
#endif
    mode = 255;
    podMode = 6;
  }
}

/*
  mode:
  0 - Main screen
  252 - indicator mode (podMode 0-4: CO2, humidity, temperature, rain, pressure)
  253 - LCD brightness (podMode 0-11: 0-100% or auto)
  254 - indicator brightness (podMode 0-11: 0-100% or auto)
  255 - setup menu (podMode: 1 - Save, 2 - Exit, 3 - Indicator Brightness,
                     4 - LCD Brightness, 5 - Indicator Mode, 6 - Calibration CO2)
*/
void modesTick() {
  button.tick();
  boolean changeFlag = false;
  if (button.isSingle()) {    // одинарное нажатие: меняем значение / пункт меню
    if (mode == 0) {
      mode0scr++;                                  // переключение крупного показателя
      if (CO2_SENSOR == 0 && mode0scr == 1) mode0scr++;
      if (mode0scr > 4) mode0scr = 0;
      changeFlag = true;
    } else if (mode >= 240) {
      podMode++;
      switch (mode) {
        case 252:             // режим LED индикатора
          if (podMode > 4) podMode = 0;
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
          if (podMode > 6) podMode = 1;
          changeFlag = true;
          break;
      }
    }
  }
  if (button.isDouble()) {                    // двойное нажатие: вход/выход
    if (mode == 0) {
      mode = 255;                             // вход в меню
      podMode = 3;
      changeFlag = true;
    } else if (mode == 255) {
      enterMenuItem();                        // войти в выбранный пункт / выполнить
      changeFlag = true;
    } else if (mode >= 252 && mode <= 254) {
      mode = 255;                             // назад в меню
      podMode = 1;
      changeFlag = true;
    }
  }

  if ((button.isTriple()) && (mode == 0)) {   // тройное нажатие: вход в настройки
    mode = 255;
    podMode = 3;
    changeFlag = true;
  }

  if (button.isHolded()) {    // удержание кнопки (с)НР
    //    if ((mode >=252) && (mode <= 254)) {
    //      mode = 255;
    //      podMode = 1;
    //    }
    switch (mode) {
      case 0:
        bigDig = !bigDig;
        break;
      case 252:       // реж. индикатора
        mode = 255;
        podMode = 1;
        break;
      case 253:       // ярк. экрана
        mode = 255;
        podMode = 1;
        break;
      case 254:       // ярк. индикатора
        mode = 255;
        podMode = 1;
        break;
      case 255:       // главное меню
        enterMenuItem();
        break;
      default:
        mode = 0;
    }
    changeFlag = true;
  }

  if (changeFlag) {
    if (mode >= 240) {
      lcd.clear();
      lcd.setCursor(0, 0);
    }
    if (mode == 255) {          // Перебираем варианты в главном меню (с)НР
#if (WEEK_LANG == 1)
      lcd.print("HACTPO\2K\4:");              // ---Настройки
#else
      lcd.print("Setup Menu");
#endif
      lcd.setCursor(0, 1);
      switch (podMode) {
        case 1:
#if (WEEK_LANG == 1)
          lcd.print("COXPAH\4T\1");     // ---Сохранить
#else
          lcd.print("Save");
#endif
          break;
        case 2:
#if (WEEK_LANG == 1)
          lcd.print("B\6XO\3");         // --- Выход
#else
          lcd.print("Exit");
#endif
          break;
        case 5:
#if (WEEK_LANG == 1)
          lcd.print("PE\10.\4H\3\4KATOPA");  // ---Реж.индик.
#else
          lcd.print("Indicator Mode");
#endif
          break;
        case 3:
#if (WEEK_LANG == 1)
          lcd.print("\5PK.\4H\3\4KATOPA");  // ---Ярк.индик.
#else
          lcd.print("Indicator Brightness");
#endif
          break;
        case 4:
#if (WEEK_LANG == 1)
          lcd.print("\5PK.\7KPAHA");    // ---Ярк.экрана
#else
          lcd.print("LCD Brightness");
#endif
          break;
        case 6:
          lcd.print("Calibration CO2");
          lcd.setCursor(0, 2);
          lcd.print("Sensor");
          lcd.setCursor(0, 3);
          {
            byte d = EEPROM.read(11), m = EEPROM.read(12), y = EEPROM.read(13);
            if (d >= 1 && d <= 31 && m >= 1 && m <= 12) {
              lcd.print(F("Last Date: "));
              if (d < 10) lcd.print('0');
              lcd.print(d); lcd.print('.');
              if (m < 10) lcd.print('0');
              lcd.print(m); lcd.print('.');
              if (y < 10) lcd.print('0');
              lcd.print(y);
            } else {
              lcd.print(F("Last Date: --.--.--"));
            }
          }
          break;
      }
    }

    if (mode == 252) {                        // --------------------- показать  "Реж.индикатора"
      LEDType = podMode;
      lcd.setCursor(0, 0);
#if (WEEK_LANG == 1)
      lcd.print("PE\10.\4H\3\4KATOPA:");
#else
      lcd.print("Indicator Mode:");
#endif
      lcd.setCursor(0, 1);
      switch (podMode) {
        case 0:
          lcd.print("CO2");
          break;
        case 1:
#if (WEEK_LANG == 1)
          lcd.print("B\6A\10H.");          // влажн.
#else
          lcd.print("Humidity");
#endif
          break;
        case 2:
          lcd.print("Temperature");
          break;
        case 3:
#if (WEEK_LANG == 1)
          lcd.print("OCA\3K\5");          // осадки
#else
          lcd.print("Rain");
#endif
          break;
        case 4:
#if (WEEK_LANG == 1)
          lcd.print("\3AB\6EH\5E");       // давление
#else
          lcd.print("Pressure");
#endif
          break;
      }

    }
    if (mode == 253) {                        // --------------------- показать  "Ярк.экрана"
#if (WEEK_LANG == 1)
      lcd.print("\5PK.\7KPAHA:");// + String(LCD_BRIGHT * 10) + "%  ");
#else
      lcd.print("LCD Brightness:");
#endif
      lcd.setCursor(0, 1);
      if (LCD_BRIGHT == 11) {
#if (WEEK_LANG == 1)
        lcd.print("ABTO ");
#else
        lcd.print("Auto ");
#endif
      }
      else { lcd.print(LCD_BRIGHT * 10); lcd.print("%"); }
    }
    if (mode == 254) {                        // --------------------- показать  "Ярк.индикатора"
#if (WEEK_LANG == 1)
      lcd.print("\5PK.\4H\3\4K.:");// + String(LED_BRIGHT * 10) + "%  ");
#else
      lcd.print("Indicator Brightness");
#endif
      lcd.setCursor(0, 1);
      if (LED_BRIGHT == 11) {
#if (WEEK_LANG == 1)
        lcd.print("ABTO ");
#else
        lcd.print("Auto ");
#endif
      }
      else { lcd.print(LED_BRIGHT * 10); lcd.print("%"); }
    }

    if (mode == 0) {
      lcd.clear();
      loadClock();
      drawSensors();
      if (DISPLAY_TYPE == 1) drawData();
    }
  }
}

void readSensors() {
  bme.takeForcedMeasurement();
  dispTemp = bme.readTemperature() + TEMP_CORRECTION;
  dispHum = bme.readHumidity();
  dispPres = (float)bme.readPressure() * 0.00750062;
#if (CO2_SENSOR == 1)
  dispCO2 = mhz19.getCarbonDioxide();
  if (dispCO2 < 0) dispCO2 = mhz19.getCarbonDioxide();   // повторный запрос при сбое UART (с)НР
#else
  dispCO2 = 0;
#endif

  if (millis() > CO2_WARMUP_MS) co2Gate = false;
  if (dispCO2 < 0 || dispCO2 > 4000) co2Gate = true;

#if (DEBUG == 1)
  Serial.print(F("CO2=")); Serial.print(dispCO2);
  Serial.print(F(" T=")); Serial.print(dispTemp, 1);
  Serial.print(F(" H=")); Serial.print(dispHum, 1);
  Serial.print(F(" P=")); Serial.println(dispPres);
#endif
}

void drawSensors() {
#if (DISPLAY_TYPE == 1)
  // дисплей 2004 ----------------------------------

  if (mode0scr != 2) {                        // Температура (с)НР ----------------------------
    lcd.setCursor(0, 2);
    if (bigDig) {
      if (mode0scr == 1) lcd.setCursor(15, 2);
      if (mode0scr != 1) lcd.setCursor(15, 0);
    } else {
      lcd.print("T");
    }
    printTemp(dispTemp);
    lcd.write(223);
  } else {
    drawTemp(dispTemp, 0, 0);
  }

  if (mode0scr != 4) {                        // Влажность (с)НР ----------------------------
    if (bigDig) lcd.setCursor(15, 1);
    else lcd.setCursor(dispHum >= 100 ? 7 : 8, 2);   // 100% сдвигаем влево, чтобы сохранить пробел
    if (!bigDig) lcd.print("H");
    lcd.print((int)dispHum); lcd.print("%");
  } else {
    drawHum(dispHum, 0, 0);
  }

#if (CO2_SENSOR == 1)
  if (mode0scr != 1) {                       // СО2 (с)НР ----------------------------

    if (bigDig) {
      lcd.setCursor(15, 2);
      if (co2Gate) lcd.print(F("---")); else lcd.print(dispCO2);
      lcd.print("p");
    } else {
      lcd.setCursor(13, 2);
      if (co2Gate) lcd.print(F("---")); else lcd.print(dispCO2);
      lcd.print("ppm");
    }
  } else {
    drawPPM(dispCO2, 0, 0);
  }
#endif

  if (mode0scr != 3) {                      // Давление (с)НР ---------------------------
    lcd.setCursor(0, 3);
    if (bigDig && mode0scr == 0) lcd.setCursor(15, 3);
    if (bigDig && (mode0scr == 1 || mode0scr == 2)) lcd.setCursor(15, 0);
    if (bigDig && mode0scr == 4) lcd.setCursor(15, 1);
    if (!(bigDig && mode0scr == 1)) {
      if (!bigDig) lcd.print("P:");
      lcd.print(dispPres);
      lcd.print(bigDig ? "mm" : "mmHg");
    }
  } else {
    drawPres(dispPres, 0, 0);
  }

  if (mode0scr != 0) {                      // время (с)НР ----------------------------
    lcd.setCursor(15, 3);
    if (hrs / 10 == 0) lcd.print(" ");
    lcd.print(hrs);
    lcd.print(":");
    if (mins / 10 == 0) lcd.print("0");
    lcd.print(mins);
  } else {
    drawClock(hrs, mins, 0, 0); //, 1);
  }
#else

  // дисплей 1602 ----------------------------------
  if (!bigDig) {              // если только мелкими цифрами (с)НР
    lcd.setCursor(0, 0);
    printTemp(dispTemp);
    lcd.write(223);
    lcd.setCursor(6, 0);
    lcd.print(dispHum); lcd.print("% ");

#if (CO2_SENSOR == 1)
    if (co2Gate) lcd.print(F("---")); else lcd.print(dispCO2);
    lcd.print("ppm");
    if (dispCO2 < 1000) lcd.print(" ");
#endif

    lcd.setCursor(0, 1);
    lcd.print(dispPres); lcd.print(" mm  Rain ");
    lcd.print(dispRain); lcd.print("% ");
  } else {                    // для крупных цифр (с)НР
    switch (mode0scr) {
      case 0:
        drawClock(hrs, mins, 0, 0);
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
      case 5:
        break;
    }
  }
#endif
}

void plotSensorsTick() {
  // 10 минутный таймер
  if (testTimer(predictTimerD, predictTimer)) {
    // тут делаем линейную аппроксимацию для предсказания погоды
    long averPress = 0;
    for (byte i = 0; i < 10; i++) {
      bme.takeForcedMeasurement();
      averPress += bme.readPressure();
      delay(1);
    }
    averPress /= 10;

    for (byte i = 0; i < 5; i++) {                   // счётчик от 0 до 5 (да, до 5. Так как 4 меньше 5)
      pressure_array[i] = pressure_array[i + 1];     // сдвинуть массив давлений КРОМЕ ПОСЛЕДНЕЙ ЯЧЕЙКИ на шаг назад
    }
    pressure_array[5] = averPress;                   // последний элемент массива теперь - новое давление
    sumX = 0;
    sumY = 0;
    sumX2 = 0;
    sumXY = 0;
    for (int i = 0; i < 6; i++) {                    // для всех элементов массива
      //sumX += time_array[i];
      sumX += i;
      sumY += (long)pressure_array[i];
      //sumX2 += time_array[i] * time_array[i];
      sumX2 += i * i;
      //sumXY += (long)time_array[i] * pressure_array[i];
      sumXY += (long)i * pressure_array[i];
    }
    a = 0;
    a = (long)6 * sumXY;                            // расчёт коэффициента наклона приямой
    a = a - (long)sumX * sumY;
    a = (float)a / (6 * sumX2 - sumX * sumX);
    delta = a * 6;      // расчёт изменения давления
    dispRain = map(delta, -250, 250, 100, -100);    // пересчитать в проценты
    //Serial.println(String(pressure_array[5]) + " " + String(delta) + " " + String(dispRain));   // дебаг
  }
}

boolean dotFlag;
void clockTick() {
  dotFlag = !dotFlag;
  if (dotFlag) {            // каждую секунду пересчёт времени
    secs++;
    if (secs > 59) {        // каждую минуту
      secs = 0;
      mins++;
      if (mins <= 59 && mode == 0) {
        drawSensors();      // (с)НР
      }
    }
    if (mins > 59) {        // каждый час
      // loadClock();        // Обновляем знаки, чтобы русские буквы в днях недели тоже обновились. (с)НР
      now = rtc.now();
      secs = now.second();
      mins = now.minute();
      hrs = now.hour();
      if (mode == 0) drawSensors();
      if (hrs > 23) hrs = 0;
      if (mode == 0 && DISPLAY_TYPE) drawData();
    }
    if ((DISP_MODE != 0 && mode == 0) && DISPLAY_TYPE == 1 && !bigDig) {   // Если режим секунд или дни недели по-русски, и 2-х строчные цифры то показывать секунды (с)НР
      lcd.setCursor(15, 1);
      if (secs < 10) lcd.print(" ");
      lcd.print(secs);
    }
  }

  if (mode == 0) {                                     // Точки (с)НР ---------------------------------------------------

    byte code;
    if (dotFlag) code = 165;
    else code = 32;
    if (mode0scr == 0 && (bigDig && DISPLAY_TYPE == 0 || DISPLAY_TYPE == 1)) {          // мигание большими точками только в нулевом режиме главного экрана (с)НР
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

  if ((dispCO2 >= blinkLEDCO2 && LEDType == 0 || dispHum <= blinkLEDHum && LEDType == 1 || dispTemp >= blinkLEDTemp && LEDType == 2) && !dotFlag) setLEDcolor(0);     // мигание индикатора в зависимости от значения и привязанного сенсора (с)НР
  else setLED();
}

boolean testTimer(unsigned long & dataTimer, unsigned long setTimer) {   // Проверка таймеров (с)НР
  if (millis() - dataTimer >= setTimer) {
    dataTimer = millis();
    return true;
  } else {
    return false;
  }
}
