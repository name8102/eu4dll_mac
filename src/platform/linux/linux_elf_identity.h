#pragma once

#include "runtime/manifest/patch_manifest.h"
#include "runtime/patch/memory.h"

#include <array>
#include <cstdint>
#include <string>

namespace eu4dll::linux_platform {

// ELF identity helpers. File hashing lives on the platform side; the
// supported-digest and version-text policy lives in the Linux target profile.
bool ComputeFileSha256(const std::string &path,
                       std::array<std::uint8_t, 32> &digest, std::string &error);
std::string Sha256Hex(const std::array<std::uint8_t, 32> &digest);

// Searches executable-adjacent readable regions for the exact expected
// version string (including its NUL terminator). Returns true when found.
bool ContainsVersionString(patch::Memory &memory, const std::string &expected);

// Development-only override. Fail-closed by default; an explicit
// EU4DLL_ALLOW_UNSUPPORTED_ELF=1 produces an unmistakable warning and still
// validates the version string.
bool AllowUnsupportedElf();

// Builds the manifest identity for a local ELF file.
bool IdentityForFile(const std::string &path, manifest::ImageIdentity &identity,
                     std::string &error);

}  // namespace eu4dll::linux_platform
