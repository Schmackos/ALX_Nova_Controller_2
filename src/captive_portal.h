#pragma once

#include <string.h>

// Pure functions for captive portal logic — testable without hardware.

// Returns true if the URI path matches a known OS connectivity-check probe URL.
// These probes are sent by OS captive portal detection mechanisms.
inline bool captive_portal_is_probe_url(const char* uri) {
    if (!uri) return false;
    return (strcmp(uri, "/generate_204")               == 0 ||  // Android/Chrome
            strcmp(uri, "/gen_204")                    == 0 ||  // Android (alt)
            strcmp(uri, "/hotspot-detect.html")        == 0 ||  // Apple iOS/macOS
            strcmp(uri, "/library/test/success.html")  == 0 ||  // Apple (alt)
            strcmp(uri, "/connecttest.txt")            == 0 ||  // Windows 10/11
            strcmp(uri, "/redirect")                   == 0 ||  // Windows stage 2
            strcmp(uri, "/ncsi.txt")                   == 0 ||  // Windows legacy
            strcmp(uri, "/success.txt")                == 0 ||  // Firefox
            strcmp(uri, "/canonical.html")             == 0 ||  // Firefox (alt)
            strcmp(uri, "/connectivity-check")         == 0 ||  // Ubuntu/NetworkManager
            strcmp(uri, "/check_network_status.txt")   == 0);   // Samsung
}

// Returns true if the Host header refers to this device's own IP.
// Direct requests to the device IP should NOT be redirected.
// DNS-hijacked requests from AP clients will have external hostnames.
inline bool captive_portal_is_device_host(const char* hostHeader,
                                           const char* apIP,
                                           const char* staIP) {
    if (!hostHeader || hostHeader[0] == '\0') return true; // empty host → direct request
    if (apIP  && strcmp(hostHeader, apIP)  == 0) return true;
    if (staIP && strcmp(hostHeader, staIP) == 0) return true;
    // Also match bare IPs without port
    if (strncmp(hostHeader, "192.168.4.", 10) == 0) return true;
    return false;
}
