#ifndef PUBSUBCLIENT_MOCK_H
#define PUBSUBCLIENT_MOCK_H

#include <functional>
#include <map>
#include <string>
#include <vector>


// Mock WiFiClient (forward declare)
class WiFiClient {
public:
  bool isConnected = false;
  std::string host;
  uint16_t port = 0;

  bool connect(const char *host, uint16_t port) {
    this->host = host;
    this->port = port;
    isConnected = true;
    return true;
  }

  void stop() { isConnected = false; }

  bool connected() const { return this->isConnected; }
};

// Mock PubSubClient for MQTT testing
class PubSubClient {
public:
  typedef void (*MQTT_CALLBACK_SIGNATURE)(char *, uint8_t *, unsigned int);

  std::string broker;
  uint16_t brokerPort = 1883;
  std::string clientId;
  std::string username;
  std::string password;
  bool isConnected = false;
  WiFiClient *client = nullptr;
  MQTT_CALLBACK_SIGNATURE callback = nullptr;

  // Published messages (for testing)
  static std::map<std::string, std::string> publishedMessages;
  static std::vector<std::string> subscribedTopics;

  PubSubClient() : client(nullptr), callback(nullptr) {}

  PubSubClient(WiFiClient &wifiClient)
      : client(&wifiClient), callback(nullptr) {}

  void setClient(WiFiClient &wifiClient) { client = &wifiClient; }

  void setServer(const char *domain, uint16_t port) {
    broker = domain ? std::string(domain) : "";
    brokerPort = port;
  }

  void setCallback(MQTT_CALLBACK_SIGNATURE newCallback) {
    callback = newCallback;
  }

  bool connect(const char *id) {
    if (!id || broker.empty()) {
      return false;
    }
    clientId = id;
    if (client) {
      if (!client->connect(broker.c_str(), brokerPort)) {
        return false;
      }
    }
    isConnected = true;
    return true;
  }

  bool connect(const char *id, const char *user, const char *pass) {
    if (!id || !user || !pass) {
      return false;
    }
    username = user;
    password = pass;
    return connect(id);
  }

  void disconnect() {
    isConnected = false;
    if (client) {
      client->stop();
    }
  }

  bool publish(const char *topic, const char *payload) {
    if (!isConnected || !topic || !payload) {
      return false;
    }
    publishedMessages[std::string(topic)] = std::string(payload);
    return true;
  }

  bool publish(const char *topic, const uint8_t *payload,
               unsigned int plength) {
    if (!isConnected || !topic) {
      return false;
    }
    std::string payloadStr(reinterpret_cast<const char *>(payload), plength);
    publishedMessages[std::string(topic)] = payloadStr;
    return true;
  }

  bool subscribe(const char *topic) {
    if (!isConnected || !topic) {
      return false;
    }
    std::string topicStr(topic);
    // Check if already subscribed
    for (const auto &t : subscribedTopics) {
      if (t == topicStr) {
        return true;
      }
    }
    subscribedTopics.push_back(topicStr);
    return true;
  }

  bool unsubscribe(const char *topic) {
    if (!topic) {
      return false;
    }
    std::string topicStr(topic);
    for (auto it = subscribedTopics.begin(); it != subscribedTopics.end();
         ++it) {
      if (*it == topicStr) {
        subscribedTopics.erase(it);
        return true;
      }
    }
    return false;
  }

  bool connected() const { return isConnected; }

  bool loop() {
    // Mock loop - just maintain connection
    return isConnected;
  }

  int state() {
    return isConnected ? 0 : -1; // 0 = connected, -1 = disconnected
  }

  // Static methods for testing
  static void reset() {
    publishedMessages.clear();
    subscribedTopics.clear();
  }

  static bool wasMessagePublished(const char *topic) {
    return publishedMessages.find(std::string(topic)) !=
           publishedMessages.end();
  }

  static std::string getPublishedMessage(const char *topic) {
    auto it = publishedMessages.find(std::string(topic));
    if (it != publishedMessages.end()) {
      return it->second;
    }
    return "";
  }

  static bool wasTopicSubscribed(const char *topic) {
    std::string topicStr(topic);
    for (const auto &t : subscribedTopics) {
      if (t == topicStr) {
        return true;
      }
    }
    return false;
  }

  static void clearMessages() { publishedMessages.clear(); }

  static void clearSubscriptions() { subscribedTopics.clear(); }
};

// Static member initialization
std::map<std::string, std::string> PubSubClient::publishedMessages;
std::vector<std::string> PubSubClient::subscribedTopics;

#endif // PUBSUBCLIENT_MOCK_H
