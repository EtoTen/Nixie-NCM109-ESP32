#ifndef GPS_TIME_H
#define GPS_TIME_H
#include <Arduino.h>
#include <TimeLib.h>
#include <Timezone.h>
#include "SPI_Nixie_Interface.h"
#include <SparkFun_u-blox_GNSS_Arduino_Library.h>
#include <RTC_Time.h>

// Define the time between sync events
#define SYNC_INTERVAL_HOURS 1
#define SYNC_INTERVAL_MINUTES (SYNC_INTERVAL_HOURS * 60L)
#define SYNC_INTERVAL_SECONDS (SYNC_INTERVAL_MINUTES * 60L)

// GPS
#define GPS_RX 16 // GPS TX
#define GPS_TX 17 // GPS RX
#define GPS_BAUD 115200
#define gpsPort Serial1

#define GPS_RETRIES 10
#define UBLOX_ADDRESS 0x42

TwoWire GPSi2c = TwoWire(0);
SFE_UBLOX_GNSS myGNSS;

class GPSTime
{

public:
    GPSTime(SPINixieInterface &shield) : _shield(shield)
    {
        GPSi2c.begin(I2C_SDA, I2C_SDC);
        
        if (!myGNSS.begin(GPSi2c, UBLOX_ADDRESS, 60000, false)) // Connect to the u-blox module using Wire port
                                                                // if (myGNSS.begin())
        {
            Serial.println(F("u-blox GNSS not detected at default I2C address. Please check wiring. Freezing."));
            while (1)
                ;
        }
        else
        {
            myGNSS.setI2COutput(COM_TYPE_UBX); // Set the I2C port to output UBX only (turn off NMEA noise)
        }
    }
    static GPSTime &getInstance()
    {
        return *_instance;
    }

    static void createSingleton(SPINixieInterface &shield)
    {
        GPSTime *GPS = new GPSTime(shield);
        _instance = GPS;
    }

    // Get GPS time with retries on access failure
    time_t getTime()
    {

        time_t result;
        for (int i = 0; i < GPS_RETRIES; i++)
        {
            result = _getTime();
            if (result != 0)
            {  /*
                tmElements_t tm;
                Serial.println("Got GPS Time!");
                breakTime(result, tm);
                printTimLibTime(tm);
                  rtcTime->setRTCTime(tm);
                */
                rtcTime->setRTCTime(result);
                //_shield._getRTCTime();
            }
            Serial.println("Problem getting GPS time. Retrying...");
            unsigned long start = millis();
            while (millis() - start < 600)
                ;
            ;
        }
        Serial.println("GPS Problem - Could not obtain time. Falling back to RTC.");

        return rtcTime->getRTCTime();
    }

private:
    static GPSTime *_instance;
    // Instance of shield
    SPINixieInterface &_shield;

     RTC *rtcTime = rtcTime->getInstance();

    void printTimLibTime(tmElements_t &mTime)
    {
        Serial.print("TimeLib Converted Time: ");

        Serial.print(mTime.Hour);
        Serial.print(":");
        if (mTime.Minute < 10)
            Serial.print('0');
        Serial.print(mTime.Minute);
        Serial.print(":");
        if (mTime.Second < 10)
            Serial.print('0');
        Serial.println(mTime.Second);
    }
    // https://github.com/sparkfun/SparkFun_Ublox_Arduino_Library/blob/master/examples/Example15_GetDateTime/Example15_GetDateTime.ino
    void printGPSTime()
    {
        Serial.println("GNSS Information: ");
        long latitude = myGNSS.getLatitude();
        Serial.print(F("Lat: "));
        Serial.println(latitude);

        long longitude = myGNSS.getLongitude();
        Serial.print(F("Long: "));
        Serial.print(longitude);
        Serial.println(F(" (degrees * 10^-7)"));

        long altitude = myGNSS.getAltitude();
        Serial.print(F("Alt: "));
        Serial.print(altitude);
        Serial.println(F(" (mm)"));

        byte SIV = myGNSS.getSIV();
        Serial.print(F("SIV: "));
        Serial.println(SIV);

        Serial.print(myGNSS.getYear());
        Serial.print("-");
        Serial.print(myGNSS.getMonth());
        Serial.print("-");
        Serial.print(myGNSS.getDay());
        Serial.print(" ");
        Serial.print(myGNSS.getHour());
        Serial.print(":");
        Serial.print(myGNSS.getMinute());
        Serial.print(":");
        Serial.println(myGNSS.getSecond());

        Serial.print("Time is ");
        if (myGNSS.getTimeValid() == false)
        {
            Serial.print("not ");
        }
        Serial.print("valid");
        Serial.println("");
        Serial.print("Date is ");
        if (myGNSS.getDateValid() == false)
        {
            Serial.print("not ");
        }
        Serial.print("valid");
        Serial.println("");
    }

    time_t _getTime()
    {
        unsigned long start = millis();
        Serial.println("Reading...");
        while ((millis() - start < 60000))
        {
            printGPSTime();
            if (myGNSS.getTimeValid() && myGNSS.getDateValid())
            {
                return myGNSS.getUnixEpoch();
            }
        }
        return 0;
    }
    /*
        time_t _getRTCTime()
        {
            bool isRTCAvailable = true;
            tmElements_t m;
            _shield.getRTCTime(m);

            byte prevSeconds = m.Second;
            unsigned long RTC_ReadingStartTime = millis();

            Serial.print("Current RTC time: ");
            Serial.print(m.Hour);
            Serial.print(":");
            if (m.Minute < 10)
                Serial.print('0');
            Serial.print(m.Minute);
            Serial.print(":");
            if (m.Second < 10)
                Serial.print('0');
            Serial.println(m.Second);

            while (prevSeconds == m.Second)
            {
                _shield.getRTCTime(m);
                if ((millis() - RTC_ReadingStartTime) > 3000)
                {
                    Serial.println("Warning! RTC did not respond!");
                    isRTCAvailable = false;
                    break;
                }
            }

            // Set system time if RTC is available
            if (isRTCAvailable)
            {
                Serial.println("Got time from RTC");
                return makeTime(m);
            }
            else
            {
                return 0;
            }
        }
    */
};

GPSTime *GPSTime::_instance = 0;

time_t getGPSTime()
{
    return GPSTime::getInstance().getTime();
}

// Initialize the GPS code
void initGPSTime(SPINixieInterface &shield)
{
    // Create instance of GPS class
    GPSTime::createSingleton(shield);

    // Set the time provider to GPS
    setSyncProvider(getGPSTime);

    // Set the interval between GPS calls
    setSyncInterval(SYNC_INTERVAL_SECONDS);
}

#endif