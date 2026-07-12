#include "pinyinHelper.h"

#include <cpp-pinyin/G2pglobal.h>
#include <cpp-pinyin/Pinyin.h>

struct PinyinHelper::Implementation {
    Pinyin::Pinyin converter;
};

PinyinHelper &PinyinHelper::getInstance() {
    static PinyinHelper instance;
    return instance;
}

PinyinHelper::PinyinHelper() : implementation_(std::make_unique<Implementation>()) {}
PinyinHelper::~PinyinHelper() = default;

void PinyinHelper::SetDictionaryPath(const std::string &path) {
    Pinyin::setDictionaryPath(path);
}

std::vector<std::string> PinyinHelper::ReadingsFor(
        const std::string &utf8Character) {
    return implementation_->converter.getDefaultPinyin(
        utf8Character, Pinyin::ManTone::Style::NORMAL, true, false);
}
