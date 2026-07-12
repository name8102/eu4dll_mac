#include "east_asian_names.h"

#include <algorithm>
#include <array>

namespace eu4dll::features::east_asian_names {

bool IsEastAsianGroup(const std::string &cultureGroup, CultureMode mode) {
    static constexpr std::array<const char *, 7> vanilla{{
        "east_asian", "korean_g", "southeastasian_group", "japanese_g",
        "tibetan_group", "altaic", "evenks"}};
    static constexpr std::array<const char *, 11> bimillennium{{
        "east_asian", "korean_g", "japanese_g", "tibetan_group", "altaic",
        "evenks", "mongolic", "tartar", "buyeo_g", "thai_group", "ainu_g"}};
    const auto contains = [&cultureGroup](const auto &groups) {
        return std::find(groups.begin(), groups.end(), cultureGroup) != groups.end();
    };
    return mode == CultureMode::BimillenniumUniversalis
        ? contains(bimillennium) : contains(vanilla);
}

bool IsChineseCulture(const std::string &cultureGroup, const std::string &culture) {
    return cultureGroup == "east_asian" || culture == "miao" ||
           culture == "bai" || culture == "yi";
}

NamePolicy PolicyFor(const std::string &cultureGroup, const std::string &culture,
                     CultureMode mode) {
    if (!IsEastAsianGroup(cultureGroup, mode) && culture != "zhuang") return {};
    return {NameOrder::SurnameThenGiven,
            IsChineseCulture(cultureGroup, culture) ? "" : " "};
}

std::string Format(const std::string &givenName, const std::string &surname,
                   const NamePolicy &policy) {
    if (policy.order == NameOrder::SurnameThenGiven) {
        return surname + policy.separator + givenName;
    }
    return givenName + policy.separator + surname;
}

} // namespace eu4dll::features::east_asian_names
