/*
	xsns_06_dht.ino - DHTxx and AM23xx temperature and humidity sensor support for Sonoff-Tasmota

	Copyright (C) 2017	Theo Arends

	This program is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program.	If not, see <http://www.gnu.org/licenses/>.
*/

#ifdef USE_DHT
/*********************************************************************************************\
 * DHT11, DHT21 (AM2301), DHT22 (AM2302, AM2321) - Temperature and Humidy
 *
 * Reading temperature or humidity takes about 250 milliseconds!
 * Sensor readings may also be up to 2 seconds 'old' (its a very slow sensor)
 * Source: Adafruit Industries https://github.com/adafruit/DHT-sensor-library
\*********************************************************************************************/

#include <Ticker.h>

#define DHT_MAX_SENSORS		3
#define DHT_MAX_RETRY		8
#define DHT_CONTROL_PERIOD	250
#define DHT_MIN_INTERVAL	5000

typedef struct DhtDesc {
	byte		pin;
	byte		type;
	uint8_t 	trycount;
	uint8_t		data[5];
	char		stype[10];
} DhtDesc;

static enum {
	DHT_WAIT_PREP	 = 0,
	DHT_WAIT_READ,
	DHT_QUIET,
	DHT_IN_PROGRESS
} dht_state = DHT_WAIT_PREP;

static uint32_t dht_max_cycles;
static byte		dht_sensors = 0;
static byte		loopcount = 0;
static Ticker	DhtTicker;
static Ticker	DhtReadTicker;

static DhtDesc Dhts[DHT_MAX_SENSORS];

static void
DhtReadPrep()
{
	for (byte i = 0; i < dht_sensors; i++)
		digitalWrite(Dhts[i].pin, HIGH);
}

static uint32_t
DhtExpectPulse(struct DhtDesc &dht, bool level)
{
	uint32_t count = 0;

	while (digitalRead(dht.pin) == level)
		if (count++ >= dht_max_cycles)
			return 0;

	return count;
}

static void
DhtEndReadDataSensor(struct DhtDesc& dht)
{
	uint32_t cycles[80];

	noInterrupts();
	digitalWrite(dht.pin, HIGH);
	delayMicroseconds(40);
	pinMode(dht.pin, INPUT_PULLUP);
	delayMicroseconds(10);
	if (0 == DhtExpectPulse(dht, LOW)) {
		AddLog_P(LOG_LEVEL_DEBUG, PSTR(D_LOG_DHT D_TIMEOUT_WAITING_FOR " " D_START_SIGNAL_LOW " " D_PULSE));
		goto somethingwrong_unlock;
	}
	if (0 == DhtExpectPulse(dht, HIGH)) {
		AddLog_P(LOG_LEVEL_DEBUG, PSTR(D_LOG_DHT D_TIMEOUT_WAITING_FOR " " D_START_SIGNAL_HIGH " " D_PULSE));
		goto somethingwrong_unlock;
	}
	for (int i = 0; i < 80; i += 2) {
		cycles[i]	 = DhtExpectPulse(dht, LOW);
		cycles[i+1] = DhtExpectPulse(dht, HIGH);
	}
	interrupts();

	memset(dht.data, 0, sizeof(dht.data));

	for (int i=0; i<40; ++i) {
		uint32_t lowCycles	= cycles[2*i];
		uint32_t highCycles = cycles[2*i+1];
		if ((0 == lowCycles) || (0 == highCycles)) {
			AddLog_P(LOG_LEVEL_DEBUG, PSTR(D_LOG_DHT D_TIMEOUT_WAITING_FOR " " D_PULSE));
			dht.trycount = DHT_MAX_RETRY; //dht.data contains wrong data 
			goto somethingwrong;
		}
		dht.data[i/8] <<= 1;
		if (highCycles > lowCycles) {
			dht.data[i/8] |= 1;
		}
	}

	AddLog_PP(LOG_LEVEL_DEBUG, PSTR(D_LOG_DHT D_RECEIVED " %02X, %02X, %02X, %02X, %02X =? %02X"),
			dht.data[0], dht.data[1], dht.data[2], dht.data[3], dht.data[4],
			(dht.data[0] + dht.data[1] + dht.data[2] + dht.data[3]) & 0xFF);

	if (dht.data[4] != ((dht.data[0] + dht.data[1] + dht.data[2] + dht.data[3]) & 0xFF)) {
		AddLog_P(LOG_LEVEL_DEBUG, PSTR(D_LOG_DHT D_CHECKSUM_FAILURE));
		dht.trycount = DHT_MAX_RETRY; //dht.data contains wrong data 
		goto somethingwrong;
	}

	// mark as success
	dht.trycount = 0;
	return;

somethingwrong_unlock:
	interrupts();

somethingwrong:
	// trycount > 0 - unsuccessful read
	if (dht.trycount < DHT_MAX_RETRY)
		dht.trycount++;
}

static void
DhtEndReadData()
{
	for (byte i = 0; i < dht_sensors; i++)
		DhtEndReadDataSensor(Dhts[i]);
}

static void
DhtStartReadData()
{
	for (byte i = 0; i < dht_sensors; i++) {
		DhtDesc	&dht = Dhts[i];
		
		pinMode(dht.pin, OUTPUT);
		digitalWrite(dht.pin, LOW);
	}

	// delay could not be called inside Ticker callback
	DhtReadTicker.once_ms(20, DhtEndReadData); 
}

boolean
DhtGetTempHum(struct DhtDesc& dht, float &t, float &h)
{
	if (dht.trycount >= DHT_MAX_RETRY)
		return false;

	t = NAN;
	h = NAN;

	switch (dht.type) {
		case GPIO_DHT11:
			h = dht.data[0];
			t = ConvertTemp(dht.data[2]);
			break;
		case GPIO_DHT22:
		case GPIO_DHT21:
			h = dht.data[0];
			h *= 256;
			h += dht.data[1];
			h *= 0.1;
			t = dht.data[2] & 0x7F;
			t *= 256;
			t += dht.data[3];
			t *= 0.1;
			if (dht.data[2] & 0x80) {
				t *= -1;
			}
			t = ConvertTemp(t);
			break;
	}

	return (!isnan(t) && !isnan(h) && h > 0);
}

boolean DhtSetup(byte pin, byte type)
{
	boolean success = false;

	if (dht_sensors < DHT_MAX_SENSORS) {
		DhtDesc& dht = Dhts[dht_sensors];

		dht.pin = pin;
		dht.type = type;
		dht.trycount = DHT_MAX_RETRY;
		dht_sensors++;
		success = true;
	}
	return success;
}

/********************************************************************************************/
static void
DhtWatch()
{
	switch(dht_state) {
		case DHT_WAIT_PREP:
			DhtReadPrep();
			dht_state = DHT_WAIT_READ;
			break;
		case DHT_WAIT_READ:
			dht_state = DHT_IN_PROGRESS;
			DhtStartReadData();
			dht_state = DHT_QUIET;
			break;
		case DHT_IN_PROGRESS:
			/* do not count loops if we are in progress state */
			AddLog_P(LOG_LEVEL_INFO, PSTR(D_LOG_DHT "Seems, another loop is not finished yet"));
			return;
		case  DHT_QUIET:
			if (loopcount >= DHT_MIN_INTERVAL/DHT_CONTROL_PERIOD) {
				loopcount = 0;
				dht_state = DHT_WAIT_PREP;
				return;
			}
			break;
	}

	loopcount++;
}

void DhtInit()
{
	dht_max_cycles = microsecondsToClockCycles(1000);	// 1 millisecond timeout for reading pulses from DHT sensor.

	for (byte i = 0; i < dht_sensors; i++) {
		DhtDesc	&dht = Dhts[i];

		pinMode(dht.pin, INPUT_PULLUP);
		switch (dht.type) {
		case GPIO_DHT11:
			strcpy_P(dht.stype, PSTR("DHT11"));
			break;
		case GPIO_DHT21:
			strcpy_P(dht.stype, PSTR("AM2301"));
			break;
		case GPIO_DHT22:
			strcpy_P(dht.stype, PSTR("DHT22"));
		}
		if (dht_sensors > 1) {
			snprintf_P(dht.stype, sizeof(dht.stype), PSTR("%s-%02d"), dht.stype, dht.pin);
		}
	}

	DhtTicker.attach_ms(DHT_CONTROL_PERIOD, DhtWatch);
}

#define SHOW_JSON	0x01
#define SHOW_WEB	0x02
#define SHOW_MQTT	0x03

void DhtShow(byte type)
{
	char temperature[10];
	char humidity[10];
	float t;
	float h;

#ifdef USE_DOMOTICZ
	byte dsxflg = 0;
#endif
	for (byte i = 0; i < dht_sensors; i++) {
		DhtDesc	&dht = Dhts[i];

		// Read temperature
		if (!DhtGetTempHum(dht, t, h))
			continue;

		dtostrfd(t, Settings.flag.temperature_resolution, temperature);
		dtostrfd(h, Settings.flag.humidity_resolution, humidity);

#ifdef USE_LCD1602A
		LcdDataExchange.DHT22_temperature = t;
		LcdDataExchange.DHT22_humidity = h;
#endif
		if (type == SHOW_MQTT) {
			char	x[100];
			BufferString	topic(x, sizeof(x));

			topic.sprintf_P(FPSTR("%s_temperature"), dht.stype);
			MqttPublishSimple(topic.c_str(), t);
			topic.reset();

			topic.sprintf_P(FPSTR("%s_humidity"), dht.stype);
			MqttPublishSimple(topic.c_str(), h);
		} else if (type == SHOW_JSON) {
			mqtt_msg.sprintf_P(FPSTR( JSON_SNS_TEMPHUM), dht.stype, temperature, humidity);
#ifdef USE_DOMOTICZ
			if (!dsxflg) {
				DomoticzTempHumSensor(temperature, humidity);
				dsxflg++;
			}
#endif	// USE_DOMOTICZ
#ifdef USE_WEBSERVER
		} else if (type == SHOW_WEB) {
				mqtt_msg.sprintf_P(FPSTR(HTTP_SNS_TEMP), dht.stype, temperature, TempUnit());
				mqtt_msg.sprintf_P(FPSTR(HTTP_SNS_HUM), dht.stype, humidity);
#endif	// USE_WEBSERVER
		}
	}
}

/*********************************************************************************************\
 * Interface
\*********************************************************************************************/

#define XSNS_06

boolean Xsns06(byte function, void *arg)
{
	boolean result = false;

	if (dht_flg) {
		switch (function) {
			case FUNC_XSNS_INIT:
				DhtInit();
				break;
			case FUNC_XSNS_JSON_APPEND:
				DhtShow(SHOW_JSON);
				break;
			case FUNC_XSNS_MQTT_SIMPLE:
				DhtShow(SHOW_MQTT);
				break;
#ifdef USE_WEBSERVER
			case FUNC_XSNS_WEB:
				DhtShow(SHOW_WEB);
				break;
#endif	// USE_WEBSERVER
		}
	}
	return result;
}

#endif	// USE_DHT
