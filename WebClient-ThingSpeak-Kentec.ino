/*
  SENSOR STATUS DISPLAY

  https://github.com/Andy4495/ConnectedStatusMonitor

  Status display for various sensor data readings downloaded from ThingSpeak IoT Platform.
  Designed specifically for use with TM4C129 Connected LaunchPad
  and Kentec Touch Display BoosterPack (SPI)
  Initial web connection code based on "Web client" example sketch by David A. Mellis.
  NTP functions are from the Arduino Time library

  07/10/2018 - A.T. - Initial updates
  08/17/2018 - A.T. - Functional display code for weather station, workshop and slim temps, and garage door status.
  08/19/2018 - A.T. - Add state machine to turn on display depending on state of light sensor on pin A13
  08/20/2018 - A.T. - Add NTP time and date to status display
  08/22/2018 - A.T. - Add automatic DST support. Other display cleanup.
  08/23/2018 - A.T. - Fix DST to switch at 2:00 AM (instead of midnight)
  08/24/2018 - A.T. - Minor updates: display time before sensor readings (since it is faster),
                      use #define for DST effective hour, flash LED1 while display backlight off,
                      display welcome message in larger (scaled) font,
                      enable ethernet link and activity LEDs.
  08/24/2018 - A.T. - Ethernet LEDs disabled by default, turned on with PUSH1 at reset.
                      Use "time.nist.gov" instead of specific IP for time server.
  10/02/2018 - A.T. - Replace Workshop display with Pond sensor. Update thresholds for value colors.
  10/10/2018 - A.T. - Change pond battery threshold (using 2AAs instead of LiPo).
  01/30/2019 - A.T. - Change pond battery threshold (using 3xAAs through a TPS715A33 3.3V regulator).
                    - Fix display of negative temps.
                    - Increased time to wait for ThingSpeak response.
  11/04/2019 - A.T. - Add Workshop temp display.
                    - Remove pond battery display (since it is AC powered)
                    - Add battery level warning for Workshop sensor
  11/07/2019 - A.T. - Add support for VFD display
                    - Moved light sensor to pin 68/A19
  11/11/2019 - A.T. - Update VFD message text.
  11/14/2019 - A.T. - Adjust VFD display timing.
                      Change lipo low batt level to 3.8V (lasts about 2.5 days at this point)
  11/21/2019 - A.T. - Turn sensor 4 and 5 voltage reading red if less than 2 days remaining.
                      (Sensors 4 and 5 now send charge time remaining instead of millis).
  12/01/2019 - A.T. - Add support for displaying Pressure history on SparkFun OLED (parallel mode).
                      -- Query ThingSpeak for pressure data even when display is off
                    - Don't display time if no response/invalid response from time server
                    - Updated low battery thresholds
                    - Remove check for PUSH1 -- ethernet status LEDs always disabled.
  01/19/2020 - A.T. - Fix a case where invalid time needs to be cleared from display.
  01/21/2020 - A.T. - Change LiPo low battery threshold to 3.75V
  02/13/2020 - A.T. - Update to use new Futaba VFD library naming.
  04/27/2020 - A.T. - Change lipo time remaining threshold to zero, since the value does not seem
                      to correlate with reality in this application. Tweaked the lipo battery voltage threshold.
  09/01/2020 - A.T. - Add support for turtle pond temperature. Rename "Pond" to "Fish" to distinguish the two sensors.
  09/14/2020 - A.T. - Change "Gargoyle" to "Sensor5"

  *** IMPORTANT ***
    The Kentec_35_SPI library has an issue where the _getRawTouch() function called in the begin() method
    can get stuck in an endless loop. Therefore, for proper operation of the display, it is necessary to
    comment out the call to _getRaw_Touch() in the begin() method in the file Screen_K35_SPI.cpp in the
    Kentec_35_SPI library.
  * ***************
    This sketch uses a modified version of SparkFun's MicroOLED library so that it compiles with Energia
    and works with MSP and TM4C129 processors:  https://github.com/Andy4495/SparkFun_Micro_OLED_Arduino_Library
  * ***************


  *** Future improvements:
    Color a sensor value (or the label) yellow or red if an update has not been received for more than X minutes
    Add a pixel to y-coordinates to allow for hanging comma space (otherwise comma touches next line)
    Update lux string to include commas
    Deal with JSON parse failure -- maybe just display last good value (i.e., no display indication of bad JSON)

*/
#include <ArduinoJson.h>             // From https://arduinojson.org/
#include <SPI.h>
#include <Ethernet.h>
#include <EthernetUdp.h>
#include <TimeLib.h>                 // From https://github.com/PaulStoffregen/Time
#include "dst.h"


#include "Screen_K35_SPI.h"
Screen_K35_SPI myScreen;
// ThingSpeakKeys.h is not included in the code distribution, since it contains private key info
// This file should #define the following:
// #define LAUNCHPAD_MAC {<comma-separated 8-byte MAC address of your ethernet card>}
// Plus #defines for each of your ThingSpeak feed keys and channel IDs
#include "ThingSpeakKeys.h"
#include "Coordinates.h"

// Enter a MAC address for your controller below.
// Newer Ethernet shields have a MAC address printed on a sticker on the shield
// byte mac[] = {  0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };
byte mac[] = LAUNCHPAD_MAC; // Defined in ThingSpeakKeys.h
const char* server = "api.thingspeak.com";

const char* timeServer = "time.nist.gov";  // Automatically resolves to nearest active server
// IPAddress timeServer(132.163.97.1); // time.nist.gov

// Use Standard Time for Time Zone. DST is corrected when printing.
// Time zones are #defined in dst.h
int timeZone = STANDARD_TZ;   // Use standard time, DST correction is done later

time_t t;

uint16_t timeColor;

EthernetUDP Udp;
unsigned int localPort = 8888;  // local port to listen for UDP packets

const int NTP_PACKET_SIZE = 48; // NTP time is in the first 48 bytes of message
byte packetBuffer[NTP_PACKET_SIZE]; //buffer to hold incoming & outgoing packets

char receiveBuffer[1024] = {};
char printBuffer[32] = {};
char prevPrintBuffer[32] = {};

// Based on the Arduinojson.org site, this is the size needed for the buffer for the longest message (weather station)
// (Actually, the final term could be "+ 542", but I changed it to "+ 600" just to be on the safe side.
const size_t bufferSize = JSON_ARRAY_SIZE(1) + JSON_OBJECT_SIZE(2) + JSON_OBJECT_SIZE(10) + JSON_OBJECT_SIZE(16) + 600;

// Initialize the Ethernet client library
// with the IP address and port of the server
// that you want to connect to (port 80 is default for HTTP):
EthernetClient client;

Layout layout;

//----- Sensor Reading Global Storage -----//
#define TEMPSIZE  6
#define LUXSIZE   9
#define RHSIZE    5
#define PSIZE     6
#define GDSIZE    7
#define BATTSIZE  6
#define TADSIZE  20
#define TIMESIZE 21
char outdoorTemp[TEMPSIZE];
char prevOutdoorTemp[TEMPSIZE];
char outdoorLux[LUXSIZE];
char prevOutdoorLux[LUXSIZE];
char outdoorRH[RHSIZE];
char prevOutdoorRH[RHSIZE];
char outdoorP[PSIZE];
char prevOutdoorP[PSIZE];
char slimTemp[TEMPSIZE];
char prevSlimTemp[TEMPSIZE];
char fishTemp[TEMPSIZE];
char prevFishTemp[TEMPSIZE];
char turtleTemp[TEMPSIZE];
char prevTurtleTemp[TEMPSIZE];
char sensor5Temp[TEMPSIZE];
char prevSensor5Temp[TEMPSIZE];
char workshopTemp[TEMPSIZE];
char prevWorkshopTemp[TEMPSIZE];
char garageDoor[GDSIZE];
char prevGarageDoor[GDSIZE];
char outdoorBatt[BATTSIZE];
char prevOutdoorBatt[BATTSIZE];
char slimBatt[BATTSIZE];
char prevSlimBatt[BATTSIZE];
char sensor5Batt[BATTSIZE];
char prevSensor5Batt[BATTSIZE];
char timeAndDate[TADSIZE];
char prevTimeAndDate[TADSIZE];
char weatherTime[TIMESIZE];
char slimTime[TIMESIZE];
char sensor5Time[TIMESIZE];
char pondTime[TIMESIZE];
char workshopTime[TIMESIZE];
char garageTime[TIMESIZE];

#define LIGHT_SENSOR_PIN           68
#define LIGHT_SENSOR_ADC          A19
#define LIGHT_SENSOR_THRESHOLD   2000    // Based on 10K resistor and cheap photoresistor voltage divider and 12-bit ADC (4096 max value)
#define LIGHTS_ON_SLEEP_TIME    30000
#define LIGHTS_OFF_SLEEP_TIME    5000
#define BACKLIGHT_PIN              40
#define SLEEPING_STATUS_LED      PN_1    // Flash when display backlight is off to show the unit is active
#define LIPO_LO_BATT_LEVEL       3780   // There are still a few days left at this level.
#define LIPO_LO_TIME_REMAIN         0   // Value reported by Fuel Gauge drops too quickly. Setting to zero effectively disables this check.

// VFD support
#include <FutabaVFD162S.h>
#define VFD_CLOCK_PIN              74
#define VFD_DATA_PIN               73
#define VFD_RESET_PIN              72
#define VFD_BUFFER_EN              76    // Controls the EN pins on the CD40109 buffer
#define VFD_POWER_CONTROL          77    // HIGH == OFF
#define VFD_BRIGHTNESS            128
FutabaVFD162S vfd(VFD_CLOCK_PIN, VFD_DATA_PIN, VFD_RESET_PIN);
int vfd_loop_counter = 0; // Used for testing

// SparkFun OLED support
// This is a modified version of SparkFun's library so that it compiles with Energia and works with MSP and TM4C129 processors
#include <SFE_MicroOLED.h>           // https://github.com/Andy4495/SparkFun_Micro_OLED_Arduino_Library
// MicroOLED(uint8_t rst, uint8_t dc, uint8_t cs, uint8_t wr, uint8_t rd,
//           uint8_t d0, uint8_t d1, uint8_t d2, uint8_t d3,
//           uint8_t d4, uint8_t d5, uint8_t d6, uint8_t d7)
MicroOLED oled(35, 50, 79, 48, 58,
               12, 29, 49, 85, 86, 59, 51, 30);
/* Test data
  long p_hist[] = { 2902, 2902, 2902, 2902, 2903, 2903, 2903, 2903, 2903, 2903, 2903, 2904, 2904, 2904, 2905, 2905,
                 2904, 2905, 2905, 2905, 2905, 2905, 2905, 2906, 2906, 2906, 2907, 2907, 2907, 2907, 2909, 2908,
                 2908, 2908, 2909, 2909, 2909, 2908, 2909, 2909, 2909, 2910, 2910, 2911, 2911, 2911, 2911, 2911,
                 2911, 2912, 2912, 2912, 2912, 2913, 2913, 2913, 2913, 2914, 2914, 2914, 2914, 2914, 2915, 2915
               };
*/

// Circular buffer of Pressure readings.
// Initial value of 0xFEFE so only show partial display with limited data on startup
long p_hist[64];
unsigned int hist_position = 0;         // Current position in p_hist circular buffer
unsigned int hist_num_entries = 0;      // Number of valid entries in the history (used after reset until buffer is filled first time)
const unsigned int hist_len = 64;
long last_weather_entry_id = 0;  // Used to track whether we received new data
unsigned long lastPressureQuery = 0; // Track times between readings when displays are off
#define PRESSURE_READINGS_TIME 120000UL   // Time between readings: 2 minutes (2 min * 60 s/min * 1000 ms/s)

int  statusLEDstate = 0;

enum {LIGHTS_OFF, LIGHTS_TURNED_ON, LIGHTS_ON, LIGHTS_TURNED_OFF};
int lightSensorState = LIGHTS_OFF;

void setup() {

  vfdIOSetup();
  pinMode(LIGHT_SENSOR_PIN, INPUT);
  pinMode(SLEEPING_STATUS_LED, OUTPUT);

  /// This is just for checking button out of reset
  /// The rest of the time, this pin (85/PJ0) is used as data pin to OLED.
  pinMode(PUSH1, INPUT_PULLUP);

  // start the serial library:
  Serial.begin(9600);
  delay(1000);
  Serial.println("Starting the Sensor Monitor...");

  Serial.println("Initializing LCD...");
  myScreen.begin();
  Serial.println("...LCD Initialized");
  myScreen.setPenSolid(true);
  myScreen.setFontSolid(false);
  myScreen.setFontSize(2);
  myScreen.setOrientation(2);
  displayStartup();

  Serial.println("Initializing Ethernet...");
  // start the Ethernet connection:
  if (Ethernet.begin(mac) == 0) {
    Serial.println("Failed to configure Ethernet using DHCP.");
    myScreen.gText(24, 120, "Failed!", redColour, blackColour);
    myScreen.gText(24, 160, "Program halted.", redColour, blackColour);
    // no point in carrying on, so do nothing forevermore:
    for (;;)
      ;
  }

  Serial.println("Ethernet initialized...");
  // give the Ethernet shield a second to initialize:
  delay(1000);

  /* Disable this check since the I/O pin used for PUSH1 is used by the OLED
    // Turn on Ethernet status LEDs if PUSH1 is pressed at reboot
    // Otherwise, they default to off.
    if (digitalRead(PUSH1) == LOW) {
      Ethernet.enableLinkLed();
      Ethernet.enableActivityLed();
      Serial.println("Ethernet LINK and ACTIVITY LEDs enabled.");
    }
    else {
      Serial.println("Ethernet status LEDs disabled by default.");
      // Following code can be used to turn off Ethernet status LEDs (default)
      // GPIODirModeSet(ACTIVITY_LED_BASE, ACTIVITY_LED_PIN, GPIO_DIR_MODE_IN);
      // GPIODirModeSet(LINK_LED_BASE, LINK_LED_PIN, GPIO_DIR_MODE_IN);
    }
  */

  Serial.print("JsonBuffer size: ");
  Serial.println(bufferSize);
  Serial.println("Connecting to NTP....");

  //  DisplayCharacterMap(); // For testing

  // Initialze the "previous" strings to empty
  prevOutdoorTemp[0] = 0;
  prevOutdoorLux[0] = 0;
  prevOutdoorRH[0] = 0;
  prevOutdoorP[0] = 0;
  prevSlimTemp[0] = 0;
  prevWorkshopTemp[0] = 0;
  prevFishTemp[0] = 0;
  prevTurtleTemp[0] = 0;
  prevGarageDoor[0] = 0;
  prevOutdoorBatt[0] = 0;
  prevSlimBatt[0] = 0;
  prevTimeAndDate[0] = 0;

  Udp.begin(localPort);
  Serial.println("Setting up NTP...");
  setSyncProvider(getNtpTime);

  Serial.println("Setting up OLED.");
  oled.begin();     // Initialize the OLED
  oled.clear(PAGE); // Clear the display's internal memory
  oled.clear(ALL);  // Clear the library's display buffer
  oled.command(DISPLAYOFF); // Turn off the display until other displays are enabled
}

void loop()
{
  int lightSensor;

  lightSensor = analogRead(LIGHT_SENSOR_ADC);
  Serial.print("Light Sensor: ");
  Serial.println(lightSensor);

  switch (lightSensorState) {
    case LIGHTS_OFF:
      if (lightSensor > LIGHT_SENSOR_THRESHOLD) {
        lightSensorState = LIGHTS_TURNED_ON;
      }

      else {
        if (millis() - lastPressureQuery > PRESSURE_READINGS_TIME) {
          lastPressureQuery = millis();
          getPressure();
        }
        statusLEDstate = ~statusLEDstate;                     // Flash the LED
        digitalWrite(SLEEPING_STATUS_LED, statusLEDstate);
        digitalWrite(BACKLIGHT_PIN, 0);
        vfdOff();
        oled.clear(PAGE);
        oled.display();
        oled.command(DISPLAYOFF);
        delay(LIGHTS_OFF_SLEEP_TIME);
      }
      break;

    case LIGHTS_TURNED_ON:
      statusLEDstate = 0;
      digitalWrite(SLEEPING_STATUS_LED, statusLEDstate);
      myScreen.clear();
      digitalWrite(BACKLIGHT_PIN, 1);
      displayWelcome();
      displayTitles();
      vfdOn();
      oled.command(DISPLAYON);
      lightSensorState = LIGHTS_ON;
      break;

    case LIGHTS_ON:
      if (lightSensor > LIGHT_SENSOR_THRESHOLD) {
        getAndDisplayTime();
        getAndDisplayWeather();
        getAndDisplaySlim();
        getAndDisplaySensor5();
        getAndDisplayPond();
        getAndDisplayWorkshop();
        getAndDisplayGarage();
        Serial.println("Disconnecting. Waiting 30 seconds before next query. ");
        displayOLED();
        displayVFD();
        // The VFD display messages take 30 seconds, so no need for separate delay.
        //        delay(LIGHTS_ON_SLEEP_TIME);
      }
      else
      {
        lightSensorState = LIGHTS_OFF;
      }
      break;

    case LIGHTS_TURNED_OFF:   // State not needed
      Serial.println("Invalid State! Changing state to LIGHTS_OFF.");
      lightSensorState = LIGHTS_OFF;
      break;

    default:  // Should never get here
      Serial.println("Reached 'default' case in switch statement! Changing state to LIGHTS_OFF.");
      lightSensorState = LIGHTS_OFF;
      break;
  }

}

void GetThingSpeakChannel(EthernetClient* c, const char* chan, const char* key, int results)
{
  const int BUF_SIZE = 256;
  char buffer[BUF_SIZE];

  snprintf(buffer, BUF_SIZE, "GET /channels/%s/feeds.json?api_key=%s&results=%d", chan, key, results);
  /// Serial.println(buffer);
  c->println(buffer);
  c->println();
}

void GetThingSpeakField(EthernetClient* c, const char* chan, const char* key, const char* field, int results)
{
  const int BUF_SIZE = 256;
  char buffer[BUF_SIZE];

  snprintf(buffer, BUF_SIZE, "GET /channels/%s/field/%s.json?api_key=%s&results=%d", chan, field, key, results);
  /// Serial.println(buffer);
  c->println(buffer);
  c->println();
}

void getAndDisplayWeather() {

  DynamicJsonBuffer jsonBuffer(bufferSize);

  int i = 0;
  char c;

  uint16_t battColor;

  // if you get a connection, report back via serial:
  if (client.connect(server, 80)) {
    Serial.println("connected");
  }
  else {
    // if you didn't get a connection to the server:
    Serial.println("connection failed");
  }

  // Make a HTTP request for Weather Station
  GetThingSpeakChannel(&client, WEATHERSTATION_CHANNEL, WEATHERSTATION_KEY, 1);

  // Need to check for connection and wait for characters
  // Need to timeout after some time, but not too soon before receiving response
  // Initially just use delay(), but replace with improved code using millis()
  delay(750);

  while (client.connected()) {
    /// Add a timeout with millis()
    c = client.read();
    if (c != -1) receiveBuffer[i++] = c;
    if (i > sizeof(receiveBuffer) - 2) break;    // Leave a byte for the null terminator
  }

  receiveBuffer[i] = '\0';
  Serial.print("JSON received size: ");
  Serial.println(i);
  Serial.println("JSON buffer: ");
  Serial.println(receiveBuffer);
  Serial.println("");
  client.stop();

  JsonObject& root = jsonBuffer.parseObject(receiveBuffer);
  /*
    "Parsing Program" code generated from ArduinoJson Assistant at arduinojson.org:

    JsonObject& channel = root["channel"];
    long channel_id = channel["id"]; // 379945
    const char* channel_name = channel["name"]; // "Weather-Station"
    const char* channel_description = channel["description"]; // "Outdoor weather station using MSP430 LaunchPad and SENSORS BoosterPack. "
    const char* channel_latitude = channel["latitude"]; // "0.0"
    const char* channel_longitude = channel["longitude"]; // "0.0"
    const char* channel_field1 = channel["field1"]; // "Temp-BME280"
    const char* channel_field2 = channel["field2"]; // "Temp-TMP007-Ext"
    const char* channel_field3 = channel["field3"]; // "Temp-TMP007-Int"
    const char* channel_field4 = channel["field4"]; // "Temp-MSP430"
    const char* channel_field5 = channel["field5"]; // "RH-BME280"
    const char* channel_field6 = channel["field6"]; // "Pressure-BME280"
    const char* channel_field7 = channel["field7"]; // "Lux-OPT3001"
    const char* channel_field8 = channel["field8"]; // "Batt-Vcc"
    const char* channel_created_at = channel["created_at"]; // "2017-12-07T00:28:34Z"
    const char* channel_updated_at = channel["updated_at"]; // "2018-06-10T22:26:22Z"
    long channel_last_entry_id = channel["last_entry_id"]; // 90649
  */

  if (root.success()) {

    JsonObject& feeds0 = root["feeds"][0];

    const char* feeds0_created_at = feeds0["created_at"]; // "2018-06-10T22:26:23Z"
    long feeds0_entry_id = feeds0["entry_id"]; // 90649

    long Tf = strtol(feeds0["field3"], NULL, 10);
    long lux = strtol(feeds0["field7"], NULL, 10);
    long rh = strtol(feeds0["field5"], NULL, 10);
    long p = strtol(feeds0["field6"], NULL, 10);
    long wBatt = strtol(feeds0["field8"], NULL, 10);
    strncpy(weatherTime, feeds0_created_at, TIMESIZE - 1);
    weatherTime[TIMESIZE - 1] = '\0';                         // hard-code a null terminator at end of string

    Serial.println("Parsed JSON: ");
    Serial.print("Created at: ");
    Serial.println(feeds0_created_at);
    Serial.print("Entry ID: ");
    Serial.println(feeds0_entry_id);

    snprintf(outdoorTemp, TEMPSIZE, "%3i.%i", Tf / 10, abs(Tf) % 10);
    snprintf(outdoorLux, LUXSIZE, "%8i", lux);
    snprintf(outdoorRH, RHSIZE, "%2i.%i", rh / 10, rh % 10);
    snprintf(outdoorP, PSIZE, "%2i.%02i", p / 100, p % 100);
    snprintf(outdoorBatt, BATTSIZE, "%i.%03i", wBatt / 1000, wBatt % 1000);

    if (wBatt < 2600) battColor = redColour;
    else battColor = greenColour;

    if (feeds0_entry_id != last_weather_entry_id) {
      last_weather_entry_id = feeds0_entry_id;
      if (hist_num_entries < hist_len)  // If we haven't filled the buffer the first time after reset
      {
        p_hist[hist_num_entries++] = p;
      }
      else // Once buffer has filled, then use a circular buffer and track current postion with wraparound
      {
        p_hist[hist_position++] = p;
        if (hist_position == hist_len) hist_position = 0; // Wrap around if needed.
      }
    }
  }
  else
  {
    Serial.println("JSON parse failed.");
    snprintf(outdoorTemp, TEMPSIZE,  "  N/A");
    snprintf(outdoorLux, LUXSIZE, "     N/A");
    snprintf(outdoorRH, RHSIZE,       " N/A");
    snprintf(outdoorP, PSIZE,        "  N/A");
    snprintf(outdoorBatt, BATTSIZE,  "  N/A");
    strcpy(weatherTime, "     N/A");
    battColor = whiteColour;
  }

  myScreen.gText(layout.WeatherTempValue.x, layout.WeatherTempValue.y, prevOutdoorTemp, blackColour);
  myScreen.gText(layout.WeatherTempValue.x, layout.WeatherTempValue.y, outdoorTemp);
  strncpy(prevOutdoorTemp, outdoorTemp, TEMPSIZE);

  myScreen.gText(layout.WeatherLuxValue.x, layout.WeatherLuxValue.y, prevOutdoorLux, blackColour);
  myScreen.gText(layout.WeatherLuxValue.x, layout.WeatherLuxValue.y, outdoorLux);
  strncpy(prevOutdoorLux, outdoorLux, LUXSIZE);

  myScreen.gText(layout.WeatherRHValue.x, layout.WeatherRHValue.y, prevOutdoorRH, blackColour);
  myScreen.gText(layout.WeatherRHValue.x, layout.WeatherRHValue.y, outdoorRH);
  strncpy(prevOutdoorRH, outdoorRH, RHSIZE);

  myScreen.gText(layout.WeatherPValue.x, layout.WeatherPValue.y, prevOutdoorP, blackColour);
  myScreen.gText(layout.WeatherPValue.x, layout.WeatherPValue.y, outdoorP);
  strncpy(prevOutdoorP, outdoorP, PSIZE);

  myScreen.gText(layout.BattOutdoorValue.x, layout.BattOutdoorValue.y, prevOutdoorBatt, blackColour);
  myScreen.gText(layout.BattOutdoorValue.x, layout.BattOutdoorValue.y, outdoorBatt, battColor);
  strncpy(prevOutdoorBatt, outdoorBatt, BATTSIZE);

} // getAndDisplayWeather()

void getPressure() {

  DynamicJsonBuffer jsonBuffer(bufferSize);

  int i = 0;
  char c;

  // if you get a connection, report back via serial:
  if (client.connect(server, 80)) {
    Serial.println("connected");
  }
  else {
    // if you didn't get a connection to the server:
    Serial.println("connection failed");
  }

  // Make a HTTP request for Weather Station
  GetThingSpeakField(&client, WEATHERSTATION_CHANNEL, WEATHERSTATION_KEY, "field6", 1);

  // Need to check for connection and wait for characters
  // Need to timeout after some time, but not too soon before receiving response
  // Initially just use delay(), but replace with improved code using millis()
  delay(750);

  while (client.connected()) {
    /// Add a timeout with millis()
    c = client.read();
    if (c != -1) receiveBuffer[i++] = c;
    if (i > sizeof(receiveBuffer) - 2) break;    // Leave a byte for the null terminator
  }

  receiveBuffer[i] = '\0';
  Serial.print("JSON received size: ");
  Serial.println(i);
  Serial.println("JSON received (field6): ");
  Serial.println(receiveBuffer);
  Serial.println("");
  client.stop();

  JsonObject& root = jsonBuffer.parseObject(receiveBuffer);
  /*
    "Parsing Program" code generated from ArduinoJson Assistant at arduinojson.org:

    JsonObject& channel = root["channel"];
    long channel_id = channel["id"]; // 379945
    const char* channel_name = channel["name"]; // "Weather-Station"
    const char* channel_description = channel["description"]; // "Outdoor weather station using MSP430 LaunchPad and SENSORS BoosterPack. "
    const char* channel_latitude = channel["latitude"]; // "0.0"
    const char* channel_longitude = channel["longitude"]; // "0.0"
    const char* channel_field6 = channel["field6"]; // "Pressure-BME280"
    const char* channel_created_at = channel["created_at"]; // "2017-12-07T00:28:34Z"
    const char* channel_updated_at = channel["updated_at"]; // "2018-06-10T22:26:22Z"
    long channel_last_entry_id = channel["last_entry_id"]; // 90649
  */

  if (root.success()) {

    JsonObject& feeds0 = root["feeds"][0];

    const char* feeds0_created_at = feeds0["created_at"]; // "2018-06-10T22:26:23Z"
    long feeds0_entry_id = feeds0["entry_id"]; // 90649

    long p = strtol(feeds0["field6"], NULL, 10);

    Serial.println("Parsed JSON: ");
    Serial.print("Pressure: ");
    Serial.println(p);
    Serial.print("Created at: ");
    Serial.println(feeds0_created_at);
    Serial.print("Entry ID: ");
    Serial.println(feeds0_entry_id);

    if (feeds0_entry_id != last_weather_entry_id) {
      last_weather_entry_id = feeds0_entry_id;
      if (hist_num_entries < hist_len)  // If we haven't filled the buffer the first time after reset
      {
        p_hist[hist_num_entries++] = p;
      }
      else // Once buffer has filled, then use a circular buffer and track current postion with wraparound
      {
        p_hist[hist_position++] = p;
        if (hist_position == hist_len) hist_position = 0; // Wrap around if needed.
      }
    }
    Serial.print("hist_num_entries: ");
    Serial.println(hist_num_entries);
    Serial.print("hist_position: ");
    Serial.println(hist_position);
  }
  else
  {
    Serial.println("JSON parse failed.");
  }
} // getPressure()


void getAndDisplaySlim() {

  DynamicJsonBuffer jsonBuffer(bufferSize);

  int i = 0;
  char c;

  uint16_t tempColor, battColor;

  // if you get a connection, report back via serial:
  if (client.connect(server, 80)) {
    Serial.println("connected");
  }
  else {
    // if you didn't get a connection to the server:
    Serial.println("connection failed");
  }

  // Make a HTTP request for Slim's sensor
  GetThingSpeakChannel(&client, SLIMTEMP_CHANNEL, SLIMTEMP_KEY, 1);

  // Need to check for connection and wait for characters
  // Need to timeout after some time, but not too soon before receiving response
  // Initially just use delay(), but replace with improved code using millis()
  delay(750);

  while (client.connected()) {
    /// Add a timeout with millis()
    c = client.read();
    if (c != -1) receiveBuffer[i++] = c;
    if (i > sizeof(receiveBuffer) - 2) break;    // Leave a byte for the null terminator
  }

  receiveBuffer[i] = '\0';
  Serial.println("JSON received: ");
  Serial.println(receiveBuffer);
  Serial.println("");
  client.stop();

  JsonObject& root = jsonBuffer.parseObject(receiveBuffer);
  /*
    "Parsing Program" code generated from ArduinoJson Assistant at arduinojson.org:

    JsonObject& channel = root["channel"];
    long channel_id = channel["id"]; // 412285
    const char* channel_name = channel["name"]; // "Slim's Temp"
    const char* channel_description = channel["description"]; // "Slim's temperature sensor. "
    const char* channel_latitude = channel["latitude"]; // "0.0"
    const char* channel_longitude = channel["longitude"]; // "0.0"
    const char* channel_field1 = channel["field1"]; // "Temp"
    const char* channel_field2 = channel["field2"]; // "Vcc"
    const char* channel_field3 = channel["field3"]; // "Loops"
    const char* channel_field4 = channel["field4"]; // "Millis"
    const char* channel_field5 = channel["field5"]; // "RSSI"
    const char* channel_field6 = channel["field6"]; // "LQI"
    const char* channel_created_at = channel["created_at"]; // "2018-01-26T17:19:35Z"
    const char* channel_updated_at = channel["updated_at"]; // "2018-08-17T02:18:28Z"
    long channel_last_entry_id = channel["last_entry_id"]; // 165702
  */

  if (root.success()) {

    JsonObject& feeds0 = root["feeds"][0];
    const char* feeds0_created_at = feeds0["created_at"]; // "2018-06-10T22:26:23Z"
    long feeds0_entry_id = feeds0["entry_id"]; // 90649

    long Tslim = strtol(feeds0["field1"], NULL, 10);
    long sBatt = strtol(feeds0["field2"], NULL, 10);
    strncpy(slimTime, feeds0_created_at, TIMESIZE - 1);
    slimTime[TIMESIZE - 1] = '\0';                         // hard-code a null terminator at end of string

    Serial.println("Parsed JSON: ");
    Serial.print("Created at: ");
    Serial.println(feeds0_created_at);
    Serial.print("Entry ID: ");
    Serial.println(feeds0_entry_id);

    if (Tslim < 800) tempColor = redColour;
    else tempColor = greenColour;

    if (sBatt < 2300) battColor = redColour;
    else battColor = greenColour;

    snprintf(slimTemp, TEMPSIZE, "%3i.%i", Tslim / 10, abs(Tslim) % 10);
    snprintf(slimBatt, BATTSIZE, "%i.%03i", sBatt / 1000, sBatt % 1000);
  }
  else
  {
    Serial.println("JSON parse failed.");
    snprintf(slimTemp, TEMPSIZE, "  N/A");
    snprintf(slimBatt, BATTSIZE, "  N/A");
    strcpy(slimTime, "     N/A");
    battColor = whiteColour;
    tempColor = whiteColour;
  }

  myScreen.gText(layout.SlimTempValue.x, layout.SlimTempValue.y, prevSlimTemp, blackColour);
  myScreen.gText(layout.SlimTempValue.x, layout.SlimTempValue.y, slimTemp, tempColor);
  strncpy(prevSlimTemp, slimTemp, TEMPSIZE);

  myScreen.gText(layout.BattSlimValue.x, layout.BattSlimValue.y, prevSlimBatt, blackColour);
  myScreen.gText(layout.BattSlimValue.x, layout.BattSlimValue.y, slimBatt, battColor);
  strncpy(prevSlimBatt, slimBatt, BATTSIZE);

} // getAndDisplaySlim()

void getAndDisplaySensor5() {

  DynamicJsonBuffer jsonBuffer(bufferSize);

  int i = 0;
  char c;

  uint16_t tempColor, battColor;

  // if you get a connection, report back via serial:
  if (client.connect(server, 80)) {
    Serial.println("connected");
  }
  else {
    // if you didn't get a connection to the server:
    Serial.println("connection failed");
  }

  // Make a HTTP request for Slim's sensor
  GetThingSpeakChannel(&client, TEMP5_CHANNEL, TEMP5_KEY, 1);

  // Need to check for connection and wait for characters
  // Need to timeout after some time, but not too soon before receiving response
  // Initially just use delay(), but replace with improved code using millis()
  delay(750);

  while (client.connected()) {
    /// Add a timeout with millis()
    c = client.read();
    if (c != -1) receiveBuffer[i++] = c;
    if (i > sizeof(receiveBuffer) - 2) break;    // Leave a byte for the null terminator
  }

  receiveBuffer[i] = '\0';
  Serial.println("JSON received: ");
  Serial.println(receiveBuffer);
  Serial.println("");
  client.stop();

  JsonObject& root = jsonBuffer.parseObject(receiveBuffer);
  /*
    "Parsing Program" code generated from ArduinoJson Assistant at arduinojson.org:

    JsonObject& channel = root["channel"];
    long channel_id = channel["id"]; // 412284
    const char* channel_name = channel["name"]; // "Indoor Temp 5"
    const char* channel_description = channel["description"]; // "Indoor Temp Sensor #5"
    const char* channel_latitude = channel["latitude"]; // "0.0"
    const char* channel_longitude = channel["longitude"]; // "0.0"
    const char* channel_field1 = channel["field1"]; // "Temp"
    const char* channel_field2 = channel["field2"]; // "Vcc"
    const char* channel_field3 = channel["field3"]; // "Loops"
    const char* channel_field4 = channel["field4"]; // "Millis"
    const char* channel_field5 = channel["field5"]; // "RSSI"
    const char* channel_field6 = channel["field6"]; // "LQI"
    const char* channel_created_at = channel["created_at"]; // "2018-01-26T17:18:49Z"
    const char* channel_updated_at = channel["updated_at"]; // "2018-09-18T08:44:13Z"
    long channel_last_entry_id = channel["last_entry_id"]; // 243115
  */

  if (root.success()) {

    JsonObject& feeds0 = root["feeds"][0];
    const char* feeds0_created_at = feeds0["created_at"]; // "2018-06-10T22:26:23Z"
    long feeds0_entry_id = feeds0["entry_id"]; // 90649

    long T5 = strtol(feeds0["field1"], NULL, 10);
    long B5 = strtol(feeds0["field2"], NULL, 10);
    unsigned long remain5 = strtoul(feeds0["field4"], NULL, 10);
    strncpy(sensor5Time, feeds0_created_at, TIMESIZE - 1);
    sensor5Time[TIMESIZE - 1] = '\0';                         // hard-code a null terminator at end of string

    Serial.println("Parsed JSON: ");
    Serial.print("Created at: ");
    Serial.println(feeds0_created_at);
    Serial.print("Entry ID: ");
    Serial.println(feeds0_entry_id);

    if (T5 > 850) tempColor = redColour;
    else tempColor = greenColour;

    if ( (B5 < LIPO_LO_BATT_LEVEL) || (remain5 < LIPO_LO_TIME_REMAIN) ) battColor = redColour;
    else battColor = greenColour;

    snprintf(sensor5Temp, TEMPSIZE, "%3i.%i", T5 / 10, abs(T5) % 10);
    snprintf(sensor5Batt, BATTSIZE, "%i.%03i", B5 / 1000, B5 % 1000);
  }
  else
  {
    Serial.println("JSON parse failed.");
    snprintf(sensor5Temp, TEMPSIZE, "  N/A");
    snprintf(sensor5Batt, BATTSIZE, "  N/A");
    strcpy(sensor5Time, "     N/A");
    battColor = whiteColour;
    tempColor = whiteColour;
  }

  myScreen.gText(layout.Sensor5TempValue.x, layout.Sensor5TempValue.y, prevSensor5Temp, blackColour);
  myScreen.gText(layout.Sensor5TempValue.x, layout.Sensor5TempValue.y, sensor5Temp, tempColor);
  strncpy(prevSensor5Temp, sensor5Temp, TEMPSIZE);

  myScreen.gText(layout.BattSensor5Value.x, layout.BattSensor5Value.y, prevSensor5Batt, blackColour);
  myScreen.gText(layout.BattSensor5Value.x, layout.BattSensor5Value.y, sensor5Batt, battColor);
  strncpy(prevSensor5Batt, sensor5Batt, BATTSIZE);

} // getAndDisplaySensor5()

void getAndDisplayWorkshop() {

  DynamicJsonBuffer jsonBuffer(bufferSize);

  int i = 0;
  char c;

  uint16_t battColor;

  // if you get a connection, report back via serial:
  if (client.connect(server, 80)) {
    Serial.println("connected");
  }
  else {
    // if you didn't get a connection to the server:
    Serial.println("connection failed");
  }

  // Make a HTTP request for Slim's sensor
  GetThingSpeakChannel(&client, TEMP4_CHANNEL, TEMP4_KEY, 1);

  // Need to check for connection and wait for characters
  // Need to timeout after some time, but not too soon before receiving response
  // Initially just use delay(), but replace with improved code using millis()
  delay(750);

  while (client.connected()) {
    /// Add a timeout with millis()
    c = client.read();
    if (c != -1) receiveBuffer[i++] = c;
    if (i > sizeof(receiveBuffer) - 2) break;    // Leave a byte for the null terminator
  }

  receiveBuffer[i] = '\0';
  Serial.println("JSON received: ");
  Serial.println(receiveBuffer);
  Serial.println("");
  client.stop();

  JsonObject& root = jsonBuffer.parseObject(receiveBuffer);

  if (root.success()) {

    JsonObject& feeds0 = root["feeds"][0];
    const char* feeds0_created_at = feeds0["created_at"]; // "2018-06-10T22:26:23Z"
    long feeds0_entry_id = feeds0["entry_id"]; // 90649

    long T4 = strtol(feeds0["field1"], NULL, 10);
    long B4 = strtol(feeds0["field2"], NULL, 10);
    long remain4 = strtol(feeds0["field4"], NULL, 10);
    strncpy(workshopTime, feeds0_created_at, TIMESIZE - 1);
    workshopTime[TIMESIZE - 1] = '\0';                         // hard-code a null terminator at end of string

    Serial.println("Parsed JSON: ");
    Serial.print("Created at: ");
    Serial.println(feeds0_created_at);
    Serial.print("Entry ID: ");
    Serial.println(feeds0_entry_id);

    if ( (B4 < LIPO_LO_BATT_LEVEL) || (remain4 < LIPO_LO_TIME_REMAIN))
      battColor = redColour;
    else
      battColor = blackColour;

    snprintf(workshopTemp, TEMPSIZE, "%3i.%i", T4 / 10, abs(T4) % 10);
  }
  else
  {
    Serial.println("JSON parse failed.");
    snprintf(workshopTemp, TEMPSIZE, "  N/A");
    strcpy(workshopTime, "     N/A");
  }

  myScreen.gText(layout.WorkshopTempValue.x, layout.WorkshopTempValue.y, prevWorkshopTemp, blackColour);
  myScreen.gText(layout.WorkshopTempValue.x, layout.WorkshopTempValue.y, workshopTemp, whiteColour);
  myScreen.gText(layout.WorkshopLoBat.x, layout.WorkshopLoBat.y, WorkshopLoBat, battColor);
  strncpy(prevWorkshopTemp, workshopTemp, TEMPSIZE);

} // getAndDisplayWorkshop()

void getAndDisplayPond() {

  DynamicJsonBuffer jsonBuffer(bufferSize);

  int i = 0;
  char c;

  // if you get a connection, report back via serial:
  if (client.connect(server, 80)) {
    Serial.println("connected");
  }
  else {
    // if you didn't get a connection to the server:
    Serial.println("connection failed");
  }

  // Make a HTTP request for Pond sensor
  GetThingSpeakChannel(&client, POND_CHANNEL, POND_KEY, 1);

  // Need to check for connection and wait for characters
  // Need to timeout after some time, but not too soon before receiving response
  // Initially just use delay(), but replace with improved code using millis()
  delay(750);

  while (client.connected()) {
    /// Add a timeout with millis()
    c = client.read();
    if (c != -1) receiveBuffer[i++] = c;
    if (i > sizeof(receiveBuffer) - 2) break;    // Leave a byte for the null terminator
  }

  receiveBuffer[i] = '\0';
  Serial.println("JSON received: ");
  Serial.println(receiveBuffer);
  Serial.println("");
  client.stop();

  JsonObject& root = jsonBuffer.parseObject(receiveBuffer);
  /*
    "Parsing Program" code generated from ArduinoJson Assistant at arduinojson.org:

    JsonObject& channel = root["channel"];
    long channel_id = channel["id"]; // 572681
    const char* channel_name = channel["name"]; // "Pond Sensor"
    const char* channel_description = channel["description"]; // "Various sensor readings at the pond. "
    const char* channel_latitude = channel["latitude"]; // "0.0"
    const char* channel_longitude = channel["longitude"]; // "0.0"
    const char* channel_field1 = channel["field1"]; // "Air Temp"
    const char* channel_field2 = channel["field2"]; // "Submerged Temp"
    const char* channel_field3 = channel["field3"]; // "Battery mV"
    const char* channel_field4 = channel["field4"]; // "Millis"
    const char* channel_field5 = channel["field5"]; // "Pump Status"
    const char* channel_field6 = channel["field6"]; // "Aerator Status"
    const char* channel_created_at = channel["created_at"]; // "2018-09-09T19:02:08Z"
    const char* channel_updated_at = channel["updated_at"]; // "2018-09-09T23:27:23Z"
    int channel_last_entry_id = channel["last_entry_id"]; // 77
  */

  if (root.success()) {

    JsonObject& feeds0 = root["feeds"][0];
    const char* feeds0_created_at = feeds0["created_at"]; // "2018-06-10T22:26:23Z"
    long feeds0_entry_id = feeds0["entry_id"]; // 90649

    long fishWaterT = strtol(feeds0["field2"], NULL, 10);
    // The Turtle Pond temp sensor is stored in "field1" which was originally designed as the "Air Temp" value
    long turtleWaterT = strtol(feeds0["field1"], NULL, 10);
    long pondmV = strtol(feeds0["field3"], NULL, 10);
    strncpy(pondTime, feeds0_created_at, TIMESIZE - 1);
    pondTime[TIMESIZE - 1] = '\0';                         // hard-code a null terminator at end of string

    Serial.println("Parsed JSON: ");
    Serial.print("Created at: ");
    Serial.println(feeds0_created_at);
    Serial.print("Entry ID: ");
    Serial.println(feeds0_entry_id);

    snprintf(fishTemp, TEMPSIZE, "%3i.%i", fishWaterT / 10, abs(fishWaterT) % 10);
    snprintf(turtleTemp, TEMPSIZE, "%3i.%i", turtleWaterT / 10, abs(turtleWaterT) % 10);
  }
  else
  {
    Serial.println("JSON parse failed.");
    snprintf(fishTemp, TEMPSIZE, "  N/A");
    snprintf(turtleTemp, TEMPSIZE, "  N/A");
    strcpy(pondTime, "     N/A");
  }

  myScreen.gText(layout.FishTempValue.x, layout.FishTempValue.y, prevFishTemp, blackColour);
  myScreen.gText(layout.FishTempValue.x, layout.FishTempValue.y, fishTemp);
  strncpy(prevFishTemp, fishTemp, TEMPSIZE);
  myScreen.gText(layout.TurtleTempValue.x, layout.TurtleTempValue.y, prevTurtleTemp, blackColour);
  myScreen.gText(layout.TurtleTempValue.x, layout.TurtleTempValue.y, turtleTemp);
  strncpy(prevTurtleTemp, turtleTemp, TEMPSIZE);

} // getAndDisplayPond()

void getAndDisplayGarage() {

  DynamicJsonBuffer jsonBuffer(bufferSize);

  uint16_t doorColor;

  int i = 0;
  char c;


  // if you get a connection, report back via serial:
  if (client.connect(server, 80)) {
    Serial.println("connected");
  }
  else {
    // if you didn't get a connection to the server:
    Serial.println("connection failed");
  }

  // Make a HTTP request for Garage sensor
  GetThingSpeakChannel(&client, REPEATER_CHANNEL, REPEATER_KEY, 1);

  // Need to check for connection and wait for characters
  // Need to timeout after some time, but not too soon before receiving response
  // Initially just use delay(), but replace with improved code using millis()
  delay(750);

  while (client.connected()) {
    /// Add a timeout with millis()
    c = client.read();
    if (c != -1) receiveBuffer[i++] = c;
    if (i > sizeof(receiveBuffer) - 2) break;    // Leave a byte for the null terminator
  }

  receiveBuffer[i] = '\0';
  Serial.println("JSON received: ");
  Serial.println(receiveBuffer);
  Serial.println("");
  client.stop();

  JsonObject& root = jsonBuffer.parseObject(receiveBuffer);
  /*
    "Parsing Program" code generated from ArduinoJson Assistant at arduinojson.org:

    JsonObject& channel = root["channel"];
    long channel_id = channel["id"]; // 452942
    const char* channel_name = channel["name"]; // "Garage Repeater"
    const char* channel_description = channel["description"]; // "Repeater hub to improve weather station reception"
    const char* channel_latitude = channel["latitude"]; // "0.0"
    const char* channel_longitude = channel["longitude"]; // "0.0"
    const char* channel_field1 = channel["field1"]; // "Temp"
    const char* channel_field2 = channel["field2"]; // "Vcc"
    const char* channel_field3 = channel["field3"]; // "RxHubEthUptime"
    const char* channel_field4 = channel["field4"]; // "Millis"
    const char* channel_field5 = channel["field5"]; // "RSSI"
    const char* channel_field6 = channel["field6"]; // "LQI"
    const char* channel_field7 = channel["field7"]; // "RxHubMinutes"
    const char* channel_field8 = channel["field8"]; // "Door"
    const char* channel_created_at = channel["created_at"]; // "2018-03-18T16:17:02Z"
    const char* channel_updated_at = channel["updated_at"]; // "2018-08-17T16:41:12Z"
    long channel_last_entry_id = channel["last_entry_id"]; // 51130
  */

  if (root.success()) {

    JsonObject& feeds0 = root["feeds"][0];
    const char* feeds0_created_at = feeds0["created_at"]; // "2018-06-10T22:26:23Z"
    long feeds0_entry_id = feeds0["entry_id"]; // 90649

    long door = strtol(feeds0["field8"], NULL, 10);
    strncpy(garageTime, feeds0_created_at, TIMESIZE - 1);
    garageTime[TIMESIZE - 1] = '\0';                         // hard-code a null terminator at end of string

    Serial.println("Parsed JSON: ");
    Serial.print("Created at: ");
    Serial.println(feeds0_created_at);
    Serial.print("Entry ID: ");
    Serial.println(feeds0_entry_id);

    if (door > 45) {
      snprintf(garageDoor, GDSIZE, "  OPEN");
      doorColor = redColour;
    }
    else {
      snprintf(garageDoor, GDSIZE, "Closed");
      doorColor = greenColour;
    }
  }
  else
  {
    Serial.println("JSON parse failed.");
    snprintf(garageDoor, GDSIZE, "   N/A");
    strcpy(garageTime, "     N/A");
    doorColor = whiteColour;
  }

  myScreen.gText(layout.GDValue.x, layout.GDValue.y, prevGarageDoor, blackColour);
  myScreen.gText(layout.GDValue.x, layout.GDValue.y, garageDoor, doorColor);
  strncpy(prevGarageDoor, garageDoor, TEMPSIZE);

} // getAndDisplayGarage()

void getAndDisplayTime() {

  int i, yr, mo, da, dst_status;
  int theHour;

  t = now(); // Get the current time

  Serial.print("timeNotSet = ");
  Serial.print(timeNotSet);
  Serial.print(", timeNeedsSync = ");
  Serial.print(timeNeedsSync);
  Serial.print(", timeSet = ");
  Serial.print(timeSet);
  Serial.print(". timeStatus() = ");
  Serial.println(timeStatus());

  if (timeStatus() == timeSet) {
    yr = year(t);
    mo = month(t);
    da = day(t);
    theHour = hour(t);
    i = (yr - DST_FIRST_YEAR) * 4;
    dst_status = 0;               // Assume standard time unless changed below

    if (yr <= MAX_DST_YEAR) {
      if ( (mo == dst_info[i + 0] && da == dst_info[i + 1] && theHour >= DST_EFFECTIVE_HOUR) ||    // DST changeover time
           (mo == dst_info[i + 0] && da > dst_info[i + 1]) ||
           (mo > dst_info[i + 0]) )
        // Past start of DST, now check if DST has ended
        if ( (mo == dst_info[i + 2] && da == dst_info[i + 3] && theHour < DST_EFFECTIVE_HOUR) ||   // ST changover time
             (mo == dst_info[i + 2] && da < dst_info[i + 3]) ||
             (mo < dst_info[i + 2]) )
        {
          dst_status = 1;
          t = t + 3600; // Add an hour
        }
    }

    /*    /// Test Code
        int k, l;
        //  i = (2029 - DST_FIRST_YEAR) * 4;
        i = 0;
        theHour = 1;
        for (k = 1; k < 13; k++)   // Cycle through months
          for (l = 1; l < 32; l++) // Cycle through days
          {
            dst_status = 0;
            if ( (k == dst_info[i + 0] && l == dst_info[i + 1] && theHour >= DST_EFFECTIVE_HOUR) ||    // DST changeover at 2:00 AM
                 (k == dst_info[i + 0] && l > dst_info[i + 1]) ||
                 (k > dst_info[i + 0]) )
              // Past start of DST, now check if DST has ended
              if ( (k == dst_info[i + 2] && l == dst_info[i + 3] && theHour < DST_EFFECTIVE_HOUR) ||   // ST changover at 2:00 AM
                   (k == dst_info[i + 2] && l < dst_info[i + 3]) ||
                   (k < dst_info[i + 2]) )
              {
                dst_status = 1;
              }

            snprintf(timeAndDate, TADSIZE, "%02d %s %02d:%02d %s",
                     l,  monthShortStr(k), theHour, minute(t), (dst_status) ? DAYLIGHT_TZ_STRING : STANDARD_TZ_STRING);
            Serial.print("Time and Date String: ");
            Serial.println(timeAndDate);
          }

        /// */

    snprintf(timeAndDate, TADSIZE, "%02d %s %2d:%02d %s %s",
             day(t), monthShortStr(month(t)), hourFormat12(t), minute(t), (isAM(t)) ? "AM" : "PM", (dst_status) ? DAYLIGHT_TZ_STRING : STANDARD_TZ_STRING);

    myScreen.gText(layout.TimeAndDateValue.x, layout.TimeAndDateValue.y, prevTimeAndDate, blackColour);
    myScreen.gText(layout.TimeAndDateValue.x, layout.TimeAndDateValue.y, timeAndDate, timeColor);
    strncpy(prevTimeAndDate, timeAndDate, TADSIZE);
  } // if (timeStatus() == timeSet)
  else { // timeStatus is not valid, so clear the time display
    myScreen.gText(layout.TimeAndDateValue.x, layout.TimeAndDateValue.y, prevTimeAndDate, blackColour);
  }
} // getAndDisplayTime()

void displayStartup() {
  myScreen.gText(0, 0, "Weather Station", yellowColour, blackColour, 1, 1);
  myScreen.gText(24,  60, "Initializing", redColour, blackColour);
  myScreen.gText(48,  80, "Ethernet. . .", redColour, blackColour);
}

void displayWelcome() {
  //  myScreen.gText(0, 0, "Weather Station", blueColour, blackColour, 1, 1);
  myScreen.setFontSize(0);  // gText font size multiplier only works with font size 0
  myScreen.gText(24,  60, "Welcome,", redColour, blackColour, 4, 4); // For font size 2: x = 240/2 - 12*8/2 = 72
  myScreen.gText(60, 120, "Andy!", redColour, blackColour, 4, 4);    // For font size 2: x = 240/2 - 12*5/2 = 90
  myScreen.setFontSize(2);
  delay(2000);
}

void displayTitles() {
  myScreen.setOrientation(2);
  myScreen.clear();
  myScreen.gText(layout.WeatherTitle.x, layout.WeatherTitle.y, WeatherTitle);
  myScreen.gText(layout.WeatherTempUnits.x, layout.WeatherTempUnits.y, DegreesF);
  myScreen.gText(layout.WeatherLuxUnits.x, layout.WeatherLuxUnits.y, Lux);
  myScreen.gText(layout.WeatherRHUnits.x, layout.WeatherRHUnits.y, RH);
  myScreen.gText(layout.WeatherPUnits.x, layout.WeatherPUnits.y, inHG);
  myScreen.gText(layout.SlimTitle.x, layout.SlimTitle.y, SlimTitle);
  myScreen.gText(layout.SlimTempUnits.x, layout.SlimTempUnits.y, DegreesF);
  myScreen.gText(layout.Sensor5Title.x, layout.Sensor5Title.y, Sensor5Title);
  myScreen.gText(layout.Sensor5TempUnits.x, layout.Sensor5TempUnits.y, DegreesF);
  myScreen.gText(layout.WorkshopTitle.x, layout.WorkshopTitle.y, WorkshopTitle);
  myScreen.gText(layout.WorkshopTempUnits.x, layout.WorkshopTempUnits.y, DegreesF);
  myScreen.gText(layout.FishTitle.x, layout.FishTitle.y, FishTitle);
  myScreen.gText(layout.FishTempUnits.x, layout.FishTempUnits.y, DegreesF);
  myScreen.gText(layout.TurtleTitle.x, layout.TurtleTitle.y, TurtleTitle);
  myScreen.gText(layout.TurtleTempUnits.x, layout.TurtleTempUnits.y, DegreesF);
  myScreen.gText(layout.GDTitle.x, layout.GDTitle.y, GDTitle);
  myScreen.gText(layout.BattTitle.x, layout.BattTitle.y, BatteriesTitle);
  myScreen.gText(layout.BattOutdoorSubtitle.x, layout.BattOutdoorSubtitle.y, OutdoorSubtitle);
  myScreen.gText(layout.BattOutdoorUnits.x, layout.BattOutdoorValue.y, V);
  myScreen.gText(layout.BattSlimSubtitle.x, layout.BattSlimSubtitle.y, SlimSubtitle);
  myScreen.gText(layout.BattSlimUnits.x, layout.BattSlimUnits.y, V);
  myScreen.gText(layout.BattSensor5Subtitle.x, layout.BattSensor5Subtitle.y, Sensor5Subtitle);
  myScreen.gText(layout.BattSensor5Units.x, layout.BattSensor5Units.y, V);
  // Don't really need to display "Time and Date" title -- it's pretty obvious
  // myScreen.gText(layout.TimeAndDateTitle.x, layout.TimeAndDateTitle.y, TimeAndDateTitle);
}

void vfdIOSetup() {

  pinMode(VFD_BUFFER_EN, OUTPUT);
  digitalWrite(VFD_BUFFER_EN, LOW);        // Disable CD40109 buffer
  pinMode(VFD_POWER_CONTROL, OUTPUT);
  digitalWrite(VFD_POWER_CONTROL, HIGH);   // Turn off VFD power
  pinMode(VFD_CLOCK_PIN, OUTPUT);
  digitalWrite(VFD_CLOCK_PIN, HIGH);       // Set IO pins to initial states
  pinMode(VFD_DATA_PIN, OUTPUT);
  digitalWrite(VFD_DATA_PIN, LOW);
  pinMode(VFD_RESET_PIN, OUTPUT);
  digitalWrite(VFD_RESET_PIN, LOW);

  // Call vfd.begin to set up internal variables. Note that the
  // VFD is powered down at this point, so the reset and device
  // configuration that is done in begin() aren't actually getting
  // received by VFD.
  vfd.begin(16, 2);

}

void vfdOff() {

  Serial.println("VFD powered down.");
  digitalWrite(VFD_BUFFER_EN, LOW);       // Disable CD40109 buffer before turning off power
  digitalWrite(VFD_POWER_CONTROL, HIGH);

}

void vfdOn() {

  Serial.println("VFD turned on.");
  digitalWrite(VFD_POWER_CONTROL, LOW);   // Turn on VFD power before enabling IO pins
  digitalWrite(VFD_CLOCK_PIN, HIGH);
  digitalWrite(VFD_DATA_PIN, LOW);
  digitalWrite(VFD_RESET_PIN, LOW);
  digitalWrite(VFD_BUFFER_EN, HIGH);       // Enable IO pins

  digitalWrite(VFD_RESET_PIN, HIGH);      // VFD reset sequence
  delay(5);
  digitalWrite(VFD_RESET_PIN, LOW);
  delay(5);

  // Configure VFD after coming out of reset
  vfd.writeCharacterDirect(FutabaVFD162S::SET_INPUT_OUTPUT_MODE_CHARACTER);
  vfd.writeCharacterDirect(0x01);  // I/O Mode 1: Unidirectional (no handshake)
  vfd.clear();
  vfd.setBrightness(VFD_BRIGHTNESS);
}

void displayVFD() {
#define DISPLAY_DELAY 7000

  // Printing time text "+5" to skip over "YYYY-" in time string
  vfd.clear();
  vfd.print("Weather: ");
  vfd.setCursor(0, 1);        // Line 2
  vfd.print(weatherTime + 5);
  delay(DISPLAY_DELAY);
  vfd.clear();
  vfd.print("Slim: ");
  vfd.setCursor(0, 1);        // Line 2
  vfd.print(slimTime + 5);
  delay(DISPLAY_DELAY);
  vfd.clear();
  vfd.print("Sensor 5: ");
  vfd.setCursor(0, 1);        // Line 2
  vfd.print(sensor5Time + 5);
  delay(DISPLAY_DELAY);
  vfd.clear();
  vfd.print("Pond: ");
  vfd.setCursor(0, 1);        // Line 2
  vfd.print(pondTime + 5);
  delay(DISPLAY_DELAY);
  vfd.clear();
  vfd.print("Workshop: ");
  vfd.setCursor(0, 1);        // Line 2
  vfd.print(workshopTime + 5);
  delay(DISPLAY_DELAY);
  vfd.clear();
  vfd.print("Repeater: ");
  vfd.setCursor(0, 1);        // Line 2
  vfd.print(garageTime + 5);
  //  delay(DISPLAY_DELAY);    // No delay after last status, since pulling update from ThingSpeak takes several seconds
} // displayVFD()

void displayOLED() {
  int hist_max = 0;
  int hist_min = 32767;
  int hist_mid;
  int hist_delta;
  char p_decimal[3];

  for (int i = 0; i < hist_num_entries; i++) {
    if ( p_hist[i] > hist_max) hist_max = p_hist[i];
    if ( p_hist[i] < hist_min) hist_min = p_hist[i];
  }

  hist_mid = (hist_max + hist_min) / 2;
  hist_delta = hist_max - hist_min;

  Serial.print("Min: ");
  Serial.println(hist_min);
  Serial.print("Max: ");
  Serial.println(hist_max);
  Serial.print("Hist size: ");
  Serial.println(hist_num_entries);
  Serial.print("Hist position: ");
  Serial.println(hist_position);

  oled.clear(PAGE);  // Clear the buffer

  // If range is values is < 30 (lines available in display area), then plot the points relative to row 15
  // If values is >= 30, then use map function to fit the values in display area
  for (int i = 0; i < hist_num_entries; i++) {
    if (hist_delta < 30) oled.pixel(i, 15 - (p_hist[(i + hist_position ) % hist_len] - hist_mid));
    else oled.pixel(i, map(p_hist[(i + hist_position ) % hist_len], hist_min, hist_max, 34, 0));
  }

  if (hist_num_entries > 0) {
    oled.setFontType(1);  // 8x16 characters
    oled.setCursor(0, 37); // Bottom row, no blank pixel under letters
    oled.print("P:");
    if (hist_num_entries < hist_len) oled.print(p_hist[hist_num_entries - 1] / 100);
    else oled.print(p_hist[(hist_position - 1) % hist_len] / 100);
    oled.print(".");
    if (hist_num_entries < hist_len) {
      snprintf(p_decimal, 3, "%.2d", p_hist[hist_num_entries - 1] % 100);
    }
    else {
      snprintf(p_decimal, 3, "%.2d", p_hist[(hist_position - 1) % hist_len] % 100);
    }
    oled.print(p_decimal);
    oled.display();
  }
} // displayOLED()

time_t getNtpTime()
{
  while (Udp.parsePacket() > 0) ; // discard any previously received packets
  Serial.println("Transmit NTP Request");
  sendNTPpacket(timeServer);
  uint32_t beginWait = millis();
  while (millis() - beginWait < 1500) {
    int size = Udp.parsePacket();
    if (size >= NTP_PACKET_SIZE) {
      Serial.println("Receive NTP Response");
      timeColor = whiteColour;
      Udp.read(packetBuffer, NTP_PACKET_SIZE);  // read packet into the buffer
      unsigned long secsSince1900;
      // convert four bytes starting at location 40 to a long integer
      secsSince1900 =  (unsigned long)packetBuffer[40] << 24;
      secsSince1900 |= (unsigned long)packetBuffer[41] << 16;
      secsSince1900 |= (unsigned long)packetBuffer[42] << 8;
      secsSince1900 |= (unsigned long)packetBuffer[43];
      return secsSince1900 - 2208988800UL + timeZone * SECS_PER_HOUR;
    }
  }
  Serial.println("No NTP Response :-(");
  timeColor = blackColour;
  return 0; // return 0 if unable to get the time
}

// send an NTP request to the time server at the given address
void sendNTPpacket(const char* address)
{
  // set all bytes in the buffer to 0
  memset(packetBuffer, 0, NTP_PACKET_SIZE);
  // Initialize values needed to form NTP request
  // (see URL above for details on the packets)
  packetBuffer[0] = 0b11100011;   // LI, Version, Mode
  packetBuffer[1] = 0;     // Stratum, or type of clock
  packetBuffer[2] = 6;     // Polling Interval
  packetBuffer[3] = 0xEC;  // Peer Clock Precision
  // 8 bytes of zero for Root Delay & Root Dispersion
  packetBuffer[12]  = 49;
  packetBuffer[13]  = 0x4E;
  packetBuffer[14]  = 49;
  packetBuffer[15]  = 52;
  // all NTP fields have been given values, now
  // you can send a packet requesting a timestamp:
  Udp.beginPacket(address, 123); //NTP requests are to port 123
  Udp.write(packetBuffer, NTP_PACKET_SIZE);
  Udp.endPacket();
}

void DisplayCharacterMap()
{
  unsigned char tempChar[2];
  tempChar[1] = '\0';
  tempChar[0] = 128;
  int i, j;

  delay(2000);
  myScreen.clear();
  for (j = 0; j < 8; j++)
  {
    for (i = 0; i < 16; i++) {
      myScreen.gText(i * 12, j * 16, (const char*) tempChar);
      tempChar[0]++;
    }
  }
  // delay(60000);
}
