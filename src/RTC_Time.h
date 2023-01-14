#ifndef RTC_TIME_H
#define RTC_TIME_H

#include <Wire.h>
#include "RTClib.h"
#include <time.h>
#include <TimeLib.h>

#define I2C_SDA 22 // pin 27 on atmega328 | SDA GIOP21 on ESP32
#define I2C_SDC 21 // pin 28 on atmega328 | SCL GIOP22 on ESP32

class RTC
{

public:
    static RTC *instance;
    static RTC *getInstance()
    {
        if (!instance)
            instance = new RTC;
        return instance;
    }

    time_t getRTCTime()
    {
        if (this->_isRTCAvailable)
        {
            return this->rtc.now().unixtime();
        }
        else
            return now();
    }

    void setRTCTime(const time_t mTime)
    {
        if (this->_isRTCAvailable)
        {
            Serial.print("Adjusting RTC time to: ");
            Serial.println(mTime);
            this->rtc.adjust(DateTime(mTime));
        }
        else
        {
            Serial.print("Can't adjust RTC time, as RTC is not present, adjusting local time to: ");
            Serial.println(mTime);
        }

        timeval tv;
        tv.tv_sec = time_t(mTime);

        settimeofday(&tv, &this->_tz_utc);
    }

    boolean isRTCAvailable()
    {
        return this->_isRTCAvailable;
    }

private:
    boolean _isRTCAvailable = false;
    timezone _tz_utc = {0, 0};

    RTC()
    {
        Serial.println("RTC singleton constructor.");
        this->I2CWire.begin(I2C_SDA, I2C_SDC, 100000ul);

        if (!this->rtc.begin(&I2CWire))
        {
            Serial.println("Error couldn't find RTC!");
            this->_isRTCAvailable = false;
            return;
        }
        else {
            this->_isRTCAvailable = true;
        }

        if (!this->rtc.isrunning())
        {
            Serial.println("RTC is NOT running, setting time to compile time.");
            this->rtc.adjust(DateTime(F(__DATE__), F(__TIME__))); // set RTC time to Compile time
        }
        Serial.print("Current RTC Time (unixtime): ");
        Serial.println(this->rtc.now().unixtime());
    }

    RTC_DS1307 rtc;
    TwoWire I2CWire = TwoWire(0);

    byte decToBcd(byte val)
    {
        // Convert normal decimal numbers to binary coded decimal
        return ((val / 10 * 16) + (val % 10));
    }

    byte bcdToDec(byte val)
    {
        // Convert binary coded decimal to normal decimal numbers
        return ((val / 16 * 10) + (val % 16));
    }
};
// Initialize pointer to zero so that it can be initialized in first call to getInstance
RTC *RTC::instance = 0;
#endif