#include "runtime/patch/branch_resolver.h"
#include "runtime/patch/byte_buffer_memory.h"

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>

namespace {

using eu4dll::patch::Address;

void Require(bool condition, const char *message) {
    if (!condition) {
        std::cerr << "FAILED: " << message << '\n';
        std::exit(1);
    }
}

class FakeAllocator final : public eu4dll::patch::ExecutableCodeAllocator {
public:
    explicit FakeAllocator(bool failAllocate = false, bool failProtect = false)
        : failAllocate_(failAllocate), failProtect_(failProtect) {}

    std::optional<Address> AllocateNear(Address anchor, std::size_t size,
                                        std::string &error) override {
        if (failAllocate_) {
            error = "injected allocation failure";
            return std::nullopt;
        }
        // Reachable heap page owned by the test; resolver writes the stub here.
        void *page = std::malloc(size);
        if (page == nullptr) {
            error = "malloc failed";
            return std::nullopt;
        }
        const auto address =
            static_cast<Address>(reinterpret_cast<std::uintptr_t>(page));
        allocations_.push_back({address, size, page});
        // Keep the allocation near the anchor for the reachability check when
        // possible; heap distance is not controllable, so tests use anchors
        // near the allocation or accept the resolver's defensive check.
        (void)anchor;
        return address;
    }

    bool MakeExecutable(Address address, std::size_t size, std::string &error) override {
        if (failProtect_) {
            error = "injected protect failure";
            return false;
        }
        madeExecutable_.push_back({address, size});
        return true;
    }

    void Release(Address address, std::size_t size) override {
        for (auto it = allocations_.begin(); it != allocations_.end(); ++it) {
            if (it->address == address) {
                std::free(it->page);
                allocations_.erase(it);
                released_.push_back(address);
                return;
            }
        }
        (void)size;
    }

    std::size_t liveCount() const { return allocations_.size(); }
    std::size_t releasedCount() const { return released_.size(); }

    ~FakeAllocator() override {
        for (const auto &allocation : allocations_) std::free(allocation.page);
    }

private:
    struct Allocation {
        Address address = 0;
        std::size_t size = 0;
        void *page = nullptr;
    };
    bool failAllocate_ = false;
    bool failProtect_ = false;
    std::vector<Allocation> allocations_;
    std::vector<std::pair<Address, std::size_t>> madeExecutable_;
    std::vector<Address> released_;
};

void TestDirectReachableAllocatesNothing() {
    FakeAllocator allocator;
    const Address instruction = 0x100000;
    const Address target = 0x100100;  // Well within rel32.
    std::string error;
    const auto resolved = eu4dll::patch::ResolveBranchTarget(
        instruction, target, eu4dll::patch::BranchKind::Jump, &allocator, error);
    Require(static_cast<bool>(resolved), "direct branch must resolve");
    Require(!resolved->usesTrampoline, "direct branch must not use a trampoline");
    Require(resolved->encodeTarget == target, "direct branch encodes the final target");
    Require(allocator.liveCount() == 0, "direct branch must allocate nothing");
}

void TestNullAllocatorFailsOutOfRange() {
    std::string error;
    const auto resolved = eu4dll::patch::ResolveBranchTarget(
        0x100000, 0x90000000ULL, eu4dll::patch::BranchKind::Call, nullptr, error);
    Require(!resolved, "out-of-range without allocator must fail");
    Require(!error.empty(), "out-of-range failure must carry a diagnostic");
}

void TestStubEncoding() {
    const Address target = 0x1122334455667788ULL;
    const auto stub = eu4dll::patch::BuildAbsoluteJumpStub(target);
    Require(stub.size() == 14, "stub must be 14 bytes");
    Require(stub[0] == 0xFF && stub[1] == 0x25, "stub must start with FF 25");
    Require(stub[2] == 0 && stub[3] == 0 && stub[4] == 0 && stub[5] == 0,
            "stub displacement must be zero");
    Address decoded = 0;
    std::memcpy(&decoded, stub.data() + 6, sizeof(decoded));
    Require(decoded == target, "stub must carry the absolute target");
}

void TestAllocationFailureReleasesNothing() {
    FakeAllocator allocator(true /*failAllocate*/);
    std::string error;
    const auto resolved = eu4dll::patch::ResolveBranchTarget(
        0x100000, 0x90000000ULL, eu4dll::patch::BranchKind::Jump, &allocator, error);
    Require(!resolved, "allocation failure must fail resolution");
    Require(allocator.liveCount() == 0 && allocator.releasedCount() == 0,
            "failed allocation must not leak");
}

void TestProtectFailureReleasesTrampoline() {
    FakeAllocator allocator(false, true /*failProtect*/);
    std::string error;
    const auto resolved = eu4dll::patch::ResolveBranchTarget(
        0x100000, 0x90000000ULL, eu4dll::patch::BranchKind::Jump, &allocator, error);
    Require(!resolved, "protect failure must fail resolution");
    Require(allocator.liveCount() == 0, "protect failure must release the page");
    Require(allocator.releasedCount() == 1, "protect failure must release exactly once");
}

}  // namespace

int main() {
    TestDirectReachableAllocatesNothing();
    TestNullAllocatorFailsOutOfRange();
    TestStubEncoding();
    TestAllocationFailureReleasesNothing();
    TestProtectFailureReleasesTrampoline();
    std::cout << "branch resolver tests passed" << std::endl;
    return 0;
}
