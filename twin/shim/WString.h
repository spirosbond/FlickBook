// FlickBook digital twin - host implementation of the Arduino String class.
// Semantics follow the Arduino/ESP32 core (cores/esp32/WString.h) closely
// enough for the firmware, but storage is a std::string.
#pragma once

#include <cstdint>
#include <cstddef>
#include <string>

class __FlashStringHelper;
#define FPSTR(p) (reinterpret_cast<const __FlashStringHelper *>(p))
#define F(s) (reinterpret_cast<const __FlashStringHelper *>(s))

class String
{
public:
    String() = default;
    String(const char *cstr) : s_(cstr ? cstr : "") {}
    String(const char *cstr, unsigned int length) : s_(cstr ? std::string(cstr, length) : std::string()) {}
    String(const uint8_t *cstr, unsigned int length) : String(reinterpret_cast<const char *>(cstr), length) {}
    String(const std::string &s) : s_(s) {}
    String(const __FlashStringHelper *f) : String(reinterpret_cast<const char *>(f)) {}
    String(const String &) = default;
    String(String &&) noexcept = default;
    explicit String(char c) : s_(1, c) {}
    explicit String(unsigned char v, unsigned char base = 10);
    explicit String(int v, unsigned char base = 10);
    explicit String(unsigned int v, unsigned char base = 10);
    explicit String(long v, unsigned char base = 10);
    explicit String(unsigned long v, unsigned char base = 10);
    explicit String(long long v, unsigned char base = 10);
    explicit String(unsigned long long v, unsigned char base = 10);
    explicit String(float v, unsigned int decimalPlaces = 2);
    explicit String(double v, unsigned int decimalPlaces = 2);

    String &operator=(const String &) = default;
    String &operator=(String &&) noexcept = default;
    String &operator=(const char *cstr)
    {
        s_ = cstr ? cstr : "";
        return *this;
    }
    String &operator=(const __FlashStringHelper *f) { return *this = reinterpret_cast<const char *>(f); }

    bool reserve(unsigned int size)
    {
        s_.reserve(size);
        return true;
    }
    unsigned int length() const { return (unsigned int)s_.size(); }
    bool isEmpty() const { return s_.empty(); }
    void clear() { s_.clear(); }

    bool concat(const String &str)
    {
        s_ += str.s_;
        return true;
    }
    bool concat(const char *cstr)
    {
        if (!cstr)
            return false;
        s_ += cstr;
        return true;
    }
    bool concat(const char *cstr, unsigned int length)
    {
        if (!cstr)
            return false;
        s_.append(cstr, length);
        return true;
    }
    bool concat(const uint8_t *cstr, unsigned int length) { return concat(reinterpret_cast<const char *>(cstr), length); }
    bool concat(char c)
    {
        s_.push_back(c);
        return true;
    }
    bool concat(unsigned char v) { return concat(String(v)); }
    bool concat(int v) { return concat(String(v)); }
    bool concat(unsigned int v) { return concat(String(v)); }
    bool concat(long v) { return concat(String(v)); }
    bool concat(unsigned long v) { return concat(String(v)); }
    bool concat(long long v) { return concat(String(v)); }
    bool concat(unsigned long long v) { return concat(String(v)); }
    bool concat(float v) { return concat(String(v)); }
    bool concat(double v) { return concat(String(v)); }
    bool concat(const __FlashStringHelper *f) { return concat(reinterpret_cast<const char *>(f)); }

    template <typename T>
    String &operator+=(const T &rhs)
    {
        concat(rhs);
        return *this;
    }
    String &operator+=(const char *rhs)
    {
        concat(rhs);
        return *this;
    }

    int compareTo(const String &s) const { return s_.compare(s.s_); }
    bool equals(const String &s) const { return s_ == s.s_; }
    bool equals(const char *cstr) const { return s_ == (cstr ? cstr : ""); }
    bool equalsIgnoreCase(const String &s) const;
    bool equalsConstantTime(const String &s) const { return equals(s); }
    bool operator==(const String &rhs) const { return equals(rhs); }
    bool operator==(const char *cstr) const { return equals(cstr); }
    bool operator!=(const String &rhs) const { return !equals(rhs); }
    bool operator!=(const char *cstr) const { return !equals(cstr); }
    bool operator<(const String &rhs) const { return compareTo(rhs) < 0; }
    bool operator>(const String &rhs) const { return compareTo(rhs) > 0; }
    bool operator<=(const String &rhs) const { return compareTo(rhs) <= 0; }
    bool operator>=(const String &rhs) const { return compareTo(rhs) >= 0; }

    bool startsWith(const String &prefix) const { return startsWith(prefix, 0); }
    bool startsWith(const String &prefix, unsigned int offset) const;
    bool endsWith(const String &suffix) const;

    char charAt(unsigned int index) const { return index < s_.size() ? s_[index] : 0; }
    void setCharAt(unsigned int index, char c)
    {
        if (index < s_.size())
            s_[index] = c;
    }
    char operator[](unsigned int index) const { return charAt(index); }
    char &operator[](unsigned int index);
    void getBytes(unsigned char *buf, unsigned int bufsize, unsigned int index = 0) const;
    void toCharArray(char *buf, unsigned int bufsize, unsigned int index = 0) const
    {
        getBytes(reinterpret_cast<unsigned char *>(buf), bufsize, index);
    }
    const char *c_str() const { return s_.c_str(); }
    char *begin() { return &s_[0]; }
    char *end() { return &s_[0] + s_.size(); }
    const char *begin() const { return c_str(); }
    const char *end() const { return c_str() + s_.size(); }

    int indexOf(char ch) const { return indexOf(ch, 0); }
    int indexOf(char ch, unsigned int fromIndex) const;
    int indexOf(const String &str) const { return indexOf(str, 0); }
    int indexOf(const String &str, unsigned int fromIndex) const;
    int indexOf(const char *str) const { return indexOf(String(str), 0); }
    int indexOf(const char *str, unsigned int fromIndex) const { return indexOf(String(str), fromIndex); }
    int lastIndexOf(char ch) const;
    int lastIndexOf(char ch, unsigned int fromIndex) const;
    int lastIndexOf(const String &str) const;
    int lastIndexOf(const String &str, unsigned int fromIndex) const;
    int lastIndexOf(const char *str) const { return lastIndexOf(String(str)); }
    String substring(unsigned int beginIndex) const { return substring(beginIndex, length()); }
    String substring(unsigned int beginIndex, unsigned int endIndex) const;

    void replace(char find, char replace);
    void replace(const String &find, const String &replace);
    void remove(unsigned int index);
    void remove(unsigned int index, unsigned int count);
    void toLowerCase();
    void toUpperCase();
    void trim();

    long toInt() const;
    float toFloat() const;
    double toDouble() const;

    const std::string &str() const { return s_; }
    explicit operator std::string() const { return s_; }
    // ESP32's String converts to true unless its buffer is invalid (allocation
    // failure), i.e. even an empty String is "true" in `if (str)`.
    explicit operator bool() const { return true; }

private:
    std::string s_;
};

// Concatenation (Arduino uses StringSumHelper; plain values work the same for the firmware).
inline String operator+(const String &a, const String &b)
{
    String r(a);
    r.concat(b);
    return r;
}
inline String operator+(const String &a, const char *b)
{
    String r(a);
    r.concat(b);
    return r;
}
inline String operator+(const char *a, const String &b)
{
    String r(a);
    r.concat(b);
    return r;
}
inline String operator+(const String &a, char b)
{
    String r(a);
    r.concat(b);
    return r;
}
inline String operator+(char a, const String &b)
{
    String r(a);
    r.concat(b);
    return r;
}
#define TWIN_STRING_NUM_PLUS(T)                \
    inline String operator+(const String &a, T b) \
    {                                          \
        String r(a);                           \
        r.concat(String(b));                   \
        return r;                              \
    }
TWIN_STRING_NUM_PLUS(unsigned char)
TWIN_STRING_NUM_PLUS(int)
TWIN_STRING_NUM_PLUS(unsigned int)
TWIN_STRING_NUM_PLUS(long)
TWIN_STRING_NUM_PLUS(unsigned long)
TWIN_STRING_NUM_PLUS(long long)
TWIN_STRING_NUM_PLUS(unsigned long long)
TWIN_STRING_NUM_PLUS(float)
TWIN_STRING_NUM_PLUS(double)
#undef TWIN_STRING_NUM_PLUS
inline String operator+(const String &a, const __FlashStringHelper *b) { return a + reinterpret_cast<const char *>(b); }
inline bool operator==(const char *a, const String &b) { return b == a; }
inline bool operator!=(const char *a, const String &b) { return b != a; }
