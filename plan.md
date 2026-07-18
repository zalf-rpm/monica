# Plan

1. Implement new procedural soil temperature module in `src/core/soiltemperature_simple.h/.cpp`.
   - **Done**
2. Update model wiring and call sites for soil temperature.
   - **Done**
3. Resolve compile/toolchain issue and verify build.
   - **Done**
4. Harden editor build tasks.
   - **Done**
5. Convert requested module pairs to `_simple` mirrors and wire build/call sites.
   - **Done**
6. Next pass toward purely procedural interfaces for `_simple` modules.
   - Added procedural make/lifecycle/free-function APIs for all requested `_simple` modules.
   - Rewired `MonicaModel` orchestration path to those procedural APIs.
   - Began method-removal pass; currently still incomplete for large internal method sets, especially in `crop-module_simple`, `soilmoisture_simple`, `soilorganic_simple`, and parts of `soilcolumn_simple`/`soiltransport_simple`.
   - Soil-organic wrapper procedures now read/write the struct directly for serialization, crop wiring, irrigation, incorporation, and getters.
   - Build remains green (`monica_lib`).
   - **In progress**
