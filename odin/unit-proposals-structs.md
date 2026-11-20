# Unit proposals for the other structs — applied, as guesses

> **Every unit in this file is a machine-made guess** (Claude, 2026-11-20), not documentation.
> They cover the 423 numeric fields of the structs in `odin/monica` outside `monica/params`
> (see `unit-proposals.md` for those) that have no `unit` tag yet. Not covered: the
> infrastructure packages `capnp`, `io` and `trace` (ports, output ids, rounding digits), and the
> generated Crop2ML packages (`odin/amei`, units from their XML).

Each proposal comes from how the field is computed and used (assignments, conversions, the
fields and parameters it is combined with), the legacy outputs that read it
(`io/build_output.odin`), already tagged neighbours, and comments in the Odin and C++ code.
**Confidence**: *high* — the quantity and unit follow from the code; *medium* — the kind of
quantity is clear, the exact unit less so (per ha or per m2, per day or per step, ...); *low* —
a best guess. Units follow `CONVENTIONS.md` §9a: `"0-1"` fractions, `"1"` other dimensionless
numbers, `""` no quantity (flags, categories, stage numbers), `"layer#"` layer indices and counts,
`"d"` counts of days; arrays get the unit of an element.

All 423 are in the code since 2026-11-20
(`tools/apply_unit_proposals.py --file odin/unit-proposals-structs.md --all --write`), as
`unit:"..." unit_guess:"<confidence>"`. **To review** a field: correct `unit` in the code if
needed and delete its `unit_guess` tag; `grep -rn 'unit_guess:"low"' odin/monica` lists what
is left. The tables are the record of why each unit was guessed (the `ok` column is no longer
used).

The field names link to their declarations (`#L<line>`); after code changes, refresh the links
with `python odin/tools/link_unit_proposals.py`.

Proposals: 423 — 342 high, 65 medium, 16 low confidence.

## Findings beyond units

The scan reads every use of these fields, and noticed things that aren't unit questions. Marked
**(checked)** where verified by hand; the rest are the scan's reading of the code, to be checked.
The names link to the code (`python odin/tools/link_unit_proposals.py` refreshes them).

- **Fields whose unit changes during a step.** [`Crop_Growth.crop_n_demand`](monica/core/crop_growth.odin#L27): kgN ha-1 in
  [`growth_step`](monica/core/crop_growth.odin#L86), divided by 10000 into kgN m-2 in [`growth_limit_n_demand`](monica/core/crop_growth.odin#L334); the final value is
  kgN m-2 (the [`PotNup`](monica/io/build_output.odin#L1510) output multiplies by 10000) **(checked)**. [`Soil_Temperature.solution`](monica/core/soil_temperature.odin#L47)
  holds a right-hand side in J m-2, then the solved temperatures in °C.
- **Parameter tags corrected** (in [`unit-proposals.md`](unit-proposals.md) and the code, 2026-11-20) **(checked)**:
  [`Species_Parameters.max_n_uptake_param`](monica/params/crop_parameters.odin#L47) is per metre of root (kg N m-1 d-1, not per ha), and
  [`Soil_Moisture_Module_Parameters.xsa_critical_soil_moisture`](monica/params/module_parameters.odin#L68) is a factor on field capacity
  ("1", not m3 m-3).
- **Output units that look wrong** (legacy names, kept for byte-identical outputs, §9a):
  [`RootWaUptak`](monica/io/output_paths.odin#L176) "KgN ha-1" reads transpiration (mm); [`TimeUnderAnoxia`](monica/io/output_paths.odin#L209) "0;1" is a number of
  days; [`rootDensity`](monica/io/output_paths.odin#L171) has no unit (m m-3); [`SMB-CO2-ER`](monica/io/build_output.odin#L1529) has none (kgC m-3 d-1); [`ActNupLayer`](monica/io/build_output.odin#L1519)
  is spelled "KgN ha-1".
- **Comments with wrong units.** [`Fvcb_Leaf_Fraction`](monica/core/photosynthesis_fvcb.odin#L110): [`oi`](monica/core/photosynthesis_fvcb.odin#L115), [`ci`](monica/core/photosynthesis_fvcb.odin#L116), [`cc`](monica/core/photosynthesis_fvcb.odin#L117) labelled "umol m-2"
  (are mmol mol-1, umol mol-1, umol mol-1), [`kc`](monica/core/photosynthesis_fvcb.odin#L113)/[`ko`](monica/core/photosynthesis_fvcb.odin#L114) "umol mol-1 mbar-1"; [`Voc_Cp_Data`](monica/core/voc_common.odin#L75):
  [`oi`](monica/core/voc_common.odin#L78)/[`ci`](monica/core/voc_common.odin#L79) comments swapped; [`Voc_Leaf_Emissions`](monica/core/voc_common.odin#L239): "ug m-2ground h-1" for values per g leaf dry
  weight; [`Soil_Temperature.b`](monica/core/soil_temperature.odin#L39) "[m]" for a value in m-1; [`O3_Impact_Params.gamma2`](monica/core/o3_impact.odin#L16)
  "nmol m-2 s-1" for its inverse; [`Rpscd_Res.sat`](monica/soil/soil_tables.odin#L103)/[`fc`](monica/soil/soil_tables.odin#L104)/[`pwp`](monica/soil/soil_tables.odin#L105) "[m3 m-3]" for vol% (divided by
  100 later); [`O3_Impact_Out.hourly_o3_up`](monica/core/o3_impact.odin#L54) "per hour" for a value per second; [`O3_Impact_In.gs`](monica/core/o3_impact.odin#L43)
  "ground area" but divided by LAI (per leaf area).
- **Possible bugs, same in the C++.** [`Voc_Photosynth_T.par`](monica/core/voc_common.odin#L211): the Guenther path multiplies by
  [`VOC_W_IN_UMOL`](monica/core/voc_common.odin#L16) (1/4.57), i.e. divides, where its own comment and the JJV path
  ([`VOC_UMOL_IN_W`](monica/core/voc_common.odin#L15)) multiply by 4.57 ([`voc_guenther.odin:83`](monica/core/voc_guenther.odin#L83), C++ [`voc-guenther.cpp:96`](../src/core/voc-guenther.cpp#L96))
  **(checked)**.
  [`Soil_Organic.sum_nh3_volatilised`](monica/core/soil_organic.odin#L58) sums only the urea part, not [`total_nh3_volatilised`](monica/core/soil_organic.odin#L44).
  [`Soil_Organic.net_n_mineralisation_rate`](monica/core/soil_organic.odin#L43) uses layer 0's thickness for every layer.
  [`Crop_Root_System.total_root_length`](monica/core/crop_root_system.odin#L25) is computed differently at initialisation/transplant
  than daily (the C++ has a todo there). [`Crop_Photosynthesis.fraction_of_intercepted_radiation2`](monica/core/crop_photosynthesis.odin#L34) is
  never set but read by the [`LightInterception2`](monica/io/build_output.odin#L1507) output.
  [`Crop_Photosynthesis.gross_photosynthesis_reference_mol`](monica/core/crop_photosynthesis.odin#L22) is tagged "mol m-2 s-1" in
  [`Crop_Et0_In`](monica/core/crop_et0.odin#L17) and "µmol m-2 s-1" in the ET module's input, and its 22414 factor fits neither
  exactly. Automatic sowing reads JSON [`max-3d-precip`](monica/run/worksteps.odin#L321) but writes [`max-3d-precip-sum`](monica/run/worksteps.odin#L647).
- **Unused or never set** (candidates for removal, if they aren't kept for C++ parity):
  crop: [`n_content_deficit`](monica/core/crop_n_uptake.odin#L23), [`soil_specific_max_rooting_depth`](monica/core/crop_root_system.odin#L20), [`crop_water_uptake`](monica/core/crop_water_uptake.odin#L23),
  [`accumulated_et_a`](monica/core/crop_water_uptake.odin#L28), [`accumulated_transpiration`](monica/core/crop_water_uptake.odin#L29) (declared only), [`reserve_assimilate_pool`](monica/core/crop_growth.odin#L25)
  (always 0), [`n_concentration_root_old`](monica/core/crop_n_status.odin#L18), [`intercropping_other_lait`](monica/core/crop_module.odin#L104) (set, never read); soil:
  [`Soil_Transport.time_step`](monica/core/soil_transport.odin#L33), [`total_dispersion`](monica/core/soil_transport.odin#L34), [`Soil_Moisture.soil_moisture_crit`](monica/core/soil_moisture.odin#L75),
  [`total_water_removal`](monica/core/soil_moisture.odin#L80), [`interception`](monica/core/soil_moisture.odin#L59), [`sum_surface_run_off`](monica/core/soil_moisture.odin#L79), [`Soil_Organic.irrigation_amount`](monica/core/soil_organic.odin#L22),
  [`Aom_Properties.aom_fast_dec_rate_to_smb_slow`](monica/core/soil_column.odin#L28) (never assigned),
  [`Frost_Component.accumulated_frost_depth`](monica/core/frost_component.odin#L13); VOC/FvCB: [`Voc_Cp_Data`](monica/core/voc_common.odin#L75): [`jj`](monica/core/voc_common.odin#L83), [`jj1000`](monica/core/voc_common.odin#L84), [`jv`](monica/core/voc_common.odin#L85) (and
  the [`Fvcb_Leaf_Fraction`](monica/core/photosynthesis_fvcb.odin#L110) fields feeding them), [`Voc_Micro_Climate_Data.sunlitfoliagefraction`](monica/core/voc_common.odin#L176),
  [`Voc_Leaf_Emission_T.foliage_layer`](monica/core/voc_common.odin#L231), the par-temp branch's [`sunlitfoliagefraction24`](monica/core/voc_common.odin#L177), [`par24`](monica/core/voc_common.odin#L212),
  [`par240`](monica/core/voc_common.odin#L213) (branch never taken), [`Fvcb_Canopy_Hourly_Params.gm_25`](monica/core/photosynthesis_fvcb.odin#L90) (gm hard-coded to 0.4), and in
  [`Voc_Species_Data`](monica/core/voc_common.odin#L89): [`hdj`](monica/core/voc_common.odin#L111), [`sdj`](monica/core/voc_common.odin#L112), [`ko25`](monica/core/voc_common.odin#L115), [`qjvc`](monica/core/voc_common.odin#L117), [`aejm`](monica/core/voc_common.odin#L121), [`slamin`](monica/core/voc_common.odin#L123) (unused), [`aekc`](monica/core/voc_common.odin#L119), [`aeko`](monica/core/voc_common.odin#L120),
  [`aevc`](monica/core/voc_common.odin#L122), [`kc25`](monica/core/voc_common.odin#L114), [`vcmax25`](monica/core/voc_common.odin#L116) (copied, never read), [`ct_is`](monica/core/voc_common.odin#L100), [`ct_mt`](monica/core/voc_common.odin#L101), [`ha_*`](monica/core/voc_common.odin#L103), [`ds_*`](monica/core/voc_common.odin#L106) (read, never
  set: always 0).
- **No single unit.** [`Cutting_Value.value`](monica/core/crop_module.odin#L1230) is a fraction, kgDM ha-1 or m2 m-2 depending on its
  [`Cutting_Unit`](monica/core/crop_module.odin#L1215) (proposed "", low). [`Monica_Model.accu_*_stress`](monica/core/monica_model.odin#L76) are sums of daily 0-1 factors
  (proposed "1"; "d" would do too).
- **New spellings** to confirm: "Heq-NRW tDM-1" (humus equivalents per t dry matter, residues and
  organic fertilisers), "kgFM ha-1" (organic fertiliser fresh matter).

## `Crop_Canopy` (monica/core/crop_canopy.odin)

| ok | field | line | type | proposed unit | confidence | basis |
| --- | --- | --- | --- | --- | --- | --- |
| [ ] | [`Crop_Canopy.crop_diameter`](monica/core/crop_canopy.odin#L19) | 19 | `f64` | `m` | high | max_crop_diameter (m) * relative development, crop_canopy.odin:197; used with crop_height in GAI |
| [ ] | [`Crop_Canopy.green_area_index`](monica/core/crop_canopy.odin#L21) | 21 | `f64` | `m2 m-2` | high | LAI + crop_height*PI*crop_diameter*plant_density, crop_canopy.odin:261 (C++ comment [m2 m-2]) |
| [ ] | [`Crop_Canopy.soil_coverage`](monica/core/crop_canopy.odin#L22) | 22 | `f64` | `0-1` | high | 1 - exp(-0.5*LAI), crop_canopy.odin:159; Water_Uptake_In.soil_coverage tagged 0-1 |
| [ ] | [`Crop_Canopy.kcb_ini`](monica/core/crop_canopy.odin#L26) | 26 | `f64` | `1` | high | basal crop coefficient like kcb_factor (tagged 1); default 0.15, crop_module.odin:122 |
| [ ] | [`Crop_Canopy.kcb_mid`](monica/core/crop_canopy.odin#L27) | 27 | `f64` | `1` | high | from stage_kc_factor (unit 1) - 0.05/0.10, crop_module.odin:397; interpolated into kcb_factor (1) |
| [ ] | [`Crop_Canopy.kcb_end`](monica/core/crop_canopy.odin#L28) | 28 | `f64` | `1` | high | from stage_kc_factor (unit 1) - 0.05, crop_module.odin:401; interpolated into kcb_factor (1) |

## `Crop_Carbon` (monica/core/crop_carbon.odin)

| ok | field | line | type | proposed unit | confidence | basis |
| --- | --- | --- | --- | --- | --- | --- |
| [ ] | [`Crop_Carbon.respiration`](monica/core/crop_carbon.odin#L11) | 11 | `f64` | `kgC ha-1` | high | total_respired (kgCH2O ha-1) /30*12, crop_carbon.odin:26; NPP = GPP (kgC ha-1) - respiration |

## `Crop_Carry_Over` (monica/core/crop_carry_over.odin)

| ok | field | line | type | proposed unit | confidence | basis |
| --- | --- | --- | --- | --- | --- | --- |
| [ ] | [`Crop_Carry_Over.developmental_stage`](monica/core/crop_carry_over.odin#L27) | 27 | `int` | `` | high | copy of phenology's developmental_stage (stage index); Oxygen_Deficiency_In tags it '' |
| [ ] | [`Crop_Carry_Over.leaf_senescence_increment`](monica/core/crop_carry_over.odin#L41) | 41 | `f64` | `kgDM ha-1` | high | organ_senescence_increment[leaf]; read as Canopy_Growth_In.leaf_senescence_increment (kgDM ha-1) |
| [ ] | [`Crop_Carry_Over.belowground_biomass`](monica/core/crop_carry_over.odin#L48) | 48 | `f64` | `kgDM ha-1` | high | copy of growth's belowground_biomass; N_Status_In.belowground_biomass tagged kgDM ha-1 |

## `Crop_Growth` (monica/core/crop_growth.odin)

| ok | field | line | type | proposed unit | confidence | basis |
| --- | --- | --- | --- | --- | --- | --- |
| [ ] | [`Crop_Growth.organ_biomass`](monica/core/crop_growth.odin#L15) | 15 | `[dynamic]f64` | `kgDM ha-1` | high | output 'OrgBiom' kgDM ha-1 (build_output.odin:1473); += organ_growth_increment (kgDM ha-1) |
| [ ] | [`Crop_Growth.organ_green_biomass`](monica/core/crop_growth.odin#L16) | 16 | `[dynamic]f64` | `kgDM ha-1` | high | organ_biomass - organ_dead_biomass, crop_growth.odin:259; output 'OrgGreenBiom' kgDM ha-1 |
| [ ] | [`Crop_Growth.organ_dead_biomass`](monica/core/crop_growth.odin#L17) | 17 | `[dynamic]f64` | `kgDM ha-1` | high | accumulates senescence increments, subtracted from organ_biomass (kgDM ha-1), crop_growth.odin:260 |
| [ ] | [`Crop_Growth.organ_senescence_increment`](monica/core/crop_growth.odin#L19) | 19 | `[dynamic]f64` | `kgDM ha-1` | high | organ_green_biomass * senescence rate (daily), crop_growth.odin:229; same form as organ_growth_increment |
| [ ] | [`Crop_Growth.aboveground_biomass_old`](monica/core/crop_growth.odin#L21) | 21 | `f64` | `kgDM ha-1` | high | yesterday's aboveground_biomass (kgDM ha-1), crop_growth.odin:130; N_Uptake_In tag kgDM ha-1 |
| [ ] | [`Crop_Growth.belowground_biomass`](monica/core/crop_growth.odin#L22) | 22 | `f64` | `kgDM ha-1` | high | sum of non-root belowground organ_biomass, crop_growth.odin:269; N_Status_In tag kgDM ha-1 |
| [ ] | [`Crop_Growth.belowground_biomass_old`](monica/core/crop_growth.odin#L23) | 23 | `f64` | `kgDM ha-1` | high | yesterday's belowground_biomass, crop_growth.odin:132; N_Uptake_In tag kgDM ha-1 |
| [ ] | [`Crop_Growth.total_biomass`](monica/core/crop_growth.odin#L24) | 24 | `f64` | `kgDM ha-1` | high | sum of organ_biomass (kgDM ha-1), crop_growth.odin:272 |
| [ ] | [`Crop_Growth.reserve_assimilate_pool`](monica/core/crop_growth.odin#L25) | 25 | `f64` | `kgCH2O ha-1` | low | only ever set to 0 (crop_growth.odin:128/276), never read; assimilate pool by name |
| [ ] | [`Crop_Growth.crop_n_demand`](monica/core/crop_growth.odin#L27) | 27 | `f64` | `kgN m-2` | medium | INCONSISTENT: kgN ha-1 in growth_step (:289), converted to kgN m-2 in growth_limit_n_demand (:343); output PotNup *10000 |
| [ ] | [`Crop_Growth.max_n_uptake`](monica/core/crop_growth.odin#L28) | 28 | `f64` | `kgN m-1 d-1` | medium | total_root_length (m m-2) * max_n_uptake * time_step compared to kgN m-2, crop_growth.odin:342; param tag kg N ha-1 d-1 wrong |

## `Crop_Management` (monica/core/crop_management.odin)

| ok | field | line | type | proposed unit | confidence | basis |
| --- | --- | --- | --- | --- | --- | --- |
| [ ] | [`Crop_Management.transplant_shock_duration`](monica/core/crop_management.odin#L13) | 13 | `int` | `d` | high | compared with days_since_transplant counter incremented daily, crop_management.odin:140 |
| [ ] | [`Crop_Management.days_since_transplant`](monica/core/crop_management.odin#L14) | 14 | `int` | `d` | high | daily counter (+1 per step), crop_management.odin:143; -1 = inactive |
| [ ] | [`Crop_Management.transplant_efficiency`](monica/core/crop_management.odin#L15) | 15 | `f64` | `0-1` | high | 0.2..1 ramp, crop_management.odin:141; Photosynthesis_In.transplant_efficiency tagged 0-1 |
| [ ] | [`Crop_Management.cutting_delay_days`](monica/core/crop_management.odin#L16) | 16 | `int` | `d` | high | from species cutting_delay_days (d), decremented daily, crop_management.odin:132; Photosynthesis_In tag d |

## `Crop_Module` (monica/core/crop_module.odin)

| ok | field | line | type | proposed unit | confidence | basis |
| --- | --- | --- | --- | --- | --- | --- |
| [ ] | [`Crop_Module.no_of_organs`](monica/core/crop_module.odin#L60) | 60 | `int` | `` | high | count of organs (array length), crop_module.odin:282 |
| [ ] | [`Crop_Module.no_of_dev_stages`](monica/core/crop_module.odin#L61) | 61 | `int` | `` | high | count of developmental stages |
| [ ] | [`Crop_Module.storage_organ`](monica/core/crop_module.odin#L62) | 62 | `int` | `` | high | organ index; Growth_In.storage_organ tagged '' |

## `Crop_Intercropping` (monica/core/crop_module.odin)

| ok | field | line | type | proposed unit | confidence | basis |
| --- | --- | --- | --- | --- | --- | --- |
| [ ] | [`Crop_Intercropping.intercropping_other_crop_height`](monica/core/crop_module.odin#L103) | 103 | `f64` | `m` | high | set from other crop's height, compared to crop_height (m), crop_module.odin:849 |
| [ ] | [`Crop_Intercropping.intercropping_other_lait`](monica/core/crop_module.odin#L104) | 104 | `f64` | `m2 m-2` | medium | other crop's LAI(t) by name, set crop_module.odin:422, never read |

## `Cutting_Value` (monica/core/crop_module.odin)

| ok | field | line | type | proposed unit | confidence | basis |
| --- | --- | --- | --- | --- | --- | --- |
| [ ] | [`Cutting_Value.value`](monica/core/crop_module.odin#L1230) | 1230 | `f64` | `` | low | unit depends on Cutting_Unit: fraction (Percentage) or kgDM ha-1 (Biomass) or m2 m-2 (LAI), crop_growth.odin:405/420 |

## `Crop_N_Status` (monica/core/crop_n_status.odin)

| ok | field | line | type | proposed unit | confidence | basis |
| --- | --- | --- | --- | --- | --- | --- |
| [ ] | [`Crop_N_Status.n_concentration_aboveground_biomass_old`](monica/core/crop_n_status.odin#L17) | 17 | `f64` | `kgN kgDM-1` | high | copy of n_concentration_aboveground_biomass, crop_n_status.odin:48; N_Uptake_In tag kgN kgDM-1 |
| [ ] | [`Crop_N_Status.n_concentration_root_old`](monica/core/crop_n_status.odin#L18) | 18 | `f64` | `kgN kgDM-1` | high | copy of n_concentration_root (kgN kgDM-1), crop_n_status.odin:49; never read |

## `Crop_N_Uptake` (monica/core/crop_n_uptake.odin)

| ok | field | line | type | proposed unit | confidence | basis |
| --- | --- | --- | --- | --- | --- | --- |
| [ ] | [`Crop_N_Uptake.n_uptake_from_layer`](monica/core/crop_n_uptake.odin#L15) | 15 | `[dynamic]f64` | `kgN m-2` | high | *10000 [kg m-2]->[kg ha-1] into total_n_uptake, crop_n_uptake.odin:197; output ActNupLayer *10000 |
| [ ] | [`Crop_N_Uptake.total_n_input`](monica/core/crop_n_uptake.odin#L20) | 20 | `f64` | `kgN ha-1` | high | total_n_uptake + fixed_n (kgN ha-1), crop_n_uptake.odin:211; or crop_n_demand*10000 |
| [ ] | [`Crop_N_Uptake.n_content_deficit`](monica/core/crop_n_uptake.odin#L23) | 23 | `f64` | `kgN ha-1` | low | unused (declared only, also in C++ only serialized); unit guessed from name |

## `Crop_O3` (monica/core/crop_o3.odin)

| ok | field | line | type | proposed unit | confidence | basis |
| --- | --- | --- | --- | --- | --- | --- |
| [ ] | [`Crop_O3.o3_senescence`](monica/core/crop_o3.odin#L13) | 13 | `f64` | `0-1` | high | = O3_Impact_Out.f_ls (0.6..1), multiplies vcmax_25, crop_photosynthesis.odin:542; init 1.0 |

## `Crop_Phenology` (monica/core/crop_phenology.odin)

| ok | field | line | type | proposed unit | confidence | basis |
| --- | --- | --- | --- | --- | --- | --- |
| [ ] | [`Crop_Phenology.developmental_stage`](monica/core/crop_phenology.odin#L15) | 15 | `int` | `` | high | stage index; Canopy_Kc_In.developmental_stage tagged '' |
| [ ] | [`Crop_Phenology.final_developmental_stage`](monica/core/crop_phenology.odin#L16) | 16 | `int` | `` | high | stage index; N_Uptake_In.final_developmental_stage tagged '' |
| [ ] | [`Crop_Phenology.current_temperature_sum`](monica/core/crop_phenology.odin#L17) | 17 | `[dynamic]f64` | `°C d` | high | per-stage temperature sum; Canopy_Kc_In.current_temperature_sum tagged °C d |
| [ ] | [`Crop_Phenology.total_temperature_sum`](monica/core/crop_phenology.odin#L19) | 19 | `f64` | `°C d` | high | Growth_N_Limit_In/Water_Uptake_In.total_temperature_sum tagged °C d |
| [ ] | [`Crop_Phenology.temperature_sum_to_flowering`](monica/core/crop_phenology.odin#L20) | 20 | `f64` | `°C d` | high | sum of stage_temperature_sum, crop_module.odin:315; Photosynthesis_In tag °C d |
| [ ] | [`Crop_Phenology.vernalisation_days`](monica/core/crop_phenology.odin#L24) | 24 | `f64` | `d` | high | vernalisation input struct tags it d (crop_phenology.odin:458); compared with vernalisation_requirement (d) |
| [ ] | [`Crop_Phenology.no_of_crop_steps`](monica/core/crop_phenology.odin#L27) | 27 | `int` | `d` | medium | count of (daily) crop steps, crop_module.odin:1023; only compared with 0 |
| [ ] | [`Crop_Phenology.anthesis_day`](monica/core/crop_phenology.odin#L29) | 29 | `int` | `` | high | set from julian_day (day of year), crop_phenology.odin:345; -1 = none |
| [ ] | [`Crop_Phenology.maturity_day`](monica/core/crop_phenology.odin#L30) | 30 | `int` | `` | high | set from julian_day (day of year), crop_phenology.odin:348; -1 = none |
| [ ] | [`Crop_Phenology.time_step`](monica/core/crop_phenology.odin#L32) | 32 | `f64` | `d` | high | passed as time_step to inputs tagged d (Canopy_Growth_In, N_Uptake_In); C++ dt = 1.0 |

## `Crop_Photosynthesis` (monica/core/crop_photosynthesis.odin)

| ok | field | line | type | proposed unit | confidence | basis |
| --- | --- | --- | --- | --- | --- | --- |
| [ ] | [`Crop_Photosynthesis.co2_method`](monica/core/crop_photosynthesis.odin#L18) | 18 | `int` | `` | high | method selector (2 or 3), crop_photosynthesis.odin:180/296 |
| [ ] | [`Crop_Photosynthesis.assimilation_rate`](monica/core/crop_photosynthesis.odin#L19) | 19 | `f64` | `kg CO2 ha-1 h-1` | high | max_assimilation_rate (kg CO2 ha-1 h-1) * t_response, crop_photosynthesis.odin:310 (AMAX) |
| [ ] | [`Crop_Photosynthesis.gross_photosynthesis_mol`](monica/core/crop_photosynthesis.odin#L21) | 21 | `f64` | `mol m-2 s-1` | medium | kg CO2 ha-1 d-1 * 22414/(10*86400*44), comment [mol m-2 s-1] (:611); factor 22414 makes it not exactly mol |
| [ ] | [`Crop_Photosynthesis.gross_photosynthesis_reference_mol`](monica/core/crop_photosynthesis.odin#L22) | 22 | `f64` | `mol m-2 s-1` | medium | Et0_In tag mol m-2 s-1 but evapotranspiration module tags it µmol m-2 s-1 - inconsistent; same 22414 formula |
| [ ] | [`Crop_Photosynthesis.gross_assimilates`](monica/core/crop_photosynthesis.odin#L23) | 23 | `f64` | `kgCH2O ha-1` | high | = assimilates (kgCH2O ha-1), crop_photosynthesis.odin:630; Carbon_In tag kgCH2O ha-1 |
| [ ] | [`Crop_Photosynthesis.net_maintenance_respiration`](monica/core/crop_photosynthesis.odin#L27) | 27 | `f64` | `kgCH2O ha-1` | high | min(gross_photosynthesis (kgCH2O ha-1), organ green biomass * maint coeff), crop_photosynthesis.odin:743 |
| [ ] | [`Crop_Photosynthesis.total_respired`](monica/core/crop_photosynthesis.odin#L28) | 28 | `f64` | `kgCH2O ha-1` | high | gross_assimilates - assimilates [kg CH2O ha-1], crop_photosynthesis.odin:723; Carbon_In tag |
| [ ] | [`Crop_Photosynthesis.sunlit_leaf_area_index`](monica/core/crop_photosynthesis.odin#L29) | 29 | `[dynamic]f64` | `m2 m-2` | high | hourly fvcb sunlit.lai (commented m2 m-2 in photosynthesis_fvcb.odin:103), crop_photosynthesis.odin:546 |
| [ ] | [`Crop_Photosynthesis.shaded_leaf_area_index`](monica/core/crop_photosynthesis.odin#L30) | 30 | `[dynamic]f64` | `m2 m-2` | high | hourly fvcb shaded.lai (m2 m-2), crop_photosynthesis.odin:547 |
| [ ] | [`Crop_Photosynthesis.k_tkc`](monica/core/crop_photosynthesis.odin#L31) | 31 | `f64` | `1` | high | Arrhenius temperature factor exp(...)*sqrt(...), crop_photosynthesis.odin:196; Growth_In.k_tkc tagged 1 |
| [ ] | [`Crop_Photosynthesis.k_tko`](monica/core/crop_photosynthesis.odin#L32) | 32 | `f64` | `1` | high | Arrhenius temperature factor scaling ko25, crop_photosynthesis.odin:197 |
| [ ] | [`Crop_Photosynthesis.fraction_of_intercepted_radiation1`](monica/core/crop_photosynthesis.odin#L33) | 33 | `f64` | `0-1` | high | 1 - exp(-k*LAI), crop_photosynthesis.odin:435/601; output LightInterception1 |
| [ ] | [`Crop_Photosynthesis.fraction_of_intercepted_radiation2`](monica/core/crop_photosynthesis.odin#L34) | 34 | `f64` | `0-1` | medium | never set (always 0), only read by getter for output LightInterception2; fraction by name |
| [ ] | [`Crop_Photosynthesis.max_assimilation_rate`](monica/core/crop_photosynthesis.odin#L38) | 38 | `f64` | `kg CO2 ha-1 h-1` | high | cultivar MaxAssimilationRate (kg CO2 ha-1 h-1), reduced by cuttings |

## `Crop_Root_System` (monica/core/crop_root_system.odin)

| ok | field | line | type | proposed unit | confidence | basis |
| --- | --- | --- | --- | --- | --- | --- |
| [ ] | [`Crop_Root_System.rooting_depth_m`](monica/core/crop_root_system.odin#L16) | 16 | `f64` | `m` | high | initial_rooting_depth (m) + temp * root_penetration_rate (m °C-1 d-1), crop_root_system.odin:149 |
| [ ] | [`Crop_Root_System.max_rooting_depth`](monica/core/crop_root_system.odin#L18) | 18 | `f64` | `m` | high | crop_specific_max_rooting_depth [m], crop_module.odin:378; capped by layers*thickness |
| [ ] | [`Crop_Root_System.soil_specific_max_rooting_depth`](monica/core/crop_root_system.odin#L20) | 20 | `f64` | `m` | medium | unused (declared only, C++ use commented out); C++ comment WURZMAX [m] |
| [ ] | [`Crop_Root_System.root_biomass`](monica/core/crop_root_system.odin#L21) | 21 | `f64` | `kgDM ha-1` | high | = root organ biomass, crop_root_system.odin:65; N_Uptake_In.root_biomass tagged kgDM ha-1 |
| [ ] | [`Crop_Root_System.root_biomass_old`](monica/core/crop_root_system.odin#L22) | 22 | `f64` | `kgDM ha-1` | high | yesterday's root_biomass, crop_root_system.odin:64; N_Uptake_In tag kgDM ha-1 |
| [ ] | [`Crop_Root_System.root_density`](monica/core/crop_root_system.odin#L23) | 23 | `[dynamic]f64` | `m m-3` | high | share of total_root_length per layer [m m-3], crop_root_system.odin:194; Water_Uptake_In tag m m-3 |
| [ ] | [`Crop_Root_System.root_diameter`](monica/core/crop_root_system.odin#L24) | 24 | `[dynamic]f64` | `m` | high | 0.0002 - i*0.00001, crop_root_system.odin:201; N_Uptake_In.root_diameter tagged m |
| [ ] | [`Crop_Root_System.total_root_length`](monica/core/crop_root_system.odin#L25) | 25 | `f64` | `m m-2` | medium | root_biomass*specific_root_length [m m-2] (:167), Growth_N_Limit_In tag m m-2; init formula (:295) differs (C++ todo) |
| [ ] | [`Crop_Root_System.current_total_temperature_sum_root`](monica/core/crop_root_system.odin#L26) | 26 | `f64` | `°C d` | high | daily sum of root-growth effective temperature, crop_root_system.odin:124; vs root_growth_lag |

## `Crop_Stress` (monica/core/crop_stress.odin)

| ok | field | line | type | proposed unit | confidence | basis |
| --- | --- | --- | --- | --- | --- | --- |
| [ ] | [`Crop_Stress.time_under_anoxia_threshold_default`](monica/core/crop_stress.odin#L17) | 17 | `int` | `d` | high | threshold for time_under_anoxia (tagged d), default 4, crop_stress.odin:41 |
| [ ] | [`Crop_Stress.days_after_begin_flowering`](monica/core/crop_stress.odin#L19) | 19 | `int` | `d` | high | daily counter, crop_stress.odin:138; used in flowering curve |
| [ ] | [`Crop_Stress.total_crop_heat_impact`](monica/core/crop_stress.odin#L20) | 20 | `f64` | `0-1` | medium | sum of 0-1 heat impact * daily flowering rate (sums <=1), crop_stress.odin:136; becomes crop_heat_redux |
| [ ] | [`Crop_Stress.lt50_m`](monica/core/crop_stress.odin#L23) | 23 | `f64` | `°C` | high | min(lt50 (°C), lt50_m), compared with crown temperature, crop_stress.odin:171/223 |
| [ ] | [`Crop_Stress.drought_impact_on_fertility`](monica/core/crop_stress.odin#L24) | 24 | `f64` | `0-1` | high | Growth_In.drought_impact_on_fertility tagged 0-1; default 1.0 |

## `Crop_To_Soil` (monica/core/crop_to_soil.odin)

| ok | field | line | type | proposed unit | confidence | basis |
| --- | --- | --- | --- | --- | --- | --- |
| [ ] | [`Crop_To_Soil.developmental_stage`](monica/core/crop_to_soil.odin#L27) | 27 | `int` | `` | high | copy of phenology's stage index, crop_to_soil.odin:58 |
| [ ] | [`Crop_To_Soil.soil_coverage`](monica/core/crop_to_soil.odin#L29) | 29 | `f64` | `0-1` | high | copy of canopy soil_coverage 1-exp(-0.5 LAI); damping weight in soil temperature, soil_temperature.odin:287 |
| [ ] | [`Crop_To_Soil.net_precipitation`](monica/core/crop_to_soil.odin#L39) | 39 | `f64` | `mm` | high | copy of water uptake net_precipitation; set into soil_moisture.net_precipitation_mm, soil_moisture.odin:195 |
| [ ] | [`Crop_To_Soil.n_uptake_from_layer`](monica/core/crop_to_soil.odin#L45) | 45 | `[]f64` | `kgN m-2` | high | view of Crop_N_Uptake.n_uptake_from_layer (kgN m-2; *10000 to kg ha-1 at crop_n_uptake.odin:197) |

## `Crop_Events` (monica/core/crop_to_soil.odin)

| ok | field | line | type | proposed unit | confidence | basis |
| --- | --- | --- | --- | --- | --- | --- |
| [ ] | [`Crop_Events.stage`](monica/core/crop_to_soil.odin#L106) | 106 | `int` | `` | high | stage number for the 'Stage-<n>' event, 0 = none |

## `Snow_For_Crop` (monica/core/crop_to_soil.odin)

| ok | field | line | type | proposed unit | confidence | basis |
| --- | --- | --- | --- | --- | --- | --- |
| [ ] | [`Snow_For_Crop.snow_depth`](monica/core/crop_to_soil.odin#L126) | 126 | `f64` | `mm` | high | copied from Snow_Component.snow_depth (mm, output 'SnowD' mm), crop_to_soil.odin:133 |
| [ ] | [`Snow_For_Crop.frost_depth`](monica/core/crop_to_soil.odin#L127) | 127 | `f64` | `m` | high | copied from Frost_Component.frost_depth (unit m, output 'FrostD' m), crop_to_soil.odin:133 |

## `Crop_Voc` (monica/core/crop_voc.odin)

| ok | field | line | type | proposed unit | confidence | basis |
| --- | --- | --- | --- | --- | --- | --- |
| [ ] | [`Crop_Voc.step_size24`](monica/core/crop_voc.odin#L13) | 13 | `int` | `` | high | ring-buffer length (24 hourly entries), crop_module.odin:129; count |
| [ ] | [`Crop_Voc.step_size240`](monica/core/crop_voc.odin#L14) | 14 | `int` | `` | high | ring-buffer length (240 hourly entries), crop_module.odin:130; count |
| [ ] | [`Crop_Voc.rad24`](monica/core/crop_voc.odin#L15) | 15 | `[dynamic]f64` | `W m-2` | high | stores globrad_wm2 = MJ m-2 h-1 *1e6/3600, crop_voc.odin:56,73 (global, not PAR) |
| [ ] | [`Crop_Voc.rad240`](monica/core/crop_voc.odin#L16) | 16 | `[dynamic]f64` | `W m-2` | high | stores globrad_wm2 = MJ m-2 h-1 *1e6/3600, crop_voc.odin:56,64 (global, not PAR) |
| [ ] | [`Crop_Voc.tfol24`](monica/core/crop_voc.odin#L17) | 17 | `[dynamic]f64` | `°C` | high | stores hourly leaf_temp (°C), crop_voc.odin:74; later +273.15 to K in voc_jjv |
| [ ] | [`Crop_Voc.tfol240`](monica/core/crop_voc.odin#L18) | 18 | `[dynamic]f64` | `°C` | high | stores hourly leaf_temp (°C), crop_voc.odin:65 |
| [ ] | [`Crop_Voc.index24`](monica/core/crop_voc.odin#L19) | 19 | `int` | `` | high | ring-buffer index into rad24/tfol24, crop_voc.odin:67-74 |
| [ ] | [`Crop_Voc.index240`](monica/core/crop_voc.odin#L20) | 20 | `int` | `` | high | ring-buffer index into rad240/tfol240, crop_voc.odin:58-65 |

## `Crop_Water_Uptake` (monica/core/crop_water_uptake.odin)

| ok | field | line | type | proposed unit | confidence | basis |
| --- | --- | --- | --- | --- | --- | --- |
| [ ] | [`Crop_Water_Uptake.interception_storage`](monica/core/crop_water_uptake.odin#L14) | 14 | `f64` | `mm` | high | 2.5*height*coverage storage, compared with remaining_evapotranspiration (mm), crop_water_uptake.odin:153 |
| [ ] | [`Crop_Water_Uptake.net_precipitation`](monica/core/crop_water_uptake.odin#L15) | 15 | `f64` | `mm` | high | gross_precipitation (mm) - interception, crop_water_uptake.odin:135 |
| [ ] | [`Crop_Water_Uptake.potential_transpiration`](monica/core/crop_water_uptake.odin#L18) | 18 | `f64` | `mm` | high | remaining_evapotranspiration (mm) * soil_coverage [mm], crop_water_uptake.odin:168 (TRAMAX [mm]) |
| [ ] | [`Crop_Water_Uptake.transpiration_redux`](monica/core/crop_water_uptake.odin#L21) | 21 | `[dynamic]f64` | `0-1` | high | per-layer reduction factor clipped 0..1, crop_water_uptake.odin:187-210; transpiration*(1-redux) |
| [ ] | [`Crop_Water_Uptake.root_effectivity`](monica/core/crop_water_uptake.odin#L22) | 22 | `[dynamic]f64` | `0-1` | high | per-layer effectivity 0.15..1 (or 0.5), crop_water_uptake.odin:188-216; weights root_density |
| [ ] | [`Crop_Water_Uptake.crop_water_uptake`](monica/core/crop_water_uptake.odin#L23) | 23 | `[dynamic]f64` | `mm` | low | unused (declared only; C++ only serialized); old TP = transpiration (mm) |
| [ ] | [`Crop_Water_Uptake.accumulated_et_a`](monica/core/crop_water_uptake.odin#L28) | 28 | `f64` | `mm` | medium | unused (declared only); accumulated actual ET by name |
| [ ] | [`Crop_Water_Uptake.accumulated_transpiration`](monica/core/crop_water_uptake.odin#L29) | 29 | `f64` | `mm` | medium | unused (declared only); accumulated transpiration by name |

## `Frost_Component` (monica/core/frost_component.odin)

| ok | field | line | type | proposed unit | confidence | basis |
| --- | --- | --- | --- | --- | --- | --- |
| [ ] | [`Frost_Component.accumulated_frost_depth`](monica/core/frost_component.odin#L13) | 13 | `f64` | `m` | medium | running sum of daily frost_depth (m), frost_component.odin:68; never read elsewhere (unused; strictly m d) |
| [ ] | [`Frost_Component.negative_degree_days`](monica/core/frost_component.odin#L14) | 14 | `f64` | `°C d` | high | accumulates -temperature_under_snow (°C) per day, frost_component.odin calc_frost_depth |
| [ ] | [`Frost_Component.frost_days`](monica/core/frost_component.odin#L16) | 16 | `int` | `d` | high | incremented once per day while frost_depth > 0, frost_component.odin calc_frost_depth |
| [ ] | [`Frost_Component.lambda_redux`](monica/core/frost_component.odin#L17) | 17 | `[dynamic]f64` | `0-1` | high | set to 0 (frozen) or 1, multiplies layer lambda in soil_moisture.odin:410 |
| [ ] | [`Frost_Component.temperature_under_snow`](monica/core/frost_component.odin#L18) | 18 | `f64` | `°C` | high | air temperature scaled by snow cover; feeds soil_temperature input tagged °C (modules/soil_temperature/soil_temperature.odin:65) |
| [ ] | [`Frost_Component.hydraulic_conductivity_redux`](monica/core/frost_component.odin#L19) | 19 | `f64` | `0-1` | high | set to 0/0.1 or param HydraulicConductivityRedux (0-1), frost_component.odin update_lambda_redux |
| [ ] | [`Frost_Component.time_step`](monica/core/frost_component.odin#L20) | 20 | `f64` | `d` | high | from env_params.time_step (unit d), soil_moisture.odin:142 |
| [ ] | [`Frost_Component.initial_hydraulic_conductivity_redux`](monica/core/frost_component.odin#L21) | 21 | `f64` | `0-1` | high | from mod_params.hydraulic_conductivity_redux (0-1), soil_moisture.odin:141 |

## `Monica_Model` (monica/core/monica_model.odin)

| ok | field | line | type | proposed unit | confidence | basis |
| --- | --- | --- | --- | --- | --- | --- |
| [ ] | [`Monica_Model.daily_sum_organic_fertilizer_dm`](monica/core/monica_model.odin#L64) | 64 | `f64` | `kgDM ha-1` | high | += amount_fm * aom_dry_matter_content, monica_model.odin:253 |
| [ ] | [`Monica_Model.sum_organic_fertilizer_dm`](monica/core/monica_model.odin#L65) | 65 | `f64` | `kgDM ha-1` | high | sum of organic fertiliser DM, used as kg ha-1 /1000 * heq, monica_model.odin:527 |
| [ ] | [`Monica_Model.days_with_crop`](monica/core/monica_model.odin#L75) | 75 | `int` | `d` | high | +1 per crop day, monica_model.odin:861 |
| [ ] | [`Monica_Model.accu_n_stress`](monica/core/monica_model.odin#L76) | 76 | `f64` | `1` | low | daily sum of crop_n_redux (0-1), monica_model.odin:950 ('d' also defensible); only read by ref tests |
| [ ] | [`Monica_Model.accu_water_stress`](monica/core/monica_model.odin#L77) | 77 | `f64` | `1` | low | daily sum of transpiration_deficit (0-1), monica_model.odin:951; only read by ref tests |
| [ ] | [`Monica_Model.accu_heat_stress`](monica/core/monica_model.odin#L78) | 78 | `f64` | `1` | low | daily sum of crop_heat_redux (0-1), monica_model.odin:952; only read by ref tests |
| [ ] | [`Monica_Model.accu_oxygen_stress`](monica/core/monica_model.odin#L79) | 79 | `f64` | `1` | low | daily sum of oxygen_deficit (0-1), monica_model.odin:953; only read by ref tests |

## `Harvest_Spec_Value` (monica/core/monica_model.odin)

| ok | field | line | type | proposed unit | confidence | basis |
| --- | --- | --- | --- | --- | --- | --- |
| [ ] | [`Harvest_Spec_Value.export_percentage`](monica/core/monica_model.odin#L458) | 458 | `f64` | `%` | high | organ_biomass * export_percentage / 100, monica_model.odin:616; default 100 |

## `Harvest_Opt_Carbon_Management_Data` (monica/core/monica_model.odin)

| ok | field | line | type | proposed unit | confidence | basis |
| --- | --- | --- | --- | --- | --- | --- |
| [ ] | [`Harvest_Opt_Carbon_Management_Data.crop_impact_on_humus_balance`](monica/core/monica_model.odin#L470) | 470 | `f64` | `Heq-NRW ha-1` | medium | added to humus_balance_carry_over (Heq-NRW ha-1), monica_model.odin:526-531 |
| [ ] | [`Harvest_Opt_Carbon_Management_Data.max_residue_recover_fraction`](monica/core/monica_model.odin#L471) | 471 | `f64` | `0-1` | high | residue_biomass * fraction = max exported residues, monica_model.odin:560; default 1 |
| [ ] | [`Harvest_Opt_Carbon_Management_Data.residue_heq`](monica/core/monica_model.odin#L473) | 473 | `f64` | `Heq-NRW tDM-1` | medium | residue_biomass (kgDM ha-1)/1000 * residue_heq -> humus balance (Heq-NRW ha-1), monica_model.odin:535 |
| [ ] | [`Harvest_Opt_Carbon_Management_Data.organic_fertilizer_heq`](monica/core/monica_model.odin#L474) | 474 | `f64` | `Heq-NRW tDM-1` | medium | org. fertiliser DM (kg ha-1)/1000 * heq -> humus balance (Heq-NRW ha-1), monica_model.odin:531 |

## `O3_Impact_Params` (monica/core/o3_impact.odin)

| ok | field | line | type | proposed unit | confidence | basis |
| --- | --- | --- | --- | --- | --- | --- |
| [ ] | [`O3_Impact_Params.gamma1`](monica/core/o3_impact.odin#L15) | 15 | `f64` | `1` | high | comment unitless; gamma1/gamma2 compared with O3 uptake, 1+gamma1-gamma2*up, o3_impact.odin:79 |
| [ ] | [`O3_Impact_Params.gamma2`](monica/core/o3_impact.odin#L16) | 16 | `f64` | `m2 s nmol-1` | medium | gamma2*O3_up (nmol m-2 s-1) is dimensionless, o3_impact.odin:80; comment 'nmol m-2 s-1' is the inverse |
| [ ] | [`O3_Impact_Params.gamma3`](monica/core/o3_impact.odin#L17) | 17 | `f64` | `m2 umol-1` | high | comment (umol m-2)-1; 1-gamma3*O3_tot_up (µmol m-2), o3_impact.odin:122 |
| [ ] | [`O3_Impact_Params.upper_thr_stomatal`](monica/core/o3_impact.odin#L18) | 18 | `f64` | `0-1` | high | threshold on soil water depletion fraction (0..1), clamped to [0,1], Raes et al. 2009, o3_impact.odin:146 |
| [ ] | [`O3_Impact_Params.lower_thr_stomatal`](monica/core/o3_impact.odin#L19) | 19 | `f64` | `0-1` | high | threshold on soil water depletion fraction, default 1, compared with sw_depletion_f, o3_impact.odin:170 |
| [ ] | [`O3_Impact_Params.fshape_stomatal`](monica/core/o3_impact.odin#L20) | 20 | `f64` | `1` | high | shape factor in exp(drel*fshape), o3_impact.odin:175; dimensionless |

## `O3_Impact_In` (monica/core/o3_impact.odin)

| ok | field | line | type | proposed unit | confidence | basis |
| --- | --- | --- | --- | --- | --- | --- |
| [ ] | [`O3_Impact_In.fc`](monica/core/o3_impact.odin#L38) | 38 | `f64` | `m3 m-3` | high | comment m3 m-3; avg of soil_layers field_capacity, crop_o3.odin:59 |
| [ ] | [`O3_Impact_In.wp`](monica/core/o3_impact.odin#L39) | 39 | `f64` | `m3 m-3` | high | comment m3 m-3; avg of permanent_wilting_point, crop_o3.odin:60 |
| [ ] | [`O3_Impact_In.swc`](monica/core/o3_impact.odin#L40) | 40 | `f64` | `m3 m-3` | high | comment m3 m-3; avg of soil_moisture_m3, crop_o3.odin:61 |
| [ ] | [`O3_Impact_In.et0`](monica/core/o3_impact.odin#L41) | 41 | `f64` | `mm d-1` | high | comment mm d-1; from reference_et0 (daily, tagged mm), crop_o3.odin:62; Raes adj uses (5-et0) |
| [ ] | [`O3_Impact_In.o3a`](monica/core/o3_impact.odin#L42) | 42 | `f64` | `ppb` | high | from atmospheric_o3_concentration `unit:"ppb"`, crop_o3.odin:63; comment nbar or nmol mol-1 |
| [ ] | [`O3_Impact_In.gs`](monica/core/o3_impact.odin#L43) | 43 | `f64` | `mol m-2 s-1 bar-1` | medium | comment says ground area, but crop_o3.odin:52-55 divides gs by LAI -> per leaf area; inconsistent |
| [ ] | [`O3_Impact_In.h`](monica/core/o3_impact.odin#L44) | 44 | `int` | `h` | high | hour of day 0-23, from O3_Hourly_In.hour `unit:"h"`; h==0 resets daily damage |
| [ ] | [`O3_Impact_In.reldev`](monica/core/o3_impact.odin#L45) | 45 | `f64` | `0-1` | high | from relative_total_development `unit:"0-1"`, crop_o3.odin:66 |
| [ ] | [`O3_Impact_In.gdd_flo`](monica/core/o3_impact.odin#L46) | 46 | `f64` | `°C d` | high | from temperature_sum_to_flowering `unit:"°C d"`, crop_o3.odin:67 |
| [ ] | [`O3_Impact_In.gdd_mat`](monica/core/o3_impact.odin#L47) | 47 | `f64` | `°C d` | high | from total_temperature_sum `unit:"°C d"`, crop_o3.odin:68 |
| [ ] | [`O3_Impact_In.f_o3s_d_prev`](monica/core/o3_impact.odin#L48) | 48 | `f64` | `0-1` | high | previous f_o3s_d (product of 0..1 reductions), Crop_O3.o3_short_term_damage tagged 0-1 |
| [ ] | [`O3_Impact_In.sum_o3_up`](monica/core/o3_impact.odin#L49) | 49 | `f64` | `umol m-2` | high | from Crop_O3.o3_sum_uptake `unit:"µmol m-2"`, crop_o3.odin:70 |

## `O3_Impact_Out` (monica/core/o3_impact.odin)

| ok | field | line | type | proposed unit | confidence | basis |
| --- | --- | --- | --- | --- | --- | --- |
| [ ] | [`O3_Impact_Out.hourly_o3_up`](monica/core/o3_impact.odin#L54) | 54 | `f64` | `umol m-2 h-1` | medium | comment umol m-2 h-1, but code is nmol m-2 s-1 /1000 without *3600 (o3_impact.odin:209): numerically µmol m-2 s-1 |
| [ ] | [`O3_Impact_Out.f_o3s_d`](monica/core/o3_impact.odin#L55) | 55 | `f64` | `0-1` | high | product of 0..1 hourly reduction factors, o3_impact.odin:213; stored as o3_short_term_damage 0-1 |
| [ ] | [`O3_Impact_Out.f_o3l`](monica/core/o3_impact.odin#L56) | 56 | `f64` | `0-1` | high | max(0.5, 1-gamma3*O3_tot_up), o3_impact.odin:122 |
| [ ] | [`O3_Impact_Out.f_ls`](monica/core/o3_impact.odin#L57) | 57 | `f64` | `0-1` | high | between 0.6 and 1, o3_impact.odin:132-142; multiplies vcmax_25 |
| [ ] | [`O3_Impact_Out.ws_st_clos`](monica/core/o3_impact.odin#L58) | 58 | `f64` | `0-1` | high | 1-(exp(drel*fshape)-1)/(exp(fshape)-1) with drel 0..1, o3_impact.odin:175; stored as 0-1 |

## `Fvcb_Canopy_Hourly_Params` (monica/core/photosynthesis_fvcb.odin)

| ok | field | line | type | proposed unit | confidence | basis |
| --- | --- | --- | --- | --- | --- | --- |
| [ ] | [`Fvcb_Canopy_Hourly_Params.vcmax_25`](monica/core/photosynthesis_fvcb.odin#L86) | 86 | `f64` | `umol m-2 s-1` | high | = species vcmax25 `unit:"umol m-2 s-1"` * O3 factors, crop_photosynthesis.odin:542 (per leaf area) |
| [ ] | [`Fvcb_Canopy_Hourly_Params.kn`](monica/core/photosynthesis_fvcb.odin#L87) | 87 | `f64` | `1` | high | dimensionless N allocation coefficient in exp(-kn), photosynthesis_fvcb.odin:369; default 0.713 |
| [ ] | [`Fvcb_Canopy_Hourly_Params.gb`](monica/core/photosynthesis_fvcb.odin#L88) | 88 | `f64` | `mol m-2 s-1 bar-1` | high | comment; per leaf area, multiplied by LAI to ground area, photosynthesis_fvcb.odin:629 |
| [ ] | [`Fvcb_Canopy_Hourly_Params.g0`](monica/core/photosynthesis_fvcb.odin#L89) | 89 | `f64` | `mol m-2 s-1 bar-1` | high | comment; per leaf area, multiplied by LAI, photosynthesis_fvcb.odin:631 |
| [ ] | [`Fvcb_Canopy_Hourly_Params.gm_25`](monica/core/photosynthesis_fvcb.odin#L90) | 90 | `f64` | `mol m-2 s-1 bar-1` | medium | comment; only used by unused fvcb_gm_bernacchi_f (gm_t hardcoded 0.4) - effectively unused |

## `Fvcb_Canopy_Hourly_In` (monica/core/photosynthesis_fvcb.odin)

| ok | field | line | type | proposed unit | confidence | basis |
| --- | --- | --- | --- | --- | --- | --- |
| [ ] | [`Fvcb_Canopy_Hourly_In.global_rad`](monica/core/photosynthesis_fvcb.odin#L100) | 100 | `f64` | `MJ m-2 h-1` | high | comment MJ m-2 h-1; *1e6/3600 -> W m-2, photosynthesis_fvcb.odin:618 |
| [ ] | [`Fvcb_Canopy_Hourly_In.extra_terr_rad`](monica/core/photosynthesis_fvcb.odin#L101) | 101 | `f64` | `MJ m-2 h-1` | high | comment MJ m-2 h-1; ratio globrad/extra_terr_rad in diffuse fraction |
| [ ] | [`Fvcb_Canopy_Hourly_In.solar_el`](monica/core/photosynthesis_fvcb.odin#L102) | 102 | `f64` | `rad` | high | comment radians; sin(solar_elev) in extinction coefficients |
| [ ] | [`Fvcb_Canopy_Hourly_In.lai`](monica/core/photosynthesis_fvcb.odin#L103) | 103 | `f64` | `m2 m-2` | high | comment m2 m-2; leaf area index |
| [ ] | [`Fvcb_Canopy_Hourly_In.leaf_temp`](monica/core/photosynthesis_fvcb.odin#L104) | 104 | `f64` | `°C` | high | comment degC; +273 to K in fvcb_tresp_bernacchi_f |
| [ ] | [`Fvcb_Canopy_Hourly_In.vpd`](monica/core/photosynthesis_fvcb.odin#L105) | 105 | `f64` | `kPa` | high | comment KPa; fvcb_f_vpd_f b1 = 0.15 kPa-1 |
| [ ] | [`Fvcb_Canopy_Hourly_In.ca`](monica/core/photosynthesis_fvcb.odin#L106) | 106 | `f64` | `ppm` | high | = atmospheric_co2_concentration (ppm = umol mol-1), crop_photosynthesis.odin:539 |

## `Fvcb_Leaf_Fraction` (monica/core/photosynthesis_fvcb.odin)

| ok | field | line | type | proposed unit | confidence | basis |
| --- | --- | --- | --- | --- | --- | --- |
| [ ] | [`Fvcb_Leaf_Fraction.lai`](monica/core/photosynthesis_fvcb.odin#L111) | 111 | `f64` | `m2 m-2` | high | sunlit/shaded LAI from fvcb_lai_sunlit_shaded_f |
| [ ] | [`Fvcb_Leaf_Fraction.gs`](monica/core/photosynthesis_fvcb.odin#L112) | 112 | `f64` | `mol m-2 s-1 bar-1` | medium | comment ground area: g0*LAI or derived from ground-area A; mixes leaf-area par.g0 (fvcb.odin:688) |
| [ ] | [`Fvcb_Leaf_Fraction.kc`](monica/core/photosynthesis_fvcb.odin#L113) | 113 | `f64` | `umol mol-1` | high | Bernacchi Kc ~404 at 25°C (umol mol-1=ubar), passed unscaled to VOC kc; comment 'mbar-1' is wrong |
| [ ] | [`Fvcb_Leaf_Fraction.ko`](monica/core/photosynthesis_fvcb.odin#L114) | 114 | `f64` | `mmol mol-1` | high | Bernacchi Ko ~278 at 25°C (mbar), crop_voc.odin *1000 to umol mol-1 |
| [ ] | [`Fvcb_Leaf_Fraction.oi`](monica/core/photosynthesis_fvcb.odin#L115) | 115 | `f64` | `mmol mol-1` | high | fvcb_oi_f ~210 (O2 mmol mol-1), crop_voc.odin *1000; comment 'umol m-2' is wrong |
| [ ] | [`Fvcb_Leaf_Fraction.ci`](monica/core/photosynthesis_fvcb.odin#L116) | 116 | `f64` | `umol mol-1` | high | derived intercellular CO2, same units as ca (ppm) in cubic; comment 'umol m-2' is wrong |
| [ ] | [`Fvcb_Leaf_Fraction.cc`](monica/core/photosynthesis_fvcb.odin#L117) | 117 | `f64` | `umol mol-1` | high | chloroplast CO2, same units as ca/ci; shaded.cc duplicates ci (C++ quirk); comment 'umol m-2' wrong |
| [ ] | [`Fvcb_Leaf_Fraction.comp`](monica/core/photosynthesis_fvcb.odin#L118) | 118 | `f64` | `umol mol-1` | high | comment; Gamma* = 0.5*Vomax/Vcmax*Kc*Oi/Ko (~43 at 25°C) |
| [ ] | [`Fvcb_Leaf_Fraction.vc_max`](monica/core/photosynthesis_fvcb.odin#L119) | 119 | `f64` | `umol m-2 s-1` | high | canopy Vc / fraction LAI -> per leaf area, photosynthesis_fvcb.odin:621 |
| [ ] | [`Fvcb_Leaf_Fraction.j_max`](monica/core/photosynthesis_fvcb.odin#L120) | 120 | `f64` | `umol m-2 s-1` | high | canopy Jmax / fraction LAI -> per leaf area, photosynthesis_fvcb.odin:622 |
| [ ] | [`Fvcb_Leaf_Fraction.rad`](monica/core/photosynthesis_fvcb.odin#L121) | 121 | `f64` | `W m-2` | high | share of hourly global rad (MJ m-2 h-1 *1e6/3600), photosynthesis_fvcb.odin:618-620; global, not PAR |
| [ ] | [`Fvcb_Leaf_Fraction.jj`](monica/core/photosynthesis_fvcb.odin#L122) | 122 | `f64` | `umol m-2 s-1` | high | J_c / fraction LAI -> per leaf area; not read by voc_jjv_gamma_ph (recomputed) |
| [ ] | [`Fvcb_Leaf_Fraction.jv`](monica/core/photosynthesis_fvcb.odin#L123) | 123 | `f64` | `umol m-2 s-1` | high | derive_jv_f / fraction LAI -> per leaf area; not read by VOC code |
| [ ] | [`Fvcb_Leaf_Fraction.jj1000`](monica/core/photosynthesis_fvcb.odin#L124) | 124 | `f64` | `umol m-2 s-1` | high | J at PAR=1000 umol m-2 s-1 per leaf area; not read by VOC code |

## `Fvcb_Canopy_Hourly_Out` (monica/core/photosynthesis_fvcb.odin)

| ok | field | line | type | proposed unit | confidence | basis |
| --- | --- | --- | --- | --- | --- | --- |
| [ ] | [`Fvcb_Canopy_Hourly_Out.canopy_net_photos`](monica/core/photosynthesis_fvcb.odin#L129) | 129 | `f64` | `umol m-2 h-1` | high | (A_sun+A_sh) [umol CO2 m-2 s-1 ground] *3600, photosynthesis_fvcb.odin:663 |
| [ ] | [`Fvcb_Canopy_Hourly_Out.canopy_resp`](monica/core/photosynthesis_fvcb.odin#L130) | 130 | `f64` | `umol m-2 h-1` | high | (rd_sun+rd_sh) [umol CO2 m-2 s-1 ground] *3600, photosynthesis_fvcb.odin:595 |
| [ ] | [`Fvcb_Canopy_Hourly_Out.canopy_gross_photos`](monica/core/photosynthesis_fvcb.odin#L131) | 131 | `f64` | `umol m-2 h-1` | high | net + resp; *44/100/1000 summed to daily, crop_photosynthesis.odin:550 |
| [ ] | [`Fvcb_Canopy_Hourly_Out.jmax_c`](monica/core/photosynthesis_fvcb.odin#L132) | 132 | `f64` | `umol m-2 s-1` | high | sum of sunlit+shaded canopy Jmax (ground area), photosynthesis_fvcb.odin:586 |

## `Fvcb_Lumped_Coeffs` (monica/core/photosynthesis_fvcb.odin)

| ok | field | line | type | proposed unit | confidence | basis |
| --- | --- | --- | --- | --- | --- | --- |
| [ ] | [`Fvcb_Lumped_Coeffs.p`](monica/core/photosynthesis_fvcb.odin#L434) | 434 | `f64` | `umol m-2 s-1` | medium | coefficient of A^2 in cubic for A (umol m-2 s-1), Yin & Struik 2009; -p/3 added to A |
| [ ] | [`Fvcb_Lumped_Coeffs.q`](monica/core/photosynthesis_fvcb.odin#L435) | 435 | `f64` | `umol2 m-4 s-2` | medium | Q=(p^2-3q)/9, sqrt(Q) added to A in fvcb_a1_f -> square of A units |
| [ ] | [`Fvcb_Lumped_Coeffs.psi`](monica/core/photosynthesis_fvcb.odin#L436) | 436 | `f64` | `rad` | high | psi = acos(...), used in cos(psi/3), photosynthesis_fvcb.odin:461 |

## `Snow_Component` (monica/core/snow_component.odin)

| ok | field | line | type | proposed unit | confidence | basis |
| --- | --- | --- | --- | --- | --- | --- |
| [ ] | [`Snow_Component.snow_accumulation_threshold_temperature`](monica/core/snow_component.odin#L24) | 24 | `f64` | `°C` | high | compared with mean air temperature, snow_component.odin:150; param tagged °C |
| [ ] | [`Snow_Component.temperature_limit_for_liquid_water`](monica/core/snow_component.odin#L25) | 25 | `f64` | `°C` | high | compared with mean air temperature, snow_component.odin:152; param tagged °C |
| [ ] | [`Snow_Component.correction_rain`](monica/core/snow_component.odin#L26) | 26 | `f64` | `1` | high | multiplier on net precipitation (mm -> mm), snow_component.odin:160; param tagged 1 |
| [ ] | [`Snow_Component.correction_snow`](monica/core/snow_component.odin#L27) | 27 | `f64` | `1` | high | multiplier on net precipitation (mm -> mm), snow_component.odin:162; param tagged 1 |
| [ ] | [`Snow_Component.refreeze_p1`](monica/core/snow_component.odin#L29) | 29 | `f64` | `1` | low | coefficient in p1*(Trefreeze-T)^p2 giving refreeze in mm, snow_component.odin:183; strictly mm °C^-p2; param tag guessed 1 |
| [ ] | [`Snow_Component.refreeze_p2`](monica/core/snow_component.odin#L30) | 30 | `f64` | `1` | high | exponent in p1*(Trefreeze-T)^p2, snow_component.odin:184 |
| [ ] | [`Snow_Component.new_snow_density_min`](monica/core/snow_component.odin#L31) | 31 | `f64` | `kg dm-3` | medium | added to snow_max_additional_density and compared with snow_density (kg dm-3), snow_component.odin:236 |
| [ ] | [`Snow_Component.snow_max_additional_density`](monica/core/snow_component.odin#L32) | 32 | `f64` | `kg dm-3` | medium | summed with new_snow_density_min to cap snow_density (kg dm-3), snow_component.odin:237 |
| [ ] | [`Snow_Component.snow_packing`](monica/core/snow_component.odin#L33) | 33 | `f64` | `1` | medium | dimensionless densification factor (1+packing)*density, snow_component.odin:233 |
| [ ] | [`Snow_Component.snow_retention_capacity_min`](monica/core/snow_component.odin#L34) | 34 | `f64` | `0-1` | medium | lower bound of retention capacity, multiplied with snow water equivalent (mm) -> mm, snow_component.odin:257-265 |
| [ ] | [`Snow_Component.snow_retention_capacity_max`](monica/core/snow_component.odin#L35) | 35 | `f64` | `0-1` | medium | upper bound of retention capacity (fraction of SWE retained), snow_component.odin:255-265 |

## `Aom_Properties` (monica/core/soil_column.odin)

| ok | field | line | type | proposed unit | confidence | basis |
| --- | --- | --- | --- | --- | --- | --- |
| [ ] | [`Aom_Properties.aom_slow_dec_rate_to_smb_slow`](monica/core/soil_column.odin#L26) | 26 | `f64` | `kgC m-3 d-1` | high | part (0-1) * aom_slow_dec_coeff (d-1) * aom_slow (kgC m-3), soil_organic.odin:460 |
| [ ] | [`Aom_Properties.aom_slow_dec_rate_to_smb_fast`](monica/core/soil_column.odin#L27) | 27 | `f64` | `kgC m-3 d-1` | high | part (0-1) * aom_slow_dec_coeff (d-1) * aom_slow (kgC m-3), soil_organic.odin:462 |
| [ ] | [`Aom_Properties.aom_fast_dec_rate_to_smb_slow`](monica/core/soil_column.odin#L28) | 28 | `f64` | `kgC m-3 d-1` | low | never assigned or read (unused); by analogy with aom_fast_dec_rate_to_smb_fast |
| [ ] | [`Aom_Properties.aom_fast_dec_rate_to_smb_fast`](monica/core/soil_column.odin#L29) | 29 | `f64` | `kgC m-3 d-1` | high | aom_fast_dec_coeff (d-1) * aom_fast (kgC m-3), soil_organic.odin:477 |
| [ ] | [`Aom_Properties.aom_slow_dec_coeff`](monica/core/soil_column.odin#L30) | 30 | `f64` | `d-1` | high | aom_slow_dec_coeff_standard (d-1) * temperature/moisture factors, soil_organic.odin:435 |
| [ ] | [`Aom_Properties.aom_fast_dec_coeff`](monica/core/soil_column.odin#L31) | 31 | `f64` | `d-1` | high | aom_fast_dec_coeff_standard (d-1) * temperature/moisture factors, soil_organic.odin:436 |
| [ ] | [`Aom_Properties.aom_slow_dec_coeff_standard`](monica/core/soil_column.odin#L32) | 32 | `f64` | `d-1` | high | copied from organic param aom_slow_dec_coeff_standard tagged d-1, soil_organic.odin:1442 |
| [ ] | [`Aom_Properties.aom_fast_dec_coeff_standard`](monica/core/soil_column.odin#L33) | 33 | `f64` | `d-1` | high | copied from organic param aom_fast_dec_coeff_standard tagged d-1, soil_organic.odin:1443 |
| [ ] | [`Aom_Properties.part_aom_slow_to_smb_slow`](monica/core/soil_column.odin#L34) | 34 | `f64` | `0-1` | high | partitioning fraction from organic params (tagged kg kg-1), soil_organic.odin:1447,461 |
| [ ] | [`Aom_Properties.part_aom_slow_to_smb_fast`](monica/core/soil_column.odin#L35) | 35 | `f64` | `0-1` | high | partitioning fraction from organic params (tagged kg kg-1), soil_organic.odin:1448,463 |
| [ ] | [`Aom_Properties.days_after_application`](monica/core/soil_column.odin#L38) | 38 | `int` | `d` | high | set to 1 on application, +1 per day, used in volatilisation half-life, soil_organic.odin:962,993 |
| [ ] | [`Aom_Properties.aom_dry_matter_content`](monica/core/soil_column.odin#L39) | 39 | `f64` | `kg DM kg FM-1` | high | copied from organic param tagged 'kg DM kg FM-1', soil_organic.odin:1458 |
| [ ] | [`Aom_Properties.aom_nh4_content`](monica/core/soil_column.odin#L40) | 40 | `f64` | `kgN kgDM-1` | high | copied from organic param tagged 'kg N kg DM-1', soil_organic.odin:1459 |
| [ ] | [`Aom_Properties.aom_slow_delta`](monica/core/soil_column.odin#L41) | 41 | `f64` | `kgC m-3` | high | -(dec_coeff*aom_slow), added directly to aom_slow (kgC m-3) each day, soil_organic.odin:446,1254 |
| [ ] | [`Aom_Properties.aom_fast_delta`](monica/core/soil_column.odin#L42) | 42 | `f64` | `kgC m-3` | high | -(dec_coeff*aom_fast), added directly to aom_fast (kgC m-3) each day, soil_organic.odin:450,1255 |

## `Soil_Layer` (monica/core/soil_column.odin)

| ok | field | line | type | proposed unit | confidence | basis |
| --- | --- | --- | --- | --- | --- | --- |
| [ ] | [`Soil_Layer.soil_cn_ratio`](monica/core/soil_column.odin#L84) | 84 | `f64` | `1` | high | C/N ratio from soil param 'CN' (default 10), used in soil_organic.odin:119 inert C calc |
| [ ] | [`Soil_Layer.soil_moisture_percent_fc`](monica/core/soil_column.odin#L85) | 85 | `f64` | `%` | high | SoilMoisturePercentFC, output unit '% [0-100]' soil_parameters.odin:411; used /100 in soil_column.odin make_soil_layer |

## `Delayed_N_Min_Application_Params` (monica/core/soil_column.odin)

| ok | field | line | type | proposed unit | confidence | basis |
| --- | --- | --- | --- | --- | --- | --- |
| [ ] | [`Delayed_N_Min_Application_Params.sampling_depth`](monica/core/soil_column.odin#L238) | 238 | `f64` | `m` | high | passed to get_layer_number_for_depth like 0.3 m, soil_column.odin:451; source param sampling_depth tagged m |
| [ ] | [`Delayed_N_Min_Application_Params.crop_n_target`](monica/core/soil_column.odin#L239) | 239 | `f64` | `kgN ha-1` | high | comment 'Converts [kg N ha-1] to [kg N m-3]' at soil_column.odin:467; source n_target tagged kg N ha-1 |
| [ ] | [`Delayed_N_Min_Application_Params.crop_n_target_30`](monica/core/soil_column.odin#L240) | 240 | `f64` | `kgN ha-1` | high | converted /10000/thickness to kg N m-3, soil_column.odin:469; source n_target30 tagged kg N ha-1 |
| [ ] | [`Delayed_N_Min_Application_Params.fertilizer_min_application`](monica/core/soil_column.odin#L241) | 241 | `f64` | `kgN ha-1` | high | compared with fertiliser_recommendation (kg N ha-1), soil_column.odin:481 |
| [ ] | [`Delayed_N_Min_Application_Params.fertilizer_max_application`](monica/core/soil_column.odin#L242) | 242 | `f64` | `kgN ha-1` | high | compared with/subtracted from fertiliser_recommendation (kg N ha-1), soil_column.odin:484 |
| [ ] | [`Delayed_N_Min_Application_Params.top_dressing_delay`](monica/core/soil_column.odin#L243) | 243 | `int` | `d` | high | copied to Soil_Column.top_dressing_delay, decremented once per day, soil_column.odin:565 |

## `Soil_Column` (monica/core/soil_column.odin)

| ok | field | line | type | proposed unit | confidence | basis |
| --- | --- | --- | --- | --- | --- | --- |
| [ ] | [`Soil_Column.groundwater_table_layer_idx`](monica/core/soil_column.odin#L251) | 251 | `int` | `layer#` | high | set from Soil_Moisture.groundwater_table_layer (layer index), soil_moisture.odin:221 |
| [ ] | [`Soil_Column.snow_depth_mm`](monica/core/soil_column.odin#L255) | 255 | `f64` | `mm` | high | copied from Snow_Component.snow_depth (mm), snow_component.odin:305; crop_stress input tagged mm |
| [ ] | [`Soil_Column.max_mineralisation_depth_m`](monica/core/soil_column.odin#L256) | 256 | `f64` | `m` | high | name; summed against layer_thickness_m in calculate_number_of_organic_layers; default 0.4 |
| [ ] | [`Soil_Column.number_of_organic_layers`](monica/core/soil_column.odin#L257) | 257 | `int` | `layer#` | high | count of layers within max_mineralisation_depth, soil_column.odin calculate_number_of_organic_layers |
| [ ] | [`Soil_Column.top_dressing`](monica/core/soil_column.odin#L258) | 258 | `f64` | `kgN ha-1` | high | fertiliser_recommendation - max_application (kg N ha-1), soil_column.odin:485 |
| [ ] | [`Soil_Column.top_dressing_delay`](monica/core/soil_column.odin#L260) | 260 | `int` | `d` | high | decremented once per daily call of apply_possible_top_dressing, soil_column.odin:565 |

## `Soil_Moisture` (monica/core/soil_moisture.odin)

| ok | field | line | type | proposed unit | confidence | basis |
| --- | --- | --- | --- | --- | --- | --- |
| [ ] | [`Soil_Moisture.no_of_soil_layers`](monica/core/soil_moisture.odin#L44) | 44 | `int` | `layer#` | high | len(sc.layers), soil_moisture.odin:117 |
| [ ] | [`Soil_Moisture.field_capacity_below_m3_m3`](monica/core/soil_moisture.odin#L53) | 53 | `f64` | `m3 m-3` | high | copied from bottom layer field_capacity (m3 m-3), soil_moisture.odin:183 |
| [ ] | [`Soil_Moisture.gravitational_water`](monica/core/soil_moisture.odin#L54) | 54 | `[dynamic]f64` | `mm` | high | converted via /1000/layer_thickness_m to m3 m-3, soil_moisture.odin:422; reduced by percolation_rate (mm d-1) |
| [ ] | [`Soil_Moisture.groundwater_added`](monica/core/soil_moisture.odin#L55) | 55 | `f64` | `mm` | high | (sm - pore volume)*1000*layer_thickness_m, soil_moisture.odin:589; added to surface_water_storage (mm) |
| [ ] | [`Soil_Moisture.groundwater_table_layer`](monica/core/soil_moisture.odin#L56) | 56 | `int` | `layer#` | high | layer index of groundwater table, compared with ib/no_of_soil_layers, soil_moisture.odin:206-221 |
| [ ] | [`Soil_Moisture.interception`](monica/core/soil_moisture.odin#L59) | 59 | `f64` | `mm` | low | only reset to 0 at soil_moisture.odin:317, never read (unused); interception storage in C++ is mm |
| [ ] | [`Soil_Moisture.lambda_below`](monica/core/soil_moisture.odin#L61) | 61 | `f64` | `1` | high | copied from bottom layer lambda (tagged 1), soil_moisture.odin:185 |
| [ ] | [`Soil_Moisture.layer_thickness_m`](monica/core/soil_moisture.odin#L62) | 62 | `f64` | `m` | high | from p_layer_thickness, used as /1000/layer_thickness_m mm->m3 m-3, soil_moisture.odin:133 |
| [ ] | [`Soil_Moisture.leaching_depth_layer_idx`](monica/core/soil_moisture.odin#L63) | 63 | `int` | `layer#` | high | floor(0.5 + leaching_depth_m/layer_thickness) - 1, soil_moisture.odin:134 |
| [ ] | [`Soil_Moisture.net_precipitation_mm`](monica/core/soil_moisture.odin#L64) | 64 | `f64` | `mm` | high | set from precipitation_mm, passed to calc_snow_layer, soil_moisture.odin:195-224 |
| [ ] | [`Soil_Moisture.soil_moisture_below_m3_m3`](monica/core/soil_moisture.odin#L74) | 74 | `f64` | `m3 m-3` | high | copied from bottom layer soil_moisture_m3, soil_moisture.odin:181 |
| [ ] | [`Soil_Moisture.soil_moisture_crit`](monica/core/soil_moisture.odin#L75) | 75 | `f64` | `m3 m-3` | low | never assigned or read (unused, C++ vm_SoilMoisture_crit only serialized); name suggests critical moisture |
| [ ] | [`Soil_Moisture.pore_volume_below_m3_m3`](monica/core/soil_moisture.odin#L77) | 77 | `f64` | `m3 m-3` | high | copied from bottom layer saturation (m3 m-3), soil_moisture.odin:184 |
| [ ] | [`Soil_Moisture.sum_surface_run_off`](monica/core/soil_moisture.odin#L79) | 79 | `f64` | `mm` | high | sum of surface_run_off (mm), soil_moisture.odin:453; never read |
| [ ] | [`Soil_Moisture.total_water_removal`](monica/core/soil_moisture.odin#L80) | 80 | `f64` | `m3` | low | never used (unused); only C++ comment 'Total water removal of layer [m3]' soilmoisture.h:248 |
| [ ] | [`Soil_Moisture.water_flux_below`](monica/core/soil_moisture.odin#L82) | 82 | `f64` | `mm` | high | assigned to sc.flux_at_lower_boundary (tagged mm), soil_moisture.odin:613 |
| [ ] | [`Soil_Moisture.xsa_critical_soil_moisture`](monica/core/soil_moisture.odin#L83) | 83 | `f64` | `0-1` | medium | acts as reducer factor multiplying field_capacity, soil_moisture.odin:1328-1330; param tag 'm3 m-3' (guess) looks wrong |

## `Soil_Organic` (monica/core/soil_organic.odin)

| ok | field | line | type | proposed unit | confidence | basis |
| --- | --- | --- | --- | --- | --- | --- |
| [ ] | [`Soil_Organic.irrigation_amount`](monica/core/soil_organic.odin#L22) | 22 | `f64` | `mm` | high | accumulates irrigation amounts also passed to daily_sum_irrigation_water, monica_model.odin:945; reset daily, never read |
| [ ] | [`Soil_Organic.aom_fast_delta_sum`](monica/core/soil_organic.odin#L26) | 26 | `[dynamic]f64` | `kgC m-3` | high | sum of pool aom_fast_delta (kgC m-3), added into c_balance, soil_organic.odin:1258,1273 |
| [ ] | [`Soil_Organic.aom_fast_sum`](monica/core/soil_organic.odin#L28) | 28 | `[dynamic]f64` | `kgC m-3` | high | sum of pool aom_fast (kgC m-3), soil_organic.odin:1261; output 'AOMf' kgC m-3 |
| [ ] | [`Soil_Organic.aom_slow_delta_sum`](monica/core/soil_organic.odin#L29) | 29 | `[dynamic]f64` | `kgC m-3` | high | sum of pool aom_slow_delta (kgC m-3), added into c_balance, soil_organic.odin:1257,1272 |
| [ ] | [`Soil_Organic.aom_slow_sum`](monica/core/soil_organic.odin#L31) | 31 | `[dynamic]f64` | `kgC m-3` | high | sum of pool aom_slow (kgC m-3), soil_organic.odin:1260; output 'AOMs' kgC m-3 |
| [ ] | [`Soil_Organic.c_balance`](monica/core/soil_organic.odin#L32) | 32 | `[dynamic]f64` | `kgC m-3` | high | sum of kgC m-3 inputs/deltas, added to soil_organic_c, soil_organic.odin:1269-1283; output 'CBal' kgC m-3 |
| [ ] | [`Soil_Organic.decomposer_respiration`](monica/core/soil_organic.odin#L33) | 33 | `f64` | `kgC m-2` | high | sum smb_co2_evolution_rate*thickness '[kg C m-3] -> [kg C m-2]' soil_organic.odin:678; getter *10000 -> kgC ha-1 |
| [ ] | [`Soil_Organic.inert_soil_organic_c`](monica/core/soil_organic.odin#L35) | 35 | `[dynamic]f64` | `kgC m-3` | high | Falloon IOM in t ha-1 converted /10000*1000/thickness, soil_organic.odin:132-153; subtracted from soil_organic_c |
| [ ] | [`Soil_Organic.inert_soil_organic_c_high_cn`](monica/core/soil_organic.odin#L36) | 36 | `[dynamic]f64` | `kgC m-3` | high | converted /10000*1000/thickness and subtracted from soil_organic_c (kgC m-3), soil_organic.odin:120-130 |
| [ ] | [`Soil_Organic.net_n_mineralisation`](monica/core/soil_organic.odin#L42) | 42 | `f64` | `kgN m-2` | high | sum /n_balance/ (kgN m-3) * thickness '[kg m-3] --> [kg m-2]' soil_organic.odin:651; getter *10000 -> 'NetNmin' kgN ha-1 |
| [ ] | [`Soil_Organic.net_n_mineralisation_rate`](monica/core/soil_organic.odin#L43) | 43 | `[dynamic]f64` | `kgN m-2` | high | /n_balance/*lay0 thickness '[kg m-2]' soil_organic.odin:650; getter *10000 -> 'Nmin' kgN ha-1 (daily amount) |
| [ ] | [`Soil_Organic.total_nh3_volatilised`](monica/core/soil_organic.odin#L44) | 44 | `f64` | `kgN m-2` | high | comment '[kg N m-2]' soil_organic.odin:989; getter *10000 -> output 'NH3' kgN ha-1 |
| [ ] | [`Soil_Organic.nh3_volatilised`](monica/core/soil_organic.odin#L45) | 45 | `f64` | `kgN m-2` | high | nh3_volatilising (kgN m-3) * layer thickness, comment 'kg N m-2 d-1', soil_organic.odin:306 |
| [ ] | [`Soil_Organic.smb_co2_evolution_rate`](monica/core/soil_organic.odin#L46) | 46 | `[dynamic]f64` | `kgC m-3 d-1` | high | sum of (1-eff)*decomposition rates (kgC m-3 d-1), soil_organic.odin:675; output 'SMB-CO2-ER' has empty unit |
| [ ] | [`Soil_Organic.smb_fast_delta`](monica/core/soil_organic.odin#L47) | 47 | `[dynamic]f64` | `kgC m-3` | high | added directly to layer smb_fast (kgC m-3), soil_organic.odin:1267 |
| [ ] | [`Soil_Organic.smb_slow_delta`](monica/core/soil_organic.odin#L48) | 48 | `[dynamic]f64` | `kgC m-3` | high | added directly to layer smb_slow (kgC m-3), soil_organic.odin:1266 |
| [ ] | [`Soil_Organic.soil_organic_c`](monica/core/soil_organic.odin#L50) | 50 | `[dynamic]f64` | `kgC m-3` | high | soil_organic_carbon (kg kg-1) * bulk density (kg m-3), soil_organic.odin:117,1280 |
| [ ] | [`Soil_Organic.soil_organic_c_high_cn`](monica/core/soil_organic.odin#L51) | 51 | `[dynamic]f64` | `kgC m-3` | high | soil_organic_c - inert_soil_organic_c_high_cn (kgC m-3), soil_organic.odin:128 |
| [ ] | [`Soil_Organic.som_fast_delta`](monica/core/soil_organic.odin#L52) | 52 | `[dynamic]f64` | `kgC m-3` | high | added directly to layer som_fast (kgC m-3), soil_organic.odin:1265 |
| [ ] | [`Soil_Organic.som_slow_delta`](monica/core/soil_organic.odin#L54) | 54 | `[dynamic]f64` | `kgC m-3` | high | added directly to layer som_slow (kgC m-3), soil_organic.odin:1264 |
| [ ] | [`Soil_Organic.sum_denitrification`](monica/core/soil_organic.odin#L55) | 55 | `f64` | `kgN m-2` | high | sum of total_denitrification '[kg N m-2]' soil_organic.odin:1124; field comment kg-N/m2; getter *10000 |
| [ ] | [`Soil_Organic.sum_net_n_mineralisation`](monica/core/soil_organic.odin#L56) | 56 | `f64` | `kgN m-2` | high | accumulates /n_balance/*thickness '[kg m-2]' soil_organic.odin:652; getter *10000 -> kgN ha-1 |
| [ ] | [`Soil_Organic.sum_n2o_produced`](monica/core/soil_organic.odin#L57) | 57 | `f64` | `kgN ha-1` | high | sum of n2o_produced (tagged kgN ha-1), soil_organic.odin:1669; getter returns as is |
| [ ] | [`Soil_Organic.sum_nh3_volatilised`](monica/core/soil_organic.odin#L58) | 58 | `f64` | `kgN m-2` | high | sum of nh3_volatilised (kgN m-2), soil_organic.odin:1668; getter *10000 -> kgN ha-1 |
| [ ] | [`Soil_Organic.total_denitrification`](monica/core/soil_organic.odin#L59) | 59 | `f64` | `kgN m-2` | high | sum act_denitrification_rate*thickness '[kg m-3] --> [kg m-2]' soil_organic.odin:1121; getter *10000 |

## `Soil_Temperature` (monica/core/soil_temperature.odin)

| ok | field | line | type | proposed unit | confidence | basis |
| --- | --- | --- | --- | --- | --- | --- |
| [ ] | [`Soil_Temperature.no_of_temp_layers`](monica/core/soil_temperature.odin#L33) | 33 | `int` | `layer#` | high | len(layers) + 2 (ground and bottom layer), soil_temperature.odin:81 |
| [ ] | [`Soil_Temperature.no_of_soil_layers`](monica/core/soil_temperature.odin#L34) | 34 | `int` | `layer#` | high | len(soil_column.layers), soil_temperature.odin:82 |
| [ ] | [`Soil_Temperature.v`](monica/core/soil_temperature.odin#L36) | 36 | `[dynamic]f64` | `m` | medium | layer_thickness_m * n_tau (1), soil_temperature.odin:146; C++ comment says [m3] (volume per m2 ground) |
| [ ] | [`Soil_Temperature.volume_matrix`](monica/core/soil_temperature.odin#L37) | 37 | `[dynamic]f64` | `J m-2 K-1` | medium | v (m) * heat_capacity (J m-3 K-1); C++ comment [J K-1] (per m2 ground), soiltemperature.cpp:189 |
| [ ] | [`Soil_Temperature.volume_matrix_old`](monica/core/soil_temperature.odin#L38) | 38 | `[dynamic]f64` | `J m-2 K-1` | medium | copy of volume_matrix, soil_temperature.odin:200 |
| [ ] | [`Soil_Temperature.b`](monica/core/soil_temperature.odin#L39) | 39 | `[dynamic]f64` | `m-1` | high | 2/(lt_i + lt_i-1) with thicknesses in m, soil_temperature.odin:145; C++ comment '[m]' is wrong |
| [ ] | [`Soil_Temperature.matrix_primary_diagonal`](monica/core/soil_temperature.odin#L40) | 40 | `[dynamic]f64` | `J m-2 K-1` | medium | volume_matrix - secondary diagonals; C++ comment [J K-1], soiltemperature.cpp:205 |
| [ ] | [`Soil_Temperature.matrix_secondary_diagonal`](monica/core/soil_temperature.odin#L41) | 41 | `[dynamic]f64` | `J m-2 K-1` | medium | -b (m-1) * heat_conductivity_mean (per time step); C++ comment [J K-1], soiltemperature.cpp:195 |
| [ ] | [`Soil_Temperature.heat_conductivity`](monica/core/soil_temperature.odin#L42) | 42 | `[dynamic]f64` | `J m-1 d-1 K-1` | medium | C++ comment '--> [J m-1 d-1 K-1]' soiltemperature.cpp:143 (includes *time_step, so per step) |
| [ ] | [`Soil_Temperature.heat_conductivity_mean`](monica/core/soil_temperature.odin#L43) | 43 | `[dynamic]f64` | `J m-1 d-1 K-1` | medium | thickness-weighted mean of heat_conductivity, soil_temperature.odin:182 |
| [ ] | [`Soil_Temperature.heat_capacity`](monica/core/soil_temperature.odin#L44) | 44 | `[dynamic]f64` | `J m-3 K-1` | high | sum of fraction*density(kg m-3)*specific heat(J kg-1 K-1); C++ comment '--> [J m-3 K-1]' soiltemperature.cpp:157 |
| [ ] | [`Soil_Temperature.damping_factor`](monica/core/soil_temperature.odin#L45) | 45 | `f64` | `0-1` | high | 0.8, weighs soil_coverage vs 1-soil_coverage in shading coefficient, soil_temperature.odin:291 |
| [ ] | [`Soil_Temperature.solution`](monica/core/soil_temperature.odin#L47) | 47 | `[dynamic]f64` | `°C` | low | holds J m-2 (volume_matrix*T + heat_flow) as RHS, then solved temperatures copied to soil_temperature (°C); mixed use |
| [ ] | [`Soil_Temperature.matrix_diagonal`](monica/core/soil_temperature.odin#L48) | 48 | `[dynamic]f64` | `J m-2 K-1` | medium | LU diagonal from matrix_primary_diagonal, soil_temperature.odin:241-246 |
| [ ] | [`Soil_Temperature.matrix_lower_triangle`](monica/core/soil_temperature.odin#L49) | 49 | `[dynamic]f64` | `1` | medium | ratio secondary_diagonal / matrix_diagonal (same units), soil_temperature.odin:243 |
| [ ] | [`Soil_Temperature.heat_flow`](monica/core/soil_temperature.odin#L50) | 50 | `[dynamic]f64` | `J m-2` | medium | soil_surface_temperature * b * heat_conductivity_mean, C++ comment [J], soil_temperature.odin:230 |

## `Soil_Transport` (monica/core/soil_transport.odin)

| ok | field | line | type | proposed unit | confidence | basis |
| --- | --- | --- | --- | --- | --- | --- |
| [ ] | [`Soil_Transport.diffusion_coeff`](monica/core/soil_transport.odin#L24) | 24 | `[dynamic]f64` | `m2 d-1` | medium | D0 [m2 d-1] * factors * time_step_factor ('[m2 t-1]'), soil_transport.odin:262; per sub-step |
| [ ] | [`Soil_Transport.dispersion`](monica/core/soil_transport.odin#L25) | 25 | `[dynamic]f64` | `kgN m-3 d-1` | medium | dispersion_coeff*conc diff/lt^2, combined with convection (kgN m-3 d-1), soil_transport.odin:355; per sub-step |
| [ ] | [`Soil_Transport.dispersion_coeff`](monica/core/soil_transport.odin#L26) | 26 | `[dynamic]f64` | `m2 d-1` | medium | moisture*(D + dispersion_length*velocity) - 0.5*lt*/pr/ (m2 per step), soil_transport.odin:268 |
| [ ] | [`Soil_Transport.n_uptake_from_layer`](monica/core/soil_transport.odin#L28) | 28 | `[dynamic]f64` | `kgN m-2` | high | compared with soil_no3*lti - minimum_available_n, comment 'Crop N uptake from layer i [kg N m-2]' soil_transport.odin:135 |
| [ ] | [`Soil_Transport.pore_water_velocity`](monica/core/soil_transport.odin#L29) | 29 | `[dynamic]f64` | `m d-1` | medium | /percolation (m t-1) / field capacity/ commented '[m t-1]', soil_transport.odin:250; per sub-step |
| [ ] | [`Soil_Transport.soil_no3_aq`](monica/core/soil_transport.odin#L32) | 32 | `[dynamic]f64` | `kgN m-3` | high | soil_no3 (kgN m-3 soil) / soil moisture -> 'kg m solute-3', soil_transport.odin:148 (per m3 soil water) |
| [ ] | [`Soil_Transport.time_step`](monica/core/soil_transport.odin#L33) | 33 | `f64` | `d` | low | set to 1.0 and never read (unused; env_params.time_step is used instead), soil_transport.odin:57 |
| [ ] | [`Soil_Transport.total_dispersion`](monica/core/soil_transport.odin#L34) | 34 | `[dynamic]f64` | `kgN m-3 d-1` | low | only resized, never assigned or read (unused); by analogy with dispersion |

## `Voc_Cp_Data` (monica/core/voc_common.odin)

| ok | field | line | type | proposed unit | confidence | basis |
| --- | --- | --- | --- | --- | --- | --- |
| [ ] | [`Voc_Cp_Data.kc`](monica/core/voc_common.odin#L76) | 76 | `f64` | `umol mol-1` | high | = Fvcb kc (Bernacchi, umol mol-1 = ubar); km = kc*(1+oi/ko) added to ci, voc_jjv.odin:25 |
| [ ] | [`Voc_Cp_Data.ko`](monica/core/voc_common.odin#L77) | 77 | `f64` | `umol mol-1` | high | = Fvcb ko (mmol mol-1) *1000, crop_voc.odin:128; comment swaps oi/ci descriptions |
| [ ] | [`Voc_Cp_Data.oi`](monica/core/voc_common.odin#L78) | 78 | `f64` | `umol mol-1` | high | = Fvcb oi (mmol mol-1) *1000, crop_voc.odin:129; it is O2, comment mislabels it as CO2 |
| [ ] | [`Voc_Cp_Data.ci`](monica/core/voc_common.odin#L79) | 79 | `f64` | `umol mol-1` | high | = Fvcb ci; crop_photosynthesis.odin:250 comment [umol mol-1]; comment 'umol m-2' wrong |
| [ ] | [`Voc_Cp_Data.comp`](monica/core/voc_common.odin#L80) | 80 | `f64` | `umol mol-1` | high | crop_photosynthesis.odin:255 comment [umol mol-1]; ci/comp ratio in voc_jjv.odin:40 |
| [ ] | [`Voc_Cp_Data.vc_max`](monica/core/voc_common.odin#L81) | 81 | `f64` | `umol m-2 s-1` | high | vcmax_bernacchi(t, vcmax25 umol m-2 s-1)*stress, crop_voc.odin:131 (per leaf area) |
| [ ] | [`Voc_Cp_Data.j_max`](monica/core/voc_common.odin#L82) | 82 | `f64` | `umol m-2 s-1` | high | jmax_bernacchi(t,120)*stress, crop_voc.odin:135; combined with PAR in voc_jjv.odin:28 |
| [ ] | [`Voc_Cp_Data.jj`](monica/core/voc_common.odin#L83) | 83 | `f64` | `umol m-2 s-1` | medium | comment; copied from Fvcb jj but never read (gamma_ph recomputes jj locally) |
| [ ] | [`Voc_Cp_Data.jj1000`](monica/core/voc_common.odin#L84) | 84 | `f64` | `umol m-2 s-1` | medium | comment; copied from Fvcb jj1000 but never read |
| [ ] | [`Voc_Cp_Data.jv`](monica/core/voc_common.odin#L85) | 85 | `f64` | `umol m-2 s-1` | medium | comment; copied from Fvcb jv but never read (gamma_ph recomputes jv) |

## `Voc_Species_Data` (monica/core/voc_common.odin)

| ok | field | line | type | proposed unit | confidence | basis |
| --- | --- | --- | --- | --- | --- | --- |
| [ ] | [`Voc_Species_Data.id`](monica/core/voc_common.odin#L90) | 90 | `int` | `` | high | species id used as map key in Voc_Emissions |
| [ ] | [`Voc_Species_Data.ef_monos`](monica/core/voc_common.odin#L93) | 93 | `f64` | `ug gDW-1 h-1` | high | comment; copied from crop param ef_monos `unit:"ug gDW-1 h-1"`, crop_voc.odin:109 |
| [ ] | [`Voc_Species_Data.ef_mono`](monica/core/voc_common.odin#L94) | 94 | `f64` | `ug gDW-1 h-1` | high | comment; copied from crop param ef_mono `unit:"ug gDW-1 h-1"`; code treats it as ugC (c0 uses MC) |
| [ ] | [`Voc_Species_Data.ef_iso`](monica/core/voc_common.odin#L95) | 95 | `f64` | `ug gDW-1 h-1` | high | comment; copied from crop param ef_iso `unit:"ug gDW-1 h-1"`; code treats it as ugC (c0 uses MC) |
| [ ] | [`Voc_Species_Data.theta`](monica/core/voc_common.odin#L98) | 98 | `f64` | `1` | high | curvature of non-rectangular hyperbola, default 0.9, voc_jjv.odin:28 |
| [ ] | [`Voc_Species_Data.fage`](monica/core/voc_common.odin#L99) | 99 | `f64` | `1` | medium | leaf age activity factor gamma_a (default 1.0), multiplies emission, voc_jjv.odin:111 |
| [ ] | [`Voc_Species_Data.ct_is`](monica/core/voc_common.odin#L100) | 100 | `f64` | `1` | high | dimensionless constant inside exp(ct_is - ha/(R T)), voc_jjv.odin:72; never set (0) |
| [ ] | [`Voc_Species_Data.ct_mt`](monica/core/voc_common.odin#L101) | 101 | `f64` | `1` | high | dimensionless constant inside exp(ct_mt - ha/(R T)), voc_jjv.odin:75; never set (0) |
| [ ] | [`Voc_Species_Data.ha_is`](monica/core/voc_common.odin#L103) | 103 | `f64` | `J mol-1` | high | comment; ha/(R[J mol-1 K-1]*T[K]), voc_jjv.odin:72; never set (0) |
| [ ] | [`Voc_Species_Data.ha_mt`](monica/core/voc_common.odin#L104) | 104 | `f64` | `J mol-1` | high | comment; ha/(R*T), voc_jjv.odin:75; never set (0) |
| [ ] | [`Voc_Species_Data.ds_is`](monica/core/voc_common.odin#L106) | 106 | `f64` | `J mol-1 K-1` | high | comment J mol-1 K-1; ds*T-hd over R*T, voc_jjv.odin:73; never set (0) |
| [ ] | [`Voc_Species_Data.ds_mt`](monica/core/voc_common.odin#L107) | 107 | `f64` | `J mol-1 K-1` | high | comment J mol-1 K-1; ds*T-hd over R*T, voc_jjv.odin:76; never set (0) |
| [ ] | [`Voc_Species_Data.hd_is`](monica/core/voc_common.odin#L108) | 108 | `f64` | `J mol-1` | high | comment; ds*T-hd over R*T, voc_jjv.odin:73; default 284600 |
| [ ] | [`Voc_Species_Data.hd_mt`](monica/core/voc_common.odin#L109) | 109 | `f64` | `J mol-1` | high | comment; ds*T-hd over R*T, voc_jjv.odin:76; default 284600 |
| [ ] | [`Voc_Species_Data.hdj`](monica/core/voc_common.odin#L111) | 111 | `f64` | `J mol-1` | medium | comment J mol-1 (deactivation energy of Jmax, default 220000); unused in the port |
| [ ] | [`Voc_Species_Data.sdj`](monica/core/voc_common.odin#L112) | 112 | `f64` | `J mol-1 K-1` | medium | entropy term of Jmax T response (default 703, typical J mol-1 K-1); unused in the port |
| [ ] | [`Voc_Species_Data.kc25`](monica/core/voc_common.odin#L114) | 114 | `f64` | `umol mol-1` | high | comment; crop param kc25 tagged umol mol-1; copied crop_voc.odin:116, otherwise unused |
| [ ] | [`Voc_Species_Data.ko25`](monica/core/voc_common.odin#L115) | 115 | `f64` | `mmol mol-1` | high | comment mmol mol-1 (default 179); unused in the port |
| [ ] | [`Voc_Species_Data.vcmax25`](monica/core/voc_common.odin#L116) | 116 | `f64` | `umol m-2 s-1` | high | comment; crop param vcmax25 tagged umol m-2 s-1; copied crop_voc.odin:112, not read via species |
| [ ] | [`Voc_Species_Data.qjvc`](monica/core/voc_common.odin#L117) | 117 | `f64` | `1` | medium | ratio Jmax/Vcmax (default 2.0); unused in the port |
| [ ] | [`Voc_Species_Data.aekc`](monica/core/voc_common.odin#L119) | 119 | `f64` | `J mol-1` | high | comment; crop param aekc tagged J mol-1; copied but not read by VOC code |
| [ ] | [`Voc_Species_Data.aeko`](monica/core/voc_common.odin#L120) | 120 | `f64` | `J mol-1` | high | comment J mol-1; copied but not read by VOC code |
| [ ] | [`Voc_Species_Data.aejm`](monica/core/voc_common.odin#L121) | 121 | `f64` | `J mol-1` | high | comment J mol-1; unused in the port |
| [ ] | [`Voc_Species_Data.aevc`](monica/core/voc_common.odin#L122) | 122 | `f64` | `J mol-1` | high | comment J mol-1; copied but not read by VOC code |
| [ ] | [`Voc_Species_Data.slamin`](monica/core/voc_common.odin#L123) | 123 | `f64` | `m2 kg-1` | medium | comment m2 kg-1 (default 20); unused in the port |
| [ ] | [`Voc_Species_Data.scale_i`](monica/core/voc_common.odin#L125) | 125 | `f64` | `1` | high | multiplier that cancels in ef_iso computation, voc_jjv.odin:183,188 |
| [ ] | [`Voc_Species_Data.scale_m`](monica/core/voc_common.odin#L126) | 126 | `f64` | `1` | high | multiplier that cancels in ef_mono computation, voc_jjv.odin:184,189 |
| [ ] | [`Voc_Species_Data.m_fol`](monica/core/voc_common.odin#L129) | 129 | `f64` | `kg m-2` | high | leaf_green_biomass kgDM ha-1 /(100*100), crop_voc.odin:104 'kg/ha -> kg/m2' |
| [ ] | [`Voc_Species_Data.lai`](monica/core/voc_common.odin#L132) | 132 | `f64` | `m2 m-2` | high | = input.lai / leaf fraction lai (m2 m-2), crop_voc.odin:103,121 |
| [ ] | [`Voc_Species_Data.sla`](monica/core/voc_common.odin#L135) | 135 | `f64` | `m2 kg-1` | high | lai/m_fol or SLA ha kg-1 *1e4, crop_voc.odin:106 'ha/kg -> m2/kg'; lsw = 1000/sla g m-2 |

## `Voc_Micro_Climate_Data` (monica/core/voc_common.odin)

| ok | field | line | type | proposed unit | confidence | basis |
| --- | --- | --- | --- | --- | --- | --- |
| [ ] | [`Voc_Micro_Climate_Data.rad`](monica/core/voc_common.odin#L168) | 168 | `f64` | `W m-2` | high | globrad_wm2 or Fvcb rad (W m-2 global), crop_voc.odin:77,124 |
| [ ] | [`Voc_Micro_Climate_Data.rad24`](monica/core/voc_common.odin#L169) | 169 | `f64` | `W m-2` | high | mean of rad24 buffer (W m-2), crop_voc.odin:82 |
| [ ] | [`Voc_Micro_Climate_Data.rad240`](monica/core/voc_common.odin#L170) | 170 | `f64` | `W m-2` | high | mean of rad240 buffer (W m-2), crop_voc.odin:87 |
| [ ] | [`Voc_Micro_Climate_Data.t_fol`](monica/core/voc_common.odin#L171) | 171 | `f64` | `°C` | high | = leaf_temp °C, +273.15 to K in voc_jjv/guenther |
| [ ] | [`Voc_Micro_Climate_Data.t_fol24`](monica/core/voc_common.odin#L172) | 172 | `f64` | `°C` | high | mean of tfol24 buffer (°C), crop_voc.odin:93; +273.15 to K |
| [ ] | [`Voc_Micro_Climate_Data.t_fol240`](monica/core/voc_common.odin#L173) | 173 | `f64` | `°C` | high | mean of tfol240 buffer (°C), crop_voc.odin:98; +273.15 to K |
| [ ] | [`Voc_Micro_Climate_Data.sunlitfoliagefraction`](monica/core/voc_common.odin#L176) | 176 | `f64` | `0-1` | medium | comment fraction; never set nor read in the port - unused |
| [ ] | [`Voc_Micro_Climate_Data.sunlitfoliagefraction24`](monica/core/voc_common.odin#L177) | 177 | `f64` | `0-1` | medium | comment fraction; never set, read only in disabled par-temp branch (voc_jjv.odin:149) |
| [ ] | [`Voc_Micro_Climate_Data.co2concentration`](monica/core/voc_common.odin#L179) | 179 | `f64` | `ppm` | high | = atmospheric_co2_concentration `unit:"ppm"`, crop_voc.odin:99; f_co2 = 370/co2 |

## `Voc_Photosynth_T` (monica/core/voc_common.odin)

| ok | field | line | type | proposed unit | confidence | basis |
| --- | --- | --- | --- | --- | --- | --- |
| [ ] | [`Voc_Photosynth_T.par`](monica/core/voc_common.odin#L211) | 211 | `f64` | `umol m-2 s-1` | medium | JJV: W m-2*0.45*4.57; Guenther divides by 4.57 (VOC_W_IN_UMOL, C++ bug), so not umol there |
| [ ] | [`Voc_Photosynth_T.par24`](monica/core/voc_common.odin#L212) | 212 | `f64` | `umol m-2 s-1` | high | rad24*0.45*4.57, voc_jjv.odin:215; only read in disabled par-temp branch |
| [ ] | [`Voc_Photosynth_T.par240`](monica/core/voc_common.odin#L213) | 213 | `f64` | `umol m-2 s-1` | high | rad240*0.45*4.57, voc_jjv.odin:216; only read in disabled par-temp branch |

## `Voc_Foliage_T` (monica/core/voc_common.odin)

| ok | field | line | type | proposed unit | confidence | basis |
| --- | --- | --- | --- | --- | --- | --- |
| [ ] | [`Voc_Foliage_T.temp_k`](monica/core/voc_common.odin#L218) | 218 | `f64` | `K` | high | t_fol + 273.15, voc_jjv.odin:217 |
| [ ] | [`Voc_Foliage_T.temp_k24`](monica/core/voc_common.odin#L219) | 219 | `f64` | `K` | high | t_fol24 + 273.15, voc_jjv.odin:218 |
| [ ] | [`Voc_Foliage_T.temp_k240`](monica/core/voc_common.odin#L220) | 220 | `f64` | `K` | high | t_fol240 + 273.15, voc_jjv.odin:219 |

## `Voc_Enzyme_Activity_T` (monica/core/voc_common.odin)

| ok | field | line | type | proposed unit | confidence | basis |
| --- | --- | --- | --- | --- | --- | --- |
| [ ] | [`Voc_Enzyme_Activity_T.ef_iso`](monica/core/voc_common.odin#L225) | 225 | `f64` | `ug gDW-1 h-1` | high | = species ef_iso (Guenther) or ef*f_co2 (JJV comment ugC gDW-1 h-1), voc_jjv.odin:188 |
| [ ] | [`Voc_Enzyme_Activity_T.ef_mono`](monica/core/voc_common.odin#L226) | 226 | `f64` | `ug gDW-1 h-1` | high | = species ef_mono (Guenther) or ef*f_co2 (JJV comment ugC gDW-1 h-1), voc_jjv.odin:189 |

## `Voc_Leaf_Emission_T` (monica/core/voc_common.odin)

| ok | field | line | type | proposed unit | confidence | basis |
| --- | --- | --- | --- | --- | --- | --- |
| [ ] | [`Voc_Leaf_Emission_T.foliage_layer`](monica/core/voc_common.odin#L231) | 231 | `int` | `` | high | canopy layer index (C++ size_t); never used in the port |

## `Voc_Leaf_Emissions` (monica/core/voc_common.odin)

| ok | field | line | type | proposed unit | confidence | basis |
| --- | --- | --- | --- | --- | --- | --- |
| [ ] | [`Voc_Leaf_Emissions.isoprene`](monica/core/voc_common.odin#L240) | 240 | `f64` | `ug gDW-1 h-1` | medium | ef*factors; converted by lsw/(3600*12)*lai to umol m-2: per gDW (ugC), comment 'ug m-2ground h-1' wrong |
| [ ] | [`Voc_Leaf_Emissions.monoterp`](monica/core/voc_common.odin#L241) | 241 | `f64` | `ug gDW-1 h-1` | medium | ef*factors; converted by lsw/(3600*12)*lai to umol m-2: per gDW (ugC), comment 'ug m-2ground h-1' wrong |

## `Voc_Jjv_Gamma_En_Res` (monica/core/voc_jjv.odin)

| ok | field | line | type | proposed unit | confidence | basis |
| --- | --- | --- | --- | --- | --- | --- |
| [ ] | [`Voc_Jjv_Gamma_En_Res.en_iso`](monica/core/voc_jjv.odin#L56) | 56 | `f64` | `1` | high | exp(ct-ha/RT)/(1+exp(...)): dimensionless activity factor, voc_jjv.odin:71 |
| [ ] | [`Voc_Jjv_Gamma_En_Res.en_mono`](monica/core/voc_jjv.odin#L57) | 57 | `f64` | `1` | high | exp(ct-ha/RT)/(1+exp(...)): dimensionless activity factor, voc_jjv.odin:74 |
| [ ] | [`Voc_Jjv_Gamma_En_Res.ennorm_iso`](monica/core/voc_jjv.odin#L58) | 58 | `f64` | `1` | high | same formula at reference temperature; only ratio en/ennorm used |
| [ ] | [`Voc_Jjv_Gamma_En_Res.ennorm_mono`](monica/core/voc_jjv.odin#L59) | 59 | `f64` | `1` | high | same formula at reference temperature; only ratio en/ennorm used |

## `Monica_Et` (monica/modules/evapotranspiration/monica_et.odin)

| ok | field | line | type | proposed unit | confidence | basis |
| --- | --- | --- | --- | --- | --- | --- |
| [ ] | [`Monica_Et.no_of_layers`](monica/modules/evapotranspiration/monica_et.odin#L27) | 27 | `int` | `layer#` | high | number of soil layers n used to size/copy per-layer outputs, monica_et.odin |

## `Apsim_Campbell` (monica/modules/soil_temperature/apsim_campbell.odin)

| ok | field | line | type | proposed unit | confidence | basis |
| --- | --- | --- | --- | --- | --- | --- |
| [ ] | [`Apsim_Campbell.no_of_layers`](monica/modules/soil_temperature/apsim_campbell.odin#L29) | 29 | `int` | `layer#` | high | number of soil layers n used to size/copy per-layer outputs, apsim_campbell.odin |

## `Bioma_Parton_Swat` (monica/modules/soil_temperature/bioma_parton_swat.odin)

| ok | field | line | type | proposed unit | confidence | basis |
| --- | --- | --- | --- | --- | --- | --- |
| [ ] | [`Bioma_Parton_Swat.no_of_layers`](monica/modules/soil_temperature/bioma_parton_swat.odin#L21) | 21 | `int` | `layer#` | high | number of soil layers n used to size/copy per-layer outputs, bioma_parton_swat.odin |

## `Bioma_Swat_Swat` (monica/modules/soil_temperature/bioma_swat_swat.odin)

| ok | field | line | type | proposed unit | confidence | basis |
| --- | --- | --- | --- | --- | --- | --- |
| [ ] | [`Bioma_Swat_Swat.no_of_layers`](monica/modules/soil_temperature/bioma_swat_swat.odin#L21) | 21 | `int` | `layer#` | high | number of soil layers n used to size/copy per-layer outputs, bioma_swat_swat.odin |

## `Dssat_Epicst` (monica/modules/soil_temperature/dssat_epicst.odin)

| ok | field | line | type | proposed unit | confidence | basis |
| --- | --- | --- | --- | --- | --- | --- |
| [ ] | [`Dssat_Epicst.no_of_layers`](monica/modules/soil_temperature/dssat_epicst.odin#L20) | 20 | `int` | `layer#` | high | number of soil layers n used to size/copy per-layer outputs, dssat_epicst.odin |

## `Dssat_St` (monica/modules/soil_temperature/dssat_st.odin)

| ok | field | line | type | proposed unit | confidence | basis |
| --- | --- | --- | --- | --- | --- | --- |
| [ ] | [`Dssat_St.no_of_layers`](monica/modules/soil_temperature/dssat_st.odin#L21) | 21 | `int` | `layer#` | high | number of soil layers n used to size/copy per-layer outputs, dssat_st.odin |

## `Monica_Soil_Temp` (monica/modules/soil_temperature/monica_soiltemp.odin)

| ok | field | line | type | proposed unit | confidence | basis |
| --- | --- | --- | --- | --- | --- | --- |
| [ ] | [`Monica_Soil_Temp.no_of_layers`](monica/modules/soil_temperature/monica_soiltemp.odin#L25) | 25 | `int` | `layer#` | high | number of soil layers n used to size/copy per-layer outputs, monica_soiltemp.odin |

## `Simplace` (monica/modules/soil_temperature/simplace.odin)

| ok | field | line | type | proposed unit | confidence | basis |
| --- | --- | --- | --- | --- | --- | --- |
| [ ] | [`Simplace.no_of_layers`](monica/modules/soil_temperature/simplace.odin#L27) | 27 | `int` | `layer#` | high | number of soil layers n used to size/copy per-layer outputs, simplace.odin |

## `Stics` (monica/modules/soil_temperature/stics.odin)

| ok | field | line | type | proposed unit | confidence | basis |
| --- | --- | --- | --- | --- | --- | --- |
| [ ] | [`Stics.no_of_layers`](monica/modules/soil_temperature/stics.odin#L20) | 20 | `int` | `layer#` | high | number of soil layers n used to size/copy per-layer outputs, stics.odin |

## `Cultivation_Method` (monica/run/cultivation_method.odin)

| ok | field | line | type | proposed unit | confidence | basis |
| --- | --- | --- | --- | --- | --- | --- |
| [ ] | [`Cultivation_Method.custom_id`](monica/run/cultivation_method.odin#L19) | 19 | `int` | `` | high | JSON 'customId', an identifier (cultivation_method.odin:45) |

## `Sowing_Data` (monica/run/workstep.odin)

| ok | field | line | type | proposed unit | confidence | basis |
| --- | --- | --- | --- | --- | --- | --- |
| [ ] | [`Sowing_Data.plant_density`](monica/run/workstep.odin#L74) | 74 | `int` | `plants m-2` | high | JSON 'PlantDensity', copied to species plant_density tagged plants m-2; C++ to_json 'plants m-2' |
| [ ] | [`Sowing_Data.initial_kcb`](monica/run/workstep.odin#L75) | 75 | `f64` | `1` | high | FAO-56 basal crop coefficient Kcb (JSON 'initialKcb'), dimensionless |

## `Transplant_Data` (monica/run/workstep.odin)

| ok | field | line | type | proposed unit | confidence | basis |
| --- | --- | --- | --- | --- | --- | --- |
| [ ] | [`Transplant_Data.initial_stage`](monica/run/workstep.odin#L95) | 95 | `int` | `` | high | developmental stage number, passed to set_stage (crop_management.odin:45) |
| [ ] | [`Transplant_Data.initial_gdd`](monica/run/workstep.odin#L96) | 96 | `f64` | `°C d` | high | JSON 'initialTemperatureSum', forced into phenology temperature sums tagged °C d |
| [ ] | [`Transplant_Data.init_root_mass`](monica/run/workstep.odin#L97) | 97 | `f64` | `kgDM ha-1` | medium | JSON 'initialRootBiomass', written to organ_biomass[0] (crop_growth.odin:352), MONICA organ biomass kgDM ha-1 |
| [ ] | [`Transplant_Data.init_leaf_mass`](monica/run/workstep.odin#L98) | 98 | `f64` | `kgDM ha-1` | medium | JSON 'initialLeafBiomass', written to organ_biomass[1] (crop_growth.odin:353) |
| [ ] | [`Transplant_Data.init_shoot_mass`](monica/run/workstep.odin#L99) | 99 | `f64` | `kgDM ha-1` | medium | JSON 'initialShootBiomass', written to organ_biomass[2] (crop_growth.odin:354) |
| [ ] | [`Transplant_Data.init_lai`](monica/run/workstep.odin#L100) | 100 | `f64` | `m2 m-2` | high | JSON 'initialLAI', forced as leaf area index via canopy_transplant |
| [ ] | [`Transplant_Data.post_transplant_delay`](monica/run/workstep.odin#L101) | 101 | `int` | `d` | high | JSON 'postTransplantDelay' -> transplant_shock_duration compared with days_since_transplant |

## `Automatic_Sowing_Data` (monica/run/workstep.odin)

| ok | field | line | type | proposed unit | confidence | basis |
| --- | --- | --- | --- | --- | --- | --- |
| [ ] | [`Automatic_Sowing_Data.min_temp_threshold`](monica/run/workstep.odin#L120) | 120 | `f64` | `°C` | high | JSON 'min-temp', C++ to_json unit °C; compared with avg tavg/tmin |
| [ ] | [`Automatic_Sowing_Data.days_in_temp_window`](monica/run/workstep.odin#L121) | 121 | `int` | `d` | high | JSON 'days-in-temp-window', C++ to_json unit 'd' |
| [ ] | [`Automatic_Sowing_Data.min_percent_asw`](monica/run/workstep.odin#L122) | 122 | `f64` | `%` | high | JSON 'min-%-asw', compared with sm/asw*100 (workstep.odin:373) |
| [ ] | [`Automatic_Sowing_Data.max_percent_asw`](monica/run/workstep.odin#L123) | 123 | `f64` | `%` | high | JSON 'max-%-asw', compared with sm/asw*100 (workstep.odin:373) |
| [ ] | [`Automatic_Sowing_Data.max_3day_precip_sum`](monica/run/workstep.odin#L124) | 124 | `f64` | `mm` | high | JSON 'max-3d-precip', C++ to_json unit mm; compared with 3-day precip sum |
| [ ] | [`Automatic_Sowing_Data.max_current_day_precip_sum`](monica/run/workstep.odin#L125) | 125 | `f64` | `mm` | high | JSON 'max-curr-day-precip', C++ to_json unit mm |
| [ ] | [`Automatic_Sowing_Data.temp_sum_above_base_temp`](monica/run/workstep.odin#L126) | 126 | `f64` | `°C d` | high | sum of max(0, T - base_temp) over days (worksteps.odin:483); C++ to_json says °C |
| [ ] | [`Automatic_Sowing_Data.base_temp`](monica/run/workstep.odin#L127) | 127 | `f64` | `°C` | high | JSON 'base-temp', C++ to_json unit °C |
| [ ] | [`Automatic_Sowing_Data.soil_depth_for_averaging`](monica/run/workstep.odin#L129) | 129 | `f64` | `m` | high | JSON avg-soil-temp 'depth', C++ to_json unit m; get_layer_number_for_depth |
| [ ] | [`Automatic_Sowing_Data.days_in_soil_temp_window`](monica/run/workstep.odin#L130) | 130 | `int` | `d` | high | JSON avg-soil-temp 'days', window length in days |
| [ ] | [`Automatic_Sowing_Data.sowing_if_above_avg_soil_temp`](monica/run/workstep.odin#L131) | 131 | `f64` | `°C` | high | JSON avg-soil-temp 'Tavg', C++ to_json unit °C |

## `Harvest_Data` (monica/run/workstep.odin)

| ok | field | line | type | proposed unit | confidence | basis |
| --- | --- | --- | --- | --- | --- | --- |
| [ ] | [`Harvest_Data.incorporate_into_layer_no`](monica/run/workstep.odin#L143) | 143 | `int` | `layer#` | medium | 1-based soil layer number (JSON 'incorporateIntoLayerNo'); 'layer#' as used for layer indices in core |

## `Automatic_Harvest_Data` (monica/run/workstep.odin)

| ok | field | line | type | proposed unit | confidence | basis |
| --- | --- | --- | --- | --- | --- | --- |
| [ ] | [`Automatic_Harvest_Data.min_percent_asw`](monica/run/workstep.odin#L152) | 152 | `f64` | `%` | high | JSON 'min-%-asw', compared with sm/asw*100 (workstep.odin:373) |
| [ ] | [`Automatic_Harvest_Data.max_percent_asw`](monica/run/workstep.odin#L153) | 153 | `f64` | `%` | high | JSON 'max-%-asw', compared with sm/asw*100 (workstep.odin:373) |
| [ ] | [`Automatic_Harvest_Data.max_3day_precip_sum`](monica/run/workstep.odin#L154) | 154 | `f64` | `mm` | high | JSON 'max-3d-precip-sum', C++ to_json unit mm |
| [ ] | [`Automatic_Harvest_Data.max_current_day_precip_sum`](monica/run/workstep.odin#L155) | 155 | `f64` | `mm` | high | JSON 'max-curr-day-precip', C++ to_json unit mm |

## `Cutting_Data` (monica/run/workstep.odin)

| ok | field | line | type | proposed unit | confidence | basis |
| --- | --- | --- | --- | --- | --- | --- |
| [ ] | [`Cutting_Data.cut_max_assimilation_rate_fraction`](monica/run/workstep.odin#L166) | 166 | `f64` | `0-1` | high | JSON 'cut-max-assimilation-rate' in % converted with .PERCENT (/100) on load |

## `Mineral_Fertilization_Data` (monica/run/workstep.odin)

| ok | field | line | type | proposed unit | confidence | basis |
| --- | --- | --- | --- | --- | --- | --- |
| [ ] | [`Mineral_Fertilization_Data.amount`](monica/run/workstep.odin#L172) | 172 | `f64` | `kgN ha-1` | high | JSON 'amount' [x, "kg N"] in examples; daily_sum_fertiliser output kgN ha-1 |
| [ ] | [`Mineral_Fertilization_Data.at_layer`](monica/run/workstep.odin#L177) | 177 | `int` | `layer#` | medium | 1-based soil layer number (JSON 'atLayer'), 0 = top; 'layer#' as for layer indices in core |

## `N_Demand_Fertilization_Data` (monica/run/workstep.odin)

| ok | field | line | type | proposed unit | confidence | basis |
| --- | --- | --- | --- | --- | --- | --- |
| [ ] | [`N_Demand_Fertilization_Data.n_demand`](monica/run/workstep.odin#L185) | 185 | `f64` | `kgN ha-1` | high | JSON 'N-demand' [x, "kg"]; passed as ndemand_kg_ha (soil_column.odin:501) |
| [ ] | [`N_Demand_Fertilization_Data.depth`](monica/run/workstep.odin#L186) | 186 | `f64` | `m` | high | JSON 'depth', C++ to_json [m] 'depth of Nmin measurement'; compared with rooting depth |
| [ ] | [`N_Demand_Fertilization_Data.stage`](monica/run/workstep.odin#L187) | 187 | `int` | `` | high | developmental stage number, compared with current stage (worksteps.odin:974) |

## `Organic_Fertilization_Data` (monica/run/workstep.odin)

| ok | field | line | type | proposed unit | confidence | basis |
| --- | --- | --- | --- | --- | --- | --- |
| [ ] | [`Organic_Fertilization_Data.amount`](monica/run/workstep.odin#L194) | 194 | `f64` | `kgFM ha-1` | high | JSON 'amount' [30000, "kg"]; passed as amount_fm, multiplied by aom_dry_matter_content (kg DM kg FM-1) |
| [ ] | [`Organic_Fertilization_Data.incorporate_into_layer_no`](monica/run/workstep.odin#L196) | 196 | `int` | `layer#` | medium | 1-based soil layer number (JSON 'incorporateIntoLayerNo'); 'layer#' as for layer indices in core |

## `Tillage_Data` (monica/run/workstep.odin)

| ok | field | line | type | proposed unit | confidence | basis |
| --- | --- | --- | --- | --- | --- | --- |
| [ ] | [`Tillage_Data.depth`](monica/run/workstep.odin#L201) | 201 | `f64` | `m` | high | JSON 'depth' [0.15, "m"] in installer examples; C++ default 0.3 |

## `Irrigation_Data` (monica/run/workstep.odin)

| ok | field | line | type | proposed unit | confidence | basis |
| --- | --- | --- | --- | --- | --- | --- |
| [ ] | [`Irrigation_Data.amount`](monica/run/workstep.odin#L206) | 206 | `f64` | `mm` | high | JSON 'amount' [x, "mm"] in examples; C++ IrrigationParameters to_json 'mm' |
| [ ] | [`Irrigation_Data.at_layer`](monica/run/workstep.odin#L212) | 212 | `int` | `layer#` | medium | 1-based soil layer number (JSON 'atLayer'), 0 = surface; 'layer#' as for layer indices in core |

## `Automatic_Irrigation_Data` (monica/run/workstep.odin)

| ok | field | line | type | proposed unit | confidence | basis |
| --- | --- | --- | --- | --- | --- | --- |
| [ ] | [`Automatic_Irrigation_Data.start_stage`](monica/run/workstep.odin#L221) | 221 | `int` | `` | high | developmental stage number (JSON 'startStage'), clamped 0..7 |
| [ ] | [`Automatic_Irrigation_Data.end_stage`](monica/run/workstep.odin#L222) | 222 | `int` | `` | high | developmental stage number (JSON 'endStage'), clamped to <=7 |

## `Workstep` (monica/run/workstep.odin)

| ok | field | line | type | proposed unit | confidence | basis |
| --- | --- | --- | --- | --- | --- | --- |
| [ ] | [`Workstep.apply_no_of_days_after_event`](monica/run/workstep.odin#L277) | 277 | `int` | `d` | high | JSON 'days': number of days after the event (workstep.odin:410,438) |
| [ ] | [`Workstep.days_after_event_count`](monica/run/workstep.odin#L279) | 279 | `int` | `d` | high | daily counter of days since event, compared with apply_no_of_days_after_event |

## `Soil_Parameters` (monica/soil/soil_parameters.odin)

| ok | field | line | type | proposed unit | confidence | basis |
| --- | --- | --- | --- | --- | --- | --- |
| [ ] | [`Soil_Parameters.soil_sand_content`](monica/soil/soil_parameters.odin#L59) | 59 | `f64` | `kg kg-1` | high | JSON 'Sand', % converted to fraction on load; Soil_Layer tag kg kg-1 |
| [ ] | [`Soil_Parameters.soil_clay_content`](monica/soil/soil_parameters.odin#L60) | 60 | `f64` | `kg kg-1` | high | JSON 'Clay', % converted to fraction on load; Soil_Layer tag kg kg-1 |
| [ ] | [`Soil_Parameters.soil_ph`](monica/soil/soil_parameters.odin#L61) | 61 | `f64` | `1` | high | JSON 'pH'; Soil_Layer.soil_ph tagged '1' |
| [ ] | [`Soil_Parameters.soil_stone_content`](monica/soil/soil_parameters.odin#L62) | 62 | `f64` | `m3 m-3` | high | JSON 'Sceleton' vol% converted if '%'; to_json 'vol% [0-1] (m3 m-3)'; Soil_Layer tag |
| [ ] | [`Soil_Parameters.lambda`](monica/soil/soil_parameters.odin#L63) | 63 | `f64` | `1` | medium | JSON 'Lambda' (soil water conductivity coefficient), no documented unit; Soil_Layer.lambda tagged '1' |
| [ ] | [`Soil_Parameters.field_capacity`](monica/soil/soil_parameters.odin#L64) | 64 | `f64` | `m3 m-3` | high | JSON 'FieldCapacity', % converted on load; Soil_Layer tag m3 m-3 |
| [ ] | [`Soil_Parameters.saturation`](monica/soil/soil_parameters.odin#L65) | 65 | `f64` | `m3 m-3` | high | JSON 'PoreVolume', % converted on load; Soil_Layer tag m3 m-3 |
| [ ] | [`Soil_Parameters.permanent_wilting_point`](monica/soil/soil_parameters.odin#L66) | 66 | `f64` | `m3 m-3` | high | JSON 'PermanentWiltingPoint', % converted on load; Soil_Layer tag m3 m-3 |
| [ ] | [`Soil_Parameters.soil_ammonium`](monica/soil/soil_parameters.odin#L68) | 68 | `f64` | `kgN m-3` | high | JSON 'SoilAmmonium', to_json 'kg NH4-N m-3'; Soil_Layer tag kgN m-3 |
| [ ] | [`Soil_Parameters.soil_nitrate`](monica/soil/soil_parameters.odin#L69) | 69 | `f64` | `kgN m-3` | high | JSON 'SoilNitrate', to_json 'kg NO3-N m-3'; Soil_Layer tag kgN m-3 |
| [ ] | [`Soil_Parameters.soil_cn_ratio`](monica/soil/soil_parameters.odin#L70) | 70 | `f64` | `1` | high | JSON 'CN', a C:N ratio (dimensionless), like cn_ratio_aom_* tagged '1' |
| [ ] | [`Soil_Parameters.soil_moisture_percent_fc`](monica/soil/soil_parameters.odin#L71) | 71 | `f64` | `%` | high | JSON 'SoilMoisturePercentFC' '% [0-100]'; used as fc*x/100 (soil_column.odin:151) |
| [ ] | [`Soil_Parameters.thickness`](monica/soil/soil_parameters.odin#L73) | 73 | `f64` | `m` | high | JSON 'Thickness' converted to m via transform_if_not_meters |
| [ ] | [`Soil_Parameters.soil_raw_density`](monica/soil/soil_parameters.odin#L80) | 80 | `f64` | `kg m-3` | high | JSON 'SoilRawDensity', to_json 'kg m-3'; /1000 -> g cm-3 in KA5 lookup |
| [ ] | [`Soil_Parameters.soil_bulk_density`](monica/soil/soil_parameters.odin#L81) | 81 | `f64` | `kg m-3` | high | JSON 'SoilBulkDensity', to_json 'kg m-3'; Soil_Layer tag kg m-3 |
| [ ] | [`Soil_Parameters.soil_organic_carbon`](monica/soil/soil_parameters.odin#L82) | 82 | `f64` | `kg kg-1` | high | JSON 'SoilOrganicCarbon' in %, always /100 (.PERCENT) on load; Soil_Layer tag kg kg-1 |
| [ ] | [`Soil_Parameters.soil_organic_matter`](monica/soil/soil_parameters.odin#L83) | 83 | `f64` | `kg kg-1` | high | JSON 'SoilOrganicMatter', % converted on load; *100 -> % in KA5 lookup |

## `Van_Genuchten_Params` (monica/soil/soil_pwp_fc_sat.odin)

| ok | field | line | type | proposed unit | confidence | basis |
| --- | --- | --- | --- | --- | --- | --- |
| [ ] | [`Van_Genuchten_Params.theta_r`](monica/soil/soil_pwp_fc_sat.odin#L22) | 22 | `f64` | `m3 m-3` | high | residual water content, set from pwp (m3 m-3) or 0.041/0.179 (soil_pwp_fc_sat.odin:443,480) |
| [ ] | [`Van_Genuchten_Params.theta_s`](monica/soil/soil_pwp_fc_sat.odin#L23) | 23 | `f64` | `m3 m-3` | high | saturated water content; becomes Fc_Sat_Pwp.sat (soil_pwp_fc_sat.odin:342) |
| [ ] | [`Van_Genuchten_Params.alpha`](monica/soil/soil_pwp_fc_sat.odin#L24) | 24 | `f64` | `cm-1` | medium | multiplies matric head given in cm (-330, -15000, 10^pF) in VG equation |
| [ ] | [`Van_Genuchten_Params.m`](monica/soil/soil_pwp_fc_sat.odin#L25) | 25 | `f64` | `1` | high | van Genuchten shape exponent (m = 1 - 1/n), dimensionless |
| [ ] | [`Van_Genuchten_Params.n`](monica/soil/soil_pwp_fc_sat.odin#L26) | 26 | `f64` | `1` | high | van Genuchten shape exponent, dimensionless |
| [ ] | [`Van_Genuchten_Params.volumetric_water_content_at_matric_head`](monica/soil/soil_pwp_fc_sat.odin#L27) | 27 | `f64` | `m3 m-3` | high | VG theta(h); assigned to Fc_Sat_Pwp.fc/pwp (soil_pwp_fc_sat.odin:319,343,355) |

## `Fc_Sat_Pwp` (monica/soil/soil_pwp_fc_sat.odin)

| ok | field | line | type | proposed unit | confidence | basis |
| --- | --- | --- | --- | --- | --- | --- |
| [ ] | [`Fc_Sat_Pwp.fc`](monica/soil/soil_pwp_fc_sat.odin#L32) | 32 | `f64` | `m3 m-3` | high | (fc_unmod+fc_mod)/100 commented [m3 m-3] (soil_pwp_fc_sat.odin:259); fills field_capacity |
| [ ] | [`Fc_Sat_Pwp.sat`](monica/soil/soil_pwp_fc_sat.odin#L33) | 33 | `f64` | `m3 m-3` | high | (sat_unmod+sat_mod)/100 commented [m3 m-3] (soil_pwp_fc_sat.odin:260); fills saturation |
| [ ] | [`Fc_Sat_Pwp.pwp`](monica/soil/soil_pwp_fc_sat.odin#L34) | 34 | `f64` | `m3 m-3` | high | (pwp_unmod+pwp_mod)/100 commented [m3 m-3] (soil_pwp_fc_sat.odin:261); fills permanent_wilting_point |

## `Rpscd_Res` (monica/soil/soil_tables.odin)

| ok | field | line | type | proposed unit | confidence | basis |
| --- | --- | --- | --- | --- | --- | --- |
| [ ] | [`Rpscd_Res.sat`](monica/soil/soil_tables.odin#L103) | 103 | `f64` | `%` | high | airCapacity+fieldCapacity from SoilCharacteristic*.json in vol% (e.g. 82), /100 later; comment [m3 m-3] is wrong |
| [ ] | [`Rpscd_Res.fc`](monica/soil/soil_tables.odin#L104) | 104 | `f64` | `%` | high | fieldCapacity from SoilCharacteristic*.json in vol% (e.g. 34), /100 later; comment [m3 m-3] is wrong |
| [ ] | [`Rpscd_Res.pwp`](monica/soil/soil_tables.odin#L105) | 105 | `f64` | `%` | high | fieldCapacity-nFieldCapacity in vol%, /100 later (soil_pwp_fc_sat.odin:261); comment [m3 m-3] is wrong |
