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

Not renamed this round: `vo_AOM_Pool` (`vo_` prefix, not `vs_`).

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
