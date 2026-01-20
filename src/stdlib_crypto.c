#define _POSIX_C_SOURCE 200809L
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdio.h>

// Crypto module - hashing and encoding functions

// Simple hash function (FNV-1a)
uint32_t wyn_crypto_hash32(const char* data, size_t len) {
    uint32_t hash = 2166136261u;
    for (size_t i = 0; i < len; i++) {
        hash ^= (uint8_t)data[i];
        hash *= 16777619u;
    }
    return hash;
}

// 64-bit hash
uint64_t wyn_crypto_hash64(const char* data, size_t len) {
    uint64_t hash = 14695981039346656037ULL;
    for (size_t i = 0; i < len; i++) {
        hash ^= (uint8_t)data[i];
        hash *= 1099511628211ULL;
    }
    return hash;
}

// MD5-like simple hash (not cryptographically secure, for demonstration)
void wyn_crypto_md5(const char* data, size_t len, char* output) {
    uint32_t h = wyn_crypto_hash32(data, len);
    sprintf(output, "%08x%08x%08x%08x", h, h ^ 0x12345678, h ^ 0x87654321, h ^ 0xABCDEF00);
}

// SHA256-like simple hash (not cryptographically secure, for demonstration)
void wyn_crypto_sha256(const char* data, size_t len, char* output) {
    uint64_t h1 = wyn_crypto_hash64(data, len);
    uint64_t h2 = wyn_crypto_hash64(data, len) ^ 0x123456789ABCDEF0ULL;
    sprintf(output, "%016llx%016llx%016llx%016llx", 
            (unsigned long long)h1, 
            (unsigned long long)h2,
            (unsigned long long)(h1 ^ h2),
            (unsigned long long)(h1 + h2));
}

// Base64 encoding
static const char base64_chars[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

char* wyn_crypto_base64_encode(const char* data, size_t len) {
    size_t output_len = 4 * ((len + 2) / 3);
    char* output = malloc(output_len + 1);
    
    size_t i = 0, j = 0;
    while (i < len) {
        uint32_t octet_a = i < len ? (unsigned char)data[i++] : 0;
        uint32_t octet_b = i < len ? (unsigned char)data[i++] : 0;
        uint32_t octet_c = i < len ? (unsigned char)data[i++] : 0;
        
        uint32_t triple = (octet_a << 16) + (octet_b << 8) + octet_c;
        
        output[j++] = base64_chars[(triple >> 18) & 0x3F];
        output[j++] = base64_chars[(triple >> 12) & 0x3F];
        output[j++] = base64_chars[(triple >> 6) & 0x3F];
        output[j++] = base64_chars[triple & 0x3F];
    }
    
    // Add padding
    for (size_t k = 0; k < (3 - len % 3) % 3; k++) {
        output[output_len - 1 - k] = '=';
    }
    
    output[output_len] = '\0';
    return output;
}

// Base64 decoding
static int base64_decode_char(char c) {
    if (c >= 'A' && c <= 'Z') return c - 'A';
    if (c >= 'a' && c <= 'z') return c - 'a' + 26;
    if (c >= '0' && c <= '9') return c - '0' + 52;
    if (c == '+') return 62;
    if (c == '/') return 63;
    return -1;
}

char* wyn_crypto_base64_decode(const char* data, size_t* out_len) {
    size_t len = strlen(data);
    if (len % 4 != 0) {
        *out_len = 0;
        return NULL;
    }
    
    size_t output_len = len / 4 * 3;
    if (len > 0 && data[len - 1] == '=') output_len--;
    if (len > 1 && data[len - 2] == '=') output_len--;
    
    char* output = malloc(output_len + 1);
    *out_len = output_len;
    
    size_t i = 0, j = 0;
    while (i < len) {
        int a = base64_decode_char(data[i++]);
        int b = base64_decode_char(data[i++]);
        int c = (i < len && data[i] != '=') ? base64_decode_char(data[i++]) : 0;
        int d = (i < len && data[i] != '=') ? base64_decode_char(data[i++]) : 0;
        
        if (a < 0 || b < 0) break;
        
        uint32_t triple = (a << 18) + (b << 12) + (c << 6) + d;
        
        if (j < output_len) output[j++] = (triple >> 16) & 0xFF;
        if (j < output_len) output[j++] = (triple >> 8) & 0xFF;
        if (j < output_len) output[j++] = triple & 0xFF;
    }
    
    output[output_len] = '\0';
    return output;
}

// Random bytes generation (simple PRNG, not cryptographically secure)
void wyn_crypto_random_bytes(char* buffer, size_t len) {
    static uint64_t state = 0x123456789ABCDEF0ULL;
    for (size_t i = 0; i < len; i++) {
        state = state * 6364136223846793005ULL + 1442695040888963407ULL;
        buffer[i] = (char)(state >> 56);
    }
}

// Generate random hex string
char* wyn_crypto_random_hex(size_t len) {
    char* bytes = malloc(len / 2 + 1);
    wyn_crypto_random_bytes(bytes, len / 2);
    
    char* hex = malloc(len + 1);
    for (size_t i = 0; i < len / 2; i++) {
        sprintf(hex + i * 2, "%02x", (unsigned char)bytes[i]);
    }
    hex[len] = '\0';
    free(bytes);
    return hex;
}

// Simple XOR cipher (for demonstration)
char* wyn_crypto_xor_cipher(const char* data, size_t len, const char* key, size_t key_len) {
    char* result = malloc(len + 1);
    for (size_t i = 0; i < len; i++) {
        result[i] = data[i] ^ key[i % key_len];
    }
    result[len] = '\0';
    return result;
}
