#include "platform/linux/linux_elf_identity.h"

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <sstream>

namespace eu4dll::linux_platform {
namespace {

constexpr uint32_t RotateRight(uint32_t value, unsigned int count) {
    return (value >> count) | (value << (32 - count));
}

class Sha256 {
public:
    void Update(const uint8_t *data, size_t size) {
        totalSize_ += size;
        while (size > 0) {
            const size_t room = block_.size() - blockSize_;
            const size_t chunk = std::min(size, room);
            std::memcpy(block_.data() + blockSize_, data, chunk);
            blockSize_ += chunk;
            data += chunk;
            size -= chunk;
            if (blockSize_ == block_.size()) {
                Transform(block_.data());
                blockSize_ = 0;
            }
        }
    }

    std::array<uint8_t, 32> Finalize() {
        const uint64_t bitSize = totalSize_ * 8;
        block_[blockSize_++] = 0x80;
        if (blockSize_ > 56) {
            std::fill(block_.begin() + blockSize_, block_.end(), 0);
            Transform(block_.data());
            blockSize_ = 0;
        }
        std::fill(block_.begin() + blockSize_, block_.begin() + 56, 0);
        for (size_t i = 0; i < sizeof(bitSize); ++i) {
            block_[63 - i] = static_cast<uint8_t>(bitSize >> (i * 8));
        }
        Transform(block_.data());
        std::array<uint8_t, 32> digest{};
        for (size_t i = 0; i < state_.size(); ++i) {
            digest[i * 4] = static_cast<uint8_t>(state_[i] >> 24);
            digest[i * 4 + 1] = static_cast<uint8_t>(state_[i] >> 16);
            digest[i * 4 + 2] = static_cast<uint8_t>(state_[i] >> 8);
            digest[i * 4 + 3] = static_cast<uint8_t>(state_[i]);
        }
        return digest;
    }

private:
    void Transform(const uint8_t *block) {
        static constexpr std::array<uint32_t, 64> kConstants{{
            0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1,
            0x923f82a4, 0xab1c5ed5, 0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3,
            0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174, 0xe49b69c1, 0xefbe4786,
            0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
            0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147,
            0x06ca6351, 0x14292967, 0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13,
            0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85, 0xa2bfe8a1, 0xa81a664b,
            0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
            0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a,
            0x5b9cca4f, 0x682e6ff3, 0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208,
            0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2}};
        std::array<uint32_t, 64> words{};
        for (size_t i = 0; i < 16; ++i) {
            words[i] = static_cast<uint32_t>(block[i * 4]) << 24 |
                       static_cast<uint32_t>(block[i * 4 + 1]) << 16 |
                       static_cast<uint32_t>(block[i * 4 + 2]) << 8 |
                       static_cast<uint32_t>(block[i * 4 + 3]);
        }
        for (size_t i = 16; i < words.size(); ++i) {
            const uint32_t s0 = RotateRight(words[i - 15], 7) ^ RotateRight(words[i - 15], 18) ^
                                (words[i - 15] >> 3);
            const uint32_t s1 = RotateRight(words[i - 2], 17) ^ RotateRight(words[i - 2], 19) ^
                                (words[i - 2] >> 10);
            words[i] = words[i - 16] + s0 + words[i - 7] + s1;
        }
        auto working = state_;
        for (size_t i = 0; i < words.size(); ++i) {
            const uint32_t s1 = RotateRight(working[4], 6) ^ RotateRight(working[4], 11) ^
                                RotateRight(working[4], 25);
            const uint32_t choose = (working[4] & working[5]) ^ (~working[4] & working[6]);
            const uint32_t temp1 = working[7] + s1 + choose + kConstants[i] + words[i];
            const uint32_t s0 = RotateRight(working[0], 2) ^ RotateRight(working[0], 13) ^
                                RotateRight(working[0], 22);
            const uint32_t majority = (working[0] & working[1]) ^ (working[0] & working[2]) ^
                                      (working[1] & working[2]);
            const uint32_t temp2 = s0 + majority;
            working[7] = working[6];
            working[6] = working[5];
            working[5] = working[4];
            working[4] = working[3] + temp1;
            working[3] = working[2];
            working[2] = working[1];
            working[1] = working[0];
            working[0] = temp1 + temp2;
        }
        for (size_t i = 0; i < state_.size(); ++i) state_[i] += working[i];
    }

    std::array<uint32_t, 8> state_{{0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
                                    0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19}};
    std::array<uint8_t, 64> block_{};
    size_t blockSize_ = 0;
    uint64_t totalSize_ = 0;
};

}  // namespace

bool ComputeFileSha256(const std::string &path,
                       std::array<std::uint8_t, 32> &digest, std::string &error) {
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        error = "failed to open main executable for SHA-256: " + path;
        return false;
    }
    Sha256 sha256;
    std::array<uint8_t, 64 * 1024> buffer{};
    while (input) {
        input.read(reinterpret_cast<char *>(buffer.data()), buffer.size());
        sha256.Update(buffer.data(), static_cast<size_t>(input.gcount()));
    }
    if (!input.eof()) {
        error = "failed while reading main executable for SHA-256: " + path;
        return false;
    }
    digest = sha256.Finalize();
    return true;
}

std::string Sha256Hex(const std::array<std::uint8_t, 32> &digest) {
    std::ostringstream stream;
    stream << std::hex << std::setfill('0');
    for (const auto byte : digest) {
        stream << std::setw(2) << static_cast<unsigned>(byte);
    }
    return stream.str();
}

bool ContainsVersionString(patch::Memory &memory, const std::string &expected) {
    if (expected.empty()) return false;
    std::string error;
    // Prefer read-only segments (rodata) but fall back to executable regions
    // so ByteBufferMemory fixtures without PT_LOAD classes still validate.
    auto regions = memory.MainModuleRegions(patch::RegionPurpose::ReadOnlySearch, error);
    if (regions.empty()) {
        regions = memory.MainModuleRegions(patch::RegionPurpose::ExecutableSearch, error);
    }
    if (!error.empty() || regions.empty()) return false;
    std::vector<std::uint8_t> needle(expected.begin(), expected.end());
    needle.push_back(0);
    for (const auto &region : regions) {
        if (region.size < needle.size()) continue;
        std::vector<std::uint8_t> haystack(region.size);
        if (!memory.Read(region.address, haystack.data(), haystack.size(), error)) {
            continue;
        }
        if (std::search(haystack.begin(), haystack.end(), needle.begin(), needle.end()) !=
            haystack.end()) {
            return true;
        }
    }
    return false;
}

bool AllowUnsupportedElf() {
    const char *value = std::getenv("EU4DLL_ALLOW_UNSUPPORTED_ELF");
    return value != nullptr && std::strcmp(value, "1") == 0;
}

bool IdentityForFile(const std::string &path, manifest::ImageIdentity &identity,
                     std::string &error) {
    std::array<std::uint8_t, 32> digest{};
    if (!ComputeFileSha256(path, digest, error)) return false;
    identity = manifest::MakeFileSha256Identity(digest);
    return true;
}

}  // namespace eu4dll::linux_platform
