#ifndef MBEDTLS_PKCS5_MOCK_H
#define MBEDTLS_PKCS5_MOCK_H

#include <cstdint>
#include <cstring>

// Mock PBKDF2-HMAC for native tests.
// Uses FNV-1a expansion (same approach as the SHA256 mock in md.h) so that:
//   - Same (password + salt) always produces the same derived key
//   - Different inputs produce different keys
inline int mbedtls_pkcs5_pbkdf2_hmac_ext(
    int md_type,
    const unsigned char *password, size_t plen,
    const unsigned char *salt, size_t slen,
    unsigned int iterations,
    size_t key_length,
    unsigned char *output) {

  (void)md_type;
  (void)iterations;

  // Combine password + salt into one buffer for hashing
  // Use FNV-1a seeded with salt bytes, then mixed with password
  for (size_t round = 0; round < key_length / 4; round++) {
    uint32_t h = 2166136261u ^ (uint32_t)(round * 0x85ebca6bu);

    // Mix in salt
    for (size_t i = 0; i < slen; i++) {
      h ^= salt[i];
      h *= 16777619u;
    }

    // Mix in password
    for (size_t i = 0; i < plen; i++) {
      h ^= password[i];
      h *= 16777619u;
    }

    output[round * 4 + 0] = (uint8_t)(h >> 24);
    output[round * 4 + 1] = (uint8_t)(h >> 16);
    output[round * 4 + 2] = (uint8_t)(h >> 8);
    output[round * 4 + 3] = (uint8_t)(h);
  }

  return 0;
}

#endif // MBEDTLS_PKCS5_MOCK_H
