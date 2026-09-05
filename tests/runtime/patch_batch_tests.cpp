#include "runtime/patch/branch_resolver.h"
#include "runtime/patch/byte_buffer_memory.h"
#include "runtime/patch/patch_batch.h"
#include "runtime/patch/patch_runtime.h"

#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

namespace {

using eu4dll::patch::Address;
using eu4dll::patch::ByteBufferMemory;

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

// Fault-injecting wrapper: delegates storage to ByteBufferMemory but fails
// every Write at or after `failFromWrite` (1-based). With failFromWrite=2 the
// first commit write succeeds, the second fails, and the rollback write also
// fails, which exercises the distinct rollback diagnostic.
class FailingMemory final : public eu4dll::patch::Memory {
public:
    FailingMemory(std::vector<std::uint8_t> bytes, Address base, int failFromWrite)
        : inner_(std::move(bytes), base), failFromWrite_(failFromWrite) {}

    bool Read(Address address, std::uint8_t *buffer, std::size_t size,
              std::string &error) const override {
        return inner_.Read(address, buffer, size, error);
    }
    bool Write(Address address, const std::uint8_t *data, std::size_t size,
               std::string &error) override {
        ++writes_;
        if (writes_ >= failFromWrite_) {
            error = "injected write failure";
            return false;
        }
        return inner_.Write(address, data, size, error);
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
    // Fail the second commit write but allow the rollback write through by
    // failing exactly once: use a large threshold and a memory that only
    // fails the second write. Here failFromWrite=2 fails write #2; the
    // rollback is write #3 which also fails, so use a dedicated path below.
    // Instead verify rollback success with a memory that fails once: emulate
    // by failing write #2 only via a custom counter reset. Simplest: fail
    // write #2, allow #3+ by failing only when writes_==2.
    class FailSecondOnlyMemory final : public eu4dll::patch::Memory {
    public:
        FailSecondOnlyMemory(std::vector<std::uint8_t> bytes, Address base)
            : inner_(std::move(bytes), base) {}
        bool Read(Address address, std::uint8_t *buffer, std::size_t size,
                  std::string &error) const override {
            return inner_.Read(address, buffer, size, error);
        }
        bool Write(Address address, const std::uint8_t *data, std::size_t size,
                   std::string &error) override {
            ++writes_;
            if (writes_ == 2) {
                error = "injected write failure";
                return false;
            }
            return inner_.Write(address, data, size, error);
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
    // First write succeeds, second fails, and the rollback write also fails
    // because every write after the first is rejected.
    FailingMemory memory({0xAA, 0xBB, 0xCC, 0xDD}, 0x5000, 2);
    eu4dll::patch::PatchBatch batch(memory);
    batch.Add(RawAt("AA BB", 0x11));
    batch.Add(RawAt("CC DD", 0x22));
    const auto result = batch.Commit();
    Require(!result, "batch must fail");
    Require(result.diagnostic.operation == eu4dll::patch::PatchOperation::Rollback,
            "rollback-write failure must surface a rollback diagnostic");
}

void TestEmptyBatchFails() {
    ByteBufferMemory memory({0xAA}, 0x6000);
    eu4dll::patch::PatchBatch batch(memory);
    Require(!batch.Preflight(), "empty batch preflight must fail");
    Require(!batch.Commit(), "empty batch commit must fail");
}

}  // namespace

int main() {
    TestBatchSuccess();
    TestExpectedByteMismatchWritesNothing();
    TestOverlappingWritesRejected();
    TestRollbackRestoresFirstWrite();
    TestRollbackFailureIsDistinct();
    TestEmptyBatchFails();
    std::cout << "patch batch tests passed" << std::endl;
    return 0;
}
