// FlickBook digital twin - agent command interface.
//
// Commands are JSON objects {"cmd": "...", ...}; responses are JSON objects
// with "ok": true/false. The same dispatcher serves the stdio transport
// (one JSON object per line) and the HTTP transport (POST /api/cmd).
#pragma once

#include <string>

namespace twin
{
namespace agent
{

struct Control
{
    bool quit = false;
    bool reset = false;          // re-exec the twin with a fresh SD card
    std::string reset_sd_template; // non-empty: use this directory as the new SD template
};

std::string handle(const std::string &requestJson, Control &ctl);

// Base64 helper (used for inline PNGs).
std::string base64(const void *data, size_t len);

} // namespace agent
} // namespace twin
