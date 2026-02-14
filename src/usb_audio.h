#ifndef USB_AUDIO_H
#define USB_AUDIO_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

// ===== USB Audio Connection States =====
enum UsbAudioState : uint8_t {
    USB_AUDIO_DISCONNECTED = 0,
    USB_AUDIO_CONNECTED,       // Host attached but not streaming
    USB_AUDIO_STREAMING,       // Host actively sending audio
};

// ===== SPSC Ring Buffer (lock-free, single-producer single-consumer) =====
// Producer: USB callback context
// Consumer: audio_capture_task on Core 1
struct UsbAudioRingBuffer {
    int32_t *buffer;           // Stereo interleaved samples (left-justified int32)
    uint32_t capacity;         // Total frames (stereo pairs) — must be power of 2
    volatile uint32_t writePos; // Written by producer only
    volatile uint32_t readPos;  // Written by consumer only
    uint32_t overruns;         // Producer couldn't write (full)
    uint32_t underruns;        // Consumer couldn't read (empty)
};

// ===== Ring Buffer API (pure, testable) =====

// Initialize ring buffer with given capacity (must be power of 2).
// buffer must point to capacity * 2 int32_t elements (stereo interleaved).
void usb_rb_init(UsbAudioRingBuffer *rb, int32_t *buffer, uint32_t capacity_frames);

// Reset positions and counters
void usb_rb_reset(UsbAudioRingBuffer *rb);

// Write stereo frames to ring buffer. Returns number of frames actually written.
uint32_t usb_rb_write(UsbAudioRingBuffer *rb, const int32_t *data, uint32_t frames);

// Read stereo frames from ring buffer. Returns number of frames actually read.
uint32_t usb_rb_read(UsbAudioRingBuffer *rb, int32_t *data, uint32_t frames);

// Number of frames available to read
uint32_t usb_rb_available(const UsbAudioRingBuffer *rb);

// Number of frames of free space for writing
uint32_t usb_rb_free(const UsbAudioRingBuffer *rb);

// Fill level as fraction 0.0-1.0
float usb_rb_fill_level(const UsbAudioRingBuffer *rb);

// ===== Format Conversion (pure, testable) =====

// Convert USB PCM16 stereo to left-justified int32 (matching I2S ADC format)
// src: interleaved int16 stereo samples (L,R,L,R,...)
// dst: interleaved int32 stereo samples (L,R,L,R,...), bits [31:8] = audio
// frames: number of stereo frames
void usb_pcm16_to_int32(const int16_t *src, int32_t *dst, uint32_t frames);

// Convert USB PCM24 stereo (3 bytes packed) to left-justified int32
// src: packed 24-bit samples (3 bytes per sample, interleaved stereo)
// dst: interleaved int32 stereo samples, bits [31:8] = audio
// frames: number of stereo frames
void usb_pcm24_to_int32(const uint8_t *src, int32_t *dst, uint32_t frames);

// Convert UAC2 volume (1/256 dB, range -32767 to 0) to linear gain (0.0 to 1.0)
float usb_volume_to_linear(int16_t volume_256db);

// ===== Public API (ESP32 + native stubs) =====

// Initialize USB audio subsystem (call from setup, before WiFi)
void usb_audio_init(void);

// Deinitialize USB audio (stop task, clean up)
void usb_audio_deinit(void);

// Get current connection state
UsbAudioState usb_audio_get_state(void);

// Check connection/streaming status
bool usb_audio_is_connected(void);
bool usb_audio_is_streaming(void);

// Read stereo frames from USB ring buffer into I2S-format int32 buffer.
// Returns number of frames read (0 if not streaming or buffer empty).
// out must hold at least frames*2 int32_t elements.
uint32_t usb_audio_read(int32_t *out, uint32_t frames);

// Number of frames available in the ring buffer
uint32_t usb_audio_available_frames(void);

// Get current format info
uint32_t usb_audio_get_sample_rate(void);
uint8_t usb_audio_get_bit_depth(void);
uint8_t usb_audio_get_channels(void);

// Get host volume/mute
int16_t usb_audio_get_volume(void);    // 1/256 dB units
bool usb_audio_get_mute(void);
float usb_audio_get_volume_linear(void); // 0.0-1.0

// Get buffer health stats
uint32_t usb_audio_get_overruns(void);
uint32_t usb_audio_get_underruns(void);

#endif // USB_AUDIO_H
