#pragma once

#include "features/localized_search/localized_search.h"

#include <memory>
#include <string>
#include <vector>

// Target-facing adapter. cpp-pinyin is deliberately hidden in the .cpp file.
class PinyinHelper final : public eu4dll::features::localized_search::PinyinProvider {
public:
    static PinyinHelper &getInstance();

    PinyinHelper(const PinyinHelper &) = delete;
    PinyinHelper &operator=(const PinyinHelper &) = delete;

    std::vector<std::string> ReadingsFor(const std::string &utf8Character) override;
    static void SetDictionaryPath(const std::string &path);

private:
    struct Implementation;

    PinyinHelper();
    ~PinyinHelper();

    std::unique_ptr<Implementation> implementation_;
};
