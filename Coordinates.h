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
  Coord BatSlimValue;
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
const char inHG[] = "in HG";
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
  WeatherTitle.x =          0;  // Centered = 78
  WeatherTitle.y =          0;
  WeatherTempValue.x =    143;  // "100.1 " -> 6 chars * 12 = 72 -> 215 - 72 = 143
  WeatherTempValue.y =     16;
  WeatherTempUnits.x =    215;
  WeatherTempUnits.y =     16;
  WeatherLuxValue.x =      71;  // "99,999,999 " -> 11 chars * 12 = 132 -> 203 - 132 = 71
  WeatherLuxValue.y =      32;
  WeatherLuxUnits.x =     203;
  WeatherLuxUnits.y =      32;
  WeatherRHValue.x =      143;  // "58.2 " -> 5 chars * 12 = 60 -> 203 - 60 = 143
  WeatherRHValue.y =       48;
  WeatherRHUnits.x =      203;
  WeatherRHUnits.y =       48;
  WeatherPValue.x =       107;  // "29.32 " -> 6 chars * 12 = 72 -> 179 - 72 = 107
  WeatherPValue.y =        64;
  WeatherPUnits.x =       179;
  WeatherPUnits.y =        64;
  SlimTitle.x =             0;  // Centered = 96
  SlimTitle.y =            80;
  SlimTempValue.x =       143;  // "100.1 " -> 6 chars * 12 = 72 -> 215 - 72 = 143
  SlimTempValue.y =        96;
  SlimTempUnits.x =       215;
  SlimTempUnits.y =        96;
  WorkshopTitle.x =         0;  // Centered = 72
  WorkshopTitle.y =       112;
  WorkshopTempValue.x =   143;  // "100.1 " -> 6 chars * 12 = 72 -> 215 - 72 = 143
  WorkshopTempValue.y =   128;
  WorkshopTempUnits.x =   215;
  WorkshopTempUnits.y =   128;
  GDTitle.x =               0;  // Centered = 54
  GDTitle.y =             144;
  GDValue.x =             167;  // "Closed" -> 6 chars * 12 = 72 -> 239 - 72 = 167
  GDValue.y =             160;
  BattTitle.x =             0;  // Centered = 66
  BattTitle.y =           176;
  BattOutdoorSubtitle.x =  24; 
  BattOutdoorSubtitle.y = 192;
  BattOutdoorValue.x =    155;  // "3.123 " -> 6 chars * 12 = 72 -> 227 - 72 = 155
  BattOutdoorValue.y =    192;
  BattOutdoorUnits.x =    227;
  BattOutdoorUnits.y =    192;
  BattSlimSubtitle.x =     24;
  BattSlimSubtitle.y =    208;
  BatSlimValue.x =        155;  // "3.123 " -> 6 chars * 12 = 72 -> 227 - 72 = 155
  BatSlimValue.y =        208;
  BattSlimUnits.x =       227;
  BattSlimUnits.y =        208;
  BattWorkshopSubtitle.x =  24;
  BattWorkshopSubtitle.y = 224;
  BattWorkshopValue.x =    155;  // "3.123 " -> 6 chars * 12 = 72 -> 227 - 72 = 155
  BattWorkshopValue.y =    224;
  BattWorkshopUnits.x =    227;
  BattWorkshopUnits.y =    224;
  BattSensor5Subtitle.x =   24;
  BattSensor5Subtitle.y =  240;
  BattSensor5Value.x =     155;  // "3.123 " -> 6 chars * 12 = 72 -> 227 - 72 = 155
  BattSensor5Value.y =     240;
  BattSensor5Units.x =     227;
  BattSensor5Units.y =     240;
  TimeAndDateTitle.x =       0;  // Centered = 42
  TimeAndDateTitle.y =     256;
  TimeAndDateValue.x =      47;  // "14-Jun hh:mm:ssZ" -> 16 chars * 12 = 192 -> 239 - 192 = 47
  TimeAndDateValue.y =     272;
}

