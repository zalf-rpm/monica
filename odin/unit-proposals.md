# Unit proposals for the parameter structs — AUTO-GENERATED, NOT REVIEWED

> **Every unit in this file is a machine-made guess** (Claude, 2026-10-28), not documentation.
> None of them is in the code. They cover the 203 numeric fields of `odin/monica/params` that have no
> unit in `monica-parameters` (`[value, "unit"]`) or in the C++ `monica-parameters.h` comments; the
> 84 documented ones are already tagged (`CONVENTIONS.md` §9a).

Each proposal comes from the parameter's name, its values in `monica-parameters` (shown as
min..max (count) or the distinct values), C++ comments without a unit, and the units of related,
documented fields. **Confidence**: *high* — the quantity is unambiguous (a temperature, a depth, a
day of the year); *medium* — the kind of quantity is clear, the exact unit less so; *low* — a
model parameter whose unit can't be told from name and values.

Units follow §9a: `"0-1"` fractions, `"1"` other dimensionless numbers, `""` no quantity (switches,
stage numbers, days of the year).

**To review:** tick `[x]` for a proposal to accept, correct the unit in place if needed (then tick
it too), leave `[ ]` to skip. Then

    python odin/tools/apply_unit_proposals.py            # list what would be tagged
    python odin/tools/apply_unit_proposals.py --write    # write the ticked rows as `unit` tags

and delete the applied rows (or the file). Already tagged fields are skipped.

Proposals: 203 — 133 high, 45 medium, 25 low confidence.


## `Species_Parameters` (monica/params/crop_parameters.odin)

| ok | field | JSON key | values | proposed unit | confidence | basis |
| --- | --- | --- | --- | --- | --- | --- |
| [ ] | `Species_Parameters.carboxylation_pathway` | `CarboxylationPathway` | 0, 1, 2 | `` | high | category: 1 = C3, 2 = C4 |
| [ ] | `Species_Parameters.default_radiation_use_efficiency` | `DefaultRadiationUseEfficiency` | 0, 0.5 | `kg CO2 ha-1 h-1 (J m-2 s-1)-1` | low | initial light use efficiency of the photosynthesis-light curve (EFF); unit not stated anywhere |
| [ ] | `Species_Parameters.part_biological_n_fixation` | `PartBiologicalNFixation` | 0, 0.55, 0.74 | `0-1` | high | part of N demand covered by fixation |
| [ ] | `Species_Parameters.initial_kc_factor` | `InitialKcFactor` | 0, 0.4 | `1` | high | crop coefficient |
| [ ] | `Species_Parameters.luxury_n_coeff` | `LuxuryNCoeff` | 0..1.65 (29) | `1` | medium | multiplier on the critical N concentration |
| [ ] | `Species_Parameters.max_crop_diameter` | `MaxCropDiameter` | 0..0.1 (29) | `m` | high | a diameter, values 0..0.1 |
| [ ] | `Species_Parameters.stage_at_max_height` | `StageAtMaxHeight` | 0..4 (29) | `` | high | a development stage number |
| [ ] | `Species_Parameters.stage_at_max_diameter` | `StageAtMaxDiameter` | 0..4 (29) | `` | high | a development stage number |
| [ ] | `Species_Parameters.minimum_n_concentration` | `MinimumNConcentration` | 0..1.656 (29) | `kg N kg DM-1` | medium | an N concentration like the other N concentrations; some files have values > 1 |
| [ ] | `Species_Parameters.minimum_temperature_for_assimilation` |  |  | `°C` | high | a temperature |
| [ ] | `Species_Parameters.optimum_temperature_for_assimilation` |  |  | `°C` | high | a temperature |
| [ ] | `Species_Parameters.maximum_temperature_for_assimilation` |  |  | `°C` | high | a temperature |
| [ ] | `Species_Parameters.n_concentration_aboveground_biomass` |  |  | `kg N kg DM-1` | high | initial value of the crop module field of the same name |
| [ ] | `Species_Parameters.n_concentration_b0` | `NConcentrationB0` | 0..9.492 (29) | `1` | medium | coefficient of the N dilution curve |
| [ ] | `Species_Parameters.n_concentration_pn` | `NConcentrationPN` | 0..5 (29) | `1` | medium | coefficient of the N dilution curve |
| [ ] | `Species_Parameters.n_concentration_root` | `NConcentrationRoot` | 0, 0.02 | `kg N kg DM-1` | high | initial root N concentration |
| [ ] | `Species_Parameters.development_acceleration_by_nitrogen_stress` |  |  | `` | high | a switch, 0/1 |
| [ ] | `Species_Parameters.field_condition_modifier` | `FieldConditionModifier` | 1 | `1` | high | a multiplier |
| [ ] | `Species_Parameters.assimilate_reallocation` | `AssimilateReallocation` | 0, 0.3, 0.38 | `0-1` | medium | fraction of assimilates reallocated, values 0..0.38 |
| [ ] | `Species_Parameters.base_temperature` | `BaseTemperature` | -15..15 (175) | `°C` | high | per stage, values -15..15 |
| [ ] | `Species_Parameters.organ_maintenance_respiration` | `OrganMaintenanceRespiration` | 0..0.03 (115) | `kg CH2O kg DM-1 d-1` | medium | per organ maintenance respiration coefficient (old MAIRT) |
| [ ] | `Species_Parameters.organ_growth_respiration` | `OrganGrowthRespiration` | 0..0.55 (115) | `kg CH2O kg DM-1` | medium | per organ growth respiration coefficient |
| [ ] | `Species_Parameters.stage_max_root_n_concentration` | `StageMaxRootNConcentration` | 0.009..0.1 (174) | `kg N kg DM-1` | medium | per stage, values 0.009..0.1 (old WGMAX) |
| [ ] | `Species_Parameters.initial_organ_biomass` | `InitialOrganBiomass` | 0..380 (115) | `kg DM ha-1` | high | per organ, values 0..380 |
| [ ] | `Species_Parameters.critical_oxygen_content` | `CriticalOxygenContent` | 0..0.8 (174) | `m3 m-3` | medium | per stage, air-filled pore volume (old LUKRIT) |
| [ ] | `Species_Parameters.stage_mobil_from_storage_coeff` | `StageMobilFromStorageCoeff` | 0..0.2884 (24) | `0-1` | medium | per stage, fraction mobilised, values 0..0.29 |
| [ ] | `Species_Parameters.sampling_depth` | `SamplingDepth` | 0, 0.6, 0.9 | `m` | high | values 0.6, 0.9 |
| [ ] | `Species_Parameters.target_n_sampling_depth` | `TargetNSamplingDepth` | 0..340 (29) | `kg N ha-1` | high | N target for the sampling depth, values 0..340 |
| [ ] | `Species_Parameters.target_n30` | `TargetN30` | 0..200 (29) | `kg N ha-1` | high | N target for 0-30 cm, values 0..200 |
| [ ] | `Species_Parameters.max_n_uptake_param` | `MaxNUptakeParam` | 0..7.82 (29) | `kg N ha-1 d-1` | low | parameter of the maximum daily N uptake |
| [ ] | `Species_Parameters.root_distribution_param` | `RootDistributionParam` | 0..0.01 (29) | `1` | low | shape parameter of the root distribution |
| [ ] | `Species_Parameters.root_growth_lag` | `RootGrowthLag` | -60, -30, 0 | `°C d` | medium | temperature sum before root growth, values -60..0 |
| [ ] | `Species_Parameters.minimum_temperature_root_growth` | `MinimumTemperatureRootGrowth` | 0..10 (29) | `°C` | high | a temperature |
| [ ] | `Species_Parameters.initial_rooting_depth` | `InitialRootingDepth` | 0, 0.06, 0.1 | `m` | high | values 0.06, 0.1 |
| [ ] | `Species_Parameters.root_penetration_rate` | `RootPenetrationRate` | 0..0.0025 (29) | `m °C-1 d-1` | medium | rooting depth increase per degree day, values 0..0.0025 |
| [ ] | `Species_Parameters.root_form_factor` | `RootFormFactor` | 0..5 (29) | `1` | medium | shape parameter |
| [ ] | `Species_Parameters.specific_root_length` | `SpecificRootLength` | 0, 300 | `m g-1` | medium | value 300 |
| [ ] | `Species_Parameters.stage_after_cut` | `StageAfterCut` | 0, 2, 7 | `` | high | a development stage number (zero-based) |
| [ ] | `Species_Parameters.limiting_temperature_heat_stress` | `LimitingTemperatureHeatStress` | 0, 40 | `°C` | high | a temperature, value 40 |
| [ ] | `Species_Parameters.cutting_delay_days` | `CuttingDelayDays` | 0, 6.11964, 10 | `d` | high | days |
| [ ] | `Species_Parameters.drought_impact_on_fertility_factor` |  |  | `1` | medium | a factor |
| [ ] | `Species_Parameters.ef_mono` | `EF_MONO` |  | `ug gDW-1 h-1` | high | C++ Voc::SpeciesData comment for EF_MONO |
| [ ] | `Species_Parameters.ef_monos` | `EF_MONOS` |  | `ug gDW-1 h-1` | high | C++ Voc::SpeciesData comment for EF_MONOS |
| [ ] | `Species_Parameters.ef_iso` | `EF_ISO` |  | `ug gDW-1 h-1` | high | emission factor like EF_MONO |
| [ ] | `Species_Parameters.vcmax25` | `VCMAX25` |  | `umol m-2 s-1` | high | Farquhar Vcmax at 25 °C |
| [ ] | `Species_Parameters.aekc` | `AEKC` |  | `J mol-1` | high | activation energy of Kc |
| [ ] | `Species_Parameters.aeko` | `AEKO` |  | `J mol-1` | high | activation energy of Ko |
| [ ] | `Species_Parameters.aevc` | `AEVC` |  | `J mol-1` | high | activation energy of Vcmax |
| [ ] | `Species_Parameters.kc25` | `KC25` |  | `umol mol-1` | high | Michaelis constant for CO2 at 25 °C (260) |
| [ ] | `Species_Parameters.ko25` | `KO25` |  | `mmol mol-1` | high | Michaelis constant for O2 at 25 °C (179) |
| [ ] | `Species_Parameters.dormancy_start_doy` | `DormancyStartDoy` |  | `` | high | a day of the year (0 = unset) |
| [ ] | `Species_Parameters.dormancy_end_doy` | `DormancyEndDoy` |  | `` | high | a day of the year |

## `Cultivar_Parameters` (monica/params/crop_parameters.odin)

| ok | field | JSON key | values | proposed unit | confidence | basis |
| --- | --- | --- | --- | --- | --- | --- |
| [ ] | `Cultivar_Parameters.max_assimilation_rate` | `MaxAssimilationRate` | 13..150 (59) | `kg CO2 ha-1 h-1` | high | AMAX, values 13..150; ReferenceMaxAssimilationRate is the same quantity |
| [ ] | `Cultivar_Parameters.light_extinction_coefficient` | `LightExtinctionCoefficient` |  | `1` | high | extinction coefficient |
| [ ] | `Cultivar_Parameters.residue_n_ratio` | `ResidueNRatio` | 0.3..2 (59) | `1` | medium | ratio of residue to product N concentration |
| [ ] | `Cultivar_Parameters.lt50_cultivar` | `LT50cultivar` | -24..0 (59) | `°C` | high | lethal temperature, values -24..0 |
| [ ] | `Cultivar_Parameters.crop_height_p1` | `CropHeightP1` | 3.74..12 (59) | `1` | medium | shape parameter of the height curve |
| [ ] | `Cultivar_Parameters.crop_height_p2` | `CropHeightP2` | 0..0.6 (59) | `1` | medium | shape parameter of the height curve |
| [ ] | `Cultivar_Parameters.assimilate_partitioning_coeff` |  |  | `0-1` | high | per stage and organ, rows sum to 1 |
| [ ] | `Cultivar_Parameters.organ_senescence_rate` |  |  | `d-1` | medium | per stage and organ (old DEAD) |
| [ ] | `Cultivar_Parameters.drought_stress_threshold` | `DroughtStressThreshold` | 0..1 (373) | `0-1` | high | per stage, values 0..1 |
| [ ] | `Cultivar_Parameters.vernalisation_requirement` | `VernalisationRequirement` | 0..50 (373) | `d` | high | vernalisation days, values 0..50 |
| [ ] | `Cultivar_Parameters.heat_sum_irrigation_start` | `HeatSumIrrigationStart` | 0..600 (59) | `°C d` | high | a temperature sum, values 0..600 |
| [ ] | `Cultivar_Parameters.heat_sum_irrigation_end` | `HeatSumIrrigationEnd` | 600..2300 (59) | `°C d` | high | a temperature sum, values 600..2300 |
| [ ] | `Cultivar_Parameters.frost_hardening` | `FrostHardening` | 0, 0.014, 0.018 | `1` | low | parameter of the LT50 model |
| [ ] | `Cultivar_Parameters.frost_dehardening` | `FrostDehardening` | 0, 5.05 | `1` | low | parameter of the LT50 model |
| [ ] | `Cultivar_Parameters.low_temperature_exposure` | `LowTemperatureExposure` | 0, 0.654 | `1` | low | parameter of the LT50 model |
| [ ] | `Cultivar_Parameters.respiratory_stress` | `RespiratoryStress` | 0, 0.54 | `1` | low | parameter of the LT50 model |
| [ ] | `Cultivar_Parameters.latest_harvest_doy` | `LatestHarvestDoy` | -1 | `` | high | a day of the year (-1 = unset) |
| [ ] | `Cultivar_Parameters.early_ref_leaf_exp` | `EarlyRefLeafExp` |  | `°C` | medium | reference temperature of leaf expansion (12 = wheat) |
| [ ] | `Cultivar_Parameters.ref_leaf_exp` | `RefLeafExp` |  | `°C` | medium | reference temperature of leaf expansion (20 = wheat, 22 = maize) |
| [ ] | `Cultivar_Parameters.min_temp_dev_we` | `MinTempDev_WE` |  | `°C` | high | Wang-Engel cardinal temperature |
| [ ] | `Cultivar_Parameters.opt_temp_dev_we` | `OptTempDev_WE` |  | `°C` | high | Wang-Engel cardinal temperature |
| [ ] | `Cultivar_Parameters.max_temp_dev_we` | `MaxTempDev_WE` |  | `°C` | high | Wang-Engel cardinal temperature |

## `Soil_Moisture_Module_Parameters` (monica/params/module_parameters.odin)

| ok | field | JSON key | values | proposed unit | confidence | basis |
| --- | --- | --- | --- | --- | --- | --- |
| [ ] | `Soil_Moisture_Module_Parameters.saturated_hydraulic_conductivity` |  |  | `mm d-1` | medium | a conductivity |
| [ ] | `Soil_Moisture_Module_Parameters.surface_roughness` | `SurfaceRoughness` | 0.02 | `m` | medium | value 0.02 |
| [ ] | `Soil_Moisture_Module_Parameters.groundwater_discharge` | `GroundwaterDischarge` | 3 | `1` | low | value 3; meaning unclear from the name |
| [ ] | `Soil_Moisture_Module_Parameters.hydraulic_conductivity_redux` | `HydraulicConductivityRedux` | 0.1 | `0-1` | high | a reduction factor, value 0.1 |
| [ ] | `Soil_Moisture_Module_Parameters.snow_accumulation_threshold_temperature` |  |  | `°C` | high | a temperature |
| [ ] | `Soil_Moisture_Module_Parameters.kc_factor` | `KcFactor` | 0.75 | `1` | high | bare soil crop coefficient |
| [ ] | `Soil_Moisture_Module_Parameters.temperature_limit_for_liquid_water` |  |  | `°C` | high | a temperature |
| [ ] | `Soil_Moisture_Module_Parameters.correction_snow` | `CorrectionSnow` | 1.14 | `1` | high | precipitation correction factor |
| [ ] | `Soil_Moisture_Module_Parameters.correction_rain` | `CorrectionRain` | 1 | `1` | high | precipitation correction factor |
| [ ] | `Soil_Moisture_Module_Parameters.snow_max_additional_density` | `SnowMaxAdditionalDensity` | 0.25 | `kg dm-3` | medium | a snow density like Snow_Component.snow_density |
| [ ] | `Soil_Moisture_Module_Parameters.new_snow_density_min` | `NewSnowDensityMin` | 0.1 | `kg dm-3` | medium | a snow density, value 0.1 |
| [ ] | `Soil_Moisture_Module_Parameters.snow_retention_capacity_min` | `SnowRetentionCapacityMin` | 0.05 | `0-1` | medium | water retention as a fraction of the snow pack |
| [ ] | `Soil_Moisture_Module_Parameters.refreeze_parameter1` | `RefreezeParameter1` | 1.5 | `1` | low | refreeze model parameter |
| [ ] | `Soil_Moisture_Module_Parameters.refreeze_parameter2` | `RefreezeParameter2` | 0.36 | `1` | low | refreeze model parameter |
| [ ] | `Soil_Moisture_Module_Parameters.refreeze_temperature` | `RefreezeTemperature` | -1.7 | `°C` | high | a temperature (C++ Snow_Component: [°C]) |
| [ ] | `Soil_Moisture_Module_Parameters.snow_melt_temperature` | `SnowMeltTemperature` | 0.31 | `°C` | high | a temperature (C++ Snow_Component: [°C]) |
| [ ] | `Soil_Moisture_Module_Parameters.snow_packing` | `SnowPacking` | 0.01 | `1` | low | packing parameter, value 0.01 |
| [ ] | `Soil_Moisture_Module_Parameters.snow_retention_capacity_max` | `SnowRetentionCapacityMax` | 0.17 | `0-1` | medium | water retention as a fraction of the snow pack |
| [ ] | `Soil_Moisture_Module_Parameters.evaporation_zeta` | `EvaporationZeta` | 40 | `1` | medium | shape parameter of the evaporation reduction, value 40 |
| [ ] | `Soil_Moisture_Module_Parameters.xsa_critical_soil_moisture` | `XSACriticalSoilMoisture` | 0.1 | `m3 m-3` | medium | a soil moisture, value 0.1 |
| [ ] | `Soil_Moisture_Module_Parameters.maximum_evaporation_impact_depth` | `MaximumEvaporationImpactDepth` | 5 | `layer#` | medium | value 5; the ET module input of the same name is [layers] |
| [ ] | `Soil_Moisture_Module_Parameters.max_percolation_rate` | `MaxPercolationRate` | 10 | `mm d-1` | high | C++ SoilMoisture::pm_MaxPercolationRate [mm d-1] |
| [ ] | `Soil_Moisture_Module_Parameters.moisture_init_value` | `MoistureInitValue` | 0 | `0-1` | medium | initial moisture as a fraction of field capacity |

## `Soil_Temperature_Module_Parameters` (monica/params/module_parameters.odin)

| ok | field | JSON key | values | proposed unit | confidence | basis |
| --- | --- | --- | --- | --- | --- | --- |
| [ ] | `Soil_Temperature_Module_Parameters.n_tau` | `NTau` | 0.65 | `1` | medium | value 0.65 |
| [ ] | `Soil_Temperature_Module_Parameters.initial_surface_temperature` | `InitialSurfaceTemperature` | 10 | `°C` | high | a temperature |
| [ ] | `Soil_Temperature_Module_Parameters.base_temperature` | `BaseTemperature` | -15..15 (175) | `°C` | high | a temperature |
| [ ] | `Soil_Temperature_Module_Parameters.soil_albedo` | `SoilAlbedo` | 0.7 | `0-1` | high | an albedo |
| [ ] | `Soil_Temperature_Module_Parameters.soil_moisture` | `SoilMoisture` | 0.25 | `m3 m-3` | high | a soil moisture, value 0.25 |

## `Soil_Transport_Module_Parameters` (monica/params/module_parameters.odin)

| ok | field | JSON key | values | proposed unit | confidence | basis |
| --- | --- | --- | --- | --- | --- | --- |
| [ ] | `Soil_Transport_Module_Parameters.dispersion_length` | `DispersionLength` | 0.049 | `m` | medium | value 0.049 |
| [ ] | `Soil_Transport_Module_Parameters.ad` | `AD` | 0.002 | `1` | medium | tortuosity factor (Crop_Module_Parameters.tortuosity is "old AD") |
| [ ] | `Soil_Transport_Module_Parameters.diffusion_coefficient_standard` | `DiffusionCoefficientStandard` | 0.000214 | `m2 d-1` | medium | a diffusion coefficient, value 0.000214 |
| [ ] | `Soil_Transport_Module_Parameters.n_deposition` | `NDeposition` | 0 | `kg N ha-1 y-1` | high | as Site_Parameters.n_deposition |

## `Stics_Parameters` (monica/params/module_parameters.odin)

| ok | field | JSON key | values | proposed unit | confidence | basis |
| --- | --- | --- | --- | --- | --- | --- |
| [ ] | `Stics_Parameters.code_vnit` | `code_vnit` | 1 | `` | high | an option switch |
| [ ] | `Stics_Parameters.code_tnit` | `code_tnit` | 2 | `` | high | an option switch |
| [ ] | `Stics_Parameters.code_rationit` | `code_rationit` | 2 | `` | high | an option switch |
| [ ] | `Stics_Parameters.code_hourly_wfps_nit` | `code_hourly_wfps_nit` | 2 | `` | high | an option switch |
| [ ] | `Stics_Parameters.code_pdenit` | `code_pdenit` | 1 | `` | high | an option switch |
| [ ] | `Stics_Parameters.code_ratiodenit` | `code_ratiodenit` | 2 | `` | high | an option switch |
| [ ] | `Stics_Parameters.code_hourly_wfps_denit` | `code_hourly_wfps_denit` | 2 | `` | high | an option switch |
| [ ] | `Stics_Parameters.hminn` | `hminn` | 0.3 | `0-1` | medium | moisture threshold of nitrification (relative) |
| [ ] | `Stics_Parameters.hoptn` | `hoptn` | 0.9 | `0-1` | medium | moisture optimum of nitrification (relative) |
| [ ] | `Stics_Parameters.ph_min_nit` | `pHminnit` | 4 | `1` | high | a pH |
| [ ] | `Stics_Parameters.ph_max_nit` | `pHmaxnit` | 7.2 | `1` | high | a pH |
| [ ] | `Stics_Parameters.ph_min_den` | `pHminden` | 5.6 | `1` | high | a pH |
| [ ] | `Stics_Parameters.ph_max_den` | `pHmaxden` | 9.2 | `1` | high | a pH |
| [ ] | `Stics_Parameters.wfpsc` | `wfpsc` | 0.62 | `0-1` | high | a water-filled pore space |
| [ ] | `Stics_Parameters.rationit` | `rationit` | 0.0016 | `0-1` | high | N2O fraction of nitrification |
| [ ] | `Stics_Parameters.ratiodenit` | `ratiodenit` | 0.2 | `0-1` | high | N2O fraction of denitrification |

## `Soil_Organic_Module_Parameters` (monica/params/module_parameters.odin)

| ok | field | JSON key | values | proposed unit | confidence | basis |
| --- | --- | --- | --- | --- | --- | --- |
| [ ] | `Soil_Organic_Module_Parameters.som_slow_utilization_efficiency` | `SOM_SlowUtilizationEfficiency` | 0.4 | `0-1` | high | C++ comment: []; an efficiency |
| [ ] | `Soil_Organic_Module_Parameters.som_fast_utilization_efficiency` | `SOM_FastUtilizationEfficiency` | 0.5 | `0-1` | high | C++ comment: []; an efficiency |
| [ ] | `Soil_Organic_Module_Parameters.aom_slow_utilization_efficiency` | `AOM_SlowUtilizationEfficiency` | 0.4 | `0-1` | high | C++ comment: []; an efficiency |
| [ ] | `Soil_Organic_Module_Parameters.aom_fast_utilization_efficiency` | `AOM_FastUtilizationEfficiency` | 0.1 | `0-1` | high | C++ comment: []; an efficiency |
| [ ] | `Soil_Organic_Module_Parameters.aom_fast_max_c_to_n` | `AOM_FastMaxC_to_N` | 1000 | `1` | high | a C:N ratio |
| [ ] | `Soil_Organic_Module_Parameters.part_som_fast_to_som_slow` | `PartSOM_Fast_to_SOM_Slow` | 0.3 | `kg kg-1` | high | a partition, like the documented part_aom_* parameters |
| [ ] | `Soil_Organic_Module_Parameters.part_smb_slow_to_som_fast` | `PartSMB_Slow_to_SOM_Fast` | 0.6 | `kg kg-1` | high | a partition, like the documented part_aom_* parameters |
| [ ] | `Soil_Organic_Module_Parameters.part_smb_fast_to_som_fast` | `PartSMB_Fast_to_SOM_Fast` | 0.6 | `kg kg-1` | high | a partition, like the documented part_aom_* parameters |
| [ ] | `Soil_Organic_Module_Parameters.part_som_to_smb_slow` | `PartSOM_to_SMB_Slow` | 0.015 | `kg kg-1` | high | a partition, like the documented part_aom_* parameters |
| [ ] | `Soil_Organic_Module_Parameters.part_som_to_smb_fast` | `PartSOM_to_SMB_Fast` | 0.0002 | `kg kg-1` | high | a partition, like the documented part_aom_* parameters |
| [ ] | `Soil_Organic_Module_Parameters.cn_ratio_smb` | `CN_Ratio_SMB` | 6.7 | `1` | high | a C:N ratio; C++ comment: [] |
| [ ] | `Soil_Organic_Module_Parameters.q_ten_factor` | `QTenFactor` | 2.95 | `1` | high | Q10; C++ comment: [] |
| [ ] | `Soil_Organic_Module_Parameters.denit1` | `Denit1` | 0.2 | `1` | low | denitrification parameter |
| [ ] | `Soil_Organic_Module_Parameters.denit2` | `Denit2` | 0.8 | `1` | low | denitrification parameter |
| [ ] | `Soil_Organic_Module_Parameters.denit3` | `Denit3` | 0.9 | `1` | low | denitrification parameter |
| [ ] | `Soil_Organic_Module_Parameters.hydrolysis_km` | `HydrolysisKM` | 0.00334 | `mol L-1` | low | Michaelis constant of urea hydrolysis (Tabatabai 1973) |
| [ ] | `Soil_Organic_Module_Parameters.activation_energy` | `ActivationEnergy` | 41000 | `J mol-1` | high | an activation energy, 41000 (Gould et al. 1973) |
| [ ] | `Soil_Organic_Module_Parameters.hydrolysis_p1` | `HydrolysisP1` | 0 | `1` | low | hydrolysis parameter (Sadeghi et al. 1988) |
| [ ] | `Soil_Organic_Module_Parameters.hydrolysis_p2` | `HydrolysisP2` | 0 | `1` | low | hydrolysis parameter (Sadeghi et al. 1988) |
| [ ] | `Soil_Organic_Module_Parameters.max_mineralisation_depth` | `MaxMineralisationDepth` | 0.4 | `m` | high | value 0.4 |

## `Yield_Component` (monica/params/organic_parameters.odin)

| ok | field | JSON key | values | proposed unit | confidence | basis |
| --- | --- | --- | --- | --- | --- | --- |
| [ ] | `Yield_Component.organ_id` | `organId` | 2..6 (124) | `` | high | an organ number |
| [ ] | `Yield_Component.yield_percentage` | `yieldPercentage` | 0.01..1 (124) | `0-1` | high | values 0.01..1 (a fraction despite the name) |
| [ ] | `Yield_Component.yield_dry_matter` | `yieldDryMatter` | 0.06..0.91 (124) | `kg DM kg FM-1` | high | dry matter content, values 0.06..0.91 |

## `Automatic_Harvest_Parameters` (monica/params/organic_parameters.odin)

| ok | field | JSON key | values | proposed unit | confidence | basis |
| --- | --- | --- | --- | --- | --- | --- |
| [ ] | `Automatic_Harvest_Parameters.latest_harvest_doy` | `latestHarvestDOY` |  | `` | high | a day of the year |

## `NMin_Crop_Parameters` (monica/params/organic_parameters.odin)

| ok | field | JSON key | values | proposed unit | confidence | basis |
| --- | --- | --- | --- | --- | --- | --- |
| [ ] | `NMin_Crop_Parameters.sampling_depth` | `samplingDepth` |  | `m` | high | as Species_Parameters.sampling_depth |
| [ ] | `NMin_Crop_Parameters.n_target` | `nTarget` |  | `kg N ha-1` | high | as Species_Parameters.target_n_sampling_depth |
| [ ] | `NMin_Crop_Parameters.n_target30` | `nTarget30` |  | `kg N ha-1` | high | as Species_Parameters.target_n30 |

## `Organic_Matter_Parameters` (monica/params/organic_parameters.odin)

| ok | field | JSON key | values | proposed unit | confidence | basis |
| --- | --- | --- | --- | --- | --- | --- |
| [ ] | `Organic_Matter_Parameters.cn_ratio_aom_slow` | `CN_Ratio_AOM_Slow` | 13.1..200 (99) | `1` | high | a C:N ratio; C++ comment: [] |
| [ ] | `Organic_Matter_Parameters.cn_ratio_aom_fast` | `CN_Ratio_AOM_Fast` | 0..16 (99) | `1` | high | a C:N ratio; C++ comment: [] |

## `NMin_Application_Parameters` (monica/params/site_sim_parameters.odin)

| ok | field | JSON key | values | proposed unit | confidence | basis |
| --- | --- | --- | --- | --- | --- | --- |
| [ ] | `NMin_Application_Parameters.min` | `min` |  | `kg N ha-1` | high | minimum fertiliser application (sim.json NMinUserParams) |
| [ ] | `NMin_Application_Parameters.max` | `max` |  | `kg N ha-1` | high | maximum fertiliser application |
| [ ] | `NMin_Application_Parameters.delay_in_days` | `delayInDays` |  | `d` | high | days |

## `Irrigation_Parameters` (monica/params/site_sim_parameters.odin)

| ok | field | JSON key | values | proposed unit | confidence | basis |
| --- | --- | --- | --- | --- | --- | --- |
| [ ] | `Irrigation_Parameters.fw` |  |  | `0-1` | high | C++ comment: fraction of wetted soil surface [0-1] |

## `Automatic_Irrigation_Parameters` (monica/params/site_sim_parameters.odin)

| ok | field | JSON key | values | proposed unit | confidence | basis |
| --- | --- | --- | --- | --- | --- | --- |
| [ ] | `Automatic_Irrigation_Parameters.amount` | `amount` |  | `mm` | high | sim.json: [17, "mm"] |
| [ ] | `Automatic_Irrigation_Parameters.percent_nfc` | `set_to_%nFC` |  | `%` | high | sim.json: "_set_to_%nFC": [100, "%"] |
| [ ] | `Automatic_Irrigation_Parameters.threshold` | `trigger_if_nFC_below_%` |  | `%` | high | sim.json: "trigger_if_nFC_below_%": [90, "%"] |
| [ ] | `Automatic_Irrigation_Parameters.critical_moisture_depth_m` |  |  | `m` | high | sim.json: "calc_nFC_until_depth_m": [0.3, "m"] |
| [ ] | `Automatic_Irrigation_Parameters.min_days_between_irrigation_events` | `minDaysBetweenIrrigationEvents` |  | `d` | high | days |

## `Measured_Groundwater_Table_Information` (monica/params/site_sim_parameters.odin)

| ok | field | JSON key | values | proposed unit | confidence | basis |
| --- | --- | --- | --- | --- | --- | --- |
| [ ] | `Measured_Groundwater_Table_Information.groundwater_info` |  |  | `m` | medium | ISO date -> groundwater depth |

## `Site_Parameters` (monica/params/site_sim_parameters.odin)

| ok | field | JSON key | values | proposed unit | confidence | basis |
| --- | --- | --- | --- | --- | --- | --- |
| [ ] | `Site_Parameters.latitude` | `Latitude` |  | `deg` | high | a latitude |
| [ ] | `Site_Parameters.soil_cn_ratio` | `Soil_CN_Ratio` |  | `1` | high | a C:N ratio |
| [ ] | `Site_Parameters.drainage_coeff` | `DrainageCoeff` |  | `1` | low | drainage coefficient |
| [ ] | `Site_Parameters.soil_specific_humus_balance_correction` |  |  | `Heq ha-1` | medium | C++ comment: humus equivalents |
| [ ] | `Site_Parameters.bare_soil_kc_factor` | `Bare_soil_KC_factor` |  | `1` | high | a crop coefficient |
| [ ] | `Site_Parameters.number_of_layers` | `NumberOfLayers` |  | `1` | high | a count |
| [ ] | `Site_Parameters.layer_thickness` | `LayerThickness` |  | `m` | high | as Soil_Layer.layer_thickness_m |

## `Simulation_Parameters` (monica/params/site_sim_parameters.odin)

| ok | field | JSON key | values | proposed unit | confidence | basis |
| --- | --- | --- | --- | --- | --- | --- |
| [ ] | `Simulation_Parameters.number_of_layers` | `NumberOfLayers` |  | `1` | high | a count |
| [ ] | `Simulation_Parameters.layer_thickness` | `LayerThickness` |  | `m` | high | as Soil_Layer.layer_thickness_m |
| [ ] | `Simulation_Parameters.start_pv_index` | `StartPVIndex` |  | `` | high | an index |
| [ ] | `Simulation_Parameters.julian_day_automatic_fertilising` | `JulianDayAutomaticFertilising` |  | `` | high | a day of the year |
| [ ] | `Simulation_Parameters.no_of_previous_days_serialized_climate_data` |  |  | `d` | high | days |

## `Crop_Module_Parameters` (monica/params/site_sim_parameters.odin)

| ok | field | JSON key | values | proposed unit | confidence | basis |
| --- | --- | --- | --- | --- | --- | --- |
| [ ] | `Crop_Module_Parameters.canopy_reflection_coefficient` | `CanopyReflectionCoefficient` | 0.08 | `0-1` | high | a reflection coefficient, value 0.08 |
| [ ] | `Crop_Module_Parameters.reference_max_assimilation_rate` | `ReferenceMaxAssimilationRate` | 30 | `kg CO2 ha-1 h-1` | high | reference AMAX, value 30 |
| [ ] | `Crop_Module_Parameters.reference_leaf_area_index` | `ReferenceLeafAreaIndex` | 1.44 | `m2 m-2` | high | an LAI |
| [ ] | `Crop_Module_Parameters.maintenance_respiration_parameter1` |  |  | `1` | low | respiration model parameter |
| [ ] | `Crop_Module_Parameters.maintenance_respiration_parameter2` |  |  | `1` | low | respiration model parameter |
| [ ] | `Crop_Module_Parameters.minimum_n_concentration_root` | `MinimumNConcentrationRoot` | 0.005 | `kg N kg DM-1` | high | an N concentration, value 0.005 |
| [ ] | `Crop_Module_Parameters.reference_albedo` | `ReferenceAlbedo` | 0.23 | `0-1` | high | an albedo |
| [ ] | `Crop_Module_Parameters.stomata_conductance_alpha` | `StomataConductanceAlpha` | 40 | `1` | medium | stomata model parameter, value 40 |
| [ ] | `Crop_Module_Parameters.saturation_beta` | `SaturationBeta` | 2.5 | `1` | medium | stomata model parameter, value 2.5 |
| [ ] | `Crop_Module_Parameters.growth_respiration_redux` | `GrowthRespirationRedux` | 0.7 | `0-1` | medium | a reduction factor, value 0.7 |
| [ ] | `Crop_Module_Parameters.max_crop_n_demand` | `MaxCropNDemand` | 6 | `kg N ha-1 d-1` | medium | maximum daily N demand, value 6 |
| [ ] | `Crop_Module_Parameters.growth_respiration_parameter1` | `GrowthRespirationParameter1` | 0.1 | `1` | low | respiration model parameter, value 0.1 |
| [ ] | `Crop_Module_Parameters.growth_respiration_parameter2` | `GrowthRespirationParameter2` | 38 | `°C` | medium | value 38, used like a temperature |
| [ ] | `Crop_Module_Parameters.tortuosity` | `Tortuosity` | 0.002 | `1` | medium | a tortuosity factor (old AD) |
| [ ] | `Crop_Module_Parameters.time_under_anoxia_threshold` |  |  | `d` | high | per stage, compared with Crop_Module.time_under_anoxia (days) |
| [ ] | `Crop_Module_Parameters.intercropping_k_s` | `k_s` |  | `1` | low | intercropping parameter |
| [ ] | `Crop_Module_Parameters.intercropping_k_t` | `k_t` |  | `1` | low | intercropping parameter |
| [ ] | `Crop_Module_Parameters.intercropping_ph_redux` | `PHredux` |  | `0-1` | low | intercropping reduction factor |
| [ ] | `Crop_Module_Parameters.intercropping_dvs_phr` | `DVS_PHr` |  | `` | low | a development stage |

## `Environment_Parameters` (monica/params/site_sim_parameters.odin)

| ok | field | JSON key | values | proposed unit | confidence | basis |
| --- | --- | --- | --- | --- | --- | --- |
| [ ] | `Environment_Parameters.albedo` | `Albedo` | 0.23 | `0-1` | high | an albedo |
| [ ] | `Environment_Parameters.atmospheric_co2` | `AtmosphericCO2` | 0 | `ppm` | high | as Monica_Model.atmospheric_co2_concentration |
| [ ] | `Environment_Parameters.atmospheric_co2s` |  |  | `ppm` | high | year -> CO2 |
| [ ] | `Environment_Parameters.atmospheric_o3` | `AtmosphericO3` |  | `ppb` | high | as Monica_Model.atmospheric_o3_concentration |
| [ ] | `Environment_Parameters.atmospheric_o3s` |  |  | `ppb` | high | year -> O3 |
| [ ] | `Environment_Parameters.wind_speed_height_m` | `WindSpeedHeight` | 2 | `m` | high | a height |
| [ ] | `Environment_Parameters.leaching_depth_m` | `LeachingDepth` | 1.6 | `m` | high | a depth |
| [ ] | `Environment_Parameters.time_step` | `timeStep` | 1 | `d` | high | the model time step |
| [ ] | `Environment_Parameters.max_groundwater_depth_m` | `MaxGroundwaterDepth` | 18 | `m` | high | a depth |
| [ ] | `Environment_Parameters.min_groundwater_depth_m` | `MinGroundwaterDepth` | 20 | `m` | high | a depth |
| [ ] | `Environment_Parameters.min_groundwater_depth_month` | `MinGroundwaterDepthMonth` | 3 | `` | high | a month number |
