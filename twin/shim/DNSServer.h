// FlickBook digital twin - captive-portal DNS stub (no DNS is served).
#pragma once
#include "Arduino.h"
#include "IPAddress.h"

class DNSServer
{
public:
    bool start(uint16_t port, const String &domainName, const IPAddress &resolvedIP)
    {
        running_ = true;
        return true;
    }
    void stop() { running_ = false; }
    void processNextRequest() {}
    void setErrorReplyCode(int) {}
    void setTTL(uint32_t) {}

private:
    bool running_ = false;
};
