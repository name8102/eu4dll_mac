#include "save_filenames.h"

#include "features/escaped_text/escaped_text.h"

#include <new>

namespace eu4dll::features::save_filenames {

std::string &ToDiskNameInPlace(std::string &escapedName) {
    escapedName = escaped_text::escaped_to_utf8(escapedName).text;
    return escapedName;
}

std::string &ToDisplayNameInPlace(std::string &utf8Name) {
    utf8Name = escaped_text::utf8_to_escaped(utf8Name).text;
    return utf8Name;
}

std::string DisplayCopy(const std::string &utf8Name) {
    return escaped_text::utf8_to_escaped(utf8Name).text;
}

void AppendDisplayCopy(std::string &destination, const std::string &utf8Name) {
    const std::string display = DisplayCopy(utf8Name);
    destination += display;
}

void ConstructDisplayCopy(const std::string *utf8Name, void *outputStorage) {
    if (utf8Name == nullptr || outputStorage == nullptr) return;
    new (outputStorage) std::string(DisplayCopy(*utf8Name));
}

void DestroyDisplayCopy(void *outputStorage) {
    if (outputStorage == nullptr) return;
    static_cast<std::string *>(outputStorage)->~basic_string();
}

} // namespace eu4dll::features::save_filenames
