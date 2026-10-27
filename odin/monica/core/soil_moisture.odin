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
import "../../support/cr_math"

STOMATA_RESISTANCE :: 100 // FAO default value [s m-1]

// C++: struct monica::SoilMoisture
Soil_Moisture :: struct {
	// NOTE(deviation): the evapotranspiration modules, nil without
	// (evapotranspiration_impl.odin)
	modules:                      ^Modules,
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
	resize(&sm.transpiration, no_of_mois_layers)

	sm.layer_thickness_m = layer_thickness
	sm.leaching_depth_layer_idx =
		int(libc.floor(0.5 + (sm.env_params.leaching_depth_m / layer_thickness))) - 1

	initialize_snow_component(&sm.snow_component, sc, &sm.mod_params)
	initialize_frost_component(
		&sm.frost_component,
		sc,
		sm.mod_params.hydraulic_conductivity_redux,
		sm.env_params.time_step,
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
	max_air_temp_deg_c,
	min_air_temp_deg_c,
	relative_humidity_perc,
	avg_air_temp_deg_c,
	wind_speed_m_per_s,
	wind_speed_height_m,
	global_radiation_mj_per_m2: f64,
	julian_day: int,
	reference_evapotranspiration_mm: f64,
	dual_kc_method: bool = false,
	daily_sum_irrigation_water_mm: f64 = 0.0,
	crop_cleared := false, // the crop is harvested, gone tomorrow (Monica_Model.clear_crop_upon_next_day)
) {
	sc := sm.soil_column

	for i in 0 ..< sm.no_of_soil_layers {
		sc.layers[i].soil_water_flux = 0.0
	}

	sm.soil_moisture_below_m3_m3 = sc.layers[sm.no_of_soil_layers - 1].soil_moisture_m3
	sm.water_flux_below = 0.0
	sm.field_capacity_below_m3_m3 = sc.layers[sm.no_of_soil_layers - 1].field_capacity
	sm.pore_volume_below_m3_m3 = sc.layers[sm.no_of_soil_layers - 1].saturation
	sm.lambda_below = sc.layers[sm.no_of_soil_layers - 1].lambda

	dev_stage := 0
	// C++: `sm->monica.currentCropModule.get()` - see the package comment.
	// C++: if (monica.isCropPlanted() && !monica.clearCropUponNextDay()): a
	// harvested crop, whose step no longer runs, is no crop here
	if sm.crop_module != nil && !crop_cleared {
		sm.soil_coverage_percent = sm.crop_module.soil_coverage
		sm.kc_factor = sm.crop_module.kc_factor
		dev_stage = sm.crop_module.developmental_stage
		if dev_stage > 0 {
			sm.net_precipitation_mm = sm.crop_module.net_precipitation
		} else {
			sm.net_precipitation_mm = precipitation_mm
		}
	} else {
		sm.kc_factor = sm.mod_params.kc_factor
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

	oscill_ground_water_layer := int(groundwater_depth_m / sc.layers[0].layer_thickness_m)
	if (sm.groundwater_table_layer > oscill_ground_water_layer &&
		   sm.groundwater_table_layer < sm.no_of_soil_layers + 2) ||
	   sm.groundwater_table_layer >= sm.no_of_soil_layers + 2 {
		sm.groundwater_table_layer = oscill_ground_water_layer
	}

	sc.groundwater_table_layer_idx = sm.groundwater_table_layer

	// calculates snow layer water storage and release
	calc_snow_layer(&sm.snow_component, avg_air_temp_deg_c, sm.net_precipitation_mm)

	// Calculates frost and thaw depth and switches lambda
	calc_soil_frost(&sm.frost_component, avg_air_temp_deg_c, sm.snow_component.snow_depth)

	// calculates infiltration of water from surface
	infiltration(sm, sm.snow_component.water_to_infiltrate)

	if 0.0 < groundwater_depth_m && groundwater_depth_m <= 10.0 {
		percolation_with_groundwater(sm, oscill_ground_water_layer)
		groundwater_replenishment(sm)
	} else {
		percolation_without_groundwater(sm)
		backwater_replenishment(sm)
	}

	// NOTE(deviation): evapotranspiration modules (evapotranspiration_impl.odin)
	if sm.modules == nil {
		evapotranspiration(
			sm,
			max_air_temp_deg_c,
			min_air_temp_deg_c,
			relative_humidity_perc,
			avg_air_temp_deg_c,
			wind_speed_m_per_s,
			wind_speed_height_m,
			global_radiation_mj_per_m2,
			precipitation_mm,
			dev_stage,
			julian_day,
			reference_evapotranspiration_mm,
			dual_kc_method,
			daily_sum_irrigation_water_mm,
		)
	} else {
		evapotranspiration_with_modules(
			sm,
			max_air_temp_deg_c,
			min_air_temp_deg_c,
			relative_humidity_perc,
			avg_air_temp_deg_c,
			wind_speed_m_per_s,
			wind_speed_height_m,
			global_radiation_mj_per_m2,
			precipitation_mm,
			dev_stage,
			julian_day,
			reference_evapotranspiration_mm,
			dual_kc_method,
			daily_sum_irrigation_water_mm,
		)
	}

	capillary_rise(sm)
}

// C++: void monica::soilmoisture::infiltration(SoilMoisture*, double)
infiltration :: proc(sm: ^Soil_Moisture, water_to_infiltrate: f64) {
	sc := sm.soil_column
	scl_0 := &sc.layers[0]

	sm.infiltration = 0.0
	sm.interception = 0.0
	sm.surface_run_off = 0.0
	sm.groundwater_added = 0.0
	sm.actual_transpiration = 0.0

	surface_water_storage_old := sc.surface_water_storage

	sc.surface_water_storage += water_to_infiltrate

	sm.soil_moisture_deficit = (scl_0.saturation - scl_0.soil_moisture_m3) / scl_0.saturation
	reduced_hydraulic_conductivity :=
		sm.mod_params.saturated_hydraulic_conductivity *
		sm.mod_params.hydraulic_conductivity_redux

	if reduced_hydraulic_conductivity > 0.0 {
		potential_infiltration :=
			reduced_hydraulic_conductivity *
			0.2 *
			sm.soil_moisture_deficit *
			sm.soil_moisture_deficit

		sm.infiltration = min(sc.surface_water_storage, potential_infiltration)

		sm.infiltration = min(
			sm.infiltration,
			((scl_0.saturation - scl_0.soil_moisture_m3) * 1000.0 * sm.layer_thickness_m),
		)

		sm.infiltration = max(0.0, sm.infiltration)
	} else {
		sm.infiltration = 0.0
	}

	if sm.infiltration > 0.0 {
		sc.surface_water_storage -= sm.infiltration
	}

	if sc.surface_water_storage >
	   (10.0 * sm.mod_params.surface_roughness / (sm.site_params.slope + 0.001)) {
		run_off_factor :=
			0.02 + (sm.mod_params.surface_roughness / 4.0) + (sm.soil_coverage_percent / 15.0)
		if sm.site_params.slope < 0.0 || sm.site_params.slope > 1.0 {
			fmt.eprintln("Slope value out ouf boundary")
		} else if sm.site_params.slope == 0.0 {
			sm.surface_run_off = 0.0
		} else if sm.site_params.slope > run_off_factor {
			sm.surface_run_off += sc.surface_water_storage
		} else {
			sm.surface_run_off +=
				((sm.site_params.slope * run_off_factor) /
					(run_off_factor * run_off_factor)) *
				sc.surface_water_storage
		}

		sc.surface_water_storage -= sm.surface_run_off
	}

	scl_0.soil_moisture_m3 += (sm.infiltration / 1000.0 / sm.layer_thickness_m)

	scl_0.soil_water_flux = sm.infiltration

	if scl_0.soil_moisture_m3 > scl_0.field_capacity {
		sm.gravitational_water[0] =
			(scl_0.soil_moisture_m3 - scl_0.field_capacity) * 1000.0 * sm.layer_thickness_m
		lambda_reduced := scl_0.lambda * sm.frost_component.lambda_redux[0]
		percolation_factor := 1 + lambda_reduced * sm.gravitational_water[0]
		sm.percolation_rate[0] =
			(sm.gravitational_water[0] * sm.gravitational_water[0] * lambda_reduced) /
			percolation_factor
		if sm.percolation_rate[0] > sm.mod_params.max_percolation_rate {
			sm.percolation_rate[0] = sm.mod_params.max_percolation_rate
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
					(scl_0.soil_moisture_m3 - scl_0.saturation) * 1000.0 * sm.layer_thickness_m
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
		   (surface_water_storage_old + water_to_infiltrate) -
		   (sm.surface_run_off + sm.infiltration + sc.surface_water_storage),
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

	rooting_depth := sm.crop_module != nil ? sm.crop_module.rooting_depth : 0

	// NOTE(c++-quirk): C++ computes this as size_t arithmetic
	// (max(size_t(1), groundwater_table_layer - vc_RootingDepth)): if
	// vc_RootingDepth ever exceeds groundwater_table_layer the subtraction
	// wraps to a huge positive number rather than going negative, reproduced
	// here with explicit uint arithmetic. Dormant while cropModule is nil
	// (vc_RootingDepth always 0); matters once phase 5 plants a crop whose
	// roots reach below a shallow water table.
	groundwater_distance := max(
		uint(1),
		uint(sm.groundwater_table_layer) - uint(rooting_depth),
	)

	if f64(groundwater_distance) * sm.layer_thickness_m <= 2.70 {
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
			capillary_rise_rate := min(
				0.01,
				soil.capillary_rise_rates_get_rate(
					cr,
					scl_i.soil_texture,
					int(groundwater_distance),
				),
			)
			if sm.available_water[i] < sm.capillary_water70[i] {
				water_added_from_capillary_rise := capillary_rise_rate
				scl_i.soil_moisture_m3 += water_added_from_capillary_rise / sm.layer_thickness_m
				for j := start_layer_idx; j >= i; j -= 1 {
					sc.layers[j].soil_water_flux -= water_added_from_capillary_rise * 1000.0
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
	groundwater_discharge := sm.mod_params.groundwater_discharge

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

				lambda_reduced := lambda_ib^ * sm.frost_component.lambda_redux[ib]
				percolation_factor := 1 + lambda_reduced * sm.gravitational_water[ib]
				sm.percolation_rate[ib] =
					(sm.gravitational_water[ib] * sm.gravitational_water[ib] * lambda_reduced) /
					percolation_factor

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
		sc.flux_at_lower_boundary = sm.water_flux_below
	} else {
		sc.flux_at_lower_boundary = sc.layers[sm.leaching_depth_layer_idx].soil_water_flux
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
			sm.percolation_rate[i] = sm.mod_params.groundwater_discharge
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
				sc.surface_water_storage += sm.groundwater_added
				sm.groundwater_added = 0.0
			}
		} else {
			sm.groundwater_added = 0.0
		}
	}

	if sm.leaching_depth_layer_idx > sm.groundwater_table_layer - 1 {
		if sm.groundwater_table_layer - 1 < 0 {
			sc.flux_at_lower_boundary = 0.0
		} else {
			if sm.groundwater_table_layer - 1 == sm.no_of_soil_layers {
				sc.flux_at_lower_boundary = sm.water_flux_below
			} else {
				sc.flux_at_lower_boundary =
					sc.layers[sm.groundwater_table_layer - 1].soil_water_flux
			}
		}
	} else {
		if sm.leaching_depth_layer_idx == sm.no_of_soil_layers {
			sc.flux_at_lower_boundary = sm.water_flux_below
		} else {
			sc.flux_at_lower_boundary = sc.layers[sm.leaching_depth_layer_idx].soil_water_flux
		}
	}
}

// C++: void monica::soilmoisture::percolationWithoutGroundwater(SoilMoisture*)
percolation_without_groundwater :: proc(sm: ^Soil_Moisture) {
	max_percolation_rate := sm.mod_params.max_percolation_rate
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
			lambda_reduced := lambda_ib^ * sm.frost_component.lambda_redux[ib]
			percolation_factor := 1.0 + (lambda_reduced * sm.gravitational_water[ib])
			sm.percolation_rate[ib] =
				(sm.gravitational_water[ib] * sm.gravitational_water[ib] * lambda_reduced) /
				percolation_factor

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
		sc.flux_at_lower_boundary = sc.layers[sm.leaching_depth_layer_idx].soil_water_flux
	} else {
		sc.flux_at_lower_boundary = sc.layers[sm.no_of_soil_layers - 1].soil_water_flux
	}
}

// C++: void monica::soilmoisture::backwaterReplenishment(SoilMoisture*)
backwater_replenishment :: proc(sm: ^Soil_Moisture) {
	start_layer_idx := sm.no_of_soil_layers
	backwater_table := sm.no_of_soil_layers
	backwater_added := 0.0

	sc := sm.soil_column

	// find first layer from top where the water content exceeds pore volume
	for i in 0 ..< sm.no_of_soil_layers {
		scl_i := &sc.layers[i]
		if scl_i.soil_moisture_m3 > scl_i.saturation {
			start_layer_idx = i
			backwater_table = i
		}
	}

	// if there is no such thing nothing will happen
	if backwater_table == 0 {
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

		soil_moisture_i^ += backwater_added / 1000.0 / sm.layer_thickness_m
		if i > 0 {
			sc.layers[i - 1].soil_water_flux -= backwater_added
		}

		if soil_moisture_i^ > pore_volume_i^ {
			backwater_added = (soil_moisture_i^ - pore_volume_i^) * 1000.0 * sm.layer_thickness_m
			soil_moisture_i^ = pore_volume_i^
			backwater_table -= 1 // Backwater table rises

			if i == 0 && backwater_table == 0 {
				// if backwater reaches surface
				sc.surface_water_storage += backwater_added
				backwater_added = 0.0
			}
		} else {
			backwater_added = 0.0
		}
	}
}

// C++: double monica::soilmoisture::dualKcPrecomputation(SoilMoisture*, double,
//        double, double)
//
// Only ever called when sm.crop_module != nil (see evapotranspiration's
// useDualKc guard), matching the C++'s unchecked `cropModule->` derefs below.
// daily_sum_irrigation_water stands in for `sm->monica.dailySumIrrigationWater`
// - see the package comment.
dual_kc_precomputation :: proc(
	sm: ^Soil_Moisture,
	wind_speed, tmin, tmax, precip: f64,
	daily_sum_irrigation_water: f64,
) -> f64 {
	e_pot_dual_kc := 0.0 // [mm d-1] replaces (1-beta)*PET per layer

	// ET0 is already in vm_ReferenceEvapotranspiration [mm d-1]
	et0 := sm.reference_evapotranspiration

	// --- Kcb: basal crop coefficient (interpolated in crop module) ---
	kcb := sm.crop_module.kcb_factor

	// --- Calculate Depletion first (FAO-56 §8.3) to inform memory logic ---
	scl_0 := &sm.soil_column.layers[0]
	fc0 := scl_0.field_capacity
	wp0 := scl_0.permanent_wilting_point
	swc0 := scl_0.soil_moisture_m3
	ze := 0.1 // evaporation depth [m], FAO-56 typical top layer
	tew := 1000.0 * (fc0 - 0.5 * wp0) * ze

	// A. Dynamic REW (FAO-56 Table 19 Mapping via Pedology)
	//
	// NOTE(c++-quirk): these comparisons are against mixed-case strings
	// ("Ss", "Su2", ...) while real KA5 textures in this port's fixtures are
	// always uppercase ("SS", "SU2", ...) - so none of these branches ever
	// match on real data and REW always falls through to the FC0-based
	// fallback below. Reproduced as-is, not fixed.
	rew := 0.0
	ka5_texture := scl_0.soil_texture

	switch {
	case ka5_texture == "Ss":
		rew = 2.5
	case ka5_texture == "Su2" || ka5_texture == "Sl2":
		rew = 3.5
	case ka5_texture == "Su3" || ka5_texture == "Sl3":
		rew = 4.5
	case ka5_texture == "Su4" || ka5_texture == "Sl4" || ka5_texture == "St2":
		rew = 5.5
	case ka5_texture == "St3" || ka5_texture == "Ls2" || ka5_texture == "Us2":
		rew = 8.0
	case ka5_texture == "Uu" ||
	     ka5_texture == "Us3" ||
	     ka5_texture == "Us4" ||
	     ka5_texture == "Ul2" ||
	     ka5_texture == "Ul3" ||
	     ka5_texture == "Ul4":
		rew = 8.5
	case ka5_texture == "Ls3" ||
	     ka5_texture == "Ls4" ||
	     ka5_texture == "Lu2" ||
	     ka5_texture == "Lu3" ||
	     ka5_texture == "Lu4":
		rew = 9.0
	case ka5_texture == "Ut2" || ka5_texture == "Ut3":
		rew = 9.5
	case ka5_texture == "Ut4" ||
	     ka5_texture == "Lt2" ||
	     ka5_texture == "Lt3" ||
	     ka5_texture == "Lts":
		rew = 10.5
	case ka5_texture == "Ts2" ||
	     ka5_texture == "Ts3" ||
	     ka5_texture == "Ts4" ||
	     ka5_texture == "Tu2" ||
	     ka5_texture == "Tu3" ||
	     ka5_texture == "Tu4" ||
	     ka5_texture == "Tl":
		rew = 11.5
	case ka5_texture == "Tt":
		rew = 12.0
	}

	// Fallback if KA5 lookup is unavailable or fails:
	if rew == 0.0 {
		if fc0 < 0.18 {
			// Coarse soils (Sand, Sandy Loams): REW ranges from 2.0 to 7.0 mm
			rew = 2.5 + 25.0 * (fc0 - 0.05)
			rew = max(2.0, min(rew, 7.0))
		} else if fc0 >= 0.28 {
			// Fine soils (Clays): REW ranges from 8.0 to 12.0 mm
			rew = 8.0 + 20.0 * (fc0 - 0.28)
			rew = max(8.0, min(rew, 12.0))
		} else {
			rew = 8.0
		}
	}
	rew = min(rew, tew) // Hard boundary safety clamp

	// Current depletion De [mm] = what the top layer is missing compared to FC
	de := 1000.0 * max(0.0, fc0 - swc0) * ze

	// --- Memory state update for wetting events ---
	// precip := sm.vm_GrossPrecipitation
	irrig_applied := daily_sum_irrigation_water
	if precip > 0.0 {
		sm.last_wetting_was_rain = true
	} else if irrig_applied > 0.0 {
		sm.last_wetting_was_rain = false
	}

	// --- fw: fraction of wetted soil surface (event-level, FAO-56 §8.3) ---
	fw_today: f64
	if precip > 0.0 {
		fw_today = 1.0 // rain wets the full surface
	} else if de <= rew && sm.last_wetting_was_rain {
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
	kr := 1.0
	if de > rew {
		if (tew - rew) > 0.0 {
			kr = (tew - de) / (tew - rew)
		} else {
			kr = 0.0
		}
	}
	kr = max(0.0, min(kr, 1.0))

	// B. Rigorous Climatic Kc_max (FAO-56 Equation 72)
	u2 := wind_speed > 0.0 ? wind_speed : 2.0
	eo_tmax := 0.6108 * cr_math.exp((17.27 * tmax) / (tmax + 237.3))
	eo_tmin := 0.6108 * cr_math.exp((17.27 * tmin) / (tmin + 237.3))
	rh_min := max(5.0, min(100.0, (eo_tmin / eo_tmax) * 100.0))
	baseline := 1.2 // FAO-56 §6 default for most crops
	h := max(0.01, sm.crop_module.crop_height) // native simulated height [m]
	kc_max := baseline + (0.04 * (u2 - 2.0) - 0.004 * (rh_min - 45.0)) * cr_math.pow(h / 3.0, 0.3)
	kc_max = max(kc_max, kcb + 0.05)

	// C. few: fraction of exposed and wetted soil (FAO-56 Eq. 74)
	fc := max(0.0, min(sm.soil_coverage_percent, 0.99))
	few := min(1.0 - fc, fw_adj)
	few = max(0.001, few) // guard against zero denominator

	// --- Ke: soil evaporation coefficient (FAO-56 eq. 71) ---
	ke := kr * (kc_max - kcb)
	ke = min(ke, few * kc_max)
	ke = max(0.0, ke)

	// Potential soil evaporation [mm d-1]
	e_pot_dual_kc = et0 * ke
	// Respect the hard cap from HERMES (6.5 mm/day applies to total, cap E too)
	e_pot_dual_kc = min(e_pot_dual_kc, 6.5)

	sm.ke = ke

	return e_pot_dual_kc
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
	avg_air_temp_deg_c,
	wind_speed_m_per_s,
	wind_speed_height_m,
	globral_radiation_mj_per_m2,
	precip_mm: f64,
	developmental_stage: int,
	julian_day: int,
	reference_evapotranspiration_mm: f64,
	dual_kc_method: bool,
	daily_sum_irrigation_water_mm: f64,
) {
	sc := sm.soil_column

	e_reducer_1 := 0.0
	e_reducer_2 := 0.0
	e_reducer_3 := 0.0
	e_reducer := 0.0
	potential_evapotranspiration := 0.0
	evaporated_from_intercept := 0.0
	sm.evaporated_from_surface = 0.0
	evaporation_from_surface := false

	snow_depth := sm.snow_component.snow_depth

	// Berechnung der Bodenevaporation bis max. 4dm Tiefe
	evaporation_zeta := sm.mod_params.evaporation_zeta

	sm.xsa_critical_soil_moisture = sm.mod_params.xsa_critical_soil_moisture

	maximum_evaporation_impact_depth := sm.mod_params.maximum_evaporation_impact_depth

	// If a crop grows, ETp is taken from crop module
	if developmental_stage > 0 {
		// C++: `monica.currentCropModule.get()` - see the package comment.
		if reference_evapotranspiration_mm < 0.0 {
			sm.reference_evapotranspiration = sm.crop_module.reference_evapotranspiration
		} else {
			sm.reference_evapotranspiration = reference_evapotranspiration_mm
		}

		potential_evapotranspiration = sm.crop_module.remaining_evapotranspiration
		evaporated_from_intercept = sm.crop_module.evaporated_from_intercept
	} else { 	// if no crop grows ETp is calculated from ET0 * kc
		if reference_evapotranspiration_mm < 0.0 {
			sm.reference_evapotranspiration = reference_evapotranspiration(
				sm,
				max_air_temp_deg,
				min_air_temp_deg,
				relative_humidity_perc,
				avg_air_temp_deg_c,
				wind_speed_m_per_s,
				wind_speed_height_m,
				globral_radiation_mj_per_m2,
				julian_day,
			)
		} else {
			sm.reference_evapotranspiration = reference_evapotranspiration_mm
		}

		potential_evapotranspiration = sm.reference_evapotranspiration * sm.kc_factor
	}

	sm.actual_evaporation = 0.0
	sm.actual_transpiration = 0.0

	// from HERMES:
	if potential_evapotranspiration > 6.5 {
		potential_evapotranspiration = 6.5
	}

	if potential_evapotranspiration > 0.0 {
		// If surface is water-logged, subsequent evaporation from surface water sources
		if sc.surface_water_storage > 0.0 {
			evaporation_from_surface = true
			// Water surface evaporates with Kc = 1.1.
			potential_evapotranspiration = potential_evapotranspiration * (1.1 / sm.kc_factor)

			// If a snow layer is present no water evaporates from surface water sources
			if snow_depth > 0.0 {
				sm.evaporated_from_surface = 0.0
			} else {
				if sc.surface_water_storage < potential_evapotranspiration {
					potential_evapotranspiration -= sc.surface_water_storage
					sm.evaporated_from_surface = sc.surface_water_storage
					sc.surface_water_storage = 0.0
				} else {
					sc.surface_water_storage -= potential_evapotranspiration
					sm.evaporated_from_surface = potential_evapotranspiration
					potential_evapotranspiration = 0.0
				}
			}
			potential_evapotranspiration = potential_evapotranspiration * (sm.kc_factor / 1.1)
		}

		if potential_evapotranspiration > 0 { 	// Evaporation from soil
			// -----------------------------------------------------------------
			// FAO-56 Dual Kc pathway - precompute potential soil evaporation
			// (E_pot) using the Ke coefficient. Runs ONLY when dualKcMethod is
			// enabled, a crop is present (vc_DevelopmentalStage > 0), and
			// sm.crop_module != nil. Otherwise the Single Kc loop runs unchanged.
			// -----------------------------------------------------------------
			e_pot_dual_kc := 0.0
			use_dual_kc := dual_kc_method && developmental_stage > 0 && sm.crop_module != nil
			if use_dual_kc {
				e_pot_dual_kc = dual_kc_precomputation(
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

			for i_layer in 0 ..< sm.no_of_soil_layers {
				e_reducer_1 = get_e_reducer_1(
					sm,
					i_layer,
					sm.soil_coverage_percent,
					potential_evapotranspiration,
				)

				scl_i := &sc.layers[i_layer]

				if f64(i_layer) >= maximum_evaporation_impact_depth {
					// layer is too deep for evaporation
					e_reducer_2 = 0.0
				} else {
					e_reducer_2 = get_deprivation_factor(
						i_layer + 1,
						maximum_evaporation_impact_depth,
						evaporation_zeta,
						sm.layer_thickness_m,
					)
				}

				if i_layer > 0 {
					if scl_i.soil_moisture_m3 < sc.layers[i_layer - 1].soil_moisture_m3 {
						e_reducer_3 = 0.1
					} else {
						e_reducer_3 = 1.0
					}
				} else {
					e_reducer_3 = 1.0
				}
				e_reducer = e_reducer_1 * e_reducer_2 * e_reducer_3

				if developmental_stage > 0 {
					// vegetation is present
					if use_dual_kc {
						sm.evaporation[i_layer] = e_reducer * e_pot_dual_kc
					} else {
						// Single Kc: original (1 - beta) * EReducer * PET partitioning
						if sm.soil_coverage_percent >= 0.0 && sm.soil_coverage_percent < 1.0 {
							sm.evaporation[i_layer] =
								((1.0 - sm.soil_coverage_percent) * e_reducer) *
								potential_evapotranspiration
						} else {
							if sm.soil_coverage_percent >= 1.0 {
								sm.evaporation[i_layer] = 0.0
							}
						}
					}

					if snow_depth > 0.0 {
						sm.evaporation[i_layer] = 0.0
					}

					// C++: `monica.currentCropModule.get()->vc_Transpiration` - see the package comment.
					sm.transpiration[i_layer] = sm.crop_module.transpiration[i_layer]

					// Transpiration is capped in case potential ET after surface
					// and interception evaporation has occurred on same day
					if evaporation_from_surface {
						sm.transpiration[i_layer] =
							sm.soil_coverage_percent * e_reducer * potential_evapotranspiration
					}
				} else {
					// no vegetation present - Single Kc / bare soil, always unchanged
					if snow_depth > 0.0 {
						sm.evaporation[i_layer] = 0.0
					} else {
						sm.evaporation[i_layer] = potential_evapotranspiration * e_reducer
					}
					sm.transpiration[i_layer] = 0.0
				}

				sm.evapotranspiration[i_layer] =
					sm.evaporation[i_layer] + sm.transpiration[i_layer]
				scl_i.soil_moisture_m3 -=
					(sm.evapotranspiration[i_layer] / 1000.0 / sm.layer_thickness_m)

				// Generelle Begrenzung des Evaporationsentzuges
				if scl_i.soil_moisture_m3 < 0.01 {
					scl_i.soil_moisture_m3 = 0.01
				}

				sm.actual_transpiration += sm.transpiration[i_layer]
				sm.actual_evaporation += sm.evaporation[i_layer]
			}
		}
	}
	sm.actual_evapotranspiration =
		sm.actual_transpiration +
		sm.actual_evaporation +
		evaporated_from_intercept +
		sm.evaporated_from_surface
}

// C++: double monica::soilmoisture::referenceEvapotranspiration(SoilMoisture*,
//        double, double, double, double, double, double, double, double, int,
//        double)
//
// Penman-Monteith method, FAO Irrigation and Drainage Paper 56.
reference_evapotranspiration :: proc(
	sm: ^Soil_Moisture,
	max_air_temperature,
	min_air_temperature,
	relative_humidity,
	mean_air_temperature,
	wind_speed,
	wind_speed_height,
	global_radiation: f64,
	julian_day: int,
) -> f64 {
	pi := 3.14159265358979323

	declination := -23.4 * cr_math.cos(2.0 * pi * ((f64(julian_day) + 10.0) / 365.0))
	declination_sinus :=
		cr_math.sin(declination * pi / 180.0) * cr_math.sin(sm.site_params.latitude * pi / 180.0)
	declination_cosinus :=
		cr_math.cos(declination * pi / 180.0) * cr_math.cos(sm.site_params.latitude * pi / 180.0)

	arg_astro_day_length := declination_sinus / declination_cosinus
	// The argument of asin must be in the range of -1 to 1
	arg_astro_day_length = tl.bound(-1.0, arg_astro_day_length, 1.0)
	astronomic_day_lenght := 12.0 * (pi + 2.0 * cr_math.asin(arg_astro_day_length)) / pi

	arg_effective_day_length :=
		(-cr_math.sin(8.0 * pi / 180.0) + declination_sinus) / declination_cosinus
	arg_effective_day_length = tl.bound(-1.0, arg_effective_day_length, 1.0)
	effective_day_lenght := 12.0 * (pi + 2.0 * cr_math.asin(arg_effective_day_length)) / pi

	arg_photo_day_length :=
		(-cr_math.sin(-6.0 * pi / 180.0) + declination_sinus) / declination_cosinus
	arg_photo_day_length = tl.bound(-1.0, arg_photo_day_length, 1.0)
	photoperiodic_daylength := 12.0 * (pi + 2.0 * cr_math.asin(arg_photo_day_length)) / pi
	_ = effective_day_lenght
	_ = photoperiodic_daylength

	arg_phot_act := min(
		1.0,
		((declination_sinus / declination_cosinus) *
			(declination_sinus / declination_cosinus)),
	)
	// The argument of sqrt must be >= 0
	phot_act_radiation_mean :=
		3600.0 *
		(declination_sinus * astronomic_day_lenght +
				24.0 / pi * declination_cosinus * libc.sqrt(1.0 - arg_phot_act))

	clear_day_radiation := 0.0
	if phot_act_radiation_mean > 0 && astronomic_day_lenght > 0 {
		clear_day_radiation =
			0.5 *
			1300.0 *
			phot_act_radiation_mean *
			cr_math.exp(-0.14 / (phot_act_radiation_mean / (astronomic_day_lenght * 3600.0)))
	}
	// vc_OvercastDayRadiation, like vc_EffectiveDayLenght/vc_PhotoperiodicDaylength/
	// vm_AerodynamicResistance below, is computed by the C++ but never read
	// afterward (dead in the C++ itself) - kept for literal fidelity.
	overcast_day_radiation := 0.2 * clear_day_radiation
	_ = overcast_day_radiation

	// C++: written as in the crop module's radiation, so both give the same bits
	solar_constant := 0.082
	sc := 24.0 * 60.0 / pi * solar_constant * (1.0 + 0.033 * cr_math.cos(2.0 * pi * f64(julian_day) / 365.0))
	arg_sha := tl.bound(
		-1.0,
		-cr_math.tan(sm.site_params.latitude * pi / 180.0) * cr_math.tan(declination * pi / 180.0),
		1.0,
	)
	// The argument of acos must be in the range of -1 to 1
	sha := cr_math.acos(arg_sha)

	// [J cm-2] --> [MJ m-2]
	extraterrestrial_radiation :=
		sc * (sha * declination_sinus + declination_cosinus * cr_math.sin(sha)) // [MJ m-2]

	// Calculation of atmospheric pressure
	atmospheric_pressure :=
		101.3 * cr_math.pow(((293.0 - (0.0065 * sm.site_params.height_nn)) / 293.0), 5.26)

	// Calculation of psychrometer constant - Luftfeuchtigkeit
	psycrometer_constant := 0.000665 * atmospheric_pressure

	// Calc. of saturated water vapour pressure at daily max temperature
	saturated_vapour_pressure_max :=
		0.6108 * cr_math.exp((17.27 * max_air_temperature) / (237.3 + max_air_temperature))

	// Calc. of saturated water vapour pressure at daily min temperature
	saturated_vapour_pressure_min :=
		0.6108 * cr_math.exp((17.27 * min_air_temperature) / (237.3 + min_air_temperature))

	// Calculation of the saturated water vapour pressure
	saturated_vapour_pressure :=
		(saturated_vapour_pressure_max + saturated_vapour_pressure_min) / 2.0

	// Calculation of the water vapour pressure
	vapour_pressure: f64
	if relative_humidity <= 0.0 {
		// Assuming Tdew = Tmin as suggested in FAO56 Allen et al. 1998
		vapour_pressure = saturated_vapour_pressure_min
	} else {
		vapour_pressure = relative_humidity * saturated_vapour_pressure
	}

	// Calculation of the air saturation deficit
	saturation_deficit := saturated_vapour_pressure - vapour_pressure

	// Slope of saturation water vapour pressure-to-temperature relation
	saturated_vapour_pressure_slope :=
		(4098.0 *
			(0.6108 *
					cr_math.exp((17.27 * mean_air_temperature) / (mean_air_temperature + 237.3)))) /
		((mean_air_temperature + 237.3) * (mean_air_temperature + 237.3))

	// Calculation of wind speed in 2m height
	// 0.5 minimum allowed windspeed for Penman-Monteith-Method FAO
	wind_speed_2m := max(0.5, wind_speed * (4.87 / cr_math.log(67.8 * wind_speed_height - 5.42)))

	// Calculation of the aerodynamic resistance
	aerodynamic_resistance := 208.0 / wind_speed_2m

	surface_resistance := STOMATA_RESISTANCE / 1.44

	clear_sky_solar_radiation :=
		(0.75 + 0.00002 * sm.site_params.height_nn) * extraterrestrial_radiation
	relative_shortwave_radiation :=
		clear_sky_solar_radiation > 0 ? min(global_radiation / clear_sky_solar_radiation, 1.0) : 1.0

	bolzmann_constant := 0.0000000049
	shortwave_radiation := (1.0 - sm.crop_mod_params.reference_albedo) * global_radiation
	longwave_radiation :=
		bolzmann_constant *
		((cr_math.pow(min_air_temperature + 273.16, 4.0) +
					cr_math.pow(max_air_temperature + 273.16, 4.0)) /
				2.0) *
		(1.35 * relative_shortwave_radiation - 0.35) *
		(0.34 - 0.14 * libc.sqrt(vapour_pressure))
	sm.net_radiation = shortwave_radiation - longwave_radiation

	// Calculation of the reference evapotranspiration - Penman-Monteith-Methode FAO
	ref_evapotranspiration :=
		((0.408 * saturated_vapour_pressure_slope * sm.net_radiation) +
			(psycrometer_constant *
					(900.0 / (mean_air_temperature + 273.0)) *
					wind_speed_2m *
					saturation_deficit)) /
		(saturated_vapour_pressure_slope +
				psycrometer_constant * (1.0 + (surface_resistance / aerodynamic_resistance)))

	if ref_evapotranspiration < 0.0 {
		ref_evapotranspiration = 0.0
	}

	return ref_evapotranspiration
}

// C++: double monica::soilmoisture::getEReducer1(const SoilMoisture*, int,
//        double, double)
get_e_reducer_1 :: proc(
	sm: ^Soil_Moisture,
	i_layer: int,
	percentage_soil_coverage, reference_evapotranspiration: f64,
) -> f64 {
	e_reduction_factor: f64
	evaporation_reduction_method := 1
	// C++: vm_SoilMoisture[i_Layer] - the current moisture, after the day's
	// infiltration and percolation. C++ used to read the soil column here,
	// which it only writes back at the end of the step, so it saw the
	// moisture of the start of the day (fixed in C++ master and AMEI).
	soil_moisture_m3 := sm.soil_column.layers[i_layer].soil_moisture_m3
	pwp := sm.soil_column.layers[i_layer].permanent_wilting_point
	fk := sm.soil_column.layers[i_layer].field_capacity
	relative_evaporable_water: f64
	critical_soil_moisture: f64
	xsa: f64
	reducer: f64

	if soil_moisture_m3 < (0.33 * pwp) {
		soil_moisture_m3 = 0.33 * pwp
	}

	relative_evaporable_water = (soil_moisture_m3 - (0.33 * pwp)) / (fk - (0.33 * pwp))

	if relative_evaporable_water > 1.0 {
		relative_evaporable_water = 1.0
	}

	if evaporation_reduction_method == 0 {
		// THESEUS
		critical_soil_moisture = 0.65 * fk
		if percentage_soil_coverage > 0 {
			if reference_evapotranspiration > 2.5 {
				xsa = (0.65 * fk - pwp) * (fk - pwp)
				reducer =
					xsa + (((1 - xsa) / 17.5) * (reference_evapotranspiration - 2.5))
			} else {
				reducer = sm.xsa_critical_soil_moisture / 2.5 * reference_evapotranspiration
			}
			critical_soil_moisture = sm.soil_column.layers[i_layer].field_capacity * reducer
		}

		// Calculation of an evaporation-reducing factor in relation to soil water content
		if soil_moisture_m3 > critical_soil_moisture {
			e_reduction_factor = 1.0
		} else {
			if soil_moisture_m3 > (0.33 * pwp) {
				e_reduction_factor = relative_evaporable_water
			} else {
				e_reduction_factor = 0.0
			}
		}
	} else if evaporation_reduction_method == 1 {
		// HERMES
		e_reduction_factor = 0.0
		if relative_evaporable_water > 0.33 {
			e_reduction_factor = 1.0 - (0.1 * (1.0 - relative_evaporable_water) / (1.0 - 0.33))
		} else if relative_evaporable_water > 0.22 {
			e_reduction_factor =
				0.9 - (0.625 * (0.33 - relative_evaporable_water) / (0.33 - 0.22))
		} else if relative_evaporable_water > 0.2 {
			e_reduction_factor =
				0.275 - (0.225 * (0.22 - relative_evaporable_water) / (0.22 - 0.2))
		} else {
			e_reduction_factor = 0.05 - (0.05 * (0.2 - relative_evaporable_water) / 0.2)
		}
	}
	return e_reduction_factor
}

// C++: double monica::soilmoisture::getDeprivationFactor(int, double, double, double)
get_deprivation_factor :: proc(
	layer_no: int,
	deprivation_depth, zeta, layer_thickness: f64,
) -> f64 {
	deprivation_factor: f64

	// factor to introduce layer thickness in this algorithm, to allow layer
	// thickness scaling (Claas Nendel)
	layer_thickness_factor := deprivation_depth / (layer_thickness * 10.0)

	if libc.fabs(zeta) < 0.0003 {
		deprivation_factor =
			(2.0 / layer_thickness_factor) -
			(1.0 / (layer_thickness_factor * layer_thickness_factor)) * f64(2 * layer_no - 1)
		return deprivation_factor
	} else {
		c2 := cr_math.log(
			(layer_thickness_factor + zeta * f64(layer_no)) /
			(layer_thickness_factor + zeta * f64(layer_no - 1)),
		)
		c3 := zeta / (layer_thickness_factor * (zeta + 1.0))
		deprivation_factor = (c2 - c3) / (cr_math.log(zeta + 1.0) - zeta / (zeta + 1.0))
		return deprivation_factor
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
		lsum += sm.soil_column.layers[i].layer_thickness_m
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
	avg_air_temp: f64,
) -> (
	snow_depth: f64,
	temperature_under_snow: f64,
) {
	snow_depth = sm.snow_component.snow_depth
	temperature_under_snow = calc_temperature_under_snow(&sm.frost_component, avg_air_temp, snow_depth)
	return
}
