#include "WebServer.h"

#include <cstring>

#include "http.h"
#include "twin.h"

using namespace twin;

size_t WiFiClient::write(const uint8_t *buf, size_t size)
{
    if (fd_ < 0)
        return 0;
    return http::writeAll(fd_, buf, size) ? size : 0;
}

static HTTPMethod methodFromString(const std::string &m)
{
    if (m == "GET")
        return HTTP_GET;
    if (m == "POST")
        return HTTP_POST;
    if (m == "DELETE")
        return HTTP_DELETE;
    if (m == "PUT")
        return HTTP_PUT;
    if (m == "PATCH")
        return HTTP_PATCH;
    if (m == "HEAD")
        return HTTP_HEAD;
    if (m == "OPTIONS")
        return HTTP_OPTIONS;
    return HTTP_ANY;
}

WebServer::WebServer(int port) : port_(port) {}

WebServer::~WebServer() { stop(); }

void WebServer::begin()
{
    if (!listener_)
        listener_ = new http::Listener();
    if (!listener_->isOpen())
    {
        if (listener_->listen(config().web_bind, config().web_port))
            serialLogMark("WebServer(port %d) listening on http://%s:%d/", port_, config().web_bind.c_str(),
                          listener_->port());
        else
            serialLogMark("WebServer(port %d): could not listen on %s:%d", port_, config().web_bind.c_str(),
                          config().web_port);
    }
}

void WebServer::stop()
{
    if (listener_)
    {
        listener_->close();
        delete listener_;
        listener_ = nullptr;
    }
}

void WebServer::on(const String &uri, HTTPMethod method, THandlerFunction fn, THandlerFunction ufn)
{
    routes_.push_back({uri, method, fn, ufn});
}

String WebServer::arg(const String &name) const
{
    for (auto &kv : args_)
        if (kv.first == name)
            return kv.second;
    if (name == "plain")
        return String();
    return String();
}

String WebServer::arg(int i) const { return i >= 0 && i < (int)args_.size() ? args_[i].second : String(); }
String WebServer::argName(int i) const { return i >= 0 && i < (int)args_.size() ? args_[i].first : String(); }

bool WebServer::hasArg(const String &name) const
{
    for (auto &kv : args_)
        if (kv.first == name)
            return true;
    return false;
}

String WebServer::header(const String &name) const
{
    for (auto &kv : reqHeaders_)
        if (kv.first.equalsIgnoreCase(name))
            return kv.second;
    return String();
}

bool WebServer::hasHeader(const String &name) const
{
    for (auto &kv : reqHeaders_)
        if (kv.first.equalsIgnoreCase(name))
            return true;
    return false;
}

void WebServer::sendHeader(const String &name, const String &value, bool first)
{
    if (first)
        respHeaders_.insert(respHeaders_.begin(), {name, value});
    else
        respHeaders_.push_back({name, value});
}

void WebServer::send(int code, const char *content_type, const String &content)
{
    if (fd_ < 0 || headersSent_)
        return;
    std::string h = "HTTP/1.1 " + std::to_string(code) + " " + http::statusText(code) + "\r\n";
    h += std::string("Content-Type: ") + (content_type ? content_type : "text/html") + "\r\n";
    size_t len = contentLength_ == CONTENT_LENGTH_NOT_SET ? content.length() : contentLength_;
    if (len != CONTENT_LENGTH_UNKNOWN)
        h += "Content-Length: " + std::to_string(len) + "\r\n";
    for (auto &kv : respHeaders_)
        h += std::string(kv.first.c_str()) + ": " + kv.second.c_str() + "\r\n";
    h += "Connection: close\r\n\r\n";
    http::writeAll(fd_, h);
    headersSent_ = true;
    if (content.length())
        http::writeAll(fd_, content.c_str(), content.length());
}

void WebServer::sendContent(const String &content) { sendContent(content.c_str(), content.length()); }

void WebServer::sendContent(const char *content, size_t size)
{
    if (fd_ >= 0 && size)
        http::writeAll(fd_, content, size);
}

void WebServer::dispatch(http::Request &req)
{
    uri_ = String(req.path);
    method_ = methodFromString(req.method);
    args_.clear();
    reqHeaders_.clear();
    respHeaders_.clear();
    headersSent_ = false;
    contentLength_ = CONTENT_LENGTH_NOT_SET;
    for (auto &kv : req.args)
        args_.push_back({String(kv.first), String(kv.second)});
    for (auto &kv : req.headers)
        reqHeaders_.push_back({String(kv.first), String(kv.second)});

    const Route *route = nullptr;
    for (auto &r : routes_)
    {
        if (r.uri == uri_ && (r.method == HTTP_ANY || r.method == method_))
        {
            route = &r;
            break;
        }
    }

    auto ct = req.headers.find("content-type");
    bool multipart = ct != req.headers.end() && ct->second.find("multipart/form-data") != std::string::npos;
    if (route && multipart)
    {
        THandlerFunction ufn = route->ufn ? route->ufn : fileUpload_;
        for (auto &part : http::parseMultipart(req.body, ct->second))
        {
            if (part.filename.empty())
            {
                args_.push_back({String(part.name), String(part.data)});
                continue;
            }
            if (!ufn)
                continue;
            upload_.filename = String(part.filename);
            upload_.name = String(part.name);
            upload_.type = String(part.contentType);
            upload_.totalSize = 0;
            upload_.currentSize = 0;
            upload_.status = UPLOAD_FILE_START;
            ufn();
            size_t off = 0;
            while (off < part.data.size())
            {
                size_t n = std::min((size_t)HTTP_UPLOAD_BUFLEN, part.data.size() - off);
                memcpy(upload_.buf, part.data.data() + off, n);
                upload_.currentSize = n;
                upload_.status = UPLOAD_FILE_WRITE;
                ufn();
                upload_.totalSize += n;
                off += n;
            }
            upload_.currentSize = 0;
            upload_.status = UPLOAD_FILE_END;
            ufn();
        }
    }
    else if (!multipart && !req.body.empty() && !hasArg("plain"))
    {
        args_.push_back({String("plain"), String(req.body)});
    }

    if (route)
        route->fn();
    else if (notFound_)
        notFound_();
    else
        send(404, "text/plain", String("Not found: ") + uri_);
}

void WebServer::handleClient()
{
    if (!listener_ || !listener_->isOpen())
        return;
    int fd = listener_->accept();
    if (fd < 0)
        return;
    http::Request req;
    if (http::readRequest(fd, req))
    {
        fd_ = fd;
        client_ = WiFiClient(fd);
        dispatch(req);
        if (!headersSent_)
            send(500, "text/plain", String("handler sent no response"));
    }
    else
    {
        http::sendResponse(fd, 400, "text/plain", "bad request");
    }
    http::closeClient(fd);
    fd_ = -1;
    client_ = WiFiClient();
    noteActivity();
}
