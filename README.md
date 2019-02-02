Connected IoT Status Monitor
============================

Energia (Arduino-derived IDE for TI microcontrollers) sketch for a status display of various sensor data readings that are queried from the ThingSpeak IoT Platform.

The sketch is specifically designed for use with the Texas Instruments TM4C1294 [Connected LaunchPad][1] and [Kentec Touch Display BoosterPack][2] (SPI version).

While the sketch is written for a specific hardware configuration, it can be used as an example for the following (both on Arduino and Energia platforms):
- TCP and UDP connections using Ethernet
- [ArduinoJson][3] library for parsing JSON strings
- Reading data using [REST API][4] for [ThingsSpeak][6] IoT platform
- Arduino [Time Library][5]
- Retrieving NTP time
- Calculate Standard Time or Daylight Saving Time based on current day/time.
- Using a simple [photo resistor circuit][7] to detect when a room light is turned on

Parts of the code are messy and could be improved. However, it does what I want, so they will probably stay as they are. In particular:
- I tried to come up with a creative solution for defining x/y coordinates for the display items. My solution ended up being a little cumbersome and difficult to maintain if the screen layout needs to change significantly.
- The DST conversion algorithm is optimized for U.S. time zones. It is not generalized for all time zone/DST cases.

## Light Detector Circuit ##
This display is located in a windowless workshop, and I only want the display backlight on and the board querying the sensors database when I am in the workshop -- which is essentially any time the room light is on. So I constructed a simple circuit using a 10K resistor and a cheap photo resistor (which probably came from an Arduino starter kit). The 10K and photo resistors are wired in series, with the 10K pulled to Vcc and the photo resistor connected to ground. Analog Pin A13 reads the voltage at the 10K/photo resistor connection (a simple voltage divider). Since it is just checking for a dark room vs. a lighted room, a threshold of half the ADC range is used.

## Ethernet Status LEDs ##
The Connected LaunchPad supports Ethernet Link (LED4) and Activity (LED3) status indicators. However, I find these LEDs are bright and distracting. So the sketch disables them by default. If you want to enable them, hold down PUSH1 (labeled "USR_SW1" on the board's silkscreen) during reset.

## Kentec Display Library ##
The [LCD display][2] requires the Screen_K35_SPI library. This library is included in the MSP430 board package with Energia, but not in the Tiva boards package. The MSP430 version of the library requires some minor modifications to work with the TM4C1294 LauchPad. These steps need to be performed any time you upgrade Energia or the Tiva boards package version.

Note that these directions are Windows-specific. Mac and Linux instructions are left as an exercise to the reader.

1. Copy the directory `Kentec_35_SPI` from `<Energia Install Directory>\energia-1.8.7E21\hardware\energia\msp430\libraries` to `~\AppData\Local\Energia15\packages\energia\hardware\tivac\1.0.3\libraries`
2. In file `LCD_screen_font.h`, add the following to the end of line 48:
        || defined(__TM4C1294NCPDT__)
  This enables the large 12x8 font size (font size 3).
3. In file `Screen_K35_SPI.cpp`, add the following to the end of line 143:
        || defined(__TM4C1294NCPDT__)
  This selects the correct SPI port when connecting the Kentec display to "Booster Pack 1" connectors on the Connected LaunchPad.
4. In file `library.properties`, add the following to the end of line 9:
        ,tivac
  This keeps the IDE from complaining about a posssible incompatible library. 

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
