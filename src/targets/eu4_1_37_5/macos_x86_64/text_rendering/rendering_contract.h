#pragma once

#include "runtime/patch/patch_runtime.h"
#include "targets/eu4_1_37_5/macos_x86_64/target_facts.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace eu4dll::targets::eu4_1_37_5::macos_x86_64::text_rendering {

enum class PatchId : std::size_t {
    ScreenPreprocess,
    ScreenLineBreak,
    ScreenGlyphLoop,
    TexturePreprocess,
    TextureLineBreak,
    TextureGlyphLoop,
    LayoutHeight,
    LayoutWidth,
    LayoutRequiredSize,
    LayoutActualRealGlyph,
    LayoutActualRequiredSize,
    LayoutForceWrap,
    LayoutHistory,
    LayoutEllipsis,
    LayoutDisableTruncation,
    MapAddNameSpacing,
    MapAddNameUppercase,
    MapAddNameGlyphCount,
    MapFillVertexGlyph,
    MapFillVertexMeasure,
    MapCurveGlyph,
    MapCurveReset,
    MapCurveLength,
    MapCurveLogicalSizeFirst,
    MapCurveLogicalSizeSecond,
    MapAddNudgedNames,
    Render3dGlyphLoop,
    Render3dPreprocess,
    Count,
};

struct ContinuationFact {
    const char *name;
    std::ptrdiff_t offset;
};

struct RenderingPatchDescriptor {
    PatchId id;
    const char *factId;
    const char *feature;
    const HookSite *site;
    patch::MutationKind mutationKind;
    patch::CallWidth callWidth;
    std::size_t mutationWidth;
    std::ptrdiff_t expectedOffset;
    std::vector<std::uint8_t> expectedOriginalBytes;
    std::vector<std::uint8_t> expectedMask;
    std::vector<std::uint8_t> mutationBytes;
    std::vector<ContinuationFact> continuations;
    bool optimizeNakedHook;
};

inline constexpr std::size_t kRenderingPatchCount =
        static_cast<std::size_t>(PatchId::Count);

[[nodiscard]] const std::array<RenderingPatchDescriptor,
                               kRenderingPatchCount> &descriptors();
[[nodiscard]] const RenderingPatchDescriptor &descriptor(PatchId id);
[[nodiscard]] patch::PatchDescription make_patch_description(
        PatchId id, patch::Address mutationTarget = 0);

} // namespace eu4dll::targets::eu4_1_37_5::macos_x86_64::text_rendering
