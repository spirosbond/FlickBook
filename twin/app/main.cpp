// FlickBook digital twin - entry point.
//
//   flicktwin [options]
//
// Modes (combinable):
//   (default)        headless: boot, run until idle, optional --screenshot/--script, exit
//   --stdio          agent API: one JSON command per stdin line, one JSON reply per stdout line
//   --http PORT      agent API + browser viewer on http://127.0.0.1:PORT/
//   --window         SDL2 window (if built with TWIN_WITH_SDL)
//
// See twin/README.md and twin/AGENTS.md.
#include <poll.h>
#include <signal.h>
#include <unistd.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "agent.h"
#include "http.h"
#include "panel.h"
#include "runtime.h"
#include "twin.h"

#ifdef TWIN_WITH_SDL
int runSdlWindow(float scale, bool &resetRequested, twin::http::Listener *http);
#endif
void pollHttpAgent(twin::http::Listener &l, twin::agent::Control &ctl);

namespace fs = std::filesystem;
using namespace twin;

namespace
{

struct Options
{
    std::string sd_dir;      // use this directory directly (persistent)
    std::string sd_template; // copy this template to a fresh temp dir
    std::string books_dir;   // extra EPUBs copied into /books
    bool fixtures = true;
    bool keep_sd = false;
    bool stdio = false;
    int http_port = -1;
    bool window = false;
    float scale = 0.75f;
    std::string screenshot;
    std::string script;
    int64_t run_ms = -1;
    bool realtime_set = false;
};

std::vector<std::string> g_args; // original argv for exec-based reset
std::string g_tempSd;            // SD work dir we created (deleted on exit)
volatile sig_atomic_t g_stop = 0;

void usage()
{
    fprintf(stderr,
            "FlickBook digital twin (GRAYSCALE=%d)\n"
            "usage: flicktwin [options]\n"
            "  --stdio                JSON-lines agent API on stdin/stdout\n"
            "  --http PORT            agent API + browser viewer on 127.0.0.1:PORT (0 = any)\n"
            "  --window               SDL2 window (interactive)\n"
            "  --scale F              window scale (default 0.75)\n"
            "  --screenshot FILE.png  headless: save the panel after booting/script\n"
            "  --script FILE          run JSON-lines commands from FILE (headless)\n"
            "  --run-ms N             headless: run N ms of virtual time after boot\n"
            "  --sd DIR               use DIR as the SD card directly (changes persist)\n"
            "  --sd-template DIR      copy DIR to a fresh temp SD card (default: firmware sdcard/)\n"
            "  --books DIR            copy *.epub from DIR into /books (default: twin fixtures)\n"
            "  --no-fixtures          do not add the twin's fixture EPUBs\n"
            "  --keep-sd              keep the temp SD card directory on exit\n"
            "  --realtime / --deterministic   clock mode (default: deterministic; --window => realtime)\n"
            "  --seed N               RNG seed (default 1)\n"
            "  --web-port N           host port for the firmware's WebServer(80) (default 8080)\n"
            "  --battery V            initial battery voltage (default 4.05)\n"
            "  --no-sd-card | --sd-read-errors | --sd-capacity BYTES   SD fault injection\n"
            "  --psram BYTES          PSRAM size cap for single allocations (default 4194304)\n"
            "  --no-touch             touchscreen.init() fails\n"
            "  --no-ghosting | --no-refresh-time   panel model switches\n"
            "  --serial-log FILE      also write serial output to FILE\n"
            "  --quiet                do not echo serial output to stderr\n",
            TWIN_GRAYSCALE);
}

bool parseArgs(int argc, char **argv, Options &o)
{
    auto &c = config();
    for (int i = 1; i < argc; ++i)
    {
        std::string a = argv[i];
        auto next = [&](const char *what) -> std::string {
            if (i + 1 >= argc)
            {
                fprintf(stderr, "missing value for %s\n", what);
                exit(2);
            }
            return argv[++i];
        };
        if (a == "--stdio")
            o.stdio = true;
        else if (a == "--http")
            o.http_port = std::stoi(next("--http"));
        else if (a == "--window")
            o.window = true;
        else if (a == "--scale")
            o.scale = std::stof(next("--scale"));
        else if (a == "--screenshot")
            o.screenshot = next("--screenshot");
        else if (a == "--script")
            o.script = next("--script");
        else if (a == "--run-ms")
            o.run_ms = std::stoll(next("--run-ms"));
        else if (a == "--sd")
            o.sd_dir = next("--sd");
        else if (a == "--sd-template")
            o.sd_template = next("--sd-template");
        else if (a == "--books")
            o.books_dir = next("--books");
        else if (a == "--no-fixtures")
            o.fixtures = false;
        else if (a == "--keep-sd")
            o.keep_sd = true;
        else if (a == "--realtime")
            c.realtime = true, o.realtime_set = true;
        else if (a == "--deterministic")
            c.realtime = false, o.realtime_set = true;
        else if (a == "--seed")
            c.seed = (uint32_t)std::stoul(next("--seed"));
        else if (a == "--web-port")
            c.web_port = std::stoi(next("--web-port"));
        else if (a == "--battery")
            peripherals().battery_v = std::stod(next("--battery"));
        else if (a == "--no-sd-card")
            c.sd_present = false;
        else if (a == "--sd-read-errors")
            c.sd_read_errors = true;
        else if (a == "--sd-capacity")
            c.sd_capacity_bytes = std::stoull(next("--sd-capacity"));
        else if (a == "--psram")
            c.psram_bytes = std::stoull(next("--psram"));
        else if (a == "--no-touch")
            c.touch_init_ok = false;
        else if (a == "--no-ghosting")
            c.ghosting = false;
        else if (a == "--no-refresh-time")
            c.simulate_refresh_time = false;
        else if (a == "--serial-log")
            c.serial_log_path = next("--serial-log");
        else if (a == "--quiet")
            c.echo_serial = false;
        else if (a == "-h" || a == "--help")
        {
            usage();
            exit(0);
        }
        else
        {
            fprintf(stderr, "unknown option %s\n", a.c_str());
            usage();
            return false;
        }
    }
    if (o.window && !o.realtime_set)
        c.realtime = true;
    if (o.stdio)
        c.echo_serial = false; // stdout belongs to the protocol; serial is in get_serial_log
    return true;
}

void copyTree(const fs::path &from, const fs::path &to)
{
    fs::create_directories(to);
    fs::copy(from, to, fs::copy_options::recursive | fs::copy_options::overwrite_existing);
}

bool prepareSd(Options &o)
{
    auto &c = config();
    if (!o.sd_dir.empty())
    {
        c.sd_root = fs::absolute(o.sd_dir).string();
        return fs::is_directory(c.sd_root);
    }
    std::string tmpl = o.sd_template;
    std::string books = o.books_dir;
    if (tmpl.empty())
    {
        tmpl = TWIN_DEFAULT_SD_TEMPLATE;
        if (books.empty() && o.fixtures)
            books = TWIN_FIXTURE_BOOKS;
    }
    char pattern[] = "/tmp/flicktwin-sd-XXXXXX";
    if (!mkdtemp(pattern))
    {
        perror("mkdtemp");
        return false;
    }
    g_tempSd = pattern;
    try
    {
        if (!fs::is_directory(tmpl))
        {
            fprintf(stderr, "SD template %s is not a directory\n", tmpl.c_str());
            return false;
        }
        copyTree(tmpl, g_tempSd);
        if (!books.empty() && fs::is_directory(books))
        {
            fs::create_directories(fs::path(g_tempSd) / "books");
            for (auto &e : fs::directory_iterator(books))
                if (e.is_regular_file() && e.path().extension() == ".epub")
                    fs::copy_file(e.path(), fs::path(g_tempSd) / "books" / e.path().filename(),
                                  fs::copy_options::overwrite_existing);
        }
    }
    catch (const std::exception &ex)
    {
        fprintf(stderr, "preparing SD card failed: %s\n", ex.what());
        return false;
    }
    c.sd_root = g_tempSd;
    return true;
}

void cleanup()
{
    if (!g_tempSd.empty())
    {
        std::error_code ec;
        fs::remove_all(g_tempSd, ec);
        g_tempSd.clear();
    }
}

[[noreturn]] void reexec(const std::string &newTemplate)
{
    std::vector<std::string> args;
    args.push_back(g_args[0]);
    for (size_t i = 1; i < g_args.size(); ++i)
    {
        if (!newTemplate.empty() && (g_args[i] == "--sd-template" || g_args[i] == "--sd" || g_args[i] == "--books"))
        {
            ++i;
            continue;
        }
        if (!newTemplate.empty() && g_args[i] == "--no-fixtures")
            continue;
        args.push_back(g_args[i]);
    }
    if (!newTemplate.empty())
    {
        args.push_back("--sd-template");
        args.push_back(newTemplate);
    }
    cleanup();
    fflush(stdout);
    fflush(stderr);
    std::vector<char *> cargv;
    for (auto &s : args)
        cargv.push_back(const_cast<char *>(s.c_str()));
    cargv.push_back(nullptr);
    execv("/proc/self/exe", cargv.data());
    execvp(cargv[0], cargv.data());
    perror("execv");
    _exit(1);
}

void onSignal(int) { g_stop = 1; }

void emitReady(FILE *out)
{
    agent::Control ignored;
    std::string state = agent::handle("{\"cmd\":\"get_state\"}", ignored);
    fprintf(out, "{\"event\":\"ready\",\"state\":%s}\n", state.c_str());
    fflush(out);
}

int runScript(const std::string &path, agent::Control &ctl)
{
    std::ifstream in(path);
    if (!in)
    {
        fprintf(stderr, "cannot open script %s\n", path.c_str());
        return 2;
    }
    std::string line;
    int failures = 0;
    while (std::getline(in, line))
    {
        size_t b = line.find_first_not_of(" \t\r");
        if (b == std::string::npos || line[b] == '#')
            continue;
        std::string res = agent::handle(line, ctl);
        if (res.size() > 400)
            fprintf(stderr, "[script] %s -> %.400s...\n", line.c_str(), res.c_str());
        else
            fprintf(stderr, "[script] %s -> %s\n", line.c_str(), res.c_str());
        if (res.find("\"ok\":false") != std::string::npos)
            ++failures;
        if (ctl.quit || ctl.reset)
            break;
    }
    return failures ? 1 : 0;
}

bool freeRunning()
{
    // When nothing commands the twin, keep the firmware running if it has to
    // serve something (web server) or when following wall-clock time.
    return config().realtime || peripherals().wifi_ap;
}

} // namespace

int main(int argc, char **argv)
{
    for (int i = 0; i < argc; ++i)
        g_args.push_back(argv[i]);
    Options o;
    if (!parseArgs(argc, argv, o))
        return 2;
    signal(SIGINT, onSignal);
    signal(SIGTERM, onSignal);
    signal(SIGPIPE, SIG_IGN);

    if (!prepareSd(o))
    {
        cleanup();
        return 2;
    }
    if (o.keep_sd)
        g_tempSd.clear(); // never delete
    randomSeed(config().seed);
    serialLogMark("FlickBook twin starting (GRAYSCALE=%d, %s clock, SD=%s)", TWIN_GRAYSCALE,
                  config().realtime ? "realtime" : "deterministic", config().sd_root.c_str());

    runtime::boot();
    agent::Control ctl;

    http::Listener httpAgent;
    if (o.http_port >= 0)
    {
        if (!httpAgent.listen("127.0.0.1", o.http_port))
        {
            fprintf(stderr, "cannot listen on 127.0.0.1:%d\n", o.http_port);
            cleanup();
            return 2;
        }
        fprintf(stderr, "[twin] viewer + agent API on http://127.0.0.1:%d/\n", httpAgent.port());
    }

    int rc = 0;
#ifdef TWIN_WITH_SDL
    if (o.window)
    {
        bool reset = false;
        rc = runSdlWindow(o.scale, reset, o.http_port >= 0 ? &httpAgent : nullptr);
        if (reset)
            reexec("");
        cleanup();
        return rc;
    }
#else
    if (o.window)
    {
        fprintf(stderr, "this build has no SDL2 window support (configure with -DTWIN_WITH_SDL=ON)\n");
        cleanup();
        return 2;
    }
#endif

    if (o.stdio || o.http_port >= 0)
    {
        if (!config().realtime)
            runtime::runUntilIdle();
        if (o.stdio)
            emitReady(stdout);
        std::string buf;
        bool stdinOpen = o.stdio;
        while (!g_stop && !ctl.quit && !ctl.reset)
        {
            pollfd pfd{STDIN_FILENO, POLLIN, 0};
            const int timeout = freeRunning() ? 0 : 10;
            if (stdinOpen)
                poll(&pfd, 1, timeout);
            else if (timeout)
                usleep(timeout * 1000);
            if (stdinOpen && (pfd.revents & (POLLIN | POLLHUP | POLLERR)))
            {
                char tmp[65536];
                ssize_t n = read(STDIN_FILENO, tmp, sizeof(tmp));
                if (n <= 0)
                    stdinOpen = false;
                else
                    buf.append(tmp, (size_t)n);
                size_t nl;
                while ((nl = buf.find('\n')) != std::string::npos && !ctl.reset && !ctl.quit)
                {
                    std::string line = buf.substr(0, nl);
                    buf.erase(0, nl + 1);
                    if (line.find_first_not_of(" \t\r") == std::string::npos)
                        continue;
                    std::string res = agent::handle(line, ctl);
                    fprintf(stdout, "%s\n", res.c_str());
                    fflush(stdout);
                }
                if (!stdinOpen && o.http_port < 0)
                    break;
            }
            if (httpAgent.isOpen())
                pollHttpAgent(httpAgent, ctl);
            if (freeRunning() && !ctl.reset && !ctl.quit)
                runtime::step();
        }
        if (ctl.reset)
        {
            httpAgent.close();
            reexec(ctl.reset_sd_template);
        }
        cleanup();
        return 0;
    }

    // Headless batch mode.
    if (!config().realtime)
        runtime::runUntilIdle();
    if (!o.script.empty())
        rc = runScript(o.script, ctl);
    if (o.run_ms > 0)
        runtime::runFor((uint64_t)o.run_ms);
    if (!o.screenshot.empty())
    {
        if (!panel::writePng(o.screenshot, panel::logical()))
        {
            fprintf(stderr, "cannot write %s\n", o.screenshot.c_str());
            rc = 2;
        }
        else
            fprintf(stderr, "[twin] screenshot written to %s\n", o.screenshot.c_str());
    }
    if (!warnings().empty())
        fprintf(stderr, "[twin] %zu warning(s); first: %s\n", warnings().size(), warnings().front().c_str());
    if (ctl.reset)
        reexec(ctl.reset_sd_template);
    cleanup();
    return rc;
}
