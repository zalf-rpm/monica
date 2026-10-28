# C++/legacy name -> Odin field name map

This file exists because `odin/CONVENTIONS.md` §2 used to require every
struct field to keep its exact C++ spelling, specifically so the trace-diff
regression oracle (`odin/monica/trace/trace.odin`) and the output-path
aliases (`odin/monica/io/output_paths.odin`) could match on name alone. As
of the Crop_Module cleanup, fields are snake_cased like the rest of Odin
instead - this file is the lookup that replaces "the name already tells
you" for anyone diffing against the C++ source or a C++ trace dump.

Each table covers one C++ struct. Add a new table here whenever a struct's
fields get renamed away from their C++ spelling.

The tables from `HarvestData::Spec::Value` to the end come from the second pass
(2026-10-28), which renamed every remaining C++-style struct field, local, parameter and
procedure: snake_case, C++ prefixes (`vs_`, `vo_`, `po_`, ...) and leading underscores dropped,
scientific symbols lowercased (`EF_ISO` -> `ef_iso`, `LAI` -> `lai`). Kept: the
`__enable_..._fix__` feature flags, named after their JSON keys. Tables headed "(port-only)" have no
C++ struct; their left column is the port's previous name. Locals and parameters aren't listed.

The trace-diff oracle itself does NOT need this file: every `_ref` test's
`tr.dump(t, "struct.OldCppName", cm.new_odin_name)` call already hardcodes
the C++-matching label as a separate string literal (see trace.odin's "the
walk" section) - so trace output keeps lining up with the C++ side
automatically, without consulting this table. This file is for humans
reading the two implementations side by side (or writing a new `_ref`
fixture), not for any running code.

## `monica::CropModule` -> `core.Crop_Module` (odin/monica/core/crop_module.odin)

| C++ (`crop-module.h`) | Odin |
| --- | --- |
| `addOrganicMatter` | `add_organic_matter` |
| `assimilatePartCoeffsReduced` | `assimilate_part_coeffs_reduced` |
| `cropModParams` | `crop_mod_params` |
| `cropParams` | `crop_params` |
| `cropPhotosynthesisResults` | `crop_photosynthesis_results` |
| `dyingOut` | `dying_out` |
| `fireEvent` | `fire_event` |
| `fractionOfInterceptedRadiation1` | `fraction_of_intercepted_radiation1` |
| `fractionOfInterceptedRadiation2` | `fraction_of_intercepted_radiation2` |
| `getSnowDepthAndCalcTempUnderSnow` | `get_snow_depth_and_calc_temp_under_snow` |
| `guentherEmissions` | `guenther_emissions` |
| `intercroppingOtherCropHeight` | `intercropping_other_crop_height` |
| `intercroppingOtherLAIt` | `intercropping_other_lait` |
| `jjvEmissions` | `jjv_emissions` |
| `noOfCropSteps` | `no_of_crop_steps` |
| `noOfDevStages` | `no_of_dev_stages` |
| `noOfOrgans` | `no_of_organs` |
| `pc_CO2Method` | `pc_co2_method` |
| `perennialCropDormancyPeriodEndDate` | `perennial_crop_dormancy_period_end_date` |
| `perennialCropParams` | `perennial_crop_params` |
| `residueParams` | `residue_params` |
| `rootNRedux` | `root_n_redux` |
| `simParams` | `sim_params` |
| `siteParams` | `site_params` |
| `soilColumn` | `soil_column` |
| `stemElongationEventFired` | `stem_elongation_event_fired` |
| `stepSize24` | `step_size24` |
| `stepSize240` | `step_size240` |
| `TimeUnderAnoxiaThresholdDefault` | `time_under_anoxia_threshold_default` |
| `vc_AbovegroundBiomass` | `aboveground_biomass` |
| `vc_AbovegroundBiomassOld` | `aboveground_biomass_old` |
| `vc_AccumulatedETa` | `accumulated_et_a` |
| `vc_AccumulatedTranspiration` | `accumulated_transpiration` |
| `vc_ActualTranspiration` | `actual_transpiration` |
| `vc_ActualTranspirationDeficit` | `actual_transpiration_deficit` |
| `vc_AnthesisDay` | `anthesis_day` |
| `vc_Assimilates` | `assimilates` |
| `vc_AssimilationRate` | `assimilation_rate` |
| `vc_AstronomicDayLenght` | `astronomic_day_lenght` |
| `vc_BelowgroundBiomass` | `belowground_biomass` |
| `vc_BelowgroundBiomassOld` | `belowground_biomass_old` |
| `vc_ClearDayRadiation` | `clear_day_radiation` |
| `vc_CriticalNConcentration` | `critical_n_concentration` |
| `vc_CropDiameter` | `crop_diameter` |
| `vc_CropFrostRedux` | `crop_frost_redux` |
| `vc_CropHeatRedux` | `crop_heat_redux` |
| `vc_CropHeight` | `crop_height` |
| `vc_CropNDemand` | `crop_n_demand` |
| `vc_CropNRedux` | `crop_n_redux` |
| `vc_CropWaterUptake` | `crop_water_uptake` |
| `vc_CurrentTemperatureSum` | `current_temperature_sum` |
| `vc_CurrentTotalTemperatureSum` | `current_total_temperature_sum` |
| `vc_CurrentTotalTemperatureSumRoot` | `current_total_temperature_sum_root` |
| `vc_CuttingDelayDays` | `cutting_delay_days` |
| `vc_DaylengthFactor` | `daylength_factor` |
| `vc_DaysAfterBeginFlowering` | `days_after_begin_flowering` |
| `vc_DaysSinceTransplant` | `days_since_transplant` |
| `vc_Declination` | `declination` |
| `vc_DevelopmentalStage` | `developmental_stage` |
| `vc_DroughtImpactOnFertility` | `drought_impact_on_fertility` |
| `vc_EffectiveDayLength` | `effective_day_length` |
| `vc_ErrorMessage` | `error_message` |
| `vc_ErrorStatus` | `error_status` |
| `vc_EvaporatedFromIntercept` | `evaporated_from_intercept` |
| `vc_exportedCutBiomass` | `exported_cut_biomass` |
| `vc_ExtraterrestrialRadiation` | `extraterrestrial_radiation` |
| `vc_FinalDevelopmentalStage` | `final_developmental_stage` |
| `vc_FixedN` | `fixed_n` |
| `vc_GlobalRadiation` | `global_radiation` |
| `vc_GreenAreaIndex` | `green_area_index` |
| `vc_GrossAssimilates` | `gross_assimilates` |
| `vc_GrossPhotosynthesis` | `gross_photosynthesis` |
| `vc_GrossPhotosynthesis_mol` | `gross_photosynthesis_mol` |
| `vc_GrossPhotosynthesisReference_mol` | `gross_photosynthesis_reference_mol` |
| `vc_GrossPrimaryProduction` | `gross_primary_production` |
| `vc_GrowthCycleEnded` | `growth_cycle_ended` |
| `vc_GrowthRespirationAS` | `growth_respiration_as` |
| `vc_InterceptionStorage` | `interception_storage` |
| `vc_Kcb_end` | `kcb_end` |
| `vc_Kcb_ini` | `kcb_ini` |
| `vc_Kcb_mid` | `kcb_mid` |
| `vc_KcbFactor` | `kcb_factor` |
| `vc_KcFactor` | `kc_factor` |
| `vc_KTkc` | `k_tkc` |
| `vc_KTko` | `k_tko` |
| `vc_LeafAreaIndex` | `leaf_area_index` |
| `vc_LT50` | `lt50` |
| `vc_LT50M` | `lt50_m` |
| `vc_MaintenanceRespirationAS` | `maintenance_respiration_as` |
| `vc_MaturityDay` | `maturity_day` |
| `vc_MaturityReached` | `maturity_reached` |
| `vc_MaxNUptake` | `max_n_uptake` |
| `vc_MaxRootingDepth` | `max_rooting_depth` |
| `vc_NConcentrationAbovegroundBiomass` | `n_concentration_aboveground_biomass` |
| `vc_NConcentrationAbovegroundBiomassOld` | `n_concentration_aboveground_biomass_old` |
| `vc_NConcentrationRoot` | `n_concentration_root` |
| `vc_NConcentrationRootOld` | `n_concentration_root_old` |
| `vc_NContentDeficit` | `n_content_deficit` |
| `vc_NetMaintenanceRespiration` | `net_maintenance_respiration` |
| `vc_NetPhotosynthesis` | `net_photosynthesis` |
| `vc_NetPrecipitation` | `net_precipitation` |
| `vc_NetPrimaryProduction` | `net_primary_production` |
| `vc_NUptakeFromLayer` | `n_uptake_from_layer` |
| `vc_O3_longTermDamage` | `o3_long_term_damage` |
| `vc_O3_senescence` | `o3_senescence` |
| `vc_O3_shortTermDamage` | `o3_short_term_damage` |
| `vc_O3_sumUptake` | `o3_sum_uptake` |
| `vc_O3_WStomatalClosure` | `o3_w_stomatal_closure` |
| `vc_OrganBiomass` | `organ_biomass` |
| `vc_OrganDeadBiomass` | `organ_dead_biomass` |
| `vc_OrganGreenBiomass` | `organ_green_biomass` |
| `vc_OrganGrowthIncrement` | `organ_growth_increment` |
| `vc_OrganSenescenceIncrement` | `organ_senescence_increment` |
| `vc_OvercastDayRadiation` | `overcast_day_radiation` |
| `vc_OxygenDeficit` | `oxygen_deficit` |
| `vc_PhotActRadiationMean` | `phot_act_radiation_mean` |
| `vc_PhotoperiodicDaylength` | `photoperiodic_daylength` |
| `vc_PotentialTranspiration` | `potential_transpiration` |
| `vc_PotentialTranspirationDeficit` | `potential_transpiration_deficit` |
| `vc_ReferenceEvapotranspiration` | `reference_evapotranspiration` |
| `vc_RelativeTotalDevelopment` | `relative_total_development` |
| `vc_RemainingEvapotranspiration` | `remaining_evapotranspiration` |
| `vc_ReserveAssimilatePool` | `reserve_assimilate_pool` |
| `vc_residueCutBiomass` | `residue_cut_biomass` |
| `vc_Respiration` | `respiration` |
| `vc_RootBiomass` | `root_biomass` |
| `vc_RootBiomassOld` | `root_biomass_old` |
| `vc_RootDensity` | `root_density` |
| `vc_RootDiameter` | `root_diameter` |
| `vc_RootEffectivity` | `root_effectivity` |
| `vc_RootingDepth` | `rooting_depth` |
| `vc_RootingDepth_m` | `rooting_depth_m` |
| `vc_RootingZone` | `rooting_zone` |
| `vc_shadedLeafAreaIndex` | `shaded_leaf_area_index` |
| `vc_SoilCoverage` | `soil_coverage` |
| `vc_SoilSpecificMaxRootingDepth` | `soil_specific_max_rooting_depth` |
| `vc_StomataResistance` | `stomata_resistance` |
| `vc_StorageOrgan` | `storage_organ` |
| `vc_sumExportedCutBiomass` | `sum_exported_cut_biomass` |
| `vc_sumResidueCutBiomass` | `sum_residue_cut_biomass` |
| `vc_SumTotalNUptake` | `sum_total_n_uptake` |
| `vc_sunlitLeafAreaIndex` | `sunlit_leaf_area_index` |
| `vc_TargetNConcentration` | `target_n_concentration` |
| `vc_TemperatureSumToFlowering` | `temperature_sum_to_flowering` |
| `vc_TimeStep` | `time_step` |
| `vc_TimeUnderAnoxia` | `time_under_anoxia` |
| `vc_TotalBiomass` | `total_biomass` |
| `vc_TotalBiomassNContent` | `total_biomass_n_content` |
| `vc_TotalCropHeatImpact` | `total_crop_heat_impact` |
| `vc_TotalNInput` | `total_n_input` |
| `vc_TotalNUptake` | `total_n_uptake` |
| `vc_TotalRespired` | `total_respired` |
| `vc_TotalRootLength` | `total_root_length` |
| `vc_TotalTemperatureSum` | `total_temperature_sum` |
| `vc_Transpiration` | `transpiration` |
| `vc_TranspirationDeficit` | `transpiration_deficit` |
| `vc_TranspirationReduced` | `transpiration_reduced` |
| `vc_TranspirationRedux` | `transpiration_redux` |
| `vc_TransplantEfficiency` | `transplant_efficiency` |
| `vc_TransplantShockDuration` | `transplant_shock_duration` |
| `vc_VernalisationDays` | `vernalisation_days` |
| `vc_VernalisationFactor` | `vernalisation_factor` |
| `vocSpecies` | `voc_species` |
| `vs_SoilMineralNContent` | `vs_soil_mineral_n_content` |
| `vs_SoilSpecificMaxRootingDepth` | `vs_soil_specific_max_rooting_depth` |

Fields not listed here (e.g. `intercropping`, `rad24`, `full240`) were
already lowercase/snake-shaped and needed no change.

## `monica::AOM_Properties` -> `core.Aom_Properties` (odin/monica/core/soil_column.odin)

| C++ (`soilcolumn.h`) | Odin |
| --- | --- |
| `vo_AOM_DryMatterContent` | `aom_dry_matter_content` |
| `vo_AOM_Fast` | `aom_fast` |
| `vo_AOM_FastDecCoeff` | `aom_fast_dec_coeff` |
| `vo_AOM_FastDecCoeffStandard` | `aom_fast_dec_coeff_standard` |
| `vo_AOM_FastDecRate_to_SMB_Fast` | `aom_fast_dec_rate_to_smb_fast` |
| `vo_AOM_FastDecRate_to_SMB_Slow` | `aom_fast_dec_rate_to_smb_slow` |
| `vo_AOM_FastDelta` | `aom_fast_delta` |
| `vo_AOM_NH4Content` | `aom_nh4_content` |
| `vo_AOM_Slow` | `aom_slow` |
| `vo_AOM_SlowDecCoeff` | `aom_slow_dec_coeff` |
| `vo_AOM_SlowDecCoeffStandard` | `aom_slow_dec_coeff_standard` |
| `vo_AOM_SlowDecRate_to_SMB_Fast` | `aom_slow_dec_rate_to_smb_fast` |
| `vo_AOM_SlowDecRate_to_SMB_Slow` | `aom_slow_dec_rate_to_smb_slow` |
| `vo_AOM_SlowDelta` | `aom_slow_delta` |
| `vo_CN_Ratio_AOM_Fast` | `cn_ratio_aom_fast` |
| `vo_CN_Ratio_AOM_Slow` | `cn_ratio_aom_slow` |
| `vo_DaysAfterApplication` | `days_after_application` |
| `vo_PartAOM_Slow_to_SMB_Fast` | `part_aom_slow_to_smb_fast` |
| `vo_PartAOM_Slow_to_SMB_Slow` | `part_aom_slow_to_smb_slow` |

Not renamed this round: `incorporation`, `noVolatilization` (plain camelCase, no `vo_` prefix).

## `monica::SoilLayer` -> `core.Soil_Layer` (odin/monica/core/soil_column.odin)

| C++ (`soilcolumn.h`) | Odin |
| --- | --- |
| `vs_FieldCapacity` | `field_capacity` |
| `vs_Lambda` | `lambda` |
| `vs_LayerThickness` | `layer_thickness` |
| `vs_PermanentWiltingPoint` | `permanent_wilting_point` |
| `vs_Saturation` | `saturation` |
| `vs_SMB_Fast` | `smb_fast` |
| `vs_SMB_Slow` | `smb_slow` |
| `vs_Soil_CN_Ratio` | `soil_cn_ratio` |
| `vs_SoilAmmonium` | `soil_ammonium` |
| `vs_SoilBulkDensity` | `soil_bulk_density` |
| `vs_SoilCarbamid` | `soil_carbamid` |
| `vs_SoilClayContent` | `soil_clay_content` |
| `vs_SoilFrozen` | `soil_frozen` |
| `vs_SoilMoisture_m3` | `soil_moisture_m3` |
| `vs_SoilMoisturePercentFC` | `soil_moisture_percent_fc` |
| `vs_SoilNH4` | `soil_nh4` |
| `vs_SoilNitrate` | `soil_nitrate` |
| `vs_SoilNO2` | `soil_no2` |
| `vs_SoilNO3` | `soil_no3` |
| `vs_SoilOrganicCarbon` | `soil_organic_carbon` |
| `vs_SoilOrganicMatter` | `soil_organic_matter` |
| `vs_SoilpH` | `soil_ph` |
| `vs_SoilRawDensity` | `soil_raw_density` |
| `vs_SoilSandContent` | `soil_sand_content` |
| `vs_SoilStoneContent` | `soil_stone_content` |
| `vs_SoilTemperature` | `soil_temperature` |
| `vs_SoilTexture` | `soil_texture` |
| `vs_SoilWaterFlux` | `soil_water_flux` |
| `vs_SOM_Fast` | `som_fast` |
| `vs_SOM_Slow` | `som_slow` |
| `vo_AOM_Pool` | `aom_pool` |

`vo_AOM_Pool` was renamed in the second pass (2026-10-28).

## `monica::FrostComponent` -> `core.Frost_Component` (odin/monica/core/frost_component.odin)

| C++ (`frost-component.h`) | Odin |
| --- | --- |
| `vm_accumulatedFrostDepth` | `accumulated_frost_depth` |
| `vm_FrostDays` | `frost_days` |
| `vm_FrostDepth` | `frost_depth` |
| `vm_HydraulicConductivityRedux` | `hydraulic_conductivity_redux` |
| `vm_LambdaRedux` | `lambda_redux` |
| `vm_NegativeDegreeDays` | `negative_degree_days` |
| `vm_TemperatureUnderSnow` | `temperature_under_snow` |
| `vm_ThawDepth` | `thaw_depth` |

Not renamed this round: `soilColumn`, `pt_TimeStep` (no `vm_` prefix). `pm_HydraulicConductivityRedux` was renamed later, see `monica::SoilMoistureModuleParameters` below.

## `monica::SnowComponent` -> `core.Snow_Component` (odin/monica/core/snow_component.odin)

| C++ (`snow-component.h`) | Odin |
| --- | --- |
| `vm_AccumulatedSnowDepth` | `accumulated_snow_depth` |
| `vm_CorrectionRain` | `correction_rain` |
| `vm_CorrectionSnow` | `correction_snow` |
| `vm_FrozenWaterInSnow` | `frozen_water_in_snow` |
| `vm_LiquidWaterInSnow` | `liquid_water_in_snow` |
| `vm_maxSnowDepth` | `max_snow_depth` |
| `vm_NewSnowDensityMin` | `new_snow_density_min` |
| `vm_RefreezeP1` | `refreeze_p1` |
| `vm_RefreezeP2` | `refreeze_p2` |
| `vm_RefreezeTemperature` | `refreeze_temperature` |
| `vm_SnowAccumulationThresholdTemperature` | `snow_accumulation_threshold_temperature` |
| `vm_SnowDensity` | `snow_density` |
| `vm_SnowDepth` | `snow_depth` |
| `vm_SnowMaxAdditionalDensity` | `snow_max_additional_density` |
| `vm_SnowmeltTemperature` | `snowmelt_temperature` |
| `vm_SnowPacking` | `snow_packing` |
| `vm_SnowRetentionCapacityMax` | `snow_retention_capacity_max` |
| `vm_SnowRetentionCapacityMin` | `snow_retention_capacity_min` |
| `vm_TemperatureLimitForLiquidWater` | `temperature_limit_for_liquid_water` |
| `vm_WaterToInfiltrate` | `water_to_infiltrate` |

Not renamed this round: `soilColumn` (no `vm_` prefix). Note Soil_Column has its own, separate `vm_SnowDepth` mirror field (`sc.soilColumn.vm_SnowDepth = sc.snow_depth`) - out of scope this round, unrelated to Snow_Component's own field of the same C++ name.

## `monica::SoilMoistureModuleParameters` -> `params.Soil_Moisture_Module_Parameters` (odin/monica/params/module_parameters.odin)

| C++ (`monica-parameters.h`) | Odin |
| --- | --- |
| `pm_SaturatedHydraulicConductivity` | `saturated_hydraulic_conductivity` |
| `pm_SurfaceRoughness` | `surface_roughness` |
| `pm_GroundwaterDischarge` | `groundwater_discharge` |
| `pm_HydraulicConductivityRedux` | `hydraulic_conductivity_redux` |
| `pm_SnowAccumulationTresholdTemperature` | `snow_accumulation_threshold_temperature` |
| `pm_KcFactor` | `kc_factor` |
| `pm_TemperatureLimitForLiquidWater` | `temperature_limit_for_liquid_water` |
| `pm_CorrectionSnow` | `correction_snow` |
| `pm_CorrectionRain` | `correction_rain` |
| `pm_SnowMaxAdditionalDensity` | `snow_max_additional_density` |
| `pm_NewSnowDensityMin` | `new_snow_density_min` |
| `pm_SnowRetentionCapacityMin` | `snow_retention_capacity_min` |
| `pm_RefreezeParameter1` | `refreeze_parameter1` |
| `pm_RefreezeParameter2` | `refreeze_parameter2` |
| `pm_RefreezeTemperature` | `refreeze_temperature` |
| `pm_SnowMeltTemperature` | `snow_melt_temperature` |
| `pm_SnowPacking` | `snow_packing` |
| `pm_SnowRetentionCapacityMax` | `snow_retention_capacity_max` |
| `pm_EvaporationZeta` | `evaporation_zeta` |
| `pm_XSACriticalSoilMoisture` | `xsa_critical_soil_moisture` |
| `pm_MaximumEvaporationImpactDepth` | `maximum_evaporation_impact_depth` |
| `pm_MaxPercolationRate` | `max_percolation_rate` |
| `pm_MoistureInitValue` | `moisture_init_value` |

`pm_SnowAccumulationTresholdTemperature`'s "Treshold" typo (missing `h`) is only in the C++ field
name and the external JSON key ("SnowAccumulationTresholdTemperature", unchanged - both the merge
and to_json procs still read/write that literal key, since it must match the parameter files on
disk). The Odin field itself is spelled correctly, matching the correctly-spelled, unrelated
`vm_SnowAccumulationThresholdTemperature` in `Snow_Component` above.

## `monica::SoilTemperatureModuleParameters` -> `params.Soil_Temperature_Module_Parameters` (odin/monica/params/module_parameters.odin)

| C++ (`monica-parameters.h`) | Odin |
| --- | --- |
| `pt_NTau` | `n_tau` |
| `pt_InitialSurfaceTemperature` | `initial_surface_temperature` |
| `pt_BaseTemperature` | `base_temperature` |
| `pt_QuartzRawDensity` | `quartz_raw_density` |
| `pt_DensityAir` | `density_air` |
| `pt_DensityWater` | `density_water` |
| `pt_DensityHumus` | `density_humus` |
| `pt_SpecificHeatCapacityAir` | `specific_heat_capacity_air` |
| `pt_SpecificHeatCapacityQuartz` | `specific_heat_capacity_quartz` |
| `pt_SpecificHeatCapacityWater` | `specific_heat_capacity_water` |
| `pt_SpecificHeatCapacityHumus` | `specific_heat_capacity_humus` |
| `pt_SoilAlbedo` | `soil_albedo` |
| `pt_SoilMoisture` | `soil_moisture` |

## `monica::SoilTransportModuleParameters` -> `params.Soil_Transport_Module_Parameters` (odin/monica/params/module_parameters.odin)

| C++ (`monica-parameters.h`) | Odin |
| --- | --- |
| `pq_DispersionLength` | `dispersion_length` |
| `pq_AD` | `ad` |
| `pq_DiffusionCoefficientStandard` | `diffusion_coefficient_standard` |
| `pq_NDeposition` | `n_deposition` |

## `monica::SoilOrganicModuleParameters` -> `params.Soil_Organic_Module_Parameters` (odin/monica/params/module_parameters.odin)

| C++ (`monica-parameters.h`) | Odin |
| --- | --- |
| `po_SOM_SlowDecCoeffStandard` | `som_slow_dec_coeff_standard` |
| `po_SOM_FastDecCoeffStandard` | `som_fast_dec_coeff_standard` |
| `po_SMB_SlowMaintRateStandard` | `smb_slow_maint_rate_standard` |
| `po_SMB_FastMaintRateStandard` | `smb_fast_maint_rate_standard` |
| `po_SMB_SlowDeathRateStandard` | `smb_slow_death_rate_standard` |
| `po_SMB_FastDeathRateStandard` | `smb_fast_death_rate_standard` |
| `po_SMB_UtilizationEfficiency` | `smb_utilization_efficiency` |
| `po_SOM_SlowUtilizationEfficiency` | `som_slow_utilization_efficiency` |
| `po_SOM_FastUtilizationEfficiency` | `som_fast_utilization_efficiency` |
| `po_AOM_SlowUtilizationEfficiency` | `aom_slow_utilization_efficiency` |
| `po_AOM_FastUtilizationEfficiency` | `aom_fast_utilization_efficiency` |
| `po_AOM_FastMaxC_to_N` | `aom_fast_max_c_to_n` |
| `po_PartSOM_Fast_to_SOM_Slow` | `part_som_fast_to_som_slow` |
| `po_PartSMB_Slow_to_SOM_Fast` | `part_smb_slow_to_som_fast` |
| `po_PartSMB_Fast_to_SOM_Fast` | `part_smb_fast_to_som_fast` |
| `po_PartSOM_to_SMB_Slow` | `part_som_to_smb_slow` |
| `po_PartSOM_to_SMB_Fast` | `part_som_to_smb_fast` |
| `po_CN_Ratio_SMB` | `cn_ratio_smb` |
| `po_LimitClayEffect` | `limit_clay_effect` |
| `po_QTenFactor` | `q_ten_factor` |
| `po_TempDecOptimal` | `temp_dec_optimal` |
| `po_MoistureDecOptimal` | `moisture_dec_optimal` |
| `po_AmmoniaOxidationRateCoeffStandard` | `ammonia_oxidation_rate_coeff_standard` |
| `po_NitriteOxidationRateCoeffStandard` | `nitrite_oxidation_rate_coeff_standard` |
| `po_TransportRateCoeff` | `transport_rate_coeff` |
| `po_SpecAnaerobDenitrification` | `spec_anaerob_denitrification` |
| `po_ImmobilisationRateCoeffNO3` | `immobilisation_rate_coeff_no3` |
| `po_ImmobilisationRateCoeffNH4` | `immobilisation_rate_coeff_nh4` |
| `po_Denit1` | `denit1` |
| `po_Denit2` | `denit2` |
| `po_Denit3` | `denit3` |
| `po_HydrolysisKM` | `hydrolysis_km` |
| `po_ActivationEnergy` | `activation_energy` |
| `po_HydrolysisP1` | `hydrolysis_p1` |
| `po_HydrolysisP2` | `hydrolysis_p2` |
| `po_AtmosphericResistance` | `atmospheric_resistance` |
| `po_N2OProductionRate` | `n2o_production_rate` |
| `po_Inhibitor_NH3` | `inhibitor_nh3` |
| `ps_MaxMineralisationDepth` | `max_mineralisation_depth` |
| `sticsParams` | `stics_params` |

`ps_MaxMineralisationDepth` and `sticsParams` are members of `SoilOrganicModuleParameters` despite
not following its `po_` prefix (confirmed in `src/core/monica-parameters.h`/`.cpp` - genuinely `ps_`
in the C++, not a transcription slip); renamed anyway for consistency with the rest of the struct.
The three `__enable_kaiteew_*__` flags are left as-is - they follow the codebase-wide dunder-flag
convention also used by `__enable_vernalisation_factor_fix__` and friends in `crop_parameters.odin`,
where the literal name doubles as the JSON key, not an unconverted CamelCase leftover.

## `monica::OrganicMatterParameters` -> `params.Organic_Matter_Parameters` (odin/monica/params/organic_parameters.odin)

| C++ (`monica-parameters.h`) | Odin |
| --- | --- |
| `vo_AOM_DryMatterContent` | `aom_dry_matter_content` |
| `vo_AOM_NH4Content` | `aom_nh4_content` |
| `vo_AOM_NO3Content` | `aom_no3_content` |
| `vo_AOM_CarbamidContent` | `aom_carbamid_content` |
| `vo_CorgContent` | `corg_content` |
| `vo_AOM_SlowDecCoeffStandard` | `aom_slow_dec_coeff_standard` |
| `vo_AOM_FastDecCoeffStandard` | `aom_fast_dec_coeff_standard` |
| `vo_PartAOM_to_AOM_Slow` | `part_aom_to_aom_slow` |
| `vo_PartAOM_to_AOM_Fast` | `part_aom_to_aom_fast` |
| `vo_CN_Ratio_AOM_Slow` | `cn_ratio_aom_slow` |
| `vo_CN_Ratio_AOM_Fast` | `cn_ratio_aom_fast` |
| `vo_PartAOM_Slow_to_SMB_Slow` | `part_aom_slow_to_smb_slow` |
| `vo_PartAOM_Slow_to_SMB_Fast` | `part_aom_slow_to_smb_fast` |
| `vo_NConcentration` | `n_concentration` |

`Organic_Fertilizer_Parameters` and `Crop_Residue_Parameters` (same file) embed this struct via
`using base: Organic_Matter_Parameters` and inherit the renamed fields; neither adds fields of its
own with a `vo_` prefix.

## `monica::SoilTransport` -> `core.Soil_Transport` (odin/monica/core/soil_transport.odin)

| C++ (`soiltransport.h`) | Odin |
| --- | --- |
| `vq_Convection` | `convection` |
| `vq_DiffusionCoeff` | `diffusion_coeff` |
| `vq_Dispersion` | `dispersion` |
| `vq_DispersionCoeff` | `dispersion_coeff` |
| `vq_LeachingAtBoundary` | `leaching_at_boundary` |
| `vq_PercolationRate` | `percolation_rate` |
| `vq_PoreWaterVelocity` | `pore_water_velocity` |
| `vq_SoilNO3` | `soil_no3` |
| `vq_SoilNO3_aq` | `soil_no3_aq` |
| `vq_TimeStep` | `time_step` |
| `vq_TotalDispersion` | `total_dispersion` |

Not renamed this round: `soilColumn`, `modParams`, `siteParams`, `envParams`, `cropModParams`, `cropModule` (camelCase, no `vq_` prefix); `vc_NUptakeFromLayer` (mirrors Crop_Module, already renamed there) and `vs_SoilMineralNContent` (dead field) are also out of scope this round.

## `monica::SoilTemperature` -> `core.Soil_Temperature` (odin/monica/core/soil_temperature.odin)

| C++ (`soiltemperature.h`) | Odin |
| --- | --- |
| `B` | `b` |
| `dampingFactor` | `damping_factor` |
| `heatCapacity` | `heat_capacity` |
| `heatConductivity` | `heat_conductivity` |
| `heatConductivityMean` | `heat_conductivity_mean` |
| `heatFlow` | `heat_flow` |
| `matrixDiagonal` | `matrix_diagonal` |
| `matrixLowerTriangle` | `matrix_lower_triangle` |
| `matrixPrimaryDiagonal` | `matrix_primary_diagonal` |
| `matrixSecondaryDiagonal` | `matrix_secondary_diagonal` |
| `noOfSoilLayers` | `no_of_soil_layers` |
| `noOfTempLayers` | `no_of_temp_layers` |
| `soilColumn` | `soil_column` |
| `soilColumnBottomLayer` | `soil_column_bottom_layer` |
| `soilColumnGroundLayer` | `soil_column_ground_layer` |
| `soilSurfaceTemperature` | `soil_surface_temperature` |
| `soilTemperature` | `soil_temperature` |
| `V` | `v` |
| `volumeMatrix` | `volume_matrix` |
| `volumeMatrixOld` | `volume_matrix_old` |

Not renamed: `params`, `solution` (already lowercase). Full camelCase->snake_case pass, unlike the vX_-prefix structs above.

## `monica::MonicaModel` -> `core.Monica_Model` (odin/monica/core/monica_model.odin)

| C++ (`monica-model.h`) | Odin |
| --- | --- |
| `clearCropUponNextDay` | `clear_crop_upon_next_day` |
| `climateData` | `climate_data` |
| `cropPs` | `crop_ps` |
| `cultivationMethodCount` | `cultivation_method_count` |
| `currentCropModule` | `current_crop_module` |
| `currentEvents` | `current_events` |
| `currentStepDate` | `current_step_date` |
| `dailySumFertiliser` | `daily_sum_fertiliser` |
| `dailySumIrrigationWater` | `daily_sum_irrigation_water` |
| `dailySumOrganicFertilizerDM` | `daily_sum_organic_fertilizer_dm` |
| `dailySumOrgFertiliser` | `daily_sum_org_fertiliser` |
| `envPs` | `env_ps` |
| `groundwaterInformation` | `groundwater_information` |
| `humusBalanceCarryOver` | `humus_balance_carry_over` |
| `optCarbonExportedResidues` | `opt_carbon_exported_residues` |
| `optCarbonReturnedResidues` | `opt_carbon_returned_residues` |
| `p_accuHeatStress` | `p_accu_heat_stress` |
| `p_accuNStress` | `p_accu_n_stress` |
| `p_accuOxygenStress` | `p_accu_oxygen_stress` |
| `p_accuWaterStress` | `p_accu_water_stress` |
| `p_daysWithCrop` | `p_days_with_crop` |
| `previousDaysEvents` | `previous_days_events` |
| `simPs` | `sim_ps` |
| `sitePs` | `site_ps` |
| `soilColumn` | `soil_column` |
| `soilMoisture` | `soil_moisture` |
| `soilOrganic` | `soil_organic` |
| `soilTemperature` | `soil_temperature` |
| `soilTransport` | `soil_transport` |
| `sumFertiliser` | `sum_fertiliser` |
| `sumOrganicFertilizerDM` | `sum_organic_fertilizer_dm` |
| `sumOrgFertiliser` | `sum_org_fertiliser` |
| `vs_GroundwaterDepth` | `groundwater_depth_m` |
| `vw_AtmosphericCO2Concentration` | `atmospheric_co2_concentration` |
| `vw_AtmosphericO3Concentration` | `atmospheric_o3_concentration` |

Full camelCase->snake_case pass, no prefix stripped (p_/vw_/vs_ prefixes kept, rest snake_cased) -
except these three, whose `vs_`/`vw_` prefix was dropped (and `GroundwaterDepth` got a `_m` unit
suffix) in a later rename pass that updated `monica_model.odin` but missed the `AtmCO2`/`AtmO3`/
`Groundw` rows in `output_paths.odin`, which still had the intermediate `vw_`/`vs_`-prefixed names
and so failed `output_paths_test.odin`'s `test_every_alias_compiles` until fixed.

## `monica::SpeciesParameters` -> `params.Species_Parameters` (odin/monica/params/crop_parameters.odin)

| C++ (`monica-parameters.h`) | Odin |
| --- | --- |
| `dormancyEndDoy` | `dormancy_end_doy` |
| `dormancyStartDoy` | `dormancy_start_doy` |
| `pc_AbovegroundOrgan` | `aboveground_organ` |
| `pc_AssimilateReallocation` | `assimilate_reallocation` |
| `pc_BaseTemperature` | `base_temperature` |
| `pc_CarboxylationPathway` | `carboxylation_pathway` |
| `pc_CriticalOxygenContent` | `critical_oxygen_content` |
| `pc_CuttingDelayDays` | `cutting_delay_days` |
| `pc_DefaultRadiationUseEfficiency` | `default_radiation_use_efficiency` |
| `pc_DevelopmentAccelerationByNitrogenStress` | `development_acceleration_by_nitrogen_stress` |
| `pc_DroughtImpactOnFertilityFactor` | `drought_impact_on_fertility_factor` |
| `pc_FieldConditionModifier` | `field_condition_modifier` |
| `pc_InitialKcFactor` | `initial_kc_factor` |
| `pc_InitialOrganBiomass` | `initial_organ_biomass` |
| `pc_InitialRootingDepth` | `initial_rooting_depth` |
| `pc_LimitingTemperatureHeatStress` | `limiting_temperature_heat_stress` |
| `pc_LuxuryNCoeff` | `luxury_n_coeff` |
| `pc_MaxCropDiameter` | `max_crop_diameter` |
| `pc_MaximumTemperatureForAssimilation` | `maximum_temperature_for_assimilation` |
| `pc_MaxNUptakeParam` | `max_n_uptake_param` |
| `pc_MinimumNConcentration` | `minimum_n_concentration` |
| `pc_MinimumTemperatureForAssimilation` | `minimum_temperature_for_assimilation` |
| `pc_MinimumTemperatureRootGrowth` | `minimum_temperature_root_growth` |
| `pc_NConcentrationAbovegroundBiomass` | `n_concentration_aboveground_biomass` |
| `pc_NConcentrationB0` | `n_concentration_b0` |
| `pc_NConcentrationPN` | `n_concentration_pn` |
| `pc_NConcentrationRoot` | `n_concentration_root` |
| `pc_OptimumTemperatureForAssimilation` | `optimum_temperature_for_assimilation` |
| `pc_OrganGrowthRespiration` | `organ_growth_respiration` |
| `pc_OrganMaintenanceRespiration` | `organ_maintenance_respiration` |
| `pc_PartBiologicalNFixation` | `part_biological_n_fixation` |
| `pc_PlantDensity` | `plant_density` |
| `pc_RootDistributionParam` | `root_distribution_param` |
| `pc_RootFormFactor` | `root_form_factor` |
| `pc_RootGrowthLag` | `root_growth_lag` |
| `pc_RootPenetrationRate` | `root_penetration_rate` |
| `pc_SamplingDepth` | `sampling_depth` |
| `pc_SpeciesId` | `species_id` |
| `pc_SpecificRootLength` | `specific_root_length` |
| `pc_StageAfterCut` | `stage_after_cut` |
| `pc_StageAtMaxDiameter` | `stage_at_max_diameter` |
| `pc_StageAtMaxHeight` | `stage_at_max_height` |
| `pc_StageMaxRootNConcentration` | `stage_max_root_n_concentration` |
| `pc_StageMobilFromStorageCoeff` | `stage_mobil_from_storage_coeff` |
| `pc_StorageOrgan` | `storage_organ` |
| `pc_TargetN30` | `target_n30` |
| `pc_TargetNSamplingDepth` | `target_n_sampling_depth` |
| `pc_TransitionStageLeafExp` | `transition_stage_leaf_exp` |

Not renamed: `EF_MONO`, `EF_MONOS`, `EF_ISO`, `VCMAX25`, `AEKC`, `AEKO`, `AEVC`, `KC25`, `KO25` (ALL-CAPS literature symbols, neither `pc_`-prefixed nor camelCase - out of scope).

## `monica::CultivarParameters` -> `params.Cultivar_Parameters` (odin/monica/params/crop_parameters.odin)

| C++ (`monica-parameters.h`) | Odin |
| --- | --- |
| `pc_AssimilatePartitioningCoeff` | `assimilate_partitioning_coeff` |
| `pc_BaseDaylength` | `base_daylength` |
| `pc_BeginSensitivePhaseHeatStress` | `begin_sensitive_phase_heat_stress` |
| `pc_CriticalTemperatureHeatStress` | `critical_temperature_heat_stress` |
| `pc_CropHeightP1` | `crop_height_p1` |
| `pc_CropHeightP2` | `crop_height_p2` |
| `pc_CropSpecificMaxRootingDepth` | `crop_specific_max_rooting_depth` |
| `pc_CultivarId` | `cultivar_id` |
| `pc_DaylengthRequirement` | `daylength_requirement` |
| `pc_Description` | `description` |
| `pc_DroughtStressThreshold` | `drought_stress_threshold` |
| `pc_EarlyRefLeafExp` | `early_ref_leaf_exp` |
| `pc_EndSensitivePhaseHeatStress` | `end_sensitive_phase_heat_stress` |
| `pc_FrostDehardening` | `frost_dehardening` |
| `pc_FrostHardening` | `frost_hardening` |
| `pc_HeatSumIrrigationEnd` | `heat_sum_irrigation_end` |
| `pc_HeatSumIrrigationStart` | `heat_sum_irrigation_start` |
| `pc_LatestHarvestDoy` | `latest_harvest_doy` |
| `pc_LightExtinctionCoefficient` | `light_extinction_coefficient` |
| `pc_LowTemperatureExposure` | `low_temperature_exposure` |
| `pc_LT50cultivar` | `lt50cultivar` |
| `pc_MaxAssimilationRate` | `max_assimilation_rate` |
| `pc_MaxCropHeight` | `max_crop_height` |
| `pc_MaxTempDev_WE` | `max_temp_dev_we` |
| `pc_MinTempDev_WE` | `min_temp_dev_we` |
| `pc_OptimumTemperature` | `optimum_temperature` |
| `pc_OptTempDev_WE` | `opt_temp_dev_we` |
| `pc_OrganIdsForCutting` | `organ_ids_for_cutting` |
| `pc_OrganIdsForPrimaryYield` | `organ_ids_for_primary_yield` |
| `pc_OrganIdsForSecondaryYield` | `organ_ids_for_secondary_yield` |
| `pc_OrganSenescenceRate` | `organ_senescence_rate` |
| `pc_Perennial` | `perennial` |
| `pc_RefLeafExp` | `ref_leaf_exp` |
| `pc_ResidueNRatio` | `residue_n_ratio` |
| `pc_RespiratoryStress` | `respiratory_stress` |
| `pc_SpecificLeafArea` | `specific_leaf_area` |
| `pc_StageKcFactor` | `stage_kc_factor` |
| `pc_StageTemperatureSum` | `stage_temperature_sum` |
| `pc_VernalisationRequirement` | `vernalisation_requirement` |
| `winterCrop` | `winter_crop` |

## `monica::MineralFertilizerParameters` -> `params.Mineral_Fertilizer_Parameters` (odin/monica/params/site_sim_parameters.odin)

| C++ (`monica-parameters.h`) | Odin |
| --- | --- |
| `vo_Carbamid` | `carbamid` |
| `vo_NH4` | `nh4` |
| `vo_NO3` | `no3` |

## `monica::NMinApplicationParameters` -> `params.NMin_Application_Parameters` (odin/monica/params/site_sim_parameters.odin)

| C++ (`monica-parameters.h`) | Odin |
| --- | --- |
| `delayInDays` | `delay_in_days` |

## `monica::IrrigationParameters` -> `params.Irrigation_Parameters` (odin/monica/params/site_sim_parameters.odin)

| C++ (`monica-parameters.h`) | Odin |
| --- | --- |
| `nitrateConcentration` | `nitrate_concentration` |
| `sulfateConcentration` | `sulfate_concentration` |
| `isDripIrrigation` | `is_drip_irrigation` |

`fw` was already a bare, unprefixed short name - left as-is.

## `monica::AutomaticIrrigationParameters` -> `params.Automatic_Irrigation_Parameters` (odin/monica/params/site_sim_parameters.odin)

| C++ (`monica-parameters.h`) | Odin |
| --- | --- |
| `startDate` | `start_date` |
| `endDate` | `end_date` |
| `percentNFC` | `percent_nfc` |
| `criticalMoistureDepthM` | `critical_moisture_depth_m` |
| `minDaysBetweenIrrigationEvents` | `min_days_between_irrigation_events` |

`amount` and `threshold` were already bare, unprefixed short names - left as-is. `base` (the
embedded `Irrigation_Parameters`) is untouched.

## `monica::MeasuredGroundwaterTableInformation` -> `params.Measured_Groundwater_Table_Information` (odin/monica/params/site_sim_parameters.odin)

| C++ (`monica-parameters.h`) | Odin |
| --- | --- |
| `groundwaterInformationAvailable` | `groundwater_information_available` |
| `groundwaterInfo` | `groundwater_info` |

## `monica::SiteParameters` -> `params.Site_Parameters` (odin/monica/params/site_sim_parameters.odin)

| C++ (`monica-parameters.h`) | Odin |
| --- | --- |
| `vs_Latitude` | `latitude` |
| `vs_Slope` | `slope` |
| `vs_HeightNN` | `height_nn` |
| `vs_GroundwaterDepth` | `groundwater_depth` |
| `vs_Soil_CN_Ratio` | `soil_cn_ratio` |
| `vs_DrainageCoeff` | `drainage_coeff` |
| `vq_NDeposition` | `n_deposition` |
| `vs_MaxEffectiveRootingDepth` | `max_effective_rooting_depth` |
| `vs_ImpenetrableLayerDepth` | `impenetrable_layer_depth` |
| `vs_SoilSpecificHumusBalanceCorrection` | `soil_specific_humus_balance_correction` |
| `bareSoilKcFactor` | `bare_soil_kc_factor` |
| `numberOfLayers` | `number_of_layers` |
| `layerThickness` | `layer_thickness` |
| `vs_SoilParameters` | `soil_parameters` |
| `initSoilProfileSpec` | `init_soil_profile_spec` |
| `pwpFcSatFunction` | `pwp_fc_sat_function` |

`vs_Soil_CN_Ratio` has zero genuine outside uses - every apparent external match (`monica/core/
soil_column.odin`, `monica/soil/soil_parameters.odin`) actually belongs to the unrelated
`soil.Soil_Parameters` struct, which coincidentally has its own field of the identical C++ name.
Confirmed by reading both call sites' receiver types before touching anything outside this file.

## `monica::SimulationParameters` -> `params.Simulation_Parameters` (odin/monica/params/site_sim_parameters.odin)

| C++ (`monica-parameters.h`) | Odin |
| --- | --- |
| `startDate` | `start_date` |
| `endDate` | `end_date` |
| `pc_NitrogenResponseOn` | `nitrogen_response_on` |
| `pc_WaterDeficitResponseOn` | `water_deficit_response_on` |
| `pc_EmergenceFloodingControlOn` | `emergence_flooding_control_on` |
| `pc_EmergenceMoistureControlOn` | `emergence_moisture_control_on` |
| `pc_FrostKillOn` | `frost_kill_on` |
| `p_UseAutomaticIrrigation` | `use_automatic_irrigation` |
| `p_AutoIrrigationParams` | `auto_irrigation_params` |
| `p_UseNMinMineralFertilisingMethod` | `use_n_min_mineral_fertilising_method` |
| `p_NMinFertiliserPartition` | `n_min_fertiliser_partition` |
| `p_NMinUserParams` | `n_min_user_params` |
| `p_UseSecondaryYields` | `use_secondary_yields` |
| `p_UseAutomaticHarvestTrigger` | `use_automatic_harvest_trigger` |
| `p_NumberOfLayers` | `number_of_layers` |
| `p_LayerThickness` | `layer_thickness` |
| `p_StartPVIndex` | `start_pv_index` |
| `p_JulianDayAutomaticFertilising` | `julian_day_automatic_fertilising` |
| `serializeMonicaStateAtEnd` | `serialize_monica_state_at_end` |
| `serializeMonicaStateAtEndToJson` | `serialize_monica_state_at_end_to_json` |
| `pathToSerializationAtEndFile` | `path_to_serialization_at_end_file` |
| `loadSerializedMonicaStateAtStart` | `load_serialized_monica_state_at_start` |
| `deserializedMonicaStateFromJson` | `deserialized_monica_state_from_json` |
| `pathToLoadSerializationFile` | `path_to_load_serialization_file` |
| `noOfPreviousDaysSerializedClimateData` | `no_of_previous_days_serialized_climate_data` |
| `dualKcMethod` | `dual_kc_method` |

Several JSON keys are textually identical to the C++ field name (`"startDate"`,
`"noOfPreviousDaysSerializedClimateData"`, `"serializeMonicaStateAtEnd"` and others across the
structs above) - those string literals are untouched, only the Odin identifiers changed.

## `monica::CropModuleParameters` -> `params.Crop_Module_Parameters` (odin/monica/params/site_sim_parameters.odin)

| C++ (`monica-parameters.h`) | Odin |
| --- | --- |
| `pc_CanopyReflectionCoefficient` | `canopy_reflection_coefficient` |
| `pc_ReferenceMaxAssimilationRate` | `reference_max_assimilation_rate` |
| `pc_ReferenceLeafAreaIndex` | `reference_leaf_area_index` |
| `pc_MaintenanceRespirationParameter1` | `maintenance_respiration_parameter1` |
| `pc_MaintenanceRespirationParameter2` | `maintenance_respiration_parameter2` |
| `pc_MinimumNConcentrationRoot` | `minimum_n_concentration_root` |
| `pc_MinimumAvailableN` | `minimum_available_n` |
| `pc_ReferenceAlbedo` | `reference_albedo` |
| `pc_StomataConductanceAlpha` | `stomata_conductance_alpha` |
| `pc_SaturationBeta` | `saturation_beta` |
| `pc_GrowthRespirationRedux` | `growth_respiration_redux` |
| `pc_MaxCropNDemand` | `max_crop_n_demand` |
| `pc_GrowthRespirationParameter1` | `growth_respiration_parameter1` |
| `pc_GrowthRespirationParameter2` | `growth_respiration_parameter2` |
| `pc_Tortuosity` | `tortuosity` |
| `pc_AdjustRootDepthForSoilProps` | `adjust_root_depth_for_soil_props` |
| `pc_TimeUnderAnoxiaThreshold` | `time_under_anoxia_threshold` |
| `isIntercropping` | `is_intercropping` |
| `sequentialWaterUse` | `sequential_water_use` |
| `twoWaySync` | `two_way_sync` |
| `pc_intercropping_k_s` | `intercropping_k_s` |
| `pc_intercropping_k_t` | `intercropping_k_t` |
| `pc_intercropping_phRedux` | `intercropping_ph_redux` |
| `pc_intercropping_dvs_phr` | `intercropping_dvs_phr` |
| `pc_intercropping_autoPhRedux` | `intercropping_auto_ph_redux` |
| `pc_intercropping_reader_sr` | `intercropping_reader_sr` |
| `pc_intercropping_writer_sr` | `intercropping_writer_sr` |

The seven `__enable_*__`/`__disable_*__` flags are left as-is, matching the codebase-wide dunder-flag
convention (see the `Soil_Organic_Module_Parameters` note above). `EnvironmentParameters` and
`CentralParameterProvider` in this same file were already fully snake_cased in earlier work and are
not part of this pass.

Locals and parameters in `crop_module.odin`, `soil_moisture.odin` and `soil_transport.odin` that
used to cache one of these fields under its original C++ name have since been snake_cased with the
prefixes dropped (`pc_Tortuosity` -> `tortuosity`, `vm_SoilMoisture_m3` -> `soil_moisture_m3`), and
aliases that were only read a few times outside loops were inlined (`vs_Latitude` ->
`cm.site_params.latitude`).

## `monica::CultivationMethod` -> `run.Cultivation_Method` (odin/monica/run/cultivation_method.odin)

| C++ (`cultivation-method.h`) | Odin |
| --- | --- |
| `allWorksteps` | `all_worksteps` |
| `allAbsWorksteps` | `all_abs_worksteps` |
| `unfinishedDynamicWorksteps` | `unfinished_dynamic_worksteps` |
| `customId` | `custom_id` |
| `canBeSkipped` | `can_be_skipped` |
| `isCoverCrop` | `is_cover_crop` |

## `monica::Workstep` -> `run.Workstep` (odin/monica/run/workstep.odin)

| C++ (`workstep.h`) | Odin |
| --- | --- |
| `absDate` | `abs_date` |
| `applyNoOfDaysAfterEvent` | `apply_no_of_days_after_event` |
| `afterEvent` | `after_event` |
| `daysAfterEventCount` | `days_after_event_count` |
| `daysAfterEventCountActivated` | `days_after_event_count_activated` |
| `isActive` | `is_active` |
| `runAtStartOfDay` | `run_at_start_of_day` |

## `monica::SowingData` -> `run.Sowing_Data` (odin/monica/run/workstep.odin)

| C++ (`worksteps/sowing.h`) | Odin |
| --- | --- |
| `isValid` | `is_valid` |
| `sowingDate` | `sowing_date` |
| `harvestDate` | `harvest_date` |
| `isPerennialCrop` | `is_perennial_crop` |
| `cropParams` | `crop_params` |
| `separatePerennialCropParams` | `separate_perennial_crop_params` |
| `residueParams` | `residue_params` |
| `plantDensity` | `plant_density` |
| `initialKcb` | `initial_kcb` |

## `monica::TransplantData` -> `run.Transplant_Data` (odin/monica/run/workstep.odin)

| C++ (`worksteps/transplant.h`) | Odin |
| --- | --- |
| `initialStage` | `initial_stage` |
| `initialGDD` | `initial_gdd` |
| `initRootMass` | `init_root_mass` |
| `initLeafMass` | `init_leaf_mass` |
| `initShootMass` | `init_shoot_mass` |
| `initLAI` | `init_lai` |
| `postTransplantDelay` | `post_transplant_delay` |

## `monica::AutomaticSowingData` -> `run.Automatic_Sowing_Data` (odin/monica/run/workstep.odin)

| C++ (`worksteps/automatic-sowing.h`) | Odin |
| --- | --- |
| `absEarliestDate` | `abs_earliest_date` |
| `earliestDate` | `earliest_date` |
| `latestDate` | `latest_date` |
| `absLatestDate` | `abs_latest_date` |
| `minTempThreshold` | `min_temp_threshold` |
| `daysInTempWindow` | `days_in_temp_window` |
| `minPercentASW` | `min_percent_asw` |
| `maxPercentASW` | `max_percent_asw` |
| `max3dayPrecipSum` | `max_3day_precip_sum` |
| `maxCurrentDayPrecipSum` | `max_current_day_precip_sum` |
| `tempSumAboveBaseTemp` | `temp_sum_above_base_temp` |
| `baseTemp` | `base_temp` |
| `checkForSoilTemperature` | `check_for_soil_temperature` |
| `soilDepthForAveraging` | `soil_depth_for_averaging` |
| `daysInSoilTempWindow` | `days_in_soil_temp_window` |
| `sowingIfAboveAvgSoilTemp` | `sowing_if_above_avg_soil_temp` |
| `getAvgSoilTemps` | `get_avg_soil_temps` |
| `inSowingRange` | `in_sowing_range` |
| `cropSeeded` | `crop_seeded` |

## `monica::HarvestData` -> `run.Harvest_Data` (odin/monica/run/workstep.odin)

| C++ (`worksteps/harvest.h`) | Odin |
| --- | --- |
| `optCarbMgmtData` | `opt_carb_mgmt_data` |
| `incorporateIntoLayerNo` | `incorporate_into_layer_no` |

## `monica::AutomaticHarvestData` -> `run.Automatic_Harvest_Data` (odin/monica/run/workstep.odin)

| C++ (`worksteps/automatic-harvest.h`) | Odin |
| --- | --- |
| `harvestTime` | `harvest_time` |
| `latestDate` | `latest_date` |
| `absLatestDate` | `abs_latest_date` |
| `minPercentASW` | `min_percent_asw` |
| `maxPercentASW` | `max_percent_asw` |
| `max3dayPrecipSum` | `max_3day_precip_sum` |
| `maxCurrentDayPrecipSum` | `max_current_day_precip_sum` |
| `cropHarvested` | `crop_harvested` |

## `monica::CuttingData` -> `run.Cutting_Data` (odin/monica/run/workstep.odin)

| C++ (`worksteps/cutting.h`) | Odin |
| --- | --- |
| `organId2cuttingSpec` | `organ_id_2_cutting_spec` |
| `organId2biomAfterCutting` | `organ_id_2_biom_after_cutting` |
| `organId2exportFraction` | `organ_id_2_export_fraction` |
| `cutMaxAssimilationRateFraction` | `cut_max_assimilation_rate_fraction` |

## `monica::NDemandFertilizationData` -> `run.N_Demand_Fertilization_Data` (odin/monica/run/workstep.odin)

| C++ (`worksteps/n-demand-fertilization.h`) | Odin |
| --- | --- |
| `initialDate` | `initial_date` |
| `Ndemand` | `n_demand` |
| `appliedFertilizer` | `applied_fertilizer` |

## `monica::OrganicFertilizationData` -> `run.Organic_Fertilization_Data` (odin/monica/run/workstep.odin)

| C++ (`worksteps/organic-fertilization.h`) | Odin |
| --- | --- |
| `incorporateIntoLayerNo` | `incorporate_into_layer_no` |

## `monica::AutomaticIrrigationData` -> `run.Automatic_Irrigation_Data` (odin/monica/run/workstep.odin)

| C++ (`worksteps/automatic-irrigation.h`) | Odin |
| --- | --- |
| `absStartDate` | `abs_start_date` |
| `absEndDate` | `abs_end_date` |
| `irrigateCrop` | `irrigate_crop` |
| `startStage` | `start_stage` |
| `endStage` | `end_stage` |
| `cropPlanted` | `crop_planted` |

## `monica::SetValueData` -> `run.Set_Value_Data` (odin/monica/run/workstep.odin)

| C++ (`worksteps/set-value.h`) | Odin |
| --- | --- |
| `getValue` | `get_value` |

`Set_Value_Get_Value.sourceOid` -> `source_oid` has no C++ counterpart (the C++ `getValue` is a
closure; see `Set_Value_Get_Value` in workstep.odin). Fields inherited via `using` (`Transplant_Data`/
`Automatic_Sowing_Data` -> `Sowing_Data`, `Automatic_Harvest_Data` -> `Harvest_Data`) are listed only
under the struct that declares them. JSON keys (`"initialKcb"`, `"runAtStartOfDay"`, ...) are unchanged.

## `monica::CropParameters` -> `params.Crop_Parameters` (odin/monica/params/crop_parameters.odin)

| C++ (`crop-parameters.h`) | Odin |
| --- | --- |
| `speciesParams` | `species_params` |
| `cultivarParams` | `cultivar_params` |

`__enable_vernalisation_factor_fix__` keeps its dunder spelling, like the other `__enable_*__` flags.
`Species_Parameters` and `Cultivar_Parameters` were already snake_cased (tables above).

## `monica::SoilColumn` -> `core.Soil_Column` (odin/monica/core/soil_column.odin)

Only the fields that did not already match their snake_cased C++ name are listed.

| C++ (`soilcolumn.h`) | Odin |
| --- | --- |
| `cropModule` | `crop_module` |
| `vq_CropNUptake` | `crop_n_uptake` |
| `_delayedNMinApplications` | `delayed_n_min_applications` |

## `monica::SoilMoisture` -> `core.Soil_Moisture` (odin/monica/core/soil_moisture.odin)

Ported in snake_case from the start: every other field is its C++ name with
the `vm_`/`vc_`/`vs_` prefix dropped and snake_cased (`vm_ActualEvaporation`
-> `actual_evaporation`), which `tools/gen_output_aliases.py` derives on its
own. Only the exceptions are listed. `vm_SurfaceWaterStorage`,
`vm_FluxAtLowerBoundary` and `vm_WaterFlux` moved to `Soil_Column` /
`Soil_Layer` instead (see that script's `MOVED`).

| C++ (`soilmoisture.h`) | Odin |
| --- | --- |
| `vc_PercentageSoilCoverage` | `soil_coverage_percent` |

## `monica::RunMonica` -> `capnp.Run_Monica` (odin/monica/capnp/run_monica_capnp.odin)

| C++ | Odin |
| --- | --- |
| `_startedServerInDebugMode` | `started_server_in_debug_mode` |
| `_id` | `id` |
| `_name` | `name` |
| `_description` | `description` |
| `pathToSoilDir` | `path_to_soil_dir` |

## (port-only) -> `capnp.Run_Call` (odin/monica/capnp/run_monica_capnp.odin)

| previous Odin name | Odin |
| --- | --- |
| `envJson` | `env_json` |
| `dataT` | `data_t` |

## `HarvestData::Spec::Value` -> `core.Harvest_Spec_Value` (odin/monica/core/monica_model.odin)

| C++ | Odin |
| --- | --- |
| `exportPercentage` | `export_percentage` |

## `HarvestData::Spec` -> `core.Harvest_Spec` (odin/monica/core/monica_model.odin)

| C++ | Odin |
| --- | --- |
| `organ2specVal` | `organ_to_spec_val` |

## `HarvestData::OptCarbonManagementData` -> `core.Harvest_Opt_Carbon_Management_Data` (odin/monica/core/monica_model.odin)

| C++ | Odin |
| --- | --- |
| `optCarbonConservation` | `opt_carbon_conservation` |
| `cropImpactOnHumusBalance` | `crop_impact_on_humus_balance` |
| `maxResidueRecoverFraction` | `max_residue_recover_fraction` |
| `cropUsage` | `crop_usage` |
| `residueHeq` | `residue_heq` |
| `organicFertilizerHeq` | `organic_fertilizer_heq` |

## `O3impact::O3_impact_params` -> `core.O3_Impact_Params` (odin/monica/core/o3_impact.odin)

| C++ | Odin |
| --- | --- |
| `Fshape_stomatal` | `fshape_stomatal` |

## `O3impact::O3_impact_in` -> `core.O3_Impact_In` (odin/monica/core/o3_impact.odin)

| C++ | Odin |
| --- | --- |
| `FC` | `fc` |
| `WP` | `wp` |
| `SWC` | `swc` |
| `ET0` | `et0` |
| `O3a` | `o3a` |
| `GDD_flo` | `gdd_flo` |
| `GDD_mat` | `gdd_mat` |
| `fO3s_d_prev` | `f_o3s_d_prev` |
| `sum_O3_up` | `sum_o3_up` |

## `O3impact::O3_impact_out` -> `core.O3_Impact_Out` (odin/monica/core/o3_impact.odin)

| C++ | Odin |
| --- | --- |
| `hourly_O3_up` | `hourly_o3_up` |
| `fO3s_d` | `f_o3s_d` |
| `fO3l` | `f_o3l` |
| `fLS` | `f_ls` |
| `WS_st_clos` | `ws_st_clos` |

## `FvCB::FvCB_canopy_hourly_params` -> `core.Fvcb_Canopy_Hourly_Params` (odin/monica/core/photosynthesis_fvcb.odin)

| C++ | Odin |
| --- | --- |
| `Vcmax_25` | `vcmax_25` |

## `FvCB::FvCB_canopy_hourly_in` -> `core.Fvcb_Canopy_Hourly_In` (odin/monica/core/photosynthesis_fvcb.odin)

| C++ | Odin |
| --- | --- |
| `LAI` | `lai` |
| `VPD` | `vpd` |
| `Ca` | `ca` |

## `FvCB::FvCB_leaf_fraction` -> `core.Fvcb_Leaf_Fraction` (odin/monica/core/photosynthesis_fvcb.odin)

| C++ | Odin |
| --- | --- |
| `LAI` | `lai` |
| `vcMax` | `vc_max` |
| `jMax` | `j_max` |

## (port-only) -> `core.Fvcb_Lumped_Coeffs` (odin/monica/core/photosynthesis_fvcb.odin)

| previous Odin name | Odin |
| --- | --- |
| `Q` | `q` |

## `Voc::CPData` -> `core.Voc_Cp_Data` (odin/monica/core/voc_common.odin)

| C++ | Odin |
| --- | --- |
| `vcMax` | `vc_max` |
| `jMax` | `j_max` |

## `Voc::SpeciesData` -> `core.Voc_Species_Data` (odin/monica/core/voc_common.odin)

| C++ | Odin |
| --- | --- |
| `EF_MONOS` | `ef_monos` |
| `EF_MONO` | `ef_mono` |
| `EF_ISO` | `ef_iso` |
| `THETA` | `theta` |
| `FAGE` | `fage` |
| `CT_IS` | `ct_is` |
| `CT_MT` | `ct_mt` |
| `HA_IS` | `ha_is` |
| `HA_MT` | `ha_mt` |
| `DS_IS` | `ds_is` |
| `DS_MT` | `ds_mt` |
| `HD_IS` | `hd_is` |
| `HD_MT` | `hd_mt` |
| `HDJ` | `hdj` |
| `SDJ` | `sdj` |
| `KC25` | `kc25` |
| `KO25` | `ko25` |
| `VCMAX25` | `vcmax25` |
| `QJVC` | `qjvc` |
| `AEKC` | `aekc` |
| `AEKO` | `aeko` |
| `AEJM` | `aejm` |
| `AEVC` | `aevc` |
| `SLAMIN` | `slamin` |
| `SCALE_I` | `scale_i` |
| `SCALE_M` | `scale_m` |
| `mFol` | `m_fol` |

## `Voc::MicroClimateData` -> `core.Voc_Micro_Climate_Data` (odin/monica/core/voc_common.odin)

| C++ | Odin |
| --- | --- |
| `tFol` | `t_fol` |
| `tFol24` | `t_fol24` |
| `tFol240` | `t_fol240` |

## `Voc::Emissions` -> `core.Voc_Emissions` (odin/monica/core/voc_common.odin)

| C++ | Odin |
| --- | --- |
| `speciesId_2_isoprene_emission` | `species_id_to_isoprene_emission` |
| `speciesId_2_monoterpene_emission` | `species_id_to_monoterpene_emission` |

## `Voc::foliage_t` -> `core.Voc_Foliage_T` (odin/monica/core/voc_common.odin)

| C++ | Odin |
| --- | --- |
| `tempK` | `temp_k` |
| `tempK24` | `temp_k24` |
| `tempK240` | `temp_k240` |

## `monica::OId` -> `io.OId` (odin/monica/io/output.odin)

| C++ | Odin |
| --- | --- |
| `displayName` | `display_name` |
| `jsonInput` | `json_input` |
| `layerAggOp` | `layer_agg_op` |
| `timeAggOp` | `time_agg_op` |
| `fromLayer` | `from_layer` |
| `toLayer` | `to_layer` |
| `roundToDigits` | `round_to_digits` |
| `castTo` | `cast_to` |

## `Output::Data` -> `io.Output_Data` (odin/monica/io/output.odin)

| C++ | Odin |
| --- | --- |
| `origSpec` | `orig_spec` |
| `outputIds` | `output_ids` |
| `resultsObj` | `results_obj` |

## `monica::Output` -> `io.Output` (odin/monica/io/output.odin)

| C++ | Odin |
| --- | --- |
| `customId` | `custom_id` |

## `monica::SticsParameters` -> `params.Stics_Parameters` (odin/monica/params/module_parameters.odin)

| C++ | Odin |
| --- | --- |
| `pHminnit` | `ph_min_nit` |
| `pHmaxnit` | `ph_max_nit` |
| `pHminden` | `ph_min_den` |
| `pHmaxden` | `ph_max_den` |
| `Kd` | `kd` |
| `Kamm` | `kamm` |

## `monica::YieldComponent` -> `params.Yield_Component` (odin/monica/params/organic_parameters.odin)

| C++ | Odin |
| --- | --- |
| `organId` | `organ_id` |
| `yieldPercentage` | `yield_percentage` |
| `yieldDryMatter` | `yield_dry_matter` |

## `monica::AutomaticHarvestParameters` -> `params.Automatic_Harvest_Parameters` (odin/monica/params/organic_parameters.odin)

| C++ | Odin |
| --- | --- |
| `_harvestTime` | `harvest_time` |
| `_latestHarvestDOY` | `latest_harvest_doy` |

## `monica::NMinCropParameters` -> `params.NMin_Crop_Parameters` (odin/monica/params/organic_parameters.odin)

| C++ | Odin |
| --- | --- |
| `samplingDepth` | `sampling_depth` |
| `nTarget` | `n_target` |
| `nTarget30` | `n_target30` |

## `monica::CropResidueParameters` -> `params.Crop_Residue_Parameters` (odin/monica/params/organic_parameters.odin)

| C++ | Odin |
| --- | --- |
| `residueType` | `residue_type` |

## `monica::EnvironmentParameters` -> `params.Environment_Parameters` (odin/monica/params/site_sim_parameters.odin)

| C++ | Odin |
| --- | --- |
| `atmospheric_CO2` | `atmospheric_co2` |
| `atmospheric_CO2s` | `atmospheric_co2_s` |
| `atmospheric_O3` | `atmospheric_o3` |
| `atmospheric_O3s` | `atmospheric_o3s` |

## `monica::CropRotation` -> `run.Crop_Rotation` (odin/monica/run/run_monica.odin)

| C++ | Odin |
| --- | --- |
| `cropRotation` | `crop_rotation` |

## `monica::Env` -> `run.Env` (odin/monica/run/run_monica.odin)

| C++ | Odin |
| --- | --- |
| `climateData` | `climate_data` |
| `cropRotation` | `crop_rotation` |
| `cropRotations` | `crop_rotations` |
| `customId` | `custom_id` |
| `debugMode` | `debug_mode` |

## (port-only) -> `run.Spec_Expr` (odin/monica/run/run_monica.odin)

| previous Odin name | Odin |
| --- | --- |
| `eventName` | `event_name` |

## `monica::Spec` -> `run.Spec` (odin/monica/run/run_monica.odin)

| C++ | Odin |
| --- | --- |
| `origSpec` | `orig_spec` |

## `monica::StoreData` -> `run.Store_Data` (odin/monica/run/run_monica.odin)

| C++ | Odin |
| --- | --- |
| `withinEventStartEndRange` | `within_event_start_end_range` |
| `withinEventFromToRange` | `within_event_from_to_range` |
| `outputIds` | `output_ids` |
| `intermediateResults` | `intermediate_results` |
| `resultsObj` | `results_obj` |

## `Soil::SoilParameters` -> `soil.Soil_Parameters` (odin/monica/soil/soil_parameters.odin)

| C++ | Odin |
| --- | --- |
| `vs_SoilSandContent` | `soil_sand_content` |
| `vs_SoilClayContent` | `soil_clay_content` |
| `vs_SoilpH` | `soil_ph` |
| `vs_SoilStoneContent` | `soil_stone_content` |
| `vs_Lambda` | `lambda` |
| `vs_FieldCapacity` | `field_capacity` |
| `vs_Saturation` | `saturation` |
| `vs_PermanentWiltingPoint` | `permanent_wilting_point` |
| `vs_SoilTexture` | `soil_texture` |
| `vs_SoilAmmonium` | `soil_ammonium` |
| `vs_SoilNitrate` | `soil_nitrate` |
| `vs_Soil_CN_Ratio` | `soil_cn_ratio` |
| `vs_SoilMoisturePercentFC` | `soil_moisture_percent_fc` |
| `_vs_SoilRawDensity` | `soil_raw_density` |
| `_vs_SoilBulkDensity` | `soil_bulk_density` |
| `_vs_SoilOrganicCarbon` | `soil_organic_carbon` |
| `_vs_SoilOrganicMatter` | `soil_organic_matter` |

## `Soil::VanGenuchtenParams` -> `soil.Van_Genuchten_Params` (odin/monica/soil/soil_pwp_fc_sat.odin)

| C++ | Odin |
| --- | --- |
| `thetaR` | `theta_r` |
| `thetaS` | `theta_s` |
| `volumetricWaterContentAtMatricHead` | `volumetric_water_content_at_matric_head` |

## `Climate::DataAccessor` -> `climate.Data_Accessor` (odin/support/climate/climate_common.odin)

| C++ | Odin |
| --- | --- |
| `_startDate` | `start_date` |
| `_endDate` | `end_date` |
| `_data` | `data` |
| `_acd2dataIndex` | `acd_to_data_index` |
| `_fromStep` | `from_step` |
| `_numberOfSteps` | `number_of_steps` |
| `_tamp` | `tamp` |
| `_tav` | `tav` |

## `Climate::CSVViaHeaderOptions` -> `climate.Csv_Via_Header_Options` (odin/support/climate/climate_file_io.odin)

| C++ | Odin |
| --- | --- |
| `startDate` | `start_date` |
| `endDate` | `end_date` |
| `noOfHeaderLines` | `no_of_header_lines` |
| `lineNoOfHeaderLine` | `line_no_of_header_line` |
| `lineNoOfDataStart` | `line_no_of_data_start` |
| `lineNoOfDataEnd` | `line_no_of_data_end` |
| `headerName2ACDName` | `header_name_to_acd_name` |
| `convertFn` | `convert_fn` |
| `datePattern` | `date_pattern` |

## `Tools::Date` -> `date.Date` (odin/support/date/date.odin)

The C++ keeps these as `_useLeapYears`/`_isRelative` and a DIM table pointer; the left column is
the port's previous name.

| previous Odin name | Odin |
| --- | --- |
| `_no_leap_years` | `no_leap_years` |
| `_common_year_dim` | `common_year_dim` |
| `_is_relative` | `is_relative` |
