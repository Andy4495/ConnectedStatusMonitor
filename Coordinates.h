struct Coord {
  int x;
  int y;
};

struct Layout {
  Layout();
  Coord WeatherTitle;
  Coord WeatherTempValue;
  Coord WeatherTempUnits;
  Coord WeatherLuxValue;
  Coord WeatherLuxUnits;
  Coord WeatherRHValue;
  Coord WeatherRHUnits;
  Coord WeatherPValue;
  Coord WeatherPUnits;
  Coord SlimTitle;
  Coord SlimTempValue;
  Coord SlimTempUnits;
  Coord WorkshopTitle;
  Coord WorkshopTempValue;
  Coord WorkshopTempUnits;
  Coord GDTitle;
  Coord GDValue;
  Coord BattTitle;
  Coord BattOutdoorSubtitle;
  Coord BattOutdoorValue;
  Coord BattOutdoorUnits;
  Coord BattSlimSubtitle;
  Coord BattSlimValue;
  Coord BattSlimUnits;
  Coord BattWorkshopSubtitle;
  Coord BattWorkshopValue;
  Coord BattWorkshopUnits;
  Coord BattSensor5Subtitle;
  Coord BattSensor5Value;
  Coord BattSensor5Units;
  Coord TimeAndDateTitle;
  Coord TimeAndDateValue;
};

const char WeatherTitle[] = "Weather";
const char SlimTitle[] = "Slim";
const char WorkshopTitle[] = "Workshop";
const char GDTitle[] = "Garage Door";
const char BatteriesTitle[] = "Batteries";
const char TimeAndDateTitle[] = "Time and Date";
const char DegreesF[] = {176, 'F', 0};
const char Lux[] = "LUX";
const char RH[] = "%RH";
const char inHG[] = "inHg";
const char V[] = "V";
const char OutdoorSubtitle[]  = "Outdoor:";
const char SlimSubtitle[]     = "Slim:";
const char WorkshopSubtitle[] = "Workshop:";
const char Sensor5Subtitle[]  = "Sensor5:";

#define FONT_SIZE_X           12
#define FONT_SIZE_Y           16
#define SCREEN_WIDTH_X       320
#define SCREEN_WIDTH_Y       240

Layout::Layout() {
  WeatherTitle.x =           0;  // Centered = 78
  WeatherTitle.y =           0;
  // Align all weather values relative to longest units ("inHG") at 191
  WeatherTempValue.x =     119;  // "100.1 " -> 6 chars * 12 = 72 -> 191 - 72 = 119
  WeatherTempValue.y =      16;
  WeatherTempUnits.x =     215;
  WeatherTempUnits.y =      16;
  WeatherLuxValue.x =       83;  // "99999999 " -> 9 chars * 12 = 108 -> 191 - 108 = 83
  WeatherLuxValue.y =       32;
  WeatherLuxUnits.x =      203;
  WeatherLuxUnits.y =       32;
  WeatherRHValue.x =       131;  // "58.2 " -> 5 chars * 12 = 60 -> 191 - 60 = 131
  WeatherRHValue.y =        48;
  WeatherRHUnits.x =       203;
  WeatherRHUnits.y =        48;
  WeatherPValue.x =        119;  // "29.32 " -> 6 chars * 12 = 72 -> 191 - 72 = 119
  WeatherPValue.y =         64;
  WeatherPUnits.x =        191;
  WeatherPUnits.y =         64;
  SlimTitle.x =              0;  // Centered = 96
  SlimTitle.y =             96;  // Use y-value of 80 to put title on separate line
  SlimTempValue.x =        143;  // "100.1 " -> 6 chars * 12 = 72 -> 215 - 72 = 143
  SlimTempValue.y =         96;
  SlimTempUnits.x =        215;
  SlimTempUnits.y =         96;
  WorkshopTitle.x =          0;  // Centered = 72
  WorkshopTitle.y =        128;  // Use y-value of 112 to put title on separate line
  WorkshopTempValue.x =    143;  // "100.1 " -> 6 chars * 12 = 72 -> 215 - 72 = 143
  WorkshopTempValue.y =    128;
  WorkshopTempUnits.x =    215;
  WorkshopTempUnits.y =    128;
  GDTitle.x =                0;  // Centered = 54
  GDTitle.y =              160;  // Use y-value of 144 to put title on separate line
  GDValue.x =              167;  // "Closed" -> 6 chars * 12 = 72 -> 239 - 72 = 167
  GDValue.y =              160;
  BattTitle.x =              0;  // Centered = 66
  BattTitle.y =            192;
  BattOutdoorSubtitle.x =   24; 
  BattOutdoorSubtitle.y =  208;
  BattOutdoorValue.x =     155;  // "3.123 " -> 6 chars * 12 = 72 -> 227 - 72 = 155
  BattOutdoorValue.y =     208;
  BattOutdoorUnits.x =     227;
  BattOutdoorUnits.y =     208;
  BattSlimSubtitle.x =      24;
  BattSlimSubtitle.y =     224;
  BattSlimValue.x =        155;  // "3.123 " -> 6 chars * 12 = 72 -> 227 - 72 = 155
  BattSlimValue.y =        224;
  BattSlimUnits.x =        227;
  BattSlimUnits.y =        224;
  BattWorkshopSubtitle.x =  24;
  BattWorkshopSubtitle.y = 240;
  BattWorkshopValue.x =    155;  // "3.123 " -> 6 chars * 12 = 72 -> 227 - 72 = 155
  BattWorkshopValue.y =    240;
  BattWorkshopUnits.x =    227;
  BattWorkshopUnits.y =    240;
  BattSensor5Subtitle.x =   24;
  BattSensor5Subtitle.y =  256;
  BattSensor5Value.x =     155;  // "3.123 " -> 6 chars * 12 = 72 -> 227 - 72 = 155
  BattSensor5Value.y =     256;
  BattSensor5Units.x =     227;
  BattSensor5Units.y =     256;
  TimeAndDateTitle.x =       0;  // Centered = 42
  TimeAndDateTitle.y =     270;
  TimeAndDateValue.x =       5;  // "14-Jun hh:mm AM CDT" -> 19 chars * 12 = 228 -> 239 - 228 = 11 to Right Justify
                                 // Otherewise, use 11/2 = 5 to Center
  TimeAndDateValue.y =     304;  // If displaying Time and Date header, then use y value of at least 272
                                 // With no header, y-value of :
                                 //                 256 displays time and date in the next line under batteries section
                                 //                 304 displays time and date on last line of display
}

