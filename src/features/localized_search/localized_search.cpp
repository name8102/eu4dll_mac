#include "localized_search.h"

#include "features/escaped_text/escaped_text.h"

#include <algorithm>
#include <cctype>
#include <limits>
#include <set>

namespace eu4dll::features::localized_search {
namespace {

bool IsEscapedQuery(const std::string &query) {
    return query.find_first_of("\x10\x11\x12\x13") != std::string::npos;
}

std::vector<std::string> SplitUtf8(const std::string &text) {
    std::vector<std::string> characters;
    for (std::size_t index = 0; index < text.size();) {
        const auto byte = static_cast<unsigned char>(text[index]);
        std::size_t length = 1;
        if ((byte & 0xE0) == 0xC0) length = 2;
        else if ((byte & 0xF0) == 0xE0) length = 3;
        else if ((byte & 0xF8) == 0xF0) length = 4;
        length = std::min(length, text.size() - index);
        characters.push_back(text.substr(index, length));
        index += length;
    }
    return characters;
}

void GenerateCombinations(const std::vector<std::vector<std::string>> &readings,
                          std::size_t index, const std::string &full,
                          const std::string &initials,
                          std::set<std::string> &fullForms,
                          std::set<std::string> &initialForms) {
    if (index == readings.size()) {
        if (!full.empty()) fullForms.insert(full);
        if (!initials.empty()) initialForms.insert(initials);
        return;
    }
    for (const auto &reading : readings[index]) {
        const std::string normalized = NormalizeQuery(reading);
        std::string nextInitials = initials;
        if (!normalized.empty()) {
            nextInitials += normalized[0] >= 'a' && normalized[0] <= 'z'
                ? std::string(1, normalized[0]) : normalized;
        }
        GenerateCombinations(readings, index + 1, full + normalized,
                             nextInitials, fullForms, initialForms);
    }
}

} // namespace

std::string NormalizeQuery(const std::string &text) {
    std::string normalized;
    normalized.reserve(text.size());
    for (const unsigned char byte : text) {
        if (byte < 0x80) {
            normalized.push_back(static_cast<char>(std::tolower(byte)));
        } else {
            normalized.push_back(static_cast<char>(byte));
        }
    }
    for (std::size_t position = 0;
         (position = normalized.find("ü", position)) != std::string::npos;) {
        normalized.replace(position, sizeof("ü") - 1, "u");
        ++position;
    }
    return normalized;
}

std::vector<std::string> GenerateForms(const std::string &utf8Text,
                                       PinyinProvider &pinyin) {
    std::vector<std::vector<std::string>> readings;
    for (const auto &character : SplitUtf8(utf8Text)) {
        auto values = pinyin.ReadingsFor(character);
        if (values.empty()) values.push_back(character);
        readings.push_back(std::move(values));
    }
    std::set<std::string> full;
    std::set<std::string> initials;
    GenerateCombinations(readings, 0, {}, {}, full, initials);
    std::vector<std::string> forms(full.begin(), full.end());
    forms.insert(forms.end(), initials.begin(), initials.end());
    return forms;
}

int SubstringScore(const std::string &text, const std::string &query) {
    const auto position = text.find(query);
    if (position == std::string::npos) return -99;
    if (position == 0) return -2;
    if (position + query.size() == text.size()) return 1;
    return position < text.size() / 2 ? -1 : 0;
}

const std::vector<std::string> &SearchEngine::FormsFor(const std::string &originalName) {
    const auto found = cache_.find(originalName);
    if (found != cache_.end()) return found->second;

    auto forms = GenerateForms(escaped_text::escaped_to_utf8(originalName).text, pinyin_);
    std::sort(forms.begin(), forms.end());
    forms.erase(std::unique(forms.begin(), forms.end()), forms.end());
    return cache_.emplace(originalName, std::move(forms)).first->second;
}

MatchResult SearchEngine::Match(bool exactOnly, const std::string &query,
                                const std::string &originalName,
                                const std::string &cleanedName) {
    // Case folding is performed after decoding. Escaped payloads can contain
    // ASCII uppercase bytes that are part of a Han code point, not letters.
    const bool escapedQuery = IsEscapedQuery(query);
    const std::string normalizedQuery = NormalizeQuery(escapedQuery
        ? escaped_text::escaped_to_utf8(query).text : query);
    if (normalizedQuery.empty()) return {};

    if (escapedQuery || std::any_of(query.begin(), query.end(),
                                    [](unsigned char c) { return c >= 0x80; })) {
        // EU4's CleanForSorting/ToLower are byte-oriented and may already
        // have damaged cleanedName. Use the untouched display name for Han.
        const std::string normalizedName = NormalizeQuery(
            escaped_text::escaped_to_utf8(originalName).text);
        if (normalizedQuery == normalizedName) return {true, -3};
        if (exactOnly) return {};
        const int score = SubstringScore(normalizedName, normalizedQuery);
        return score == -99 ? MatchResult{} : MatchResult{true, score};
    }

    (void)cleanedName;

    int best = std::numeric_limits<int>::max();
    for (const auto &form : FormsFor(originalName)) {
        if (form == normalizedQuery) {
            if (exactOnly) return {true, -3};
            best = -3;
            continue;
        }
        if (!exactOnly) {
            const int score = SubstringScore(form, normalizedQuery);
            if (score != -99) best = std::min(best, score);
        }
    }
    return best == std::numeric_limits<int>::max()
        ? MatchResult{} : MatchResult{true, best};
}

} // namespace eu4dll::features::localized_search
