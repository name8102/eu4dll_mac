#include "runtime/patch/branch_resolver.h"
#include "runtime/patch/byte_buffer_memory.h"
#include "runtime/patch/patch_batch.h"
#include "runtime/patch/patch_runtime.h"

#include <sys/mman.h>

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>

namespace {

using eu4dll::patch::Address;
using eu4dll::patch::ByteBufferMemory;
using eu4dll::patch::WriteResult;

void Require(bool condition, const char *message) {
    if (!condition) {
        std::cerr << "FAILED: " << message << '\n';
        std::exit(1);
    }
}

eu4dll::patch::PatchDescription RawAt(const char *pattern, std::uint8_t replacement,
                                      std::ptrdiff_t offset = 0) {
    eu4dll::patch::PatchDescription description;
    description.feature = "batch-feature";
    description.target = "batch-target";
    description.location.pattern = pattern;
    description.mutation.kind = eu4dll::patch::MutationKind::RawBytes;
    description.mutation.offset = offset;
    description.mutation.bytes = {replacement};
    return description;
}

eu4dll::patch::PatchDescription JumpAt(const char *pattern, Address target) {
    eu4dll::patch::PatchDescription description;
    description.feature = "batch-jump";
    description.target = "batch-target";
    description.location.pattern = pattern;
    description.mutation.kind = eu4dll::patch::MutationKind::Jump;
    description.mutation.target = target;
    return description;
}

// Delegating wrapper: storage lives in ByteBufferMemory; writes fail cleanly
// (nothing mutated) at or after `failFromWrite` (1-based).
class FailingMemory final : public eu4dll::patch::Memory {
public:
    FailingMemory(std::vector<std::uint8_t> bytes, Address base, int failFromWrite)
        : inner_(std::move(bytes), base), failFromWrite_(failFromWrite) {}

    bool Read(Address address, std::uint8_t *buffer, std::size_t size,
              std::string &error) const override {
        return inner_.Read(address, buffer, size, error);
    }
    WriteResult Write(Address address, const std::uint8_t *data, std::size_t size) override {
        ++writes_;
        if (writes_ >= failFromWrite_) {
            WriteResult result;
            result.error = "injected write failure";
            return result;
        }
        return inner_.Write(address, data, size);
    }
    bool ReadCString(Address address, std::size_t maxSize, std::string &value,
                     std::string &error) const override {
        return inner_.ReadCString(address, maxSize, value, error);
    }
    std::optional<eu4dll::patch::MemoryRegion> MainModule(
        std::string &error) const override {
        return inner_.MainModule(error);
    }
    std::vector<eu4dll::patch::MemoryRegion> MainModuleRegions(
        eu4dll::patch::RegionPurpose purpose, std::string &error) const override {
        return inner_.MainModuleRegions(purpose, error);
    }
    std::optional<Address> ResolveSymbol(const std::string &symbol,
                                         std::string &error) const override {
        return inner_.ResolveSymbol(symbol, error);
    }
    const std::vector<std::uint8_t> &Bytes() const { return inner_.Bytes(); }

private:
    ByteBufferMemory inner_;
    int failFromWrite_ = 0;
    int writes_ = 0;
};

void TestBatchSuccess() {
    ByteBufferMemory memory({0xAA, 0xBB, 0xCC, 0xDD}, 0x1000);
    eu4dll::patch::PatchBatch batch(memory);
    batch.Add(RawAt("AA BB", 0x11));
    batch.Add(RawAt("CC DD", 0x22));
    const auto preflight = batch.Preflight();
    Require(static_cast<bool>(preflight), "batch preflight must pass");
    Require(memory.Bytes() == std::vector<std::uint8_t>({0xAA, 0xBB, 0xCC, 0xDD}),
            "preflight must not mutate");
    const auto result = batch.Commit();
    Require(static_cast<bool>(result), "batch commit must succeed");
    Require(result.installations.size() == 2, "commit must publish per-patch results");
    Require(memory.Bytes()[0] == 0x11 && memory.Bytes()[2] == 0x22,
            "batch writes must all apply");
}

void TestExpectedByteMismatchWritesNothing() {
    ByteBufferMemory memory({0xAA, 0xBB, 0xCC}, 0x2000);
    eu4dll::patch::PatchBatch batch(memory);
    batch.Add(RawAt("AA BB", 0x11));
    auto bad = RawAt("BB CC", 0x22);
    bad.expected = eu4dll::patch::ExpectedBytes{0, {0x00}, {}};
    batch.Add(std::move(bad));
    const auto before = memory.Bytes();
    const auto result = batch.Commit();
    Require(!result, "expected-byte mismatch must fail the batch");
    Require(memory.Bytes() == before, "failed batch must write nothing");
    Require(result.installations.empty(),
            "continuations must not publish on failed commit");
}

void TestOverlappingWritesRejected() {
    ByteBufferMemory memory({0xAA, 0xBB, 0xCC}, 0x3000);
    eu4dll::patch::PatchBatch batch(memory);
    auto first = RawAt("AA BB CC", 0x11);
    first.mutation.bytes = {0x11, 0x22};
    auto second = RawAt("AA BB CC", 0x33);
    second.mutation.offset = 1;
    second.mutation.bytes = {0x33};
    batch.Add(std::move(first));
    batch.Add(std::move(second));
    const auto before = memory.Bytes();
    Require(!batch.Preflight(), "overlapping preflight must fail");
    Require(!batch.Commit(), "overlapping commit must fail");
    Require(memory.Bytes() == before, "overlapping batch must write nothing");
}

void TestRollbackRestoresFirstWrite() {
    // Fail exactly the second commit write; the rollback write delegates and
    // the restore is read-back confirmed.
    class FailSecondOnlyMemory final : public eu4dll::patch::Memory {
    public:
        FailSecondOnlyMemory(std::vector<std::uint8_t> bytes, Address base)
            : inner_(std::move(bytes), base) {}
        bool Read(Address address, std::uint8_t *buffer, std::size_t size,
                  std::string &error) const override {
            return inner_.Read(address, buffer, size, error);
        }
        WriteResult Write(Address address, const std::uint8_t *data,
                          std::size_t size) override {
            ++writes_;
            if (writes_ == 2) {
                WriteResult result;
                result.error = "injected write failure";
                return result;
            }
            return inner_.Write(address, data, size);
        }
        bool ReadCString(Address address, std::size_t maxSize, std::string &value,
                         std::string &error) const override {
            return inner_.ReadCString(address, maxSize, value, error);
        }
        std::optional<eu4dll::patch::MemoryRegion> MainModule(
            std::string &error) const override {
            return inner_.MainModule(error);
        }
        std::vector<eu4dll::patch::MemoryRegion> MainModuleRegions(
            eu4dll::patch::RegionPurpose purpose, std::string &error) const override {
            return inner_.MainModuleRegions(purpose, error);
        }
        std::optional<Address> ResolveSymbol(const std::string &symbol,
                                             std::string &error) const override {
            return inner_.ResolveSymbol(symbol, error);
        }
        const std::vector<std::uint8_t> &Bytes() const { return inner_.Bytes(); }

    private:
        ByteBufferMemory inner_;
        int writes_ = 0;
    };

    FailSecondOnlyMemory memory({0xAA, 0xBB, 0xCC, 0xDD}, 0x4000);
    eu4dll::patch::PatchBatch batch(memory);
    batch.Add(RawAt("AA BB", 0x11));
    batch.Add(RawAt("CC DD", 0x22));
    const auto result = batch.Commit();
    Require(!result, "second-write failure must fail the batch");
    Require(memory.Bytes()[0] == 0xAA, "rollback must restore the first write");
    Require(result.installations.empty(), "failed batch publishes nothing");
}

void TestRollbackFailureIsDistinct() {
    // Every write at or after the second fails cleanly, so the rollback
    // restore also fails and read-back cannot confirm it.
    FailingMemory memory({0xAA, 0xBB, 0xCC, 0xDD}, 0x5000, 2);
    eu4dll::patch::PatchBatch batch(memory);
    batch.Add(RawAt("AA BB", 0x11));
    batch.Add(RawAt("CC DD", 0x22));
    const auto result = batch.Commit();
    Require(!result, "batch must fail");
    Require(result.diagnostic.operation == eu4dll::patch::PatchOperation::Rollback,
            "unconfirmed rollback must surface a rollback diagnostic");
}

void TestEmptyBatchFails() {
    ByteBufferMemory memory({0xAA}, 0x6000);
    eu4dll::patch::PatchBatch batch(memory);
    Require(!batch.Preflight(), "empty batch preflight must fail");
    Require(!batch.Commit(), "empty batch commit must fail");
}

// ---- P0 regression: trampoline lifetime follows restore confirmation ----
//
// A failed Write may still have mutated the target (bytes copied before a
// protection-restore failure). If the subsequent rollback cannot be
// read-back confirmed, the game site may still jump at the trampoline, so
// the batch must NOT munmap it. This fault path is unreachable in normal
// game soak testing, hence the targeted fault injection here.

class HonestAllocator final : public eu4dll::patch::ExecutableCodeAllocator {
public:
    explicit HonestAllocator(Address page) : page_(page) {}

    std::optional<Address> AllocateNear(Address anchor, std::size_t size,
                                        std::string &error) override {
        (void)size;
        if (allocated_) {
            error = "test allocator serves a single trampoline";
            return std::nullopt;
        }
        std::int32_t relative = 0;
        if (!eu4dll::patch::EncodeRel32(anchor, page_, relative)) {
            error = "test page is not rel32-reachable from the anchor";
            return std::nullopt;
        }
        allocated_ = true;
        return page_;
    }

    bool MakeExecutable(Address address, std::size_t size, std::string &error) override {
        if (address != page_) {
            error = "test allocator owns a single page";
            return false;
        }
        if (mprotect(reinterpret_cast<void *>(static_cast<std::uintptr_t>(page_)), size,
                     PROT_READ | PROT_EXEC) != 0) {
            error = "mprotect failed in test";
            return false;
        }
        return true;
    }

    void Release(Address address, std::size_t size) override {
        (void)size;
        if (address == page_ && allocated_ && !released_) {
            released_ = true;
        }
    }

    bool released() const { return released_; }

private:
    Address page_ = 0;
    bool allocated_ = false;
    bool released_ = false;
};

// Write #1 delegates (success). Write #2 copies the payload into the backing
// store but reports protection-restore failure (bytes ARE live). Write #3+
// (rollback attempts) touch nothing and fail, so read-back keeps showing the
// payload and no restore can be confirmed.
class PoisonedMemory final : public eu4dll::patch::Memory {
public:
    PoisonedMemory(std::vector<std::uint8_t> bytes, Address base)
        : inner_(std::move(bytes), base) {}

    bool Read(Address address, std::uint8_t *buffer, std::size_t size,
              std::string &error) const override {
        return inner_.Read(address, buffer, size, error);
    }
    WriteResult Write(Address address, const std::uint8_t *data,
                      std::size_t size) override {
        ++writes_;
        if (writes_ == 1) return inner_.Write(address, data, size);
        if (writes_ == 2) {
            auto stored = inner_.Write(address, data, size);
            Require(stored.ok(), "poison setup must store the payload");
            WriteResult result;
            result.bytesWritten = true;
            result.protectionRestored = false;
            result.error = "injected protection-restore failure";
            return result;
        }
        WriteResult result;
        result.error = "injected rollback failure";
        return result;
    }
    bool ReadCString(Address address, std::size_t maxSize, std::string &value,
                     std::string &error) const override {
        return inner_.ReadCString(address, maxSize, value, error);
    }
    std::optional<eu4dll::patch::MemoryRegion> MainModule(
        std::string &error) const override {
        return inner_.MainModule(error);
    }
    std::vector<eu4dll::patch::MemoryRegion> MainModuleRegions(
        eu4dll::patch::RegionPurpose purpose, std::string &error) const override {
        return inner_.MainModuleRegions(purpose, error);
    }
    std::optional<Address> ResolveSymbol(const std::string &symbol,
                                         std::string &error) const override {
        return inner_.ResolveSymbol(symbol, error);
    }
    const std::vector<std::uint8_t> &Bytes() const { return inner_.Bytes(); }

private:
    ByteBufferMemory inner_;
    int writes_ = 0;
};

void TestUnconfirmedRollbackRetainsTrampoline() {
    void *page = mmap(nullptr, 4096, PROT_READ | PROT_WRITE,
                      MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    Require(page != MAP_FAILED, "test mmap must succeed");
    const auto pageAddress =
        static_cast<Address>(reinterpret_cast<std::uintptr_t>(page));
    const Address base = pageAddress + 0x100000;  // near the served page

    PoisonedMemory memory({0xAA, 0xBB, 0xCC, 0xCC, 0xDD, 0xEE, 0x90, 0x90}, base);
    HonestAllocator allocator(pageAddress);
    eu4dll::patch::PatchBatch batch(memory, &allocator);
    batch.Add(RawAt("AA BB", 0x11));
    batch.Add(JumpAt("CC DD EE", base + 0x100000000ULL));  // forces a trampoline
    const auto result = batch.Commit();

    Require(!result, "poisoned write must fail the batch");
    Require(result.diagnostic.operation == eu4dll::patch::PatchOperation::Rollback,
            "unconfirmed restore must surface a rollback diagnostic");
    Require(result.diagnostic.message.find("retained") != std::string::npos,
            "diagnostic must report the intentionally retained trampoline");
    Require(!allocator.released(),
            "UNCONFIRMED rollback must NOT munmap a possibly-referenced trampoline");
    Require(result.installations.empty(), "failed batch publishes nothing");
    // Honest reporting: the raw site still holds the payload because the
    // rollback writes were dropped by the fault.
    Require(memory.Bytes()[0] == 0x11, "unrestored payload must stay visible");

    munmap(page, 4096);  // test hygiene; production code intentionally leaks
}

// Mirror policy: a cleanly failed write mutates nothing, the restore is
// confirmed, and the trampoline IS released (no leak on the safe path).
class CleanFailMemory final : public eu4dll::patch::Memory {
public:
    CleanFailMemory(std::vector<std::uint8_t> bytes, Address base)
        : inner_(std::move(bytes), base) {}

    bool Read(Address address, std::uint8_t *buffer, std::size_t size,
              std::string &error) const override {
        return inner_.Read(address, buffer, size, error);
    }
    WriteResult Write(Address address, const std::uint8_t *data,
                      std::size_t size) override {
        ++writes_;
        if (writes_ == 2) {
            WriteResult result;  // bytesWritten=false: nothing mutated
            result.error = "injected clean failure";
            return result;
        }
        return inner_.Write(address, data, size);
    }
    bool ReadCString(Address address, std::size_t maxSize, std::string &value,
                     std::string &error) const override {
        return inner_.ReadCString(address, maxSize, value, error);
    }
    std::optional<eu4dll::patch::MemoryRegion> MainModule(
        std::string &error) const override {
        return inner_.MainModule(error);
    }
    std::vector<eu4dll::patch::MemoryRegion> MainModuleRegions(
        eu4dll::patch::RegionPurpose purpose, std::string &error) const override {
        return inner_.MainModuleRegions(purpose, error);
    }
    std::optional<Address> ResolveSymbol(const std::string &symbol,
                                         std::string &error) const override {
        return inner_.ResolveSymbol(symbol, error);
    }
    const std::vector<std::uint8_t> &Bytes() const { return inner_.Bytes(); }

private:
    ByteBufferMemory inner_;
    int writes_ = 0;
};

void TestConfirmedRollbackReleasesTrampoline() {
    void *page = mmap(nullptr, 4096, PROT_READ | PROT_WRITE,
                      MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    Require(page != MAP_FAILED, "test mmap must succeed");
    const auto pageAddress =
        static_cast<Address>(reinterpret_cast<std::uintptr_t>(page));
    const Address base = pageAddress + 0x100000;

    CleanFailMemory memory({0xAA, 0xBB, 0xCC, 0xCC, 0xDD, 0xEE, 0x90, 0x90}, base);
    HonestAllocator allocator(pageAddress);
    eu4dll::patch::PatchBatch batch(memory, &allocator);
    batch.Add(RawAt("AA BB", 0x11));
    batch.Add(JumpAt("CC DD EE", base + 0x100000000ULL));
    const auto result = batch.Commit();

    Require(!result, "clean write failure must fail the batch");
    Require(result.diagnostic.operation == eu4dll::patch::PatchOperation::WriteMutation,
            "confirmed rollback keeps the original write-mutation diagnostic");
    Require(allocator.released(),
            "CONFIRMED rollback must release the trampoline (no safe-path leak)");
    Require(memory.Bytes()[0] == 0xAA, "confirmed rollback restores the first write");

    munmap(page, 4096);
}

}  // namespace

int main() {
    TestBatchSuccess();
    TestExpectedByteMismatchWritesNothing();
    TestOverlappingWritesRejected();
    TestRollbackRestoresFirstWrite();
    TestRollbackFailureIsDistinct();
    TestEmptyBatchFails();
    TestUnconfirmedRollbackRetainsTrampoline();
    TestConfirmedRollbackReleasesTrampoline();
    std::cout << "patch batch tests passed" << std::endl;
    return 0;
}
