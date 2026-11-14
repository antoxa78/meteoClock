#!/usr/bin/env bash
# Установка времени метеостанции meteoClock с ПК (без Arduino IDE и перепрошивки)
# Использование: ./set_time.sh [порт]
# По умолчанию используется /dev/ttyUSB0

PORT="${1:-/dev/ttyUSB0}"

if [ ! -e "$PORT" ]; then
  echo "Порт $PORT не найден."
  echo "Доступные последовательные порты:"
  ls /dev/ttyUSB* /dev/ttyACM* 2>/dev/null || echo "  (ничего не найдено)"
  echo "Укажите порт: ./set_time.sh /dev/ttyUSB0"
  exit 1
fi

echo "Открытие порта $PORT (устройство кратковременно перезагрузится)..."
stty -F "$PORT" 9600 raw -echo 2>/dev/null || {
  echo "Не удалось открыть порт $PORT (проверьте права доступа: sudo usermod -aG dialout \$USER)"
  exit 1
}

# Открываем порт один раз (одна перезагрузка) и шлём команду ровно один раз.
# Вторую строку слать нельзя: пока инициализируется дисплей и датчики, она
# лежит в буфере и применяется позже, откатывая часы назад на несколько секунд.
exec 3<>"$PORT"

# Ждём READY от прошивки: она печатает его, когда уже может принять время.
# Старые прошивки READY не печатают — через 8 секунд отправляем в любом случае.
READY=0
END=$((SECONDS + 8))
while [ "$SECONDS" -lt "$END" ]; do
  IFS= read -r -t 1 -u 3 LINE 2>/dev/null || continue
  if echo "$LINE" | grep -q "READY"; then
    READY=1
    break
  fi
done
if [ "$READY" -eq 0 ]; then
  echo "READY не получен (старая прошивка?) — отправляю время без ожидания готовности."
fi

# Метку времени берём непосредственно перед отправкой: открытие порта
# перезагружает Arduino, поэтому время, снятое до ожидания, было бы в прошлом.
TS="T$(date '+%Y-%m-%d %H:%M:%S')"
printf '%s\n' "$TS" >&3

# Ждём подтверждение от устройства (прошивка отвечает "TIME SET").
# Читаем несколько строк: при включённом DEBUG в порт также идут показания датчиков.
RESP=""
END=$((SECONDS + 5))
while [ "$SECONDS" -lt "$END" ]; do
  IFS= read -r -t 1 -u 3 LINE 2>/dev/null || break
  RESP+="$LINE"$'\n'
  echo "$LINE" | grep -q "TIME SET" && break
done
exec 3>&-

if echo "$RESP" | grep -q "TIME SET"; then
  echo "Время установлено: $TS"
else
  echo "Отправлено: $TS (подтверждение не получено: ${RESP:-нет ответа})"
fi
