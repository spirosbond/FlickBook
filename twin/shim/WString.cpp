#include "WString.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace
{
template <typename U>
std::string toBase(U v, unsigned char base)
{
    if (base < 2 || base > 36)
        base = 10;
    if (v == 0)
        return "0";
    std::string out;
    while (v)
    {
        unsigned d = (unsigned)(v % base);
        out.push_back((char)(d < 10 ? '0' + d : 'a' + d - 10));
        v /= base;
    }
    std::reverse(out.begin(), out.end());
    return out;
}

template <typename S>
std::string signedToBase(S v, unsigned char base)
{
    using U = typename std::make_unsigned<S>::type;
    if (base == 10 && v < 0)
        return "-" + toBase<U>((U)(-(v + 1)) + 1, base);
    return toBase<U>((U)v, base);
}

std::string fmtDouble(double v, unsigned int decimals)
{
    if (std::isnan(v))
        return "nan";
    if (std::isinf(v))
        return v < 0 ? "-inf" : "inf";
    char buf[64];
    snprintf(buf, sizeof(buf), "%.*f", (int)decimals, v);
    return buf;
}
} // namespace

String::String(unsigned char v, unsigned char base) : s_(toBase<unsigned long long>(v, base)) {}
String::String(int v, unsigned char base) : s_(signedToBase<int>(v, base)) {}
String::String(unsigned int v, unsigned char base) : s_(toBase<unsigned int>(v, base)) {}
String::String(long v, unsigned char base) : s_(signedToBase<long>(v, base)) {}
String::String(unsigned long v, unsigned char base) : s_(toBase<unsigned long>(v, base)) {}
String::String(long long v, unsigned char base) : s_(signedToBase<long long>(v, base)) {}
String::String(unsigned long long v, unsigned char base) : s_(toBase<unsigned long long>(v, base)) {}
String::String(float v, unsigned int decimalPlaces) : s_(fmtDouble(v, decimalPlaces)) {}
String::String(double v, unsigned int decimalPlaces) : s_(fmtDouble(v, decimalPlaces)) {}

bool String::equalsIgnoreCase(const String &o) const
{
    if (s_.size() != o.s_.size())
        return false;
    for (size_t i = 0; i < s_.size(); ++i)
        if (tolower((unsigned char)s_[i]) != tolower((unsigned char)o.s_[i]))
            return false;
    return true;
}

bool String::startsWith(const String &prefix, unsigned int offset) const
{
    if (offset > s_.size() || prefix.s_.size() > s_.size() - offset)
        return false;
    return s_.compare(offset, prefix.s_.size(), prefix.s_) == 0;
}

bool String::endsWith(const String &suffix) const
{
    if (suffix.s_.size() > s_.size())
        return false;
    return s_.compare(s_.size() - suffix.s_.size(), suffix.s_.size(), suffix.s_) == 0;
}

char &String::operator[](unsigned int index)
{
    static char dummy;
    if (index >= s_.size())
    {
        dummy = 0;
        return dummy;
    }
    return s_[index];
}

void String::getBytes(unsigned char *buf, unsigned int bufsize, unsigned int index) const
{
    if (!bufsize || !buf)
        return;
    if (index >= s_.size())
    {
        buf[0] = 0;
        return;
    }
    unsigned int n = bufsize - 1;
    if (n > s_.size() - index)
        n = s_.size() - index;
    memcpy(buf, s_.data() + index, n);
    buf[n] = 0;
}

int String::indexOf(char ch, unsigned int fromIndex) const
{
    if (fromIndex >= s_.size())
        return -1;
    size_t p = s_.find(ch, fromIndex);
    return p == std::string::npos ? -1 : (int)p;
}

int String::indexOf(const String &str, unsigned int fromIndex) const
{
    if (fromIndex >= s_.size())
        return -1;
    size_t p = s_.find(str.s_, fromIndex);
    return p == std::string::npos ? -1 : (int)p;
}

int String::lastIndexOf(char ch) const { return s_.empty() ? -1 : lastIndexOf(ch, (unsigned int)s_.size() - 1); }

int String::lastIndexOf(char ch, unsigned int fromIndex) const
{
    if (fromIndex >= s_.size())
        return -1;
    size_t p = s_.rfind(ch, fromIndex);
    return p == std::string::npos ? -1 : (int)p;
}

int String::lastIndexOf(const String &str) const
{
    if (str.s_.size() > s_.size())
        return -1;
    return lastIndexOf(str, (unsigned int)(s_.size() - str.s_.size()));
}

int String::lastIndexOf(const String &str, unsigned int fromIndex) const
{
    if (str.s_.empty() || s_.empty() || str.s_.size() > s_.size())
        return -1;
    if (fromIndex >= s_.size())
        fromIndex = (unsigned int)s_.size() - 1;
    size_t p = s_.rfind(str.s_, fromIndex);
    return p == std::string::npos ? -1 : (int)p;
}

String String::substring(unsigned int left, unsigned int right) const
{
    if (left > right)
        std::swap(left, right);
    if (left >= s_.size())
        return String();
    if (right > s_.size())
        right = (unsigned int)s_.size();
    return String(s_.substr(left, right - left));
}

void String::replace(char find, char repl)
{
    for (auto &c : s_)
        if (c == find)
            c = repl;
}

void String::replace(const String &find, const String &repl)
{
    if (find.s_.empty())
        return;
    std::string out;
    out.reserve(s_.size());
    size_t pos = 0;
    while (true)
    {
        size_t p = s_.find(find.s_, pos);
        if (p == std::string::npos)
        {
            out.append(s_, pos, std::string::npos);
            break;
        }
        out.append(s_, pos, p - pos);
        out += repl.s_;
        pos = p + find.s_.size();
    }
    s_.swap(out);
}

void String::remove(unsigned int index) { remove(index, (unsigned int)-1); }

void String::remove(unsigned int index, unsigned int count)
{
    if (index >= s_.size())
        return;
    if (count > s_.size() - index)
        count = (unsigned int)(s_.size() - index);
    s_.erase(index, count);
}

void String::toLowerCase()
{
    for (auto &c : s_)
        c = (char)tolower((unsigned char)c);
}

void String::toUpperCase()
{
    for (auto &c : s_)
        c = (char)toupper((unsigned char)c);
}

void String::trim()
{
    size_t b = 0;
    while (b < s_.size() && isspace((unsigned char)s_[b]))
        ++b;
    size_t e = s_.size();
    while (e > b && isspace((unsigned char)s_[e - 1]))
        --e;
    s_ = s_.substr(b, e - b);
}

long String::toInt() const { return atol(s_.c_str()); }
float String::toFloat() const { return (float)atof(s_.c_str()); }
double String::toDouble() const { return atof(s_.c_str()); }
