#include "mach_process_memory.h"

#include <dlfcn.h>
#include <mach-o/dyld.h>
#include <mach-o/getsect.h>
#include <mach/mach.h>
#include <mach/mach_vm.h>
#include <mach/vm_prot.h>

#include <algorithm>
#include <cstring>
#include <sstream>
#include <vector>

namespace eu4dll::platform::macos {
namespace {

std::string MachError(const char *operation, kern_return_t result, patch::Address address) {
    std::ostringstream stream;
    stream << operation << " failed at 0x" << std::hex << address << std::dec
           << " with Mach error " << result;
    return stream.str();
}

} // namespace

bool MachProcessMemory::Read(patch::Address address, std::uint8_t *buffer, std::size_t size,
                             std::string &error) const {
    if (address == 0 || buffer == nullptr || size == 0) {
        error = "Mach read requires an address, buffer, and non-zero size";
        return false;
    }
    mach_vm_size_t bytesRead = 0;
    const kern_return_t result = mach_vm_read_overwrite(
        mach_task_self(), static_cast<mach_vm_address_t>(address), size,
        reinterpret_cast<mach_vm_address_t>(buffer), &bytesRead);
    if (result != KERN_SUCCESS) {
        error = MachError("mach_vm_read_overwrite", result, address);
        return false;
    }
    if (bytesRead != size) {
        error = "Mach read returned fewer bytes than requested";
        return false;
    }
    return true;
}

bool MachProcessMemory::Write(patch::Address address, const std::uint8_t *data,
                              std::size_t size, std::string &error) {
    if (address == 0 || data == nullptr || size == 0) {
        error = "Mach write requires an address, data, and non-zero size";
        return false;
    }
    const mach_port_t task = mach_task_self();
    const auto destination = static_cast<mach_vm_address_t>(address);
    kern_return_t result = mach_vm_protect(
        task, destination, size, FALSE,
        VM_PROT_READ | VM_PROT_WRITE | VM_PROT_COPY | VM_PROT_EXECUTE);
    if (result != KERN_SUCCESS) {
        error = MachError("mach_vm_protect(unprotect)", result, address);
        return false;
    }

    std::memcpy(reinterpret_cast<void *>(static_cast<std::uintptr_t>(address)), data, size);

    result = mach_vm_protect(task, destination, size, FALSE, VM_PROT_READ | VM_PROT_EXECUTE);
    if (result != KERN_SUCCESS) {
        error = MachError("mach_vm_protect(reprotect)", result, address);
        return false;
    }
    return true;
}

bool MachProcessMemory::ReadCString(patch::Address address, std::size_t maxSize,
                                    std::string &value, std::string &error) const {
    value.clear();
    if (maxSize == 0) {
        error = "Mach string read requires a non-zero maximum size";
        return false;
    }
    std::vector<std::uint8_t> buffer(maxSize);
    if (!Read(address, buffer.data(), buffer.size(), error)) {
        return false;
    }
    const auto terminator = std::find(buffer.begin(), buffer.end(), 0);
    if (terminator == buffer.end()) {
        error = "string is not null-terminated within the requested size";
        return false;
    }
    value.assign(buffer.begin(), terminator);
    return true;
}

std::optional<patch::MemoryRegion> MachProcessMemory::MainModule(std::string &error) const {
    const auto *header = reinterpret_cast<const mach_header_64 *>(_dyld_get_image_header(0));
    if (header == nullptr) {
        error = "dyld did not provide the main image header";
        return std::nullopt;
    }
    unsigned long size = 0;
    getsegmentdata(header, "__TEXT", &size);
    if (size == 0) {
        error = "main image has no readable __TEXT segment";
        return std::nullopt;
    }
    return patch::MemoryRegion{
        static_cast<patch::Address>(reinterpret_cast<std::uintptr_t>(header)),
        static_cast<std::size_t>(size), "main-module::__TEXT"};
}

std::optional<patch::Address> MachProcessMemory::ResolveSymbol(const std::string &symbol,
                                                              std::string &error) const {
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

} // namespace eu4dll::platform::macos
