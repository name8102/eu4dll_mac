#pragma once

namespace eu4dll::targets::eu4_1_37_5::macos_x86_64::hook_symbols {

[[nodiscard]] bool ResolveRequiredSymbols();

} // namespace eu4dll::targets::eu4_1_37_5::macos_x86_64::hook_symbols

extern "C" {
extern void *g_CString_AppendCharAddress;
extern void *g_CString_AppendCharConstAddress;
}
