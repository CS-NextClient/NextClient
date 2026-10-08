/*********************************************************************
* Filename:   sha256.h
* Author:     Brad Conte (brad AT bradconte.com)
* Disclaimer: This code is presented "as is" without any guarantees.
* Details:    Defines the API for the corresponding SHA1 implementation.
*             Public domain (see http://www.zentut.com/c-tutorial/crypto-algorithms).
*********************************************************************/
#pragma once

#include <cstdint>
#include <string>

#define SHA256_BLOCK_SIZE 32 // SHA256 outputs a 32 byte digest

typedef struct
{
    uint8_t data[64];
    uint32_t datalen;
    unsigned long long bitlen;
    uint32_t state[8];
} SHA256_CTX;

void sha256_init(SHA256_CTX* ctx);
void sha256_update(SHA256_CTX* ctx, const uint8_t data[], size_t len);
void sha256_final(SHA256_CTX* ctx, uint8_t hash[]);

// Convenience wrapper: hashes the whole input in one call and returns
// the digest as a 64-character lowercase hex string.
std::string sha256_hex(const std::string& input);
