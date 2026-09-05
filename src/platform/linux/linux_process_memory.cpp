#include "platform/linux/linux_process_memory.h"

#include <dlfcn.h>
#include <elf.h>
#include <fcntl.h>
#include <link.h>
#include <sys/mman.h>
#include <sys/uio.h>
#include <unistd.h>

#include <cerrno>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <limits>
#include <sstream>

namespace eu4dll::linux_platform {
namespace {

uintptr_t AlignDown(uintptr_t value, size_t alignment) {
    return value & ~static_cast<uintptr_t>(alignment - 1);
}

uintptr_t AlignUp(uintptr_t value, size_t alignment) {
    return (value + alignment - 1) & ~static_cast<uintptr_t>(alignment - 1);
}

int ParsePermissions(const std::string &perms) {
    int protection = 0;
    if (perms.size() >= 1 && perms[0] == 'r') protection |= PROT_READ;
    if (perms.size() >= 2 && perms[1] == 'w') protection |= PROT_WRITE;
    if (perms.size() >= 3 && perms[2] == 'x') protection |= PROT_EXEC;
    return protection;
}

bool QueryProtection(uintptr_t address, int &protection) {
    std::ifstream maps("/proc/self/maps");
    if (!maps) return false;
    std::string line;
    while (std::getline(maps, line)) {
        std::istringstream stream(line);
        std::string range;
        std::string perms;
        if (!(stream >> range >> perms)) continue;
        const auto dash = range.find('-');
        if (dash == std::string::npos) continue;
        const uintptr_t start =
            static_cast<uintptr_t>(std::strtoull(range.substr(0, dash).c_str(), nullptr, 16));
        const uintptr_t end = static_cast<uintptr_t>(
            std::strtoull(range.substr(dash + 1).c_str(), nullptr, 16));
        if (address >= start && address < end) {
            protection = ParsePermissions(perms);
            return true;
        }
    }
    return false;
}

std::string ReadExecutablePath(std::string &error) {
    char buffer[4096]{};
    const ssize_t size = readlink("/proc/self/exe", buffer, sizeof(buffer) - 1);
    if (size < 0) {
        error = std::string("failed to resolve /proc/self/exe: ") + std::strerror(errno);
        return {};
    }
    return std::string(buffer, static_cast<std::size_t>(size));
}

int CollectMainModule(struct dl_phdr_info *info, size_t, void *data) {
    auto *out = static_cast<std::vector<LinuxLoadSegment> *>(data);
    if (info->dlpi_name != nullptr && info->dlpi_name[0] != '\0') {
        return 0;  // Only the main executable (empty name).
    }
    for (size_t i = 0; i < info->dlpi_phnum; ++i) {
        const auto &header = info->dlpi_phdr[i];
        if (header.p_type != PT_LOAD) continue;
        if (header.p_memsz == 0) continue;
        LinuxLoadSegment segment;
        segment.address = static_cast<patch::Address>(info->dlpi_addr + header.p_vaddr);
        segment.size = static_cast<std::size_t>(header.p_memsz);
        segment.readable = (header.p_flags & PF_R) != 0;
        segment.writable = (header.p_flags & PF_W) != 0;
        segment.executable = (header.p_flags & PF_X) != 0;
        out->push_back(segment);
    }
    return 1;
}

}  // namespace

LinuxProcessMemory::LinuxProcessMemory() = default;

bool LinuxProcessMemory::EnsureDiscovered(std::string &error) const {
    if (discovered_) {
        error.clear();
        return true;
    }
    executablePath_ = ReadExecutablePath(error);
    if (executablePath_.empty()) {
        discoverError_ = error;
        return false;
    }
    segments_.clear();
    dl_iterate_phdr(CollectMainModule, &segments_);
    if (segments_.empty()) {
        error = "dl_iterate_phdr() did not report PT_LOAD segments for the main executable";
        discoverError_ = error;
        return false;
    }
    imageBase_ = segments_.front().address;
    for (const auto &segment : segments_) {
        imageBase_ = std::min(imageBase_, segment.address);
    }
    discovered_ = true;
    error.clear();
    return true;
}

bool LinuxProcessMemory::Read(patch::Address address, std::uint8_t *buffer,
                              std::size_t size, std::string &error) const {
    if (address == 0 || buffer == nullptr || size == 0) {
        error = "Linux read requires an address, buffer, and non-zero size";
        return false;
    }
    // process_vm_readv fails safely on unmapped pages instead of crashing,
    // which is required when scanning discontiguous ELF regions.
    const pid_t pid = getpid();
    struct iovec local {
        buffer, size
    };
    struct iovec remote {
        reinterpret_cast<void *>(static_cast<std::uintptr_t>(address)), size
    };
    const ssize_t copied = process_vm_readv(pid, &local, 1, &remote, 1, 0);
    if (copied >= 0 && static_cast<std::size_t>(copied) == size) {
        return true;
    }
    const int vmError = errno;
    const int fd = open("/proc/self/mem", O_RDONLY | O_CLOEXEC);
    if (fd >= 0) {
        const ssize_t fallback =
            pread(fd, buffer, size, static_cast<off_t>(static_cast<std::uintptr_t>(address)));
        const int fallbackError = errno;
        close(fd);
        if (fallback >= 0 && static_cast<std::size_t>(fallback) == size) {
            return true;
        }
        std::ostringstream stream;
        stream << "Linux read failed at 0x" << std::hex << address << std::dec
               << " size=" << size << ": " << std::strerror(vmError)
               << " (fallback: " << std::strerror(fallbackError) << ")";
        error = stream.str();
        return false;
    }
    std::ostringstream stream;
    stream << "Linux read failed at 0x" << std::hex << address << std::dec
           << " size=" << size << ": " << std::strerror(vmError);
    error = stream.str();
    return false;
}

bool LinuxProcessMemory::Write(patch::Address address, const std::uint8_t *data,
                               std::size_t size, std::string &error) {
    if (address == 0 || data == nullptr || size == 0) {
        error = "Linux write requires an address, data, and non-zero size";
        return false;
    }
    const long pageSizeLong = sysconf(_SC_PAGESIZE);
    if (pageSizeLong <= 0) {
        error = "sysconf(_SC_PAGESIZE) failed";
        return false;
    }
    const auto pageSize = static_cast<std::size_t>(pageSizeLong);
    const auto raw = static_cast<std::uintptr_t>(address);
    const uintptr_t begin = AlignDown(raw, pageSize);
    const uintptr_t end = AlignUp(raw + size, pageSize);

    struct PageState {
        uintptr_t page = 0;
        int protection = 0;
    };
    std::vector<PageState> pages;
    for (uintptr_t page = begin; page < end; page += pageSize) {
        int protection = 0;
        if (!QueryProtection(page, protection)) {
            error = "Linux write could not query page protection; refusing to mutate";
            for (auto it = pages.rbegin(); it != pages.rend(); ++it) {
                mprotect(reinterpret_cast<void *>(it->page), pageSize, it->protection);
            }
            return false;
        }
        pages.push_back({page, protection});
        if (mprotect(reinterpret_cast<void *>(page), pageSize,
                     protection | PROT_WRITE) != 0) {
            error = std::string("mprotect(add-write) failed: ") + std::strerror(errno);
            for (auto it = pages.rbegin(); it != pages.rend(); ++it) {
                if (it->page == page) continue;
                mprotect(reinterpret_cast<void *>(it->page), pageSize, it->protection);
            }
            return false;
        }
    }

    std::memcpy(reinterpret_cast<void *>(raw), data, size);
#if defined(__x86_64__) || defined(__aarch64__)
    __builtin___clear_cache(reinterpret_cast<char *>(raw),
                            reinterpret_cast<char *>(raw + size));
#endif

    bool restoreOk = true;
    std::string restoreError;
    for (auto it = pages.rbegin(); it != pages.rend(); ++it) {
        if (mprotect(reinterpret_cast<void *>(it->page), pageSize, it->protection) != 0) {
            restoreOk = false;
            restoreError = std::strerror(errno);
        }
    }
    if (!restoreOk) {
        error = std::string("mprotect(restore) failed: ") + restoreError;
        return false;
    }
    return true;
}

bool LinuxProcessMemory::ReadCString(patch::Address address, std::size_t maxSize,
                                     std::string &value, std::string &error) const {
    value.clear();
    if (maxSize == 0) {
        error = "Linux string read requires a non-zero maximum size";
        return false;
    }
    for (std::size_t index = 0; index < maxSize; ++index) {
        std::uint8_t byte = 0;
        if (!Read(address + index, &byte, 1, error)) {
            return false;
        }
        if (byte == 0) return true;
        value.push_back(static_cast<char>(byte));
    }
    error = "string is not null-terminated within the requested size";
    return false;
}

std::optional<patch::MemoryRegion> LinuxProcessMemory::MainModule(
    std::string &error) const {
    // Single-region legacy view: the first executable PT_LOAD segment. Never
    // fabricate a contiguous span across unmapped ELF gaps here; use
    // MainModuleRegions() for safe scanning.
    auto regions = MainModuleRegions(patch::RegionPurpose::ExecutableSearch, error);
    if (regions.empty()) {
        if (error.empty()) error = "main executable has no executable PT_LOAD segment";
        return std::nullopt;
    }
    return regions.front();
}

std::vector<patch::MemoryRegion> LinuxProcessMemory::MainModuleRegions(
    patch::RegionPurpose purpose, std::string &error) const {
    if (!EnsureDiscovered(error)) {
        return {};
    }
    std::vector<patch::MemoryRegion> regions;
    for (const auto &segment : segments_) {
        bool include = false;
        switch (purpose) {
            case patch::RegionPurpose::ExecutableSearch:
                include = segment.executable;
                break;
            case patch::RegionPurpose::ReadOnlySearch:
                include = segment.readable && !segment.writable && !segment.executable;
                break;
            case patch::RegionPurpose::Writable:
                include = segment.writable;
                break;
        }
        if (!include) continue;
        std::ostringstream name;
        name << "elf-pt-load:"
             << (segment.readable ? "R" : "-") << (segment.writable ? "W" : "-")
             << (segment.executable ? "X" : "-");
        regions.push_back(patch::MemoryRegion{segment.address, segment.size, name.str()});
    }
    error.clear();
    return regions;
}

std::optional<patch::Address> LinuxProcessMemory::ResolveSymbol(
    const std::string &symbol, std::string &error) const {
    if (symbol.empty()) {
        error = "symbol name must not be empty";
        return std::nullopt;
    }
    void *address = dlsym(RTLD_DEFAULT, symbol.c_str());
    if (address == nullptr) {
        const char *detail = dlerror();
        error = "symbol not found: " + symbol;
        if (detail != nullptr) {
            error += " (";
            error += detail;
            error += ")";
        }
        return std::nullopt;
    }
    return static_cast<patch::Address>(reinterpret_cast<std::uintptr_t>(address));
}

}  // namespace eu4dll::linux_platform
