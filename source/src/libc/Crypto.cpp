// CommonCrypto: CC_MD5, CC_SHA1, CC_SHA256 and CCHmac (CCCrypt reports unimplemented).
#include "Crypto.h"
#include "Hle.h"
#include "Log.h"
#include "Memory.h"
#include <cstring>
#include <string>
#include <vector>

using hle::arg;
using hle::ret;
namespace {
std::uint32_t rotl(std::uint32_t x, int n) { return (x << n) | (x >> (32 - n)); }
std::uint32_t rotr(std::uint32_t x, int n) { return (x >> n) | (x << (32 - n)); }

std::vector<std::uint8_t> md5(const std::uint8_t *data, std::size_t len) {
    static const std::uint32_t K[64] = {
        0xd76aa478, 0xe8c7b756, 0x242070db, 0xc1bdceee, 0xf57c0faf, 0x4787c62a, 0xa8304613, 0xfd469501, 0x698098d8, 0x8b44f7af,
        0xffff5bb1, 0x895cd7be, 0x6b901122, 0xfd987193, 0xa679438e, 0x49b40821, 0xf61e2562, 0xc040b340, 0x265e5a51, 0xe9b6c7aa,
        0xd62f105d, 0x02441453, 0xd8a1e681, 0xe7d3fbc8, 0x21e1cde6, 0xc33707d6, 0xf4d50d87, 0x455a14ed, 0xa9e3e905, 0xfcefa3f8,
        0x676f02d9, 0x8d2a4c8a, 0xfffa3942, 0x8771f681, 0x6d9d6122, 0xfde5380c, 0xa4beea44, 0x4bdecfa9, 0xf6bb4b60, 0xbebfbc70,
        0x289b7ec6, 0xeaa127fa, 0xd4ef3085, 0x04881d05, 0xd9d4d039, 0xe6db99e5, 0x1fa27cf8, 0xc4ac5665, 0xf4292244, 0x432aff97,
        0xab9423a7, 0xfc93a039, 0x655b59c3, 0x8f0ccc92, 0xffeff47d, 0x85845dd1, 0x6fa87e4f, 0xfe2ce6e0, 0xa3014314, 0x4e0811a1,
        0xf7537e82, 0xbd3af235, 0x2ad7d2bb, 0xeb86d391};
    static const int R[64] = {7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22, 5, 9, 14, 20, 5, 9, 14, 20, 5, 9, 14, 20, 5, 9, 14, 20,
                              4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23, 6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21};
    std::vector<std::uint8_t> m(data, data + len);
    m.push_back(0x80);
    while (m.size() % 64 != 56) m.push_back(0);
    const std::uint64_t bits = std::uint64_t(len) * 8;
    for (int i = 0; i < 8; ++i) m.push_back(std::uint8_t(bits >> (8 * i)));
    std::uint32_t h[4] = {0x67452301, 0xefcdab89, 0x98badcfe, 0x10325476};
    for (std::size_t off = 0; off < m.size(); off += 64) {
        std::uint32_t w[16];
        for (int i = 0; i < 16; ++i) std::memcpy(&w[i], &m[off + 4 * i], 4);
        std::uint32_t a = h[0], b = h[1], c = h[2], d = h[3];
        for (int i = 0; i < 64; ++i) {
            std::uint32_t f;
            int g;
            if (i < 16) { f = (b & c) | (~b & d); g = i; }
            else if (i < 32) { f = (d & b) | (~d & c); g = (5 * i + 1) % 16; }
            else if (i < 48) { f = b ^ c ^ d; g = (3 * i + 5) % 16; }
            else { f = c ^ (b | ~d); g = (7 * i) % 16; }
            const std::uint32_t t = d;
            d = c;
            c = b;
            b = b + rotl(a + f + K[i] + w[g], R[i]);
            a = t;
        }
        h[0] += a; h[1] += b; h[2] += c; h[3] += d;
    }
    std::vector<std::uint8_t> out(16);
    std::memcpy(out.data(), h, 16);
    return out;
}
std::vector<std::uint8_t> sha1(const std::uint8_t *data, std::size_t len) {
    std::vector<std::uint8_t> m(data, data + len);
    m.push_back(0x80);
    while (m.size() % 64 != 56) m.push_back(0);
    const std::uint64_t bits = std::uint64_t(len) * 8;
    for (int i = 7; i >= 0; --i) m.push_back(std::uint8_t(bits >> (8 * i)));
    std::uint32_t h[5] = {0x67452301, 0xEFCDAB89, 0x98BADCFE, 0x10325476, 0xC3D2E1F0};
    for (std::size_t off = 0; off < m.size(); off += 64) {
        std::uint32_t w[80];
        for (int i = 0; i < 16; ++i) w[i] = (std::uint32_t(m[off + 4 * i]) << 24) | (m[off + 4 * i + 1] << 16) | (m[off + 4 * i + 2] << 8) | m[off + 4 * i + 3];
        for (int i = 16; i < 80; ++i) w[i] = rotl(w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16], 1);
        std::uint32_t a = h[0], b = h[1], c = h[2], d = h[3], e = h[4];
        for (int i = 0; i < 80; ++i) {
            std::uint32_t f, k;
            if (i < 20) { f = (b & c) | (~b & d); k = 0x5A827999; }
            else if (i < 40) { f = b ^ c ^ d; k = 0x6ED9EBA1; }
            else if (i < 60) { f = (b & c) | (b & d) | (c & d); k = 0x8F1BBCDC; }
            else { f = b ^ c ^ d; k = 0xCA62C1D6; }
            const std::uint32_t t = rotl(a, 5) + f + e + k + w[i];
            e = d; d = c; c = rotl(b, 30); b = a; a = t;
        }
        h[0] += a; h[1] += b; h[2] += c; h[3] += d; h[4] += e;
    }
    std::vector<std::uint8_t> out;
    for (auto v : h) for (int i = 3; i >= 0; --i) out.push_back(std::uint8_t(v >> (8 * i)));
    return out;
}
std::vector<std::uint8_t> sha256(const std::uint8_t *data, std::size_t len) {
    static const std::uint32_t K[64] = {
        0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5, 0xd807aa98, 0x12835b01,
        0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174, 0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc,
        0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da, 0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147,
        0x06ca6351, 0x14292967, 0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
        0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070, 0x19a4c116, 0x1e376c08,
        0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3, 0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208,
        0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2};
    std::vector<std::uint8_t> m(data, data + len);
    m.push_back(0x80);
    while (m.size() % 64 != 56) m.push_back(0);
    const std::uint64_t bits = std::uint64_t(len) * 8;
    for (int i = 7; i >= 0; --i) m.push_back(std::uint8_t(bits >> (8 * i)));
    std::uint32_t h[8] = {0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a, 0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19};
    for (std::size_t off = 0; off < m.size(); off += 64) {
        std::uint32_t w[64];
        for (int i = 0; i < 16; ++i) w[i] = (std::uint32_t(m[off + 4 * i]) << 24) | (m[off + 4 * i + 1] << 16) | (m[off + 4 * i + 2] << 8) | m[off + 4 * i + 3];
        for (int i = 16; i < 64; ++i) {
            const auto s0 = rotr(w[i - 15], 7) ^ rotr(w[i - 15], 18) ^ (w[i - 15] >> 3);
            const auto s1 = rotr(w[i - 2], 17) ^ rotr(w[i - 2], 19) ^ (w[i - 2] >> 10);
            w[i] = w[i - 16] + s0 + w[i - 7] + s1;
        }
        std::uint32_t a = h[0], b = h[1], c = h[2], d = h[3], e = h[4], f = h[5], g = h[6], hh = h[7];
        for (int i = 0; i < 64; ++i) {
            const auto S1 = rotr(e, 6) ^ rotr(e, 11) ^ rotr(e, 25);
            const auto ch = (e & f) ^ (~e & g);
            const auto t1 = hh + S1 + ch + K[i] + w[i];
            const auto S0 = rotr(a, 2) ^ rotr(a, 13) ^ rotr(a, 22);
            const auto maj = (a & b) ^ (a & c) ^ (b & c);
            const auto t2 = S0 + maj;
            hh = g; g = f; f = e; e = d + t1; d = c; c = b; b = a; a = t1 + t2;
        }
        h[0] += a; h[1] += b; h[2] += c; h[3] += d; h[4] += e; h[5] += f; h[6] += g; h[7] += hh;
    }
    std::vector<std::uint8_t> out;
    for (auto v : h) for (int i = 3; i >= 0; --i) out.push_back(std::uint8_t(v >> (8 * i)));
    return out;
}
using HashFn = std::vector<std::uint8_t> (*)(const std::uint8_t *, std::size_t);
void hashInto(HashFn fn) {
    const auto digest = fn(mem::base() + arg(0), std::size_t(std::uint32_t(arg(1))));
    std::memcpy(mem::base() + arg(2), digest.data(), digest.size());
    ret(arg(2));
}
std::vector<std::uint8_t> hmac(HashFn fn, std::size_t block, const std::uint8_t *key, std::size_t keyLen, const std::uint8_t *data, std::size_t len) {
    std::vector<std::uint8_t> k(key, key + keyLen);
    if (k.size() > block) k = fn(k.data(), k.size());
    k.resize(block, 0);
    std::vector<std::uint8_t> inner(block), outer(block);
    for (std::size_t i = 0; i < block; ++i) { inner[i] = k[i] ^ 0x36; outer[i] = k[i] ^ 0x5c; }
    inner.insert(inner.end(), data, data + len);
    const auto ih = fn(inner.data(), inner.size());
    outer.insert(outer.end(), ih.begin(), ih.end());
    return fn(outer.data(), outer.size());
}
} // namespace

std::string crypto::sha1Hex(const void *data, std::size_t size) {
    const auto digest = sha1(static_cast<const std::uint8_t *>(data), size);
    static const char *digits = "0123456789abcdef";
    std::string hex;
    for (auto b : digest) { hex += digits[b >> 4]; hex += digits[b & 15]; }
    return hex;
}
HLE(CC_MD5) { hashInto(md5); }
HLE(CC_SHA1) { hashInto(sha1); }
HLE(CC_SHA256) { hashInto(sha256); }
// CCHmac(algorithm, key, keyLength, data, dataLength, macOut): kCCHmacAlgSHA1 0, MD5 1, SHA256 2.
HLE(CCHmac) {
    HashFn fn = arg(0) == 1 ? md5 : arg(0) == 2 ? sha256 : sha1;
    const auto mac = hmac(fn, 64, mem::base() + arg(1), std::size_t(arg(2)), mem::base() + arg(3), std::size_t(arg(4)));
    std::memcpy(mem::base() + arg(5), mac.data(), mac.size());
}
HLE(CCCrypt) {
    hlog("crypto", "CCCrypt (op %llu, alg %llu) is not implemented", (unsigned long long)arg(0), (unsigned long long)arg(1));
    ret(std::uint64_t(std::int64_t(-4305))); // kCCUnimplemented
}
