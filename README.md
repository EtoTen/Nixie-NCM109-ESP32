#ESP32 drop in upgrade for the NCM109 Nixie controller

![schematic](/doc/schematic-ncm109-esp32.jpg?raw=true "schematic")

Summary: 

The Arduino MCU on the NCM109 controller is super old school and sucks, so I replaced it with an ESP32...

Features:

 - NTP or GPS time based sync  
 - Proper RTC usage 
 - Reworked / optimized HV5222 SPI code  
 - ESP32  Wifi with WifiManager Captive Portal 
 - Selectabke timezone via up/down buttons

Mode button:
 - *>=1 second press -> show date 
 - *>=>20 second press -> reset all settings

Up button:
 - +1 time offset
 
Down button:
 - -1 time offset

Referenced / Related Code: 
 - [https://github.com/tverona1/ESP32_NTPNixieClock](https://github.com/tverona1/ESP32_NTPNixieClock)
 - [https://github.com/afch/NixieClock](https://github.com/afch/NixieClock)
