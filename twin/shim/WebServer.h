// FlickBook digital twin - ESP32 WebServer API served on a host port.
// The firmware's WebServer(80) listens on twin::config().web_bind:web_port
// (127.0.0.1:8080 by default). Requests are processed synchronously inside
// handleClient(), exactly like on the device.
#pragma once

#include <functional>
#include <string>
#include <utility>
#include <vector>

#include "Arduino.h"
#include "WiFi.h"

enum HTTPMethod
{
    HTTP_ANY = -1,
    HTTP_DELETE = 0,
    HTTP_GET = 1,
    HTTP_HEAD = 2,
    HTTP_POST = 3,
    HTTP_PUT = 4,
    HTTP_OPTIONS = 6,
    HTTP_PATCH = 28
};

enum HTTPUploadStatus
{
    UPLOAD_FILE_START,
    UPLOAD_FILE_WRITE,
    UPLOAD_FILE_END,
    UPLOAD_FILE_ABORTED
};

#ifndef HTTP_UPLOAD_BUFLEN
#define HTTP_UPLOAD_BUFLEN 1436
#endif
#define CONTENT_LENGTH_UNKNOWN ((size_t)-1)
#define CONTENT_LENGTH_NOT_SET ((size_t)-2)

struct HTTPUpload
{
    HTTPUploadStatus status = UPLOAD_FILE_START;
    String filename;
    String name;
    String type;
    size_t totalSize = 0;
    size_t currentSize = 0;
    uint8_t buf[HTTP_UPLOAD_BUFLEN];
};

namespace twin
{
namespace http
{
class Listener;
struct Request;
} // namespace http
} // namespace twin

class WebServer
{
public:
    typedef std::function<void(void)> THandlerFunction;

    explicit WebServer(int port = 80);
    ~WebServer();

    void begin();
    void begin(uint16_t port) { begin(); }
    void stop();
    void close() { stop(); }
    void handleClient();

    void on(const String &uri, THandlerFunction handler) { on(uri, HTTP_ANY, handler); }
    void on(const String &uri, HTTPMethod method, THandlerFunction fn) { on(uri, method, fn, nullptr); }
    void on(const String &uri, HTTPMethod method, THandlerFunction fn, THandlerFunction ufn);
    void onNotFound(THandlerFunction fn) { notFound_ = fn; }
    void onFileUpload(THandlerFunction fn) { fileUpload_ = fn; }

    String uri() const { return uri_; }
    HTTPMethod method() const { return method_; }
    WiFiClient &client() { return client_; }
    HTTPUpload &upload() { return upload_; }

    String arg(const String &name) const;
    String arg(int i) const;
    String argName(int i) const;
    int args() const { return (int)args_.size(); }
    bool hasArg(const String &name) const;
    String header(const String &name) const;
    bool hasHeader(const String &name) const;
    String hostHeader() const { return header("Host"); }

    void send(int code, const char *content_type = nullptr, const String &content = String(""));
    void send(int code, const char *content_type, const char *content) { send(code, content_type, String(content)); }
    void send(int code, const String &content_type, const String &content) { send(code, content_type.c_str(), content); }
    void send_P(int code, const char *content_type, const char *content) { send(code, content_type, content); }
    void sendHeader(const String &name, const String &value, bool first = false);
    void setContentLength(size_t contentLength) { contentLength_ = contentLength; }
    void sendContent(const String &content);
    void sendContent(const char *content, size_t size);

private:
    struct Route
    {
        String uri;
        HTTPMethod method;
        THandlerFunction fn;
        THandlerFunction ufn;
    };
    void dispatch(twin::http::Request &req);
    void finishResponse();

    int port_;
    twin::http::Listener *listener_ = nullptr;
    std::vector<Route> routes_;
    THandlerFunction notFound_;
    THandlerFunction fileUpload_;

    // per-request state
    int fd_ = -1;
    bool headersSent_ = false;
    size_t contentLength_ = CONTENT_LENGTH_NOT_SET;
    String uri_;
    HTTPMethod method_ = HTTP_GET;
    std::vector<std::pair<String, String>> args_;
    std::vector<std::pair<String, String>> reqHeaders_;
    std::vector<std::pair<String, String>> respHeaders_;
    WiFiClient client_;
    HTTPUpload upload_;
};
