#pragma once

#include <cstddef>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace eu4dll::features::localized_search {

class PinyinProvider {
public:
    virtual ~PinyinProvider() = default;
    // Returns raw readings for one UTF-8 character. Combination, initials,
    // polyphonic expansion, normalization, and caching belong to SearchEngine.
    virtual std::vector<std::string> ReadingsFor(const std::string &utf8Character) = 0;
};

struct MatchResult {
    bool matched = false;
    int distance = 0;
};

std::string NormalizeQuery(const std::string &text);
std::vector<std::string> GenerateForms(const std::string &utf8Text,
                                       PinyinProvider &pinyin);
int SubstringScore(const std::string &text, const std::string &query);

class SearchEngine {
public:
    explicit SearchEngine(PinyinProvider &pinyin) : pinyin_(pinyin) {}

    MatchResult Match(bool exactOnly, const std::string &query,
                      const std::string &originalName,
                      const std::string &cleanedName);
    std::size_t CacheSize() const { return cache_.size(); }
    void ClearCache() { cache_.clear(); }

private:
    const std::vector<std::string> &FormsFor(const std::string &originalName);

    PinyinProvider &pinyin_;
    std::unordered_map<std::string, std::vector<std::string>> cache_;
};

} // namespace eu4dll::features::localized_search
