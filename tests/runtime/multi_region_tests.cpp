#include "runtime/patch/byte_buffer_memory.h"
#include "runtime/patch/patch_runtime.h"

#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

namespace {

using eu4dll::patch::Address;
using eu4dll::patch::ByteBufferMemory;
using eu4dll::patch::MemoryRegion;
using eu4dll::patch::PatchRuntime;
using eu4dll::patch::RegionPurpose;

void Require(bool condition, const char *message) {
    if (!condition) {
        std::cerr << "FAILED: " << message << '\n';
        std::exit(1);
    }
}

// Two discontiguous executable segments with an unmapped gap between them.
// Reads that span the gap must never happen: each region is read separately.
class TwoRegionMemory final : public eu4dll::patch::Memory {
public:
    TwoRegionMemory(std::vector<std::uint8_t> first, Address firstBase,
                    std::vector<std::uint8_t> second, Address secondBase)
        : first_(std::move(first), firstBase, "region-a"),
          second_(std::move(second), secondBase, "region-b") {}

    bool Read(Address address, std::uint8_t *buffer, std::size_t size,
              std::string &error) const override {
        // Fail closed on any range that is not fully inside one region, so a
        // test failure proves the scanner never reads fabricated gaps.
        std::string ignored;
        if (Contains(first_, address, size)) return first_.Read(address, buffer, size, error);
        if (Contains(second_, address, size)) return second_.Read(address, buffer, size, error);
        error = "read spans unmapped gap";
        return false;
    }

    eu4dll::patch::WriteResult Write(Address address, const std::uint8_t *data,
                                      std::size_t size) override {
        if (Contains(first_, address, size)) return first_.Write(address, data, size);
        if (Contains(second_, address, size)) return second_.Write(address, data, size);
        eu4dll::patch::WriteResult result;
        result.error = "write spans unmapped gap";
        return result;
    }

    bool ReadCString(Address address, std::size_t maxSize, std::string &value,
                     std::string &error) const override {
        value.clear();
        for (std::size_t i = 0; i < maxSize; ++i) {
            std::uint8_t byte = 0;
            if (!Read(address + i, &byte, 1, error)) return false;
            if (byte == 0) return true;
            value.push_back(static_cast<char>(byte));
        }
        error = "unterminated string";
        return false;
    }

    std::optional<MemoryRegion> MainModule(std::string &error) const override {
        return first_.MainModule(error);
    }

    std::vector<MemoryRegion> MainModuleRegions(RegionPurpose purpose,
                                                std::string &error) const override {
        if (purpose != RegionPurpose::ExecutableSearch) {
            error.clear();
            return {};
        }
        std::string ignored;
        auto a = first_.MainModule(ignored);
        auto b = second_.MainModule(ignored);
        error.clear();
        return {*a, *b};
    }

    std::optional<Address> ResolveSymbol(const std::string &symbol,
                                         std::string &error) const override {
        return first_.ResolveSymbol(symbol, error);
    }

private:
    static bool Contains(const ByteBufferMemory &memory, Address address, std::size_t size) {
        const Address base = memory.BaseAddress();
        const std::size_t extent = memory.Bytes().size();
        if (address < base || size > extent) return false;
        return address - base <= extent - size;
    }

    ByteBufferMemory first_;
    ByteBufferMemory second_;
};

eu4dll::patch::PatchDescription RawPatch(std::string pattern) {
    eu4dll::patch::PatchDescription description;
    description.feature = "multi-region";
    description.target = "test-target";
    description.location.pattern = std::move(pattern);
    description.mutation.kind = eu4dll::patch::MutationKind::RawBytes;
    description.mutation.bytes = {0xCC};
    return description;
}

void TestMatchInSecondRegion() {
    TwoRegionMemory memory({0x90, 0x90, 0x90}, 0x1000, {0xAA, 0xBB, 0xCC}, 0x9000);
    PatchRuntime runtime(memory);
    const auto result = runtime.Locate(RawPatch("AA BB").location, "multi-region", "test-target");
    Require(static_cast<bool>(result), "match in second region must succeed");
    Require(result.address == 0x9000, "match address must be in the second region");
}

void TestNoMatchAcrossRegions() {
    TwoRegionMemory memory({0x90, 0xAA}, 0x1000, {0xBB, 0x90}, 0x9000);
    PatchRuntime runtime(memory);
    // The byte pair AA BB straddles the unmapped gap; it must NOT match.
    const auto result = runtime.Locate(RawPatch("AA BB").location, "multi-region", "test-target");
    Require(!result, "straddling pattern must not match across the gap");
    Require(result.diagnostic.match == eu4dll::patch::MatchStatus::NotFound,
            "gap-spanning pattern reports not-found");
}

void TestAmbiguousAcrossRegions() {
    TwoRegionMemory memory({0xAA, 0xBB}, 0x1000, {0xAA, 0xBB}, 0x9000);
    PatchRuntime runtime(memory);
    const auto result = runtime.Locate(RawPatch("AA BB").location, "multi-region", "test-target");
    Require(!result, "duplicate matches in different regions must fail unique search");
    Require(result.diagnostic.match == eu4dll::patch::MatchStatus::Ambiguous,
            "cross-region duplicates report ambiguous");
    Require(result.diagnostic.matchCount == 2, "match count aggregates across regions");
}

void TestSingleRegionStillWorks() {
    ByteBufferMemory memory({0xAA, 0xBB, 0xCC}, 0x4000);
    PatchRuntime runtime(memory);
    const auto result = runtime.Locate(RawPatch("AA BB").location, "multi-region", "test-target");
    Require(static_cast<bool>(result), "single-region search still succeeds");
}

}  // namespace

int main() {
    TestMatchInSecondRegion();
    TestNoMatchAcrossRegions();
    TestAmbiguousAcrossRegions();
    TestSingleRegionStillWorks();
    std::cout << "multi-region tests passed" << std::endl;
    return 0;
}
