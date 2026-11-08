#pragma once
#include "Stream.h"

// Serial output is captured by the twin (stdout/stderr echo, in-memory log for
// the agent API and an optional log file). Serial input is not modelled.
class HardwareSerial : public Stream
{
public:
    void begin(unsigned long baud, uint32_t config = 0, int8_t rxPin = -1, int8_t txPin = -1) { baud_ = baud; }
    void end() {}
    int available() override { return 0; }
    int read() override { return -1; }
    int peek() override { return -1; }
    size_t write(uint8_t c) override;
    size_t write(const uint8_t *buffer, size_t size) override;
    using Print::write;
    operator bool() const { return true; }
    void setDebugOutput(bool) {}

private:
    unsigned long baud_ = 0;
};

extern HardwareSerial Serial;
