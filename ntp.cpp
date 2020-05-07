#include "ntp.h"

NTP::NTP(String address) {
  timeserver = address;
}

void NTP::begin() {
    udp.begin(UDP_LOCAL_PORT);
}

// send an NTP request to the time server at the given address
void NTP::sendPacket() {

  // set all bytes in the buffer to 0
  memset(packetBuffer, 0, NTP_PACKET_SIZE);
  // Initialize values needed to form NTP request
  // (see URL above for details on the packets)
  packetBuffer[0]  = 0b11100011;   // LI, Version, Mode
  packetBuffer[1]  = 0;     // Stratum, or type of clock
  packetBuffer[2]  = 6;     // Polling Interval
  packetBuffer[3]  = 0xEC;  // Peer Clock Precision
  // 8 bytes of zero for Root Delay & Root Dispersion
  packetBuffer[12] = 49;
  packetBuffer[13] = 0x4E;
  packetBuffer[14] = 49;
  packetBuffer[15] = 52;
  // all NTP fields have been given values, now
  // you can send a packet requesting a timestamp:
  udp.beginPacket(timeserver.c_str(), 123); //NTP requests are to port 123
  udp.write(packetBuffer, NTP_PACKET_SIZE);
  udp.endPacket();
}

uint32_t NTP::readPacket() {
  //  Serial.println("Receive NTP Response");
  udp.read(packetBuffer, NTP_PACKET_SIZE);  // read packet into the buffer
  unsigned long secsSince1900 = 0;
  // convert four bytes starting at location 40 to a long integer
  secsSince1900 |= (unsigned long)packetBuffer[40] << 24;
  secsSince1900 |= (unsigned long)packetBuffer[41] << 16;
  secsSince1900 |= (unsigned long)packetBuffer[42] <<  8;
  secsSince1900 |= (unsigned long)packetBuffer[43] <<  0;
  return secsSince1900 - 2208988800UL; // seconds since 1970
}

uint32_t NTP::getTime() {
  while (udp.parsePacket() > 0) ; // discard any previously received packets
#ifdef DEBUG
  Serial.println("Transmit NTP Request");
#endif
  sendPacket();

  uint32_t beginWait = millis();
  while (millis() - beginWait < 1500) {
    int size = udp.parsePacket();
    if (size >= NTP_PACKET_SIZE) {
      return readPacket();
    }
  }

  return 0; // return 0 if unable to get the time
}

