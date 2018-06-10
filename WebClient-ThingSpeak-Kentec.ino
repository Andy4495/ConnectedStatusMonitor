/*
  Web client

  This sketch connects to a website (http://www.google.com)
  using an Arduino Wiznet Ethernet shield.

  Circuit:
   Ethernet shield attached to pins 10, 11, 12, 13

  created 18 Dec 2009
  by David A. Mellis

*/

#include <SPI.h>
#include <Ethernet.h>
#include "ThingSpeakKeys.h"

// Enter a MAC address for your controller below.
// Newer Ethernet shields have a MAC address printed on a sticker on the shield
// byte mac[] = {  0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };
byte mac[] = LAUNCHPAD_MAC;
// IPAddress server(173,194,33,104); // Google
const char* server = "api.thingspeak.com";

char receiveBuffer[1024] = {};

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

}

void loop()
{
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
  //  client.println("GET /channels/412285/fields/1.csv?results=1");
  client.println("GET /channels/" WEATHERSTATION_CHANNEL "/fields/3.json?api_key=" WEATHERSTATION_KEY "&results=1");
  client.println();

  // Need to check for connection and wait for characters
  // Need to timeout after some time, but not too soon before receiving response
  // Initially just use delay(), but replace with improved code using millis()
  delay(500);

  while (client.connected()) {
    /// Add a timeout with millis()
    c = client.read();
    if (c != -1) receiveBuffer[i++] = c;
    if (i > sizeof(receiveBuffer) - 2) break;
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
  Serial.println("disconnecting. Waiting 30 seconds. ");
  client.stop();
  delay(30000);
}

