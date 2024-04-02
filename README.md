# Connected IoT Status Monitor

[![Arduino Compile Sketches](https://github.com/Andy4495/ConnectedStatusMonitor/actions/workflows/arduino-compile-sketches.yml/badge.svg)](https://github.com/Andy4495/ConnectedStatusMonitor/actions/workflows/arduino-compile-sketches.yml)
[![Check Markdown Links](https://github.com/Andy4495/ConnectedStatusMonitor/actions/workflows/CheckMarkdownLinks.yml/badge.svg)](https://github.com/Andy4495/ConnectedStatusMonitor/actions/workflows/CheckMarkdownLinks.yml)

Status display sketch and hardware to display various sensor data readings that are queried from the [ThingSpeak][6] IoT Platform.

The sketch is specifically designed for use with the Texas Instruments TM4C1294 [Connected LaunchPad][1], [Kentec Touch Display BoosterPack][2] (SPI version), [Futaba 162SD03][9] Vacuum Fluorescent Display, and SparkFun [Micro OLED Breakout][12].

While the sketch is written for a specific hardware configuration, it can be used as an example for the following:

- TCP and UDP connections using Ethernet
- [ArduinoJson][3] library for parsing JSON strings
- Reading data using [REST API][4] for [ThingsSpeak][6] IoT platform
- Arduino [Time Library][5]
- Retrieving NTP time
- Calculate Standard Time or Daylight Saving Time based on current day/time.
- Using a simple [photo resistor circuit][7] to detect when a room light is turned on
- Character Displays
- Bit-Mapped Displays

Parts of the code are messy and could be improved. However, the sketch does what I want, so they will probably stay as they are. In particular:

- I tried to come up with a creative solution for defining x/y coordinates for the display items. My solution ended up being cumbersome and difficult to maintain if the screen layout needs to change significantly.
- The DST conversion algorithm is optimized for U.S. time zones. It is not generalized for all time zone/DST cases.

## Light Detector Circuit

This display is located in a windowless workshop, and I only want the board querying the sensors database when I am in the workshop -- which is essentially any time the room light is on. In addition, the LCD backlight and VFD draw about 400 mA (out of a total 540 mA for the full setup), so I want to minimize power usage when I'm not in the workshop.

I constructed a simple light detection circuit using a 10K resistor and a cheap photo resistor (which probably came from an Arduino starter kit). The 10K and photo resistors are wired in series, with the 10K pulled to Vcc and the photo resistor connected to ground. Analog Pin A19 reads the voltage at the 10K/photo resistor connection (a simple voltage divider). Since it is just checking for a dark room vs. a lighted room, a threshold of half the ADC range is used.

## Vacuum Fluorescent Display

The [Futaba VFD][9] is enabled by a power control circuit with a logic-level MOSFET along with a [CD40109][11] tri-state buffer for the control signals to the VFD. The CLK, DATA, and RESET signals to the VFD are only active when the VFD is powered.

## SparkFun Micro OLED Display

The [SparkFun Micro OLED][12] is used to display the atmospheric pressure history. The past 64 readings are displayed graphically to show the pressure trend to predict the weather.

A modified version of the SparkFun Micro OLED [library][13] is required to work with the Tiva controllers.

The OLED is controlled using parallel-8080 mode. While the OLED also supports SPI and I2C serial control, the Energia Tiva SPI implementation is incompatible with the SparkFun library. I was also unable to get I2C mode to work on the TM4c129 LaunchPad and decided to use parallel mode since I had enough I/O available.

## Ethernet Status LEDs

The Connected LaunchPad supports Ethernet Link (LED4) and Activity (LED3) status indicators. However, I find these LEDs are bright and distracting. They are disabled by default. The code to enable them is in the sketch, but is commented out.

## External Libraries

- [ArduinoJson][3]
  - Sketch has been updated to suport the [version 7][17] API. Earlier versions of the library are not supported
  - Version 7 increased flash usage by about 1200 bytes, but this is still a small portion of the overall flash size (1 MB)
- [Arduino Time Library][5]
  - The code currently requires version 1.5 of the `Time` library. Version 1.6 and later use `pgm_read_ptr` instead of `pgm_read_word`, which causes a compile error with the Tiva board package
- [FutabaUsVfd162S Library][8]
- Modified [SparkFun Micro OLED Library][13]
  - This modified version is required in order to support compilation with the Tiva board
- [Kentec_35_SPI][16]
  - Version v2.0.0, modified from the library included with Tiva board package to support TM4C129 and this project configuration

## References

- Texas Instruments TM4C1294 Connected LaunchPad [EK-TM4C1294XL][1]
- Kentec Touch Display BoosterPack (SPI) [BOOSTXL-K350QVG-S1][2]
- Futaba [162SD03][9] Vacuum Fluorescent Display
- [CD40109][11] Buffer/Level-Shifter
- ThingsSpeak [IoT platform][6]
- ThingsSpeak REST API [documentation][4]
- Arduino Playground photo resistor [tutorial][7]
- SparkFun Micro OLED [Breakout][12]
- [Interface Board][14] containing system power, VFD power control, and ambient light sensor circuit

## License

The software and other files in this repository are released under what is commonly called the [MIT License][100]. See the file [`LICENSE`][101] in this repository.

[1]: http://www.ti.com/tool/EK-TM4C1294XL
[2]: http://www.ti.com/tool/boostxl-k350qvg-s1
[3]: https://arduinojson.org/
[4]: https://www.mathworks.com/help/thingspeak/read-data-from-channel.html
[5]: https://github.com/PaulStoffregen/Time
[6]: https://thingspeak.com/
[7]: https://playground.arduino.cc/Learning/PhotoResistor
[8]: https://github.com/Andy4495/FutabaVFD162S
[9]: ./extras/VFD-162.pdf
[11]: https://www.ti.com/lit/ds/symlink/cd40109b.pdf
[12]: https://www.sparkfun.com/products/13003
[13]: https://github.com/Andy4495/SparkFun_Micro_OLED_Arduino_Library
[14]: hardware/README.md
[16]: https://github.com/Andy4495/Kentec_35_SPI
[17]: https://arduinojson.org/news/2024/01/03/arduinojson-7/
[100]: https://choosealicense.com/licenses/mit/
[101]: ./LICENSE
[//]: # ([200]: https://github.com/Andy4495/ConnectedStatusMonitor)
[//]: # ( [9]: https://www.allelectronics.com/mas_assets/media/allelectronics2018/spec/VFD-162.pdf )
[//]: # ( [15]: https://arduinojson.org/v6/doc/upgrade/ )
