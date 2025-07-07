#ifndef DST_IL_H
#define DST_IL_H
/* dst.h
   Definitions for local daylight saving time. 
   These are specific to Illinois, USA.
   Manually edit for other locations.
*/

#define DST_FIRST_YEAR    2025
#define MAX_DST_YEAR      2035

#define DAYLIGHT_TZ_STRING "CDT"
#define STANDARD_TZ_STRING "CST"

#define DAYLIGHT_TZ -5    // Central Time (USA)
#define STANDARD_TZ -6    // Central Time (USA)

#define DST_EFFECTIVE_HOUR 2  // The early morning hour that DST starts or stops

// DST changeover dates (USA)
// From: https://aa.usno.navy.mil/data/daylight_time
// Data format: dst starting month, day, dst ending month, day
const int dst_info[] = {
  3,  9, 11, 2,        // 2025
  3,  8, 11, 1,        // 2026
  3, 14, 11, 7,        // 2027
  3, 12, 11, 5,        // 2028
  3, 11, 11, 4,        // 2029
  3, 10, 11, 3,        // 2030
  3,  9, 11, 2,        // 2031
  3, 14, 11, 7,        // 2032
  3, 13, 11, 6,        // 2033
  3, 12, 11, 5,        // 2034
  3, 11, 11, 4         // 2035
};
#endif
