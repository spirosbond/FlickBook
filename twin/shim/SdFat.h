// FlickBook digital twin - SdFat compatible API backed by a host directory.
//
// Only the subset of SdFat 2.x used by FlickBook and the Inkplate graphics
// library is implemented. Semantics that matter for the firmware are kept:
//   * paths are resolved case-insensitively (FAT behaviour)
//   * open() of a missing file fails unless O_CREAT is given
//   * directories can be opened and iterated with openNext() ('.'/'..' skipped)
//   * remove() requires the file to be open for writing, rmdir() requires an
//     empty directory, rename() fails if the destination exists
// Directory iteration order is alphabetical (deterministic) rather than the
// FAT on-disk creation order.
#pragma once

#include <fcntl.h>

#include <cstdint>
#include <string>
#include <vector>

#include "Arduino.h"

#ifndef O_READ
#define O_READ O_RDONLY
#endif
#ifndef O_WRITE
#define O_WRITE O_WRONLY
#endif
#ifndef O_AT_END
#define O_AT_END 0x40000000
#endif

typedef int oflag_t;

#define SD_SCK_MHZ(maxMhz) (1000000UL * (maxMhz))
#define SHARED_SPI 0
#define DEDICATED_SPI 1

class SPIClass;
struct SdSpiConfig
{
    SdSpiConfig(uint8_t cs = 0, uint8_t opt = SHARED_SPI, uint32_t maxSck = SD_SCK_MHZ(50), SPIClass *spi = nullptr) {}
};

namespace twin
{
namespace sd
{
// Host-side helpers (also used by the twin runtime and WebServer shim).
bool mounted();
void setMounted(bool m);
std::string root();
// Resolve an SD path (absolute or relative to "/") to a host path, matching
// each component case-insensitively. Returns false if a component (other than
// optionally the last one) does not exist. `exists` reports whether the full
// path exists.
bool resolve(const std::string &sdPath, std::string &hostPath, bool &exists);
uint64_t usedBytes();
void addUsedBytes(int64_t delta);
} // namespace sd
} // namespace twin

class FatFile : public Stream
{
public:
    FatFile() = default;
    FatFile(const char *path, oflag_t oflag) { open(path, oflag); }
    ~FatFile() override { close(); }
    FatFile(const FatFile &) = delete;
    FatFile &operator=(const FatFile &) = delete;

    bool open(const char *path, oflag_t oflag = O_RDONLY);
    bool open(FatFile *dirFile, const char *path, oflag_t oflag = O_RDONLY);
    bool openNext(FatFile *dirFile, oflag_t oflag = O_RDONLY);
    bool close();

    bool isOpen() const { return open_; }
    bool isDir() const { return open_ && dir_; }
    bool isFile() const { return open_ && !dir_; }
    bool isSubDir() const { return isDir() && sdPath_ != "/"; }
    bool isRoot() const { return isDir() && sdPath_ == "/"; }
    bool isReadable() const { return open_ && ((flags_ & O_ACCMODE) != O_WRONLY); }
    bool isWritable() const { return open_ && ((flags_ & O_ACCMODE) != O_RDONLY); }
    bool isHidden() const { return false; }
    explicit operator bool() const { return open_; }

    size_t getName(char *name, size_t size) const;
    size_t getSFN(char *name, size_t size) const { return getName(name, size); }
    const std::string &sdPath() const { return sdPath_; }

    uint64_t fileSize() const;
    uint64_t curPosition() const { return pos_; }
    uint64_t position() const { return pos_; }
    uint64_t size() const { return fileSize(); }
    bool seekSet(uint64_t pos);
    bool seekCur(int64_t offset) { return seekSet((uint64_t)((int64_t)pos_ + offset)); }
    bool seekEnd(int64_t offset = 0) { return seekSet((uint64_t)((int64_t)fileSize() + offset)); }
    bool seek(uint64_t pos) { return seekSet(pos); }
    void rewind() { seekSet(0); }

    int read(void *buf, size_t count);
    int read() override;
    int peek() override;
    int available() override;
    uint64_t available64() const;
    size_t readBytes(char *buffer, size_t length) override;
    using Stream::readBytes;

    size_t write(uint8_t b) override { return write(&b, 1); }
    size_t write(const uint8_t *buf, size_t count) override;
    size_t write(const void *buf, size_t count) { return write(static_cast<const uint8_t *>(buf), count); }
    using Print::write;
    void flush() override {}
    bool sync() { return true; }
    bool truncate(uint64_t length);
    bool truncate() { return truncate(pos_); }

    bool mkdir(FatFile *dir, const char *path, bool pFlag = true);
    bool remove();
    bool rmdir();
    bool rename(const char *newPath);
    bool rename(FatFile *dirFile, const char *newPath);
    bool exists(const char *path);

    uint8_t getError() const { return error_; }
    void clearError() { error_ = 0; }

private:
    bool openHost(const std::string &sdPath, oflag_t oflag);

    bool open_ = false;
    bool dir_ = false;
    int fd_ = -1;
    int flags_ = 0;
    uint64_t pos_ = 0;
    std::string sdPath_;
    std::string hostPath_;
    std::string name_;
    std::vector<std::string> entries_; // directory snapshot for openNext
    size_t nextEntry_ = 0;
    uint8_t error_ = 0;
};

typedef FatFile SdBaseFile;
typedef FatFile FsBaseFile;
typedef FatFile File32;
typedef FatFile FsFile;
typedef FatFile ExFile;

class SdFile : public FatFile
{
public:
    SdFile() = default;
    SdFile(const char *path, oflag_t oflag) : FatFile(path, oflag) {}
};

class SdFat
{
public:
    bool begin(uint8_t csPin = 0, uint32_t maxSck = 0) { return begin(SdSpiConfig()); }
    bool begin(const SdSpiConfig &cfg);
    void end() {}
    bool exists(const char *path);
    bool exists(const String &path) { return exists(path.c_str()); }
    bool mkdir(const char *path, bool pFlag = true);
    bool remove(const char *path);
    bool rmdir(const char *path);
    bool rename(const char *oldPath, const char *newPath);
    bool chdir(const char *path = "/") { return true; }
    FsFile open(const char *path, oflag_t oflag = O_RDONLY) = delete; // FsFile is not copyable on the twin
    uint8_t cardErrorCode() const { return twin::sd::mounted() ? 0 : 1; }
};

typedef SdFat SdFat32;
typedef SdFat SdExFat;
typedef SdFat SdFs;
