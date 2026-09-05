# Implementation order

1. Complete and keep stable `09-05-linux-port-foundation`.
2. Execute text-layout -> main-text -> tooltip-text -> localization-utf8, running the child real-game gate after each.
3. From the stable localization baseline: map-text -> text3D; input-ime -> clipboard-paste/save-filename; pinyin-search.
4. Execute parity-integration only after all children are complete.
5. Preserve independent feature gates until the final task has used them for full-stack bisection and soak.

Do not batch several child tasks into one unreviewable implementation change merely because all tasks were created together.
