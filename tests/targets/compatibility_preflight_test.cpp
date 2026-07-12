#include "runtime/patch/byte_buffer_memory.h"
#include "targets/eu4_1_37_5/macos_x86_64/compatibility_preflight.h"

#include <cstdlib>
#include <iostream>
#include <set>
#include <string>
#include <vector>

namespace target = eu4dll::targets::eu4_1_37_5::macos_x86_64;

namespace {

void Require(bool condition, const char *message) {
    if (!condition) {
        std::cerr << message << '\n';
        std::exit(1);
    }
}

} // namespace

int main() {
    const auto &registry = target::CompatibilityPatchRegistry();
    Require(registry.size() == 55, "capability registry must cover all 55 mutation sites");
    std::set<std::string> ids;
    for (const auto &contract : registry) {
        Require(!contract.id.empty() && ids.insert(contract.id).second,
                "capability registry IDs must be stable and unique");
        Require(contract.description.feature == contract.id,
                "registry ID and diagnostic feature must match");
        Require(contract.description.expected.has_value(),
                "every mutation site must record original bytes");
        Require(contract.description.expected->bytes.size() == contract.overwriteWidth,
                "expected bytes must cover the complete overwrite width");
        Require(contract.description.expected->mask.empty() ||
                    contract.description.expected->mask.size() == contract.overwriteWidth,
                "expected-byte mask must cover the complete overwrite width");
    }

    eu4dll::patch::ByteBufferMemory incompatible(
        std::vector<std::uint8_t>(4096, 0xCC));
    const auto before = incompatible.Bytes();
    const auto result = target::PreflightCompatibility(incompatible);
    Require(!result, "incompatible image must fail preflight");
    Require(result.checkedSites == 55 && result.checkedSymbols == 16,
            "preflight must check every site and required symbol");
    Require(result.failures.size() >= 55,
            "preflight must aggregate failures instead of stopping at the first site");
    Require(incompatible.Bytes() == before, "preflight must perform zero mutation");
    return 0;
}
