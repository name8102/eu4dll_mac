#include "targets/eu4_1_37_5/macos_x86_64/hook_symbols.h"

#include "platform/macos/symbol_lookup.h"
#include "targets/eu4_1_37_5/macos_x86_64/target_facts.h"

extern "C" {
void *g_CString_AppendCharAddress = nullptr;
void *g_CString_AppendCharConstAddress = nullptr;
}

namespace eu4dll::targets::eu4_1_37_5::macos_x86_64::hook_symbols {

bool ResolveRequiredSymbols() {
    g_CString_AppendCharAddress = platform::macos::ResolveLiveSymbol(
        "target-hooks.CString.append-char", kDiagnosticTargetId,
        symbols::kCStringAppendChar);
    g_CString_AppendCharConstAddress = platform::macos::ResolveLiveSymbol(
        "target-hooks.CString.append-c-string", kDiagnosticTargetId,
        symbols::kCStringAppendCString);
    return g_CString_AppendCharAddress != nullptr &&
           g_CString_AppendCharConstAddress != nullptr;
}

} // namespace eu4dll::targets::eu4_1_37_5::macos_x86_64::hook_symbols
