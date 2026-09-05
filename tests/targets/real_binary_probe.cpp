#include "runtime/patch/memory.h"
#include "targets/eu4_1_37_5/macos_x86_64/compatibility_preflight.h"
#include "targets/eu4_1_37_5/macos_x86_64/profile.h"
#include "targets/eu4_1_37_5/macos_x86_64/target_facts.h"

#include <cstdint>
#include <cstring>
#include <fstream>
#include <iostream>
#include <iterator>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace target = eu4dll::targets::eu4_1_37_5::macos_x86_64;

namespace {

constexpr std::uint32_t kLoadSegment64 = 0x19;
constexpr std::uint32_t kLoadSymtab = 0x02;

template<typename Value>
bool Load(const std::vector<std::uint8_t> &bytes, std::size_t offset, Value &value) {
    if (offset > bytes.size() || sizeof(value) > bytes.size() - offset) return false;
    std::memcpy(&value, bytes.data() + offset, sizeof(value));
    return true;
}

std::string FixedString(const std::vector<std::uint8_t> &bytes,
                        std::size_t offset, std::size_t size) {
    if (offset > bytes.size() || size > bytes.size() - offset) return {};
    const auto *begin = reinterpret_cast<const char *>(bytes.data() + offset);
    std::size_t length = 0;
    while (length < size && begin[length] != '\0') ++length;
    return {begin, length};
}

class MachFileMemory final : public eu4dll::patch::Memory {
public:
    explicit MachFileMemory(const char *path) : path_(path) {
        std::ifstream input(path, std::ios::binary);
        if (!input) {
            error_ = "could not open executable";
            return;
        }
        const std::istreambuf_iterator<char> begin(input);
        const std::istreambuf_iterator<char> end;
        bytes_ = {begin, end};
        Parse();
    }

    [[nodiscard]] bool Valid() const { return error_.empty(); }
    [[nodiscard]] const std::string &Error() const { return error_; }

    bool Read(eu4dll::patch::Address address, std::uint8_t *buffer,
              std::size_t size, std::string &error) const override {
        if (buffer == nullptr && size != 0) {
            error = "null read buffer";
            return false;
        }
        const auto offset = FileOffset(address, size);
        if (!offset) {
            error = "address is outside file-backed Mach-O segments";
            return false;
        }
        if (size != 0) std::memcpy(buffer, bytes_.data() + *offset, size);
        return true;
    }

    eu4dll::patch::WriteResult Write(eu4dll::patch::Address, const std::uint8_t *,
                                    std::size_t) override {
        return {false, true, "read-only binary probe"};
    }

    bool ReadCString(eu4dll::patch::Address address, std::size_t maxSize,
                     std::string &value, std::string &error) const override {
        value.clear();
        for (std::size_t index = 0; index < maxSize; ++index) {
            std::uint8_t byte = 0;
            if (!Read(address + index, &byte, 1, error)) return false;
            if (byte == 0) return true;
            value.push_back(static_cast<char>(byte));
        }
        error = "string is not terminated within the requested size";
        return false;
    }

    std::optional<eu4dll::patch::MemoryRegion> MainModule(
        std::string &error) const override {
        if (mainModule_.size == 0) {
            error = "Mach-O __TEXT segment is missing";
            return std::nullopt;
        }
        return mainModule_;
    }

    std::optional<eu4dll::patch::Address> ResolveSymbol(
        const std::string &symbol, std::string &error) const override {
        const auto found = symbols_.find(symbol);
        if (found == symbols_.end()) {
            error = "symbol not found: " + symbol;
            return std::nullopt;
        }
        return found->second;
    }

private:
    struct Segment {
        eu4dll::patch::Address vmAddress = 0;
        std::uint64_t fileOffset = 0;
        std::uint64_t fileSize = 0;
        std::string name;
    };

    std::optional<std::size_t> FileOffset(eu4dll::patch::Address address,
                                          std::size_t size) const {
        for (const auto &segment : segments_) {
            if (address < segment.vmAddress) continue;
            const auto relative = address - segment.vmAddress;
            if (relative > segment.fileSize || size > segment.fileSize - relative) continue;
            const auto offset = segment.fileOffset + relative;
            if (offset > bytes_.size() || size > bytes_.size() - offset) continue;
            return static_cast<std::size_t>(offset);
        }
        return std::nullopt;
    }

    void Parse() {
        std::uint32_t magic = 0;
        std::uint32_t commandCount = 0;
        if (!Load(bytes_, 0, magic) || magic != target::executable::kMach64Magic ||
            !Load(bytes_, 16, commandCount)) {
            error_ = "not a supported 64-bit Mach-O file";
            return;
        }

        std::uint32_t symbolOffset = 0;
        std::uint32_t symbolCount = 0;
        std::uint32_t stringOffset = 0;
        std::uint32_t stringSize = 0;
        std::size_t commandOffset = 32;
        for (std::uint32_t index = 0; index < commandCount; ++index) {
            std::uint32_t command = 0;
            std::uint32_t commandSize = 0;
            if (!Load(bytes_, commandOffset, command) ||
                !Load(bytes_, commandOffset + 4, commandSize) || commandSize < 8 ||
                commandOffset > bytes_.size() || commandSize > bytes_.size() - commandOffset) {
                error_ = "invalid Mach-O load command";
                return;
            }
            if (command == kLoadSegment64 && commandSize >= 72) {
                Segment segment;
                segment.name = FixedString(bytes_, commandOffset + 8, 16);
                Load(bytes_, commandOffset + 24, segment.vmAddress);
                Load(bytes_, commandOffset + 40, segment.fileOffset);
                Load(bytes_, commandOffset + 48, segment.fileSize);
                segments_.push_back(segment);
                if (segment.name == "__TEXT") {
                    mainModule_ = {segment.vmAddress,
                                   static_cast<std::size_t>(segment.fileSize), path_};
                }
            } else if (command == kLoadSymtab && commandSize >= 24) {
                Load(bytes_, commandOffset + 8, symbolOffset);
                Load(bytes_, commandOffset + 12, symbolCount);
                Load(bytes_, commandOffset + 16, stringOffset);
                Load(bytes_, commandOffset + 20, stringSize);
            }
            commandOffset += commandSize;
        }
        if (mainModule_.size == 0 || symbolOffset == 0 || stringOffset == 0) {
            error_ = "Mach-O target metadata is incomplete";
            return;
        }

        for (std::uint32_t index = 0; index < symbolCount; ++index) {
            const std::size_t entry = symbolOffset + static_cast<std::size_t>(index) * 16;
            std::uint32_t stringIndex = 0;
            std::uint64_t value = 0;
            if (!Load(bytes_, entry, stringIndex) || !Load(bytes_, entry + 8, value) ||
                stringIndex >= stringSize) continue;
            const auto name = FixedString(bytes_, stringOffset + stringIndex,
                                          stringSize - stringIndex);
            if (name.empty()) continue;
            const auto address = value == 0 ? mainModule_.address : value;
            symbols_.emplace(name, address);
            if (name.front() == '_') symbols_.emplace(name.substr(1), address);
        }
    }

    std::string path_;
    std::vector<std::uint8_t> bytes_;
    std::vector<Segment> segments_;
    std::unordered_map<std::string, eu4dll::patch::Address> symbols_;
    eu4dll::patch::MemoryRegion mainModule_;
    std::string error_;
};

} // namespace

int main(int argc, char **argv) {
    if (argc != 2) {
        std::cerr << "usage: eu4dll_real_binary_probe /path/to/eu4\n";
        return 2;
    }
    MachFileMemory memory(argv[1]);
    if (!memory.Valid()) {
        std::cerr << memory.Error() << '\n';
        return 2;
    }
    const auto validation = target::ValidateTarget(memory);
    if (!validation) {
        std::cerr << target::FormatValidationFailure(validation) << '\n';
        return 1;
    }
    const auto preflight = target::PreflightCompatibility(memory);
    if (!preflight) {
        for (const auto &failure : preflight.failures) {
            std::cerr << eu4dll::patch::FormatDiagnostic(failure) << '\n';
        }
        return 1;
    }
    std::cout << "target=" << target::kDiagnosticTargetId
              << " check=capability-preflight.complete"
              << " version=" << validation.versionText
              << " sites=" << preflight.checkedSites
              << " symbols=" << preflight.checkedSymbols << '\n';
    return 0;
}
