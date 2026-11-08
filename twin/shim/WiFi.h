// FlickBook digital twin - WiFi stub. There is no radio: softAP() just records
// state and reports the ESP32's default soft-AP address. The firmware's
// WebServer is served on the host instead (see WebServer.h).
#pragma once
#include "Arduino.h"
#include "IPAddress.h"

typedef enum
{
    WIFI_OFF = 0,
    WIFI_STA = 1,
    WIFI_AP = 2,
    WIFI_AP_STA = 3
} wifi_mode_t;

typedef enum
{
    WL_IDLE_STATUS = 0,
    WL_NO_SSID_AVAIL = 1,
    WL_CONNECTED = 3,
    WL_CONNECT_FAILED = 4,
    WL_DISCONNECTED = 6
} wl_status_t;

class WiFiClient : public Stream
{
public:
    WiFiClient() = default;
    explicit WiFiClient(int fd) : fd_(fd) {}
    size_t write(uint8_t b) override { return write(&b, 1); }
    size_t write(const uint8_t *buf, size_t size) override;
    using Print::write;
    int available() override { return 0; }
    int read() override { return -1; }
    int peek() override { return -1; }
    bool connected() const { return fd_ >= 0; }
    void stop() {}
    int fd() const { return fd_; }
    explicit operator bool() const { return fd_ >= 0; }

private:
    int fd_ = -1;
};

class WiFiClass
{
public:
    bool softAP(const char *ssid, const char *passphrase = nullptr, int channel = 1, int ssid_hidden = 0, int max_connection = 4);
    bool softAPdisconnect(bool wifioff = false);
    IPAddress softAPIP() { return IPAddress(192, 168, 4, 1); }
    bool mode(wifi_mode_t m)
    {
        mode_ = m;
        return true;
    }
    wifi_mode_t getMode() const { return mode_; }
    wl_status_t begin(const char *ssid, const char *passphrase = nullptr) { return WL_CONNECT_FAILED; }
    wl_status_t status() const { return WL_DISCONNECTED; }
    bool disconnect(bool wifioff = false) { return true; }
    IPAddress localIP() { return IPAddress(0, 0, 0, 0); }
    uint8_t softAPgetStationNum() { return 0; }

private:
    wifi_mode_t mode_ = WIFI_OFF;
};

extern WiFiClass WiFi;
