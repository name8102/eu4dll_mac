#include "platform/macos/macho_file.h"

#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <string>
#include <unistd.h>
#include <vector>

namespace {

template <typename T>
void Put(std::vector<std::uint8_t> &bytes, std::size_t offset, T value) {
    std::memcpy(bytes.data() + offset, &value, sizeof(value));
}

void Require(bool condition, const char *message) {
    if (!condition) {
        std::cerr << message << '\n';
        std::exit(1);
    }
}

std::string FixturePath() {
    const std::string path = "/tmp/eu4dll-macho-test-" + std::to_string(getpid());
    std::vector<std::uint8_t> bytes(0x400, 0);
    Put<std::uint32_t>(bytes, 0, 0xFEEDFACF);
    Put<std::uint32_t>(bytes, 4, 0x01000007);
    Put<std::uint32_t>(bytes, 12, 2);
    Put<std::uint32_t>(bytes, 16, 3);
    Put<std::uint32_t>(bytes, 20, 72 + 24 + 24);

    std::size_t command = 32;
    Put<std::uint32_t>(bytes, command, 0x19);
    Put<std::uint32_t>(bytes, command + 4, 72);
    std::memcpy(bytes.data() + command + 8, "__TEXT", 6);
    Put<std::uint64_t>(bytes, command + 24, 0x100000000ULL);
    Put<std::uint64_t>(bytes, command + 32, 0x300);
    Put<std::uint64_t>(bytes, command + 40, 0x100);
    Put<std::uint64_t>(bytes, command + 48, 0x300);
    command += 72;
    Put<std::uint32_t>(bytes, command, 0x1B);
    Put<std::uint32_t>(bytes, command + 4, 24);
    for (std::size_t index = 0; index < 16; ++index) bytes[command + 8 + index] = index;
    command += 24;
    Put<std::uint32_t>(bytes, command, 0x2);
    Put<std::uint32_t>(bytes, command + 4, 24);
    Put<std::uint32_t>(bytes, command + 8, 0x300);
    Put<std::uint32_t>(bytes, command + 12, 1);
    Put<std::uint32_t>(bytes, command + 16, 0x320);
    Put<std::uint32_t>(bytes, command + 20, 0x20);

    std::memcpy(bytes.data() + 0x180, "EU4 v1.37.5", 12);
    bytes[0x220] = 0xAA;
    bytes[0x221] = 0xBB;
    Put<std::uint32_t>(bytes, 0x300, 1);
    Put<std::uint64_t>(bytes, 0x308, 0x100000120ULL);
    std::memcpy(bytes.data() + 0x321, "_required", 10);

    std::ofstream output(path, std::ios::binary);
    output.write(reinterpret_cast<const char *>(bytes.data()), bytes.size());
    return path;
}

} // namespace

int main() {
    const std::string path = FixturePath();
    std::string error;
    const auto file = eu4dll::platform::macos::MachOFile::Open(path, error);
    unlink(path.c_str());
    Require(file.has_value(), error.c_str());
    Require(file->ImageBase() == 0x100000000ULL && file->Version() == "1.37.5",
            "Mach-O identity parsing failed");
    Require(file->VersionRva() == 0x85, "version marker RVA parsing failed");
    Require(file->Uuid()[15] == 15, "LC_UUID parsing failed");
    const auto rva = file->FileOffsetToRva(0x220);
    Require(rva && *rva == 0x120, "file-offset to RVA mapping failed");
    const auto symbol = file->ResolveSymbol("_required");
    Require(symbol && *symbol == 0x100000120ULL, "Mach-O symbol lookup failed");
    std::uint8_t data[2]{};
    Require(file->ReadVirtual(0x100000120ULL, data, sizeof(data), error), error.c_str());
    Require(data[0] == 0xAA && data[1] == 0xBB, "virtual read mapped wrong bytes");

    eu4dll::platform::macos::MachOFileMemory memory(*file);
    Require(memory.ResolveSymbol("_required", error).has_value(),
            "Memory adapter symbol lookup failed");
    Require(!memory.Write(0, data, sizeof(data), error),
            "Mach-O file mapping must remain read-only");
    return 0;
}
