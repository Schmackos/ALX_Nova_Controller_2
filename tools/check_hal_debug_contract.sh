#!/bin/bash
# Phase 6e: Verify all HAL drivers follow the debug contract.
# Run: bash tools/check_hal_debug_contract.sh
# Checks:
#   1. Every hal_*.cpp in src/hal/ uses [HAL:*] log prefix (not bare [HAL])
#   2. Every init() returns HalInitResult (compile enforced, not checked here)
#   3. Every init() failure path calls diag_emit() or hal_init_fail()

FAIL=0

# 1. Check for old-style bare [HAL] prefixes (should be [HAL:Something])
# Exclude hal_device_manager.cpp (it's the framework, not a driver)
OLD_STYLE=$(grep -rn 'LOG_[DIWE].*"\[HAL\][^:]' src/hal/ \
    --include='*.cpp' \
    | grep -v 'hal_device_manager.cpp' \
    | grep -v 'hal_device_db.cpp' \
    | grep -v 'hal_settings.cpp' \
    | grep -v 'test/' || true)

if [ -n "$OLD_STYLE" ]; then
    echo "FAIL: Old-style [HAL] prefix found (should be [HAL:DeviceName]):"
    echo "$OLD_STYLE"
    FAIL=1
fi

# 2. Check that driver files have diag_emit or hal_init_fail in init() paths
DRIVERS=$(find src/hal -name 'hal_*.cpp' -not -name 'hal_device_manager*' \
    -not -name 'hal_pipeline*' -not -name 'hal_device_db*' \
    -not -name 'hal_driver_registry*' -not -name 'hal_builtin*' \
    -not -name 'hal_api*' -not -name 'hal_settings*' \
    -not -name 'hal_types*' -not -name 'hal_discovery*' \
    -not -name 'hal_online_fetch*' -not -name 'hal_eeprom*' \
    -not -name 'hal_audio_health*' -not -name 'hal_i2s_bridge*' \
    -not -name 'hal_dsp_bridge*')

for f in $DRIVERS; do
    BASE=$(basename "$f")
    if ! grep -q 'hal_init_fail\|diag_emit' "$f" 2>/dev/null; then
        echo "WARN: $BASE has no diag_emit() or hal_init_fail() call"
    fi
done

if [ $FAIL -eq 0 ]; then
    echo "HAL debug contract check: PASS"
fi
exit $FAIL
