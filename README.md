Connected Status Monitor
========================

Energia sketch for a status display of various sensor data readings that are queried from the ThingSpeak IoT Platform.

The sketch is specifically designed for use with the Texas Instruments TM4C1294 [Connected LaunchPad][1] and [Kentec Touch Display BoosterPack (SPI)][2]

While the sketch is written for a specific hardware configuration, it can be used as an example for the following (both on Arduino and Energia platforms):
- TCP and UDP connections using Ethernet
- [ArduinoJson][3] library for parsing JSON strings
- Reading data using [REST API][4] for [ThingsSpeak][6] IoT platform
- Arduino [Time Library][5]
- Retrieving NTP time
- Using a simple [photo resistor circuit][7] to detect when a room light is turned on

Parts of the code are messy and could be improved. However, it does what I want, so these things will probably remain unchanged for awhile. In particular:
- Tried to come up with a creative solution for defining x/y coordinates for the display items. My solution ended up being a little cumbersome and difficult to maintain if the screen layout needs to change significantly.
- The DST conversion is somewhat hardcoded for the U.S., particularly Central Time. It is fairly easy to update for U.S. time zones, but could get a little cumbersome for time zones that aren't on hour boundaries or which don't change at 2:00 AM.
- The time server IP is hardcoded, instead of using DNS to resolve the address.

## Light Detector Circuit ##
This display is located in a windowless workshop, and I only want it on when I am in the workshop -- which is essentially any time the room light is on. So I constructed a simple circuit using a 10K resistor and a cheap photo resistor (which probably came from an Arduino starter kit). The 10K and photo resistors are wired in series, with the 10K pulled to Vcc and the photo resistor connected to ground. Analog Pin A13 reads the voltage at the 10K/photo resistor connection. Since it is just checking for a dark room vs. a lighted room, a simple threshold of half the ADC range is used. 

## External Libraries ##
* [ArduinoJson][3]
* [Arduino Time Library][5]

## References ##
* Texas Instruments TM4C1294 Connected LaunchPad [EK-TM4C1294XL][1]
* Kentec Touch Display BoosterPack (SPI) [BOOSTXL-K350QVG-S1][2]
* ThingsSpeak [IoT platform][6]
* ThingsSpeak REST API [documentation][4]
* Arduino Playground photo resistor [tutorial][7]

[1]: http://www.ti.com/tool/EK-TM4C1294XL
[2]: http://www.ti.com/tool/boostxl-k350qvg-s1
[3]: https://arduinojson.org/
[4]: https://www.mathworks.com/help/thingspeak/read-data-from-channel.html
[5]: https://github.com/PaulStoffregen/Time
[6]: https://thingspeak.com/
[7]: https://playground.arduino.cc/Learning/PhotoResistor
