#ifndef LITTLEFS_MOCK_H
#define LITTLEFS_MOCK_H

// In-memory LittleFS mock for native tests.
// Backed by std::map<std::string, std::string> — each "file" is a binary blob.
// Supports open, read, write, seek, size, close, remove, exists.

#include <cstring>
#include <map>
#include <string>
#include <stdint.h>

// File open modes (match ESP32 LittleFS)
#define FILE_READ   "r"
#define FILE_WRITE  "w"
#define FILE_APPEND "a"

class MockFile {
public:
    MockFile() : _valid(false), _pos(0), _writable(false) {}
    MockFile(const std::string& path, std::string* data, bool writable, bool append)
        : _valid(true), _path(path), _data(data), _writable(writable) {
        _pos = append ? data->size() : 0;
    }

    explicit operator bool() const { return _valid; }

    size_t size() const { return _valid && _data ? _data->size() : 0; }

    size_t read(uint8_t* buf, size_t len) {
        if (!_valid || !_data || _pos >= _data->size()) return 0;
        size_t avail = _data->size() - _pos;
        size_t toRead = (len < avail) ? len : avail;
        memcpy(buf, _data->data() + _pos, toRead);
        _pos += toRead;
        return toRead;
    }

    size_t write(const uint8_t* buf, size_t len) {
        if (!_valid || !_writable || !_data) return 0;
        // Extend if needed
        size_t needed = _pos + len;
        if (needed > _data->size()) {
            _data->resize(needed, '\0');
        }
        memcpy(&(*_data)[_pos], buf, len);
        _pos += len;
        return len;
    }

    bool seek(size_t pos) {
        if (!_valid) return false;
        _pos = pos;
        return true;
    }

    size_t position() const { return _pos; }

    int available() {
        if (!_valid || !_data) return 0;
        return (_pos < _data->size()) ? (int)(_data->size() - _pos) : 0;
    }

    void close() { _valid = false; }

    const char* name() const { return _path.c_str(); }

private:
    bool _valid;
    std::string _path;
    std::string* _data;  // Points into MockFS storage
    size_t _pos;
    bool _writable;
};

// Use MockFile as the File type in tests
typedef MockFile File;

class MockFS {
public:
    static std::map<std::string, std::string> _files;
    static bool _mounted;
    static bool _mountShouldFail;

    bool begin(bool formatOnFail = false, const char* basePath = "/littlefs",
               uint8_t maxOpenFiles = 10, const char* partitionLabel = nullptr) {
        if (_mountShouldFail) return false;
        _mounted = true;
        return true;
    }

    void end() { _mounted = false; }

    MockFile open(const char* path, const char* mode = FILE_READ) {
        if (!_mounted || !path) return MockFile();
        std::string p(path);

        if (strcmp(mode, FILE_READ) == 0 || strcmp(mode, "r") == 0) {
            auto it = _files.find(p);
            if (it == _files.end()) return MockFile();
            return MockFile(p, &it->second, false, false);
        }
        if (strcmp(mode, FILE_WRITE) == 0 || strcmp(mode, "w") == 0) {
            _files[p] = "";  // Truncate on write mode
            return MockFile(p, &_files[p], true, false);
        }
        if (strcmp(mode, FILE_APPEND) == 0 || strcmp(mode, "a") == 0) {
            if (_files.find(p) == _files.end()) _files[p] = "";
            return MockFile(p, &_files[p], true, true);
        }
        return MockFile();
    }

    bool exists(const char* path) {
        return _mounted && path && _files.find(std::string(path)) != _files.end();
    }

    bool remove(const char* path) {
        if (!_mounted || !path) return false;
        return _files.erase(std::string(path)) > 0;
    }

    bool rename(const char* from, const char* to) {
        if (!_mounted || !from || !to) return false;
        auto it = _files.find(std::string(from));
        if (it == _files.end()) return false;
        _files[std::string(to)] = it->second;
        _files.erase(it);
        return true;
    }

    // Test helpers
    static void reset() {
        _files.clear();
        _mounted = false;
        _mountShouldFail = false;
    }

    static void injectFile(const char* path, const std::string& data) {
        _files[std::string(path)] = data;
    }

    static std::string getFile(const char* path) {
        auto it = _files.find(std::string(path));
        return (it != _files.end()) ? it->second : "";
    }
};

// Static member definitions
std::map<std::string, std::string> MockFS::_files;
bool MockFS::_mounted = false;
bool MockFS::_mountShouldFail = false;

// Global LittleFS instance (mirrors ESP32 Arduino's extern LittleFS)
static MockFS LittleFS;

#endif // LITTLEFS_MOCK_H
