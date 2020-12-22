/*
* MYS_v11_MySensorNode.ino - Firmware for Mys v1.1 board temperature and humidity sensor Node with nRF24L01+ module
*
* Copyright 2014 Tomas Hozza <thozza@gmail.com>
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation; either version 2 of the License.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program; if not, see <http://www.gnu.org/licenses/>.
*
* Authors:
* Tomas Hozza <thozza@gmail.com>
* Richard Clarke <richard.ns@clarke.biz>
*
* MySensors library - http://www.mysensors.org/
* Mys v1.1 -http://www.sa2avr.se/mys-1-1/
* nRF24L01+ spec - https://www.sparkfun.com/datasheets/Wireless/Nordic/nRF24L01P_Product_Specification_1_0.pdf
*
Hardware Connections (Breakoutboard to Arduino):
 -VCC = 3.3V
 -GND = GND
 -SDA = A4 (use inline 10k resistor if your board is 5V)
 -SCL = A5 (use inline 10k resistor if your board is 5V)

Mys_v1.1 board compatible with Arduino PRO Mini 3.3V@8MHz

System Clock  = 8MHz

Information on porting a 1.5.x compatible sketch to 2.0.x

https://forum.mysensors.org/topic/4276/converting-a-sketch-from-1-5-x-to-2-0-x/2

 */

// Enable debug prints to serial monitor
//#define MY_DEBUG
//#define DEBUG_RCC

// Enable and select radio type attached
#define MY_RADIO_RF24
//#define MY_RADIO_RFM69

#define MY_NODE_ID 23
/*Makes this static so won't try and find another parent if communication with
gateway fails*/
#define MY_PARENT_NODE_ID 0
#define MY_PARENT_NODE_IS_STATIC

/**
 * @def MY_TRANSPORT_WAIT_READY_MS
 * @brief Timeout in ms until transport is ready during startup, set to 0 for no timeout
 */
#define MY_TRANSPORT_WAIT_READY_MS (1000)

/*These are actually the default pins expected by the MySensors framework.
* This means we can use the default constructor without arguments when
* creating an instance of the Mysensors class. Other defaults will include
* transmitting on channel 76 with a data rate of 250kbps.
* MyGW1 = channel 76
* MyGW2 = channel 100
*/
#define MY_RF24_CE_PIN 9
#define MY_RF24_CS_PIN 10
#define MY_RF24_CHANNEL 76


#define RELAY_PIN 6  // Arduino Digital I/O pin number for first relay (second on pin+1 etc)
#define NUMBER_OF_RELAYS 1 // Total number of attached relays
#define RELAY_ON 1  // GPIO value to write to turn on attached relay
#define RELAY_OFF 0 // GPIO value to write to turn off attached relay

#include <MySensors.h> 
#include <stdint.h>
#include <math.h> 
#include "SparkFunHTU21D.h"
#include <Wire.h>
#include <BatterySense.hpp>

#define TEMP_HUM_SENSOR 1

// Sleep time between sensor updates (in milliseconds)
//static const uint32_t DAY_UPDATE_INTERVAL_MS = 30000;
//static const uint32_t DAY_UPDATE_INTERVAL_MS = 2500;

static const uint32_t DAY_UPDATE_INTERVAL_MS = 10000;

static const float temperature_fan_on = 27.5;
static const float temperature_fan_off = 24.0;


enum child_id_t
{
  CHILD_ID_HUMIDITY,
  CHILD_ID_TEMP,
  CHILD_ID_UV,
  CHILD_ID_VOLTAGE,
  CHILD_ID_EXT_VOLTAGE,
  CHILD_ID_RELAY_1
};

/*****************************/
/********* FUNCTIONS *********/
/*****************************/

//Create an instance of the sensor objects


#if TEMP_HUM_SENSOR
const char sketchString[] = "mys_v11-temp_fan_control";
HTU21D myHTU21D;
MyMessage msgHum(CHILD_ID_HUMIDITY, V_HUM);
MyMessage msgTemp(CHILD_ID_TEMP, V_TEMP);
float readHTU21DTemperature(bool force);
void readHTU21DHumidity(bool force);
#endif

uint8_t fan1_control_pin = RELAY_PIN;

MyMessage msgFan(CHILD_ID_RELAY_1,V_STATUS);
MyMessage msgVolt(CHILD_ID_VOLTAGE, V_VOLTAGE);

bool fanState = false;

BatteryLevel batt;
/**********************************/
/********* IMPLEMENTATION *********/
/**********************************/
/*If you need to do initialization before the MySensors library starts up,
define a before() function */
void before()
{
  
    // Then set relay pins in output mode
    pinMode(fan1_control_pin, OUTPUT);
    // Set relay to last known state (using eeprom storage)
    digitalWrite(fan1_control_pin, RELAY_OFF);
    fanState = false;
  
}

/*To handle received messages, define the following function in your sketch*/
void receive(const MyMessage &message)
{
  /*Handle incoming message*/
}

/* If your node requests time using requestTime(). The following function is
used to pick up the response*/
void receiveTime(unsigned long ts)
{
}

/*You can still use setup() which is executed AFTER mysensors has been
initialised.*/
void setup()
{
  
  Wire.begin();
  
  #if TEMP_HUM_SENSOR
  myHTU21D.begin();
  #endif
  Serial.begin(9600);
  
}

void presentation()
{
   // Send the sketch version information to the gateway and Controller
  sendSketchInfo(sketchString, "0.6");
  // Register all sensors to gateway (they will be created as child devices)
  present(CHILD_ID_VOLTAGE, S_MULTIMETER);

  // Register all sensors to gw (they will be created as child devices)
  present(CHILD_ID_RELAY_1, S_BINARY);
  
#if TEMP_HUM_SENSOR
  present(CHILD_ID_HUMIDITY, S_HUM);
  present(CHILD_ID_TEMP, S_TEMP);
#endif

   
}


void loop()
{

  uint32_t update_interval_ms = DAY_UPDATE_INTERVAL_MS;
  float current_temperature;
  uint16_t battLevel = batt.getVoltage();
  send(msgVolt.set(battLevel,1));

#if TEMP_HUM_SENSOR
  current_temperature = readHTU21DTemperature(true);
  readHTU21DHumidity(true);
#endif

  if (current_temperature >= temperature_fan_on and fanState == false)
  {
    digitalWrite(fan1_control_pin, RELAY_ON);
    fanState = true;
    
    #if DEBUG_RCC
    Serial.print("Switching fan on");
    Serial.println();
    #endif
  }
  
  if (current_temperature <= temperature_fan_off and fanState == true)
  {
    digitalWrite(fan1_control_pin, RELAY_OFF);
    fanState = false;
    send(msgFan.set(false));
    #if DEBUG_RCC
    Serial.print("Switching fan off");
    Serial.println();
    #endif
  }

  send(msgFan.set(fanState));

  sleep(update_interval_ms); 
  
  
} //end loop



#if TEMP_HUM_SENSOR
float readHTU21DTemperature(bool force)
{
  static float lastTemp = 0;

  if (force)
  {
   lastTemp = -100;
  }
  float temp = myHTU21D.readTemperature();

  if(lastTemp != temp)
  {
    send(msgTemp.set(temp,1));
    lastTemp = temp;
    #ifdef DEBUG_RCC
    Serial.print(" Temperature:");
    Serial.print(temp, 1);
    Serial.print("C");
    Serial.println();
    #endif
  }

  return temp;
}

void readHTU21DHumidity(bool force)
{
  static float lastHumidity = 0;

  if (force)
  {
    lastHumidity = -100;
  }
  float humd = myHTU21D.readHumidity();

  if(lastHumidity != humd)
  {
    send(msgHum.set(humd,1));
    lastHumidity = humd;
    #ifdef DEBUG_RCC
    Serial.print(" Humidity:");
    Serial.print(humd, 1);
    Serial.print("%");
    Serial.println();
    #endif
  }
}

#endif


