#include "http.h"

#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cctype>
#include <cerrno>
#include <cstring>

namespace twin
{
namespace http
{

bool Listener::listen(const std::string &bind, int port)
{
    close();
    int fd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0)
        return false;
    int one = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons((uint16_t)port);
    if (inet_pton(AF_INET, bind.c_str(), &addr.sin_addr) != 1)
    {
        ::close(fd);
        return false;
    }
    if (::bind(fd, (sockaddr *)&addr, sizeof(addr)) != 0 || ::listen(fd, 16) != 0)
    {
        ::close(fd);
        return false;
    }
    fcntl(fd, F_SETFL, fcntl(fd, F_GETFL) | O_NONBLOCK);
    socklen_t len = sizeof(addr);
    getsockname(fd, (sockaddr *)&addr, &len);
    port_ = ntohs(addr.sin_port);
    fd_ = fd;
    return true;
}

void Listener::close()
{
    if (fd_ >= 0)
        ::close(fd_);
    fd_ = -1;
}

int Listener::accept()
{
    if (fd_ < 0)
        return -1;
    int c = ::accept(fd_, nullptr, nullptr);
    if (c < 0)
        return -1;
    fcntl(c, F_SETFL, fcntl(c, F_GETFL) & ~O_NONBLOCK);
    timeval tv{5, 0};
    setsockopt(c, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    setsockopt(c, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
#ifdef SO_NOSIGPIPE
    int one = 1;
    setsockopt(c, SOL_SOCKET, SO_NOSIGPIPE, &one, sizeof(one));
#endif
    return c;
}

static std::string lower(std::string s)
{
    for (auto &c : s)
        c = (char)tolower((unsigned char)c);
    return s;
}

static std::string trim(const std::string &s)
{
    size_t b = s.find_first_not_of(" \t\r\n");
    if (b == std::string::npos)
        return "";
    size_t e = s.find_last_not_of(" \t\r\n");
    return s.substr(b, e - b + 1);
}

bool readRequest(int fd, Request &req, size_t maxBody)
{
    std::string buf;
    char tmp[8192];
    size_t headerEnd = std::string::npos;
    while (headerEnd == std::string::npos)
    {
        ssize_t n = ::recv(fd, tmp, sizeof(tmp), 0);
        if (n <= 0)
            return false;
        buf.append(tmp, (size_t)n);
        headerEnd = buf.find("\r\n\r\n");
        if (buf.size() > 64 * 1024 && headerEnd == std::string::npos)
            return false;
    }
    std::string head = buf.substr(0, headerEnd);
    std::string rest = buf.substr(headerEnd + 4);

    size_t lineEnd = head.find("\r\n");
    std::string requestLine = head.substr(0, lineEnd);
    size_t sp1 = requestLine.find(' ');
    size_t sp2 = requestLine.find(' ', sp1 + 1);
    if (sp1 == std::string::npos || sp2 == std::string::npos)
        return false;
    req.method = requestLine.substr(0, sp1);
    std::string target = requestLine.substr(sp1 + 1, sp2 - sp1 - 1);
    size_t q = target.find('?');
    req.path = urlDecode(target.substr(0, q), false);
    req.query = q == std::string::npos ? "" : target.substr(q + 1);

    size_t pos = lineEnd == std::string::npos ? head.size() : lineEnd + 2;
    while (pos < head.size())
    {
        size_t e = head.find("\r\n", pos);
        if (e == std::string::npos)
            e = head.size();
        std::string line = head.substr(pos, e - pos);
        size_t colon = line.find(':');
        if (colon != std::string::npos)
            req.headers[lower(trim(line.substr(0, colon)))] = trim(line.substr(colon + 1));
        pos = e + 2;
    }

    size_t contentLength = 0;
    auto it = req.headers.find("content-length");
    if (it != req.headers.end())
        contentLength = (size_t)strtoull(it->second.c_str(), nullptr, 10);
    if (contentLength > maxBody)
        return false;
    req.body = rest;
    while (req.body.size() < contentLength)
    {
        ssize_t n = ::recv(fd, tmp, std::min(sizeof(tmp), contentLength - req.body.size()), 0);
        if (n <= 0)
            return false;
        req.body.append(tmp, (size_t)n);
    }
    if (req.body.size() > contentLength)
        req.body.resize(contentLength);

    parseUrlEncoded(req.query, req.args);
    auto ct = req.headers.find("content-type");
    if (ct != req.headers.end() && lower(ct->second).find("application/x-www-form-urlencoded") == 0)
        parseUrlEncoded(req.body, req.args);
    return true;
}

bool writeAll(int fd, const void *data, size_t len)
{
    const char *p = static_cast<const char *>(data);
    while (len)
    {
#ifdef MSG_NOSIGNAL
        ssize_t n = ::send(fd, p, len, MSG_NOSIGNAL);
#else
        ssize_t n = ::send(fd, p, len, 0);
#endif
        if (n <= 0)
        {
            if (n < 0 && errno == EINTR)
                continue;
            return false;
        }
        p += n;
        len -= (size_t)n;
    }
    return true;
}

bool writeAll(int fd, const std::string &s) { return writeAll(fd, s.data(), s.size()); }

const char *statusText(int code)
{
    switch (code)
    {
    case 200: return "OK";
    case 201: return "Created";
    case 204: return "No Content";
    case 302: return "Found";
    case 400: return "Bad Request";
    case 404: return "Not Found";
    case 405: return "Method Not Allowed";
    case 409: return "Conflict";
    case 413: return "Payload Too Large";
    case 500: return "Internal Server Error";
    default: return "Status";
    }
}

void sendResponse(int fd, int code, const std::string &contentType, const std::string &body,
                  const std::vector<std::pair<std::string, std::string>> &extraHeaders)
{
    std::string h = "HTTP/1.1 " + std::to_string(code) + " " + statusText(code) + "\r\n";
    h += "Content-Type: " + contentType + "\r\n";
    h += "Content-Length: " + std::to_string(body.size()) + "\r\n";
    for (auto &kv : extraHeaders)
        h += kv.first + ": " + kv.second + "\r\n";
    h += "Connection: close\r\n\r\n";
    writeAll(fd, h);
    writeAll(fd, body);
}

void closeClient(int fd)
{
    if (fd >= 0)
    {
        ::shutdown(fd, SHUT_WR);
        ::close(fd);
    }
}

static int hexVal(char c)
{
    if (c >= '0' && c <= '9')
        return c - '0';
    if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;
    if (c >= 'A' && c <= 'F')
        return c - 'A' + 10;
    return -1;
}

std::string urlDecode(const std::string &s, bool plusAsSpace)
{
    std::string out;
    out.reserve(s.size());
    for (size_t i = 0; i < s.size(); ++i)
    {
        char c = s[i];
        if (c == '+' && plusAsSpace)
            out.push_back(' ');
        else if (c == '%' && i + 2 < s.size() && hexVal(s[i + 1]) >= 0 && hexVal(s[i + 2]) >= 0)
        {
            out.push_back((char)(hexVal(s[i + 1]) * 16 + hexVal(s[i + 2])));
            i += 2;
        }
        else
            out.push_back(c);
    }
    return out;
}

void parseUrlEncoded(const std::string &s, std::vector<std::pair<std::string, std::string>> &out)
{
    size_t pos = 0;
    while (pos < s.size())
    {
        size_t amp = s.find('&', pos);
        if (amp == std::string::npos)
            amp = s.size();
        std::string kv = s.substr(pos, amp - pos);
        if (!kv.empty())
        {
            size_t eq = kv.find('=');
            if (eq == std::string::npos)
                out.emplace_back(urlDecode(kv), "");
            else
                out.emplace_back(urlDecode(kv.substr(0, eq)), urlDecode(kv.substr(eq + 1)));
        }
        pos = amp + 1;
    }
}

static std::string headerParam(const std::string &header, const std::string &key)
{
    std::string lh = lower(header);
    size_t p = lh.find(key + "=");
    while (p != std::string::npos && p > 0 && (isalnum((unsigned char)lh[p - 1]) || lh[p - 1] == '*'))
        p = lh.find(key + "=", p + 1);
    if (p == std::string::npos)
        return "";
    p += key.size() + 1;
    if (p < header.size() && header[p] == '"')
    {
        size_t e = header.find('"', p + 1);
        return header.substr(p + 1, e == std::string::npos ? std::string::npos : e - p - 1);
    }
    size_t e = header.find(';', p);
    return trim(header.substr(p, e == std::string::npos ? std::string::npos : e - p));
}

std::vector<Part> parseMultipart(const std::string &body, const std::string &contentType)
{
    std::vector<Part> parts;
    std::string boundary = headerParam(contentType, "boundary");
    if (boundary.empty())
        return parts;
    const std::string delim = "--" + boundary;
    size_t pos = body.find(delim);
    while (pos != std::string::npos)
    {
        pos += delim.size();
        if (body.compare(pos, 2, "--") == 0)
            break;
        if (body.compare(pos, 2, "\r\n") == 0)
            pos += 2;
        size_t hdrEnd = body.find("\r\n\r\n", pos);
        if (hdrEnd == std::string::npos)
            break;
        Part part;
        std::string hdrs = body.substr(pos, hdrEnd - pos);
        size_t hp = 0;
        while (hp < hdrs.size())
        {
            size_t e = hdrs.find("\r\n", hp);
            if (e == std::string::npos)
                e = hdrs.size();
            std::string line = hdrs.substr(hp, e - hp);
            size_t colon = line.find(':');
            if (colon != std::string::npos)
            {
                std::string k = lower(trim(line.substr(0, colon)));
                std::string v = trim(line.substr(colon + 1));
                if (k == "content-disposition")
                {
                    part.name = headerParam(v, "name");
                    part.filename = headerParam(v, "filename");
                }
                else if (k == "content-type")
                    part.contentType = v;
            }
            hp = e + 2;
        }
        size_t dataStart = hdrEnd + 4;
        size_t next = body.find("\r\n" + delim, dataStart);
        if (next == std::string::npos)
            break;
        part.data = body.substr(dataStart, next - dataStart);
        parts.push_back(std::move(part));
        pos = next + 2;
    }
    return parts;
}

} // namespace http
} // namespace twin
