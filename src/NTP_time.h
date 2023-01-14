#ifndef NTP_TIME_H
#define NTP_TIME_H

// Define the time between sync events
#define SYNC_INTERVAL_HOURS 1
#define SYNC_INTERVAL_MINUTES (SYNC_INTERVAL_HOURS * 60L)
#define SYNC_INTERVAL_SECONDS (SYNC_INTERVAL_MINUTES * 60L)

#define NTP_SERVER_NAME "us.pool.ntp.org" // NTP request server
#define NTP_SERVER_PORT 123               // NTP requests are to port 123
#define LOCALPORT 2390                    // Local port to listen for UDP packets
#define NTP_PACKET_SIZE 48                // NTP time stamp is in the first 48 bytes of the message
#define NTP_RETRIES 12                    // Times to try getting NTP time before failing

#include <TimeLib.h>
#include <WiFiUdp.h>
#include "SPI_Nixie_Interface.h"
#include "NTP_time.h"
#include "RTC_Time.h"

class NTP
{
public:
  NTP(SPINixieInterface &shield) : _shield(shield)
  {

    // Login succeeded so set UDP local port
    udp.begin(LOCALPORT);
  };

  static NTP &getInstance()
  {
    return *_instance;
  }

  static void createSingleton(SPINixieInterface &shield)
  {
    NTP *ntp = new NTP(shield);
    _instance = ntp;
  }

  // Get NTP time with retries on access failure
  time_t getTime()
  {
    // no sense in looping through retries if we are disconnected from wifi
    if (WiFi.status() == WL_CONNECTED)
    {
      time_t result;
      for (int i = 0; i < NTP_RETRIES; i++)
      {
        result = this->_getTime();
        if (result != 0)
        {

          rtcTime->setRTCTime(result);
          return result;
        }
        else
        {
          Serial.println("Problem getting NTP time. Retrying...");
          unsigned long start = millis();
          while (millis() - start < 1000); // 1 second
        }
      }
    }
    Serial.println("NTP Problem - Could not obtain time. Falling back to RTC.");

    return rtcTime->getRTCTime();
  }

private:
  // RTC rtcTime;
  RTC *rtcTime = rtcTime->getInstance();
  // Static NTP instance
  static NTP *_instance;
  // Instance of shield
  SPINixieInterface &_shield;
  // A UDP instance to let us send and receive packets over UDP
  WiFiUDP udp;

  // Buffer to hold outgoing and incoming packets
  byte packetBuffer[NTP_PACKET_SIZE];
  // NTP Time Provider Code
  time_t _getTime()
  {
    // Set all bytes in the buffer to 0
    memset(packetBuffer, 0, NTP_PACKET_SIZE);

    // Initialize values needed to form NTP request
    packetBuffer[0] = 0xE3; // LI, Version, Mode
    packetBuffer[2] = 0x06; // Polling Interval
    packetBuffer[3] = 0xEC; // Peer Clock Precision
    packetBuffer[12] = 0x31;
    packetBuffer[13] = 0x4E;
    packetBuffer[14] = 0x31;
    packetBuffer[15] = 0x34;

    // All NTP fields initialized, now send a packet requesting a timestamp
    udp.setTimeout(3000); // 3 seconds
    udp.beginPacket(NTP_SERVER_NAME, NTP_SERVER_PORT);
    udp.write(packetBuffer, NTP_PACKET_SIZE);
    udp.endPacket();

    // Wait for the response
    unsigned long lastChange = micros();
    while (micros() - lastChange < (2000000)) // 2 seconds max wait
    {
      // Listen for the response
      if (udp.parsePacket() == NTP_PACKET_SIZE)
      {
        // get seconds it took to respond so we can account for this
        float secondsToRespond = (micros() - lastChange) / 1000000;
        udp.read(packetBuffer, NTP_PACKET_SIZE); // Read packet into the buffer
        unsigned long secsSince1900;

        // Convert four bytes starting at location 40 to a long integer
        secsSince1900 = (unsigned long)packetBuffer[40] << 24;
        secsSince1900 |= (unsigned long)packetBuffer[41] << 16;
        secsSince1900 |= (unsigned long)packetBuffer[42] << 8;
        secsSince1900 |= (unsigned long)packetBuffer[43];

        time_t mUNIXTime = secsSince1900 - 2208988800UL; // convert to unix time

        Serial.print("Got NTP time: ");
        Serial.println(mUNIXTime);
        Serial.print("After (seconds): ");
        Serial.println(secondsToRespond);

        return (mUNIXTime); // returns time_t
      }
    }
    return 0;
  }
};

NTP *NTP::_instance = 0;

time_t getNTPTime()
{
  return NTP::getInstance().getTime();
}

// Initialize the NTP code
void initNTP(SPINixieInterface &shield)
{
  // Create instance of NTP class
  NTP::createSingleton(shield);

  // Set the time provider to NTP
  setSyncProvider(getNTPTime);

  // Set the interval between NTP calls
  setSyncInterval(SYNC_INTERVAL_SECONDS);
}

#endif