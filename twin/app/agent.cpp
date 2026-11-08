#include "agent.h"

#include <ArduinoJson.h>

#include <cstdio>
#include <filesystem>
#include <string>
#include <utility>
#include <vector>

#include "Inkplate.h"
#include "panel.h"
#include "runtime.h"
#include "twin.h"

extern Inkplate display;

namespace twin
{
namespace agent
{

std::string base64(const void *data, size_t len)
{
    static const char *tbl = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    const uint8_t *p = static_cast<const uint8_t *>(data);
    std::string out;
    out.reserve((len + 2) / 3 * 4);
    for (size_t i = 0; i < len; i += 3)
    {
        uint32_t v = (uint32_t)p[i] << 16;
        if (i + 1 < len)
            v |= (uint32_t)p[i + 1] << 8;
        if (i + 2 < len)
            v |= p[i + 2];
        out.push_back(tbl[(v >> 18) & 63]);
        out.push_back(tbl[(v >> 12) & 63]);
        out.push_back(i + 1 < len ? tbl[(v >> 6) & 63] : '=');
        out.push_back(i + 2 < len ? tbl[v & 63] : '=');
    }
    return out;
}

namespace
{

panel::Image sourceImage(JsonVariantConst req)
{
    std::string src = req["source"] | "panel";
    return src == "framebuffer" ? panel::framebuffer() : panel::logical();
}

void fillIdle(JsonDocument &res, const runtime::IdleResult &r)
{
    res["idle"] = r.idle;
    res["elapsed_ms"] = r.elapsed_ms;
}

void fillState(JsonDocument &res)
{
    res["time_ms"] = nowMs();
    res["loops"] = runtime::loopCount();
    res["booted"] = runtime::booted();
    res["realtime"] = config().realtime;
    res["display_mode"] = display.getDisplayMode() ? "3bit" : "1bit";
    res["rotation"] = panel::rotation();
    auto img = panel::logical();
    res["width"] = img.w;
    res["height"] = img.h;
    res["frame"] = panel::frameCounter();
    res["panel_hash"] = panel::hex64(panel::hash(img));
    res["refresh_count"] = refreshEvents().size();
    res["serial_bytes"] = serialLog().size();
    res["warnings"] = warnings().size();
    res["touch_pending"] = touchPending();
    auto &p = peripherals();
    res["battery_v"] = p.battery_v;
    res["frontlight_on"] = p.frontlight_on;
    res["frontlight_level"] = p.frontlight_level;
    res["wifi_ap"] = p.wifi_ap;
    res["wifi_ssid"] = p.wifi_ssid;
    res["web_url"] = "http://" + config().web_bind + ":" + std::to_string(config().web_port) + "/";
    res["sd_root"] = config().sd_root;
}

bool maybeWait(JsonVariantConst req, JsonDocument &res)
{
    if (!(req["wait"] | true))
        return true;
    auto r = runtime::runUntilIdle(req["quiet_ms"] | 400, req["max_ms"] | 60000);
    fillIdle(res, r);
    return true;
}

// JSON string literal for values appended after serialization.
std::string jsonString(const std::string &s)
{
    std::string out = "\"";
    out.reserve(s.size() + 2);
    for (unsigned char c : s)
    {
        switch (c)
        {
        case '"': out += "\\\""; break;
        case '\\': out += "\\\\"; break;
        case '\n': out += "\\n"; break;
        case '\r': out += "\\r"; break;
        case '\t': out += "\\t"; break;
        default:
            if (c < 0x20)
            {
                char buf[8];
                snprintf(buf, sizeof(buf), "\\u%04x", c);
                out += buf;
            }
            else
                out += (char)c;
        }
    }
    return out + "\"";
}

std::string error(const std::string &msg)
{
    JsonDocument res;
    res["ok"] = false;
    res["error"] = msg;
    std::string out;
    serializeJson(res, out);
    return out;
}

} // namespace

std::string handle(const std::string &requestJson, Control &ctl)
{
    JsonDocument req;
    DeserializationError err = deserializeJson(req, requestJson);
    if (err)
        return error(std::string("invalid JSON: ") + err.c_str());
    std::string cmd = req["cmd"] | "";
    JsonDocument res;
    // ArduinoJson (configured as on the device) caps strings at 65535
    // characters, so potentially large values are appended after serialization.
    std::vector<std::pair<const char *, std::string>> large;
    res["ok"] = true;
    if (req["id"].is<JsonVariantConst>())
        res["id"] = req["id"];

    if (cmd == "ping")
    {
        res["pong"] = true;
    }
    else if (cmd == "get_state")
    {
        fillState(res);
    }
    else if (cmd == "tap" || cmd == "long_press")
    {
        if (!req["x"].is<int>() || !req["y"].is<int>())
            return error("tap/long_press need integer x and y");
        uint32_t hold = req["hold_ms"] | (cmd == "tap" ? 60 : 1000);
        if (cmd == "tap")
            gestureTap(req["x"], req["y"], hold);
        else
            gestureLongPress(req["x"], req["y"], hold);
        maybeWait(req, res);
    }
    else if (cmd == "swipe")
    {
        int x0, y0, x1, y1;
        auto img = panel::logical();
        std::string dir = req["direction"] | "";
        if (!dir.empty())
        {
            // Finger movement direction. The firmware maps a finger moving
            // right-to-left ("left") to its handleSwipeRight (next page).
            const int cx = img.w / 2, cy = img.h / 2, d = req["distance"] | 300;
            x0 = x1 = cx;
            y0 = y1 = cy;
            if (dir == "left")
                x0 = cx + d / 2, x1 = cx - d / 2;
            else if (dir == "right")
                x0 = cx - d / 2, x1 = cx + d / 2;
            else if (dir == "up")
                y0 = cy + d / 2, y1 = cy - d / 2;
            else if (dir == "down")
                y0 = cy - d / 2, y1 = cy + d / 2;
            else
                return error("direction must be left/right/up/down");
        }
        else
        {
            x0 = req["x0"] | -1;
            y0 = req["y0"] | -1;
            x1 = req["x1"] | -1;
            y1 = req["y1"] | -1;
            if (x0 < 0 || y0 < 0 || x1 < 0 || y1 < 0)
                return error("swipe needs direction or x0,y0,x1,y1");
        }
        gestureSwipe(x0, y0, x1, y1, req["duration_ms"] | 150);
        maybeWait(req, res);
    }
    else if (cmd == "touch")
    {
        // Raw samples: [{"dt": ms_from_now, "fingers": n, "x": .., "y": ..}, ...]
        uint64_t base = nowMs();
        for (JsonVariantConst s : req["samples"].as<JsonArrayConst>())
        {
            TouchSample t;
            t.t = base + (uint64_t)(s["dt"] | 0);
            t.fingers = s["fingers"] | 1;
            t.x[0] = s["x"] | 0;
            t.y[0] = s["y"] | 0;
            t.x[1] = s["x2"] | 0;
            t.y[1] = s["y2"] | 0;
            touchEnqueue(t);
        }
        maybeWait(req, res);
    }
    else if (cmd == "advance_time")
    {
        uint64_t ms = req["ms"] | 0;
        runtime::runFor(ms);
        res["time_ms"] = nowMs();
    }
    else if (cmd == "run_until_idle")
    {
        fillIdle(res, runtime::runUntilIdle(req["quiet_ms"] | 400, req["max_ms"] | 60000));
        res["time_ms"] = nowMs();
    }
    else if (cmd == "screenshot")
    {
        panel::Image img = sourceImage(req);
        bool fl = req["frontlight"] | false;
        std::vector<uint8_t> png = fl ? panel::encodePngRgb(img.w, img.h, panel::logicalRgb(img, true))
                                      : panel::encodePng(img);
        if (png.empty())
            return error("PNG encoding failed");
        std::string path = req["path"] | "";
        if (!path.empty())
        {
            FILE *f = fopen(path.c_str(), "wb");
            if (!f)
                return error("cannot write " + path);
            fwrite(png.data(), 1, png.size(), f);
            fclose(f);
            res["path"] = std::filesystem::absolute(path).string();
        }
        else
        {
            large.emplace_back("png_base64", base64(png.data(), png.size()));
        }
        res["width"] = img.w;
        res["height"] = img.h;
        res["hash"] = panel::hex64(panel::hash(img));
    }
    else if (cmd == "get_framebuffer_hash")
    {
        panel::Image img = sourceImage(req);
        res["hash"] = panel::hex64(panel::hash(img));
        res["width"] = img.w;
        res["height"] = img.h;
        res["frame"] = panel::frameCounter();
    }
    else if (cmd == "get_pixels")
    {
        // Small region readback for precise assertions: rows of gray values.
        panel::Image img = sourceImage(req);
        int x = req["x"] | 0, y = req["y"] | 0, w = req["w"] | 1, h = req["h"] | 1;
        if (x < 0 || y < 0 || w <= 0 || h <= 0 || x + w > img.w || y + h > img.h || (int64_t)w * h > 65536)
            return error("region out of bounds or larger than 65536 pixels");
        JsonArray rows = res["rows"].to<JsonArray>();
        for (int j = 0; j < h; ++j)
        {
            JsonArray row = rows.add<JsonArray>();
            for (int i = 0; i < w; ++i)
                row.add(img.gray[(size_t)(y + j) * img.w + (x + i)]);
        }
    }
    else if (cmd == "get_serial_log")
    {
        const std::string &log = serialLog();
        size_t since = req["since"] | 0;
        if (since > log.size())
            since = log.size();
        large.emplace_back("text", log.substr(since));
        res["next"] = log.size();
    }
    else if (cmd == "get_refresh_events")
    {
        uint64_t since = req["since"] | 0;
        JsonArray arr = res["events"].to<JsonArray>();
        for (auto &e : refreshEvents())
        {
            if (e.seq < since)
                continue;
            JsonObject o = arr.add<JsonObject>();
            o["seq"] = e.seq;
            o["t"] = e.t;
            o["kind"] = e.kind;
            o["mode"] = e.mode ? "3bit" : "1bit";
            o["changed"] = e.changed;
            if (e.x1 >= e.x0)
            {
                JsonArray bb = o["bbox"].to<JsonArray>();
                bb.add(e.x0);
                bb.add(e.y0);
                bb.add(e.x1);
                bb.add(e.y1);
            }
        }
        res["next"] = refreshEvents().empty() ? since : refreshEvents().back().seq + 1;
    }
    else if (cmd == "get_warnings")
    {
        JsonArray arr = res["warnings"].to<JsonArray>();
        for (auto &w : warnings())
            arr.add(w);
    }
    else if (cmd == "set_battery")
    {
        if (!req["volts"].is<float>())
            return error("set_battery needs numeric volts");
        peripherals().battery_v = req["volts"].as<double>();
        res["battery_v"] = peripherals().battery_v;
    }
    else if (cmd == "reset")
    {
        ctl.reset = true;
    }
    else if (cmd == "load_sdcard")
    {
        std::string dir = req["dir"] | "";
        if (dir.empty() || !std::filesystem::is_directory(dir))
            return error("load_sdcard needs an existing directory 'dir'");
        ctl.reset = true;
        ctl.reset_sd_template = std::filesystem::absolute(dir).string();
    }
    else if (cmd == "quit")
    {
        ctl.quit = true;
    }
    else
    {
        return error("unknown cmd '" + cmd + "'");
    }

    if (res.overflowed())
        return error("response too large: a value exceeds the JSON document limits");
    std::string out;
    serializeJson(res, out);
    out.pop_back(); // closing brace
    for (const auto &kv : large)
        out += ",\"" + std::string(kv.first) + "\":" + jsonString(kv.second);
    out += "}";
    return out;
}

} // namespace agent
} // namespace twin
