# Mhz19 2.0.2 — patched for meteoClock

This is the upstream **Mhz19 2.0.2** library by Eduard Malokhvii with two
changes required by the meteoClock firmware:

1. `src/Mhz19.cpp` — `getCarbonDioxide()` now waits up to **200 ms** for the
   sensor’s reply before reading it. The stock version checks `available()`
   immediately after sending and often misses the reply on a Nano with
   `SoftwareSerial`.
2. `src/Mhz19.h` — `MHZ19_PREHEATING_DURATION` is **0** instead of 180000.
   The sensor is powered continuously in meteoClock, so the library’s
   3-minute software preheat is unnecessary; without this the display shows
   `-1` after every Arduino reset.

Wiring used by meteoClock v1.7:

- `D2` = Nano RX ← sensor **Tx**
- `D3` = Nano TX → sensor **Rx**

(Note: this is the opposite of the original `scheme2-5.jpg`; the actual
working wiring was verified on the bench.)

To install this patched library in the Arduino IDE, copy this directory to
`~/Arduino/libraries/Mhz19` (or point the sketchbook `libraries` path at the
project’s `libraries` directory).
