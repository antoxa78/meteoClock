/*
  MHZ19_uart.h - MH-Z19 CO2 sensor library for ESP-WROOM-02/32(ESP8266/ESP32) or Arduino
  version 0.3
  
  License MIT
*/

#ifndef MHZ19_uart_h_
#define MHZ19_uart_h_

#include "Arduino.h"
#ifdef ARDUINO_ARCH_ESP32
	#include "HardwareSerial.h"
#else
	#include "SoftwareSerial.h"
#endif

enum MHZ19_DATA { 
	PPM, TEMPERATURE, STAT 
};

class MHZ19_uart
{
public:
	MHZ19_uart();
	MHZ19_uart(int rx, int tx);
	virtual ~MHZ19_uart();

#ifdef ARDUINO_ARCH_ESP32
	void begin(int rx=-1, int tx=-1, int s=1);
#else
	void begin(int rx=-1, int tx=-1);
#endif
	void setAutoCalibration(boolean autocalib);
	void calibrateZero();
	void calibrateSpan(int ppm);
	
	int getPPM();
	int getTemperature();
	int getStatus();
	
	boolean isWarming();

protected:
	void writeCommand(uint8_t com[]);
	void writeCommand(uint8_t com[], uint8_t response[]);

private:
	uint8_t mhz19_checksum( uint8_t com[] );
	int getSerialData(MHZ19_DATA flg);

	static const int REQUEST_CNT = 8;
	static const int RESPONSE_CNT = 9;

#ifdef ARDUINO_ARCH_ESP32
	int _serialno = 1;
#endif
	int _rx_pin = -1;
	int _tx_pin = -1;
};

#endif
