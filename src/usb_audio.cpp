#include "usb_audio.h"
#include <cstring>
#include <cmath>

// ===== Ring Buffer Implementation (pure, lock-free SPSC) =====

void usb_rb_init(UsbAudioRingBuffer *rb, int32_t *buffer, uint32_t capacity_frames) {
    rb->buffer = buffer;
    rb->capacity = capacity_frames;
    rb->writePos = 0;
    rb->readPos = 0;
    rb->overruns = 0;
    rb->underruns = 0;
}

void usb_rb_reset(UsbAudioRingBuffer *rb) {
    rb->writePos = 0;
    rb->readPos = 0;
    rb->overruns = 0;
    rb->underruns = 0;
}

uint32_t usb_rb_available(const UsbAudioRingBuffer *rb) {
    uint32_t w = rb->writePos;
    uint32_t r = rb->readPos;
    return (w - r) & (rb->capacity - 1);
}

uint32_t usb_rb_free(const UsbAudioRingBuffer *rb) {
    // Reserve one slot to distinguish full from empty
    return rb->capacity - 1 - usb_rb_available(rb);
}

float usb_rb_fill_level(const UsbAudioRingBuffer *rb) {
    if (rb->capacity == 0) return 0.0f;
    return (float)usb_rb_available(rb) / (float)(rb->capacity - 1);
}

uint32_t usb_rb_write(UsbAudioRingBuffer *rb, const int32_t *data, uint32_t frames) {
    uint32_t free_frames = usb_rb_free(rb);
    if (frames > free_frames) {
        rb->overruns += (frames - free_frames);
        frames = free_frames;
    }
    if (frames == 0) return 0;

    uint32_t mask = rb->capacity - 1;
    uint32_t pos = rb->writePos & mask;

    for (uint32_t i = 0; i < frames; i++) {
        uint32_t idx = (pos + i) & mask;
        rb->buffer[idx * 2]     = data[i * 2];     // Left
        rb->buffer[idx * 2 + 1] = data[i * 2 + 1]; // Right
    }

    // Memory barrier implicit on ESP32 (single-core producer).
    // On native tests, sequential consistency is fine.
    rb->writePos = (rb->writePos + frames) & (rb->capacity * 2 - 1);
    return frames;
}

uint32_t usb_rb_read(UsbAudioRingBuffer *rb, int32_t *data, uint32_t frames) {
    uint32_t avail = usb_rb_available(rb);
    if (frames > avail) {
        rb->underruns += (frames - avail);
        frames = avail;
    }
    if (frames == 0) return 0;

    uint32_t mask = rb->capacity - 1;
    uint32_t pos = rb->readPos & mask;

    for (uint32_t i = 0; i < frames; i++) {
        uint32_t idx = (pos + i) & mask;
        data[i * 2]     = rb->buffer[idx * 2];     // Left
        data[i * 2 + 1] = rb->buffer[idx * 2 + 1]; // Right
    }

    rb->readPos = (rb->readPos + frames) & (rb->capacity * 2 - 1);
    return frames;
}

// ===== Format Conversion (pure) =====

void usb_pcm16_to_int32(const int16_t *src, int32_t *dst, uint32_t frames) {
    // PCM16 → left-justified int32: shift left 16 to fill bits [31:16]
    // Then the I2S pipeline expects bits [31:8] = 24-bit audio data.
    // PCM16 in bits [31:16] with zeros in [15:0] is correct for 16-bit audio
    // in a 24-bit-capable pipeline (the lower 8 bits of the 24-bit range are zero).
    for (uint32_t i = 0; i < frames * 2; i++) {
        dst[i] = ((int32_t)src[i]) << 16;
    }
}

void usb_pcm24_to_int32(const uint8_t *src, int32_t *dst, uint32_t frames) {
    // Packed 24-bit (3 bytes per sample, little-endian) → left-justified int32
    // Byte order: [low, mid, high] per sample
    // Result: bits [31:8] = 24-bit audio, bits [7:0] = 0
    for (uint32_t i = 0; i < frames * 2; i++) {
        uint32_t offset = i * 3;
        int32_t sample = (int32_t)(
            ((uint32_t)src[offset])          |
            ((uint32_t)src[offset + 1] << 8) |
            ((uint32_t)src[offset + 2] << 16)
        );
        // Sign extend from 24-bit
        if (sample & 0x800000) {
            sample |= 0xFF000000;
        }
        // Left-justify: shift left 8 so bits [31:8] hold the 24-bit value
        dst[i] = sample << 8;
    }
}

float usb_volume_to_linear(int16_t volume_256db) {
    // UAC2 volume: 1/256 dB steps. -32767 = -128 dB (silence), 0 = 0 dB (unity)
    if (volume_256db <= -32767) return 0.0f;
    if (volume_256db >= 0) return 1.0f;
    float db = (float)volume_256db / 256.0f;
    return powf(10.0f, db / 20.0f);
}

// ===== Pure Timeout Logic (available in all build modes) =====

bool usb_audio_is_stream_timed_out(unsigned long current_ms, unsigned long last_data_ms, unsigned long timeout_ms) {
    if (last_data_ms == 0) return false;
    return (current_ms - last_data_ms) > timeout_ms;
}

// ===== ESP32 Hardware Implementation =====
#if !defined(NATIVE_TEST) && defined(USB_AUDIO_ENABLED)

#include "app_state.h"
#include "config.h"
#include "debug_serial.h"
#include <esp_heap_caps.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

// TinyUSB headers
#include "tusb.h"
#include "device/usbd_pvt.h"
#include "class/audio/audio.h"
#include "esp32-hal-tinyusb.h"

// ===== UAC2 Descriptor Constants =====
#define USB_AUDIO_SAMPLE_RATE   48000
#define USB_AUDIO_BIT_DEPTH     16
#define USB_AUDIO_CHANNELS      2
#define USB_AUDIO_SUBSLOT_SIZE  (USB_AUDIO_BIT_DEPTH / 8)

// Endpoint max packet: frames_per_ms * channels * bytes_per_sample + slack
// 48 frames/ms * 2 ch * 2 bytes = 192 bytes. +4 for adaptive slack = 196.
#define USB_AUDIO_EP_SIZE       ((USB_AUDIO_SAMPLE_RATE / 1000 + 1) * USB_AUDIO_CHANNELS * USB_AUDIO_SUBSLOT_SIZE)

// UAC2 Entity IDs
#define UAC2_ENTITY_CLOCK       0x01
#define UAC2_ENTITY_INPUT_TERM  0x02
#define UAC2_ENTITY_FEATURE     0x03
#define UAC2_ENTITY_OUTPUT_TERM 0x04

// ===== Module State =====
static UsbAudioState _usbState = USB_AUDIO_DISCONNECTED;
static UsbAudioRingBuffer _ringBuffer = {};
static int32_t *_ringBufStorage = nullptr;
static uint8_t _epOut = 0;
static uint8_t _itfNum = 0;          // Assigned by framework
static int16_t _hostVolume = 0;      // 1/256 dB
static bool _hostMute = false;
static uint8_t _altSetting = 0;      // Current AS alt setting (0=idle, 1=streaming)
static bool _tinyusbHwReady = false; // True after tinyusb_init() succeeds (one-shot)
static volatile unsigned long _lastDataMs = 0;     // millis() of last xfer_cb with data
#define USB_STREAM_TIMEOUT_MS 500                   // No data for 500ms -> idle

// Saved endpoint descriptor from initial enumeration (needed for iso_alloc/activate in SET_INTERFACE)
static tusb_desc_endpoint_t _savedEpDesc;
static bool _epDescSaved = false;

// Control request response buffer
static uint8_t _ctrlBuf[64];

// Buffer for receiving isochronous OUT data (must be declared before control_xfer which primes it)
static uint8_t _isoOutBuf[USB_AUDIO_EP_SIZE] __attribute__((aligned(4)));

// Ring buffer capacity: 20ms at 48kHz = 960 frames, round up to power of 2 = 1024
#define RING_BUF_CAPACITY 1024

// ===== UAC2 Descriptor Builder =====

// Total descriptor length (computed from structure)
#define UAC2_AC_CS_LEN  (9 + 8 + 17 + 18 + 12)  // Header + Clock + IT + FU + OT = 64
#define UAC2_DESC_TOTAL_LEN (8 + 9 + UAC2_AC_CS_LEN + 9 + 9 + 16 + 6 + 7 + 8)  // = 136

static uint16_t usb_audio_descriptor_cb(uint8_t *dst, uint8_t *itf) {
    uint8_t *p = dst;
    uint8_t ac_itf = *itf;
    uint8_t as_itf = ac_itf + 1;

    // Get a free OUT endpoint for isochronous audio
    _epOut = tinyusb_get_free_out_endpoint();
    if (_epOut == 0) {
        return 0; // No free endpoint available
    }

    _itfNum = ac_itf;

    // --- IAD (Interface Association Descriptor) ---
    *p++ = 8;                                          // bLength
    *p++ = TUSB_DESC_INTERFACE_ASSOCIATION;             // bDescriptorType
    *p++ = ac_itf;                                     // bFirstInterface
    *p++ = 2;                                          // bInterfaceCount (AC + AS)
    *p++ = TUSB_CLASS_AUDIO;                           // bFunctionClass
    *p++ = AUDIO_SUBCLASS_UNDEFINED;                   // bFunctionSubClass
    *p++ = AUDIO_FUNC_PROTOCOL_CODE_V2;                // bFunctionProtocol (UAC2)
    *p++ = 0;                                          // iFunction

    // --- Audio Control Interface (standard) ---
    *p++ = 9;                                          // bLength
    *p++ = TUSB_DESC_INTERFACE;                        // bDescriptorType
    *p++ = ac_itf;                                     // bInterfaceNumber
    *p++ = 0;                                          // bAlternateSetting
    *p++ = 0;                                          // bNumEndpoints
    *p++ = TUSB_CLASS_AUDIO;                           // bInterfaceClass
    *p++ = AUDIO_SUBCLASS_CONTROL;                     // bInterfaceSubClass
    *p++ = AUDIO_INT_PROTOCOL_CODE_V2;                 // bInterfaceProtocol (UAC2)
    *p++ = 0;                                          // iInterface

    // --- CS AC Interface Header (UAC2) ---
    *p++ = 9;                                          // bLength
    *p++ = TUSB_DESC_CS_INTERFACE;                     // bDescriptorType
    *p++ = AUDIO20_CS_AC_INTERFACE_HEADER;               // bDescriptorSubtype
    *p++ = 0x00; *p++ = 0x02;                          // bcdADC = 2.00 (UAC2)
    *p++ = AUDIO20_FUNC_DESKTOP_SPEAKER;                 // bCategory
    *p++ = (uint8_t)(UAC2_AC_CS_LEN);                  // wTotalLength (low)
    *p++ = (uint8_t)(UAC2_AC_CS_LEN >> 8);             // wTotalLength (high)
    *p++ = 0;                                          // bmControls

    // --- Clock Source (Entity 1) ---
    *p++ = 8;                                          // bLength
    *p++ = TUSB_DESC_CS_INTERFACE;                     // bDescriptorType
    *p++ = AUDIO20_CS_AC_INTERFACE_CLOCK_SOURCE;         // bDescriptorSubtype
    *p++ = UAC2_ENTITY_CLOCK;                          // bClockID
    *p++ = 0x01;                                       // bmAttributes: internal fixed clock
    *p++ = 0x05;                                       // bmControls: freq read-only, validity read-only
    *p++ = 0;                                          // bAssocTerminal
    *p++ = 0;                                          // iClockSource

    // --- Input Terminal (Entity 2, USB Streaming) ---
    *p++ = 17;                                         // bLength
    *p++ = TUSB_DESC_CS_INTERFACE;                     // bDescriptorType
    *p++ = AUDIO20_CS_AC_INTERFACE_INPUT_TERMINAL;       // bDescriptorSubtype
    *p++ = UAC2_ENTITY_INPUT_TERM;                     // bTerminalID
    *p++ = 0x01; *p++ = 0x01;                          // wTerminalType = USB Streaming (0x0101)
    *p++ = 0;                                          // bAssocTerminal
    *p++ = UAC2_ENTITY_CLOCK;                          // bCSourceID
    *p++ = USB_AUDIO_CHANNELS;                         // bNrChannels
    *p++ = 0x03; *p++ = 0x00; *p++ = 0x00; *p++ = 0x00; // bmChannelConfig = FL+FR
    *p++ = 0;                                          // iChannelNames
    *p++ = 0x00; *p++ = 0x00;                          // bmControls
    *p++ = 0;                                          // iTerminal

    // --- Feature Unit (Entity 3, Volume + Mute) ---
    // bLength = 6 + 4*(numChannels+1) = 6 + 4*3 = 18
    *p++ = 18;                                         // bLength
    *p++ = TUSB_DESC_CS_INTERFACE;                     // bDescriptorType
    *p++ = AUDIO20_CS_AC_INTERFACE_FEATURE_UNIT;         // bDescriptorSubtype
    *p++ = UAC2_ENTITY_FEATURE;                        // bUnitID
    *p++ = UAC2_ENTITY_INPUT_TERM;                     // bSourceID
    // bmControls per channel (4 bytes each): master, ch1, ch2
    // Master: mute + volume (bits 0-1 = mute r/w, bits 2-3 = volume r/w)
    *p++ = 0x0F; *p++ = 0x00; *p++ = 0x00; *p++ = 0x00;  // Master: mute(3) + volume(3)
    *p++ = 0x00; *p++ = 0x00; *p++ = 0x00; *p++ = 0x00;  // Ch1: none
    *p++ = 0x00; *p++ = 0x00; *p++ = 0x00; *p++ = 0x00;  // Ch2: none
    *p++ = 0;                                          // iFeature

    // --- Output Terminal (Entity 4, Speaker) ---
    *p++ = 12;                                         // bLength
    *p++ = TUSB_DESC_CS_INTERFACE;                     // bDescriptorType
    *p++ = AUDIO20_CS_AC_INTERFACE_OUTPUT_TERMINAL;      // bDescriptorSubtype
    *p++ = UAC2_ENTITY_OUTPUT_TERM;                    // bTerminalID
    *p++ = 0x01; *p++ = 0x03;                          // wTerminalType = Speaker (0x0301)
    *p++ = 0;                                          // bAssocTerminal
    *p++ = UAC2_ENTITY_FEATURE;                        // bSourceID
    *p++ = UAC2_ENTITY_CLOCK;                          // bCSourceID
    *p++ = 0x00; *p++ = 0x00;                          // bmControls
    *p++ = 0;                                          // iTerminal

    // --- Audio Streaming Interface Alt 0 (zero bandwidth) ---
    *p++ = 9;                                          // bLength
    *p++ = TUSB_DESC_INTERFACE;                        // bDescriptorType
    *p++ = as_itf;                                     // bInterfaceNumber
    *p++ = 0;                                          // bAlternateSetting
    *p++ = 0;                                          // bNumEndpoints (zero bandwidth)
    *p++ = TUSB_CLASS_AUDIO;                           // bInterfaceClass
    *p++ = AUDIO_SUBCLASS_STREAMING;                   // bInterfaceSubClass
    *p++ = AUDIO_INT_PROTOCOL_CODE_V2;                 // bInterfaceProtocol
    *p++ = 0;                                          // iInterface

    // --- Audio Streaming Interface Alt 1 (active) ---
    *p++ = 9;                                          // bLength
    *p++ = TUSB_DESC_INTERFACE;                        // bDescriptorType
    *p++ = as_itf;                                     // bInterfaceNumber
    *p++ = 1;                                          // bAlternateSetting
    *p++ = 1;                                          // bNumEndpoints
    *p++ = TUSB_CLASS_AUDIO;                           // bInterfaceClass
    *p++ = AUDIO_SUBCLASS_STREAMING;                   // bInterfaceSubClass
    *p++ = AUDIO_INT_PROTOCOL_CODE_V2;                 // bInterfaceProtocol
    *p++ = 0;                                          // iInterface

    // --- CS AS General (UAC2) ---
    *p++ = 16;                                         // bLength
    *p++ = TUSB_DESC_CS_INTERFACE;                     // bDescriptorType
    *p++ = AUDIO20_CS_AS_INTERFACE_AS_GENERAL;           // bDescriptorSubtype
    *p++ = UAC2_ENTITY_INPUT_TERM;                     // bTerminalLink
    *p++ = 0x00;                                       // bmControls
    *p++ = AUDIO20_FORMAT_TYPE_I;                        // bFormatType
    *p++ = 0x01; *p++ = 0x00; *p++ = 0x00; *p++ = 0x00; // bmFormats = PCM (bit 0)
    *p++ = USB_AUDIO_CHANNELS;                         // bNrChannels
    *p++ = 0x03; *p++ = 0x00; *p++ = 0x00; *p++ = 0x00; // bmChannelConfig = FL+FR
    *p++ = 0;                                          // iChannelNames

    // --- Type I Format Descriptor (UAC2) ---
    *p++ = 6;                                          // bLength
    *p++ = TUSB_DESC_CS_INTERFACE;                     // bDescriptorType
    *p++ = AUDIO20_CS_AS_INTERFACE_FORMAT_TYPE;          // bDescriptorSubtype
    *p++ = AUDIO20_FORMAT_TYPE_I;                        // bFormatType
    *p++ = USB_AUDIO_SUBSLOT_SIZE;                     // bSubslotSize (2 for 16-bit)
    *p++ = USB_AUDIO_BIT_DEPTH;                        // bBitResolution

    // --- Isochronous OUT Endpoint ---
    *p++ = 7;                                          // bLength
    *p++ = TUSB_DESC_ENDPOINT;                         // bDescriptorType
    *p++ = _epOut;                                     // bEndpointAddress (OUT)
    *p++ = 0x09;                                       // bmAttributes: Isochronous, Adaptive
    *p++ = (uint8_t)(USB_AUDIO_EP_SIZE);               // wMaxPacketSize (low)
    *p++ = (uint8_t)(USB_AUDIO_EP_SIZE >> 8);          // wMaxPacketSize (high)
    *p++ = 1;                                          // bInterval (1ms at Full Speed)

    // --- CS Endpoint (Audio Class) ---
    *p++ = 8;                                          // bLength
    *p++ = TUSB_DESC_CS_ENDPOINT;                      // bDescriptorType
    *p++ = AUDIO20_CS_EP_SUBTYPE_GENERAL;                // bDescriptorSubtype
    *p++ = 0x00;                                       // bmAttributes
    *p++ = 0x00;                                       // bmControls
    *p++ = 0x00;                                       // bLockDelayUnits
    *p++ = 0x00; *p++ = 0x00;                          // wLockDelay

    // Update interface counter (we used 2 interfaces: AC + AS)
    *itf = ac_itf + 2;

    uint16_t len = (uint16_t)(p - dst);
    return len;
}

// ===== Custom Audio Class Driver =====

static void _audio_driver_init(void) {
    _usbState = USB_AUDIO_DISCONNECTED;
    _altSetting = 0;
    _hostVolume = 0;
    _hostMute = false;
}

static void _audio_driver_reset(uint8_t rhport) {
    (void)rhport;
    _usbState = USB_AUDIO_DISCONNECTED;
    _altSetting = 0;
    _epDescSaved = false;
    usb_rb_reset(&_ringBuffer);

    AppState &as = AppState::getInstance();
    as.usbAudioConnected = false;
    as.usbAudioStreaming = false;
    as.markUsbAudioDirty();
}

static uint16_t _audio_driver_open(uint8_t rhport, tusb_desc_interface_t const *desc_intf, uint16_t max_len) {
    // Only handle Audio Control or Audio Streaming interfaces
    if (desc_intf->bInterfaceClass != TUSB_CLASS_AUDIO) return 0;

    uint16_t drv_len = 0;
    const uint8_t *p = (const uint8_t *)desc_intf;

    if (desc_intf->bInterfaceSubClass == AUDIO_SUBCLASS_CONTROL) {
        // Audio Control interface — skip AC + all CS descriptors
        drv_len = desc_intf->bLength; // Standard AC interface
        p += drv_len;

        // Walk CS descriptors until next standard interface or end
        while (drv_len < max_len) {
            if (p[0] == 0) break;
            uint8_t nextType = p[1];
            if (nextType == TUSB_DESC_INTERFACE || nextType == TUSB_DESC_INTERFACE_ASSOCIATION) break;
            drv_len += p[0];
            p += p[0];
        }

        _usbState = USB_AUDIO_CONNECTED;
        AppState::getInstance().usbAudioConnected = true;
        AppState::getInstance().markUsbAudioDirty();
        LOG_I("[USB Audio] AC interface opened, connected");

    } else if (desc_intf->bInterfaceSubClass == AUDIO_SUBCLASS_STREAMING) {
        if (desc_intf->bAlternateSetting == 0) {
            // Alt 0 (zero bandwidth): claim ALL remaining AS descriptors (alt 0 +
            // alt 1 + CS + endpoints) so TinyUSB knows the full extent during
            // initial config. Also called by process_set_interface to stop streaming.
            drv_len = desc_intf->bLength;
            p += drv_len;

            // Walk through all descriptors belonging to this interface number
            while (drv_len < max_len) {
                if (p[0] == 0) break;
                uint8_t nextType = p[1];
                if (nextType == TUSB_DESC_INTERFACE_ASSOCIATION) break;
                // Stop at a different interface number
                if (nextType == TUSB_DESC_INTERFACE) {
                    tusb_desc_interface_t const *next_itf = (tusb_desc_interface_t const *)p;
                    if (next_itf->bInterfaceNumber != desc_intf->bInterfaceNumber) break;
                }
                // Save endpoint descriptor for later iso_alloc/activate in SET_INTERFACE
                if (nextType == TUSB_DESC_ENDPOINT && !_epDescSaved) {
                    memcpy(&_savedEpDesc, p, sizeof(tusb_desc_endpoint_t));
                    _epDescSaved = true;
                }
                drv_len += p[0];
                p += p[0];
            }

            // Stop streaming (SET_INTERFACE alt 0 from host)
            if (_usbState == USB_AUDIO_STREAMING) {
                _usbState = USB_AUDIO_CONNECTED;
                _altSetting = 0;
                AppState::getInstance().usbAudioStreaming = false;
                AppState::getInstance().markUsbAudioDirty();
                LOG_I("[USB Audio] Streaming stopped (alt 0)");
            }

        } else {
            // TinyUSB does not call .open() for SET_INTERFACE on custom drivers.
            // Streaming is handled in control_xfer_cb. If we reach here, log it.
            LOG_W("[USB Audio] Unexpected .open() with alt %d", desc_intf->bAlternateSetting);
            drv_len = desc_intf->bLength;
        }
    }

    return drv_len;
}

static bool _audio_driver_control_xfer(uint8_t rhport, uint8_t stage, tusb_control_request_t const *request) {
    // Handle UAC2 control requests. TinyUSB routes BOTH standard SET_INTERFACE
    // and class-specific entity requests to this callback for custom drivers.
    // We handle SET_INTERFACE here for streaming state, and class-specific
    // requests for clock source and feature unit (volume/mute) below.

    if (stage == CONTROL_STAGE_SETUP) {
        // Handle standard SET_INTERFACE (alternate setting change for streaming)
        if (request->bmRequestType_bit.type == TUSB_REQ_TYPE_STANDARD &&
            request->bRequest == TUSB_REQ_SET_INTERFACE) {
            uint16_t altSetting = request->wValue;

            if (altSetting == 0) {
                // Host stopping streaming (alt 0 = zero bandwidth)
                if (_usbState == USB_AUDIO_STREAMING) {
                    _usbState = USB_AUDIO_CONNECTED;
                    _altSetting = 0;
                    usb_rb_reset(&_ringBuffer);
                    AppState::getInstance().usbAudioStreaming = false;
                    AppState::getInstance().markUsbAudioDirty();
                    LOG_I("[USB Audio] Streaming stopped (SET_INTERFACE alt 0)");
                }
            } else {
                // Host starting streaming (alt 1+)
                // Allocate and activate ISO endpoint (must happen before xfer)
                if (_epOut && _epDescSaved) {
                    usbd_edpt_iso_alloc(rhport, _epOut, USB_AUDIO_EP_SIZE);
                    usbd_edpt_iso_activate(rhport, &_savedEpDesc);
                }

                _altSetting = altSetting;
                _usbState = USB_AUDIO_STREAMING;
                _lastDataMs = millis();
                usb_rb_reset(&_ringBuffer);
                AppState::getInstance().usbAudioStreaming = true;
                AppState::getInstance().markUsbAudioDirty();
                LOG_I("[USB Audio] Streaming started (SET_INTERFACE alt %d)", altSetting);

                // Prime ISO endpoint for first transfer
                if (_epOut) {
                    usbd_edpt_xfer(rhport, _epOut, _isoOutBuf, USB_AUDIO_EP_SIZE, false);
                }
            }

            tud_control_status(rhport, request);
            return true;
        }

        uint8_t entity = TU_U16_HIGH(request->wIndex);

        // Clock Source entity requests
        if (entity == UAC2_ENTITY_CLOCK) {
            if (request->bRequest == AUDIO20_CS_REQ_CUR) {
                if (TU_U16_HIGH(request->wValue) == 0x01) {
                    // SAM_FREQ_CONTROL — CUR
                    if (request->bmRequestType_bit.direction == TUSB_DIR_IN) {
                        // GET: return current sample rate
                        uint32_t rate = USB_AUDIO_SAMPLE_RATE;
                        memcpy(_ctrlBuf, &rate, 4);
                        return tud_control_xfer(rhport, request, _ctrlBuf, 4);
                    } else {
                        // Accept SET_CUR — Windows always sends this during startup even for
                        // read-only clocks; stalling causes Code 10. Rate validated in DATA stage.
                        return tud_control_xfer(rhport, request, _ctrlBuf, 4);
                    }
                } else if (TU_U16_HIGH(request->wValue) == 0x02) {
                    // CLOCK_VALID_CONTROL — always valid
                    if (request->bmRequestType_bit.direction == TUSB_DIR_IN) {
                        _ctrlBuf[0] = 1; // valid
                        return tud_control_xfer(rhport, request, _ctrlBuf, 1);
                    }
                }
            } else if (request->bRequest == AUDIO20_CS_REQ_RANGE) {
                if (TU_U16_HIGH(request->wValue) == 0x01) {
                    // SAM_FREQ_CONTROL — RANGE: report single supported rate
                    // Layout: wNumSubRanges(2), dMIN(4), dMAX(4), dRES(4)
                    uint16_t numRanges = 1;
                    uint32_t rate = USB_AUDIO_SAMPLE_RATE;
                    memcpy(_ctrlBuf, &numRanges, 2);
                    memcpy(_ctrlBuf + 2, &rate, 4);  // dMIN
                    memcpy(_ctrlBuf + 6, &rate, 4);  // dMAX
                    uint32_t res = 0;
                    memcpy(_ctrlBuf + 10, &res, 4);  // dRES
                    return tud_control_xfer(rhport, request, _ctrlBuf, 14);
                }
            }
        }

        // Feature Unit entity requests
        if (entity == UAC2_ENTITY_FEATURE) {
            uint8_t control_sel = TU_U16_HIGH(request->wValue);
            uint8_t channel = TU_U16_LOW(request->wValue);
            (void)channel;

            if (control_sel == AUDIO20_FU_CTRL_MUTE) {
                if (request->bRequest == AUDIO20_CS_REQ_CUR) {
                    if (request->bmRequestType_bit.direction == TUSB_DIR_IN) {
                        // GET mute
                        _ctrlBuf[0] = _hostMute ? 1 : 0;
                        return tud_control_xfer(rhport, request, _ctrlBuf, 1);
                    } else {
                        // SET mute — receive data in DATA stage
                        return tud_control_xfer(rhport, request, _ctrlBuf, 1);
                    }
                } else if (request->bRequest == AUDIO20_CS_REQ_RANGE) {
                    // Mute is boolean — no range, return 0 sub-ranges
                    uint16_t numRanges = 0;
                    memcpy(_ctrlBuf, &numRanges, 2);
                    return tud_control_xfer(rhport, request, _ctrlBuf, 2);
                }
            } else if (control_sel == AUDIO20_FU_CTRL_VOLUME) {
                if (request->bRequest == AUDIO20_CS_REQ_CUR) {
                    if (request->bmRequestType_bit.direction == TUSB_DIR_IN) {
                        // GET volume
                        memcpy(_ctrlBuf, &_hostVolume, 2);
                        return tud_control_xfer(rhport, request, _ctrlBuf, 2);
                    } else {
                        // SET volume — receive data in DATA stage
                        return tud_control_xfer(rhport, request, _ctrlBuf, 2);
                    }
                } else if (request->bRequest == AUDIO20_CS_REQ_RANGE) {
                    // Volume range: -128 dB to 0 dB in 1/256 dB steps
                    uint16_t numRanges = 1;
                    int16_t volMin = -32512; // -124 dB (0x8100 — 0x8000 is UAC2 reserved sentinel)
                    int16_t volMax = 0;      // 0 dB
                    int16_t volRes = 256;    // 1 dB steps
                    memcpy(_ctrlBuf, &numRanges, 2);
                    memcpy(_ctrlBuf + 2, &volMin, 2);
                    memcpy(_ctrlBuf + 4, &volMax, 2);
                    memcpy(_ctrlBuf + 6, &volRes, 2);
                    return tud_control_xfer(rhport, request, _ctrlBuf, 8);
                }
            }
        }

        // Unhandled — stall
        LOG_W("[USB Audio] Unhandled control: bReq=0x%02X, wVal=0x%04X, wIdx=0x%04X",
              request->bRequest, request->wValue, request->wIndex);
        return false;
    }

    // DATA stage: process received control data
    if (stage == CONTROL_STAGE_DATA) {
        uint8_t entity = TU_U16_HIGH(request->wIndex);

        // Clock SET_CUR: accept if it matches our fixed rate, otherwise stall
        if (entity == UAC2_ENTITY_CLOCK) {
            uint8_t control_sel = TU_U16_HIGH(request->wValue);
            if (control_sel == 0x01) { // SAM_FREQ_CONTROL
                uint32_t requested_rate;
                memcpy(&requested_rate, _ctrlBuf, 4);
                if (requested_rate != USB_AUDIO_SAMPLE_RATE) {
                    LOG_W("[USB Audio] Host requested unsupported rate: %u", requested_rate);
                    // We only support one rate — host should use the rate from RANGE
                }
                LOG_I("[USB Audio] Host set clock rate: %u Hz", requested_rate);
            }
            return true;
        }

        if (entity == UAC2_ENTITY_FEATURE) {
            uint8_t control_sel = TU_U16_HIGH(request->wValue);
            if (control_sel == AUDIO20_FU_CTRL_MUTE) {
                _hostMute = (_ctrlBuf[0] != 0);
                AppState::getInstance().usbAudioMute = _hostMute;
                AppState::getInstance().markUsbAudioDirty();
                LOG_I("[USB Audio] Host mute: %s", _hostMute ? "ON" : "OFF");
            } else if (control_sel == AUDIO20_FU_CTRL_VOLUME) {
                memcpy(&_hostVolume, _ctrlBuf, 2);
                AppState::getInstance().usbAudioVolume = _hostVolume;
                AppState::getInstance().markUsbAudioDirty();
                LOG_I("[USB Audio] Host volume: %d (%.1f dB)",
                      _hostVolume, (float)_hostVolume / 256.0f);
            }
        }
        return true;
    }

    return true;
}

static bool _audio_driver_xfer_cb(uint8_t rhport, uint8_t ep_addr, xfer_result_t result, uint32_t xferred_bytes) {
    (void)result;

    if (ep_addr == _epOut && xferred_bytes > 0) {
        _lastDataMs = millis();
        // Convert received USB audio to I2S format and write to ring buffer
        uint32_t frames = xferred_bytes / (USB_AUDIO_CHANNELS * USB_AUDIO_SUBSLOT_SIZE);

        // Temp buffer for converted samples (on stack, max ~196 bytes * 2 = ~800 bytes)
        int32_t converted[USB_AUDIO_EP_SIZE / USB_AUDIO_SUBSLOT_SIZE];

        if (USB_AUDIO_BIT_DEPTH == 16) {
            usb_pcm16_to_int32((const int16_t *)_isoOutBuf, converted, frames);
        } else {
            usb_pcm24_to_int32(_isoOutBuf, converted, frames);
        }

        usb_rb_write(&_ringBuffer, converted, frames);

        // Re-arm the endpoint for next transfer
        usbd_edpt_xfer(rhport, _epOut, _isoOutBuf, USB_AUDIO_EP_SIZE, false);
    }

    return true;
}

// Class driver instance registered via usbd_app_driver_get_cb
// NOTE: .sof = NULL — no SOF callback needed. Avoids 1kHz interrupt overhead.
static usbd_class_driver_t const _audio_class_driver = {
#if CFG_TUSB_DEBUG >= 2
    .name = "AUDIO",
#endif
    .init = _audio_driver_init,
    .reset = _audio_driver_reset,
    .open = _audio_driver_open,
    .control_xfer_cb = _audio_driver_control_xfer,
    .xfer_cb = _audio_driver_xfer_cb,
    .sof = NULL,
};

// Register our audio driver with TinyUSB's USBD stack
extern "C" usbd_class_driver_t const *usbd_app_driver_get_cb(uint8_t *driver_count) {
    *driver_count = 1;
    return &_audio_class_driver;
}

// Override the framework's BOS descriptor callback via --wrap linker flag.
// The Arduino ESP32 TinyUSB HAL defines tud_descriptor_bos_cb() as a STRONG
// symbol (not __attribute__((weak))), so a normal override in user code is
// silently discarded by the linker. Using -Wl,--wrap=tud_descriptor_bos_cb
// in platformio.ini guarantees our version is called.
//
// The framework's BOS includes Microsoft OS 2.0 with Compatible ID "WINUSB",
// causing Windows to load WinUSB instead of usbaudio2.sys. Our minimal BOS
// has only the USB 2.0 Extension capability (required since bcdUSB=0x0210).
static uint8_t const _minimal_bos_descriptor[] = {
    // BOS Descriptor Header (5 bytes)
    5,                          // bLength
    0x0F,                       // bDescriptorType = BOS
    12, 0,                      // wTotalLength = 12
    1,                          // bNumDeviceCaps = 1

    // USB 2.0 Extension Capability (7 bytes)
    7,                          // bLength
    0x10,                       // bDescriptorType = DEVICE_CAPABILITY
    0x02,                       // bDevCapabilityType = USB_2_0_EXTENSION
    0x00, 0x00, 0x00, 0x00     // bmAttributes: no LPM (ESP32-S3 FS doesn't support it reliably)
};

extern "C" uint8_t const *__wrap_tud_descriptor_bos_cb(void) {
    LOG_I("[USB Audio] BOS descriptor served (minimal, no MSOS2)");
    return _minimal_bos_descriptor;
}

// NOTE: No separate FreeRTOS task needed — tinyusb_init() creates its own
// "usbd" task at configMAX_PRIORITIES-1 that calls tud_task() in a loop.
// Creating a second task calling tud_task_ext() causes concurrent access
// (undefined behavior) and the usbd task's max priority starves the main loop.

// ===== Public API Implementation =====

void usb_audio_init(void) {
    // Allocate ring buffer in PSRAM (once — persists across enable/disable toggles)
    if (!_ringBufStorage) {
        _ringBufStorage = (int32_t *)heap_caps_calloc(
            RING_BUF_CAPACITY * 2, sizeof(int32_t), MALLOC_CAP_SPIRAM);
        if (!_ringBufStorage) {
            _ringBufStorage = (int32_t *)calloc(RING_BUF_CAPACITY * 2, sizeof(int32_t));
        }
        if (!_ringBufStorage) {
            LOG_E("[USB Audio] Ring buffer allocation failed!");
            return;
        }
        usb_rb_init(&_ringBuffer, _ringBufStorage, RING_BUF_CAPACITY);
    }

    // Initialize TinyUSB hardware (one-shot — can't re-init after first call).
    // tinyusb_enable_interface() must be called before tinyusb_init(), and both
    // fail if called a second time. The enable/disable toggle only controls the
    // software data path; the USB device stays enumerated once started.
    if (!_tinyusbHwReady) {
        // Register audio interface descriptor with the Arduino TinyUSB framework
        esp_err_t err = tinyusb_enable_interface(
            USB_INTERFACE_CUSTOM,
            UAC2_DESC_TOTAL_LEN,
            usb_audio_descriptor_cb
        );
        if (err != ESP_OK) {
            LOG_E("[USB Audio] Failed to register USB interface: %d", err);
            return;
        }

        // Initialize TinyUSB with our device config (don't use TINYUSB_CONFIG_DEFAULT
        // — it references Kconfig macros not available in Arduino framework builds)
        tinyusb_device_config_t cfg = {};
        cfg.vid = USB_ESPRESSIF_VID;
        cfg.pid = 0x4002;
        cfg.product_name = "ALX Nova Audio";
        cfg.manufacturer_name = "ALX Audio";
        cfg.serial_number = "ALX-USB-AUDIO";
        cfg.fw_version = 0x0100;
        cfg.usb_version = 0x0200;
        cfg.usb_class = TUSB_CLASS_MISC;
        cfg.usb_subclass = MISC_SUBCLASS_COMMON;
        cfg.usb_protocol = MISC_PROTOCOL_IAD;
        cfg.usb_attributes = TUSB_DESC_CONFIG_ATT_SELF_POWERED;
        cfg.usb_power_ma = 100;
        cfg.webusb_enabled = false;
        cfg.webusb_url = "";
        err = tinyusb_init(&cfg);
        if (err != ESP_OK) {
            LOG_E("[USB Audio] TinyUSB init failed: %d", err);
            return;
        }
        // tinyusb_init() creates its own "usbd" task (max priority, no core affinity)
        // that calls tud_task() in a loop. No additional task needed.
        _tinyusbHwReady = true;

        LOG_I("[USB Audio] TinyUSB started: %dHz/%dbit/%dch, ring=%d frames (%s)",
              USB_AUDIO_SAMPLE_RATE, USB_AUDIO_BIT_DEPTH, USB_AUDIO_CHANNELS,
              RING_BUF_CAPACITY,
              _ringBufStorage ? "PSRAM" : "internal");
    } else {
        LOG_I("[USB Audio] Re-enabled (TinyUSB already running)");
    }

    // Reset ring buffer for clean start on each enable
    usb_rb_reset(&_ringBuffer);

    // Set initial AppState
    AppState &as = AppState::getInstance();
    as.usbAudioSampleRate = USB_AUDIO_SAMPLE_RATE;
    as.usbAudioBitDepth = USB_AUDIO_BIT_DEPTH;
    as.usbAudioChannels = USB_AUDIO_CHANNELS;

    // Connect to USB bus. On first enable, tinyusb_init() already called this,
    // but it's idempotent. On re-enable after deinit(), this triggers re-enumeration.
    tud_connect();
    LOG_I("[USB Audio] Connected to USB bus");
}

void usb_audio_deinit(void) {
    // Disconnect from USB bus to trigger host-side device removal.
    // TinyUSB continues running but device is removed from enumeration.
    // Re-enable will call tud_connect() to re-enumerate as new device.
    _usbState = USB_AUDIO_DISCONNECTED;
    AppState &as = AppState::getInstance();
    as.usbAudioConnected = false;
    as.usbAudioStreaming = false;
    as.markUsbAudioDirty();

    // Tell host device is unplugged (triggers re-enumeration on next enable)
    tud_disconnect();
    LOG_I("[USB Audio] Disconnected from USB bus");
}

UsbAudioState usb_audio_get_state(void) {
    return _usbState;
}

bool usb_audio_is_connected(void) {
    return _usbState >= USB_AUDIO_CONNECTED;
}

bool usb_audio_is_streaming(void) {
    return _usbState == USB_AUDIO_STREAMING;
}

uint32_t usb_audio_read(int32_t *out, uint32_t frames) {
    if (_usbState != USB_AUDIO_STREAMING) return 0;
    return usb_rb_read(&_ringBuffer, out, frames);
}

uint32_t usb_audio_available_frames(void) {
    return usb_rb_available(&_ringBuffer);
}

uint32_t usb_audio_get_sample_rate(void) {
    return USB_AUDIO_SAMPLE_RATE;
}

uint8_t usb_audio_get_bit_depth(void) {
    return USB_AUDIO_BIT_DEPTH;
}

uint8_t usb_audio_get_channels(void) {
    return USB_AUDIO_CHANNELS;
}

int16_t usb_audio_get_volume(void) {
    return _hostVolume;
}

bool usb_audio_get_mute(void) {
    return _hostMute;
}

float usb_audio_get_volume_linear(void) {
    return usb_volume_to_linear(_hostVolume);
}

uint32_t usb_audio_get_overruns(void) {
    return _ringBuffer.overruns;
}

uint32_t usb_audio_get_underruns(void) {
    return _ringBuffer.underruns;
}

float usb_audio_get_buffer_fill(void) {
    return usb_rb_fill_level(&_ringBuffer);
}

bool usb_audio_check_timeout(void) {
    if (_usbState == USB_AUDIO_STREAMING && _lastDataMs > 0) {
        if (millis() - _lastDataMs > USB_STREAM_TIMEOUT_MS) {
            _usbState = USB_AUDIO_CONNECTED;
            _altSetting = 0;
            AppState::getInstance().usbAudioStreaming = false;
            AppState::getInstance().markUsbAudioDirty();
            LOG_I("[USB Audio] Streaming timed out (no data for %dms)", USB_STREAM_TIMEOUT_MS);
            return true;
        }
    }
    return false;
}

#elif defined(NATIVE_TEST)

// ===== Native Test Stubs =====
static UsbAudioState _nativeState = USB_AUDIO_DISCONNECTED;

void usb_audio_init(void) {}
void usb_audio_deinit(void) {}

UsbAudioState usb_audio_get_state(void) { return _nativeState; }

bool usb_audio_is_connected(void) { return _nativeState >= USB_AUDIO_CONNECTED; }
bool usb_audio_is_streaming(void) { return _nativeState == USB_AUDIO_STREAMING; }

uint32_t usb_audio_read(int32_t *out, uint32_t frames) {
    (void)out; (void)frames;
    return 0;
}
uint32_t usb_audio_available_frames(void) { return 0; }
uint32_t usb_audio_get_sample_rate(void) { return 48000; }
uint8_t usb_audio_get_bit_depth(void) { return 16; }
uint8_t usb_audio_get_channels(void) { return 2; }
int16_t usb_audio_get_volume(void) { return 0; }
bool usb_audio_get_mute(void) { return false; }
float usb_audio_get_volume_linear(void) { return 1.0f; }
uint32_t usb_audio_get_overruns(void) { return 0; }
uint32_t usb_audio_get_underruns(void) { return 0; }
float usb_audio_get_buffer_fill(void) { return 0.0f; }
bool usb_audio_check_timeout(void) { return false; }

#else
// USB_AUDIO_ENABLED not defined and not NATIVE_TEST — empty stubs
void usb_audio_init(void) {}
void usb_audio_deinit(void) {}
UsbAudioState usb_audio_get_state(void) { return USB_AUDIO_DISCONNECTED; }
bool usb_audio_is_connected(void) { return false; }
bool usb_audio_is_streaming(void) { return false; }
uint32_t usb_audio_read(int32_t *out, uint32_t frames) { (void)out; (void)frames; return 0; }
uint32_t usb_audio_available_frames(void) { return 0; }
uint32_t usb_audio_get_sample_rate(void) { return 48000; }
uint8_t usb_audio_get_bit_depth(void) { return 16; }
uint8_t usb_audio_get_channels(void) { return 2; }
int16_t usb_audio_get_volume(void) { return 0; }
bool usb_audio_get_mute(void) { return false; }
float usb_audio_get_volume_linear(void) { return 1.0f; }
uint32_t usb_audio_get_overruns(void) { return 0; }
uint32_t usb_audio_get_underruns(void) { return 0; }
float usb_audio_get_buffer_fill(void) { return 0.0f; }
bool usb_audio_check_timeout(void) { return false; }
#endif
