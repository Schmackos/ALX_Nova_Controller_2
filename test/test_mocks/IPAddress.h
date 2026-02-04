#ifndef IPADDRESS_MOCK_H
#define IPADDRESS_MOCK_H

#include <cstdint>
#include <string>
#include <cstdio>

// Mock IPAddress class
class IPAddress {
private:
    uint32_t _address = 0;
    uint8_t parts[4] = {0, 0, 0, 0};

public:
    IPAddress() : _address(0) {
        parts[0] = parts[1] = parts[2] = parts[3] = 0;
    }

    IPAddress(uint8_t first, uint8_t second, uint8_t third, uint8_t fourth) {
        parts[0] = first;
        parts[1] = second;
        parts[2] = third;
        parts[3] = fourth;
    }

    IPAddress(const uint8_t* address) {
        parts[0] = address[0];
        parts[1] = address[1];
        parts[2] = address[2];
        parts[3] = address[3];
    }

    // Comparison operators
    bool operator==(const IPAddress& other) const {
        return (parts[0] == other.parts[0] && parts[1] == other.parts[1] &&
                parts[2] == other.parts[2] && parts[3] == other.parts[3]);
    }

    bool operator!=(const IPAddress& other) const {
        return !(*this == other);
    }

    // Access operators
    uint8_t operator[](int index) const {
        if (index >= 0 && index < 4) {
            return parts[index];
        }
        return 0;
    }

    uint8_t& operator[](int index) {
        return parts[index];
    }

    // String conversion
    std::string toString() const {
        char buffer[16];
        snprintf(buffer, sizeof(buffer), "%d.%d.%d.%d",
                 parts[0], parts[1], parts[2], parts[3]);
        return std::string(buffer);
    }

    // Parse IP address from string (e.g., "192.168.1.1")
    bool fromString(const char* address) {
        if (!address) return false;

        int a, b, c, d;
        if (sscanf(address, "%d.%d.%d.%d", &a, &b, &c, &d) != 4) {
            return false;
        }

        // Validate range (0-255)
        if (a < 0 || a > 255 || b < 0 || b > 255 ||
            c < 0 || c > 255 || d < 0 || d > 255) {
            return false;
        }

        parts[0] = static_cast<uint8_t>(a);
        parts[1] = static_cast<uint8_t>(b);
        parts[2] = static_cast<uint8_t>(c);
        parts[3] = static_cast<uint8_t>(d);
        return true;
    }

    bool fromString(const std::string& address) {
        return fromString(address.c_str());
    }

    // Check if valid (non-zero)
    bool isValid() const {
        return _address != 0 || (parts[0] != 0 || parts[1] != 0 ||
                                  parts[2] != 0 || parts[3] != 0);
    }
};

// Special constant for no address
const IPAddress IPADDR_NONE;

#endif // IPADDRESS_MOCK_H
