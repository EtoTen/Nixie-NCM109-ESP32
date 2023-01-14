#include <TimeLib.h>
#include <Timezone.h>


// Name of AP when configuring WiFi credentials
#define AP_NAME "NixieClock"

//hostname on the network
#define HOSTNAME "nixie-clock"

//every 15 minutes display the anti-poison
#define DISPLAY_POISON_INTERVAL 15

//every 10 minutes display the date
#define DISPLAY_DATE_INTERVAL 10

//check wifi every 8 minutes
#define WIFI_CHECK_INTERVAL 8

// retry to connnect to wifi 3 times
#define MAX_WIFI_RETRIES 3

// web config portal after connnecting to network
#define WIFI_WEB_PORTAL_ENABLED true

// Set to false for 24 hour time mode
#define HOUR_FORMAT_12 false

//how long to show the date for, in seconds
#define SHOW_DATE_SECONDS 15

//how long to show time offset for
#define SHOW_TIME_OFFSET_SECONDS .5

// Nixie tubes are turned off at night to increase their lifetime
// Clock off and on times are in 24 hour format
#define CLOCK_OFF_FROM_HOUR 3
#define CLOCK_OFF_TO_HOUR 10
#define ALWAYS_ON true // ignore off/on times

// Suppress leading zeros
// Set to false to having leading zeros displayed
#define SUPPRESS_LEADING_ZEROS false

#define USE_NTP_TIME true
#define USE_GPS_TIME false
#define WIFI_ENABLED true


// we are not using the buzzer in this sketch
#define BUZZER_PIN 14 // pin 04 on atmega328 | 2 on NCM109 sketch
#define PLAY_SOUNDS false // sounds on/off

// analog buttons
// These have a 10k 3v3 pullup
#define MODE_BUTTON  35// pin 23 on atmega328 | A0 on NCM109 sketch
#define DOWN_BUTTON 32 // pin 24 on atmega328 | A1 on NCM109 sketch
#define UP_BUTTON  34// pin 25 on atmega328 | A2 on NCM109 sketch

// Temperature Sensor
#define EXT_TEMP_SENSOR 4 // pin 26 on atmega328 | A3 on NCM109 sketch

// OFfset for time
#define DEFAULT_TIME_OFFSET -8;


// ***************************************************************
// End of user configuration items
// ***************************************************************


void updateDisplay(void);
//const char* wl_status_to_string(wl_status_t);
const char* wl_status_to_string(uint8_t);
void write_time_offset(void);