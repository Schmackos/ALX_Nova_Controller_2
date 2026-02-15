#ifndef MBEDTLS_MD_MOCK_H
#define MBEDTLS_MD_MOCK_H

#include <cstdint>
#include <cstring>
#include <vector>

// mbedtls message digest type enum
typedef enum {
  MBEDTLS_MD_NONE = 0,
  MBEDTLS_MD_SHA256 = 6
} mbedtls_md_type_t;

// Opaque info struct (only used as pointer)
typedef struct {
  mbedtls_md_type_t type;
  uint8_t size; // digest size in bytes
} mbedtls_md_info_t;

// Context struct accumulates input data
typedef struct {
  std::vector<uint8_t> buffer;
  const mbedtls_md_info_t *md_info;
} mbedtls_md_context_t;

// Static info instance for SHA256
static const mbedtls_md_info_t _mock_sha256_info = {MBEDTLS_MD_SHA256, 32};

// Get info struct from type
inline const mbedtls_md_info_t *
mbedtls_md_info_from_type(mbedtls_md_type_t type) {
  if (type == MBEDTLS_MD_SHA256)
    return &_mock_sha256_info;
  return nullptr;
}

// Init context
inline void mbedtls_md_init(mbedtls_md_context_t *ctx) {
  ctx->buffer.clear();
  ctx->md_info = nullptr;
}

// Setup context with algorithm info
inline int mbedtls_md_setup(mbedtls_md_context_t *ctx,
                            const mbedtls_md_info_t *md_info, int hmac) {
  (void)hmac;
  ctx->md_info = md_info;
  return 0;
}

// Start/reset digest
inline int mbedtls_md_starts(mbedtls_md_context_t *ctx) {
  ctx->buffer.clear();
  return 0;
}

// Feed data
inline int mbedtls_md_update(mbedtls_md_context_t *ctx,
                             const unsigned char *input, size_t ilen) {
  ctx->buffer.insert(ctx->buffer.end(), input, input + ilen);
  return 0;
}

// Finalize — produces 32-byte deterministic hash using FNV-1a expanded
// Same input always yields same output; different inputs yield different output
inline int mbedtls_md_finish(mbedtls_md_context_t *ctx,
                             unsigned char *output) {
  // FNV-1a 32-bit on the accumulated buffer
  uint32_t hash = 2166136261u;
  for (size_t i = 0; i < ctx->buffer.size(); i++) {
    hash ^= ctx->buffer[i];
    hash *= 16777619u;
  }

  // Expand to 32 bytes by running 8 rounds with different seeds
  for (int round = 0; round < 8; round++) {
    uint32_t h = hash ^ (uint32_t)(round * 0x9E3779B9u);
    for (size_t i = 0; i < ctx->buffer.size(); i++) {
      h ^= ctx->buffer[i];
      h *= 16777619u;
    }
    output[round * 4 + 0] = (uint8_t)(h >> 24);
    output[round * 4 + 1] = (uint8_t)(h >> 16);
    output[round * 4 + 2] = (uint8_t)(h >> 8);
    output[round * 4 + 3] = (uint8_t)(h);
  }

  return 0;
}

// Free context
inline void mbedtls_md_free(mbedtls_md_context_t *ctx) {
  ctx->buffer.clear();
  ctx->md_info = nullptr;
}

#endif // MBEDTLS_MD_MOCK_H
