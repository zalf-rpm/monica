# Issues to look at

Findings that need a scientific decision rather than a code fix. Each entry:
what was observed, how to reproduce it, and the open question.

---

## 1. Emergence depends on a knife-edge soil moisture condition

*Found 2026-10-29 while checking the result changes from 3.7.0 to 3.7.1.*

### Observation

With `EmergenceMoistureControlOn` (switched on in the muencheberg_v140
simulations), the temperature sum towards emergence (stage 1 → 2) only
grows on days when the soil moisture of the top layer is between 20% of the
capillary water above the wilting point and field capacity
(`src/core/crop-module.cpp`, `emergenceCondition`, around line 1405):

```cpp
emergenceCondition =
    soilMoisture_m3 > ((0.2 * vc_CapillaryWater) + permanentWiltingPoint) &&
    soilMoisture_m3 <= fieldCapacity;
```

In wet autumns the top layer hovers right around field capacity, so on many
days the condition flips between true and false by a few thousandths of
m³/m³. Whether a winter crop reaches its temperature sum before winter -
and emerges in autumn instead of in spring - then depends on single days.
Small changes elsewhere in the model are enough to tip it.

### Example: muencheberg_v140 `sim21`, winter rye 2004

3.7.1 contains a bug fix in the soil moisture module: `eReducer1`
(`SoilMoisture::get_EReducer_1`, `src/core/soilmoisture.cpp`) now uses the
top layer's soil moisture as already updated on that day instead of the
value from the start of the day (monica commit `6efe4ecd`). It changes soil
evaporation slightly. Verified by building `6efe4ecd` with only that line
reverted: then the result is the 3.7.0 one again.

Top layer in autumn 2003 (field capacity 0.2292; "WET" = above field
capacity, the day doesn't count):

| date | 3.7.0: moisture | counts? | 3.7.1: moisture | counts? |
|---|---|---|---|---|
| 2003-10-01 | 0.2300 | no (WET) | 0.2270 | yes |
| 2003-10-05 | 0.2300 | no | 0.2300 | no |
| 2003-10-15 | 0.2240 | yes | 0.2240 | yes |
| 2003-10-25 | 0.2840 | no | 0.2840 | no |
| 2003-11-15 | 0.2120 | yes | 0.2120 | yes |
| 2003-11-24 | 0.2270 | yes | 0.2270 | yes → **emerges** |

Consequences:

| | 3.7.0 | 3.7.1 |
|---|---|---|
| emergence | 2004-03-16 | 2003-11-24 |
| development | stage 2 until June, stage 5 in August | stage 3 mid-April, stage 5 mid-June |
| maximum LAI | ~22 | ~13 |
| yield 2004 | 3030 kg/ha | 10250 kg/ha |

### It isn't a single case

In the 48 muencheberg_v140 simulations (3.7.0 → 3.7.1), almost all yield
changes above ~10% come with a shifted emergence date:

- winter rye 2004 in 8 more simulations (sim18, 19, 24, 25, 28, 32, 36, 43):
  emergence 2004-02-19 → 2003-11-24 (sim18: 2003-11-16), yield about +22%
- spring barley 2000, sim33: emergence 2000-05-23 → 2000-06-16, yield -47%
- spring barley 2000 (sim12, 37, 40), silage maize 2019 (sim26, 30, 34, 35,
  38), winter wheat 1992/2010: emergence shifted by a few days, ±10-27%

Silage maize 2011 (sim6, 7, 11, 14, 15, 17, 23, 34, 46-48: -11 to -20%)
emerges on the same day (or one day later) in both versions - there the
difference must come from a later process; not investigated yet.

### Open questions (for the scientists)

1. Is "top layer not wetter than field capacity, decided day by day" the
   intended emergence condition? In a wet autumn it blocks emergence on most
   days although the seed bed is just at field capacity. Alternatives could
   be a tolerance above field capacity, an average over several days or a
   deeper layer, or a different upper limit (e.g. saturation, or rely on
   `EmergenceFloodingControlOn` for "too wet").
2. Should a winter crop that hasn't emerged before winter behave as it does
   now (3.7.0 sim21: emergence in mid-March, then delayed development, LAI up
   to 22, low yield)? An LAI of 22 is implausible - possibly a separate issue
   with winter crops emerging in spring (vernalisation?).
3. Silage maize 2011: why the -11 to -20% yield changes without a different
   emergence date?

### How to reproduce

- 3.7.0 vs 3.7.1 results: `tests/run.py` with both builds (e.g.
  `nix build 'git+file://<repo>?rev=<commit>&submodules=1'` for
  `3.7.0.cr_math` and `3.7.1.improved_merge_climate_data`), then
  `tests/compare.py`; or the CI artifacts (`results-nix-linux`) of the
  "Build and test" workflow runs for both commits.
- Daily top-layer values: in muencheberg_v140's `sim_files/sim21.json`, add
  to the `"daily"` output after `"FrostRed",`:

  ```json
  "Surface", "Precip", ["Mois", [1, 1]], ["Fc", [1, 1]], ["Pwp", [1, 1]], ["STemp", [1, 1]],
  ```

  and compare 2003-09 to 2004-08 (`Stage`, `LAI`, `Yield`, `Mois_1` vs
  `Fc_1`).
