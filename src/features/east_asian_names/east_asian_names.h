#pragma once

#include <string>

namespace eu4dll::features::east_asian_names {

enum class CultureMode { Vanilla, BimillenniumUniversalis };
enum class NameOrder { GivenThenSurname, SurnameThenGiven };

struct NamePolicy {
    NameOrder order = NameOrder::GivenThenSurname;
    std::string separator = " ";
};

bool IsEastAsianGroup(const std::string &cultureGroup, CultureMode mode);
bool IsChineseCulture(const std::string &cultureGroup, const std::string &culture);
NamePolicy PolicyFor(const std::string &cultureGroup, const std::string &culture,
                     CultureMode mode);
std::string Format(const std::string &givenName, const std::string &surname,
                   const NamePolicy &policy);

} // namespace eu4dll::features::east_asian_names
