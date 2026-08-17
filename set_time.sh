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

TS="T$(date '+%Y-%m-%d %H:%M:%S')"

# Открываем порт один раз (одна перезагрузка) и шлём команду дважды
# на случай потери первой строки во время загрузки устройства
exec 3<>"$PORT"
sleep 3
printf '%s\n' "$TS" >&3
sleep 1
printf '%s\n' "$TS" >&3

# Ждём подтверждение от устройства (прошивка отвечает "TIME SET")
IFS= read -r -t 5 -u 3 RESP 2>/dev/null || RESP=""
exec 3>&-

if echo "$RESP" | grep -q "TIME SET"; then
  echo "Время установлено: $TS"
else
  echo "Отправлено: $TS (подтверждение не получено: ${RESP:-нет ответа})"
fi
