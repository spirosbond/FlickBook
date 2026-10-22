// FlickBook digital twin - HTTP transport for the agent API + browser viewer.
#include <fstream>
#include <sstream>

#include "agent.h"
#include "http.h"
#include "panel.h"
#include "twin.h"

using namespace twin;

namespace
{
std::string loadViewer()
{
    std::ifstream in(TWIN_VIEWER_HTML);
    if (!in)
        return "<html><body><img src='/screen.png'></body></html>";
    std::stringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

void serve(int fd, agent::Control &ctl)
{
    http::Request req;
    if (!http::readRequest(fd, req, 16u * 1024u * 1024u))
    {
        http::sendResponse(fd, 400, "text/plain", "bad request");
        return;
    }
    const std::vector<std::pair<std::string, std::string>> noCache = {{"Cache-Control", "no-store"}};
    auto arg = [&](const std::string &k) {
        for (auto &kv : req.args)
            if (kv.first == k)
                return kv.second;
        return std::string();
    };
    if (req.method == "GET" && (req.path == "/" || req.path == "/index.html"))
    {
        http::sendResponse(fd, 200, "text/html; charset=utf-8", loadViewer(), noCache);
    }
    else if (req.method == "GET" && req.path == "/screen.png")
    {
        panel::Image img = arg("source") == "framebuffer" ? panel::framebuffer() : panel::logical();
        std::vector<uint8_t> png = arg("frontlight") == "1"
                                       ? panel::encodePngRgb(img.w, img.h, panel::logicalRgb(img, true))
                                       : panel::encodePng(img);
        http::sendResponse(fd, 200, "image/png", std::string(png.begin(), png.end()), noCache);
    }
    else if (req.method == "GET" && req.path == "/api/state")
    {
        http::sendResponse(fd, 200, "application/json", agent::handle("{\"cmd\":\"get_state\"}", ctl), noCache);
    }
    else if (req.method == "POST" && req.path == "/api/cmd")
    {
        http::sendResponse(fd, 200, "application/json", agent::handle(req.body, ctl), noCache);
    }
    else
    {
        http::sendResponse(fd, 404, "text/plain", "not found");
    }
}
} // namespace

void pollHttpAgent(http::Listener &l, agent::Control &ctl)
{
    for (int i = 0; i < 8 && !ctl.reset && !ctl.quit; ++i)
    {
        int fd = l.accept();
        if (fd < 0)
            return;
        serve(fd, ctl);
        http::closeClient(fd);
    }
}
