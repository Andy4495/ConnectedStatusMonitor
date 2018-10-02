/*
  SENSOR STATUS DISPLAY

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




  *** Future improvements:
    Color a sensor value (or the label) yellow or red if an update has not been received for more than X minutes
    Add a pixel to y-coordinates to allow for hanging comma space (otherwise comma touches next line)
    Update lux string to include commas
    Have pressure indicate increasing or decreasing since last measurement (or same or if last measure was N/A)
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
char sensor5Temp[TEMPSIZE];
char prevSensor5Temp[TEMPSIZE];
char pondTemp[TEMPSIZE];
char prevPondTemp[TEMPSIZE];
char garageDoor[GDSIZE];
char prevGarageDoor[GDSIZE];
char outdoorBatt[BATTSIZE];
char prevOutdoorBatt[BATTSIZE];
char slimBatt[BATTSIZE];
char prevSlimBatt[BATTSIZE];
char sensor5Batt[BATTSIZE];
char prevSensor5Batt[BATTSIZE];
char pondBatt[BATTSIZE];
char prevPondBatt[BATTSIZE];
char timeAndDate[TADSIZE];
char prevTimeAndDate[TADSIZE];

#define LIGHT_SENSOR_PIN           42
#define LIGHT_SENSOR_ADC          A13
#define LIGHT_SENSOR_THRESHOLD   2000    // Based on 10K resistor and cheap photoresistor voltage divider and 12-bit ADC (4096 max value)
#define LIGHTS_ON_SLEEP_TIME    30000
#define LIGHTS_OFF_SLEEP_TIME    5000
#define BACKLIGHT_PIN              40
#define SLEEPING_STATUS_LED      PN_1    // Flash when display backlight is off to show the unit is active

int  statusLEDstate = 0;

enum {LIGHTS_OFF, LIGHTS_TURNED_ON, LIGHTS_ON, LIGHTS_TURNED_OFF};
int lightSensorState = LIGHTS_OFF;

void setup() {

  pinMode(LIGHT_SENSOR_PIN, INPUT);
  pinMode(SLEEPING_STATUS_LED, OUTPUT);
  pinMode(PUSH1, INPUT_PULLUP);


  // start the serial library:
  Serial.begin(9600);

  myScreen.begin();
  myScreen.setPenSolid(true);
  myScreen.setFontSolid(false);
  myScreen.setFontSize(2);
  myScreen.setOrientation(2);
  displayStartup();

  // start the Ethernet connection:
  if (Ethernet.begin(mac) == 0) {
    Serial.println("Failed to configure Ethernet using DHCP");
    myScreen.gText(24, 120, "Failed!", redColour, blackColour);
    myScreen.gText(24, 160, "Program halted.", redColour, blackColour);
    // no point in carrying on, so do nothing forevermore:
    for (;;)
      ;
  }
  // give the Ethernet shield a second to initialize:
  delay(1000);

  // Turn on Ethernet status LEDs if PUSH1 is pressed at reboot
  // Otherwise, they default to off.
  if (digitalRead(PUSH1) == LOW) {
    Ethernet.enableLinkLed();
    Ethernet.enableActivityLed();
    Serial.println("Ethernet LINK and ACTIVITY LEDs enabled.");
  }
  else {
    Serial.println("Ethernet status LEDs disabled.");
    // Following code can be used to turn of Ethernet status LEDs
    // GPIODirModeSet(ACTIVITY_LED_BASE, ACTIVITY_LED_PIN, GPIO_DIR_MODE_IN);
    // GPIODirModeSet(LINK_LED_BASE, LINK_LED_PIN, GPIO_DIR_MODE_IN);
  }

  Serial.print("JsonBuffer size: ");
  Serial.println(bufferSize);
  Serial.println("connecting...");

  //  DisplayCharacterMap(); // For testing

  // Initialze the "previous" strings to empty
  prevOutdoorTemp[0] = 0;
  prevOutdoorLux[0] = 0;
  prevOutdoorRH[0] = 0;
  prevOutdoorP[0] = 0;
  prevSlimTemp[0] = 0;
  prevPondTemp[0] = 0;
  prevGarageDoor[0] = 0;
  prevOutdoorBatt[0] = 0;
  prevSlimBatt[0] = 0;
  prevPondBatt[0] = 0;
  prevTimeAndDate[0] = 0;

  Udp.begin(localPort);
  Serial.println("Setting up NTP...");
  setSyncProvider(getNtpTime);
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
        statusLEDstate = ~statusLEDstate;                     // Flash the LED
        digitalWrite(SLEEPING_STATUS_LED, statusLEDstate);
        digitalWrite(BACKLIGHT_PIN, 0);
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
      lightSensorState = LIGHTS_ON;
      break;

    case LIGHTS_ON:
      if (lightSensor > LIGHT_SENSOR_THRESHOLD) {
        getAndDisplayTime();
        getAndDisplayWeather();
        getAndDisplaySlim();
        getAndDisplaySensor5();
        getAndDisplayPond();
        getAndDisplayGarage();
        Serial.println("Disconnecting. Waiting 30 seconds before next query. ");
        delay(LIGHTS_ON_SLEEP_TIME);
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
  delay(500);

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

    Serial.println("Parsed JSON: ");
    Serial.print("Created at: ");
    Serial.println(feeds0_created_at);
    Serial.print("Entry ID: ");
    Serial.println(feeds0_entry_id);

    snprintf(outdoorTemp, TEMPSIZE, "%3i.%i", Tf / 10, Tf % 10);
    snprintf(outdoorLux, LUXSIZE, "%8i", lux);
    snprintf(outdoorRH, RHSIZE, "%2i.%i", rh / 10, rh % 10);
    snprintf(outdoorP, PSIZE, "%2i.%02i", p / 100, p % 100);
    snprintf(outdoorBatt, BATTSIZE, "%i.%03i", wBatt / 1000, wBatt % 1000);

    if (wBatt < 2500) battColor = redColour;
    else battColor = greenColour;
  }
  else
  {
    Serial.println("JSON parse failed.");
    snprintf(outdoorTemp, TEMPSIZE,  "  N/A");
    snprintf(outdoorLux, LUXSIZE, "     N/A");
    snprintf(outdoorRH, RHSIZE,       " N/A");
    snprintf(outdoorP, PSIZE,        "  N/A");
    snprintf(outdoorBatt, BATTSIZE,  "  N/A");
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
  delay(500);

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

    Serial.println("Parsed JSON: ");
    Serial.print("Created at: ");
    Serial.println(feeds0_created_at);
    Serial.print("Entry ID: ");
    Serial.println(feeds0_entry_id);

    if (Tslim < 800) tempColor = redColour;
    else tempColor = greenColour;

    if (sBatt < 2400) battColor = redColour;
    else battColor = greenColour;

    snprintf(slimTemp, TEMPSIZE, "%3i.%i", Tslim / 10, Tslim % 10);
    snprintf(slimBatt, BATTSIZE, "%i.%03i", sBatt / 1000, sBatt % 1000);
  }
  else
  {
    Serial.println("JSON parse failed.");
    snprintf(slimTemp, TEMPSIZE, "  N/A");
    snprintf(slimBatt, BATTSIZE, "  N/A");
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
  delay(500);

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
    const char* channel_name = channel["name"]; // "Indoor Temp 5 - Gargoyle"
    const char* channel_description = channel["description"]; // "Indoor Temp Sensor #5 - Gargoyle"
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

    Serial.println("Parsed JSON: ");
    Serial.print("Created at: ");
    Serial.println(feeds0_created_at);
    Serial.print("Entry ID: ");
    Serial.println(feeds0_entry_id);

    if (T5 > 850) tempColor = redColour;
    else tempColor = greenColour;

    if (B5 < 2400) battColor = redColour;
    else battColor = greenColour;

    snprintf(sensor5Temp, TEMPSIZE, "%3i.%i", T5 / 10, T5 % 10);
    snprintf(sensor5Batt, BATTSIZE, "%i.%03i", B5 / 1000, B5 % 1000);
  }
  else
  {
    Serial.println("JSON parse failed.");
    snprintf(sensor5Temp, TEMPSIZE, "  N/A");
    snprintf(sensor5Batt, BATTSIZE, "  N/A");
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

void getAndDisplayPond() {

  DynamicJsonBuffer jsonBuffer(bufferSize);

  uint16_t battColor;

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
  delay(500);

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

    long pondWaterT = strtol(feeds0["field2"], NULL, 10);
    long pondmV = strtol(feeds0["field3"], NULL, 10);

    Serial.println("Parsed JSON: ");
    Serial.print("Created at: ");
    Serial.println(feeds0_created_at);
    Serial.print("Entry ID: ");
    Serial.println(feeds0_entry_id);

    if (pondmV < 3500) battColor = redColour;
    else battColor = greenColour;

    snprintf(pondTemp, TEMPSIZE, "%3i.%i", pondWaterT / 10, pondWaterT % 10);
    snprintf(pondBatt, BATTSIZE, "%i.%03i", pondmV / 1000, pondmV % 1000);
  }
  else
  {
    Serial.println("JSON parse failed.");
    snprintf(pondTemp, TEMPSIZE, "  N/A");
    snprintf(pondBatt, BATTSIZE, "  N/A");
    battColor = whiteColour;
  }

  myScreen.gText(layout.PondTempValue.x, layout.PondTempValue.y, prevPondTemp, blackColour);
  myScreen.gText(layout.PondTempValue.x, layout.PondTempValue.y, pondTemp);
  strncpy(prevPondTemp, pondTemp, TEMPSIZE);

  myScreen.gText(layout.BattPondValue.x, layout.BattPondValue.y, prevPondBatt, blackColour);
  myScreen.gText(layout.BattPondValue.x, layout.BattPondValue.y, pondBatt, battColor);
  strncpy(prevPondBatt, pondBatt, BATTSIZE);

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
  delay(500);

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
    myScreen.gText(layout.TimeAndDateValue.x, layout.TimeAndDateValue.y, timeAndDate);
    strncpy(prevTimeAndDate, timeAndDate, TADSIZE);
  } // if (timeStatus() == timeSet)
  // Else timeStatus is not valid, so don't change the display
} // getAndDisplayTime()

void displayStartup() {
  myScreen.gText(0, 0, "Weather Station", yellowColour, blackColour, 1, 1);
  myScreen.gText(24,  60, "Initializing", redColour, blackColour);
  myScreen.gText(48,  80, "Ethernet . . .", redColour, blackColour);
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
  myScreen.gText(layout.PondTitle.x, layout.PondTitle.y, PondTitle);
  myScreen.gText(layout.PondTempUnits.x, layout.PondTempUnits.y, DegreesF);
  myScreen.gText(layout.GDTitle.x, layout.GDTitle.y, GDTitle);
  myScreen.gText(layout.BattTitle.x, layout.BattTitle.y, BatteriesTitle);
  myScreen.gText(layout.BattOutdoorSubtitle.x, layout.BattOutdoorSubtitle.y, OutdoorSubtitle);
  myScreen.gText(layout.BattOutdoorUnits.x, layout.BattOutdoorValue.y, V);
  myScreen.gText(layout.BattSlimSubtitle.x, layout.BattSlimSubtitle.y, SlimSubtitle);
  myScreen.gText(layout.BattSlimUnits.x, layout.BattSlimUnits.y, V);
  myScreen.gText(layout.BattSensor5Subtitle.x, layout.BattSensor5Subtitle.y, Sensor5Subtitle);
  myScreen.gText(layout.BattSensor5Units.x, layout.BattSensor5Units.y, V);
  myScreen.gText(layout.BattPondSubtitle.x, layout.BattPondSubtitle.y, PondSubtitle);
  myScreen.gText(layout.BattPondUnits.x, layout.BattPondUnits.y, V);
  // Don't really need to display "Time and Date" title -- it's pretty obvious
  // myScreen.gText(layout.TimeAndDateTitle.x, layout.TimeAndDateTitle.y, TimeAndDateTitle);
}

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
