#include "pinyinHelper.h"

#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

namespace {

void Require(bool condition, const char *message) {
    if (!condition) {
        std::cerr << "FAILED: " << message << '\n';
        std::exit(1);
    }
}

bool Contains(const std::vector<std::string> &forms, const std::string &value) {
    return std::find(forms.begin(), forms.end(), value) != forms.end();
}

} // namespace

int main() {
    PinyinHelper::SetDictionaryPath(EU4DLL_PINYIN_DICTIONARY);
    auto &provider = PinyinHelper::getInstance();
    const auto raw = provider.ReadingsFor("重");
    Require(Contains(raw, "chong"), "adapter should expose raw first reading");
    Require(Contains(raw, "zhong"), "adapter should expose raw polyphonic reading");
    const auto forms = eu4dll::features::localized_search::GenerateForms("重庆", provider);
    Require(Contains(forms, "chongqing"), "portable search should build full pinyin");
    Require(Contains(forms, "cq"), "portable search should build initials");
    Require(Contains(forms, "zhongqing"), "portable search should combine polyphonic forms");
    Require(Contains(forms, "zq"), "portable search should build polyphonic initials");
    return 0;
}
