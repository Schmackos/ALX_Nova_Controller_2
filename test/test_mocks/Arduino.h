#ifndef ARDUINO_MOCK_H
#define ARDUINO_MOCK_H

#include <cstring>
#include <stdint.h>
#include <string>

// Mock Arduino types
// Basic String mock to support Arduino-style methods used in tests
class String : public std::string {
public:
  String() : std::string() {}
  String(const char *s) : std::string(s ? s : "") {}
  String(const std::string &s) : std::string(s) {}
  String(int v) : std::string(std::to_string(v)) {}
  String(long v) : std::string(std::to_string(v)) {}
  String(unsigned int v) : std::string(std::to_string(v)) {}
  String(unsigned long v) : std::string(std::to_string(v)) {}
  String(float v) : std::string(std::to_string(v)) {}
  String(double v) : std::string(std::to_string(v)) {}

  String &operator+=(const String &other) {
    this->append(other);
    return *this;
  }

  String &operator+=(const char *other) {
    this->append(other ? other : "");
    return *this;
  }

  String &operator+=(char c) {
    this->append(1, c);
    return *this;
  }

  String &operator+=(int v) {
    this->append(std::to_string(v));
    return *this;
  }

  String &operator+=(unsigned int v) {
    this->append(std::to_string(v));
    return *this;
  }

  String &operator+=(long v) {
    this->append(std::to_string(v));
    return *this;
  }

  String &operator+=(unsigned long v) {
    this->append(std::to_string(v));
    return *this;
  }

  String &operator+=(unsigned long long v) {
    this->append(std::to_string(v));
    return *this;
  }

  String &operator+=(float v) {

    this->append(std::to_string(v));
    return *this;
  }

  String &operator+=(double v) {
    this->append(std::to_string(v));
    return *this;
  }

  bool equals(const String &other) const { return *this == other; }
  int toInt() const {
    try {
      return std::stoi(*this);
    } catch (...) {
      return 0;
    }
  }
  float toFloat() const {
    try {
      return std::stof(*this);
    } catch (...) {
      return 0.0;
    }
  }

  int indexOf(const String &s, int start = 0) const {
    size_t pos = this->find(s, (size_t)start);
    return (pos == std::string::npos) ? -1 : (int)pos;
  }

  int indexOf(char c, int start = 0) const {
    size_t pos = this->find(c, (size_t)start);
    return (pos == std::string::npos) ? -1 : (int)pos;
  }

  int lastIndexOf(char c) const {
    size_t pos = this->rfind(c);
    return (pos == std::string::npos) ? -1 : (int)pos;
  }

  int lastIndexOf(char c, int start) const {
    size_t pos = this->rfind(c, (size_t)start);
    return (pos == std::string::npos) ? -1 : (int)pos;
  }

  int lastIndexOf(const String &s) const {
    size_t pos = this->rfind(s);
    return (pos == std::string::npos) ? -1 : (int)pos;
  }

  int lastIndexOf(const String &s, int start) const {
    size_t pos = this->rfind(s, (size_t)start);
    return (pos == std::string::npos) ? -1 : (int)pos;
  }

  int lastIndexOf(const char *s) const {
    size_t pos = this->rfind(s ? s : "");
    return (pos == std::string::npos) ? -1 : (int)pos;
  }

  int lastIndexOf(const char *s, int start) const {
    size_t pos = this->rfind(s ? s : "", (size_t)start);
    return (pos == std::string::npos) ? -1 : (int)pos;
  }

  String substring(int start, int end = -1) const {
    if (start < 0)
      start = 0;
    if ((size_t)start >= this->length())
      return String("");
    if (end == -1 || (size_t)end > this->length())
      return String(this->substr((size_t)start).c_str());
    return String(this->substr((size_t)start, (size_t)(end - start)).c_str());
  }

  void toLowerCase() {
    for (auto &c : *this)
      c = (char)tolower((unsigned char)c);
  }

  void toUpperCase() {
    for (auto &c : *this)
      c = (char)toupper((unsigned char)c);
  }

  const char *c_str() const { return std::string::c_str(); }
  unsigned int length() const { return (unsigned int)std::string::length(); }

  char operator[](unsigned int index) const {
    if (index >= std::string::length())
      return 0;
    return std::string::operator[](index);
  }

  char &operator[](unsigned int index) {
    return std::string::operator[](index);
  }
};

typedef bool boolean;
typedef uint8_t byte;

// Mock isDigit
inline bool isDigit(char c) { return c >= '0' && c <= '9'; }

// Pin modes
#define INPUT 0x0
#define OUTPUT 0x1
#define INPUT_PULLUP 0x2

// Pin states
#define LOW 0x0
#define HIGH 0x1

// Mock global state for testing (header-only implementation)
namespace ArduinoMock {
static unsigned long mockMillis = 0;
static int mockAnalogValue = 0;
static int mockDigitalPins[50] = {0};

// Reset function for tests
inline void reset() {
  mockMillis = 0;
  mockAnalogValue = 0;
  std::memset(mockDigitalPins, 0, sizeof(mockDigitalPins));
}
} // namespace ArduinoMock

// Mock Arduino functions
inline unsigned long millis() { return ArduinoMock::mockMillis; }
inline void delay(unsigned long ms) { ArduinoMock::mockMillis += ms; }
inline int analogRead(uint8_t pin) { return ArduinoMock::mockAnalogValue; }
inline void digitalWrite(uint8_t pin, uint8_t val) {
  if (pin < 50)
    ArduinoMock::mockDigitalPins[pin] = val;
}
inline int digitalRead(uint8_t pin) {
  return (pin < 50) ? ArduinoMock::mockDigitalPins[pin] : LOW;
}
inline void pinMode(uint8_t pin, uint8_t mode) { /* Mock - do nothing */ }

// Mock random functions
inline void randomSeed(unsigned long seed) { srand(seed); }
inline long random(long max) { return max > 0 ? rand() % max : 0; }
inline long random(long min, long max) {
  return (max > min) ? (min + (rand() % (max - min))) : min;
}

// Mock LEDC functions for buzzer tests
namespace ArduinoMock {
static uint32_t ledcLastChannel = 0;
static double ledcLastFreq = 0;
static uint32_t ledcLastDuty = 0;
static uint8_t ledcSetupCount = 0;
static uint8_t ledcAttachCount = 0;
static uint8_t ledcWriteToneCount = 0;
static uint8_t ledcWriteCount = 0;

inline void resetLedc() {
  ledcLastChannel = 0;
  ledcLastFreq = 0;
  ledcLastDuty = 0;
  ledcSetupCount = 0;
  ledcAttachCount = 0;
  ledcWriteToneCount = 0;
  ledcWriteCount = 0;
}
} // namespace ArduinoMock

inline void ledcSetup(uint8_t channel, double freq, uint8_t resolution) {
  ArduinoMock::ledcLastChannel = channel;
  ArduinoMock::ledcSetupCount++;
}
inline void ledcAttachPin(uint8_t pin, uint8_t channel) {
  ArduinoMock::ledcAttachCount++;
}
inline double ledcWriteTone(uint8_t channel, double freq) {
  ArduinoMock::ledcLastChannel = channel;
  ArduinoMock::ledcLastFreq = freq;
  ArduinoMock::ledcWriteToneCount++;
  return freq;
}
inline void ledcWrite(uint8_t channel, uint32_t duty) {
  ArduinoMock::ledcLastChannel = channel;
  ArduinoMock::ledcLastDuty = duty;
  ArduinoMock::ledcWriteCount++;
}

#endif // ARDUINO_MOCK_H
