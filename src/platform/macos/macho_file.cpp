#include "macho_file.h"

#include <algorithm>
#include <cstring>
#include <fstream>
#include <iterator>
#include <limits>
#include <set>

namespace eu4dll::platform::macos {
namespace {

constexpr std::uint32_t kMach64Magic = 0xFEEDFACF;
constexpr std::uint32_t kCpuTypeX86_64 = 0x01000007;
constexpr std::uint32_t kMachExecute = 2;
constexpr std::uint32_t kSegment64 = 0x19;
constexpr std::uint32_t kSymtab = 0x2;
constexpr std::uint32_t kUuid = 0x1B;
constexpr std::uint32_t kDyldInfoOnly = 0x80000022;
constexpr std::uint32_t kDyldExportsTrie = 0x80000033;

template <typename T>
bool ReadObject(const std::vector<std::uint8_t> &bytes, std::size_t offset, T &value) {
    if (offset > bytes.size() || bytes.size() - offset < sizeof(T)) return false;
    std::memcpy(&value, bytes.data() + offset, sizeof(T));
    return true;
}

struct Header64 {
    std::uint32_t magic;
    std::uint32_t cpuType;
    std::uint32_t cpuSubtype;
    std::uint32_t fileType;
    std::uint32_t commandCount;
    std::uint32_t commandBytes;
    std::uint32_t flags;
    std::uint32_t reserved;
};

struct LoadCommand { std::uint32_t command; std::uint32_t size; };
struct SegmentCommand64 {
    std::uint32_t command;
    std::uint32_t size;
    char name[16];
    std::uint64_t vmAddress;
    std::uint64_t vmSize;
    std::uint64_t fileOffset;
    std::uint64_t fileSize;
    std::uint32_t maximumProtection;
    std::uint32_t initialProtection;
    std::uint32_t sectionCount;
    std::uint32_t flags;
};
struct SymtabCommand {
    std::uint32_t command;
    std::uint32_t size;
    std::uint32_t symbolOffset;
    std::uint32_t symbolCount;
    std::uint32_t stringOffset;
    std::uint32_t stringSize;
};
struct DyldInfoCommand {
    std::uint32_t command;
    std::uint32_t size;
    std::uint32_t rebaseOffset;
    std::uint32_t rebaseSize;
    std::uint32_t bindOffset;
    std::uint32_t bindSize;
    std::uint32_t weakBindOffset;
    std::uint32_t weakBindSize;
    std::uint32_t lazyBindOffset;
    std::uint32_t lazyBindSize;
    std::uint32_t exportOffset;
    std::uint32_t exportSize;
};
struct LinkeditDataCommand {
    std::uint32_t command;
    std::uint32_t size;
    std::uint32_t dataOffset;
    std::uint32_t dataSize;
};
struct Nlist64 {
    std::uint32_t stringIndex;
    std::uint8_t type;
    std::uint8_t section;
    std::uint16_t description;
    std::uint64_t value;
};

bool AddOverflows(std::uint64_t first, std::uint64_t second) {
    return second > std::numeric_limits<std::uint64_t>::max() - first;
}

std::string FixedName(const char *data, std::size_t size) {
    const auto end = std::find(data, data + size, '\0');
    return std::string(data, end);
}

std::pair<std::string, std::uint64_t> FindVersion(const std::vector<std::uint8_t> &bytes) {
    static constexpr char prefix[] = "EU4 v1.37.";
    const auto found = std::search(bytes.begin(), bytes.end(), std::begin(prefix),
                                   std::end(prefix) - 1);
    if (found == bytes.end()) return {};
    std::string version;
    for (auto current = found + 5; current != bytes.end() && version.size() < 32; ++current) {
        const char character = static_cast<char>(*current);
        if ((character < '0' || character > '9') && character != '.') break;
        version.push_back(character);
    }
    return version.rfind("1.37.", 0) == 0
               ? std::make_pair(version,
                                static_cast<std::uint64_t>(found - bytes.begin() + 5))
               : std::pair<std::string, std::uint64_t>{};
}

bool ReadUleb(const std::vector<std::uint8_t> &bytes, std::size_t end,
              std::size_t &offset, std::uint64_t &value) {
    value = 0;
    unsigned shift = 0;
    while (offset < end && shift < 64) {
        const std::uint8_t byte = bytes[offset++];
        if (shift == 63 && (byte & 0xFE) != 0) return false;
        value |= static_cast<std::uint64_t>(byte & 0x7F) << shift;
        if ((byte & 0x80) == 0) return true;
        shift += 7;
    }
    return false;
}

bool ParseExportNode(const std::vector<std::uint8_t> &bytes, std::size_t trieStart,
                     std::size_t trieEnd, std::size_t nodeOffset,
                     const std::string &prefix, std::uint64_t imageBase,
                     std::set<std::size_t> &active,
                     std::vector<std::pair<std::string, std::uint64_t>> &symbols,
                     std::size_t depth = 0) {
    if (depth > 256 || prefix.size() > 4096 ||
        nodeOffset >= trieEnd - trieStart || !active.insert(nodeOffset).second) return false;
    std::size_t offset = trieStart + nodeOffset;
    std::uint64_t terminalSize = 0;
    if (!ReadUleb(bytes, trieEnd, offset, terminalSize) ||
        terminalSize > trieEnd - offset) return false;
    const std::size_t terminalEnd = offset + static_cast<std::size_t>(terminalSize);
    if (terminalSize != 0) {
        std::uint64_t flags = 0;
        if (!ReadUleb(bytes, terminalEnd, offset, flags)) return false;
        constexpr std::uint64_t kReexport = 0x08;
        if ((flags & kReexport) == 0) {
            std::uint64_t address = 0;
            if (!ReadUleb(bytes, terminalEnd, offset, address) ||
                AddOverflows(imageBase, address)) return false;
            symbols.emplace_back(prefix, imageBase + address);
        }
    }
    offset = terminalEnd;
    if (offset >= trieEnd) {
        active.erase(nodeOffset);
        return offset == trieEnd;
    }
    const std::uint8_t childCount = bytes[offset++];
    for (std::uint8_t child = 0; child < childCount; ++child) {
        const std::size_t edgeStart = offset;
        while (offset < trieEnd && bytes[offset] != 0) ++offset;
        if (offset == trieEnd) return false;
        const std::string edge(reinterpret_cast<const char *>(bytes.data() + edgeStart),
                               offset - edgeStart);
        ++offset;
        std::uint64_t childOffset = 0;
        if (!ReadUleb(bytes, trieEnd, offset, childOffset) ||
            childOffset > std::numeric_limits<std::size_t>::max() ||
            !ParseExportNode(bytes, trieStart, trieEnd,
                             static_cast<std::size_t>(childOffset), prefix + edge,
                             imageBase, active, symbols, depth + 1)) return false;
    }
    active.erase(nodeOffset);
    return true;
}

} // namespace

std::optional<MachOFile> MachOFile::Open(const std::string &path, std::string &error) {
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        error = "could not open Mach-O executable";
        return std::nullopt;
    }
    MachOFile file;
    file.bytes_.assign(std::istreambuf_iterator<char>(input),
                       std::istreambuf_iterator<char>());
    Header64 header{};
    if (!ReadObject(file.bytes_, 0, header) || header.magic != kMach64Magic ||
        header.cpuType != kCpuTypeX86_64 || header.fileType != kMachExecute) {
        error = "executable is not an x86-64 Mach-O main executable";
        return std::nullopt;
    }
    if (header.commandCount > 65536 ||
        header.commandBytes > file.bytes_.size() - sizeof(Header64)) {
        error = "invalid Mach-O load-command table";
        return std::nullopt;
    }
    std::optional<SymtabCommand> symtab;
    std::optional<std::pair<std::uint32_t, std::uint32_t>> exportTrie;
    bool sawUuid = false;
    std::size_t offset = sizeof(Header64);
    for (std::uint32_t index = 0; index < header.commandCount; ++index) {
        LoadCommand command{};
        if (!ReadObject(file.bytes_, offset, command) || command.size < sizeof(command) ||
            command.size > file.bytes_.size() - offset) {
            error = "invalid Mach-O load command";
            return std::nullopt;
        }
        if (command.command == kSegment64) {
            SegmentCommand64 segment{};
            if (!ReadObject(file.bytes_, offset, segment) ||
                AddOverflows(segment.fileOffset, segment.fileSize) ||
                segment.fileOffset + segment.fileSize > file.bytes_.size() ||
                AddOverflows(segment.vmAddress, segment.vmSize)) {
                error = "invalid Mach-O segment";
                return std::nullopt;
            }
            file.segments_.push_back({FixedName(segment.name, sizeof(segment.name)),
                                      segment.vmAddress, segment.vmSize,
                                      segment.fileOffset, segment.fileSize});
        } else if (command.command == kUuid) {
            if (command.size != 24 || offset + 24 > file.bytes_.size()) {
                error = "invalid LC_UUID command";
                return std::nullopt;
            }
            std::memcpy(file.uuid_.data(), file.bytes_.data() + offset + 8,
                        file.uuid_.size());
            sawUuid = true;
        } else if (command.command == kSymtab) {
            SymtabCommand value{};
            if (!ReadObject(file.bytes_, offset, value)) {
                error = "invalid LC_SYMTAB command";
                return std::nullopt;
            }
            symtab = value;
        } else if (command.command == kDyldInfoOnly) {
            DyldInfoCommand value{};
            if (!ReadObject(file.bytes_, offset, value)) {
                error = "invalid LC_DYLD_INFO_ONLY command";
                return std::nullopt;
            }
            if (value.exportSize != 0) {
                exportTrie = std::make_pair(value.exportOffset, value.exportSize);
            }
        } else if (command.command == kDyldExportsTrie) {
            LinkeditDataCommand value{};
            if (!ReadObject(file.bytes_, offset, value)) {
                error = "invalid LC_DYLD_EXPORTS_TRIE command";
                return std::nullopt;
            }
            exportTrie = std::make_pair(value.dataOffset, value.dataSize);
        }
        offset += command.size;
    }
    if (!sawUuid || file.segments_.empty()) {
        error = "Mach-O executable is missing LC_UUID or segments";
        return std::nullopt;
    }
    file.imageBase_ = std::numeric_limits<std::uint64_t>::max();
    std::uint64_t imageEnd = 0;
    for (const auto &segment : file.segments_) {
        if (segment.vmSize == 0 || segment.fileSize == 0) continue;
        file.imageBase_ = std::min(file.imageBase_, segment.vmAddress);
        imageEnd = std::max(imageEnd, segment.vmAddress + segment.vmSize);
    }
    if (file.imageBase_ == std::numeric_limits<std::uint64_t>::max() ||
        imageEnd < file.imageBase_) {
        error = "Mach-O executable has no mapped image";
        return std::nullopt;
    }
    file.imageSize_ = imageEnd - file.imageBase_;
    const auto version = FindVersion(file.bytes_);
    file.version_ = version.first;
    if (file.version_.empty()) {
        error = "could not find an EU4 1.37.x version marker";
        return std::nullopt;
    }
    const auto versionRva = file.FileOffsetToRva(version.second);
    if (!versionRva) {
        error = "EU4 version marker is outside mapped segments";
        return std::nullopt;
    }
    file.versionRva_ = *versionRva;
    if (symtab) {
        const std::uint64_t symbolsSize =
            static_cast<std::uint64_t>(symtab->symbolCount) * sizeof(Nlist64);
        if (!AddOverflows(symtab->symbolOffset, symbolsSize) &&
            symtab->symbolOffset + symbolsSize <= file.bytes_.size() &&
            !AddOverflows(symtab->stringOffset, symtab->stringSize) &&
            symtab->stringOffset + symtab->stringSize <= file.bytes_.size()) {
            for (std::uint32_t index = 0; index < symtab->symbolCount; ++index) {
                Nlist64 symbol{};
                ReadObject(file.bytes_, symtab->symbolOffset + index * sizeof(Nlist64), symbol);
                if (symbol.stringIndex >= symtab->stringSize) continue;
                const char *name = reinterpret_cast<const char *>(
                    file.bytes_.data() + symtab->stringOffset + symbol.stringIndex);
                const std::size_t maximum = symtab->stringSize - symbol.stringIndex;
                const void *terminator = std::memchr(name, 0, maximum);
                if (terminator != nullptr) {
                    // Undefined imports have value zero but still satisfy an install-time
                    // required-symbol capability contract. They are never valid symbol-
                    // scoped scan regions, so use the image base only as a non-zero marker.
                    file.symbols_.emplace_back(name,
                        symbol.value == 0 ? file.imageBase_ : symbol.value);
                }
            }
        }
    }
    if (exportTrie && exportTrie->second != 0) {
        const std::uint64_t trieEnd = static_cast<std::uint64_t>(exportTrie->first) +
                                      exportTrie->second;
        if (trieEnd > file.bytes_.size()) {
            error = "invalid dyld export trie range";
            return std::nullopt;
        }
        std::set<std::size_t> active;
        if (!ParseExportNode(file.bytes_, exportTrie->first,
                             static_cast<std::size_t>(trieEnd), 0, {}, file.imageBase_,
                             active, file.symbols_)) {
            error = "malformed dyld export trie";
            return std::nullopt;
        }
    }
    return file;
}

bool MachOFile::ReadVirtual(std::uint64_t address, void *output, std::size_t size,
                            std::string &error) const {
    auto *destination = static_cast<std::uint8_t *>(output);
    std::size_t remaining = size;
    while (remaining != 0) {
        const auto segment = std::find_if(segments_.begin(), segments_.end(),
            [address](const MachOSegment &candidate) {
                return address >= candidate.vmAddress &&
                       address - candidate.vmAddress < candidate.vmSize;
            });
        if (segment == segments_.end()) {
            error = "virtual address is outside mapped Mach-O segments";
            return false;
        }
        const std::uint64_t segmentOffset = address - segment->vmAddress;
        const std::uint64_t available = segment->vmSize - segmentOffset;
        const std::size_t chunk = static_cast<std::size_t>(
            std::min<std::uint64_t>(available, remaining));
        const std::uint64_t fileAvailable = segmentOffset < segment->fileSize
                                                ? segment->fileSize - segmentOffset : 0;
        const std::size_t fileChunk = static_cast<std::size_t>(
            std::min<std::uint64_t>(fileAvailable, chunk));
        if (fileChunk != 0) {
            std::memcpy(destination, bytes_.data() + segment->fileOffset + segmentOffset,
                        fileChunk);
        }
        std::fill(destination + fileChunk, destination + chunk, 0);
        destination += chunk;
        address += chunk;
        remaining -= chunk;
    }
    return true;
}

std::optional<std::uint64_t> MachOFile::FileOffsetToRva(
    std::uint64_t fileOffset) const {
    for (const auto &segment : segments_) {
        if (fileOffset >= segment.fileOffset &&
            fileOffset - segment.fileOffset < segment.fileSize) {
            return segment.vmAddress + (fileOffset - segment.fileOffset) - imageBase_;
        }
    }
    return std::nullopt;
}

std::optional<std::uint64_t> MachOFile::ResolveSymbol(const std::string &name) const {
    const auto found = std::find_if(symbols_.begin(), symbols_.end(),
        [&name](const auto &symbol) {
            return symbol.first == name || symbol.first == "_" + name;
        });
    return found == symbols_.end() ? std::nullopt
                                   : std::optional<std::uint64_t>(found->second);
}

std::optional<patch::MemoryRegion> MachOFileMemory::MainModule(std::string &) const {
    return patch::MemoryRegion{file_.ImageBase(),
                               static_cast<std::size_t>(file_.ImageSize()), "Mach-O image"};
}

std::optional<patch::Address> MachOFileMemory::ResolveSymbol(const std::string &name,
                                                             std::string &error) const {
    const auto symbol = file_.ResolveSymbol(name);
    if (!symbol) error = "required Mach-O symbol was not found: " + name;
    return symbol;
}

bool MachOFileMemory::Read(patch::Address address, std::uint8_t *output, std::size_t size,
                           std::string &error) const {
    return file_.ReadVirtual(address, output, size, error);
}

bool MachOFileMemory::ReadCString(patch::Address address, std::size_t maxSize,
                                  std::string &output, std::string &error) const {
    output.clear();
    for (std::size_t index = 0; index < maxSize; ++index) {
        std::uint8_t byte = 0;
        if (!Read(address + index, &byte, 1, error)) return false;
        if (byte == 0) return true;
        output.push_back(static_cast<char>(byte));
    }
    error = "Mach-O string exceeds maximum size";
    return false;
}

bool MachOFileMemory::Write(patch::Address, const std::uint8_t *, std::size_t,
                            std::string &error) {
    error = "read-only Mach-O mapping cannot be mutated";
    return false;
}

} // namespace eu4dll::platform::macos
