#pragma once

#include "memory.h"
#include "runtime/diagnostics/patch_diagnostic.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace eu4dll::patch {

enum class SearchScopeKind {
    MainModule,
    Symbol,
};

struct SearchScope {
    SearchScopeKind kind = SearchScopeKind::MainModule;
    std::string symbol;
    std::size_t maxSize = 0;

    static SearchScope MainModule();
    static SearchScope Symbol(std::string name, std::size_t maxSearchSize);
};

struct PatternLocation {
    std::string pattern;
    std::vector<std::string> referencedStrings;
    SearchScope scope = SearchScope::MainModule();
    bool requireUnique = true;
};

struct ExpectedBytes {
    std::ptrdiff_t offset = 0;
    std::vector<std::uint8_t> bytes;
    std::vector<std::uint8_t> mask;
};

enum class MutationKind {
    RawBytes,
    Jump,
    Call,
};

enum class CallWidth {
    Auto,
    FiveBytes,
    SixBytes,
};

struct Mutation {
    MutationKind kind = MutationKind::RawBytes;
    std::ptrdiff_t offset = 0;
    std::vector<std::uint8_t> bytes;
    Address target = 0;
    CallWidth callWidth = CallWidth::Auto;
};

struct Continuation {
    std::string name;
    std::ptrdiff_t offset = 0;
};

struct HookOptimization {
    bool enabled = false;
    Address hookAddress = 0;
    std::size_t maxScanSize = 200;
};

struct PatchDescription {
    std::string feature;
    std::string target;
    PatternLocation location;
    std::optional<ExpectedBytes> expected;
    Mutation mutation;
    std::vector<Continuation> continuations;
    HookOptimization optimization;
};

struct LocateResult {
    PatchDiagnostic diagnostic;
    Address address = 0;

    explicit operator bool() const { return diagnostic.success; }
};

struct InstallationResult {
    PatchDiagnostic diagnostic;
    std::unordered_map<std::string, Address> continuations;

    explicit operator bool() const { return diagnostic.success; }
    Address ContinuationAddress(const std::string &name) const;
};

class ResolvedSiteProvider {
public:
    virtual ~ResolvedSiteProvider() = default;
    virtual std::optional<Address> Resolve(const std::string &siteId,
                                           std::string &error) const = 0;
};

class PatchRuntime {
public:
    explicit PatchRuntime(Memory &memory) : memory_(memory) {}
    void SetResolvedSiteProvider(const ResolvedSiteProvider *provider) {
        siteProvider_ = provider;
    }

    LocateResult Locate(const PatternLocation &location, const std::string &feature,
                        const std::string &target) const;
    InstallationResult Preflight(const PatchDescription &description) const;
    InstallationResult Install(const PatchDescription &description);

    PatchDiagnostic ApplyMutation(Address address, const Mutation &mutation,
                                  const std::string &feature, const std::string &target);
    PatchDiagnostic OptimizeIndirectBranches(Address hookAddress, std::size_t maxScanSize,
                                               const std::string &feature,
                                               const std::string &target);

private:
    Memory &memory_;
    const ResolvedSiteProvider *siteProvider_ = nullptr;
};

} // namespace eu4dll::patch
