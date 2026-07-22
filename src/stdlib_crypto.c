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
    snprintf(output, 33, "%08x%08x%08x%08x", h, h ^ 0x12345678, h ^ 0x87654321, h ^ 0xABCDEF00);
}

// === Real SHA-256 (FIPS 180-4) ===
static const uint32_t wyn_sha256_K[64] = {
    0x428a2f98u, 0x71374491u, 0xb5c0fbcfu, 0xe9b5dba5u, 0x3956c25bu, 0x59f111f1u, 0x923f82a4u, 0xab1c5ed5u,
    0xd807aa98u, 0x12835b01u, 0x243185beu, 0x550c7dc3u, 0x72be5d74u, 0x80deb1feu, 0x9bdc06a7u, 0xc19bf174u,
    0xe49b69c1u, 0xefbe4786u, 0x0fc19dc6u, 0x240ca1ccu, 0x2de92c6fu, 0x4a7484aau, 0x5cb0a9dcu, 0x76f988dau,
    0x983e5152u, 0xa831c66du, 0xb00327c8u, 0xbf597fc7u, 0xc6e00bf3u, 0xd5a79147u, 0x06ca6351u, 0x14292967u,
    0x27b70a85u, 0x2e1b2138u, 0x4d2c6dfcu, 0x53380d13u, 0x650a7354u, 0x766a0abbu, 0x81c2c92eu, 0x92722c85u,
    0xa2bfe8a1u, 0xa81a664bu, 0xc24b8b70u, 0xc76c51a3u, 0xd192e819u, 0xd6990624u, 0xf40e3585u, 0x106aa070u,
    0x19a4c116u, 0x1e376c08u, 0x2748774cu, 0x34b0bcb5u, 0x391c0cb3u, 0x4ed8aa4au, 0x5b9cca4fu, 0x682e6ff3u,
    0x748f82eeu, 0x78a5636fu, 0x84c87814u, 0x8cc70208u, 0x90befffau, 0xa4506cebu, 0xbef9a3f7u, 0xc67178f2u
};

#define WYN_ROTR32(x, n) (((x) >> (n)) | ((x) << (32 - (n))))

typedef struct {
    uint32_t h[8];
    uint64_t total;
    unsigned char buf[64];
    size_t buflen;
} WynSha256Ctx;

static void wyn_sha256_compress(uint32_t h[8], const unsigned char p[64]) {
    uint32_t w[64];
    for (int i = 0; i < 16; i++) {
        w[i] = ((uint32_t)p[i*4] << 24) | ((uint32_t)p[i*4+1] << 16) |
               ((uint32_t)p[i*4+2] << 8) | (uint32_t)p[i*4+3];
    }
    for (int i = 16; i < 64; i++) {
        uint32_t s0 = WYN_ROTR32(w[i-15], 7) ^ WYN_ROTR32(w[i-15], 18) ^ (w[i-15] >> 3);
        uint32_t s1 = WYN_ROTR32(w[i-2], 17) ^ WYN_ROTR32(w[i-2], 19) ^ (w[i-2] >> 10);
        w[i] = w[i-16] + s0 + w[i-7] + s1;
    }
    uint32_t a = h[0], b = h[1], c = h[2], d = h[3];
    uint32_t e = h[4], f = h[5], g = h[6], hh = h[7];
    for (int i = 0; i < 64; i++) {
        uint32_t s1 = WYN_ROTR32(e, 6) ^ WYN_ROTR32(e, 11) ^ WYN_ROTR32(e, 25);
        uint32_t ch = (e & f) ^ ((~e) & g);
        uint32_t t1 = hh + s1 + ch + wyn_sha256_K[i] + w[i];
        uint32_t s0 = WYN_ROTR32(a, 2) ^ WYN_ROTR32(a, 13) ^ WYN_ROTR32(a, 22);
        uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
        uint32_t t2 = s0 + maj;
        hh = g; g = f; f = e; e = d + t1;
        d = c; c = b; b = a; a = t1 + t2;
    }
    h[0] += a; h[1] += b; h[2] += c; h[3] += d;
    h[4] += e; h[5] += f; h[6] += g; h[7] += hh;
}

static void wyn_sha256_begin(WynSha256Ctx* c) {
    c->h[0] = 0x6a09e667u; c->h[1] = 0xbb67ae85u; c->h[2] = 0x3c6ef372u; c->h[3] = 0xa54ff53au;
    c->h[4] = 0x510e527fu; c->h[5] = 0x9b05688cu; c->h[6] = 0x1f83d9abu; c->h[7] = 0x5be0cd19u;
    c->total = 0;
    c->buflen = 0;
}

static void wyn_sha256_feed(WynSha256Ctx* c, const unsigned char* data, size_t len) {
    c->total += len;
    while (len > 0) {
        size_t take = 64 - c->buflen;
        if (take > len) take = len;
        memcpy(c->buf + c->buflen, data, take);
        c->buflen += take;
        data += take;
        len -= take;
        if (c->buflen == 64) {
            wyn_sha256_compress(c->h, c->buf);
            c->buflen = 0;
        }
    }
}

static void wyn_sha256_done(WynSha256Ctx* c, unsigned char out[32]) {
    uint64_t bitlen = c->total * 8;
    unsigned char pad = 0x80;
    wyn_sha256_feed(c, &pad, 1);
    unsigned char zero = 0;
    while (c->buflen != 56) wyn_sha256_feed(c, &zero, 1);
    unsigned char lenbuf[8];
    for (int i = 0; i < 8; i++) lenbuf[i] = (unsigned char)(bitlen >> (56 - 8*i));
    wyn_sha256_feed(c, lenbuf, 8);
    for (int i = 0; i < 8; i++) {
        out[i*4]   = (unsigned char)(c->h[i] >> 24);
        out[i*4+1] = (unsigned char)(c->h[i] >> 16);
        out[i*4+2] = (unsigned char)(c->h[i] >> 8);
        out[i*4+3] = (unsigned char)(c->h[i]);
    }
}

// Raw binary digest: out32 must hold 32 bytes.
void wyn_sha256_raw(const unsigned char* data, size_t len, unsigned char* out32) {
    WynSha256Ctx c;
    wyn_sha256_begin(&c);
    wyn_sha256_feed(&c, data, len);
    wyn_sha256_done(&c, out32);
}

// HMAC-SHA256 (RFC 2104). Raw binary output: out32 must hold 32 bytes.
void wyn_hmac_sha256_raw(const unsigned char* key, size_t keylen,
                         const unsigned char* data, size_t datalen,
                         unsigned char* out32) {
    unsigned char kblock[64];
    memset(kblock, 0, sizeof(kblock));
    if (keylen > 64) {
        wyn_sha256_raw(key, keylen, kblock); // hashed key: 32 bytes, rest zero
    } else {
        memcpy(kblock, key, keylen);
    }
    unsigned char ipad[64], opad[64];
    for (int i = 0; i < 64; i++) {
        ipad[i] = (unsigned char)(kblock[i] ^ 0x36);
        opad[i] = (unsigned char)(kblock[i] ^ 0x5c);
    }
    WynSha256Ctx c;
    unsigned char inner[32];
    wyn_sha256_begin(&c);
    wyn_sha256_feed(&c, ipad, 64);
    wyn_sha256_feed(&c, data, datalen);
    wyn_sha256_done(&c, inner);
    wyn_sha256_begin(&c);
    wyn_sha256_feed(&c, opad, 64);
    wyn_sha256_feed(&c, inner, 32);
    wyn_sha256_done(&c, out32);
}

// Real SHA-256, hex output (previously a non-cryptographic stand-in).
void wyn_crypto_sha256(const char* data, size_t len, char* output) {
    unsigned char digest[32];
    wyn_sha256_raw((const unsigned char*)data, len, digest);
    for (int i = 0; i < 32; i++) snprintf(output + i*2, 3, "%02x", digest[i]);
    output[64] = 0;
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
        snprintf(hex + i * 2, 3, "%02x", (unsigned char)bytes[i]);
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

// SHA-1 implementation (required for WebSocket handshake)
void wyn_crypto_sha1(const unsigned char* data, size_t len, unsigned char* digest) {
    uint32_t h0=0x67452301, h1=0xEFCDAB89, h2=0x98BADCFE, h3=0x10325476, h4=0xC3D2E1F0;
    // Pre-processing: pad message
    size_t new_len = len + 1;
    while (new_len % 64 != 56) new_len++;
    unsigned char* msg = calloc(new_len + 8, 1);
    memcpy(msg, data, len);
    msg[len] = 0x80;
    uint64_t bits = len * 8;
    for (int i = 0; i < 8; i++) msg[new_len + 7 - i] = (bits >> (i * 8)) & 0xFF;
    new_len += 8;

    for (size_t offset = 0; offset < new_len; offset += 64) {
        uint32_t w[80];
        for (int i = 0; i < 16; i++)
            w[i] = ((uint32_t)msg[offset+i*4]<<24) | ((uint32_t)msg[offset+i*4+1]<<16) |
                    ((uint32_t)msg[offset+i*4+2]<<8) | msg[offset+i*4+3];
        for (int i = 16; i < 80; i++) {
            uint32_t t = w[i-3] ^ w[i-8] ^ w[i-14] ^ w[i-16];
            w[i] = (t << 1) | (t >> 31);
        }
        uint32_t a=h0, b=h1, c=h2, d=h3, e=h4;
        for (int i = 0; i < 80; i++) {
            uint32_t f, k;
            if (i < 20)      { f = (b&c)|((~b)&d); k = 0x5A827999; }
            else if (i < 40) { f = b^c^d;           k = 0x6ED9EBA1; }
            else if (i < 60) { f = (b&c)|(b&d)|(c&d); k = 0x8F1BBCDC; }
            else              { f = b^c^d;           k = 0xCA62C1D6; }
            uint32_t temp = ((a<<5)|(a>>27)) + f + e + k + w[i];
            e = d; d = c; c = (b<<30)|(b>>2); b = a; a = temp;
        }
        h0 += a; h1 += b; h2 += c; h3 += d; h4 += e;
    }
    free(msg);
    uint32_t h[5] = {h0, h1, h2, h3, h4};
    for (int i = 0; i < 5; i++)
        for (int j = 0; j < 4; j++)
            digest[i*4+j] = (h[i] >> (24 - j*8)) & 0xFF;
}

char* Crypto_sha1(const char* data) {
    unsigned char digest[20];
    wyn_crypto_sha1((const unsigned char*)data, strlen(data), digest);
    static char hex[41];
    for (int i = 0; i < 20; i++) snprintf(hex + i*2, 3, "%02x", digest[i]);
    hex[40] = '\0';
    return hex;
}

char* Crypto_sha1_base64(const char* data) {
    unsigned char digest[20];
    wyn_crypto_sha1((const unsigned char*)data, strlen(data), digest);
    return wyn_crypto_base64_encode((const char*)digest, 20);
}
