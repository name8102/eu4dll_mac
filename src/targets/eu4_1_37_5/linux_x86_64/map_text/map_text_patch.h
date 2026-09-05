#pragma once

#include "runtime/patch/memory.h"
#include "runtime/patch/patch_batch.h"

#include <cstdint>
#include <string>
#include <vector>

namespace eu4dll::targets::eu4_1_37_5::linux_x86_64::map_text {

// ---- Pure escape-walk logic (deterministic, unit-tested) ----
//
// Both C++ bridges below walk escaped text with the same rule; these
// helpers own the rule so tests cover it without the game.
inline constexpr std::uint8_t kEscapeFirst = 0x10;
inline constexpr std::uint8_t kEscapeLast = 0x13;

// Advance one logical character from `offset`: escape markers consume their
// two payload bytes, anything else consumes one. Clamped to `size`.
std::uint32_t AdvanceLogicalChar(const std::uint8_t *bytes, std::uint32_t size,
                                 std::uint32_t offset);
// Count logical glyphs in a byte range using the same rule.
std::uint32_t CountLogicalGlyphs(const std::uint8_t *bytes, std::uint32_t size);

// ---- Game CString bridges (installed as call replacements) ----
//
// Resolved through Memory::ResolveSymbol at install (fail-closed). Plain
// C++ functions with game C ABIs; no naked code.
using CStringGetSize = std::uint32_t (*)(const void *);
using CStringIndex = const char *(*)(const void *, std::uint32_t);
using CStringMutableIndex = char *(*)(void *, std::uint32_t);

// Escape-preserving ToUpper: uppercases ASCII, skips the two payload bytes
// after 0x10..0x13 (the stock ToUpper rewrites them and drifts indices).
// The following call's `xor eax,eax` proves the return value is ignored,
// hence void.
void ToUpperPreservingEscapes(void *text);
// Logical glyph count for CurveText loop bounds/allocation (same units on
// both sides: allocation length and vertex writing agree by construction).
std::uint32_t CurveTextGetGlyphCount(const void *text);

// ---- Hook slots (published from install results before commit) ----
patch::Address &CStringAppendCharSlot();
patch::Address &CStringAppendStringSlot();
patch::Address &CStringGetSizeSlot();
patch::Address &CStringIndexSlot();
patch::Address &CStringMutableIndexSlot();
patch::Address &FillPreprocessingReturnSlot();
patch::Address &FillDrawingReturnSlot();
patch::Address &SpacingReturnSlot();
patch::Address &SpacingFinalSlot();
patch::Address &AddNameAreaGlyphReturnSlot();
patch::Address &AddNudgedNamesGlyphReturnSlot();
patch::Address &CurveDrawingReturnSlot();

// ---- Naked hooks (System V x86-64, see ABI_NOTES.md) ----
void NakedFillVertexBufferPreprocessing();
void NakedFillVertexBufferDrawing();
void NakedAddNameAreaSpacing();
void NakedAddNameAreaGlyphCount();
void NakedAddNudgedNamesGlyphCount();
void NakedCurveTextDrawing();

struct MapHookTargets {
    patch::Address fillPreprocessing = 0;
    patch::Address fillDrawing = 0;
    patch::Address spacing = 0;
    patch::Address toUpperCall = 0;
    patch::Address addNameAreaGlyph = 0;
    patch::Address addNudgedNamesGlyph = 0;
    patch::Address curveDrawing = 0;
    patch::Address curveGetSizeFirst = 0;
    patch::Address curveGetSizeSecond = 0;
};

// Description factories per cluster (targets supplied by the caller so
// fixture tests use reachable dummies; live installs use real addresses).
patch::PatchDescription FillPreprocessingDescription(patch::Address hookTarget);
patch::PatchDescription FillDrawingDescription(patch::Address hookTarget);
patch::PatchDescription SpacingDescription(patch::Address hookTarget);
patch::PatchDescription ToUpperCallDescription(patch::Address converterTarget);
patch::PatchDescription AddNameAreaGlyphDescription(patch::Address hookTarget);
patch::PatchDescription AddNudgedNamesGlyphDescription(patch::Address hookTarget);
patch::PatchDescription CurveDrawingDescription(patch::Address hookTarget);
// GetSize call redirects sharing the length-calls window (offset 0/+11).
std::vector<patch::PatchDescription> CurveCallRedirects(patch::Address target);

std::vector<patch::PatchDescription> FillVertexBufferDescriptions(
    const MapHookTargets &targets);
std::vector<patch::PatchDescription> AddNameAreaDescriptions(
    const MapHookTargets &targets);
std::vector<patch::PatchDescription> AddNudgedNamesDescriptions(
    const MapHookTargets &targets);
std::vector<patch::PatchDescription> CurveTextDescriptions(
    const MapHookTargets &targets);

// Per-cluster preflight/install (each install is one atomic batch).
// Install order is FillVertexBuffer -> AddNameArea -> AddNudgedNames ->
// CurveText; see InstallMapText for failure semantics.
patch::BatchResult PreflightFillVertexBuffer(patch::Memory &memory,
                                             patch::ExecutableCodeAllocator *a = nullptr);
patch::BatchResult InstallFillVertexBuffer(patch::Memory &memory,
                                           patch::ExecutableCodeAllocator *a = nullptr);
patch::BatchResult PreflightAddNameArea(patch::Memory &memory,
                                        patch::ExecutableCodeAllocator *a = nullptr);
patch::BatchResult InstallAddNameArea(patch::Memory &memory,
                                      patch::ExecutableCodeAllocator *a = nullptr);
patch::BatchResult PreflightAddNudgedNames(patch::Memory &memory,
                                           patch::ExecutableCodeAllocator *a = nullptr);
patch::BatchResult InstallAddNudgedNames(patch::Memory &memory,
                                         patch::ExecutableCodeAllocator *a = nullptr);
patch::BatchResult PreflightCurveText(patch::Memory &memory,
                                      patch::ExecutableCodeAllocator *a = nullptr);
patch::BatchResult InstallCurveText(patch::Memory &memory,
                                    patch::ExecutableCodeAllocator *a = nullptr);

// Whole-group dry run (zero writes/allocations across all clusters).
patch::BatchResult PreflightMapText(patch::Memory &memory,
                                    patch::ExecutableCodeAllocator *a = nullptr);
// Sequential cluster commits with per-cluster logging; stops at the first
// failing cluster so the active cluster is always diagnosable. Earlier
// clusters stay installed by design (bisection over all-or-nothing).
patch::BatchResult InstallMapText(patch::Memory &memory,
                                  patch::ExecutableCodeAllocator *a = nullptr);

}  // namespace eu4dll::targets::eu4_1_37_5::linux_x86_64::map_text
