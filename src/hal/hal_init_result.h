#pragma once
// HalInitResult — structured init() return type for HAL devices.
//
// Replaces bare `bool` so init failures carry an error code and reason string.
// Helpers `hal_init_ok()` / `hal_init_fail()` keep driver code clean.

#include <stdint.h>
#include <string.h>
#include "../diag_error_codes.h"

struct HalInitResult {
    bool        success;
    uint16_t    errorCode;   // DiagErrorCode (DIAG_OK on success)
    char        reason[48];  // Human-readable failure reason
};

// Convenience constructors
inline HalInitResult hal_init_ok() {
    HalInitResult r;
    r.success = true;
    r.errorCode = (uint16_t)DIAG_OK;
    r.reason[0] = '\0';
    return r;
}

inline HalInitResult hal_init_fail(DiagErrorCode code, const char* reason) {
    HalInitResult r;
    r.success = false;
    r.errorCode = (uint16_t)code;
    if (reason) {
        strncpy(r.reason, reason, sizeof(r.reason) - 1);
        r.reason[sizeof(r.reason) - 1] = '\0';
    } else {
        r.reason[0] = '\0';
    }
    return r;
}
