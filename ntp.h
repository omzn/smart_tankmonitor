#ifndef NTP_H
#define NTP_H

#include <WiFiUdp.h>
#include "Arduino.h"

#define UDP_LOCAL_PORT      (2390)
#define NTP_PACKET_SIZE       (48)
#define SECONDS_UTC_TO_JST (32400)

class NTP {
  public:
    NTP(String address);
    uint32_t getTime();
    void begin();
    
  protected:
    WiFiUDP udp;
    String timeserver;
    
    byte packetBuffer[NTP_PACKET_SIZE];

    void sendPacket();
    uint32_t readPacket();
};

#endif
