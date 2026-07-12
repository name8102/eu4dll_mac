#include "runtime/manifest/patch_manifest.h"
#include "runtime/patch/byte_buffer_memory.h"

#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

namespace manifest = eu4dll::manifest;

namespace {

void Require(bool condition, const char *message) {
    if (!condition) {
        std::cerr << message << '\n';
        std::exit(1);
    }
}

manifest::PatchManifest Fixture() {
    manifest::PatchManifest value;
    value.uuid[0] = 0xA5;
    value.gameVersion = "1.37.5";
    manifest::PatchEntry entry;
    entry.id = "rendering.screen-preprocess";
    entry.siteRva = 0x123456;
    entry.expectedOffset = 2;
    entry.expectedBytes = {0x48, 0x89, 0xE5, 0x90, 0x90};
    entry.mutationOffset = 2;
    entry.overwriteWidth = 5;
    entry.continuations = {{"return", 7}, {"bypass", 0x40}};
    entry.optimizeHook = true;
    value.entries.push_back(std::move(entry));
    return value;
}

} // namespace

int main() {
    std::string error;
    std::vector<std::uint8_t> bytes;
    const auto fixture = Fixture();
    Require(manifest::Serialize(fixture, bytes, error), error.c_str());
    std::vector<std::uint8_t> second;
    Require(manifest::Serialize(fixture, second, error), error.c_str());
    Require(bytes == second, "manifest serialization must be deterministic");

    manifest::PatchManifest parsed;
    Require(manifest::Parse(bytes, parsed, error), error.c_str());
    Require(parsed.gameVersion == "1.37.5" && parsed.entries.size() == 1,
            "manifest round trip lost header or entries");
    Require(parsed.entries[0].siteRva == 0x123456 &&
                parsed.entries[0].continuations.size() == 2,
            "manifest round trip lost RVA contract");

    auto corrupt = bytes;
    corrupt[0] ^= 0xFF;
    Require(!manifest::Parse(corrupt, parsed, error),
            "corrupt manifest magic must be rejected");
    corrupt = bytes;
    corrupt.push_back(0);
    Require(!manifest::Parse(corrupt, parsed, error),
            "manifest trailing data must be rejected");
    corrupt = bytes;
    corrupt[33] = 1;
    Require(!manifest::Parse(corrupt, parsed, error),
            "unknown required header flags must be rejected");

    auto duplicate = Fixture();
    duplicate.entries.push_back(duplicate.entries.front());
    Require(!manifest::Serialize(duplicate, bytes, error),
            "duplicate patch ids must be rejected");
    auto shortExpected = Fixture();
    shortExpected.entries[0].expectedBytes.resize(4);
    Require(!manifest::Serialize(shortExpected, bytes, error),
            "expected bytes shorter than overwrite width must be rejected");
    auto wrongVersion = Fixture();
    wrongVersion.gameVersion = "1.38.0";
    Require(!manifest::Serialize(wrongVersion, bytes, error),
            "non-1.37 manifest must be rejected");

    auto loaded = Fixture();
    loaded.entries[0].siteRva = 8;
    loaded.entries[0].expectedOffset = 2;
    loaded.entries[0].mutationOffset = 2;
    loaded.entries[0].expectedBytes = {1, 2, 3, 4, 5};
    loaded.entries[0].continuations[1].second = 0x20;
    loaded.versionRva = 30;
    std::vector<std::uint8_t> image(64, 0);
    std::copy(loaded.entries[0].expectedBytes.begin(),
              loaded.entries[0].expectedBytes.end(), image.begin() + 10);
    std::copy(loaded.gameVersion.begin(), loaded.gameVersion.end(), image.begin() + 30);
    eu4dll::patch::ByteBufferMemory memory(image, 0x700000);
    const auto before = memory.Bytes();
    auto validated = manifest::ValidateLoadedImage(
        loaded, loaded.uuid, loaded.gameVersion, memory.BaseAddress(), memory);
    Require(validated && validated.sites.size() == 1 &&
                validated.sites[0].siteAddress == 0x700008 &&
                validated.sites[0].mutationAddress == 0x70000A,
            "loaded-image validation did not apply the ASLR base to RVAs");
    Require(memory.Bytes() == before, "loaded-image validation must not mutate memory");
    Require(static_cast<bool>(manifest::ValidateLoadedImage(
                loaded, loaded.uuid, {}, memory.BaseAddress(), memory)),
            "loaded-image validation must verify version from the cached short RVA");
    auto staleUuid = loaded.uuid;
    staleUuid[0] ^= 1;
    Require(!manifest::ValidateLoadedImage(loaded, staleUuid, loaded.gameVersion,
                                           memory.BaseAddress(), memory),
            "stale UUID must be rejected");
    auto mismatchedImage = image;
    mismatchedImage[12] ^= 1;
    eu4dll::patch::ByteBufferMemory mismatch(std::move(mismatchedImage), 0x700000);
    const auto mismatchBefore = mismatch.Bytes();
    Require(!manifest::ValidateLoadedImage(loaded, loaded.uuid, loaded.gameVersion,
                                           mismatch.BaseAddress(), mismatch),
            "expected-byte mismatch must reject the complete manifest");
    Require(mismatch.Bytes() == mismatchBefore,
            "failed loaded-image validation must perform zero mutation");
    auto outsideImage = loaded;
    outsideImage.entries[0].siteRva = 0x1000;
    Require(!manifest::ValidateLoadedImage(outsideImage, outsideImage.uuid,
                                           outsideImage.gameVersion,
                                           memory.BaseAddress(), memory),
            "out-of-image site RVAs must be rejected");
    auto outsideContinuation = loaded;
    outsideContinuation.entries[0].continuations[0].second = 0x1000;
    Require(!manifest::ValidateLoadedImage(outsideContinuation,
                                           outsideContinuation.uuid,
                                           outsideContinuation.gameVersion,
                                           memory.BaseAddress(), memory),
            "out-of-image continuation RVAs must be rejected");
    return 0;
}
