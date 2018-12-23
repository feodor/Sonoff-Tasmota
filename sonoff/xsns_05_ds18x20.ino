/*
  xsns_05_ds18x20.ino - DS18x20 temperature sensor support for Sonoff-Tasmota

  Copyright (C) 2017  Heiko Krupp and Theo Arends

  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifdef USE_DS18x20
/*********************************************************************************************\
 * DS18B20 - Temperature
\*********************************************************************************************/

#define DS18S20_CHIPID       0x10
#define DS18B20_CHIPID       0x28
#define MAX31850_CHIPID      0x3B

#define W1_SKIP_ROM          0xCC
#define W1_CONVERT_TEMP      0x44
#define W1_READ_SCRATCHPAD   0xBE

#define DS18X20_MAX_SENSORS  8

#include <OneWire.h>
#include <Ticker.h>

static OneWire *ds = NULL;
static Ticker	Ds18x20Ticker;

#define MAX_TRY_COUNT		3
#define CONTROL_PERIOD		200
//see Ds18x20Convert()
#define WAIT_TO_READ 		750 


typedef enum {
	Ds18x20_WAIT_PREP = 0,
	Ds18x20_WAIT_READ,
	Ds18x20_IN_PROGRESS
} Ds18x20States;

static struct {
	uint8_t address[DS18X20_MAX_SENSORS][8];
	uint8_t index[DS18X20_MAX_SENSORS];
	uint8_t trycount[DS18X20_MAX_SENSORS];	
	float   temperature[DS18X20_MAX_SENSORS];
	uint8_t num;
	uint8_t loopcount;
	Ds18x20States state;
	char buffer[9];
} Ds18x20State; 

void Ds18x20Init()
{
  ds = new OneWire(pin[GPIO_DSB]);

  memset(&Ds18x20State, 0, sizeof(Ds18x20State));
  Ds18x20State.state = Ds18x20_WAIT_PREP;

  for (byte i = 0; i<DS18X20_MAX_SENSORS; i++)
	  Ds18x20State.temperature[i] = NAN;

  Ds18x20Ticker.attach_ms(CONTROL_PERIOD, Ds18x20Watch);
}

void Ds18x20Watch()
{
	if (Ds18x20State.state == Ds18x20_WAIT_PREP)
	{
		// set in progrees to do not mix 
		Ds18x20State.state = Ds18x20_IN_PROGRESS;
    	Ds18x20Search();      // Check for changes in sensors number
    	Ds18x20Convert();     // Start Conversion, takes up to one second
		Ds18x20State.state = Ds18x20_WAIT_READ;
		Ds18x20State.loopcount = 0;
	}
	else if (Ds18x20State.state == Ds18x20_WAIT_READ)
	{
		Ds18x20State.loopcount++;

		if (Ds18x20State.loopcount * CONTROL_PERIOD >= WAIT_TO_READ)
		{
			Ds18x20State.state = Ds18x20_IN_PROGRESS;

			for (byte i = 0; i < Ds18x20Sensors(); i++)
			{
				float t;
	    		if (Ds18x20Read(i, t))
				{
					Ds18x20State.temperature[i] = t;
					Ds18x20State.trycount[i] = 0;
				}
				else if (Ds18x20State.trycount[i] >= MAX_TRY_COUNT)
				{
					Ds18x20State.temperature[i] = NAN;
				}
				else
				{
					Ds18x20State.trycount[i]++;
				}
			}

			Ds18x20State.state = Ds18x20_WAIT_PREP;
		}
	}
	else if (Ds18x20State.state == Ds18x20_IN_PROGRESS)
	{
		AddLog_P(LOG_LEVEL_INFO, PSTR("DS18x20: Seems, another loop is not finished yet"));
	}
	else
	{
		AddLog_PP(LOG_LEVEL_INFO, PSTR("DS18x20: Unexpected state: %d"),
				  Ds18x20State.state);
	}
}

void Ds18x20Search()
{
  uint8_t num_sensors=0;
  uint8_t sensor = 0;

  ds->reset_search();
  for (num_sensors = 0; num_sensors < DS18X20_MAX_SENSORS; num_sensors) {
    if (!ds->search(Ds18x20State.address[num_sensors])) {
      ds->reset_search();
      break;
    }
    // If CRC Ok and Type DS18S20, DS18B20 or MAX31850
    if ((OneWire::crc8(Ds18x20State.address[num_sensors], 7) == Ds18x20State.address[num_sensors][7]) &&
       ((Ds18x20State.address[num_sensors][0]==DS18S20_CHIPID) || (Ds18x20State.address[num_sensors][0]==DS18B20_CHIPID) || (Ds18x20State.address[num_sensors][0]==MAX31850_CHIPID))) {
      num_sensors++;
    }
  }
  for (byte i = 0; i < num_sensors; i++) {
    Ds18x20State.index[i] = i;
  }
  for (byte i = 0; i < num_sensors; i++) {
    for (byte j = i + 1; j < num_sensors; j++) {
      if (uint32_t(Ds18x20State.address[Ds18x20State.index[i]]) > uint32_t(Ds18x20State.address[Ds18x20State.index[j]])) {
        std::swap(Ds18x20State.index[i], Ds18x20State.index[j]);
      }
    }
  }
  Ds18x20State.num = num_sensors;
}

uint8_t Ds18x20Sensors()
{
  return Ds18x20State.num;
}

String Ds18x20Addresses(uint8_t sensor)
{
  char address[20];

  for (byte i = 0; i < 8; i++) {
    sprintf(address+2*i, "%02X", Ds18x20State.address[Ds18x20State.index[sensor]][i]);
  }
  return String(address);
}

void Ds18x20Convert()
{
  ds->reset();
  ds->write(W1_SKIP_ROM);        // Address all Sensors on Bus
  ds->write(W1_CONVERT_TEMP);    // start conversion, no parasite power on at the end
//  delay(750);                   // 750ms should be enough for 12bit conv
}

boolean Ds18x20Read(uint8_t sensor, float &t)
{
  byte data[12];
  int8_t sign = 1;
  float temp9 = 0.0;
  uint8_t present = 0;

  t = NAN;

  ds->reset();
  ds->select(Ds18x20State.address[Ds18x20State.index[sensor]]);
  ds->write(W1_READ_SCRATCHPAD); // Read Scratchpad

  for (byte i = 0; i < 9; i++) {
    data[i] = ds->read();
  }
  if (OneWire::crc8(data, 8) == data[8]) {
    switch(Ds18x20State.address[Ds18x20State.index[sensor]][0]) {
    case DS18S20_CHIPID:  // DS18S20
      if (data[1] > 0x80) {
        data[0] = (~data[0]) +1;
        sign = -1;  // App-Note fix possible sign error
      }
      if (data[0] & 1) {
        temp9 = ((data[0] >> 1) + 0.5) * sign;
      } else {
        temp9 = (data[0] >> 1) * sign;
      }
      t = ConvertTemp((temp9 - 0.25) + ((16.0 - data[6]) / 16.0));
      break;
    case DS18B20_CHIPID:   // DS18B20
    case MAX31850_CHIPID:  // MAX31850
      uint16_t temp12 = (data[1] << 8) + data[0];
      if (temp12 > 2047) {
        temp12 = (~temp12) +1;
        sign = -1;
      }
      t = ConvertTemp(sign * temp12 * 0.0625);
      break;
    }
  }
  return (!isnan(t));
}

void Ds18x20Type(uint8_t sensor)
{
  strcpy_P(Ds18x20State.buffer, PSTR("DS18x20"));
  switch(Ds18x20State.address[Ds18x20State.index[sensor]][0]) {
    case DS18S20_CHIPID:
      strcpy_P(Ds18x20State.buffer, PSTR("DS18S20"));
      break;
    case DS18B20_CHIPID:
      strcpy_P(Ds18x20State.buffer, PSTR("DS18B20"));
      break;
    case MAX31850_CHIPID:
      strcpy_P(Ds18x20State.buffer, PSTR("MAX31850"));
      break;
  }
}

#define SHOW_JSON	0x01
#define SHOW_WEB	0x02
#define SHOW_MQTT	0x03
void Ds18x20Show(byte type, void *arg)
{
  char temperature[10];
  char stemp[100];
  float sum = 0;
  byte  counter = 0;

  byte dsxflg = 0;
  for (byte i = 0; i < Ds18x20Sensors(); i++) {
    if (!isnan(Ds18x20State.temperature[i])) {
	  float t = Ds18x20State.temperature[i];

	  sum += t;
	  counter++;

#ifdef USE_LCD1602A
	  LcdDataExchange.DS18B20_temperature = t;
#endif
      Ds18x20Type(i);
	  if (type) // SHOW_JSON or SHOW_WEB
        dtostrfd(t, Settings.flag.temperature_resolution, temperature);

	  if (type == SHOW_MQTT) {
		  BufferString	topic(stemp, sizeof(stemp));

		  topic.sprintf_P(FPSTR("%s_%d_temperature"), Ds18x20State.buffer, i+1);
		  MqttPublishSimple(topic.c_str(), t);

	  } else if (type == SHOW_JSON) {
        if (!dsxflg) {
		  mqtt_msg += F(", \"DS18x20\":{");
          stemp[0] = '\0';
        }
        dsxflg++;
		mqtt_msg.sprintf_P(F("%s\"DS%d\":{\"" D_TYPE "\":\"%s\", \"" D_ADDRESS "\":\"%s\", \"" D_TEMPERATURE "\":%s}"),
						   stemp, i +1, Ds18x20State.buffer, Ds18x20Addresses(i).c_str(), temperature);
		mqtt_msg += F(", ");
#ifdef USE_DOMOTICZ
        if (1 == dsxflg) {
          DomoticzSensor(DZ_TEMP, temperature);
        }
#endif  // USE_DOMOTICZ
#ifdef USE_WEBSERVER
      } else if (type == SHOW_WEB) {
        snprintf_P(stemp, sizeof(stemp), PSTR("%s-%d"), Ds18x20State.buffer, i +1);
        mqtt_msg.sprintf_P(FPSTR(HTTP_SNS_TEMP), stemp, temperature, TempUnit());
#endif  // USE_WEBSERVER
      }
    }
  }

  if (arg)
  {
	if (counter > 0 && !isnan(sum))
		*(float*)arg = sum / counter;
	else
		*(float*)arg = NAN;
  }

  if (type == SHOW_JSON && dsxflg)
		mqtt_msg += '}';
}

/*********************************************************************************************\
 * Interface
\*********************************************************************************************/

#define XSNS_05
boolean Xsns05(byte function, void *arg)
{
  boolean result = false;

  if (pin[GPIO_DSB] < 99) {
    switch (function) {
      case FUNC_XSNS_INIT:
        Ds18x20Init();
        break;
      case FUNC_XSNS_READ:
        Ds18x20Show(0, arg);
        break;
      case FUNC_XSNS_JSON_APPEND:
        Ds18x20Show(SHOW_JSON, NULL);
        break;
	  case FUNC_XSNS_MQTT_SIMPLE:
        Ds18x20Show(SHOW_MQTT, NULL);
		break;
#ifdef USE_WEBSERVER
      case FUNC_XSNS_WEB:
        Ds18x20Show(SHOW_WEB, NULL);
        break;
#endif  // USE_WEBSERVER
    }
  }
  return result;
}

#endif  // USE_DS18x20
