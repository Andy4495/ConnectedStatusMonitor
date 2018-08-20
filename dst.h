#ifndef DST_IL_H
#define DST_IL_H
/* dst.h
   Definitions for local daylight saving time. 
   These are specific to Illinois, USA.
   Manually edit for other locations.
*/

#define DST_FIRST_YEAR    2018
#define MAX_DST_YEAR      2029

#define DAYLIGHT_TZ_STRING "CDT"
#define STANDARD_TZ_STRING "CST"

#define DAYLIGHT_TZ -5
#define STANDARD_TZ -6

const int dst_info[] = {
  3, 11, 11, 4,        // 2018
  3, 10, 11, 3,        // 2019
  3,  8, 11, 1,        // 2020
  3, 14, 11, 7,        // 2021
  3, 13, 11, 6,        // 2022
  3, 12, 11, 5,        // 2023
  3, 10, 11, 3,        // 2024
  3,  9, 11, 2,        // 2025
  3,  8, 11, 1,        // 2026
  3, 14, 11, 7,        // 2027
  3, 12, 11, 5,        // 2028
  3, 11, 11, 4         // 2029
};
#endif
