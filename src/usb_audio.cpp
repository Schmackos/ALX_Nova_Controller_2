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
#define USB_AUDIO_CHANNELS      2

// Per-alt max endpoint packet sizes at highest rate (96kHz)
// Alt 2 (24-bit) removed — PCM16 only simplifies Windows usbaudio2.sys matching
#define USB_AUDIO_MAX_EP_SIZE_16BIT  ((96000/1000+1) * 2 * 2)  // = 388 bytes
#define USB_AUDIO_MAX_EP_SIZE        USB_AUDIO_MAX_EP_SIZE_16BIT

// Runtime negotiated format (defaults; updated by host SET_CUR / SET_INTERFACE)
static uint32_t _negotiatedRate  = 48000;
static uint8_t  _negotiatedDepth = 16;

// UAC2 Entity IDs
#define UAC2_ENTITY_CLOCK       0x01
#define UAC2_ENTITY_INPUT_TERM  0x02
#define UAC2_ENTITY_FEATURE     0x03
#define UAC2_ENTITY_OUTPUT_TERM 0x04

// ===== Module State =====
static UsbAudioConnectionState _usbState = USB_AUDIO_DISCONNECTED;
static UsbAudioRingBuffer _ringBuffer = {};
static int32_t *_ringBufStorage = nullptr;
static uint8_t _epOut = 0;
static uint8_t _itfNum = 0;          // Assigned by framework
static int16_t _hostVolume = 0;      // 1/256 dB
static bool _hostMute = false;
static uint8_t _altSetting = 0;      // Current AS alt setting (0=idle, 1=streaming)
static bool _tinyusbHwReady = false; // True after tinyusb_init() succeeds (one-shot)

// Control request response buffer
static uint8_t _ctrlBuf[64];

// Buffer for receiving isochronous OUT data (must be declared before control_xfer which primes it)
// P4 DMA requires 64-byte alignment (UTMI PHY); S3 needs only 4-byte
#if CONFIG_IDF_TARGET_ESP32P4
static uint8_t _isoOutBuf[USB_AUDIO_MAX_EP_SIZE] __attribute__((aligned(64)));
#else
static uint8_t _isoOutBuf[USB_AUDIO_MAX_EP_SIZE] __attribute__((aligned(4)));
#endif

// Ring buffer capacity: 4096 frames = 42.7ms at 96kHz. PSRAM cost: 4096 × 2ch × 4 bytes = 32KB.
#define RING_BUF_CAPACITY 4096

// Static conversion buffer — avoids stack overflow in USBD task at 96kHz/24-bit
static int32_t _convBuf[USB_AUDIO_MAX_EP_SIZE / 2];

// ===== UAC2 Descriptor Builder =====

// Total descriptor length (computed from structure)
// IAD(8) + AC std(9) + AC CS(64) + AS Alt0(9) + AS Alt1(9) + CS AS General(16) + Format(6) + Endpoint(7) + CS Endpoint(8) = 136
// Alt 2 (24-bit, 46 bytes) removed to simplify Windows usbaudio2.sys matching on Full-Speed USB
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
    *p++ = 0x03;                                       // bmAttributes: internal programmable clock
    *p++ = 0x07;                                       // bmControls: freq r/w (bits 1:0=11), validity r-only (bits 3:2=01)
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

    // --- Type I Format Descriptor (UAC2) --- Alt 1 (16-bit)
    *p++ = 6;                                          // bLength
    *p++ = TUSB_DESC_CS_INTERFACE;                     // bDescriptorType
    *p++ = AUDIO20_CS_AS_INTERFACE_FORMAT_TYPE;          // bDescriptorSubtype
    *p++ = AUDIO20_FORMAT_TYPE_I;                        // bFormatType
    *p++ = 2;                                          // bSubslotSize (2 for 16-bit)
    *p++ = 16;                                         // bBitResolution

    // --- Isochronous OUT Endpoint --- Alt 1 (16-bit)
    *p++ = 7;                                          // bLength
    *p++ = TUSB_DESC_ENDPOINT;                         // bDescriptorType
    *p++ = _epOut;                                     // bEndpointAddress (OUT)
    *p++ = 0x09;                                       // bmAttributes: Isochronous, Adaptive
    *p++ = (uint8_t)(USB_AUDIO_MAX_EP_SIZE_16BIT);     // wMaxPacketSize (low) = 388
    *p++ = (uint8_t)(USB_AUDIO_MAX_EP_SIZE_16BIT >> 8);// wMaxPacketSize (high)
#if CONFIG_IDF_TARGET_ESP32P4
    *p++ = 4;                                          // bInterval: HS: 2^(4-1)=8 microframes = 1ms
#else
    *p++ = 1;                                          // bInterval: FS: every SOF = 1ms
#endif

    // --- CS Endpoint (Audio Class) --- Alt 1
    *p++ = 8;                                          // bLength
    *p++ = TUSB_DESC_CS_ENDPOINT;                      // bDescriptorType
    *p++ = AUDIO20_CS_EP_SUBTYPE_GENERAL;                // bDescriptorSubtype
    *p++ = 0x00;                                       // bmAttributes
    *p++ = 0x00;                                       // bmControls
    *p++ = 0x00;                                       // bLockDelayUnits
    *p++ = 0x00; *p++ = 0x00;                          // wLockDelay

    // Alt 2 (24-bit PCM) intentionally omitted.
    // Dual alternate settings on Full-Speed USB confuses Windows usbaudio2.sys.
    // PCM16 only (Alt 1) is the correct approach for FS UAC2 speaker devices.

    // Update interface counter (we used 2 interfaces: AC + AS)
    *itf = ac_itf + 2;

    uint16_t len = (uint16_t)(p - dst);

    // Runtime sanity check: catch descriptor length drift.
    // Use ets_printf (not LOG macros) — this runs in USB descriptor context
    // where LOG macros may not be safe. A mismatch here is a critical error
    // that would prevent Windows from recognizing the device.
    if (len != UAC2_DESC_TOTAL_LEN) {
        ets_printf("[USB Audio] CRITICAL: Descriptor length mismatch! actual=%u expected=%u\n",
                   len, UAC2_DESC_TOTAL_LEN);
    }

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
    LOG_I("[USB Audio] Bus reset");
    _usbState = USB_AUDIO_DISCONNECTED;
    _altSetting = 0;
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
            // Alt 1+ (active streaming) — called by TinyUSB's process_set_interface
            // when host sends SET_INTERFACE to start streaming.
            drv_len = desc_intf->bLength;
            p += drv_len;

            while (drv_len < max_len) {
                if (p[0] == 0) break;
                uint8_t nextType = p[1];
                if (nextType == TUSB_DESC_INTERFACE || nextType == TUSB_DESC_INTERFACE_ASSOCIATION) break;

                // Open isochronous endpoint
                if (nextType == TUSB_DESC_ENDPOINT) {
                    tusb_desc_endpoint_t const *ep = (tusb_desc_endpoint_t const *)p;
                    usbd_edpt_iso_alloc(rhport, ep->bEndpointAddress, ep->wMaxPacketSize);
                    usbd_edpt_iso_activate(rhport, ep);
                }

                drv_len += p[0];
                p += p[0];
            }

            // Start streaming — Alt 1 only (PCM16; Alt 2 / 24-bit removed)
            _altSetting = desc_intf->bAlternateSetting;
            _negotiatedDepth = 16;  // PCM16 only; Alt 2 (24-bit) no longer advertised
            AppState::getInstance().usbAudioNegotiatedDepth = _negotiatedDepth;
            AppState::getInstance().usbAudioBitDepth = _negotiatedDepth;
            _usbState = USB_AUDIO_STREAMING;
            usb_rb_reset(&_ringBuffer);
            AppState::getInstance().usbAudioStreaming = true;
            AppState::getInstance().markUsbAudioDirty();
            LOG_I("[USB Audio] Alt setting %d selected (%u-bit)", _altSetting, _negotiatedDepth);
            LOG_I("[USB Audio] Streaming started (alt %d)", desc_intf->bAlternateSetting);

            // Prime the OUT endpoint for first isochronous transfer with dynamic size
            if (_epOut) {
                uint32_t ep_size = (_negotiatedRate / 1000 + 1) * 2 * (_negotiatedDepth / 8);
                usbd_edpt_xfer(rhport, _epOut, _isoOutBuf, ep_size, false);
            }
        }
    }

    return drv_len;
}

static bool _audio_driver_control_xfer(uint8_t rhport, uint8_t stage, tusb_control_request_t const *request) {
    // Handle UAC2 class-specific control requests for clock and feature unit.
    // NOTE: Standard SET_INTERFACE is handled by TinyUSB's USBD internally —
    // it calls _audio_driver_open() for the new alt setting (not this callback).

    // Log every control transfer for debugging (helps diagnose Windows Code 10)
    LOG_D("[USB Audio] ctrl: stage=%d bmRT=0x%02X bReq=0x%02X wVal=0x%04X wIdx=0x%04X wLen=%u",
          stage, request->bmRequestType, request->bRequest,
          request->wValue, request->wIndex, request->wLength);

    if (stage == CONTROL_STAGE_SETUP) {
        // Non-class requests (standard SET_INTERFACE etc.) may be forwarded by
        // TinyUSB to class drivers. STALLing them corrupts the EP0 state machine
        // and breaks subsequent control transfers (garbled data in _ctrlBuf).
        if (request->bmRequestType_bit.type != TUSB_REQ_TYPE_CLASS) {
            // Let TinyUSB's standard request handler process SET_INTERFACE,
            // GET_INTERFACE, and other standard requests. Returning true here
            // would prevent TinyUSB from calling process_set_interface() →
            // _audio_driver_open() — breaking alt setting changes (Code 10).
            return false;
        }

        uint8_t entity = TU_U16_HIGH(request->wIndex);

        // Clock Source entity requests
        if (entity == UAC2_ENTITY_CLOCK) {
            if (request->bRequest == AUDIO20_CS_REQ_CUR) {
                if (TU_U16_HIGH(request->wValue) == 0x01) {
                    // SAM_FREQ_CONTROL — CUR
                    if (request->bmRequestType_bit.direction == TUSB_DIR_IN) {
                        // GET: return current negotiated sample rate
                        uint32_t rate = _negotiatedRate;
                        memcpy(_ctrlBuf, &rate, 4);
                        return tud_control_xfer(rhport, request, _ctrlBuf, 4);
                    } else {
                        // SET: accept data stage to receive requested rate
                        memset(_ctrlBuf, 0, 4); // Clear stale data before DMA
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
                    // SAM_FREQ_CONTROL — RANGE: report 3 supported rates
                    // Layout: wNumSubRanges(2) + 3 × (dMIN(4) + dMAX(4) + dRES(4)) = 38 bytes
                    uint16_t numRanges = 3;
                    uint32_t rates[] = {44100, 48000, 96000};
                    uint32_t zero = 0;
                    memcpy(_ctrlBuf, &numRanges, 2);
                    for (int i = 0; i < 3; i++) {
                        memcpy(_ctrlBuf + 2 + i * 12, &rates[i], 4);     // dMIN
                        memcpy(_ctrlBuf + 2 + i * 12 + 4, &rates[i], 4); // dMAX
                        memcpy(_ctrlBuf + 2 + i * 12 + 8, &zero, 4);     // dRES
                    }
                    return tud_control_xfer(rhport, request, _ctrlBuf, 38);
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
                        memset(_ctrlBuf, 0, 1);
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
                        memset(_ctrlBuf, 0, 2);
                        return tud_control_xfer(rhport, request, _ctrlBuf, 2);
                    }
                } else if (request->bRequest == AUDIO20_CS_REQ_RANGE) {
                    // Volume range: -128 dB to 0 dB in 1/256 dB steps
                    uint16_t numRanges = 1;
                    int16_t volMin = -32767; // -128 dB (0x8000 is reserved as "silence" in UAC2)
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

        // Input Terminal entity requests (Connector Control)
        if (entity == UAC2_ENTITY_INPUT_TERM) {
            uint8_t control_sel = TU_U16_HIGH(request->wValue);
            if (control_sel == 0x02) { // TE_CONNECTOR_CONTROL
                if (request->bRequest == AUDIO20_CS_REQ_CUR &&
                    request->bmRequestType_bit.direction == TUSB_DIR_IN) {
                    // GET CUR: USB streaming terminal is always "connected"
                    _ctrlBuf[0] = 1;
                    return tud_control_xfer(rhport, request, _ctrlBuf, 1);
                }
            }
            // Unknown terminal control — STALL
            return false;
        }

        // Output Terminal entity requests (Connector Control)
        if (entity == UAC2_ENTITY_OUTPUT_TERM) {
            uint8_t control_sel = TU_U16_HIGH(request->wValue);
            if (control_sel == 0x02) { // TE_CONNECTOR_CONTROL
                if (request->bRequest == AUDIO20_CS_REQ_CUR &&
                    request->bmRequestType_bit.direction == TUSB_DIR_IN) {
                    // GET CUR: speaker output is always "connected"
                    _ctrlBuf[0] = 1;
                    return tud_control_xfer(rhport, request, _ctrlBuf, 1);
                }
            }
            // Unknown terminal control — STALL
            return false;
        }

        // Unhandled class request — always STALL (returning false).
        // ACK-ing unknown requests confuses the host driver.
        LOG_W("[USB Audio] Unhandled class control: entity=0x%02X bReq=0x%02X wVal=0x%04X wIdx=0x%04X wLen=%u",
              entity, request->bRequest, request->wValue, request->wIndex, request->wLength);
        return false;
    }

    // DATA stage: process received control data
    if (stage == CONTROL_STAGE_DATA) {
        uint8_t entity = TU_U16_HIGH(request->wIndex);

        // Clock SET_CUR: validate and store negotiated rate
        if (entity == UAC2_ENTITY_CLOCK) {
            uint8_t control_sel = TU_U16_HIGH(request->wValue);
            if (control_sel == 0x01) { // SAM_FREQ_CONTROL
                uint32_t requested_rate;
                memcpy(&requested_rate, _ctrlBuf, 4);
                LOG_D("[USB Audio] Clock SET_CUR raw: [%02X %02X %02X %02X] = %u Hz",
                      _ctrlBuf[0], _ctrlBuf[1], _ctrlBuf[2], _ctrlBuf[3], requested_rate);
                if (requested_rate != 44100 && requested_rate != 48000 && requested_rate != 96000) {
                    LOG_W("[USB Audio] Host requested unsupported rate: %u", requested_rate);
                    return false; // STALL
                }
                _negotiatedRate = requested_rate;
                AppState::getInstance().usbAudioNegotiatedRate = requested_rate;
                AppState::getInstance().usbAudioSampleRate = requested_rate;
                AppState::getInstance().markUsbAudioDirty();
                LOG_I("[USB Audio] Host set clock rate: %u Hz", requested_rate);
                LOG_I("[USB Audio] Host negotiated rate: %u Hz", _negotiatedRate);
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
        // PCM16 only — Alt 2 (24-bit) no longer advertised in descriptor
        uint32_t frames = xferred_bytes / (USB_AUDIO_CHANNELS * 2);
        usb_pcm16_to_int32((const int16_t *)_isoOutBuf, _convBuf, frames);
        usb_rb_write(&_ringBuffer, _convBuf, frames);

        // Re-arm for next packet (PCM16: 2 bytes per sample)
        uint32_t ep_size = (_negotiatedRate / 1000 + 1) * USB_AUDIO_CHANNELS * 2;
        usbd_edpt_xfer(rhport, _epOut, _isoOutBuf, ep_size, false);
    }

    return true;
}

// Class driver instance registered via usbd_app_driver_get_cb
// NOTE: .sof = NULL — no SOF callback needed. Avoids 1kHz interrupt overhead.
static usbd_class_driver_t const _audio_class_driver = {
#if CFG_TUSB_DEBUG >= 2
    .name = "AUDIO",
#endif
    .init            = _audio_driver_init,
    .deinit          = NULL,               // TinyUSB 0.20.1: new field — no teardown needed
    .reset           = _audio_driver_reset,
    .open            = _audio_driver_open,
    .control_xfer_cb = _audio_driver_control_xfer,
    .xfer_cb         = _audio_driver_xfer_cb,
    .xfer_isr        = NULL,               // TinyUSB 0.20.1: new field — defer ISR transfers to xfer_cb
    .sof             = NULL,               // sof signature now (rhport, frame_count) — NULL safe
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
    0x00, 0x00, 0x00, 0x00     // bmAttributes: no LPM (ESP32-S3 FS / ESP32-P4 HS — not needed for UAC2 audio class)
};

extern "C" uint8_t const *__wrap_tud_descriptor_bos_cb(void) {
    static uint32_t callCount = 0;
    callCount++;
    if (callCount <= 3) {  // only log first few to avoid spam
        LOG_I("[USB Audio] BOS descriptor served #%u (minimal, no MSOS2)", callCount);
    }
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
        cfg.pid = 0x4004;  // Bumped from 0x4003 to break stale Windows driver cache (Alt2 removal)
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
        LOG_I("[USB Audio] USB init OK: VID=0x%04X PID=0x%04X desc=%u bytes",
              USB_ESPRESSIF_VID, 0x4004, UAC2_DESC_TOTAL_LEN);

        // Diagnostic: verify TinyUSB is actually running
        LOG_I("[USB Audio] tud_inited=%d tud_connected=%d tud_mounted=%d",
              tud_inited(), tud_connected(), tud_mounted());

        // Force a clean USB enumeration cycle:
        // The DWC2 pull-up may activate during tinyusb_init() before the USB
        // task is fully running. Windows starts enumeration immediately and
        // fails with "Device Descriptor Request Failed" if TinyUSB can't
        // respond in time. Disconnect briefly, let the task stabilize, then
        // reconnect so the host sees a fresh device attachment.
        tud_disconnect();
        vTaskDelay(pdMS_TO_TICKS(200));
        tud_connect();
        LOG_I("[USB Audio] Soft disconnect/reconnect done — host will re-enumerate");

        // Lower the TinyUSB usbd task priority from configMAX_PRIORITIES-1
        // to USB_AUDIO_TASK_PRIORITY (1). The hard-coded max priority causes
        // the usbd task to preempt the audio pipeline (priority 3) on Core 1,
        // resulting in I2S DMA timing violations and audible pops.
        TaskHandle_t usbd_task = xTaskGetHandle("usbd");
        if (usbd_task) {
            vTaskPrioritySet(usbd_task, USB_AUDIO_TASK_PRIORITY);
            LOG_I("[USB Audio] Lowered usbd task priority to %d (was %d)",
                  USB_AUDIO_TASK_PRIORITY, configMAX_PRIORITIES - 1);
        } else {
            LOG_W("[USB Audio] Could not find usbd task to lower priority");
        }

        LOG_I("[USB Audio] TinyUSB started: %uHz/%ubit/%dch, ring=%d frames (%s)",
              _negotiatedRate, _negotiatedDepth, USB_AUDIO_CHANNELS,
              RING_BUF_CAPACITY,
              _ringBufStorage ? "PSRAM" : "internal");
    } else {
        LOG_I("[USB Audio] Re-enabled (TinyUSB already running)");
        // Sync state with actual USB connection (host may still be attached)
        if (tud_mounted()) {
            _usbState = USB_AUDIO_CONNECTED;
        }
    }

    // Reset ring buffer for clean start on each enable
    usb_rb_reset(&_ringBuffer);

    // Set initial AppState
    AppState &as = AppState::getInstance();
    as.usbAudioSampleRate = _negotiatedRate;
    as.usbAudioBitDepth = _negotiatedDepth;
    as.usbAudioChannels = USB_AUDIO_CHANNELS;
    as.usbAudioConnected = (_usbState >= USB_AUDIO_CONNECTED);
    as.usbAudioStreaming = (_usbState == USB_AUDIO_STREAMING);
    as.markUsbAudioDirty();
}

void usb_audio_deinit(void) {
    // Soft disable only — TinyUSB keeps running (can't be torn down at runtime).
    // The USB device remains enumerated on the host; we just stop processing data.
    // _tinyusbHwReady is NOT cleared so re-enable skips hardware init.
    _usbState = USB_AUDIO_DISCONNECTED;
    AppState &as = AppState::getInstance();
    as.usbAudioConnected = false;
    as.usbAudioStreaming = false;
    as.markUsbAudioDirty();
    LOG_I("[USB Audio] Disabled (USB device still enumerated)");
}

UsbAudioConnectionState usb_audio_get_state(void) {
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
    return _negotiatedRate;
}

uint8_t usb_audio_get_bit_depth(void) {
    return _negotiatedDepth;
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

uint32_t usb_audio_get_negotiated_rate(void) {
    return _negotiatedRate;
}

uint8_t usb_audio_get_negotiated_depth(void) {
    return _negotiatedDepth;
}

void usb_audio_poll_connection(void) {
    if (!_tinyusbHwReady) return;

    // Log mount state transitions (before connection logic updates _usbState)
    static bool _prevMountState = false;
    static bool _prevConnectState = false;
    bool curMounted = tud_mounted();
    bool curConnected = tud_connected();
    if (curMounted != _prevMountState || curConnected != _prevConnectState) {
        LOG_I("[USB Audio] State: mounted=%d->%d connected=%d->%d",
              _prevMountState, curMounted, _prevConnectState, curConnected);
        _prevMountState = curMounted;
        _prevConnectState = curConnected;
    }

    bool mounted = curMounted;
    AppState &as = AppState::getInstance();

    if (mounted && _usbState == USB_AUDIO_DISCONNECTED) {
        // Host is connected but our state says disconnected — sync up.
        // Happens after re-enable (deinit→init) while cable stays plugged in.
        _usbState = USB_AUDIO_CONNECTED;
        as.usbAudioConnected = true;
        as.usbAudioStreaming = false;
        as.markUsbAudioDirty();
        LOG_I("[USB Audio] Connection detected (mounted)");
    } else if (!mounted && _usbState >= USB_AUDIO_CONNECTED) {
        // Host disconnected (cable pulled, or host powered off).
        // tud_mounted() returns false when no SOF received for >3ms.
        _usbState = USB_AUDIO_DISCONNECTED;
        _altSetting = 0;
        usb_rb_reset(&_ringBuffer);
        as.usbAudioConnected = false;
        as.usbAudioStreaming = false;
        as.markUsbAudioDirty();
        LOG_I("[USB Audio] Disconnection detected (unmounted)");
    }
}

#elif defined(NATIVE_TEST)

// ===== Native Test Stubs with Injectable State =====
static UsbAudioConnectionState _nativeState = USB_AUDIO_DISCONNECTED;
static uint32_t _nativeRate  = 48000;
static uint8_t  _nativeDepth = 16;
static int16_t  _nativeVolume = 0;
static bool     _nativeMute = false;
static UsbAudioRingBuffer _nativeRingBuffer = {};
static int32_t _nativeRingBufStorage[256 * 2]; // Small buffer for read tests

void usb_audio_init(void) {
    usb_rb_init(&_nativeRingBuffer, _nativeRingBufStorage, 256);
}
void usb_audio_deinit(void) {
    _nativeState = USB_AUDIO_DISCONNECTED;
}

UsbAudioConnectionState usb_audio_get_state(void) { return _nativeState; }
bool usb_audio_is_connected(void) { return _nativeState >= USB_AUDIO_CONNECTED; }
bool usb_audio_is_streaming(void) { return _nativeState == USB_AUDIO_STREAMING; }

uint32_t usb_audio_read(int32_t *out, uint32_t frames) {
    if (_nativeState != USB_AUDIO_STREAMING) return 0;
    return usb_rb_read(&_nativeRingBuffer, out, frames);
}
uint32_t usb_audio_available_frames(void) {
    return usb_rb_available(&_nativeRingBuffer);
}

uint32_t usb_audio_get_sample_rate(void) { return _nativeRate; }
uint8_t usb_audio_get_bit_depth(void) { return _nativeDepth; }
uint8_t usb_audio_get_channels(void) { return 2; }
int16_t usb_audio_get_volume(void) { return _nativeVolume; }
bool usb_audio_get_mute(void) { return _nativeMute; }
float usb_audio_get_volume_linear(void) {
    if (_nativeMute) return 0.0f;
    return usb_volume_to_linear(_nativeVolume);
}
uint32_t usb_audio_get_overruns(void) { return _nativeRingBuffer.overruns; }
uint32_t usb_audio_get_underruns(void) { return _nativeRingBuffer.underruns; }

uint32_t usb_audio_get_negotiated_rate(void) { return _nativeRate; }
uint8_t  usb_audio_get_negotiated_depth(void) { return _nativeDepth; }

// Test injection API
void usb_audio_test_set_state(UsbAudioConnectionState state) { _nativeState = state; }
void usb_audio_test_set_negotiated_format(uint32_t rate, uint8_t depth) {
    _nativeRate = rate;
    _nativeDepth = depth;
}
void usb_audio_test_set_volume(int16_t vol, bool mute) {
    _nativeVolume = vol;
    _nativeMute = mute;
}

void usb_audio_poll_connection(void) {} // No-op in native tests

#else
// USB_AUDIO_ENABLED not defined and not NATIVE_TEST — empty stubs
void usb_audio_init(void) {}
void usb_audio_deinit(void) {}
UsbAudioConnectionState usb_audio_get_state(void) { return USB_AUDIO_DISCONNECTED; }
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
uint32_t usb_audio_get_negotiated_rate(void) { return 48000; }
uint8_t  usb_audio_get_negotiated_depth(void) { return 16; }
void usb_audio_poll_connection(void) {}
#endif
