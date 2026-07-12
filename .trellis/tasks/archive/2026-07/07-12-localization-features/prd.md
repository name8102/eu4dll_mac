# Migrate save and localization features

## Goal

Deepen save filename, localized search, East Asian name ordering, UTF-8
localization loading, and date formatting into focused feature modules.

## Requirements

- Save filename hooks reuse escaped-text behavior.
- Localized search owns normalization, pinyin/initial generation, caching,
  exact/fuzzy matching, and ranking.
- East Asian name ordering owns culture classification, ordering, and separator
  policy.
- UTF-8 localization loading and date formatting remain separate features.
- cpp-pinyin and EU4 object layouts do not leak into unrelated modules.

## Acceptance Criteria

- [ ] Save filename conversion paths use the shared escaped-text module.
- [ ] Localized search has Chinese, initials, polyphonic, ranking, and cache
      tests.
- [ ] Name ordering has table-driven culture and separator tests.
- [ ] The former localization module no longer mixes unrelated feature policy.
- [ ] All migrated hooks use the patch runtime and fixed target facts.
- [ ] Existing user-visible behavior is preserved or explicitly documented.

## Dependencies

Requires 07-12-patch-runtime, 07-12-macos-target-profile, and
07-12-escaped-text.

