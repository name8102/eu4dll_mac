// Temporary game-scene diagnostics. Load only with EU4DLL_TRACE_MAP_NAMES=1.
#include "features/escaped_text/escaped_text.h"
#include "platform/linux/linux_process_memory.h"
#include "platform/linux/linux_executable_allocator.h"
#include "runtime/patch/patch_batch.h"
#include "targets/eu4_1_37_5/linux_x86_64/target_facts.h"
#include <array>
#include <algorithm>
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <string>
#include <vector>

namespace {
struct Vertex { float x,y,z,u,v; };
using Curve = void (*)(void *, const std::string &, const int *, int, Vertex *, int,
    const float *, const float *, const int *, int, int, bool, bool, const void *);
Curve original = nullptr;
std::atomic<unsigned> records{0};
void Trace(void *map, const std::string &name, const int *indices, int count,
    Vertex *vertices, int vertexCount, const float *line, const float *origin,
    const int *bounds, int width, int height, bool first, bool second, const void *font) {
    const auto utf8 = eu4dll::escaped_text::escaped_to_utf8(name).text;
    auto compact = utf8;
    compact.erase(std::remove(compact.begin(), compact.end(), ' '), compact.end());
    const bool capture = (compact == "黑羊" || compact == "白羊") && vertexCount > 0 &&
        vertexCount < 512 && records.fetch_add(1) < 32;
    std::vector<Vertex> before;
    if (capture) before.assign(vertices, vertices + vertexCount);
    original(map,name,indices,count,vertices,vertexCount,line,origin,bounds,width,height,first,second,font);
    if (!capture) return;
    static auto *mutex = new std::mutex;
    std::lock_guard<std::mutex> lock(*mutex);
    FILE *out = std::fopen("/tmp/eu4-curve-trace.jsonl", "a");
    if (!out) return;
    std::fprintf(out,"{\"name\":\"%s\",\"count\":%d,\"width\":%d,\"height\":%d,\"line\":[%g,%g,%g,%g],\"origin\":[%g,%g],\"bounds\":[%d,%d,%d,%d],\"before\":[",
        utf8.c_str(),vertexCount,width,height,line[0],line[1],line[2],line[3],origin[0],origin[1],bounds[0],bounds[1],bounds[2],bounds[3]);
    for (int i=0;i<vertexCount;++i) std::fprintf(out,"%s[%g,%g,%g]",i?",":"",before[i].x,before[i].y,before[i].z);
    std::fprintf(out,"],\"after\":[");
    for (int i=0;i<vertexCount;++i) std::fprintf(out,"%s[%g,%g,%g]",i?",":"",vertices[i].x,vertices[i].y,vertices[i].z);
    std::fprintf(out,"]}\n");
    std::fclose(out);
}
__attribute__((constructor)) void Install() {
    if (!std::getenv("EU4DLL_TRACE_MAP_NAMES")) return;
    namespace patch = eu4dll::patch;
    namespace target = eu4dll::targets::eu4_1_37_5::linux_x86_64;
    eu4dll::linux_platform::LinuxProcessMemory memory;
    patch::PatchDescription d;
    d.feature="diagnostic.curve-text";
    d.target=target::kDiagnosticTargetId;
    d.location.scope=patch::SearchScope::Symbol("_ZN18CGenerateNamesWork11AddNameAreaEPKiiiiiRK13SProvinceData",0x1250);
    d.location.pattern="E8 79 07 00 00 48 83 C4 40";
    d.expected=patch::ExpectedBytes{0,{0xE8,0x79,0x07,0,0},{}};
    d.mutation.kind=patch::MutationKind::Call;
    d.mutation.callWidth=patch::CallWidth::FiveBytes;
    d.mutation.target=reinterpret_cast<std::uintptr_t>(Trace);
    const auto site=patch::PatchRuntime(memory).Preflight(d);
    if (!site) return;
    original=reinterpret_cast<Curve>(site.diagnostic.matchAddress+5+0x779);
    static auto *allocator=new eu4dll::linux_platform::LinuxNearAllocator;
    patch::PatchBatch batch(memory,allocator);
    batch.Add(d);
    const auto result=batch.Commit();
    std::fprintf(stderr,"eu4dll curve trace: %s\n",patch::FormatDiagnostic(result.diagnostic).c_str());
}
}
