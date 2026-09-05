#include "bootstrap/linux/bootstrap_linux.h"

#include <cstdio>

namespace {

void ShowError(const char *title, const char *message) {
    std::fprintf(stderr, "eu4dll_linux [%s] %s\n", title, message);
}

}  // namespace

// Linux preload entry (LD_PRELOAD). Flow: discover host -> validate exact
// target -> preflight all base sites/resources -> atomically install base ->
// emit a consolidated result. Unknown ELF binaries fail closed before any
// mutation. No later rendering/input/localization features are enabled here.
__attribute__((constructor)) static void Eu4DllLinuxEntry() {
    std::fprintf(stderr, "eu4dll_linux [bootstrap] preload entry\n");
    std::string error;
    std::string report;
    if (!eu4dll::linux_bootstrap::BootstrapLinuxBase(error, report)) {
        ShowError("startup failure", error.c_str());
        return;
    }
    std::fprintf(stderr, "eu4dll_linux [bootstrap] %s\n", report.c_str());
    std::fprintf(stderr,
                 "eu4dll_linux [bootstrap] supported EU4 ELF accepted; base installed.\n");
}
