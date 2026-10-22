// FlickBook digital twin - SdFat implementation on top of a host directory.
#include "SdFat.h"

#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>

#include <algorithm>
#include <cerrno>
#include <climits>
#include <cstdio>

#include "twin.h"

namespace twin
{
namespace sd
{
namespace
{
bool g_mounted = false;
uint64_t g_used = 0;
bool g_usedValid = false;

std::string lower(std::string s)
{
    for (auto &c : s)
        c = (char)tolower((unsigned char)c);
    return s;
}

std::vector<std::string> splitPath(const std::string &p)
{
    std::vector<std::string> out;
    size_t i = 0;
    while (i < p.size())
    {
        size_t j = p.find('/', i);
        if (j == std::string::npos)
            j = p.size();
        std::string part = p.substr(i, j - i);
        if (!part.empty() && part != ".")
        {
            if (part == "..")
            {
                if (!out.empty())
                    out.pop_back();
            }
            else
                out.push_back(part);
        }
        i = j + 1;
    }
    return out;
}

bool isDirHost(const std::string &p)
{
    struct stat st;
    return stat(p.c_str(), &st) == 0 && S_ISDIR(st.st_mode);
}

bool existsHost(const std::string &p)
{
    struct stat st;
    return stat(p.c_str(), &st) == 0;
}

uint64_t duHost(const std::string &p)
{
    struct stat st;
    if (lstat(p.c_str(), &st) != 0)
        return 0;
    if (!S_ISDIR(st.st_mode))
        return (uint64_t)st.st_size;
    uint64_t total = 0;
    DIR *d = opendir(p.c_str());
    if (!d)
        return 0;
    while (dirent *e = readdir(d))
    {
        std::string n = e->d_name;
        if (n == "." || n == "..")
            continue;
        total += duHost(p + "/" + n);
    }
    closedir(d);
    return total;
}
} // namespace

bool mounted() { return g_mounted; }
void setMounted(bool m) { g_mounted = m; }
std::string root() { return config().sd_root; }

bool resolve(const std::string &sdPath, std::string &hostPath, bool &exists)
{
    exists = false;
    std::string cur = root();
    if (cur.empty())
        return false;
    auto parts = splitPath(sdPath);
    for (size_t i = 0; i < parts.size(); ++i)
    {
        const std::string &part = parts[i];
        std::string candidate = cur + "/" + part;
        if (!existsHost(candidate))
        {
            // FAT is case-insensitive: look for a case-insensitive match.
            std::string want = lower(part);
            std::string found;
            if (DIR *d = opendir(cur.c_str()))
            {
                while (dirent *e = readdir(d))
                {
                    if (lower(e->d_name) == want)
                    {
                        found = e->d_name;
                        break;
                    }
                }
                closedir(d);
            }
            if (!found.empty())
                candidate = cur + "/" + found;
            else if (i + 1 == parts.size())
            {
                hostPath = candidate; // last component missing: caller may create it
                return true;
            }
            else
                return false;
        }
        if (i + 1 < parts.size() && !isDirHost(candidate))
            return false;
        cur = candidate;
    }
    hostPath = cur;
    exists = true;
    return true;
}

uint64_t usedBytes()
{
    if (!g_usedValid)
    {
        g_used = duHost(root());
        g_usedValid = true;
    }
    return g_used;
}

void addUsedBytes(int64_t delta)
{
    usedBytes();
    if (delta < 0 && (uint64_t)(-delta) > g_used)
        g_used = 0;
    else
        g_used = (uint64_t)((int64_t)g_used + delta);
}

} // namespace sd
} // namespace twin

using namespace twin;

namespace
{
std::string joinSd(const std::string &dir, const char *path)
{
    if (!path)
        return dir;
    if (path[0] == '/')
        return path;
    if (dir.empty() || dir == "/")
        return std::string("/") + path;
    return dir + "/" + path;
}

std::string baseName(const std::string &p)
{
    size_t s = p.find_last_of('/');
    return s == std::string::npos ? p : p.substr(s + 1);
}

std::string normalizeSd(const std::string &p)
{
    std::string out = "/";
    size_t i = 0;
    std::vector<std::string> parts;
    while (i <= p.size())
    {
        size_t j = p.find('/', i);
        if (j == std::string::npos)
            j = p.size();
        std::string part = p.substr(i, j - i);
        if (part == "..")
        {
            if (!parts.empty())
                parts.pop_back();
        }
        else if (!part.empty() && part != ".")
            parts.push_back(part);
        i = j + 1;
    }
    for (size_t k = 0; k < parts.size(); ++k)
    {
        out += parts[k];
        if (k + 1 < parts.size())
            out += "/";
    }
    return out;
}
} // namespace

bool FatFile::openHost(const std::string &sdPathIn, oflag_t oflag)
{
    if (open_ || !sd::mounted())
        return false;
    std::string sdPath = normalizeSd(sdPathIn);
    std::string host;
    bool exists = false;
    if (!sd::resolve(sdPath, host, exists))
        return false;

    const int acc = oflag & O_ACCMODE;
    struct stat st;
    if (exists && stat(host.c_str(), &st) == 0 && S_ISDIR(st.st_mode))
    {
        if (acc != O_RDONLY)
            return false; // directories cannot be opened for writing
        DIR *d = opendir(host.c_str());
        if (!d)
            return false;
        entries_.clear();
        while (dirent *e = readdir(d))
        {
            std::string n = e->d_name;
            if (n == "." || n == "..")
                continue;
            entries_.push_back(n);
        }
        closedir(d);
        std::sort(entries_.begin(), entries_.end());
        nextEntry_ = 0;
        dir_ = true;
    }
    else
    {
        if (!exists && !(oflag & O_CREAT))
            return false;
        if (exists && (oflag & O_CREAT) && (oflag & O_EXCL))
            return false;
        int hostFlags = acc == O_RDONLY ? O_RDONLY : (acc == O_WRONLY ? O_WRONLY : O_RDWR);
        if (oflag & O_CREAT)
            hostFlags |= O_CREAT;
        uint64_t oldSize = exists ? (uint64_t)st.st_size : 0;
        if ((oflag & O_TRUNC) && acc != O_RDONLY)
        {
            hostFlags |= O_TRUNC;
            sd::addUsedBytes(-(int64_t)oldSize);
        }
        fd_ = ::open(host.c_str(), hostFlags, 0644);
        if (fd_ < 0)
            return false;
        dir_ = false;
    }
    open_ = true;
    flags_ = oflag;
    pos_ = 0;
    sdPath_ = sdPath;
    hostPath_ = host;
    name_ = sdPath == "/" ? "/" : baseName(host);
    if (!dir_ && (oflag & (O_APPEND | O_AT_END)))
        pos_ = fileSize();
    return true;
}

bool FatFile::open(const char *path, oflag_t oflag) { return openHost(joinSd("/", path), oflag); }

bool FatFile::open(FatFile *dirFile, const char *path, oflag_t oflag)
{
    if (!dirFile || !dirFile->isDir())
        return false;
    return openHost(joinSd(dirFile->sdPath_, path), oflag);
}

bool FatFile::openNext(FatFile *dirFile, oflag_t oflag)
{
    if (open_ || !dirFile || !dirFile->isDir())
        return false;
    while (dirFile->nextEntry_ < dirFile->entries_.size())
    {
        const std::string &n = dirFile->entries_[dirFile->nextEntry_++];
        if (openHost(joinSd(dirFile->sdPath_, n.c_str()), oflag))
            return true;
    }
    return false;
}

bool FatFile::close()
{
    if (fd_ >= 0)
        ::close(fd_);
    fd_ = -1;
    bool was = open_;
    open_ = false;
    dir_ = false;
    entries_.clear();
    nextEntry_ = 0;
    pos_ = 0;
    return was;
}

size_t FatFile::getName(char *name, size_t size) const
{
    if (!name || !size)
        return 0;
    if (!open_)
    {
        name[0] = 0;
        return 0;
    }
    size_t n = std::min(size - 1, name_.size());
    memcpy(name, name_.data(), n);
    name[n] = 0;
    return n;
}

uint64_t FatFile::fileSize() const
{
    if (!open_ || dir_)
        return 0;
    struct stat st;
    if (fstat(fd_, &st) != 0)
        return 0;
    return (uint64_t)st.st_size;
}

bool FatFile::seekSet(uint64_t pos)
{
    if (!open_)
        return false;
    if (dir_)
    {
        // Directory positions are 32-byte entry indices on FAT.
        nextEntry_ = (size_t)(pos / 32);
        pos_ = pos;
        return true;
    }
    if (pos > fileSize())
        return false;
    pos_ = pos;
    return true;
}

int FatFile::read(void *buf, size_t count)
{
    if (!open_ || dir_ || !isReadable())
        return -1;
    if (config().sd_read_errors)
    {
        error_ = 1;
        return -1;
    }
    ssize_t r = ::pread(fd_, buf, count, (off_t)pos_);
    if (r < 0)
        return -1;
    pos_ += (uint64_t)r;
    return (int)r;
}

int FatFile::read()
{
    uint8_t b;
    return read(&b, 1) == 1 ? b : -1;
}

int FatFile::peek()
{
    uint64_t p = pos_;
    int c = read();
    pos_ = p;
    return c;
}

uint64_t FatFile::available64() const
{
    if (!open_ || dir_)
        return 0;
    uint64_t s = fileSize();
    return s > pos_ ? s - pos_ : 0;
}

int FatFile::available()
{
    uint64_t a = available64();
    return a > (uint64_t)INT_MAX ? INT_MAX : (int)a;
}

size_t FatFile::readBytes(char *buffer, size_t length)
{
    int r = read(buffer, length);
    return r < 0 ? 0 : (size_t)r;
}

size_t FatFile::write(const uint8_t *buf, size_t count)
{
    if (!open_ || dir_ || !isWritable())
    {
        error_ = 1;
        return 0;
    }
    if (flags_ & O_APPEND)
        pos_ = fileSize();
    uint64_t before = fileSize();
    uint64_t newEnd = pos_ + count;
    if (newEnd > before)
    {
        uint64_t cap = config().sd_capacity_bytes;
        if (cap && sd::usedBytes() + (newEnd - before) > cap)
        {
            error_ = 1;
            twin::serialLogMark("SD card full (capacity %llu bytes)", (unsigned long long)cap);
            return 0;
        }
    }
    ssize_t w = ::pwrite(fd_, buf, count, (off_t)pos_);
    if (w < 0)
    {
        error_ = 1;
        return 0;
    }
    pos_ += (uint64_t)w;
    uint64_t after = fileSize();
    if (after > before)
        sd::addUsedBytes((int64_t)(after - before));
    return (size_t)w;
}

bool FatFile::truncate(uint64_t length)
{
    if (!isFile() || !isWritable())
        return false;
    uint64_t before = fileSize();
    if (ftruncate(fd_, (off_t)length) != 0)
        return false;
    sd::addUsedBytes((int64_t)length - (int64_t)before);
    if (pos_ > length)
        pos_ = length;
    return true;
}

bool FatFile::mkdir(FatFile *parent, const char *path, bool pFlag)
{
    if (open_ || !parent || !parent->isDir() || !sd::mounted())
        return false;
    std::string full = normalizeSd(joinSd(parent->sdPath_, path));
    // Walk components, creating as needed.
    std::string cur = "/";
    std::vector<std::string> parts;
    {
        size_t i = 1;
        while (i <= full.size())
        {
            size_t j = full.find('/', i);
            if (j == std::string::npos)
                j = full.size();
            if (j > i)
                parts.push_back(full.substr(i, j - i));
            i = j + 1;
        }
    }
    if (parts.empty())
        return false;
    for (size_t k = 0; k < parts.size(); ++k)
    {
        std::string next = cur == "/" ? "/" + parts[k] : cur + "/" + parts[k];
        std::string host;
        bool exists = false;
        if (!sd::resolve(next, host, exists))
            return false;
        bool last = k + 1 == parts.size();
        if (exists)
        {
            if (last)
                return false; // already exists
            struct stat st;
            if (stat(host.c_str(), &st) != 0 || !S_ISDIR(st.st_mode))
                return false;
        }
        else
        {
            if (!last && !pFlag)
                return false;
            if (::mkdir(host.c_str(), 0755) != 0)
                return false;
        }
        cur = next;
    }
    return openHost(full, O_RDONLY);
}

bool FatFile::remove()
{
    if (!isFile() || !isWritable())
        return false;
    uint64_t sz = fileSize();
    std::string host = hostPath_;
    close();
    if (::unlink(host.c_str()) != 0)
        return false;
    sd::addUsedBytes(-(int64_t)sz);
    return true;
}

bool FatFile::rmdir()
{
    if (!isDir() || sdPath_ == "/")
        return false;
    std::string host = hostPath_;
    close();
    return ::rmdir(host.c_str()) == 0; // fails if not empty, like SdFat
}

bool FatFile::rename(const char *newPath) { return rename(nullptr, newPath); }

bool FatFile::rename(FatFile *dirFile, const char *newPath)
{
    if (!open_ || sdPath_ == "/" || !newPath)
        return false;
    std::string base = dirFile && dirFile->isDir() ? dirFile->sdPath_ : "/";
    std::string target = normalizeSd(joinSd(base, newPath));
    std::string host;
    bool exists = false;
    if (!sd::resolve(target, host, exists) || exists)
        return false;
    if (::rename(hostPath_.c_str(), host.c_str()) != 0)
        return false;
    sdPath_ = target;
    hostPath_ = host;
    name_ = baseName(host);
    return true;
}

bool FatFile::exists(const char *path)
{
    std::string host;
    bool ex = false;
    std::string base = isDir() ? sdPath_ : "/";
    return sd::mounted() && sd::resolve(normalizeSd(joinSd(base, path)), host, ex) && ex;
}

// ------------------------------------------------------------------ SdFat ---
bool SdFat::begin(const SdSpiConfig &)
{
    const auto &c = config();
    bool ok = c.sd_present && !c.sd_root.empty();
    if (ok)
    {
        struct stat st;
        ok = stat(c.sd_root.c_str(), &st) == 0 && S_ISDIR(st.st_mode);
    }
    sd::setMounted(ok);
    return ok;
}

bool SdFat::exists(const char *path)
{
    std::string host;
    bool ex = false;
    return sd::mounted() && sd::resolve(normalizeSd(path), host, ex) && ex;
}

bool SdFat::mkdir(const char *path, bool pFlag)
{
    FatFile root;
    if (!root.open("/", O_RDONLY))
        return false;
    FatFile d;
    return d.mkdir(&root, path, pFlag);
}

bool SdFat::remove(const char *path)
{
    FatFile f;
    return f.open(path, O_WRONLY) && f.remove();
}

bool SdFat::rmdir(const char *path)
{
    FatFile f;
    return f.open(path, O_RDONLY) && f.rmdir();
}

bool SdFat::rename(const char *oldPath, const char *newPath)
{
    FatFile f;
    return f.open(oldPath, O_RDONLY) && f.rename(newPath);
}
