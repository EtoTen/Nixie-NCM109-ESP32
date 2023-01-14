#include "main.h"
#include <Arduino.h>
#include <driver/dac.h>
#include <WiFi.h>
#include <SPI.h>
#include <TimeLib.h>
#include <Time.h>
#include <Timezone.h>
#include <ESPmDNS.h>
#include <WiFiManager.h> //https://github.com/tzapu/WiFiManager
#include "LEDControl.h"
#include "SPI_Nixie_Interface.h"
#include "NTP_time.h"
#include "GPS_time.h"
#include "RTC_time.h"

#include <stdio.h>
#include <stdlib.h>

#include "ArduinoNvs.h"
#include "Button2.h"

#if PLAY_SOUNDS
#include <melody_player.h>
#include <melody_factory.h>
#endif

#if PLAY_SOUNDS
// audio
String melodyFilePath = "/pokemon.rtttl";
MelodyPlayer player(BUZZER_PIN);
#endif

// Logging
static const char *TAG = "main";

// Instantiate the Nixie Tube NIXIE_Interface object
SPINixieInterface NIXIE_Interface;

// Instantiate the WifiManager object
WiFiManager wifiManager;

RTC *rtcTime;

// ***************************************************************
// Utility Functions
// ***************************************************************

// Misc variables
int previousMinutePoison = 0;
int previousMinuteDate = 0;
int minutes = 0;
boolean dotToggle = false;
boolean clockOn = true;
boolean showingAntiPoison = false;
boolean showingTime = false;
boolean showingOffset = false;
boolean showingDate = false;
boolean factoryReset = false;
int32_t time_offset;
bool alreadyOff = false;

boolean firstConnection = false;
Button2 buttonMode, buttonUp, buttonDown;
hw_timer_t *mButtonTimer = NULL;

time_t localTime;
time_t utcTime;

unsigned long lastTimeDotChange = millis();
time_t lastRunDateDisplay;
time_t lastRunAntiPoisonDisplay;
time_t lastRunWifiCheck;

long modeStart = 0.0;
bool modeStarted = false;

bool resNVS; // NVS Result

void write_time_offset()
{
  resNVS = NVS.setInt("time_offset", time_offset + 500, 1);
}

boolean isHourInRange(int a, int b, int c) {
  if (b > c) { // if the range spans midnight
    return a >= b || a <= c; // return true if a is greater or equal to b or less than or equal to c
  } else {
    return a >= b && a <= c; // otherwise, return true if a is greater or equal to b and less than or equal to c
  }
}

void printTimeToSerial(time_t myTime)
{
  Serial.print(hour(myTime));
  Serial.print(":");
  if (minute(myTime) < 10)
    Serial.print('0');
  Serial.print(minute(myTime));
  Serial.print(":");
  if (second(myTime) < 10)
    Serial.print('0');
  Serial.println(second(myTime));
}

// from WifiManager
/** IP to String? */
String toStringIp(IPAddress ip)
{
  String res = "";
  for (int i = 0; i < 3; i++)
  {
    res += String((ip >> (8 * i)) & 0xFF) + ".";
  }
  res += String(((ip >> 8 * 3)) & 0xFF);
  return res;
}

void turnOffDisplay()
{

  // turn off the dots
  NIXIE_Interface.dotsEnable(false);
  // turn off the digits
  NIXIE_Interface.setNX1Digit(DIGIT_BLANK);
  NIXIE_Interface.setNX2Digit(DIGIT_BLANK);
  NIXIE_Interface.setNX3Digit(DIGIT_BLANK);
  NIXIE_Interface.setNX4Digit(DIGIT_BLANK);
  NIXIE_Interface.setNX5Digit(DIGIT_BLANK);
  NIXIE_Interface.setNX6Digit(DIGIT_BLANK);
  // turn off the LEDs
  NIXIE_Interface.setLEDColor(black);

  unsigned long lastChange = micros();

  while (micros() - lastChange < (5 * 1000000))
  {
    NIXIE_Interface.show();
  }
}

void doAntiPoisoning(void)
{
  Serial.println("doAntiPoisoning started");
  bool dots = false;
  NIXIE_Interface.dotsEnable(false);

  for (int k = 0; k < ANTI_POISON_ITERATIONS; k++)
  {
    for (int j = 0; j < 4; j++)
    {
      for (int i = 0; i < 11; i++)
      {
        NIXIE_Interface.setNX1Digit(i);
        NIXIE_Interface.setNX2Digit(i);
        NIXIE_Interface.setNX3Digit(i);
        NIXIE_Interface.setNX4Digit(i);
        NIXIE_Interface.setNX5Digit(i);
        NIXIE_Interface.setNX6Digit(i);
        // Make the changes visible
        NIXIE_Interface.show();
      }
    }
    NIXIE_Interface.setLEDColor(k % 3 == 0 ? 255 : 0, k % 3 == 1 ? 255 : 0, k % 3 == 2 ? 255 : 0);
    dots = !dots;
    NIXIE_Interface.dotsEnable(dots);
  }

  Serial.println("doAntiPoisoning ended");
}

void showTime()
{

  // Display the time

  int now_hour;
  float colorInc;

  // Set the LED's color depending upon the hour
  // Get the current hour
  if (HOUR_FORMAT_12)
  {
    // Using 12 hour format
    now_hour = hourFormat12(localTime);
    // Calculate the color of the LEDs based on the hour in 12 hour format
    colorInc = 256 / 12.0;
  }
  else
  {
    // Using 24 hour format
    now_hour = hour(localTime);
    // Calculate the color of the LEDs based on the hour in 24 hour format
    colorInc = 256 / 24.0;
  }
  // Set the NIXIE_Interfaces's LED color
  NIXIE_Interface.setLEDColor(NIXIE_Interface.colorWheel(colorInc * now_hour));

  // Display the NX1 digit
  if (now_hour >= 10)
  {
    NIXIE_Interface.setNX1Digit(now_hour / 10);
  }
  else
  {
    if (SUPPRESS_LEADING_ZEROS)
    {
      NIXIE_Interface.setNX1Digit(DIGIT_BLANK);
    }
    else
    {
      NIXIE_Interface.setNX1Digit(0);
    }
  }
  // Display the NX2 digit
  NIXIE_Interface.setNX2Digit(now_hour % 10);

  // Get the current minute
  int now_min = minute(localTime);

  // Display the NX3 digit
  NIXIE_Interface.setNX3Digit(now_min / 10);

  // Display the NX4 digit
  NIXIE_Interface.setNX4Digit(now_min % 10);

  // Get the current second
  int now_sec = second(localTime);

  // Display the NX5 digit
  NIXIE_Interface.setNX5Digit(now_sec / 10);

  // Display the NX6 digit
  NIXIE_Interface.setNX6Digit(now_sec % 10);

  if ((millis() - lastTimeDotChange) > 10000)
  {
    NIXIE_Interface.switchdDots();
    lastTimeDotChange = millis();
  }
  /* //for testing
      NIXIE_Interface.setNX1Digit(1);
      NIXIE_Interface.setNX2Digit(1);
      NIXIE_Interface.setNX3Digit(1);
      NIXIE_Interface.setNX4Digit(1);
      NIXIE_Interface.setNX5Digit(1);
      NIXIE_Interface.setNX6Digit(1);
  */
  // Display time on clock
  NIXIE_Interface.show();
}

void showDate()
{
  Serial.println("Date Display Started");

  // Turn off dots
  // NIXIE_Interface.dotsEnable(false);

  // Set all LEDs to blue to indicate date display
  NIXIE_Interface.setLEDColor(red);
  NIXIE_Interface.dotsEnable(true);

  // Get the current month 1..12
  int now_mon = month(localTime);

  // Display the NX1 digit
  if (now_mon >= 10)
  {
    NIXIE_Interface.setNX1Digit(now_mon / 10);
  }
  else
  {
    if (SUPPRESS_LEADING_ZEROS)
    {
      NIXIE_Interface.setNX1Digit(DIGIT_BLANK);
    }
    else
    {
      NIXIE_Interface.setNX1Digit(0);
    }
  }

  // Display the NX2 digit
  NIXIE_Interface.setNX2Digit(now_mon % 10);

  // Get the current day 1..31
  int now_day = day(localTime);

  // Display the NX3 digit
  if (now_day >= 10)
  {
    NIXIE_Interface.setNX3Digit(now_day / 10);
  }
  else
  {
    if (SUPPRESS_LEADING_ZEROS)
    {
      NIXIE_Interface.setNX3Digit(DIGIT_BLANK);
    }
    else
    {
      NIXIE_Interface.setNX3Digit(0);
    }
  }
  // Display the NX4 digit
  NIXIE_Interface.setNX4Digit(now_day % 10);

  // Get the current year
  int now_year = year(localTime) - 2000;

  // Display the NX5 digit
  if (now_year >= 10)
  {
    NIXIE_Interface.setNX5Digit(now_year / 10);
  }
  else
  {
    if (SUPPRESS_LEADING_ZEROS)
    {
      NIXIE_Interface.setNX5Digit(DIGIT_BLANK);
    }
    else
    {
      NIXIE_Interface.setNX5Digit(0);
    }
  }
  // Display the NX6 digit
  NIXIE_Interface.setNX6Digit(now_year % 10);
  // Delay for date display
  unsigned long lastChange = micros();

  while (micros() - lastChange < (SHOW_DATE_SECONDS * 1000000))
  {
    NIXIE_Interface.show();
  }

  Serial.println("Date Display Ended");
}

void showTimeOffset()
{
  Serial.println("showTimeOffset Started");

  Serial.print("time_offset: ");
  Serial.println(time_offset);

  NIXIE_Interface.setLEDColor(blue);
  NIXIE_Interface.dotsEnable(false);

  if (time_offset == 0)
  {
    NIXIE_Interface.setNX1Digit(DIGIT_BLANK);
    NIXIE_Interface.setNX2Digit(DIGIT_BLANK);
    NIXIE_Interface.setNX3Digit(0);
    NIXIE_Interface.setNX4Digit(0);
    NIXIE_Interface.setNX5Digit(DIGIT_BLANK);
    NIXIE_Interface.setNX6Digit(DIGIT_BLANK);
  }

  else if (time_offset > 0)
  {
    NIXIE_Interface.setNX1Digit(DIGIT_BLANK);
    NIXIE_Interface.setNX2Digit(DIGIT_BLANK);
    NIXIE_Interface.setNX3Digit(DIGIT_BLANK);
    NIXIE_Interface.setNX4Digit(DIGIT_BLANK);
    // Display the NX5-6 digit
    if (time_offset >= 10)
    {
      NIXIE_Interface.setNX5Digit(time_offset / 10);
    }
    else
    {
      if (SUPPRESS_LEADING_ZEROS)
      {
        NIXIE_Interface.setNX5Digit(DIGIT_BLANK);
      }
      else
      {
        NIXIE_Interface.setNX5Digit(0);
      }
    }

    // Display the NX6 digit
    NIXIE_Interface.setNX6Digit(time_offset % 10);
  }

  else if (time_offset < 0)
  { // Display the NX1 digit
    if (time_offset <= -10)
    {
      NIXIE_Interface.setNX1Digit(abs(time_offset / -10));
    }
    else
    {
      if (SUPPRESS_LEADING_ZEROS)
      {
        NIXIE_Interface.setNX1Digit(DIGIT_BLANK);
      }
      else
      {
        NIXIE_Interface.setNX1Digit(0);
      }
    }
    // Display the NX2 digit
    NIXIE_Interface.setNX2Digit(abs(time_offset % -10));

    NIXIE_Interface.setNX3Digit(DIGIT_BLANK);
    NIXIE_Interface.setNX4Digit(DIGIT_BLANK);
    NIXIE_Interface.setNX5Digit(DIGIT_BLANK);
    NIXIE_Interface.setNX6Digit(DIGIT_BLANK);
  }

  // Delay for date display
  unsigned long lastChange = micros();

  while (micros() - lastChange < (SHOW_TIME_OFFSET_SECONDS * 1000000))
  {
    NIXIE_Interface.show();
  }

  Serial.println("TimeOffset Display Ended");
}

void updateDisplay(void)
{

  // Determine if clock should be on or off
  // Clock is on between these hours

  if (!(isHourInRange(hour(localTime),CLOCK_OFF_FROM_HOUR,CLOCK_OFF_TO_HOUR)) || ALWAYS_ON)
  {
    if (!clockOn)
    {
      clockOn = true;
      Serial.println("Clock is On");
    }
  }
  else
  {
    // allow time offset to be displayed if changed
    // this is less confusing

    if (clockOn)
    {
      clockOn = false;
      turnOffDisplay();
      Serial.println("Clock is Off");
    }

    if (showingOffset)
    {
      write_time_offset(); // write offset to NVS on display
      showTimeOffset();
      showingOffset = false;
      clockOn = true;
    }
    showingDate = false;
    showingAntiPoison = false;
    return;
  }

  // show the date every X minutes
  if (((utcTime - lastRunDateDisplay) / 60) >= DISPLAY_DATE_INTERVAL)
  {
    lastRunDateDisplay = utcTime;
    showingDate = true;
  }
  // show the anti-poison every X minutes
  if (((utcTime - lastRunAntiPoisonDisplay) / 60) >= DISPLAY_POISON_INTERVAL)
  {
    lastRunAntiPoisonDisplay = utcTime;
    showingAntiPoison = true;
  }

  // do anti-poison every 25 minutes
  else if ((minutes != previousMinutePoison) && (minutes != 0) && ((minutes % 25) == 0))
  {
    previousMinutePoison = minutes;
    showingAntiPoison = true;
  }
  else
  {
    showingTime = true;
  }

  if (showingAntiPoison)
  {
    doAntiPoisoning();
    showingAntiPoison = false;
  }
  else if (showingDate)
  {
    showDate();
    showingDate = false;
  }
  else if (showingOffset)
  {
    write_time_offset(); // write offset to NVS on display
    showTimeOffset();
    showingOffset = false;
  }
  else if (showingTime)
  {
    showTime();
    showingTime = false;
  }
}
bool hasWifiBeenConfigured()
{
  uint8_t empty_bssid[6] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
  wifi_config_t config = {};

  if (esp_wifi_get_config(WIFI_IF_STA, &config) != ESP_OK)
    return false;
  ESP_LOGI(TAG, "SSID: %s, BSSID: " MACSTR, config.sta.ssid, MAC2STR(config.sta.bssid));
  return (strlen((const char *)config.sta.ssid) >= 1) || (memcmp(empty_bssid, config.sta.bssid, sizeof(empty_bssid)) != 0);
}

// Get WiFi SSID
String WIFI_GetSSID()
{
  String mSSID = "";
  if (!hasWifiBeenConfigured())
  {
    Serial.println("Wifi Has not been configured. Returning blank SSID.");
  }
  else
  {
    wifi_config_t conf;
    esp_wifi_get_config(WIFI_IF_STA, &conf);
    mSSID = String(reinterpret_cast<const char *>(conf.sta.ssid));
  }
  return mSSID;
}

// Get WiFi Password
String WIFI_GetPassword()
{
  String mPassword = "";
  if (!hasWifiBeenConfigured())
  {
    Serial.println("Wifi Has not been configured. Returning blank password.");
  }
  else
  {
    wifi_config_t conf;
    esp_wifi_get_config(WIFI_IF_STA, &conf);
    mPassword = String(reinterpret_cast<const char *>(conf.sta.password));
  }
  return mPassword;
}
const char *wl_status_to_string(uint8_t status)
{

  switch (status)
  {
  case WL_NO_SHIELD:
    return "WL_NO_SHIELD";
  case WL_IDLE_STATUS:
    return "WL_IDLE_STATUS";
  case WL_NO_SSID_AVAIL:
    return "WL_NO_SSID_AVAIL";
  case WL_SCAN_COMPLETED:
    return "WL_SCAN_COMPLETED";
  case WL_CONNECTED:
    return "WL_CONNECTED";
  case WL_CONNECT_FAILED:
    return "WL_CONNECT_FAILED";
  case WL_CONNECTION_LOST:
    return "WL_CONNECTION_LOST";
  case WL_DISCONNECTED:
    return "WL_DISCONNECTED";
  }
  return "Unknown error";
}
// Attempt to connect to WiFi
boolean WIFI_Connect()
{
  WiFi.disconnect();
  WiFi.setHostname(HOSTNAME);
  WiFi.mode(WIFI_STA);
  Serial.println("Connecting to WiFi...");
  Serial.print("SSID: ");
  Serial.println(WIFI_GetSSID().c_str());
  Serial.print("Password: ");
  Serial.println(WIFI_GetPassword().c_str());
  // bool res = wifiManager.autoConnect((const char *)WIFI_GetSSID().c_str(), (const char *)WIFI_GetPassword().c_str()); // password protected ap
  //  if (WiFi.begin(WIFI_GetSSID().c_str(), WIFI_GetPassword().c_str()) != WL_CONNECTED)
  WiFi.begin(WIFI_GetSSID().c_str(), WIFI_GetPassword().c_str());
  for (int x = 0; x < MAX_WIFI_RETRIES; x++)
  {
    if (WiFi.waitForConnectResult(10000) != WL_CONNECTED)
    {
      Serial.println("Failed to connect to WiFi");
    }
    else
    {
      Serial.println("Connected to WiFi");
#if WIFI_WEB_PORTAL_ENABLED
      // this seems to add minor display lag
      // start web portal
      if (!MDNS.begin(HOSTNAME))
      {
        Serial.println("Error setting up MDNS responder!");
      }
      wifiManager.setConfigPortalBlocking(false);
      wifiManager.startWebPortal();
#endif
      Serial.print("RRSI: ");
      Serial.println(WiFi.RSSI());
      Serial.print("Local IP: ");
      Serial.println(toStringIp(WiFi.localIP()));
      return true;
    }
  }
  return false;
}

void longModeClickStart(Button2 &btn)
{
  modeStart = micros();
}

void longModeClickStop(Button2 &btn)
{
  // dirty fix for button getting triggered on initialization
  if (!modeStarted)
  {
    modeStarted = true;
    return;
  }

  long microssSinceLast = (micros() - modeStart);
  long secondsSinceLast = microssSinceLast / 1000000;

  if (secondsSinceLast > 20)
  {
    factoryReset = true;
  }
  else if (secondsSinceLast > 0.0)
  {
    Serial.println("Showing date...");
    showingDate = true;
  }
}

void buttonHandler(Button2 &btn)
{
  if (btn == buttonUp)
  {
    if (showingOffset || showingDate || showingAntiPoison)
    {
      Serial.println("showing showingOffset||showingDate||showingAntiPoison, ignoring futher changes until completed");
      return;
    }

    if (time_offset < 24)
    {
      time_offset += 1;
    }
    else
    {
      Serial.println("maximal time_offset reached, not increasing further");
    }
    showingOffset = true;
  }
  else if (btn == buttonDown)
  {
    if (showingOffset || showingDate || showingAntiPoison)
    {
      Serial.println("showing showingOffset||showingDate||showingAntiPoison, ignoring futher changes until completed");
      return;
    }
    if (time_offset > -24)
    {
      time_offset -= 1;
    }
    else
    {
      Serial.println("minimal time_offset reached, not decreasing further");
    }
    showingOffset = true;
  }
}
void IRAM_ATTR buttonTimer()
{
  // proccess buttons
  buttonMode.loop();
  buttonUp.loop();
  buttonDown.loop();
}

void setup()
{

  // Configure serial interface
  Serial.begin(115200);
  delay(800); // alows serial to start, or else initial messages are lost

  Serial.println("Starting...");

  pinMode(SPI_SS, OUTPUT);
  SPI.begin(SPI_SCLK, SPI_MISO, SPI_MOSI, SPI_SS);
  SPI.setDataMode(SPI_MODE2);          // Mode 2 SPI
  SPI.setClockDivider(SPI_CLOCK_DIV8); // per HV2222 datasheet, fCLK= 8.0MHz
  // LSBFIRST | MSBFIRST
  SPI.beginTransaction(SPISettings(SPI_CLOCK_DIV8, LSBFIRST, SPI_MODE2));
  Serial.println("HV5222 initialized SPI");

  rtcTime = rtcTime->getInstance();

  utcTime = rtcTime->getRTCTime();

  lastRunDateDisplay = utcTime;
  lastRunAntiPoisonDisplay = utcTime;
  lastRunWifiCheck = utcTime;

  // Storage retrival of timezone offset from NVS Storage.
  time_offset = DEFAULT_TIME_OFFSET;
  NVS.begin();
  int nvsTimeOffset = NVS.getInt("time_offset");
  Serial.print("nvsTimeOffset: ");
  Serial.println(nvsTimeOffset);

  if (nvsTimeOffset == 0 || nvsTimeOffset < 400)
  {
    Serial.println("time_offset not found in NVS, using default");
  }
  else
  {
    time_offset = nvsTimeOffset - 500;
  }
  Serial.print("time_offset: ");
  Serial.println(time_offset);

  // start up blank
  turnOffDisplay();

  // buttons start

  buttonMode.begin(MODE_BUTTON, INPUT, false);

  buttonMode.setLongClickHandler(longModeClickStart);
  buttonMode.setLongClickDetectedHandler(longModeClickStop);

  buttonUp.begin(UP_BUTTON, INPUT, false);
  buttonUp.setTapHandler(buttonHandler);

  buttonDown.begin(DOWN_BUTTON, INPUT, false);
  buttonDown.setTapHandler(buttonHandler);

  mButtonTimer = timerBegin(0, 80, true);
  timerAttachInterrupt(mButtonTimer, &buttonTimer, true);
  timerAlarmWrite(mButtonTimer, 10000, true); // every 0.1 seconds
  timerAlarmEnable(mButtonTimer);
  // buttons end

  //  Reset saved settings for testing purposes
  //  Should be commented out for normal operation
  //  wifiManager.resetSettings();

#if WIFI_ENABLED

  wifiManager.setHostname(HOSTNAME);
  wifiManager.setCountry("US");
  wifiManager.setDebugOutput(true);

  WiFi.disconnect();
  WiFi.mode(WIFI_STA);
  WiFi.persistent(true);
  WiFi.setSleep(false);
  WiFi.setTxPower(WIFI_POWER_19_5dBm); // Set WiFi RF power output to highest level

  // If WiFi setup is not configured, start access point
  // Otherwise, connect to WiFi

  if (!hasWifiBeenConfigured())
  {
    Serial.println("Starting Wifi AP");
    // wifiManager.resetSettings();
    // wifiManager.erase();
    wifiManager.setConfigPortalBlocking(false);
    wifiManager.startConfigPortal(AP_NAME);
    firstConnection = true;
  }
  else
  {
    Serial.println("Starting Wifi Client");
    /* wifiManager.setConnectRetries(4);
     wifiManager.setConnectTimeout(60);
     wifiManager.setWiFiAutoReconnect(false);
     */
    WIFI_Connect();
  }

#endif

#if USE_NTP_TIME && WIFI_ENABLED
  if (hasWifiBeenConfigured())
    initNTP(NIXIE_Interface);
#endif
#if USE_GPS_TIME
  initGPSTime(NIXIE_Interface);
#endif

#if PLAY_SOUNDS
  // init the filesystem
  SPIFFS.begin();
  Serial.print("Loading melody from file... ");
  Melody melody = MelodyFactory.loadRtttlFile(melodyFilePath);
  if (!melody)
  {
    Serial.println("Error");
  }
  else
  {
    Serial.println("Done!");

    Serial.print("Playing ");
    Serial.print(melody.getTitle());
    Serial.print("... ");
    player.play(melody);
    Serial.println("Done!");
  }

#endif

  // Do the anti poisoning routine
  // showingAntiPoison = true;

  Serial.println("Ready!");
}

// ***************************************************************
// Program Loop
// ***************************************************************

void loop()
{
  utcTime = rtcTime->getRTCTime();
  // utcTime = now();
  //  apply the time zone offset to get local time
  localTime = utcTime + (time_offset * SECS_PER_HOUR);

  if (factoryReset)
  {
    factoryReset = false;
    wifiManager.resetSettings();
    NVS.eraseAll();
    Serial.print("It's called Daisy. Daisy, Daisy, give me your answer do. I'm half crazy all for the love of you... ");
    ESP.restart();
    return;
  }

#if WIFI_ENABLED

  // Check WiFi connectivity and restart it if it fails.
  if ((((utcTime - lastRunWifiCheck) / 60) >= WIFI_CHECK_INTERVAL) && (hasWifiBeenConfigured()))
  {
    lastRunWifiCheck = utcTime;
    Serial.println("Checking WiFi connection...");
    if (WiFi.status() != WL_CONNECTED)
    {
      Serial.println("WiFi connection was lost. Reconnecting...");
      WIFI_Connect();
    }
    else
    {
      Serial.println("Wifi is already connected.");
    }
  }
  if (wifiManager.getWebPortalActive() || wifiManager.getConfigPortalActive())
    wifiManager.process();
#endif

// once we have our connection established, we can initiate  NTP
#if USE_NTP_TIME && WIFI_ENABLED
  if (firstConnection)
  {
    if (WiFi.status() == WL_CONNECTED)
    {
      firstConnection = false;
      initNTP(NIXIE_Interface);
    }
  }
#endif

#if WIFI_ENABLED
  if (WiFi.softAPgetStationNum() > 0)
  {
    if (!alreadyOff)
      turnOffDisplay();
    alreadyOff = true;
  }
  else
  {
    updateDisplay();
    alreadyOff = false;
  }

#else
  updateDisplay();
#endif
}
