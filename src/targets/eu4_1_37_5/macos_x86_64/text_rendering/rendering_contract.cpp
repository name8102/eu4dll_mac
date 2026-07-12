#include "targets/eu4_1_37_5/macos_x86_64/text_rendering/rendering_contract.h"

#include <iterator>
#include <stdexcept>
#include <utility>

namespace eu4dll::targets::eu4_1_37_5::macos_x86_64::text_rendering {
namespace {

using MK = patch::MutationKind;
using CW = patch::CallWidth;

RenderingPatchDescriptor jump(PatchId id, const char *factId,
                              const char *feature, const HookSite &site,
                              std::ptrdiff_t expectedOffset,
                              std::vector<std::uint8_t> expected,
                              std::vector<ContinuationFact> continuations = {},
                              std::vector<std::uint8_t> expectedMask = {}) {
    if (expectedMask.empty()) expectedMask.assign(expected.size(), 0xFF);
    return {id, factId, feature, &site, MK::Jump, CW::FiveBytes, 5,
            expectedOffset, std::move(expected), std::move(expectedMask), {},
            std::move(continuations), true};
}

RenderingPatchDescriptor call(PatchId id, const char *factId,
                              const char *feature, const HookSite &site,
                              std::ptrdiff_t expectedOffset,
                              bool optimizeNakedHook = false) {
    return {id, factId, feature, &site, MK::Call, CW::FiveBytes, 5,
            expectedOffset, {0xE8, 0, 0, 0, 0}, {0xFF, 0, 0, 0, 0}, {}, {},
            optimizeNakedHook};
}

RenderingPatchDescriptor bytes(PatchId id, const char *factId,
                               const char *feature, const HookSite &site,
                               std::ptrdiff_t expectedOffset,
                               std::vector<std::uint8_t> expected,
                               std::vector<std::uint8_t> mutation) {
    const auto width = mutation.size();
    std::vector<std::uint8_t> mask(expected.size(), 0xFF);
    return {id, factId, feature, &site, MK::RawBytes, CW::Auto, width,
            expectedOffset, std::move(expected), std::move(mask),
            std::move(mutation), {}, false};
}

} // namespace

const std::array<RenderingPatchDescriptor, kRenderingPatchCount> &descriptors() {
    static const HookSite curveTextSecondCall{
        map_text::kCurveText4.pattern, map_text::kCurveText4SecondCallOffset};
    static const std::array<RenderingPatchDescriptor, kRenderingPatchCount> facts{{
        jump(PatchId::ScreenPreprocess, "main_text.kRenderToScreen1",
             "rendering.screen.preprocess", main_text::kRenderToScreen1, 0,
             {0x42, 0x0F, 0xB6, 0x14, 0x1F},
             {{"return", main_text::kRenderToScreen1.continuationOffset},
              {"bypass", main_text::kRenderToScreen1.bypassOffset}}),
        jump(PatchId::ScreenLineBreak, "main_text.kRenderToScreen2",
             "rendering.screen.line-break", main_text::kRenderToScreen2, 0,
             {0x66, 0x41, 0x83, 0x7E, 0x06},
             {{"return", main_text::kRenderToScreen2.continuationOffset},
              {"bypass", main_text::kRenderToScreen2.bypassOffset}}),
        jump(PatchId::ScreenGlyphLoop, "main_text.kRenderToScreen3",
             "rendering.screen.glyph-loop", main_text::kRenderToScreen3, 0,
             {0x41, 0x0F, 0xB6, 0x04, 0x04},
             {{"return", main_text::kRenderToScreen3.continuationOffset},
              {"bypass", main_text::kRenderToScreen3.bypassOffset}}),
        jump(PatchId::TexturePreprocess, "texture_text.kRenderToTexture1",
             "rendering.texture.preprocess", texture_text::kRenderToTexture1, 0,
             {0x0F, 0xB6, 0x00, 0x49, 0x8B},
             {{"bypass", texture_text::kRenderToTexture1.bypassOffset}}),
        jump(PatchId::TextureLineBreak, "texture_text.kRenderToTexture2",
             "rendering.texture.line-break", texture_text::kRenderToTexture2, 0,
             {0x66, 0x83, 0x7B, 0x06, 0x00},
             {{"return", texture_text::kRenderToTexture2.continuationOffset},
              {"bypass", texture_text::kRenderToTexture2.bypassOffset}}),
        jump(PatchId::TextureGlyphLoop, "texture_text.kRenderToTexture3",
             "rendering.texture.glyph-loop", texture_text::kRenderToTexture3, 0,
             {0x0F, 0xB6, 0x00, 0x4D, 0x8B},
             {{"bypass", texture_text::kRenderToTexture3.bypassOffset}}),
        jump(PatchId::LayoutHeight, "text_layout.kHeight",
             "rendering.layout.height", text_layout::kHeight, 0,
             {0x0F, 0xB6, 0x00, 0x48, 0x8B},
             {{"bypass", text_layout::kHeight.bypassOffset}}),
        jump(PatchId::LayoutWidth, "text_layout.kWidth",
             "rendering.layout.width", text_layout::kWidth, 0,
             {0x48, 0x8B, 0x9C, 0xF7, 0xE8},
             {{"return", text_layout::kWidth.continuationOffset},
              {"bypass", text_layout::kWidth.bypassOffset}}),
        jump(PatchId::LayoutRequiredSize, "text_layout.kRequiredSize",
             "rendering.layout.required-size", text_layout::kRequiredSize, 0,
             {0x0F, 0xB6, 0x00, 0x49, 0x8B},
             {{"bypass", text_layout::kRequiredSize.bypassOffset}}),
        jump(PatchId::LayoutActualRealGlyph, "text_layout.kActualRealRequiredSize1",
             "rendering.layout.actual-real-required-size.glyph",
             text_layout::kActualRealRequiredSize1, 0,
             {0x0F, 0xB6, 0x00, 0x49, 0x8B},
             {{"bypass", text_layout::kActualRealRequiredSize1.bypassOffset}}),
        jump(PatchId::LayoutActualRequiredSize, "text_layout.kActualRequiredSize",
             "rendering.layout.actual-required-size",
             text_layout::kActualRequiredSize, 0,
             {0x0F, 0xB6, 0x00, 0x48, 0x8B},
             {{"bypass", text_layout::kActualRequiredSize.bypassOffset}}),
        bytes(PatchId::LayoutForceWrap, "text_layout.kActualRequiredSizeCall",
              "rendering.layout.force-wrap", text_layout::kActualRequiredSizeCall, 0,
              {0x41, 0x0F, 0xBF, 0x44, 0x24, 0x06},
              std::vector<std::uint8_t>(std::begin(text_layout::kForceWrapBytes),
                                        std::end(text_layout::kForceWrapBytes))),
        jump(PatchId::LayoutHistory, "text_layout.kActualRealRequiredSize2",
             "rendering.layout.actual-real-required-size.history",
             text_layout::kActualRealRequiredSize2, 0,
             {0x8B, 0x9D, 0x4C, 0xFF, 0xFF},
             {{"bypass", text_layout::kActualRealRequiredSize2.bypassOffset}}),
        jump(PatchId::LayoutEllipsis, "text_layout.kActualRealRequiredSize3",
             "rendering.layout.actual-real-required-size.ellipsis",
             text_layout::kActualRealRequiredSize3, 0,
             {0x48, 0x83, 0xBD, 0xE8, 0xFE},
             {{"bypass", text_layout::kActualRealRequiredSize3.bypassOffset}}),
        bytes(PatchId::LayoutDisableTruncation,
              "text_layout.kActualRealRequiredSizeBranch",
              "rendering.layout.disable-truncation",
              text_layout::kActualRealRequiredSizeBranch,
              text_layout::kActualRealRequiredSizeBranch.mutationOffset,
              {0x0F, 0x82},
              std::vector<std::uint8_t>(std::begin(text_layout::kDisableTruncationBytes),
                                        std::end(text_layout::kDisableTruncationBytes))),
        jump(PatchId::MapAddNameSpacing, "map_text.kAddNameArea1",
             "rendering.map.add-name-area.spacing", map_text::kAddNameArea1,
             map_text::kAddNameArea1.mutationOffset,
             {0x88, 0x85, 0x28, 0xFF, 0xFF},
             {{"return", map_text::kAddNameArea1.continuationOffset},
              {"bypass", map_text::kAddNameArea1.bypassOffset}}),
        call(PatchId::MapAddNameUppercase, "map_text.kAddNameArea2",
             "rendering.map.add-name-area.uppercase", map_text::kAddNameArea2,
             map_text::kAddNameArea2.mutationOffset, true),
        jump(PatchId::MapAddNameGlyphCount, "map_text.kAddNameArea3",
             "rendering.map.add-name-area.glyph-count", map_text::kAddNameArea3, 0,
             {0x0F, 0xB6, 0x00, 0x49, 0x8B},
             {{"bypass", map_text::kAddNameArea3.bypassOffset}}),
        jump(PatchId::MapFillVertexGlyph, "map_text.kFillVertexBuffer1",
             "rendering.map.fill-vertex-buffer.glyph",
             map_text::kFillVertexBuffer1, 0,
             {0x0F, 0xB6, 0x00, 0x49, 0x8B},
             {{"bypass", map_text::kFillVertexBuffer1.bypassOffset}}),
        jump(PatchId::MapFillVertexMeasure, "map_text.kFillVertexBuffer2",
             "rendering.map.fill-vertex-buffer.measure",
             map_text::kFillVertexBuffer2, 0,
             {0x0F, 0xB6, 0x00, 0x4D, 0x8B},
             {{"bypass", map_text::kFillVertexBuffer2.bypassOffset}}),
        jump(PatchId::MapCurveGlyph, "map_text.kCurveText1",
             "rendering.map.curve-text.glyph", map_text::kCurveText1, 0,
             {0x0F, 0xB6, 0x00, 0x4D, 0x8B},
             {{"bypass", map_text::kCurveText1.bypassOffset}}),
        jump(PatchId::MapCurveReset, "map_text.kCurveText2",
             "rendering.map.curve-text.reset", map_text::kCurveText2,
             map_text::kCurveText2.mutationOffset,
             {0x41, 0xBE, 0x00, 0x00, 0x00},
             {{"bypass", map_text::kCurveText2.bypassOffset}}),
        jump(PatchId::MapCurveLength, "map_text.kCurveText3",
             "rendering.map.curve-text.length", map_text::kCurveText3,
             map_text::kCurveText3.mutationOffset, {0x4C, 0x89, 0xEF, 0, 0},
             {{"bypass", map_text::kCurveText3.bypassOffset}},
             {0xFF, 0xFF, 0xFF, 0, 0}),
        call(PatchId::MapCurveLogicalSizeFirst, "map_text.kCurveText4.first-call",
             "rendering.map.curve-text.logical-size.first", map_text::kCurveText4,
             map_text::kCurveText4.mutationOffset),
        call(PatchId::MapCurveLogicalSizeSecond, "map_text.kCurveText4.second-call",
             "rendering.map.curve-text.logical-size.second", curveTextSecondCall,
             curveTextSecondCall.mutationOffset),
        jump(PatchId::MapAddNudgedNames, "map_text.kAddNudgedNames",
             "rendering.map.add-nudged-names", map_text::kAddNudgedNames,
             map_text::kAddNudgedNames.mutationOffset,
             {0x0F, 0xB6, 0x00, 0x49, 0x8B},
             {{"bypass", map_text::kAddNudgedNames.bypassOffset}}),
        jump(PatchId::Render3dGlyphLoop, "text_3d.kRender1",
             "rendering.3d.glyph-loop", text_3d::kRender1,
             text_3d::kRender1.mutationOffset,
             {0x0F, 0xB6, 0x00, 0x49, 0x8B},
             {{"bypass", text_3d::kRender1.bypassOffset}}),
        jump(PatchId::Render3dPreprocess, "text_3d.kRender2",
             "rendering.3d.preprocess", text_3d::kRender2, 0,
             {0x0F, 0xB6, 0x00, 0x48, 0x8B},
             {{"bypass", text_3d::kRender2.bypassOffset}}),
    }};
    return facts;
}

const RenderingPatchDescriptor &descriptor(PatchId id) {
    const auto index = static_cast<std::size_t>(id);
    if (index >= descriptors().size()) {
        throw std::out_of_range("invalid rendering patch id");
    }
    return descriptors()[index];
}

patch::PatchDescription make_patch_description(PatchId id,
                                               patch::Address mutationTarget) {
    const auto &fact = descriptor(id);
    patch::PatchDescription description;
    description.feature = fact.feature;
    description.target = kDiagnosticTargetId;
    description.location.pattern = fact.site->pattern;
    description.expected = patch::ExpectedBytes{
        fact.expectedOffset, fact.expectedOriginalBytes, fact.expectedMask};
    description.mutation.kind = fact.mutationKind;
    description.mutation.offset = fact.site->mutationOffset;
    description.mutation.target = mutationTarget;
    description.mutation.callWidth = fact.callWidth;
    description.mutation.bytes = fact.mutationBytes;
    for (const auto &continuation : fact.continuations) {
        description.continuations.push_back({continuation.name, continuation.offset});
    }
    description.optimization.enabled = fact.optimizeNakedHook;
    description.optimization.hookAddress =
        fact.optimizeNakedHook ? mutationTarget : 0;
    return description;
}

} // namespace eu4dll::targets::eu4_1_37_5::macos_x86_64::text_rendering
