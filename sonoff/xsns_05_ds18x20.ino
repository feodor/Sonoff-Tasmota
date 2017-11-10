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

OneWire *ds = NULL;

uint8_t ds18x20_address[DS18X20_MAX_SENSORS][8];
uint8_t ds18x20_index[DS18X20_MAX_SENSORS];
uint8_t ds18x20_sensors = 0;
char ds18x20_types[9];


uint32_t ds18x20_tc_down_time = 0;
uint32_t ds18x20_tc_up_time = 0;
float    ds18x20_tc_duty_ratio = -1;

void Ds18x20Init()
{
  ds = new OneWire(pin[GPIO_DSB]);
}

void Ds18x20Search()
{
  uint8_t num_sensors=0;
  uint8_t sensor = 0;

  ds->reset_search();
  for (num_sensors = 0; num_sensors < DS18X20_MAX_SENSORS; num_sensors) {
    if (!ds->search(ds18x20_address[num_sensors])) {
      ds->reset_search();
      break;
    }
    // If CRC Ok and Type DS18S20, DS18B20 or MAX31850
    if ((OneWire::crc8(ds18x20_address[num_sensors], 7) == ds18x20_address[num_sensors][7]) &&
       ((ds18x20_address[num_sensors][0]==DS18S20_CHIPID) || (ds18x20_address[num_sensors][0]==DS18B20_CHIPID) || (ds18x20_address[num_sensors][0]==MAX31850_CHIPID))) {
      num_sensors++;
    }
  }
  for (byte i = 0; i < num_sensors; i++) {
    ds18x20_index[i] = i;
  }
  for (byte i = 0; i < num_sensors; i++) {
    for (byte j = i + 1; j < num_sensors; j++) {
      if (uint32_t(ds18x20_address[ds18x20_index[i]]) > uint32_t(ds18x20_address[ds18x20_index[j]])) {
        std::swap(ds18x20_index[i], ds18x20_index[j]);
      }
    }
  }
  ds18x20_sensors = num_sensors;
}

uint8_t Ds18x20Sensors()
{
  return ds18x20_sensors;
}

String Ds18x20Addresses(uint8_t sensor)
{
  char address[20];

  for (byte i = 0; i < 8; i++) {
    sprintf(address+2*i, "%02X", ds18x20_address[ds18x20_index[sensor]][i]);
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
  ds->select(ds18x20_address[ds18x20_index[sensor]]);
  ds->write(W1_READ_SCRATCHPAD); // Read Scratchpad

  for (byte i = 0; i < 9; i++) {
    data[i] = ds->read();
  }
  if (OneWire::crc8(data, 8) == data[8]) {
    switch(ds18x20_address[ds18x20_index[sensor]][0]) {
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

/********************************************************************************************/

void Ds18x20Type(uint8_t sensor)
{
  strcpy_P(ds18x20_types, PSTR("DS18x20"));
  switch(ds18x20_address[ds18x20_index[sensor]][0]) {
    case DS18S20_CHIPID:
      strcpy_P(ds18x20_types, PSTR("DS18S20"));
      break;
    case DS18B20_CHIPID:
      strcpy_P(ds18x20_types, PSTR("DS18B20"));
      break;
    case MAX31850_CHIPID:
      strcpy_P(ds18x20_types, PSTR("MAX31850"));
      break;
  }
}

void Ds18x20Show(boolean json)
{
  char temperature[10];
  char stemp[10];
  char ratio[10];
  float t;

  byte dsxflg = 0;
  for (byte i = 0; i < Ds18x20Sensors(); i++) {
    if (Ds18x20Read(i, t)) {           // Check if read failed
      Ds18x20Type(i);
      dtostrfd(t, Settings.flag.temperature_resolution, temperature);

      if (json) {
        if (!dsxflg) {
          snprintf_P(mqtt_data, sizeof(mqtt_data), PSTR("%s, \"DS18x20\":{"), mqtt_data);
          stemp[0] = '\0';
        }
        dsxflg++;
		if (ds18x20_tc_duty_ratio >= 0)
		{

			dtostrfd(ds18x20_tc_duty_ratio, Settings.flag.temperature_resolution, ratio);

        	snprintf_P(mqtt_data, sizeof(mqtt_data), PSTR("%s%s\"DS%d\":{\"" D_TYPE "\":\"%s\", \"" D_ADDRESS "\":\"%s\", \"" D_TEMPERATURE "\":%s, \"" D_RATIO "\":%s}"),
          			   mqtt_data, stemp, i +1, ds18x20_types, Ds18x20Addresses(i).c_str(), temperature, ratio);
        	strcpy(stemp, ", ");
		}
		else
		{
        	snprintf_P(mqtt_data, sizeof(mqtt_data), PSTR("%s%s\"DS%d\":{\"" D_TYPE "\":\"%s\", \"" D_ADDRESS "\":\"%s\", \"" D_TEMPERATURE "\":%s}"),
          			   mqtt_data, stemp, i +1, ds18x20_types, Ds18x20Addresses(i).c_str(), temperature);
        	strcpy(stemp, ", ");
		}
#ifdef USE_DOMOTICZ
        if (1 == dsxflg) {
          DomoticzSensor(DZ_TEMP, temperature);
        }
#endif  // USE_DOMOTICZ
#ifdef USE_WEBSERVER
      } else {
        snprintf_P(stemp, sizeof(stemp), PSTR("%s-%d"), ds18x20_types, i +1);
        snprintf_P(mqtt_data, sizeof(mqtt_data), HTTP_SNS_TEMP, mqtt_data, stemp, temperature, TempUnit());
#endif  // USE_WEBSERVER
      }
    }
  }
  if (json) {
    if (dsxflg) {
      snprintf_P(mqtt_data, sizeof(mqtt_data), PSTR("%s}"), mqtt_data);
    }
#ifdef USE_WEBSERVER
  } else {
    Ds18x20Search();      // Check for changes in sensors number
    Ds18x20Convert();     // Start Conversion, takes up to one second
#endif  // USE_WEBSERVER
  }
}

/*********************************************************************************************\
 * Interface
\*********************************************************************************************/

#define XSNS_05

boolean Xsns05(byte function)
{
  boolean result = false;

  if (pin[GPIO_DSB] < 99) {
    switch (function) {
      case FUNC_XSNS_INIT:
        Ds18x20Init();
        break;
      case FUNC_XSNS_PREP:
        Ds18x20Search();      // Check for changes in sensors number
        Ds18x20Convert();     // Start Conversion, takes up to one second
        break;
      case FUNC_XSNS_JSON_APPEND:
        Ds18x20Show(1);
        break;
#ifdef USE_WEBSERVER
      case FUNC_XSNS_WEB:
        Ds18x20Show(0);
        break;
#endif  // USE_WEBSERVER
    }
  }
  return result;
}

/*********************************************************************************************\
 * Temperature Control
\*********************************************************************************************/

#ifdef TEMPERATURE_CONTROL
void
ActTemperatureControlDs18x20()
{
    if (!Settings.enable_temperature_control)
        return;
    if (Ds18x20Sensors() == 0)
        return;

    uint8_t device = 1;
    bool is_power, should_poweron = true;
	float current_temperature = NAN, total = 0;
	byte  counter = 0;
	float total_time, on_time;

	for (byte i = 0; i < Ds18x20Sensors(); i++) 
	{
        if (Ds18x20Read(i, current_temperature))
		{
			total += current_temperature;
			counter++;
		}
	}

    if (counter > 0)
		current_temperature = total / counter;
	else
		current_temperature = NAN;

    is_power = ((power >> (device - 1)) & 0x01) ? true : false;

    if (isnan(current_temperature) ||
        isnan(Settings.destination_temperature) ||
        isnan(Settings.delta_temperature))
    {
        should_poweron = !Settings.inverted_temperature_control;
    }
    else if (current_temperature > Settings.destination_temperature + Settings.delta_temperature)
    {
        should_poweron = Settings.inverted_temperature_control;
    }
    else if (current_temperature < Settings.destination_temperature)
    {
        should_poweron = !Settings.inverted_temperature_control;
    }
    else
    {
        should_poweron = is_power;
    }

    if (should_poweron != is_power)
	{
        ExecuteCommandPower(device, should_poweron ? 1 : 0);

		if (should_poweron == !Settings.inverted_temperature_control)
		{	
			// turn on even with inverted logic
			ds18x20_tc_up_time = LocalTime();
		}
		else
		{
			if (ds18x20_tc_down_time > 0)
			{
				total_time = LocalTime() - ds18x20_tc_down_time;
				on_time = LocalTime() - ds18x20_tc_up_time;

				ds18x20_tc_duty_ratio = 100.0 * on_time/total_time;
			}
			ds18x20_tc_down_time = LocalTime();
		}
	}
}
#endif //TEMPERATURE_CONTROL

#endif  // USE_DS18x20
