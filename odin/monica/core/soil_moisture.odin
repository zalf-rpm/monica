// Port of src/core/soilmoisture.{h,cpp}: SoilMoisture, make_soil_moisture,
// step, and every soilmoisture:: proc.
//
// Deliberate deviations, both following soil_temperature.odin's precedent of
// dropping the `monica: *MonicaModel` back-pointer (MonicaModel is phase 6 and
// doesn't exist yet):
//
//  1. No `monica` field. The C++ struct's `MonicaModel &monica` is read in two
//     shapes: (a) `sm->monica.currentCropModule.get()->X` at 10 call sites
//     across `step`/`evapotranspiration`, and (b) `sm->monica.simPs.X` /
//     `sm->monica.dailySumIrrigationWater` (2 sites). For (a): the C++ struct
//     *also* carries its own `CropModule *cropModule` field, kept in sync with
//     `monica.currentCropModule.get()` by monica-model.cpp at every plant/
//     harvest event (`soilMoisture->cropModule = currentCropModule.get()`) -
//     the two pointers are provably always equal while a crop is live. So
//     every `monica.currentCropModule`-mediated read below goes through the
//     `cropModule` field instead, which already exists as a genuine C++
//     struct member (unlike soil_temperature.odin's case, this isn't inventing
//     a new field, just routing through the one MONICA itself keeps
//     synchronised). For (b): `dualKcMethod`/`dailySumIrrigationWater` become
//     explicit parameters on `step`/`evapotranspiration`/
//     `dual_kc_precomputation`, the same move used for `p_timeStep` in
//     `make_soil_temperature`.
//  2. `cropModule` points at `Crop_Module`, a phase-5 stub (see
//     crop_module_stub.odin) carrying only the 10 fields this file reads.
//     Phase 5 replaces it with the real ~4,750-line struct; every field here
//     already has its final C++ name, so nothing calling into `cropModule.X`
//     needs to change.
//
// `siteParameters`/`envPs`/`cropPs` ARE kept as genuine fields (pointers, not
// values - Odin has no reference type): the C++ struct already stores these as
// direct `const X&` members, not read through `monica`, so there's no
// deviation to make there.
package core

import tl "../../support/tools"
import p "../params"
import "../soil"
import libc "core:c/libc"
import "core:fmt"

STOMATA_RESISTANCE :: 100 // FAO default value [s m-1]

// C++: struct monica::SoilMoisture
Soil_Moisture :: struct {
	evaporated_from_surface:      f64,
	soil_column:                  ^Soil_Column,
	site_params:                  ^p.Site_Parameters,
	mod_params:                   p.Soil_Moisture_Module_Parameters,
	env_params:                   ^p.Environment_Parameters,
	crop_mod_params:              ^p.Crop_Module_Parameters,
	no_of_soil_layers:            int,
	actual_evaporation:           f64,
	actual_evapotranspiration:    f64,
	actual_transpiration:         f64,
	available_water:              [dynamic]f64,
	capillary_water:              [dynamic]f64,
	capillary_water70:            [dynamic]f64,
	evaporation:                  [dynamic]f64,
	evapotranspiration:           [dynamic]f64,
	field_capacity_below_m3_m3:   f64,
	gravitational_water:          [dynamic]f64,
	groundwater_added:            f64,
	groundwater_table_layer:      int, // C++ size_t
	heat_conductivity:            [dynamic]f64,
	infiltration:                 f64,
	interception:                 f64,
	kc_factor:                    f64,
	lambda_below:                 f64,
	layer_thickness_m:            f64,
	leaching_depth_layer_idx:     int,
	net_precipitation_mm:         f64,
	last_wetting_was_rain:        bool,
	ke:                           f64,
	irrig_fw_event:               f64,
	irrig_is_drip_event:          bool,
	net_radiation:                f64,
	soil_coverage_percent:        f64,
	percolation_rate:             [dynamic]f64,
	reference_evapotranspiration: f64,
	residual_evapotranspiration:  [dynamic]f64,
	soil_moisture_below_m3_m3:    f64,
	// C++-quirk-preserving snapshot (see get_e_reducer_1): soil_column.layers
	// used to only be written back to at the end of the day, so getEReducer1's
	// direct soil_column read saw the PREVIOUS day's moisture, not today's
	// in-progress infiltration/percolation. Since infiltration/percolation/
	// capillary_rise now mutate soil_column.layers directly (no separate
	// scratch copy), this snapshot - taken once at the top of the day, before
	// infiltration - reproduces that lag explicitly.
	soil_moisture_day_start:      [dynamic]f64,
	soil_moisture_crit:           f64,
	soil_moisture_deficit:        f64,
	pore_volume_below_m3_m3:      f64,
	surface_run_off:              f64,
	sum_surface_run_off:          f64,
	total_water_removal:          f64,
	transpiration:                [dynamic]f64,
	water_flux_below:             f64,
	xsa_critical_soil_moisture:   f64,
	snow_component:               Snow_Component,
	frost_component:              Frost_Component,
	crop_module:                  ^Crop_Module,
}

// C++: kj::Own<SoilMoisture> monica::makeSoilMoisture(MonicaModel&, const
//        SoilMoistureModuleParameters&)
//
// Takes soil_column/site_parameters/env_ps/crop_ps/p_layer_thickness directly
// instead of a MonicaModel& - see the package comment. Returns by value,
// matching make_soil_column/make_soil_temperature's precedent. Inlines C++'s
// soilmoisture::initializeFromParams(SoilMoisture*), which has no other
// caller; p_layer_thickness stands in for the C++'s `mm.simPs.p_LayerThickness`
// - see the package comment.
make_soil_moisture :: proc(
	soil_column: ^Soil_Column,
	site_parameters: ^p.Site_Parameters,
	mod_params: p.Soil_Moisture_Module_Parameters,
	env_params: ^p.Environment_Parameters,
	crop_mod_params: ^p.Crop_Module_Parameters,
	layer_thickness: f64,
	allocator := context.allocator,
) -> Soil_Moisture {
	sm: Soil_Moisture
	sm.kc_factor = 0.6
	sm.irrig_fw_event = 1.0
	sm.reference_evapotranspiration = 6.0

	sm.soil_column = soil_column
	sm.site_params = site_parameters
	sm.mod_params = mod_params
	sm.env_params = env_params
	sm.crop_mod_params = crop_mod_params

	sc := sm.soil_column

	sm.no_of_soil_layers = len(sc.layers)
	no_of_mois_layers := sm.no_of_soil_layers + 1

	resize(&sm.available_water, no_of_mois_layers)
	resize(&sm.capillary_water, no_of_mois_layers)
	resize(&sm.capillary_water70, no_of_mois_layers)
	resize(&sm.evaporation, no_of_mois_layers)
	resize(&sm.evapotranspiration, no_of_mois_layers)
	resize(&sm.gravitational_water, no_of_mois_layers)
	resize(&sm.heat_conductivity, no_of_mois_layers)
	resize(&sm.percolation_rate, no_of_mois_layers)
	resize(&sm.residual_evapotranspiration, no_of_mois_layers)
	resize(&sm.soil_moisture_day_start, sm.no_of_soil_layers)
	resize(&sm.transpiration, no_of_mois_layers)

	sm.layer_thickness_m = layer_thickness
	sm.leaching_depth_layer_idx =
		int(libc.floor(0.5 + (sm.env_params.p_LeachingDepth / layer_thickness))) - 1

	initialize_snow_component(&sm.snow_component, sc, &sm.mod_params)
	initialize_frost_component(
		&sm.frost_component,
		sc,
		sm.mod_params.pm_HydraulicConductivityRedux,
		sm.env_params.p_timeStep,
		allocator,
	)

	return sm
}

// C++: void monica::soilmoisture::step(SoilMoisture*, double, double, double,
//        double, double, double, double, double, int, double)
//
// Named soil_moisture_step, not step - see soil_temperature_step's comment on
// why (this package mirrors src/core/, not individual C++ namespaces).
// dual_kc_method stands in for `sm->monica.simPs.dualKcMethod`, daily_sum_irrigation_water
// for `sm->monica.dailySumIrrigationWater` - see the package comment.
soil_moisture_step :: proc(
	sm: ^Soil_Moisture,
	groundwater_depth_m,
	precipitation_mm,
	max_air_temp_deg_C,
	min_air_temp_deg_C,
	relative_humidity_perc,
	avg_air_temp_deg_C,
	wind_speed_m_per_s,
	wind_speed_height_m,
	global_radiation_MJ_per_m2: f64,
	julian_day: int,
	reference_evapotranspiration_mm: f64,
	dual_kc_method: bool = false,
	daily_sum_irrigation_water_mm: f64 = 0.0,
) {
	sc := sm.soil_column

	for i in 0 ..< sm.no_of_soil_layers {
		sc.layers[i].soil_water_flux = 0.0
		sm.soil_moisture_day_start[i] = sc.layers[i].soil_moisture_m3
	}

	sm.soil_moisture_below_m3_m3 = sc.layers[sm.no_of_soil_layers - 1].soil_moisture_m3
	sm.water_flux_below = 0.0
	sm.field_capacity_below_m3_m3 = sc.layers[sm.no_of_soil_layers - 1].field_capacity
	sm.pore_volume_below_m3_m3 = sc.layers[sm.no_of_soil_layers - 1].saturation
	sm.lambda_below = sc.layers[sm.no_of_soil_layers - 1].lambda

	dev_stage := 0
	// C++: `sm->monica.currentCropModule.get()` - see the package comment.
	if sm.crop_module != nil {
		sm.soil_coverage_percent = sm.crop_module.soil_coverage
		sm.kc_factor = sm.crop_module.kc_factor
		dev_stage = sm.crop_module.developmental_stage
		if dev_stage > 0 {
			sm.net_precipitation_mm = sm.crop_module.net_precipitation
		} else {
			sm.net_precipitation_mm = precipitation_mm
		}
	} else {
		sm.kc_factor = sm.mod_params.pm_KcFactor
		sm.net_precipitation_mm = precipitation_mm
		sm.soil_coverage_percent = 0.0
	}

	// Recalculates current depth of groundwater table
	sm.groundwater_table_layer = sm.no_of_soil_layers + 2
	for i := sm.no_of_soil_layers - 1;
	    i >= 0 &&
	    int(sc.layers[i].soil_moisture_m3 * 10000) == int(sc.layers[i].saturation * 10000); {
		sm.groundwater_table_layer = i
		i -= 1
	}

	oscillGroundWaterLayer := int(groundwater_depth_m / sc.layers[0].layer_thickness)
	if (sm.groundwater_table_layer > oscillGroundWaterLayer &&
		   sm.groundwater_table_layer < sm.no_of_soil_layers + 2) ||
	   sm.groundwater_table_layer >= sm.no_of_soil_layers + 2 {
		sm.groundwater_table_layer = oscillGroundWaterLayer
	}

	sc.vm_GroundwaterTableLayer = sm.groundwater_table_layer

	// calculates snow layer water storage and release
	calc_snow_layer(&sm.snow_component, avg_air_temp_deg_C, sm.net_precipitation_mm)

	// Calculates frost and thaw depth and switches lambda
	calc_soil_frost(&sm.frost_component, avg_air_temp_deg_C, sm.snow_component.snow_depth)

	// calculates infiltration of water from surface
	infiltration(sm, sm.snow_component.water_to_infiltrate)

	if 0.0 < groundwater_depth_m && groundwater_depth_m <= 10.0 {
		percolation_with_groundwater(sm, oscillGroundWaterLayer)
		groundwater_replenishment(sm)
	} else {
		percolation_without_groundwater(sm)
		backwater_replenishment(sm)
	}

	evapotranspiration(
		sm,
		max_air_temp_deg_C,
		min_air_temp_deg_C,
		relative_humidity_perc,
		avg_air_temp_deg_C,
		wind_speed_m_per_s,
		wind_speed_height_m,
		global_radiation_MJ_per_m2,
		precipitation_mm,
		dev_stage,
		julian_day,
		reference_evapotranspiration_mm,
		dual_kc_method,
		daily_sum_irrigation_water_mm,
	)

	capillary_rise(sm)
}

// C++: void monica::soilmoisture::infiltration(SoilMoisture*, double)
infiltration :: proc(sm: ^Soil_Moisture, vm_WaterToInfiltrate: f64) {
	sc := sm.soil_column
	scl_0 := &sc.layers[0]

	sm.infiltration = 0.0
	sm.interception = 0.0
	sm.surface_run_off = 0.0
	sm.groundwater_added = 0.0
	sm.actual_transpiration = 0.0

	vm_SurfaceWaterStorageOld := sc.vs_SurfaceWaterStorage

	sc.vs_SurfaceWaterStorage += vm_WaterToInfiltrate

	sm.soil_moisture_deficit =
		(scl_0.saturation - scl_0.soil_moisture_m3) / scl_0.saturation
	vm_ReducedHydraulicConductivity :=
		sm.mod_params.pm_SaturatedHydraulicConductivity *
		sm.mod_params.pm_HydraulicConductivityRedux

	if vm_ReducedHydraulicConductivity > 0.0 {
		vm_PotentialInfiltration :=
			vm_ReducedHydraulicConductivity *
			0.2 *
			sm.soil_moisture_deficit *
			sm.soil_moisture_deficit

		sm.infiltration = min(sc.vs_SurfaceWaterStorage, vm_PotentialInfiltration)

		sm.infiltration = min(
			sm.infiltration,
			((scl_0.saturation - scl_0.soil_moisture_m3) * 1000.0 * sm.layer_thickness_m),
		)

		sm.infiltration = max(0.0, sm.infiltration)
	} else {
		sm.infiltration = 0.0
	}

	if sm.infiltration > 0.0 {
		sc.vs_SurfaceWaterStorage -= sm.infiltration
	}

	if sc.vs_SurfaceWaterStorage >
	   (10.0 * sm.mod_params.pm_SurfaceRoughness / (sm.site_params.vs_Slope + 0.001)) {
		vm_RunOffFactor :=
			0.02 + (sm.mod_params.pm_SurfaceRoughness / 4.0) + (sm.soil_coverage_percent / 15.0)
		if sm.site_params.vs_Slope < 0.0 || sm.site_params.vs_Slope > 1.0 {
			fmt.eprintln("Slope value out ouf boundary")
		} else if sm.site_params.vs_Slope == 0.0 {
			sm.surface_run_off = 0.0
		} else if sm.site_params.vs_Slope > vm_RunOffFactor {
			sm.surface_run_off += sc.vs_SurfaceWaterStorage
		} else {
			sm.surface_run_off +=
				((sm.site_params.vs_Slope * vm_RunOffFactor) /
					(vm_RunOffFactor * vm_RunOffFactor)) *
				sc.vs_SurfaceWaterStorage
		}

		sc.vs_SurfaceWaterStorage -= sm.surface_run_off
	}

	scl_0.soil_moisture_m3 += (sm.infiltration / 1000.0 / sm.layer_thickness_m)

	scl_0.soil_water_flux = sm.infiltration

	if scl_0.soil_moisture_m3 > scl_0.field_capacity {
		sm.gravitational_water[0] =
			(scl_0.soil_moisture_m3 - scl_0.field_capacity) * 1000.0 * sm.layer_thickness_m
		vm_LambdaReduced := scl_0.lambda * sm.frost_component.lambda_redux[0]
		vm_PercolationFactor := 1 + vm_LambdaReduced * sm.gravitational_water[0]
		sm.percolation_rate[0] =
			(sm.gravitational_water[0] * sm.gravitational_water[0] * vm_LambdaReduced) /
			vm_PercolationFactor
		if sm.percolation_rate[0] > sm.mod_params.pm_MaxPercolationRate {
			sm.percolation_rate[0] = sm.mod_params.pm_MaxPercolationRate
		}
		sm.gravitational_water[0] = sm.gravitational_water[0] - sm.percolation_rate[0]
		sm.gravitational_water[0] = max(0.0, sm.gravitational_water[0])

		scl_0.soil_moisture_m3 =
			scl_0.field_capacity + (sm.gravitational_water[0] / 1000.0 / sm.layer_thickness_m)

		if sm.groundwater_table_layer <= 1 {
			sm.percolation_rate[0] = 0.0
		}

		if sm.groundwater_table_layer == 0 {
			sm.percolation_rate[0] = 0.0

			if scl_0.soil_moisture_m3 > scl_0.saturation {
				sm.surface_run_off +=
					(scl_0.soil_moisture_m3 - scl_0.saturation) *
					1000.0 *
					sm.layer_thickness_m
				scl_0.soil_moisture_m3 = scl_0.saturation
				return
			}
		}
	} else if scl_0.soil_moisture_m3 <= scl_0.field_capacity {
		sm.percolation_rate[0] = 0.0
		sm.gravitational_water[0] = 0.0
	}

	// Check water balance
	if libc.fabs(
		   (vm_SurfaceWaterStorageOld + vm_WaterToInfiltrate) -
		   (sm.surface_run_off + sm.infiltration + sc.vs_SurfaceWaterStorage),
	   ) >
	   0.01 {
		fmt.eprintln("water balance wrong!")
	}

	sc.layers[1].soil_water_flux = sm.percolation_rate[0]
	sm.sum_surface_run_off += sm.surface_run_off
}

// C++: void monica::soilmoisture::capillaryRise(SoilMoisture*)
capillary_rise :: proc(sm: ^Soil_Moisture, allocator := context.allocator) {
	sc := sm.soil_column

	vc_RootingDepth := sm.crop_module != nil ? sm.crop_module.rooting_depth : 0

	// NOTE(c++-quirk): C++ computes this as size_t arithmetic
	// (max(size_t(1), groundwater_table_layer - vc_RootingDepth)): if
	// vc_RootingDepth ever exceeds groundwater_table_layer the subtraction
	// wraps to a huge positive number rather than going negative, reproduced
	// here with explicit uint arithmetic. Dormant while cropModule is nil
	// (vc_RootingDepth always 0); matters once phase 5 plants a crop whose
	// roots reach below a shallow water table.
	vm_GroundwaterDistance := max(
		uint(1),
		uint(sm.groundwater_table_layer) - uint(vc_RootingDepth),
	)

	if f64(vm_GroundwaterDistance) * sm.layer_thickness_m <= 2.70 {
		for i in 0 ..< sm.no_of_soil_layers {
			scl_i := &sc.layers[i]
			sm.capillary_water[i] = scl_i.field_capacity - scl_i.permanent_wilting_point
			sm.available_water[i] = scl_i.soil_moisture_m3 - scl_i.permanent_wilting_point

			if sm.available_water[i] < 0.0 {
				sm.available_water[i] = 0.0
			}

			sm.capillary_water70[i] = 0.7 * sm.capillary_water[i]
		}

		start_layer_idx := min(sm.groundwater_table_layer, sm.no_of_soil_layers - 1)
		cr := soil.read_capillary_rise_rates(allocator)
		for i := start_layer_idx; i >= 0; i -= 1 {
			scl_i := &sc.layers[i]
			assert(len(scl_i.soil_texture) > 0)
			vm_CapillaryRiseRate := min(
				0.01,
				soil.capillary_rise_rates_get_rate(
					cr,
					scl_i.soil_texture,
					int(vm_GroundwaterDistance),
				),
			)
			if sm.available_water[i] < sm.capillary_water70[i] {
				vm_WaterAddedFromCapillaryRise := vm_CapillaryRiseRate
				scl_i.soil_moisture_m3 += vm_WaterAddedFromCapillaryRise / sm.layer_thickness_m
				for j := start_layer_idx; j >= i; j -= 1 {
					sc.layers[j].soil_water_flux -= vm_WaterAddedFromCapillaryRise * 1000.0
				}
				break
			}
		}
	}
}

// C++: void monica::soilmoisture::percolationWithGroundwater(SoilMoisture*, size_t)
percolation_with_groundwater :: proc(sm: ^Soil_Moisture, oscill_groundwater_layer: int) {
	sm.groundwater_added = 0.0
	sc := sm.soil_column
	groundwater_discharge := sm.mod_params.pm_GroundwaterDischarge

	for i in 0 ..< sm.no_of_soil_layers {
		ib := i + 1

		soil_moisture_ib := &sm.soil_moisture_below_m3_m3
		water_flux_ib := &sm.water_flux_below
		field_capacity_ib := &sm.field_capacity_below_m3_m3
		lambda_ib := &sm.lambda_below
		pore_volume_ib := &sm.pore_volume_below_m3_m3

		if ib < sm.no_of_soil_layers {
			scl_ib := &sc.layers[ib]
			soil_moisture_ib = &scl_ib.soil_moisture_m3
			water_flux_ib = &scl_ib.soil_water_flux
			field_capacity_ib = &scl_ib.field_capacity
			lambda_ib = &scl_ib.lambda
			pore_volume_ib = &scl_ib.saturation
		}

		if sm.groundwater_table_layer > ib {
			// well above groundwater table
			soil_moisture_ib^ += sm.percolation_rate[i] / 1000.0 / sm.layer_thickness_m
			water_flux_ib^ = sm.percolation_rate[i]

			if soil_moisture_ib^ > field_capacity_ib^ {
				sm.gravitational_water[ib] =
					(soil_moisture_ib^ - field_capacity_ib^) * 1000.0 * sm.layer_thickness_m

				vm_LambdaReduced := lambda_ib^ * sm.frost_component.lambda_redux[ib]
				vm_PercolationFactor := 1 + vm_LambdaReduced * sm.gravitational_water[ib]
				sm.percolation_rate[ib] =
					(sm.gravitational_water[ib] * sm.gravitational_water[ib] * vm_LambdaReduced) /
					vm_PercolationFactor

				sm.gravitational_water[ib] = sm.gravitational_water[ib] - sm.percolation_rate[ib]

				if sm.gravitational_water[ib] < 0 {
					sm.gravitational_water[ib] = 0.0
				}

				soil_moisture_ib^ =
					field_capacity_ib^ +
					(sm.gravitational_water[ib] / 1000.0 / sm.layer_thickness_m)

				if soil_moisture_ib^ > pore_volume_ib^ {
					sm.gravitational_water[ib] =
						(soil_moisture_ib^ - pore_volume_ib^) * 1000.0 * sm.layer_thickness_m
					soil_moisture_ib^ = pore_volume_ib^
					sm.percolation_rate[ib] += sm.gravitational_water[ib]
				}
			} else {
				sm.percolation_rate[ib] = 0.0
				sm.gravitational_water[ib] = 0.0
			}
		} else if sm.groundwater_table_layer == ib {
			// when the layer directly above groundwater table is reached
			if sm.groundwater_table_layer >= oscill_groundwater_layer {
				soil_moisture_ib^ += sm.percolation_rate[i] / 1000.0 / sm.layer_thickness_m
				sm.percolation_rate[ib] = groundwater_discharge
				water_flux_ib^ = sm.percolation_rate[i]
			} else {
				soil_moisture_ib^ +=
					(sm.percolation_rate[i] - groundwater_discharge) /
					1000.0 /
					sm.layer_thickness_m
				sm.percolation_rate[ib] = groundwater_discharge
				water_flux_ib^ = groundwater_discharge
			}

			if soil_moisture_ib^ >= pore_volume_ib^ {
				sm.groundwater_added =
					(soil_moisture_ib^ - pore_volume_ib^) * 1000.0 * sm.layer_thickness_m

				soil_moisture_ib^ = pore_volume_ib^

				if sm.groundwater_added <= 0.0 {
					sm.groundwater_added = 0.0
				}
			}
		} else if sm.groundwater_table_layer < ib {
			// when the groundwater table is reached
			soil_moisture_ib^ = pore_volume_ib^

			if sm.groundwater_table_layer >= oscill_groundwater_layer {
				sm.percolation_rate[ib] = sm.percolation_rate[i]
				sc.layers[i].soil_water_flux = sm.percolation_rate[ib]
			} else {
				sm.percolation_rate[ib] = groundwater_discharge
				sc.layers[i].soil_water_flux = groundwater_discharge
			}
		}
	}

	if sm.leaching_depth_layer_idx == sm.no_of_soil_layers {
		sc.vs_FluxAtLowerBoundary = sm.water_flux_below
	} else {
		sc.vs_FluxAtLowerBoundary = sc.layers[sm.leaching_depth_layer_idx].soil_water_flux
	}
}

// C++: void monica::soilmoisture::groundwaterReplenishment(SoilMoisture*)
groundwater_replenishment :: proc(sm: ^Soil_Moisture) {
	start_layer := sm.groundwater_table_layer
	if start_layer > sm.no_of_soil_layers - 1 {
		start_layer = sm.no_of_soil_layers - 1
	}

	sc := sm.soil_column

	for i := start_layer; i >= 0; i -= 1 {
		ib := i + 1

		soil_moisture_i := &sc.layers[i].soil_moisture_m3
		pore_volume_i := &sc.layers[i].saturation

		water_flux_ib := &sm.water_flux_below

		if ib < sm.no_of_soil_layers {
			water_flux_ib = &sc.layers[ib].soil_water_flux
		}

		soil_moisture_i^ += sm.groundwater_added / 1000.0 / sm.layer_thickness_m

		if i == start_layer {
			sm.percolation_rate[i] = sm.mod_params.pm_GroundwaterDischarge
		} else {
			sm.percolation_rate[i] -= sm.groundwater_added
			water_flux_ib^ = sm.percolation_rate[i]
		}

		if soil_moisture_i^ > pore_volume_i^ {
			sm.groundwater_added =
				(soil_moisture_i^ - pore_volume_i^) * 1000.0 * sm.layer_thickness_m
			soil_moisture_i^ = pore_volume_i^
			sm.groundwater_table_layer -= 1 // Groundwater table rises

			if i == 0 && sm.groundwater_table_layer == 0 {
				// if groundwater reaches surface
				sc.vs_SurfaceWaterStorage += sm.groundwater_added
				sm.groundwater_added = 0.0
			}
		} else {
			sm.groundwater_added = 0.0
		}
	}

	if sm.leaching_depth_layer_idx > sm.groundwater_table_layer - 1 {
		if sm.groundwater_table_layer - 1 < 0 {
			sc.vs_FluxAtLowerBoundary = 0.0
		} else {
			if sm.groundwater_table_layer - 1 == sm.no_of_soil_layers {
				sc.vs_FluxAtLowerBoundary = sm.water_flux_below
			} else {
				sc.vs_FluxAtLowerBoundary =
					sc.layers[sm.groundwater_table_layer - 1].soil_water_flux
			}
		}
	} else {
		if sm.leaching_depth_layer_idx == sm.no_of_soil_layers {
			sc.vs_FluxAtLowerBoundary = sm.water_flux_below
		} else {
			sc.vs_FluxAtLowerBoundary = sc.layers[sm.leaching_depth_layer_idx].soil_water_flux
		}
	}
}

// C++: void monica::soilmoisture::percolationWithoutGroundwater(SoilMoisture*)
percolation_without_groundwater :: proc(sm: ^Soil_Moisture) {
	max_percolation_rate := sm.mod_params.pm_MaxPercolationRate
	sc := sm.soil_column

	for i in 0 ..< sm.no_of_soil_layers {
		ib := i + 1

		soil_moisture_ib := &sm.soil_moisture_below_m3_m3
		water_flux_ib := &sm.water_flux_below
		field_capacity_ib := &sm.field_capacity_below_m3_m3
		lambda_ib := &sm.lambda_below

		if ib < sm.no_of_soil_layers {
			scl_ib := &sc.layers[ib]
			soil_moisture_ib = &scl_ib.soil_moisture_m3
			water_flux_ib = &scl_ib.soil_water_flux
			field_capacity_ib = &scl_ib.field_capacity
			lambda_ib = &scl_ib.lambda
		}

		soil_moisture_ib^ += sm.percolation_rate[i] / 1000.0 / sm.layer_thickness_m

		if soil_moisture_ib^ > field_capacity_ib^ {
			// too much water for this layer so some water is released to layers below
			sm.gravitational_water[ib] =
				(soil_moisture_ib^ - field_capacity_ib^) * 1000.0 * sm.layer_thickness_m
			vm_LambdaReduced := lambda_ib^ * sm.frost_component.lambda_redux[ib]
			vm_PercolationFactor := 1.0 + (vm_LambdaReduced * sm.gravitational_water[ib])
			sm.percolation_rate[ib] =
				(sm.gravitational_water[ib] * sm.gravitational_water[ib] * vm_LambdaReduced) /
				vm_PercolationFactor

			if sm.percolation_rate[ib] > max_percolation_rate {
				sm.percolation_rate[ib] = max_percolation_rate
			}

			sm.gravitational_water[ib] = sm.gravitational_water[ib] - sm.percolation_rate[ib]

			if sm.gravitational_water[ib] < 0.0 {
				sm.gravitational_water[ib] = 0.0
			}

			soil_moisture_ib^ =
				field_capacity_ib^ + (sm.gravitational_water[ib] / 1000.0 / sm.layer_thickness_m)
		} else {
			// no water will be released in other layers
			sm.percolation_rate[ib] = 0.0
			sm.gravitational_water[ib] = 0.0
		}

		water_flux_ib^ = sm.percolation_rate[i]
		sm.groundwater_added = sm.percolation_rate[ib]
	}

	if sm.leaching_depth_layer_idx > 0 && sm.leaching_depth_layer_idx < sm.no_of_soil_layers {
		sc.vs_FluxAtLowerBoundary = sc.layers[sm.leaching_depth_layer_idx].soil_water_flux
	} else {
		sc.vs_FluxAtLowerBoundary = sc.layers[sm.no_of_soil_layers - 1].soil_water_flux
	}
}

// C++: void monica::soilmoisture::backwaterReplenishment(SoilMoisture*)
backwater_replenishment :: proc(sm: ^Soil_Moisture) {
	start_layer_idx := sm.no_of_soil_layers
	vm_BackwaterTable := sm.no_of_soil_layers
	vm_BackwaterAdded := 0.0

	sc := sm.soil_column

	// find first layer from top where the water content exceeds pore volume
	for i in 0 ..< sm.no_of_soil_layers {
		scl_i := &sc.layers[i]
		if scl_i.soil_moisture_m3 > scl_i.saturation {
			start_layer_idx = i
			vm_BackwaterTable = i
		}
	}

	// if there is no such thing nothing will happen
	if vm_BackwaterTable == 0 {
		return
	}

	// Backwater replenishment upwards
	for i := start_layer_idx; i >= 0; i -= 1 {

		soil_moisture_i := &sm.soil_moisture_below_m3_m3
		pore_volume_i := &sm.pore_volume_below_m3_m3

		if i < sm.no_of_soil_layers {
			scl_i := &sc.layers[i]
			soil_moisture_i = &scl_i.soil_moisture_m3
			pore_volume_i = &scl_i.saturation
		}

		soil_moisture_i^ += vm_BackwaterAdded / 1000.0 / sm.layer_thickness_m
		if i > 0 {
			sc.layers[i - 1].soil_water_flux -= vm_BackwaterAdded
		}

		if soil_moisture_i^ > pore_volume_i^ {
			vm_BackwaterAdded = (soil_moisture_i^ - pore_volume_i^) * 1000.0 * sm.layer_thickness_m
			soil_moisture_i^ = pore_volume_i^
			vm_BackwaterTable -= 1 // Backwater table rises

			if i == 0 && vm_BackwaterTable == 0 {
				// if backwater reaches surface
				sc.vs_SurfaceWaterStorage += vm_BackwaterAdded
				vm_BackwaterAdded = 0.0
			}
		} else {
			vm_BackwaterAdded = 0.0
		}
	}
}

// C++: double monica::soilmoisture::dualKcPrecomputation(SoilMoisture*, double,
//        double, double)
//
// Only ever called when sm.cropModule != nil (see evapotranspiration's
// useDualKc guard), matching the C++'s unchecked `cropModule->` derefs below.
// daily_sum_irrigation_water stands in for `sm->monica.dailySumIrrigationWater`
// - see the package comment.
dual_kc_precomputation :: proc(
	sm: ^Soil_Moisture,
	windSpeed, tmin, tmax, precip: f64,
	daily_sum_irrigation_water: f64,
) -> f64 {
	E_pot_dualKc := 0.0 // [mm d-1] replaces (1-beta)*PET per layer

	// ET0 is already in vm_ReferenceEvapotranspiration [mm d-1]
	ET0 := sm.reference_evapotranspiration

	// --- Kcb: basal crop coefficient (interpolated in crop module) ---
	Kcb := sm.crop_module.kcb_factor

	// --- Calculate Depletion first (FAO-56 §8.3) to inform memory logic ---
	scl_0 := &sm.soil_column.layers[0]
	FC0 := scl_0.field_capacity
	WP0 := scl_0.permanent_wilting_point
	SWC0 := scl_0.soil_moisture_m3
	Ze := 0.1 // evaporation depth [m], FAO-56 typical top layer
	TEW := 1000.0 * (FC0 - 0.5 * WP0) * Ze

	// A. Dynamic REW (FAO-56 Table 19 Mapping via Pedology)
	//
	// NOTE(c++-quirk): these comparisons are against mixed-case strings
	// ("Ss", "Su2", ...) while real KA5 textures in this port's fixtures are
	// always uppercase ("SS", "SU2", ...) - so none of these branches ever
	// match on real data and REW always falls through to the FC0-based
	// fallback below. Reproduced as-is, not fixed.
	REW := 0.0
	ka5_texture := scl_0.soil_texture

	switch {
	case ka5_texture == "Ss":
		REW = 2.5
	case ka5_texture == "Su2" || ka5_texture == "Sl2":
		REW = 3.5
	case ka5_texture == "Su3" || ka5_texture == "Sl3":
		REW = 4.5
	case ka5_texture == "Su4" || ka5_texture == "Sl4" || ka5_texture == "St2":
		REW = 5.5
	case ka5_texture == "St3" || ka5_texture == "Ls2" || ka5_texture == "Us2":
		REW = 8.0
	case ka5_texture == "Uu" ||
	     ka5_texture == "Us3" ||
	     ka5_texture == "Us4" ||
	     ka5_texture == "Ul2" ||
	     ka5_texture == "Ul3" ||
	     ka5_texture == "Ul4":
		REW = 8.5
	case ka5_texture == "Ls3" ||
	     ka5_texture == "Ls4" ||
	     ka5_texture == "Lu2" ||
	     ka5_texture == "Lu3" ||
	     ka5_texture == "Lu4":
		REW = 9.0
	case ka5_texture == "Ut2" || ka5_texture == "Ut3":
		REW = 9.5
	case ka5_texture == "Ut4" ||
	     ka5_texture == "Lt2" ||
	     ka5_texture == "Lt3" ||
	     ka5_texture == "Lts":
		REW = 10.5
	case ka5_texture == "Ts2" ||
	     ka5_texture == "Ts3" ||
	     ka5_texture == "Ts4" ||
	     ka5_texture == "Tu2" ||
	     ka5_texture == "Tu3" ||
	     ka5_texture == "Tu4" ||
	     ka5_texture == "Tl":
		REW = 11.5
	case ka5_texture == "Tt":
		REW = 12.0
	}

	// Fallback if KA5 lookup is unavailable or fails:
	if REW == 0.0 {
		if FC0 < 0.18 {
			// Coarse soils (Sand, Sandy Loams): REW ranges from 2.0 to 7.0 mm
			REW = 2.5 + 25.0 * (FC0 - 0.05)
			REW = max(2.0, min(REW, 7.0))
		} else if FC0 >= 0.28 {
			// Fine soils (Clays): REW ranges from 8.0 to 12.0 mm
			REW = 8.0 + 20.0 * (FC0 - 0.28)
			REW = max(8.0, min(REW, 12.0))
		} else {
			REW = 8.0
		}
	}
	REW = min(REW, TEW) // Hard boundary safety clamp

	// Current depletion De [mm] = what the top layer is missing compared to FC
	De := 1000.0 * max(0.0, FC0 - SWC0) * Ze

	// --- Memory state update for wetting events ---
	// precip := sm.vm_GrossPrecipitation
	irrigApplied := daily_sum_irrigation_water
	if precip > 0.0 {
		sm.last_wetting_was_rain = true
	} else if irrigApplied > 0.0 {
		sm.last_wetting_was_rain = false
	}

	// --- fw: fraction of wetted soil surface (event-level, FAO-56 §8.3) ---
	fw_today: f64
	if precip > 0.0 {
		fw_today = 1.0 // rain wets the full surface
	} else if De <= REW && sm.last_wetting_was_rain {
		fw_today = 1.0 // still in Stage 1 drying from recent rain
	} else {
		fw_today = sm.irrig_fw_event // carry/use the last Irrigation workstep fw
	}
	fw_today = max(0.0, min(fw_today, 1.0))

	// Drip irrigation shading adjustment (FAO-56 §8.3)
	fw_adj := fw_today
	if sm.irrig_is_drip_event && !sm.last_wetting_was_rain && precip == 0.0 {
		fw_adj = fw_today * (1.0 - (2.0 / 3.0) * sm.soil_coverage_percent)
		fw_adj = max(0.0, min(fw_adj, 1.0))
	}

	// --- Kr: evaporation reduction coefficient (FAO-56 §8.3) ---
	Kr := 1.0
	if De > REW {
		if (TEW - REW) > 0.0 {
			Kr = (TEW - De) / (TEW - REW)
		} else {
			Kr = 0.0
		}
	}
	Kr = max(0.0, min(Kr, 1.0))

	// B. Rigorous Climatic Kc_max (FAO-56 Equation 72)
	u2 := windSpeed > 0.0 ? windSpeed : 2.0
	eo_Tmax := 0.6108 * libc.exp((17.27 * tmax) / (tmax + 237.3))
	eo_Tmin := 0.6108 * libc.exp((17.27 * tmin) / (tmin + 237.3))
	RHmin := max(5.0, min(100.0, (eo_Tmin / eo_Tmax) * 100.0))
	baseline := 1.2 // FAO-56 §6 default for most crops
	h := max(0.01, sm.crop_module.crop_height) // native simulated height [m]
	Kc_max := baseline + (0.04 * (u2 - 2.0) - 0.004 * (RHmin - 45.0)) * libc.pow(h / 3.0, 0.3)
	Kc_max = max(Kc_max, Kcb + 0.05)

	// C. few: fraction of exposed and wetted soil (FAO-56 Eq. 74)
	fc := max(0.0, min(sm.soil_coverage_percent, 0.99))
	few := min(1.0 - fc, fw_adj)
	few = max(0.001, few) // guard against zero denominator

	// --- Ke: soil evaporation coefficient (FAO-56 eq. 71) ---
	Ke := Kr * (Kc_max - Kcb)
	Ke = min(Ke, few * Kc_max)
	Ke = max(0.0, Ke)

	// Potential soil evaporation [mm d-1]
	E_pot_dualKc = ET0 * Ke
	// Respect the hard cap from HERMES (6.5 mm/day applies to total, cap E too)
	E_pot_dualKc = min(E_pot_dualKc, 6.5)

	sm.ke = Ke

	return E_pot_dualKc
}

// C++: void monica::soilmoisture::evapotranspiration(SoilMoisture*, double,
//        double, double, double, double, double, double, double, double,
//        double, int, int, double, double)
//
// dual_kc_method stands in for `sm->monica.simPs.dualKcMethod`,
// daily_sum_irrigation_water for `sm->monica.dailySumIrrigationWater` - see
// the package comment. Threaded through to dual_kc_precomputation.
evapotranspiration :: proc(
	sm: ^Soil_Moisture,
	max_air_temp_deg,
	min_air_temp_deg,
	relative_humidity_perc,
	avg_air_temp_degC,
	wind_speed_m_per_s,
	wind_speed_height_m,
	globral_radiation_MJ_per_m2,
	precip_mm: f64,
	developmental_stage: int,
	julian_day: int,
	reference_evapotranspiration_mm: f64,
	dual_kc_method: bool,
	daily_sum_irrigation_water_mm: f64,
) {
	sc := sm.soil_column

	vm_EReducer_1 := 0.0
	vm_EReducer_2 := 0.0
	vm_EReducer_3 := 0.0
	pm_EvaporationZeta: f64
	pm_MaximumEvaporationImpactDepth: f64
	vm_EReducer := 0.0
	vm_PotentialEvapotranspiration := 0.0
	vc_EvaporatedFromIntercept := 0.0
	sm.evaporated_from_surface = 0.0
	vm_EvaporationFromSurface := false

	vm_SnowDepth := sm.snow_component.snow_depth

	// Berechnung der Bodenevaporation bis max. 4dm Tiefe
	pm_EvaporationZeta = sm.mod_params.pm_EvaporationZeta

	sm.xsa_critical_soil_moisture = sm.mod_params.pm_XSACriticalSoilMoisture

	pm_MaximumEvaporationImpactDepth = sm.mod_params.pm_MaximumEvaporationImpactDepth

	// If a crop grows, ETp is taken from crop module
	if developmental_stage > 0 {
		// C++: `monica.currentCropModule.get()` - see the package comment.
		if reference_evapotranspiration_mm < 0.0 {
			sm.reference_evapotranspiration = sm.crop_module.reference_evapotranspiration
		} else {
			sm.reference_evapotranspiration = reference_evapotranspiration_mm
		}

		vm_PotentialEvapotranspiration = sm.crop_module.remaining_evapotranspiration
		vc_EvaporatedFromIntercept = sm.crop_module.evaporated_from_intercept
	} else { 	// if no crop grows ETp is calculated from ET0 * kc
		if reference_evapotranspiration_mm < 0.0 {
			sm.reference_evapotranspiration = reference_evapotranspiration(
				sm,
				max_air_temp_deg,
				min_air_temp_deg,
				relative_humidity_perc,
				avg_air_temp_degC,
				wind_speed_m_per_s,
				wind_speed_height_m,
				globral_radiation_MJ_per_m2,
				julian_day,
			)
		} else {
			sm.reference_evapotranspiration = reference_evapotranspiration_mm
		}

		vm_PotentialEvapotranspiration = sm.reference_evapotranspiration * sm.kc_factor
	}

	sm.actual_evaporation = 0.0
	sm.actual_transpiration = 0.0

	// from HERMES:
	if vm_PotentialEvapotranspiration > 6.5 {
		vm_PotentialEvapotranspiration = 6.5
	}

	if vm_PotentialEvapotranspiration > 0.0 {
		// If surface is water-logged, subsequent evaporation from surface water sources
		if sc.vs_SurfaceWaterStorage > 0.0 {
			vm_EvaporationFromSurface = true
			// Water surface evaporates with Kc = 1.1.
			vm_PotentialEvapotranspiration = vm_PotentialEvapotranspiration * (1.1 / sm.kc_factor)

			// If a snow layer is present no water evaporates from surface water sources
			if vm_SnowDepth > 0.0 {
				sm.evaporated_from_surface = 0.0
			} else {
				if sc.vs_SurfaceWaterStorage < vm_PotentialEvapotranspiration {
					vm_PotentialEvapotranspiration -= sc.vs_SurfaceWaterStorage
					sm.evaporated_from_surface = sc.vs_SurfaceWaterStorage
					sc.vs_SurfaceWaterStorage = 0.0
				} else {
					sc.vs_SurfaceWaterStorage -= vm_PotentialEvapotranspiration
					sm.evaporated_from_surface = vm_PotentialEvapotranspiration
					vm_PotentialEvapotranspiration = 0.0
				}
			}
			vm_PotentialEvapotranspiration = vm_PotentialEvapotranspiration * (sm.kc_factor / 1.1)
		}

		if vm_PotentialEvapotranspiration > 0 { 	// Evaporation from soil
			// -----------------------------------------------------------------
			// FAO-56 Dual Kc pathway - precompute potential soil evaporation
			// (E_pot) using the Ke coefficient. Runs ONLY when dualKcMethod is
			// enabled, a crop is present (vc_DevelopmentalStage > 0), and
			// sm.cropModule != nil. Otherwise the Single Kc loop runs unchanged.
			// -----------------------------------------------------------------
			E_pot_dualKc := 0.0
			useDualKc := dual_kc_method && developmental_stage > 0 && sm.crop_module != nil
			if useDualKc {
				E_pot_dualKc = dual_kc_precomputation(
					sm,
					wind_speed_m_per_s,
					min_air_temp_deg,
					max_air_temp_deg,
					precip_mm,
					daily_sum_irrigation_water_mm,
				)
			} else {
				sm.ke = 0.0
			}

			sc := sm.soil_column

			for i_Layer in 0 ..< sm.no_of_soil_layers {
				vm_EReducer_1 = get_e_reducer_1(
					sm,
					i_Layer,
					sm.soil_coverage_percent,
					vm_PotentialEvapotranspiration,
				)

				scl_i := &sc.layers[i_Layer]

				if f64(i_Layer) >= pm_MaximumEvaporationImpactDepth {
					// layer is too deep for evaporation
					vm_EReducer_2 = 0.0
				} else {
					vm_EReducer_2 = get_deprivation_factor(
						i_Layer + 1,
						pm_MaximumEvaporationImpactDepth,
						pm_EvaporationZeta,
						sm.layer_thickness_m,
					)
				}

				if i_Layer > 0 {
					if scl_i.soil_moisture_m3 < sc.layers[i_Layer - 1].soil_moisture_m3 {
						vm_EReducer_3 = 0.1
					} else {
						vm_EReducer_3 = 1.0
					}
				} else {
					vm_EReducer_3 = 1.0
				}
				vm_EReducer = vm_EReducer_1 * vm_EReducer_2 * vm_EReducer_3

				if developmental_stage > 0 {
					// vegetation is present
					if useDualKc {
						sm.evaporation[i_Layer] = vm_EReducer * E_pot_dualKc
					} else {
						// Single Kc: original (1 - beta) * EReducer * PET partitioning
						if sm.soil_coverage_percent >= 0.0 && sm.soil_coverage_percent < 1.0 {
							sm.evaporation[i_Layer] =
								((1.0 - sm.soil_coverage_percent) * vm_EReducer) *
								vm_PotentialEvapotranspiration
						} else {
							if sm.soil_coverage_percent >= 1.0 {
								sm.evaporation[i_Layer] = 0.0
							}
						}
					}

					if vm_SnowDepth > 0.0 {
						sm.evaporation[i_Layer] = 0.0
					}

					// C++: `monica.currentCropModule.get()->vc_Transpiration` - see the package comment.
					sm.transpiration[i_Layer] = sm.crop_module.transpiration[i_Layer]

					// Transpiration is capped in case potential ET after surface
					// and interception evaporation has occurred on same day
					if vm_EvaporationFromSurface {
						sm.transpiration[i_Layer] =
							sm.soil_coverage_percent * vm_EReducer * vm_PotentialEvapotranspiration
					}
				} else {
					// no vegetation present - Single Kc / bare soil, always unchanged
					if vm_SnowDepth > 0.0 {
						sm.evaporation[i_Layer] = 0.0
					} else {
						sm.evaporation[i_Layer] = vm_PotentialEvapotranspiration * vm_EReducer
					}
					sm.transpiration[i_Layer] = 0.0
				}

				sm.evapotranspiration[i_Layer] =
					sm.evaporation[i_Layer] + sm.transpiration[i_Layer]
				scl_i.soil_moisture_m3 -=
					(sm.evapotranspiration[i_Layer] / 1000.0 / sm.layer_thickness_m)

				// Generelle Begrenzung des Evaporationsentzuges
				if scl_i.soil_moisture_m3 < 0.01 {
					scl_i.soil_moisture_m3 = 0.01
				}

				sm.actual_transpiration += sm.transpiration[i_Layer]
				sm.actual_evaporation += sm.evaporation[i_Layer]
			}
		}
	}
	sm.actual_evapotranspiration =
		sm.actual_transpiration +
		sm.actual_evaporation +
		vc_EvaporatedFromIntercept +
		sm.evaporated_from_surface
}

// C++: double monica::soilmoisture::referenceEvapotranspiration(SoilMoisture*,
//        double, double, double, double, double, double, double, double, int,
//        double)
//
// Penman-Monteith method, FAO Irrigation and Drainage Paper 56.
reference_evapotranspiration :: proc(
	sm: ^Soil_Moisture,
	vw_MaxAirTemperature,
	vw_MinAirTemperature,
	vw_RelativeHumidity,
	vw_MeanAirTemperature,
	vw_WindSpeed,
	vw_WindSpeedHeight,
	vw_GlobalRadiation: f64,
	vs_JulianDay: int,
) -> f64 {
	pc_ReferenceAlbedo := sm.crop_mod_params.pc_ReferenceAlbedo
	vs_HeightNN := sm.site_params.vs_HeightNN
	vs_Latitude := sm.site_params.vs_Latitude
	PI := 3.14159265358979323

	vc_Declination := -23.4 * libc.cos(2.0 * PI * ((f64(vs_JulianDay) + 10.0) / 365.0))
	vc_DeclinationSinus :=
		libc.sin(vc_Declination * PI / 180.0) * libc.sin(vs_Latitude * PI / 180.0)
	vc_DeclinationCosinus :=
		libc.cos(vc_Declination * PI / 180.0) * libc.cos(vs_Latitude * PI / 180.0)

	arg_AstroDayLength := vc_DeclinationSinus / vc_DeclinationCosinus
	// The argument of asin must be in the range of -1 to 1
	arg_AstroDayLength = tl.bound(-1.0, arg_AstroDayLength, 1.0)
	vc_AstronomicDayLenght := 12.0 * (PI + 2.0 * libc.asin(arg_AstroDayLength)) / PI

	arg_EffectiveDayLength :=
		(-libc.sin(8.0 * PI / 180.0) + vc_DeclinationSinus) / vc_DeclinationCosinus
	arg_EffectiveDayLength = tl.bound(-1.0, arg_EffectiveDayLength, 1.0)
	vc_EffectiveDayLenght := 12.0 * (PI + 2.0 * libc.asin(arg_EffectiveDayLength)) / PI

	arg_PhotoDayLength :=
		(-libc.sin(-6.0 * PI / 180.0) + vc_DeclinationSinus) / vc_DeclinationCosinus
	arg_PhotoDayLength = tl.bound(-1.0, arg_PhotoDayLength, 1.0)
	vc_PhotoperiodicDaylength := 12.0 * (PI + 2.0 * libc.asin(arg_PhotoDayLength)) / PI
	_ = vc_EffectiveDayLenght
	_ = vc_PhotoperiodicDaylength

	arg_PhotAct := min(
		1.0,
		((vc_DeclinationSinus / vc_DeclinationCosinus) *
			(vc_DeclinationSinus / vc_DeclinationCosinus)),
	)
	// The argument of sqrt must be >= 0
	vc_PhotActRadiationMean :=
		3600.0 *
		(vc_DeclinationSinus * vc_AstronomicDayLenght +
				24.0 / PI * vc_DeclinationCosinus * libc.sqrt(1.0 - arg_PhotAct))

	vc_ClearDayRadiation := 0.0
	if vc_PhotActRadiationMean > 0 && vc_AstronomicDayLenght > 0 {
		vc_ClearDayRadiation =
			0.5 *
			1300.0 *
			vc_PhotActRadiationMean *
			libc.exp(-0.14 / (vc_PhotActRadiationMean / (vc_AstronomicDayLenght * 3600.0)))
	}
	// vc_OvercastDayRadiation, like vc_EffectiveDayLenght/vc_PhotoperiodicDaylength/
	// vm_AerodynamicResistance below, is computed by the C++ but never read
	// afterward (dead in the C++ itself) - kept for literal fidelity.
	vc_OvercastDayRadiation := 0.2 * vc_ClearDayRadiation
	_ = vc_OvercastDayRadiation

	SC := 24.0 * 60.0 / PI * 8.20 * (1.0 + 0.033 * libc.cos(2.0 * PI * f64(vs_JulianDay) / 365.0))
	arg_SHA := tl.bound(
		-1.0,
		-libc.tan(vs_Latitude * PI / 180.0) * libc.tan(vc_Declination * PI / 180.0),
		1.0,
	)
	// The argument of acos must be in the range of -1 to 1
	SHA := libc.acos(arg_SHA)

	// [J cm-2] --> [MJ m-2]
	vc_ExtraterrestrialRadiation :=
		SC * (SHA * vc_DeclinationSinus + vc_DeclinationCosinus * libc.sin(SHA)) / 100.0

	// Calculation of atmospheric pressure
	vm_AtmosphericPressure := 101.3 * libc.pow(((293.0 - (0.0065 * vs_HeightNN)) / 293.0), 5.26)

	// Calculation of psychrometer constant - Luftfeuchtigkeit
	vm_PsycrometerConstant := 0.000665 * vm_AtmosphericPressure

	// Calc. of saturated water vapour pressure at daily max temperature
	vm_SaturatedVapourPressureMax :=
		0.6108 * libc.exp((17.27 * vw_MaxAirTemperature) / (237.3 + vw_MaxAirTemperature))

	// Calc. of saturated water vapour pressure at daily min temperature
	vm_SaturatedVapourPressureMin :=
		0.6108 * libc.exp((17.27 * vw_MinAirTemperature) / (237.3 + vw_MinAirTemperature))

	// Calculation of the saturated water vapour pressure
	vm_SaturatedVapourPressure :=
		(vm_SaturatedVapourPressureMax + vm_SaturatedVapourPressureMin) / 2.0

	// Calculation of the water vapour pressure
	vm_VapourPressure: f64
	if vw_RelativeHumidity <= 0.0 {
		// Assuming Tdew = Tmin as suggested in FAO56 Allen et al. 1998
		vm_VapourPressure = vm_SaturatedVapourPressureMin
	} else {
		vm_VapourPressure = vw_RelativeHumidity * vm_SaturatedVapourPressure
	}

	// Calculation of the air saturation deficit
	vm_SaturationDeficit := vm_SaturatedVapourPressure - vm_VapourPressure

	// Slope of saturation water vapour pressure-to-temperature relation
	vm_SaturatedVapourPressureSlope :=
		(4098.0 *
			(0.6108 *
					libc.exp((17.27 * vw_MeanAirTemperature) / (vw_MeanAirTemperature + 237.3)))) /
		((vw_MeanAirTemperature + 237.3) * (vw_MeanAirTemperature + 237.3))

	// Calculation of wind speed in 2m height
	// 0.5 minimum allowed windspeed for Penman-Monteith-Method FAO
	vm_WindSpeed_2m := max(0.5, vw_WindSpeed * (4.87 / libc.log(67.8 * vw_WindSpeedHeight - 5.42)))

	// Calculation of the aerodynamic resistance
	vm_AerodynamicResistance := 208.0 / vm_WindSpeed_2m
	_ = vm_AerodynamicResistance

	vm_SurfaceResistance := STOMATA_RESISTANCE / 1.44

	vc_ClearSkySolarRadiation := (0.75 + 0.00002 * vs_HeightNN) * vc_ExtraterrestrialRadiation
	vc_RelativeShortwaveRadiation :=
		vc_ClearSkySolarRadiation > 0 ? min(vw_GlobalRadiation / vc_ClearSkySolarRadiation, 1.0) : 1.0

	pc_BolzmannConstant := 0.0000000049
	vc_ShortwaveRadiation := (1.0 - pc_ReferenceAlbedo) * vw_GlobalRadiation
	vc_LongwaveRadiation :=
		pc_BolzmannConstant *
		((libc.pow(vw_MinAirTemperature + 273.16, 4.0) +
					libc.pow(vw_MaxAirTemperature + 273.16, 4.0)) /
				2.0) *
		(1.35 * vc_RelativeShortwaveRadiation - 0.35) *
		(0.34 - 0.14 * libc.sqrt(vm_VapourPressure))
	sm.net_radiation = vc_ShortwaveRadiation - vc_LongwaveRadiation

	// Calculation of the reference evapotranspiration - Penman-Monteith-Methode FAO
	vm_ReferenceEvapotranspiration :=
		((0.408 * vm_SaturatedVapourPressureSlope * sm.net_radiation) +
			(vm_PsycrometerConstant *
					(900.0 / (vw_MeanAirTemperature + 273.0)) *
					vm_WindSpeed_2m *
					vm_SaturationDeficit)) /
		(vm_SaturatedVapourPressureSlope +
				vm_PsycrometerConstant * (1.0 + (vm_SurfaceResistance / 208.0) * vm_WindSpeed_2m))

	if vm_ReferenceEvapotranspiration < 0.0 {
		vm_ReferenceEvapotranspiration = 0.0
	}

	return vm_ReferenceEvapotranspiration
}

// C++: double monica::soilmoisture::getEReducer1(const SoilMoisture*, int,
//        double, double)
get_e_reducer_1 :: proc(
	sm: ^Soil_Moisture,
	i_Layer: int,
	vm_PercentageSoilCoverage, vm_ReferenceEvapotranspiration: f64,
) -> f64 {
	vm_EReductionFactor: f64
	vm_EvaporationReductionMethod := 1
	// C++-quirk (see soil_moisture_day_start's field comment): this reads
	// today's moisture as of the START of the day, before infiltration/
	// percolation/capillary_rise.
	vm_SoilMoisture_m3 := sm.soil_moisture_day_start[i_Layer]
	vm_PWP := sm.soil_column.layers[i_Layer].permanent_wilting_point
	vm_FK := sm.soil_column.layers[i_Layer].field_capacity
	vm_RelativeEvaporableWater: f64
	vm_CriticalSoilMoisture: f64
	vm_XSA: f64
	vm_Reducer: f64

	if vm_SoilMoisture_m3 < (0.33 * vm_PWP) {
		vm_SoilMoisture_m3 = 0.33 * vm_PWP
	}

	vm_RelativeEvaporableWater = (vm_SoilMoisture_m3 - (0.33 * vm_PWP)) / (vm_FK - (0.33 * vm_PWP))

	if vm_RelativeEvaporableWater > 1.0 {
		vm_RelativeEvaporableWater = 1.0
	}

	if vm_EvaporationReductionMethod == 0 {
		// THESEUS
		vm_CriticalSoilMoisture = 0.65 * vm_FK
		if vm_PercentageSoilCoverage > 0 {
			if vm_ReferenceEvapotranspiration > 2.5 {
				vm_XSA = (0.65 * vm_FK - vm_PWP) * (vm_FK - vm_PWP)
				vm_Reducer =
					vm_XSA + (((1 - vm_XSA) / 17.5) * (vm_ReferenceEvapotranspiration - 2.5))
			} else {
				vm_Reducer = sm.xsa_critical_soil_moisture / 2.5 * vm_ReferenceEvapotranspiration
			}
			vm_CriticalSoilMoisture = sm.soil_column.layers[i_Layer].field_capacity * vm_Reducer
		}

		// Calculation of an evaporation-reducing factor in relation to soil water content
		if vm_SoilMoisture_m3 > vm_CriticalSoilMoisture {
			vm_EReductionFactor = 1.0
		} else {
			if vm_SoilMoisture_m3 > (0.33 * vm_PWP) {
				vm_EReductionFactor = vm_RelativeEvaporableWater
			} else {
				vm_EReductionFactor = 0.0
			}
		}
	} else if vm_EvaporationReductionMethod == 1 {
		// HERMES
		vm_EReductionFactor = 0.0
		if vm_RelativeEvaporableWater > 0.33 {
			vm_EReductionFactor = 1.0 - (0.1 * (1.0 - vm_RelativeEvaporableWater) / (1.0 - 0.33))
		} else if vm_RelativeEvaporableWater > 0.22 {
			vm_EReductionFactor =
				0.9 - (0.625 * (0.33 - vm_RelativeEvaporableWater) / (0.33 - 0.22))
		} else if vm_RelativeEvaporableWater > 0.2 {
			vm_EReductionFactor =
				0.275 - (0.225 * (0.22 - vm_RelativeEvaporableWater) / (0.22 - 0.2))
		} else {
			vm_EReductionFactor = 0.05 - (0.05 * (0.2 - vm_RelativeEvaporableWater) / 0.2)
		}
	}
	return vm_EReductionFactor
}

// C++: double monica::soilmoisture::getDeprivationFactor(int, double, double, double)
get_deprivation_factor :: proc(
	layerNo: int,
	deprivationDepth, zeta, vs_LayerThickness: f64,
) -> f64 {
	deprivationFactor: f64

	// factor to introduce layer thickness in this algorithm, to allow layer
	// thickness scaling (Claas Nendel)
	layerThicknessFactor := deprivationDepth / (vs_LayerThickness * 10.0)

	if libc.fabs(zeta) < 0.0003 {
		deprivationFactor =
			(2.0 / layerThicknessFactor) -
			(1.0 / (layerThicknessFactor * layerThicknessFactor)) * f64(2 * layerNo - 1)
		return deprivationFactor
	} else {
		c2 := libc.log(
			(layerThicknessFactor + zeta * f64(layerNo)) /
			(layerThicknessFactor + zeta * f64(layerNo - 1)),
		)
		c3 := zeta / (layerThicknessFactor * (zeta + 1.0))
		deprivationFactor = (c2 - c3) / (libc.log(zeta + 1.0) - zeta / (zeta + 1.0))
		return deprivationFactor
	}
}

// C++: double monica::soilmoisture::meanWaterContent(const SoilMoisture*, double)
mean_water_content_to_depth :: proc(sm: ^Soil_Moisture, depth_m: f64) -> f64 {
	lsum := 0.0
	sum := 0.0
	count := 0

	for i in 0 ..< sm.no_of_soil_layers {
		count += 1
		smm3 := sm.soil_column.layers[i].soil_moisture_m3
		fc := sm.soil_column.layers[i].field_capacity
		pwp := sm.soil_column.layers[i].permanent_wilting_point
		sum += smm3 / (fc - pwp) //[%nFK]
		lsum += sm.soil_column.layers[i].layer_thickness
		if lsum >= depth_m {
			break
		}
	}

	return sum / f64(count)
}

// C++: double monica::soilmoisture::meanWaterContent(const SoilMoisture*, int, int)
mean_water_content :: proc(sm: ^Soil_Moisture, layer, number_of_layers: int) -> f64 {
	sum := 0.0
	count := 0

	if layer + number_of_layers > sm.no_of_soil_layers {
		return -1
	}

	for i in layer ..< layer + number_of_layers {
		count += 1
		smm3 := sm.soil_column.layers[i].soil_moisture_m3
		fc := sm.soil_column.layers[i].field_capacity
		pwp := sm.soil_column.layers[i].permanent_wilting_point
		sum += smm3 / (fc - pwp) //[%nFK]
	}

	return sum / f64(count)
}

// C++: std::pair<double,double> monica::soilmoisture::
//        getSnowDepthAndCalcTemperatureUnderSnow(const SoilMoisture*, double)
get_snow_depth_and_calc_temperature_under_snow :: proc(
	sm: ^Soil_Moisture,
	avgAirTemp: f64,
) -> (
	snowDepth: f64,
	temperatureUnderSnow: f64,
) {
	snowDepth = sm.snow_component.snow_depth
	temperatureUnderSnow = calc_temperature_under_snow(&sm.frost_component, avgAirTemp, snowDepth)
	return
}
