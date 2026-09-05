# Design

The migration keeps one canonical runtime with platform and target adapters. Sequencing is chosen for fault isolation: text-layout establishes byte/glyph measurement; UI text and tooltips build on it; localization establishes the UTF-8 content path. Map rendering then proceeds as its own high-risk branch. Input and pinyin can use the stable localization baseline without depending on map hooks.

Bootstrap should assemble independently preflightable feature groups and preserve feature gates until final bisection/soak. The final parity task owns inventory reconciliation and integration; if it discovers a nontrivial missing feature, create a small follow-up rather than turning the final audit into another monolith.
