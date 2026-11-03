# Fixes for three C++ quirks: how much they change the results

Branch `fix-cpp-quirks` fixes the three bugs listed in `PERFORMANCE.md` (section A),
one commit each, on top of `master` 7830f948. This file records how much each fix
changes the results of the test suites (2026-11-03).

| | Commit | File | Fix |
|---|---|---|---|
| A1 | `6949c14f` | `src/core/soilorganic.cpp` | `fo_Urea` and `fo_Volatilisation` copied the top soil layer (`auto layer0/lay0 = soilColumn.at(0)`). Now a reference: volatilised NH3 is actually subtracted from the layer's NH4, and the AOM pools' `vo_DaysAfterApplication` advances. |
| A2 | `eeedcd19` | `src/core/crop-module.cpp` | The root density taper `(i_Layer - vc_RootingDepth) / (vc_RootingZone - vc_RootingDepth)` divided `size_t` by `size_t` - always 0. Now floating point division. |
| A3 | `1f35bce6` | `src/run/cultivation-method.cpp` | The automatic sowing temperature sum ran over the whole climate history since the simulation start. Now from the sowing window's `earliest-date` up to today. |

**A3 is an interpretation**: neither the code nor the parameter description
("temperature sum above T-base needed") says which period was meant. Alternatives would
be a sum since January 1 or since the last harvest - a one-line change.

## Method

- Release builds (`pixi run build`, Windows, MSVC) of `master` 7830f948, of A1 alone,
  of A2 alone (`master` plus only the A2 change) and of the branch head (all three).
- `pixi run test` with each build: suites `hohenfinow2` (7 simulations) and
  `muencheberg_v140` (48 simulations), parameters `monica-parameters` at `fb0f4785`.
- Two runs of `master` were byte-identical, so every difference comes from the fixes.
- `pixi run compare` lists per-column maxima over all days. Those are dominated by
  events shifting by a day: a crop maturing or harvested one day earlier shows the whole
  biomass as the difference on that day (e.g. TraDef up to 20%, `reldev-harv` up to 43%,
  `Stage` 0 <-> 6). The numbers below therefore use quantities that are robust against
  such shifts: per harvest (`crop` sections) and per column total over the whole run
  (sum for fluxes, mean for states).

## Results

### Yield per harvest, all three fixes (`crop` sections)

| Suite / crop | Harvests | Mean change | Range | Mean abs. change |
|---|--:|--:|--:|--:|
| v140 winter rye | 528 | -0.77% | -9.0% ... +7.3% | 1.21% |
| v140 oats | 144 | -1.61% | -7.1% ... +1.2% | 1.64% |
| v140 winter wheat | 384 | +0.20% | -6.5% ... +4.9% | 1.12% |
| v140 spring wheat | 48 | -0.46% | -2.9% ... +2.2% | 0.89% |
| v140 spring barley | 520 | -0.39% | -5.5% ... +5.3% | 0.70% |
| v140 potato | 432 | -0.08% | -5.1% ... +3.2% | 0.51% |
| v140 sugar beet | 384 | +0.20% | -3.4% ... +5.0% | 0.43% |
| v140 silage maize | 336 | -0.05% | -0.8% ... 0 | 0.05% |
| v140 phacelia | 192 | 0 | 0 | 0 |
| Hohenfinow2 winter wheat | 29 | -0.07% | -4.8% ... +1.9% | 0.58% |
| Hohenfinow2 spring barley | 1 | -4.45% | | |
| Hohenfinow2 winter rye | 1 | +0.73% | | |

Over all 3003 harvests 70% of the yields change, by 0.71% on average (at most 8.96%).
Further harvest values, mean absolute change (max): above-ground biomass 0.42% (5.1%),
N deficit 0.22% (5.1%), transpiration deficit 0.24%, max. LAI 0.27% (5.7%).

### Per fix

Yields: all `crop` sections. N and water fluxes: totals over the run of Hohenfinow2
`sim` (the simulation that writes them; NH3 from its monthly section).

| | A1 (NH4 / volatilisation) | A2 (root taper) | All three |
|---|---|---|---|
| Mean abs. yield change | 0.15% (max 4.5%) | 0.62% (max 8.4%) | 0.71% (max 9.0%) |
| Mean yield change, v140 winter rye | -0.29% | -0.52% | -0.77% |
| Mean yield change, v140 oats | -0.42% | -1.24% | -1.61% |
| NH3 volatilisation | **-8.2%** | +0.02% | -8.3% |
| N2O | -2.4% | 0 | -2.6% |
| N leaching | -1.9% | +1.1% | -0.7% |
| N uptake (`ActNup`) | -1.8% | -0.9% | -2.5% |
| Denitrification | 0 | -0.5% | -0.5% |
| Groundwater recharge | +0.05% | +0.02% | +0.16% |
| Automatic irrigation | 0 | +3.3% (`sim-min-co2`, `sim-min-warmer`) | +3.3% (same two) |

- **A1 mainly changes the N balance.** With the days since application advancing, the
  volatilisation rate decays as intended and NH3 losses drop by about 8%; the NH4 that
  does volatilise now really leaves the soil, which slightly lowers N uptake, N2O and
  leaching.
- **A2 mainly changes yields.** The roots between rooting depth and rooting zone get
  less weight, which moves water and N uptake towards the upper layers.
- **The effects don't simply add up**: leaching -1.9% (A1), +1.1% (A2), -0.7% (both).
- **A3 has no effect in these suites**: `muencheberg_v140` has no automatic sowing, and
  Hohenfinow2's automatic sowing always uses `"temp-sum-above-base-temp": 0`, which a sum
  of non-negative values always satisfies. Seeing A3's effect needs a scenario with a
  positive threshold.

## Reproducing

    git switch master && pixi run build
    pixi run test --monica _cmake_release/monica-run --engine master --out results/master
    git switch fix-cpp-quirks && pixi run build
    pixi run test --monica _cmake_release/monica-run --engine fixes --out results/fixes
    pixi run compare results/master results/fixes

(On Windows `_cmake_release/monica-run.exe`.) The per-harvest and per-total numbers
above came from a small script on top of the result directories; it isn't part of the
repository.
