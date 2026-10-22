// FlickBook digital twin - tiny single-threaded HTTP/1.1 server helpers.
// Used both by the firmware's WebServer shim and by the twin's agent API /
// browser viewer. Connections are handled one at a time (Connection: close),
// polled from the main loop, so everything stays deterministic.
#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <utility>
#include <vector>

namespace twin
{
namespace http
{

struct Request
{
    std::string method;
    std::string path;  // without query
    std::string query; // raw query string
    std::map<std::string, std::string> headers; // lower-case keys
    std::string body;
    std::vector<std::pair<std::string, std::string>> args; // query + urlencoded form
};

struct Part
{
    std::string name;
    std::string filename;
    std::string contentType;
    std::string data;
};

class Listener
{
public:
    ~Listener() { close(); }
    bool listen(const std::string &bind, int port);
    void close();
    bool isOpen() const { return fd_ >= 0; }
    int port() const { return port_; }
    // Accept a pending connection (non-blocking). Returns client fd or -1.
    int accept();

private:
    int fd_ = -1;
    int port_ = 0;
};

// Read a full request from a client socket (blocking with timeout).
bool readRequest(int fd, Request &req, size_t maxBody = 256u * 1024u * 1024u);
bool writeAll(int fd, const void *data, size_t len);
bool writeAll(int fd, const std::string &s);
void sendResponse(int fd, int code, const std::string &contentType, const std::string &body,
                  const std::vector<std::pair<std::string, std::string>> &extraHeaders = {});
void closeClient(int fd);

std::string urlDecode(const std::string &s, bool plusAsSpace = true);
void parseUrlEncoded(const std::string &s, std::vector<std::pair<std::string, std::string>> &out);
std::vector<Part> parseMultipart(const std::string &body, const std::string &contentType);
const char *statusText(int code);

} // namespace http
} // namespace twin
