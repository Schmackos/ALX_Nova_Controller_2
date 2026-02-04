#ifndef PREFERENCES_MOCK_H
#define PREFERENCES_MOCK_H

#include "Arduino.h"
#include <map>
#include <string>

// Mock Preferences class for NVS storage testing
class Preferences {
public:
  static std::map<std::string, std::map<std::string, std::string>> storage;

  std::string currentNamespace;
  bool readOnlyMode = false;

  bool begin(const char *name, bool readOnly = false) {
    currentNamespace = name ? std::string(name) : "";
    readOnlyMode = readOnly;
    // Create namespace if it doesn't exist
    if (storage.find(currentNamespace) == storage.end()) {
      storage[currentNamespace] = std::map<std::string, std::string>();
    }
    return true;
  }

  void end() { currentNamespace.clear(); }

  // String methods
  String getString(const char *key, const String &defaultValue = "") {
    if (currentNamespace.empty() || !key) {
      return defaultValue;
    }
    std::string keyStr(key);
    auto nsIt = storage.find(currentNamespace);
    if (nsIt != storage.end()) {
      auto keyIt = nsIt->second.find(keyStr);
      if (keyIt != nsIt->second.end()) {
        return String(keyIt->second.c_str());
      }
    }
    return defaultValue;
  }

  void putString(const char *key, const String &value) {
    if (readOnlyMode || currentNamespace.empty() || !key) {
      return;
    }
    storage[currentNamespace][std::string(key)] = std::string(value.c_str());
  }

  // Bool methods
  bool getBool(const char *key, bool defaultValue = false) {
    if (currentNamespace.empty() || !key) {
      return defaultValue;
    }
    std::string keyStr(key);
    auto nsIt = storage.find(currentNamespace);
    if (nsIt != storage.end()) {
      auto keyIt = nsIt->second.find(keyStr);
      if (keyIt != nsIt->second.end()) {
        return keyIt->second == "true" || keyIt->second == "1";
      }
    }
    return defaultValue;
  }

  void putBool(const char *key, bool value) {
    if (readOnlyMode || currentNamespace.empty() || !key) {
      return;
    }
    storage[currentNamespace][std::string(key)] = value ? "true" : "false";
  }

  // Int methods
  int getInt(const char *key, int defaultValue = 0) {
    if (currentNamespace.empty() || !key) {
      return defaultValue;
    }
    std::string keyStr(key);
    auto nsIt = storage.find(currentNamespace);
    if (nsIt != storage.end()) {
      auto keyIt = nsIt->second.find(keyStr);
      if (keyIt != nsIt->second.end()) {
        return std::stoi(keyIt->second);
      }
    }
    return defaultValue;
  }

  void putInt(const char *key, int value) {
    if (readOnlyMode || currentNamespace.empty() || !key) {
      return;
    }
    storage[currentNamespace][std::string(key)] = std::to_string(value);
  }

  // Double methods (for floats/doubles)
  double getDouble(const char *key, double defaultValue = 0.0) {
    if (currentNamespace.empty() || !key) {
      return defaultValue;
    }
    std::string keyStr(key);
    auto nsIt = storage.find(currentNamespace);
    if (nsIt != storage.end()) {
      auto keyIt = nsIt->second.find(keyStr);
      if (keyIt != nsIt->second.end()) {
        return std::stod(keyIt->second);
      }
    }
    return defaultValue;
  }

  void putDouble(const char *key, double value) {
    if (readOnlyMode || currentNamespace.empty() || !key) {
      return;
    }
    storage[currentNamespace][std::string(key)] = std::to_string(value);
  }

  // Check if key exists
  bool isKey(const char *key) {
    if (currentNamespace.empty() || !key) {
      return false;
    }
    std::string keyStr(key);
    auto nsIt = storage.find(currentNamespace);
    if (nsIt != storage.end()) {
      return nsIt->second.find(keyStr) != nsIt->second.end();
    }
    return false;
  }

  // Clear namespace
  bool remove(const char *key) {
    if (readOnlyMode || currentNamespace.empty() || !key) {
      return false;
    }
    std::string keyStr(key);
    auto nsIt = storage.find(currentNamespace);
    if (nsIt != storage.end()) {
      return nsIt->second.erase(keyStr) > 0;
    }
    return false;
  }

  void clear() {
    if (readOnlyMode || currentNamespace.empty()) {
      return;
    }
    storage[currentNamespace].clear();
  }

  // Static method to clear all storage
  static void clearAll() { storage.clear(); }

  // Static method to reset for tests
  static void reset() { clearAll(); }
};

// Static member initialization
std::map<std::string, std::map<std::string, std::string>> Preferences::storage;

#endif // PREFERENCES_MOCK_H
