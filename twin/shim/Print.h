// FlickBook digital twin - Arduino Print / Stream.
#pragma once

#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "WString.h"

#define DEC 10
#define HEX 16
#define OCT 8
#define BIN 2

class Print;

class Printable
{
public:
    virtual ~Printable() = default;
    virtual size_t printTo(Print &p) const = 0;
};

class Print
{
public:
    virtual ~Print() = default;
    virtual size_t write(uint8_t) = 0;
    virtual size_t write(const uint8_t *buffer, size_t size)
    {
        size_t n = 0;
        while (size--)
        {
            if (write(*buffer++))
                n++;
            else
                break;
        }
        return n;
    }
    size_t write(const char *str) { return str ? write(reinterpret_cast<const uint8_t *>(str), strlen(str)) : 0; }
    size_t write(const char *buffer, size_t size) { return write(reinterpret_cast<const uint8_t *>(buffer), size); }
    virtual int availableForWrite() { return 0; }
    virtual void flush() {}

    size_t printf(const char *format, ...) __attribute__((format(printf, 2, 3)));

    size_t print(const __FlashStringHelper *f) { return print(reinterpret_cast<const char *>(f)); }
    size_t print(const String &s) { return write(s.c_str(), s.length()); }
    size_t print(const char *s) { return write(s); }
    size_t print(char c) { return write((uint8_t)c); }
    size_t print(unsigned char v, int base = DEC) { return print((unsigned long)v, base); }
    size_t print(int v, int base = DEC) { return print((long)v, base); }
    size_t print(unsigned int v, int base = DEC) { return print((unsigned long)v, base); }
    size_t print(long v, int base = DEC) { return print(String(v, (unsigned char)base)); }
    size_t print(unsigned long v, int base = DEC) { return print(String(v, (unsigned char)base)); }
    size_t print(long long v, int base = DEC) { return print(String(v, (unsigned char)base)); }
    size_t print(unsigned long long v, int base = DEC) { return print(String(v, (unsigned char)base)); }
    size_t print(double v, int digits = 2) { return print(String(v, (unsigned int)digits)); }

    size_t print(const Printable &x) { return x.printTo(*this); }

    size_t println() { return write("\r\n"); }
    template <typename T>
    size_t println(const T &v)
    {
        size_t n = print(v);
        return n + println();
    }
    template <typename T>
    size_t println(const T &v, int fmt)
    {
        size_t n = print(v, fmt);
        return n + println();
    }
};

class Stream : public Print
{
public:
    virtual int available() = 0;
    virtual int read() = 0;
    virtual int peek() = 0;

    void setTimeout(unsigned long timeout) { timeout_ = timeout; }
    unsigned long getTimeout() const { return timeout_; }

    virtual size_t readBytes(char *buffer, size_t length)
    {
        size_t count = 0;
        while (count < length)
        {
            int c = read();
            if (c < 0)
                break;
            *buffer++ = (char)c;
            count++;
        }
        return count;
    }
    size_t readBytes(uint8_t *buffer, size_t length) { return readBytes(reinterpret_cast<char *>(buffer), length); }
    String readString()
    {
        String r;
        int c;
        while ((c = read()) >= 0)
            r += (char)c;
        return r;
    }

protected:
    unsigned long timeout_ = 1000;
};
