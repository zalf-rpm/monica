# Unit proposals for the parameter structs — applied, as guesses

> **Every unit in this file is a machine-made guess** (Claude, 2026-10-28), not documentation.
> They cover the 203 numeric fields of `odin/monica/params` that have no unit in
> `monica-parameters` (`[value, "unit"]`) or in the C++ `monica-parameters.h` comments; the
> 86 documented ones are tagged without a mark (`CONVENTIONS.md` §9a).

All 203 are in the code since 2026-11-20 (`tools/apply_unit_proposals.py --all --write`), each with
its confidence as a second tag, so a guess stays recognisable and out of the printed unit:

```odin
luxury_n_coeff: f64 `unit:"1" unit_guess:"medium"`,
```

Each proposal comes from the parameter's name, its values in `monica-parameters` (shown as
min..max (count) or the distinct values), C++ comments without a unit, and the units of related,
documented fields. **Confidence** (`unit_guess`): *high* — the quantity is unambiguous (a
temperature, a depth, a day of the year); *medium* — the kind of quantity is clear, the exact unit
less so; *low* — a model parameter whose unit can't be told from name and values.

Units follow §9a: `"0-1"` fractions, `"1"` other dimensionless numbers, `""` no quantity (switches,
stage numbers, days of the year).

**To review** a field: correct `unit` in the code if needed and delete its `unit_guess` tag. What
is left to review:

    grep -rn 'unit_guess:"low"' odin/monica/params       # or "medium", "high", or just unit_guess

The field names link to their declarations (`#L<line>`); after code changes, refresh the links
with `python odin/tools/link_unit_proposals.py`.

The tables below are the record of why each unit was guessed (the `ok` column is no longer used).
The numeric parameter fields added since (e.g. `max_potential_evapotranspiration`,
`infiltration_depth`) came with their unit; as of 2026-11-20 every numeric field of
`odin/monica/params` has a `unit` tag.

Proposals: 203 — 133 high, 46 medium, 24 low confidence (two corrected on 2026-11-20 from the code: `xsa_critical_soil_moisture`, `max_n_uptake_param`).

## `Species_Parameters` (monica/params/crop_parameters.odin)

| ok | field | JSON key | values | proposed unit | confidence | basis |
| --- | --- | --- | --- | --- | --- | --- |
| [ ] | [`Species_Parameters.carboxylation_pathway`](monica/params/crop_parameters.odin#L16) | `CarboxylationPathway` | 0, 1, 2 | `` | high | category: 1 = C3, 2 = C4 |
| [ ] | [`Species_Parameters.default_radiation_use_efficiency`](monica/params/crop_parameters.odin#L17) | `DefaultRadiationUseEfficiency` | 0, 0.5 | `kg CO2 ha-1 h-1 (J m-2 s-1)-1` | low | initial light use efficiency of the photosynthesis-light curve (EFF); unit not stated anywhere |
| [ ] | [`Species_Parameters.part_biological_n_fixation`](monica/params/crop_parameters.odin#L18) | `PartBiologicalNFixation` | 0, 0.55, 0.74 | `0-1` | high | part of N demand covered by fixation |
| [ ] | [`Species_Parameters.initial_kc_factor`](monica/params/crop_parameters.odin#L19) | `InitialKcFactor` | 0, 0.4 | `1` | high | crop coefficient |
| [ ] | [`Species_Parameters.luxury_n_coeff`](monica/params/crop_parameters.odin#L20) | `LuxuryNCoeff` | 0..1.65 (29) | `1` | medium | multiplier on the critical N concentration |
| [ ] | [`Species_Parameters.max_crop_diameter`](monica/params/crop_parameters.odin#L21) | `MaxCropDiameter` | 0..0.1 (29) | `m` | high | a diameter, values 0..0.1 |
| [ ] | [`Species_Parameters.stage_at_max_height`](monica/params/crop_parameters.odin#L22) | `StageAtMaxHeight` | 0..4 (29) | `` | high | a development stage number |
| [ ] | [`Species_Parameters.stage_at_max_diameter`](monica/params/crop_parameters.odin#L23) | `StageAtMaxDiameter` | 0..4 (29) | `` | high | a development stage number |
| [ ] | [`Species_Parameters.minimum_n_concentration`](monica/params/crop_parameters.odin#L24) | `MinimumNConcentration` | 0..1.656 (29) | `kg N kg DM-1` | medium | an N concentration like the other N concentrations; some files have values > 1 |
| [ ] | [`Species_Parameters.minimum_temperature_for_assimilation`](monica/params/crop_parameters.odin#L25) |  |  | `°C` | high | a temperature |
| [ ] | [`Species_Parameters.optimum_temperature_for_assimilation`](monica/params/crop_parameters.odin#L26) |  |  | `°C` | high | a temperature |
| [ ] | [`Species_Parameters.maximum_temperature_for_assimilation`](monica/params/crop_parameters.odin#L27) |  |  | `°C` | high | a temperature |
| [ ] | [`Species_Parameters.n_concentration_aboveground_biomass`](monica/params/crop_parameters.odin#L28) |  |  | `kg N kg DM-1` | high | initial value of the crop module field of the same name |
| [ ] | [`Species_Parameters.n_concentration_b0`](monica/params/crop_parameters.odin#L29) | `NConcentrationB0` | 0..9.492 (29) | `1` | medium | coefficient of the N dilution curve |
| [ ] | [`Species_Parameters.n_concentration_pn`](monica/params/crop_parameters.odin#L30) | `NConcentrationPN` | 0..5 (29) | `1` | medium | coefficient of the N dilution curve |
| [ ] | [`Species_Parameters.n_concentration_root`](monica/params/crop_parameters.odin#L31) | `NConcentrationRoot` | 0, 0.02 | `kg N kg DM-1` | high | initial root N concentration |
| [ ] | [`Species_Parameters.development_acceleration_by_nitrogen_stress`](monica/params/crop_parameters.odin#L32) |  |  | `` | high | a switch, 0/1 |
| [ ] | [`Species_Parameters.field_condition_modifier`](monica/params/crop_parameters.odin#L33) | `FieldConditionModifier` | 1 | `1` | high | a multiplier |
| [ ] | [`Species_Parameters.assimilate_reallocation`](monica/params/crop_parameters.odin#L34) | `AssimilateReallocation` | 0, 0.3, 0.38 | `0-1` | medium | fraction of assimilates reallocated, values 0..0.38 |
| [ ] | [`Species_Parameters.base_temperature`](monica/params/crop_parameters.odin#L35) | `BaseTemperature` | -15..15 (175) | `°C` | high | per stage, values -15..15 |
| [ ] | [`Species_Parameters.organ_maintenance_respiration`](monica/params/crop_parameters.odin#L36) | `OrganMaintenanceRespiration` | 0..0.03 (115) | `kg CH2O kg DM-1 d-1` | medium | per organ maintenance respiration coefficient (old MAIRT) |
| [ ] | [`Species_Parameters.organ_growth_respiration`](monica/params/crop_parameters.odin#L37) | `OrganGrowthRespiration` | 0..0.55 (115) | `kg CH2O kg DM-1` | medium | per organ growth respiration coefficient |
| [ ] | [`Species_Parameters.stage_max_root_n_concentration`](monica/params/crop_parameters.odin#L38) | `StageMaxRootNConcentration` | 0.009..0.1 (174) | `kg N kg DM-1` | medium | per stage, values 0.009..0.1 (old WGMAX) |
| [ ] | [`Species_Parameters.initial_organ_biomass`](monica/params/crop_parameters.odin#L39) | `InitialOrganBiomass` | 0..380 (115) | `kg DM ha-1` | high | per organ, values 0..380 |
| [ ] | [`Species_Parameters.critical_oxygen_content`](monica/params/crop_parameters.odin#L40) | `CriticalOxygenContent` | 0..0.8 (174) | `m3 m-3` | medium | per stage, air-filled pore volume (old LUKRIT) |
| [ ] | [`Species_Parameters.stage_mobil_from_storage_coeff`](monica/params/crop_parameters.odin#L41) | `StageMobilFromStorageCoeff` | 0..0.2884 (24) | `0-1` | medium | per stage, fraction mobilised, values 0..0.29 |
| [ ] | [`Species_Parameters.sampling_depth`](monica/params/crop_parameters.odin#L44) | `SamplingDepth` | 0, 0.6, 0.9 | `m` | high | values 0.6, 0.9 |
| [ ] | [`Species_Parameters.target_n_sampling_depth`](monica/params/crop_parameters.odin#L45) | `TargetNSamplingDepth` | 0..340 (29) | `kg N ha-1` | high | N target for the sampling depth, values 0..340 |
| [ ] | [`Species_Parameters.target_n30`](monica/params/crop_parameters.odin#L46) | `TargetN30` | 0..200 (29) | `kg N ha-1` | high | N target for 0-30 cm, values 0..200 |
| [ ] | [`Species_Parameters.max_n_uptake_param`](monica/params/crop_parameters.odin#L47) | `MaxNUptakeParam` | 0..7.82 (29) | `kg N m-1 d-1` | medium | per metre of root: crop_growth.odin multiplies it by the root length (m m-2) and the time step into kg m-2 (comment `[kg m root-1]`); first guessed `kg N ha-1 d-1`, low |
| [ ] | [`Species_Parameters.root_distribution_param`](monica/params/crop_parameters.odin#L48) | `RootDistributionParam` | 0..0.01 (29) | `1` | low | shape parameter of the root distribution |
| [ ] | [`Species_Parameters.root_growth_lag`](monica/params/crop_parameters.odin#L50) | `RootGrowthLag` | -60, -30, 0 | `°C d` | medium | temperature sum before root growth, values -60..0 |
| [ ] | [`Species_Parameters.minimum_temperature_root_growth`](monica/params/crop_parameters.odin#L51) | `MinimumTemperatureRootGrowth` | 0..10 (29) | `°C` | high | a temperature |
| [ ] | [`Species_Parameters.initial_rooting_depth`](monica/params/crop_parameters.odin#L52) | `InitialRootingDepth` | 0, 0.06, 0.1 | `m` | high | values 0.06, 0.1 |
| [ ] | [`Species_Parameters.root_penetration_rate`](monica/params/crop_parameters.odin#L53) | `RootPenetrationRate` | 0..0.0025 (29) | `m °C-1 d-1` | medium | rooting depth increase per degree day, values 0..0.0025 |
| [ ] | [`Species_Parameters.root_form_factor`](monica/params/crop_parameters.odin#L54) | `RootFormFactor` | 0..5 (29) | `1` | medium | shape parameter |
| [ ] | [`Species_Parameters.specific_root_length`](monica/params/crop_parameters.odin#L55) | `SpecificRootLength` | 0, 300 | `m g-1` | medium | value 300 |
| [ ] | [`Species_Parameters.stage_after_cut`](monica/params/crop_parameters.odin#L56) | `StageAfterCut` | 0, 2, 7 | `` | high | a development stage number (zero-based) |
| [ ] | [`Species_Parameters.limiting_temperature_heat_stress`](monica/params/crop_parameters.odin#L57) | `LimitingTemperatureHeatStress` | 0, 40 | `°C` | high | a temperature, value 40 |
| [ ] | [`Species_Parameters.cutting_delay_days`](monica/params/crop_parameters.odin#L58) | `CuttingDelayDays` | 0, 6.11964, 10 | `d` | high | days |
| [ ] | [`Species_Parameters.drought_impact_on_fertility_factor`](monica/params/crop_parameters.odin#L59) |  |  | `1` | medium | a factor |
| [ ] | [`Species_Parameters.ef_mono`](monica/params/crop_parameters.odin#L60) | `EF_MONO` |  | `ug gDW-1 h-1` | high | C++ Voc::SpeciesData comment for EF_MONO |
| [ ] | [`Species_Parameters.ef_monos`](monica/params/crop_parameters.odin#L61) | `EF_MONOS` |  | `ug gDW-1 h-1` | high | C++ Voc::SpeciesData comment for EF_MONOS |
| [ ] | [`Species_Parameters.ef_iso`](monica/params/crop_parameters.odin#L62) | `EF_ISO` |  | `ug gDW-1 h-1` | high | emission factor like EF_MONO |
| [ ] | [`Species_Parameters.vcmax25`](monica/params/crop_parameters.odin#L63) | `VCMAX25` |  | `umol m-2 s-1` | high | Farquhar Vcmax at 25 °C |
| [ ] | [`Species_Parameters.aekc`](monica/params/crop_parameters.odin#L64) | `AEKC` |  | `J mol-1` | high | activation energy of Kc |
| [ ] | [`Species_Parameters.aeko`](monica/params/crop_parameters.odin#L65) | `AEKO` |  | `J mol-1` | high | activation energy of Ko |
| [ ] | [`Species_Parameters.aevc`](monica/params/crop_parameters.odin#L66) | `AEVC` |  | `J mol-1` | high | activation energy of Vcmax |
| [ ] | [`Species_Parameters.kc25`](monica/params/crop_parameters.odin#L67) | `KC25` |  | `umol mol-1` | high | Michaelis constant for CO2 at 25 °C (260) |
| [ ] | [`Species_Parameters.ko25`](monica/params/crop_parameters.odin#L68) | `KO25` |  | `mmol mol-1` | high | Michaelis constant for O2 at 25 °C (179) |
| [ ] | [`Species_Parameters.dormancy_start_doy`](monica/params/crop_parameters.odin#L70) | `DormancyStartDoy` |  | `` | high | a day of the year (0 = unset) |
| [ ] | [`Species_Parameters.dormancy_end_doy`](monica/params/crop_parameters.odin#L71) | `DormancyEndDoy` |  | `` | high | a day of the year |

## `Cultivar_Parameters` (monica/params/crop_parameters.odin)

| ok | field | JSON key | values | proposed unit | confidence | basis |
| --- | --- | --- | --- | --- | --- | --- |
| [ ] | [`Cultivar_Parameters.max_assimilation_rate`](monica/params/crop_parameters.odin#L275) | `MaxAssimilationRate` | 13..150 (59) | `kg CO2 ha-1 h-1` | high | AMAX, values 13..150; ReferenceMaxAssimilationRate is the same quantity |
| [ ] | [`Cultivar_Parameters.light_extinction_coefficient`](monica/params/crop_parameters.odin#L276) | `LightExtinctionCoefficient` |  | `1` | high | extinction coefficient |
| [ ] | [`Cultivar_Parameters.residue_n_ratio`](monica/params/crop_parameters.odin#L278) | `ResidueNRatio` | 0.3..2 (59) | `1` | medium | ratio of residue to product N concentration |
| [ ] | [`Cultivar_Parameters.lt50_cultivar`](monica/params/crop_parameters.odin#L279) | `LT50cultivar` | -24..0 (59) | `°C` | high | lethal temperature, values -24..0 |
| [ ] | [`Cultivar_Parameters.crop_height_p1`](monica/params/crop_parameters.odin#L280) | `CropHeightP1` | 3.74..12 (59) | `1` | medium | shape parameter of the height curve |
| [ ] | [`Cultivar_Parameters.crop_height_p2`](monica/params/crop_parameters.odin#L281) | `CropHeightP2` | 0..0.6 (59) | `1` | medium | shape parameter of the height curve |
| [ ] | [`Cultivar_Parameters.assimilate_partitioning_coeff`](monica/params/crop_parameters.odin#L283) |  |  | `0-1` | high | per stage and organ, rows sum to 1 |
| [ ] | [`Cultivar_Parameters.organ_senescence_rate`](monica/params/crop_parameters.odin#L284) |  |  | `d-1` | medium | per stage and organ (old DEAD) |
| [ ] | [`Cultivar_Parameters.drought_stress_threshold`](monica/params/crop_parameters.odin#L288) | `DroughtStressThreshold` | 0..1 (373) | `0-1` | high | per stage, values 0..1 |
| [ ] | [`Cultivar_Parameters.vernalisation_requirement`](monica/params/crop_parameters.odin#L292) | `VernalisationRequirement` | 0..50 (373) | `d` | high | vernalisation days, values 0..50 |
| [ ] | [`Cultivar_Parameters.heat_sum_irrigation_start`](monica/params/crop_parameters.odin#L293) | `HeatSumIrrigationStart` | 0..600 (59) | `°C d` | high | a temperature sum, values 0..600 |
| [ ] | [`Cultivar_Parameters.heat_sum_irrigation_end`](monica/params/crop_parameters.odin#L294) | `HeatSumIrrigationEnd` | 600..2300 (59) | `°C d` | high | a temperature sum, values 600..2300 |
| [ ] | [`Cultivar_Parameters.frost_hardening`](monica/params/crop_parameters.odin#L298) | `FrostHardening` | 0, 0.014, 0.018 | `1` | low | parameter of the LT50 model |
| [ ] | [`Cultivar_Parameters.frost_dehardening`](monica/params/crop_parameters.odin#L299) | `FrostDehardening` | 0, 5.05 | `1` | low | parameter of the LT50 model |
| [ ] | [`Cultivar_Parameters.low_temperature_exposure`](monica/params/crop_parameters.odin#L300) | `LowTemperatureExposure` | 0, 0.654 | `1` | low | parameter of the LT50 model |
| [ ] | [`Cultivar_Parameters.respiratory_stress`](monica/params/crop_parameters.odin#L301) | `RespiratoryStress` | 0, 0.54 | `1` | low | parameter of the LT50 model |
| [ ] | [`Cultivar_Parameters.latest_harvest_doy`](monica/params/crop_parameters.odin#L302) | `LatestHarvestDoy` | -1 | `` | high | a day of the year (-1 = unset) |
| [ ] | [`Cultivar_Parameters.early_ref_leaf_exp`](monica/params/crop_parameters.odin#L306) | `EarlyRefLeafExp` |  | `°C` | medium | reference temperature of leaf expansion (12 = wheat) |
| [ ] | [`Cultivar_Parameters.ref_leaf_exp`](monica/params/crop_parameters.odin#L307) | `RefLeafExp` |  | `°C` | medium | reference temperature of leaf expansion (20 = wheat, 22 = maize) |
| [ ] | [`Cultivar_Parameters.min_temp_dev_we`](monica/params/crop_parameters.odin#L308) | `MinTempDev_WE` |  | `°C` | high | Wang-Engel cardinal temperature |
| [ ] | [`Cultivar_Parameters.opt_temp_dev_we`](monica/params/crop_parameters.odin#L309) | `OptTempDev_WE` |  | `°C` | high | Wang-Engel cardinal temperature |
| [ ] | [`Cultivar_Parameters.max_temp_dev_we`](monica/params/crop_parameters.odin#L310) | `MaxTempDev_WE` |  | `°C` | high | Wang-Engel cardinal temperature |

## `Soil_Moisture_Module_Parameters` (monica/params/module_parameters.odin)

| ok | field | JSON key | values | proposed unit | confidence | basis |
| --- | --- | --- | --- | --- | --- | --- |
| [ ] | [`Soil_Moisture_Module_Parameters.saturated_hydraulic_conductivity`](monica/params/module_parameters.odin#L49) |  |  | `mm d-1` | medium | a conductivity |
| [ ] | [`Soil_Moisture_Module_Parameters.surface_roughness`](monica/params/module_parameters.odin#L50) | `SurfaceRoughness` | 0.02 | `m` | medium | value 0.02 |
| [ ] | [`Soil_Moisture_Module_Parameters.groundwater_discharge`](monica/params/module_parameters.odin#L51) | `GroundwaterDischarge` | 3 | `1` | low | value 3; meaning unclear from the name |
| [ ] | [`Soil_Moisture_Module_Parameters.hydraulic_conductivity_redux`](monica/params/module_parameters.odin#L52) | `HydraulicConductivityRedux` | 0.1 | `0-1` | high | a reduction factor, value 0.1 |
| [ ] | [`Soil_Moisture_Module_Parameters.snow_accumulation_threshold_temperature`](monica/params/module_parameters.odin#L53) |  |  | `°C` | high | a temperature |
| [ ] | [`Soil_Moisture_Module_Parameters.kc_factor`](monica/params/module_parameters.odin#L54) | `KcFactor` | 0.75 | `1` | high | bare soil crop coefficient |
| [ ] | [`Soil_Moisture_Module_Parameters.temperature_limit_for_liquid_water`](monica/params/module_parameters.odin#L55) |  |  | `°C` | high | a temperature |
| [ ] | [`Soil_Moisture_Module_Parameters.correction_snow`](monica/params/module_parameters.odin#L56) | `CorrectionSnow` | 1.14 | `1` | high | precipitation correction factor |
| [ ] | [`Soil_Moisture_Module_Parameters.correction_rain`](monica/params/module_parameters.odin#L57) | `CorrectionRain` | 1 | `1` | high | precipitation correction factor |
| [ ] | [`Soil_Moisture_Module_Parameters.snow_max_additional_density`](monica/params/module_parameters.odin#L58) | `SnowMaxAdditionalDensity` | 0.25 | `kg dm-3` | medium | a snow density like Snow_Component.snow_density |
| [ ] | [`Soil_Moisture_Module_Parameters.new_snow_density_min`](monica/params/module_parameters.odin#L59) | `NewSnowDensityMin` | 0.1 | `kg dm-3` | medium | a snow density, value 0.1 |
| [ ] | [`Soil_Moisture_Module_Parameters.snow_retention_capacity_min`](monica/params/module_parameters.odin#L60) | `SnowRetentionCapacityMin` | 0.05 | `0-1` | medium | water retention as a fraction of the snow pack |
| [ ] | [`Soil_Moisture_Module_Parameters.refreeze_parameter1`](monica/params/module_parameters.odin#L61) | `RefreezeParameter1` | 1.5 | `1` | low | refreeze model parameter |
| [ ] | [`Soil_Moisture_Module_Parameters.refreeze_parameter2`](monica/params/module_parameters.odin#L62) | `RefreezeParameter2` | 0.36 | `1` | low | refreeze model parameter |
| [ ] | [`Soil_Moisture_Module_Parameters.refreeze_temperature`](monica/params/module_parameters.odin#L63) | `RefreezeTemperature` | -1.7 | `°C` | high | a temperature (C++ Snow_Component: [°C]) |
| [ ] | [`Soil_Moisture_Module_Parameters.snow_melt_temperature`](monica/params/module_parameters.odin#L64) | `SnowMeltTemperature` | 0.31 | `°C` | high | a temperature (C++ Snow_Component: [°C]) |
| [ ] | [`Soil_Moisture_Module_Parameters.snow_packing`](monica/params/module_parameters.odin#L65) | `SnowPacking` | 0.01 | `1` | low | packing parameter, value 0.01 |
| [ ] | [`Soil_Moisture_Module_Parameters.snow_retention_capacity_max`](monica/params/module_parameters.odin#L66) | `SnowRetentionCapacityMax` | 0.17 | `0-1` | medium | water retention as a fraction of the snow pack |
| [ ] | [`Soil_Moisture_Module_Parameters.evaporation_zeta`](monica/params/module_parameters.odin#L67) | `EvaporationZeta` | 40 | `1` | medium | shape parameter of the evaporation reduction, value 40 |
| [ ] | [`Soil_Moisture_Module_Parameters.xsa_critical_soil_moisture`](monica/params/module_parameters.odin#L68) | `XSACriticalSoilMoisture` | 0.1 | `1` | medium | a factor on field capacity (soil_moisture.odin: critical moisture = FC * xsa / 2.5 * ET0), as the Crop2ML ET module tags it; first guessed `m3 m-3` |
| [ ] | [`Soil_Moisture_Module_Parameters.maximum_evaporation_impact_depth`](monica/params/module_parameters.odin#L69) | `MaximumEvaporationImpactDepth` | 5 | `layer#` | medium | value 5; the ET module input of the same name is [layers] |
| [ ] | [`Soil_Moisture_Module_Parameters.max_percolation_rate`](monica/params/module_parameters.odin#L70) | `MaxPercolationRate` | 10 | `mm d-1` | high | C++ SoilMoisture::pm_MaxPercolationRate [mm d-1] |
| [ ] | [`Soil_Moisture_Module_Parameters.moisture_init_value`](monica/params/module_parameters.odin#L71) | `MoistureInitValue` | 0 | `0-1` | medium | initial moisture as a fraction of field capacity |

## `Soil_Temperature_Module_Parameters` (monica/params/module_parameters.odin)

| ok | field | JSON key | values | proposed unit | confidence | basis |
| --- | --- | --- | --- | --- | --- | --- |
| [ ] | [`Soil_Temperature_Module_Parameters.n_tau`](monica/params/module_parameters.odin#L207) | `NTau` | 0.65 | `1` | medium | value 0.65 |
| [ ] | [`Soil_Temperature_Module_Parameters.initial_surface_temperature`](monica/params/module_parameters.odin#L208) | `InitialSurfaceTemperature` | 10 | `°C` | high | a temperature |
| [ ] | [`Soil_Temperature_Module_Parameters.base_temperature`](monica/params/module_parameters.odin#L209) | `BaseTemperature` | -15..15 (175) | `°C` | high | a temperature |
| [ ] | [`Soil_Temperature_Module_Parameters.soil_albedo`](monica/params/module_parameters.odin#L218) | `SoilAlbedo` | 0.7 | `0-1` | high | an albedo |
| [ ] | [`Soil_Temperature_Module_Parameters.soil_moisture`](monica/params/module_parameters.odin#L219) | `SoilMoisture` | 0.25 | `m3 m-3` | high | a soil moisture, value 0.25 |

## `Soil_Transport_Module_Parameters` (monica/params/module_parameters.odin)

| ok | field | JSON key | values | proposed unit | confidence | basis |
| --- | --- | --- | --- | --- | --- | --- |
| [ ] | [`Soil_Transport_Module_Parameters.dispersion_length`](monica/params/module_parameters.odin#L281) | `DispersionLength` | 0.049 | `m` | medium | value 0.049 |
| [ ] | [`Soil_Transport_Module_Parameters.ad`](monica/params/module_parameters.odin#L282) | `AD` | 0.002 | `1` | medium | tortuosity factor (Crop_Module_Parameters.tortuosity is "old AD") |
| [ ] | [`Soil_Transport_Module_Parameters.diffusion_coefficient_standard`](monica/params/module_parameters.odin#L283) | `DiffusionCoefficientStandard` | 0.000214 | `m2 d-1` | medium | a diffusion coefficient, value 0.000214 |
| [ ] | [`Soil_Transport_Module_Parameters.n_deposition`](monica/params/module_parameters.odin#L284) | `NDeposition` | 0 | `kg N ha-1 y-1` | high | as Site_Parameters.n_deposition |

## `Stics_Parameters` (monica/params/module_parameters.odin)

| ok | field | JSON key | values | proposed unit | confidence | basis |
| --- | --- | --- | --- | --- | --- | --- |
| [ ] | [`Stics_Parameters.code_vnit`](monica/params/module_parameters.odin#L326) | `code_vnit` | 1 | `` | high | an option switch |
| [ ] | [`Stics_Parameters.code_tnit`](monica/params/module_parameters.odin#L327) | `code_tnit` | 2 | `` | high | an option switch |
| [ ] | [`Stics_Parameters.code_rationit`](monica/params/module_parameters.odin#L328) | `code_rationit` | 2 | `` | high | an option switch |
| [ ] | [`Stics_Parameters.code_hourly_wfps_nit`](monica/params/module_parameters.odin#L329) | `code_hourly_wfps_nit` | 2 | `` | high | an option switch |
| [ ] | [`Stics_Parameters.code_pdenit`](monica/params/module_parameters.odin#L330) | `code_pdenit` | 1 | `` | high | an option switch |
| [ ] | [`Stics_Parameters.code_ratiodenit`](monica/params/module_parameters.odin#L331) | `code_ratiodenit` | 2 | `` | high | an option switch |
| [ ] | [`Stics_Parameters.code_hourly_wfps_denit`](monica/params/module_parameters.odin#L332) | `code_hourly_wfps_denit` | 2 | `` | high | an option switch |
| [ ] | [`Stics_Parameters.hminn`](monica/params/module_parameters.odin#L333) | `hminn` | 0.3 | `0-1` | medium | moisture threshold of nitrification (relative) |
| [ ] | [`Stics_Parameters.hoptn`](monica/params/module_parameters.odin#L334) | `hoptn` | 0.9 | `0-1` | medium | moisture optimum of nitrification (relative) |
| [ ] | [`Stics_Parameters.ph_min_nit`](monica/params/module_parameters.odin#L335) | `pHminnit` | 4 | `1` | high | a pH |
| [ ] | [`Stics_Parameters.ph_max_nit`](monica/params/module_parameters.odin#L336) | `pHmaxnit` | 7.2 | `1` | high | a pH |
| [ ] | [`Stics_Parameters.ph_min_den`](monica/params/module_parameters.odin#L338) | `pHminden` | 5.6 | `1` | high | a pH |
| [ ] | [`Stics_Parameters.ph_max_den`](monica/params/module_parameters.odin#L339) | `pHmaxden` | 9.2 | `1` | high | a pH |
| [ ] | [`Stics_Parameters.wfpsc`](monica/params/module_parameters.odin#L340) | `wfpsc` | 0.62 | `0-1` | high | a water-filled pore space |
| [ ] | [`Stics_Parameters.rationit`](monica/params/module_parameters.odin#L354) | `rationit` | 0.0016 | `0-1` | high | N2O fraction of nitrification |
| [ ] | [`Stics_Parameters.ratiodenit`](monica/params/module_parameters.odin#L359) | `ratiodenit` | 0.2 | `0-1` | high | N2O fraction of denitrification |

## `Soil_Organic_Module_Parameters` (monica/params/module_parameters.odin)

| ok | field | JSON key | values | proposed unit | confidence | basis |
| --- | --- | --- | --- | --- | --- | --- |
| [ ] | [`Soil_Organic_Module_Parameters.som_slow_utilization_efficiency`](monica/params/module_parameters.odin#L516) | `SOM_SlowUtilizationEfficiency` | 0.4 | `0-1` | high | C++ comment: []; an efficiency |
| [ ] | [`Soil_Organic_Module_Parameters.som_fast_utilization_efficiency`](monica/params/module_parameters.odin#L517) | `SOM_FastUtilizationEfficiency` | 0.5 | `0-1` | high | C++ comment: []; an efficiency |
| [ ] | [`Soil_Organic_Module_Parameters.aom_slow_utilization_efficiency`](monica/params/module_parameters.odin#L518) | `AOM_SlowUtilizationEfficiency` | 0.4 | `0-1` | high | C++ comment: []; an efficiency |
| [ ] | [`Soil_Organic_Module_Parameters.aom_fast_utilization_efficiency`](monica/params/module_parameters.odin#L519) | `AOM_FastUtilizationEfficiency` | 0.1 | `0-1` | high | C++ comment: []; an efficiency |
| [ ] | [`Soil_Organic_Module_Parameters.aom_fast_max_c_to_n`](monica/params/module_parameters.odin#L520) | `AOM_FastMaxC_to_N` | 1000 | `1` | high | a C:N ratio |
| [ ] | [`Soil_Organic_Module_Parameters.part_som_fast_to_som_slow`](monica/params/module_parameters.odin#L521) | `PartSOM_Fast_to_SOM_Slow` | 0.3 | `kg kg-1` | high | a partition, like the documented part_aom_* parameters |
| [ ] | [`Soil_Organic_Module_Parameters.part_smb_slow_to_som_fast`](monica/params/module_parameters.odin#L522) | `PartSMB_Slow_to_SOM_Fast` | 0.6 | `kg kg-1` | high | a partition, like the documented part_aom_* parameters |
| [ ] | [`Soil_Organic_Module_Parameters.part_smb_fast_to_som_fast`](monica/params/module_parameters.odin#L523) | `PartSMB_Fast_to_SOM_Fast` | 0.6 | `kg kg-1` | high | a partition, like the documented part_aom_* parameters |
| [ ] | [`Soil_Organic_Module_Parameters.part_som_to_smb_slow`](monica/params/module_parameters.odin#L524) | `PartSOM_to_SMB_Slow` | 0.015 | `kg kg-1` | high | a partition, like the documented part_aom_* parameters |
| [ ] | [`Soil_Organic_Module_Parameters.part_som_to_smb_fast`](monica/params/module_parameters.odin#L525) | `PartSOM_to_SMB_Fast` | 0.0002 | `kg kg-1` | high | a partition, like the documented part_aom_* parameters |
| [ ] | [`Soil_Organic_Module_Parameters.cn_ratio_smb`](monica/params/module_parameters.odin#L526) | `CN_Ratio_SMB` | 6.7 | `1` | high | a C:N ratio; C++ comment: [] |
| [ ] | [`Soil_Organic_Module_Parameters.q_ten_factor`](monica/params/module_parameters.odin#L528) | `QTenFactor` | 2.95 | `1` | high | Q10; C++ comment: [] |
| [ ] | [`Soil_Organic_Module_Parameters.denit1`](monica/params/module_parameters.odin#L537) | `Denit1` | 0.2 | `1` | low | denitrification parameter |
| [ ] | [`Soil_Organic_Module_Parameters.denit2`](monica/params/module_parameters.odin#L538) | `Denit2` | 0.8 | `1` | low | denitrification parameter |
| [ ] | [`Soil_Organic_Module_Parameters.denit3`](monica/params/module_parameters.odin#L539) | `Denit3` | 0.9 | `1` | low | denitrification parameter |
| [ ] | [`Soil_Organic_Module_Parameters.hydrolysis_km`](monica/params/module_parameters.odin#L540) | `HydrolysisKM` | 0.00334 | `mol L-1` | low | Michaelis constant of urea hydrolysis (Tabatabai 1973) |
| [ ] | [`Soil_Organic_Module_Parameters.activation_energy`](monica/params/module_parameters.odin#L541) | `ActivationEnergy` | 41000 | `J mol-1` | high | an activation energy, 41000 (Gould et al. 1973) |
| [ ] | [`Soil_Organic_Module_Parameters.hydrolysis_p1`](monica/params/module_parameters.odin#L542) | `HydrolysisP1` | 0 | `1` | low | hydrolysis parameter (Sadeghi et al. 1988) |
| [ ] | [`Soil_Organic_Module_Parameters.hydrolysis_p2`](monica/params/module_parameters.odin#L543) | `HydrolysisP2` | 0 | `1` | low | hydrolysis parameter (Sadeghi et al. 1988) |
| [ ] | [`Soil_Organic_Module_Parameters.max_mineralisation_depth`](monica/params/module_parameters.odin#L547) | `MaxMineralisationDepth` | 0.4 | `m` | high | value 0.4 |

## `Yield_Component` (monica/params/organic_parameters.odin)

| ok | field | JSON key | values | proposed unit | confidence | basis |
| --- | --- | --- | --- | --- | --- | --- |
| [ ] | [`Yield_Component.organ_id`](monica/params/organic_parameters.odin#L15) | `organId` | 2..6 (124) | `` | high | an organ number |
| [ ] | [`Yield_Component.yield_percentage`](monica/params/organic_parameters.odin#L16) | `yieldPercentage` | 0.01..1 (124) | `0-1` | high | values 0.01..1 (a fraction despite the name) |
| [ ] | [`Yield_Component.yield_dry_matter`](monica/params/organic_parameters.odin#L17) | `yieldDryMatter` | 0.06..0.91 (124) | `kg DM kg FM-1` | high | dry matter content, values 0.06..0.91 |

## `Automatic_Harvest_Parameters` (monica/params/organic_parameters.odin)

| ok | field | JSON key | values | proposed unit | confidence | basis |
| --- | --- | --- | --- | --- | --- | --- |
| [ ] | [`Automatic_Harvest_Parameters.latest_harvest_doy`](monica/params/organic_parameters.odin#L62) | `latestHarvestDOY` |  | `` | high | a day of the year |

## `NMin_Crop_Parameters` (monica/params/organic_parameters.odin)

| ok | field | JSON key | values | proposed unit | confidence | basis |
| --- | --- | --- | --- | --- | --- | --- |
| [ ] | [`NMin_Crop_Parameters.sampling_depth`](monica/params/organic_parameters.odin#L113) | `samplingDepth` |  | `m` | high | as Species_Parameters.sampling_depth |
| [ ] | [`NMin_Crop_Parameters.n_target`](monica/params/organic_parameters.odin#L114) | `nTarget` |  | `kg N ha-1` | high | as Species_Parameters.target_n_sampling_depth |
| [ ] | [`NMin_Crop_Parameters.n_target30`](monica/params/organic_parameters.odin#L115) | `nTarget30` |  | `kg N ha-1` | high | as Species_Parameters.target_n30 |

## `Organic_Matter_Parameters` (monica/params/organic_parameters.odin)

| ok | field | JSON key | values | proposed unit | confidence | basis |
| --- | --- | --- | --- | --- | --- | --- |
| [ ] | [`Organic_Matter_Parameters.cn_ratio_aom_slow`](monica/params/organic_parameters.odin#L155) | `CN_Ratio_AOM_Slow` | 13.1..200 (99) | `1` | high | a C:N ratio; C++ comment: [] |
| [ ] | [`Organic_Matter_Parameters.cn_ratio_aom_fast`](monica/params/organic_parameters.odin#L156) | `CN_Ratio_AOM_Fast` | 0..16 (99) | `1` | high | a C:N ratio; C++ comment: [] |

## `NMin_Application_Parameters` (monica/params/site_sim_parameters.odin)

| ok | field | JSON key | values | proposed unit | confidence | basis |
| --- | --- | --- | --- | --- | --- | --- |
| [ ] | [`NMin_Application_Parameters.min`](monica/params/site_sim_parameters.odin#L69) | `min` |  | `kg N ha-1` | high | minimum fertiliser application (sim.json NMinUserParams) |
| [ ] | [`NMin_Application_Parameters.max`](monica/params/site_sim_parameters.odin#L70) | `max` |  | `kg N ha-1` | high | maximum fertiliser application |
| [ ] | [`NMin_Application_Parameters.delay_in_days`](monica/params/site_sim_parameters.odin#L71) | `delayInDays` |  | `d` | high | days |

## `Irrigation_Parameters` (monica/params/site_sim_parameters.odin)

| ok | field | JSON key | values | proposed unit | confidence | basis |
| --- | --- | --- | --- | --- | --- | --- |
| [ ] | [`Irrigation_Parameters.fw`](monica/params/site_sim_parameters.odin#L111) |  |  | `0-1` | high | C++ comment: fraction of wetted soil surface [0-1] |

## `Automatic_Irrigation_Parameters` (monica/params/site_sim_parameters.odin)

| ok | field | JSON key | values | proposed unit | confidence | basis |
| --- | --- | --- | --- | --- | --- | --- |
| [ ] | [`Automatic_Irrigation_Parameters.amount`](monica/params/site_sim_parameters.odin#L157) | `amount` |  | `mm` | high | sim.json: [17, "mm"] |
| [ ] | [`Automatic_Irrigation_Parameters.percent_nfc`](monica/params/site_sim_parameters.odin#L158) | `set_to_%nFC` |  | `%` | high | sim.json: "_set_to_%nFC": [100, "%"] |
| [ ] | [`Automatic_Irrigation_Parameters.threshold`](monica/params/site_sim_parameters.odin#L159) | `trigger_if_nFC_below_%` |  | `%` | high | sim.json: "trigger_if_nFC_below_%": [90, "%"] |
| [ ] | [`Automatic_Irrigation_Parameters.critical_moisture_depth_m`](monica/params/site_sim_parameters.odin#L160) |  |  | `m` | high | sim.json: "calc_nFC_until_depth_m": [0.3, "m"] |
| [ ] | [`Automatic_Irrigation_Parameters.min_days_between_irrigation_events`](monica/params/site_sim_parameters.odin#L161) | `minDaysBetweenIrrigationEvents` |  | `d` | high | days |

## `Measured_Groundwater_Table_Information` (monica/params/site_sim_parameters.odin)

| ok | field | JSON key | values | proposed unit | confidence | basis |
| --- | --- | --- | --- | --- | --- | --- |
| [ ] | [`Measured_Groundwater_Table_Information.groundwater_info`](monica/params/site_sim_parameters.odin#L248) |  |  | `m` | medium | ISO date -> groundwater depth |

## `Site_Parameters` (monica/params/site_sim_parameters.odin)

| ok | field | JSON key | values | proposed unit | confidence | basis |
| --- | --- | --- | --- | --- | --- | --- |
| [ ] | [`Site_Parameters.latitude`](monica/params/site_sim_parameters.odin#L328) | `Latitude` |  | `deg` | high | a latitude |
| [ ] | [`Site_Parameters.soil_cn_ratio`](monica/params/site_sim_parameters.odin#L332) | `Soil_CN_Ratio` |  | `1` | high | a C:N ratio |
| [ ] | [`Site_Parameters.drainage_coeff`](monica/params/site_sim_parameters.odin#L333) | `DrainageCoeff` |  | `1` | low | drainage coefficient |
| [ ] | [`Site_Parameters.soil_specific_humus_balance_correction`](monica/params/site_sim_parameters.odin#L337) |  |  | `Heq ha-1` | medium | C++ comment: humus equivalents |
| [ ] | [`Site_Parameters.bare_soil_kc_factor`](monica/params/site_sim_parameters.odin#L338) | `Bare_soil_KC_factor` |  | `1` | high | a crop coefficient |
| [ ] | [`Site_Parameters.number_of_layers`](monica/params/site_sim_parameters.odin#L339) | `NumberOfLayers` |  | `1` | high | a count |
| [ ] | [`Site_Parameters.layer_thickness`](monica/params/site_sim_parameters.odin#L340) | `LayerThickness` |  | `m` | high | as Soil_Layer.layer_thickness_m |

## `Simulation_Parameters` (monica/params/site_sim_parameters.odin)

| ok | field | JSON key | values | proposed unit | confidence | basis |
| --- | --- | --- | --- | --- | --- | --- |
| [ ] | [`Simulation_Parameters.number_of_layers`](monica/params/site_sim_parameters.odin#L495) | `NumberOfLayers` |  | `1` | high | a count |
| [ ] | [`Simulation_Parameters.layer_thickness`](monica/params/site_sim_parameters.odin#L496) | `LayerThickness` |  | `m` | high | as Soil_Layer.layer_thickness_m |
| [ ] | [`Simulation_Parameters.start_pv_index`](monica/params/site_sim_parameters.odin#L497) | `StartPVIndex` |  | `` | high | an index |
| [ ] | [`Simulation_Parameters.julian_day_automatic_fertilising`](monica/params/site_sim_parameters.odin#L498) | `JulianDayAutomaticFertilising` |  | `` | high | a day of the year |
| [ ] | [`Simulation_Parameters.no_of_previous_days_serialized_climate_data`](monica/params/site_sim_parameters.odin#L505) |  |  | `d` | high | days |

## `Crop_Module_Parameters` (monica/params/site_sim_parameters.odin)

| ok | field | JSON key | values | proposed unit | confidence | basis |
| --- | --- | --- | --- | --- | --- | --- |
| [ ] | [`Crop_Module_Parameters.canopy_reflection_coefficient`](monica/params/site_sim_parameters.odin#L665) | `CanopyReflectionCoefficient` | 0.08 | `0-1` | high | a reflection coefficient, value 0.08 |
| [ ] | [`Crop_Module_Parameters.reference_max_assimilation_rate`](monica/params/site_sim_parameters.odin#L666) | `ReferenceMaxAssimilationRate` | 30 | `kg CO2 ha-1 h-1` | high | reference AMAX, value 30 |
| [ ] | [`Crop_Module_Parameters.reference_leaf_area_index`](monica/params/site_sim_parameters.odin#L667) | `ReferenceLeafAreaIndex` | 1.44 | `m2 m-2` | high | an LAI |
| [ ] | [`Crop_Module_Parameters.maintenance_respiration_parameter1`](monica/params/site_sim_parameters.odin#L668) |  |  | `1` | low | respiration model parameter |
| [ ] | [`Crop_Module_Parameters.maintenance_respiration_parameter2`](monica/params/site_sim_parameters.odin#L669) |  |  | `1` | low | respiration model parameter |
| [ ] | [`Crop_Module_Parameters.minimum_n_concentration_root`](monica/params/site_sim_parameters.odin#L670) | `MinimumNConcentrationRoot` | 0.005 | `kg N kg DM-1` | high | an N concentration, value 0.005 |
| [ ] | [`Crop_Module_Parameters.reference_albedo`](monica/params/site_sim_parameters.odin#L672) | `ReferenceAlbedo` | 0.23 | `0-1` | high | an albedo |
| [ ] | [`Crop_Module_Parameters.stomata_conductance_alpha`](monica/params/site_sim_parameters.odin#L673) | `StomataConductanceAlpha` | 40 | `1` | medium | stomata model parameter, value 40 |
| [ ] | [`Crop_Module_Parameters.saturation_beta`](monica/params/site_sim_parameters.odin#L674) | `SaturationBeta` | 2.5 | `1` | medium | stomata model parameter, value 2.5 |
| [ ] | [`Crop_Module_Parameters.growth_respiration_redux`](monica/params/site_sim_parameters.odin#L675) | `GrowthRespirationRedux` | 0.7 | `0-1` | medium | a reduction factor, value 0.7 |
| [ ] | [`Crop_Module_Parameters.max_crop_n_demand`](monica/params/site_sim_parameters.odin#L676) | `MaxCropNDemand` | 6 | `kg N ha-1 d-1` | medium | maximum daily N demand, value 6 |
| [ ] | [`Crop_Module_Parameters.growth_respiration_parameter1`](monica/params/site_sim_parameters.odin#L677) | `GrowthRespirationParameter1` | 0.1 | `1` | low | respiration model parameter, value 0.1 |
| [ ] | [`Crop_Module_Parameters.growth_respiration_parameter2`](monica/params/site_sim_parameters.odin#L678) | `GrowthRespirationParameter2` | 38 | `°C` | medium | value 38, used like a temperature |
| [ ] | [`Crop_Module_Parameters.tortuosity`](monica/params/site_sim_parameters.odin#L679) | `Tortuosity` | 0.002 | `1` | medium | a tortuosity factor (old AD) |
| [ ] | [`Crop_Module_Parameters.time_under_anoxia_threshold`](monica/params/site_sim_parameters.odin#L681) |  |  | `d` | high | per stage, compared with Crop_Module.time_under_anoxia (days) |
| [ ] | [`Crop_Module_Parameters.intercropping_k_s`](monica/params/site_sim_parameters.odin#L692) | `k_s` |  | `1` | low | intercropping parameter |
| [ ] | [`Crop_Module_Parameters.intercropping_k_t`](monica/params/site_sim_parameters.odin#L693) | `k_t` |  | `1` | low | intercropping parameter |
| [ ] | [`Crop_Module_Parameters.intercropping_ph_redux`](monica/params/site_sim_parameters.odin#L694) | `PHredux` |  | `0-1` | low | intercropping reduction factor |
| [ ] | [`Crop_Module_Parameters.intercropping_dvs_phr`](monica/params/site_sim_parameters.odin#L695) | `DVS_PHr` |  | `` | low | a development stage |

## `Environment_Parameters` (monica/params/site_sim_parameters.odin)

| ok | field | JSON key | values | proposed unit | confidence | basis |
| --- | --- | --- | --- | --- | --- | --- |
| [ ] | [`Environment_Parameters.albedo`](monica/params/site_sim_parameters.odin#L873) | `Albedo` | 0.23 | `0-1` | high | an albedo |
| [ ] | [`Environment_Parameters.atmospheric_co2`](monica/params/site_sim_parameters.odin#L875) | `AtmosphericCO2` | 0 | `ppm` | high | as Monica_Model.atmospheric_co2_concentration |
| [ ] | [`Environment_Parameters.atmospheric_co2s`](monica/params/site_sim_parameters.odin#L876) |  |  | `ppm` | high | year -> CO2 |
| [ ] | [`Environment_Parameters.atmospheric_o3`](monica/params/site_sim_parameters.odin#L877) | `AtmosphericO3` |  | `ppb` | high | as Monica_Model.atmospheric_o3_concentration |
| [ ] | [`Environment_Parameters.atmospheric_o3s`](monica/params/site_sim_parameters.odin#L878) |  |  | `ppb` | high | year -> O3 |
| [ ] | [`Environment_Parameters.wind_speed_height_m`](monica/params/site_sim_parameters.odin#L879) | `WindSpeedHeight` | 2 | `m` | high | a height |
| [ ] | [`Environment_Parameters.leaching_depth_m`](monica/params/site_sim_parameters.odin#L880) | `LeachingDepth` | 1.6 | `m` | high | a depth |
| [ ] | [`Environment_Parameters.time_step`](monica/params/site_sim_parameters.odin#L881) | `timeStep` | 1 | `d` | high | the model time step |
| [ ] | [`Environment_Parameters.max_groundwater_depth_m`](monica/params/site_sim_parameters.odin#L882) | `MaxGroundwaterDepth` | 18 | `m` | high | a depth |
| [ ] | [`Environment_Parameters.min_groundwater_depth_m`](monica/params/site_sim_parameters.odin#L883) | `MinGroundwaterDepth` | 20 | `m` | high | a depth |
| [ ] | [`Environment_Parameters.min_groundwater_depth_month`](monica/params/site_sim_parameters.odin#L884) | `MinGroundwaterDepthMonth` | 3 | `` | high | a month number |
