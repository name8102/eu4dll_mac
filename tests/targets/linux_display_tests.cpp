#include "runtime/patch/byte_buffer_memory.h"
#include "targets/eu4_1_37_5/linux_x86_64/display_formatting/display_patch.h"
#include <algorithm>
#include <cstdlib>
#include <iostream>

namespace display = eu4dll::targets::eu4_1_37_5::linux_x86_64::display_formatting;
namespace {
void Require(bool ok, const char *message) {
    if (!ok) { std::cerr << message << '\n'; std::exit(1); }
}
eu4dll::patch::ByteBufferMemory Fixture(int defect) {
    std::vector<std::uint8_t> bytes(0x3000, 0xcc);
    const auto put = [&](std::size_t offset, std::initializer_list<std::uint8_t> code) {
        std::copy(code.begin(), code.end(), bytes.begin() + offset);
    };
    put(0x4f, {0xe8,0xc2,0xc6,9,0});
    put(0x111f, {0xe8,0x6a,0x36,0xec,0});
    put(0x21bb, {0xe8,0x0a,0,0x55,1});
    put(0x2203, {0xe8,0xc2,0xff,0x54,1});
    put(0x225e, {0xe8,0x67,0xff,0x54,1});
    if (defect == 1) bytes[0x225e] = 0xcc;
    if (defect == 2) put(0x226e, {0xe8,0x67,0xff,0x54,1});
    // Keep fixture CALL destinations in rel32 reach without an allocator.
    const auto base = reinterpret_cast<eu4dll::patch::Address>(display::PreflightDisplay);
    eu4dll::patch::ByteBufferMemory memory(std::move(bytes), base);
    memory.DefineSymbol("_ZN10CTopbarGui26RefreshSpeedControlsWindowERK8CCountry", base);
    memory.DefineSymbol("_ZNK8CMonarch11GetFullNameEv", base + 0x1000);
    memory.DefineSymbol("_ZN8CCountry18GetNewRepublicNameE11CCountryTagPK8CCultureRK20CWeightedStringTableRK6CArrayI7CStringEbiiRbRKS8_bb", base + 0x2000);
    memory.DefineSymbol("_ZNK14CGregorianDate9GetStringERK7CString", base + 0x2800);
    if (defect != 3) memory.DefineSymbol("_ZN11CDLCManager10_pInstanceE", base + 0x2900);
    return memory;
}
}
int main() {
    for (int defect = 0; defect <= 3; ++defect) {
        auto memory = Fixture(defect);
        const auto before = memory.Bytes();
        const auto result = display::PreflightDisplay(memory, nullptr);
        Require(static_cast<bool>(result) == (defect == 0), "display preflight rejects missing/ambiguous sites and symbols");
        Require(memory.Bytes() == before, "display preflight never mutates bytes");
    }
}
