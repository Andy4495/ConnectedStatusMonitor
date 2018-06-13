/*
  Web client

  This sketch connects to a website (http://www.google.com)
  using an Arduino Wiznet Ethernet shield.

  Circuit:
   Ethernet shield attached to pins 10, 11, 12, 13

  created 18 Dec 2009
  by David A. Mellis

*/
#include <ArduinoJson.h>
#include <SPI.h>
#include <Ethernet.h>
#include "Screen_K35_SPI.h"
Screen_K35_SPI myScreen;
#include "ThingSpeakKeys.h"

// Enter a MAC address for your controller below.
// Newer Ethernet shields have a MAC address printed on a sticker on the shield
// byte mac[] = {  0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };
byte mac[] = LAUNCHPAD_MAC;
// IPAddress server(173,194,33,104); // Google
const char* server = "api.thingspeak.com";

char receiveBuffer[1024] = {};
char printBuffer[32] = {};
char prevPrintBuffer[32] = {};

const size_t bufferSize = JSON_ARRAY_SIZE(1) + JSON_OBJECT_SIZE(2) + JSON_OBJECT_SIZE(10) + JSON_OBJECT_SIZE(16) + 600;

// Initialize the Ethernet client library
// with the IP address and port of the server
// that you want to connect to (port 80 is default for HTTP):
EthernetClient client;

void setup() {
  // start the serial library:
  Serial.begin(9600);
  // start the Ethernet connection:
  if (Ethernet.begin(mac) == 0) {
    Serial.println("Failed to configure Ethernet using DHCP");
    // no point in carrying on, so do nothing forevermore:
    for (;;)
      ;
  }
  // give the Ethernet shield a second to initialize:
  delay(1000);
  Serial.println("connecting...");

  myScreen.begin();
  myScreen.setPenSolid(true);
  myScreen.setFontSolid(false);
  myScreen.setFontSize(2);
  myScreen.gText(0, 0, "ThingSpeak JSON Queries", blueColour, grayColour, 1, 1);

}

void loop()
{
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

  // Make a HTTP request:
  //  client.println("GET /channels/412285/fields/4.csv?results=1");
  // client.println("GET /channels/" WEATHERSTATION_CHANNEL "/fields/4.json?api_key=" WEATHERSTATION_KEY "&results=1"); // Get a specific feed from a channel
  client.println("GET /channels/" WEATHERSTATION_CHANNEL "/feeds.json?api_key=" WEATHERSTATION_KEY "&results=1"); // Get all feeds from a channel
  client.println();

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
  /*
    // if there are incoming bytes available
    // from the server, read them and print them:
    while (client.available()) {
      char c = client.read();
      Serial.print(c);
    }

    // if the server's disconnected, stop the client:
    if (!client.connected()) {
      Serial.println("Not connected...");
    }
  */

  receiveBuffer[i] = '\0';
  Serial.println("JSON received: ");
  Serial.println(receiveBuffer);
  Serial.println("");
  client.stop();

  JsonObject& root = jsonBuffer.parseObject(receiveBuffer);
  /*
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
    const char* feeds0_field3 = feeds0["field4"]; // "669"

    long Tf = strtol(feeds0_field3, NULL, 10);
    Serial.println("Parsed JSON: ");
    Serial.print("Created at: ");
    Serial.println(feeds0_created_at);
    Serial.print("Entry ID: ");
    Serial.println(feeds0_entry_id);
    Serial.print("Temperature: ");
    Serial.print(Tf / 10);
    Serial.print(".");
    Serial.println(Tf % 10);

    snprintf(printBuffer, 32, "%i.%i", Tf / 10, Tf % 10);

    //myScreen.clear(); // Default color is blackColour
    myScreen.setFontSize(1);
    myScreen.gText(0, 17, "Outdoor Temp: ");
    myScreen.setFontSize(0);
    myScreen.gText(8*14, 24, prevPrintBuffer, blackColour);
    myScreen.gText(8*14, 24, printBuffer);
    strncpy(prevPrintBuffer, printBuffer, 32);
  }
  else
  {
    Serial.println("JSON parse failed.");

    myScreen.gText(0, 20, "Outdoor Temp: ");
    myScreen.gText(80, 20, prevPrintBuffer, blackColour);
    myScreen.gText(80, 20, "N/A");
    strncpy(prevPrintBuffer, "N/A", 32);

  }


  Serial.println("Disconnecting. Waiting 30 seconds before next query. ");

  delay(30000);
}

