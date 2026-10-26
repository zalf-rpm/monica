// Evapotranspiration modules: Crop2ML packages (amei/<package>/) behind one
// interface, like monica/modules/soil_temperature (plan-crop2ml-modules.md
// §§1.2-1.3, phase 5).
//
// The process is the part of MONICA's soil moisture step that the C++ AMEI
// branch made replaceable: soil_moisture.odin's `evapotranspiration` (C++
// fm_Evapotranspiration), after infiltration and percolation, before the
// capillary rise. Unlike soil temperature it changes MONICA's state: the soil
// moisture and the surface water storage come in and go out again.
//
// Only Monica_Evapotranspiration so far. Monica_AMEI_potential_Evapotranspiration
// computes ET0 and the potential ET from intermediates (vapour pressures, net
// radiation, stomata resistance) that MONICA only has as locals, in two
// different places (soil moisture without a crop, the crop module with one),
// so it has no binding yet.
package evapotranspiration

// What the evapotranspiration modules read, in MONICA's units. Layers are top
// down, one per MONICA layer.
Input :: struct {
	// site and parameters, constant over a run
	latitude:                         f64, // [deg]
	height_nn:                        f64, // [m]
	reference_albedo:                 f64, // MONICA's crop module parameter
	evaporation_zeta:                 f64, // MONICA's soil moisture parameters ...
	maximum_evaporation_impact_depth: f64, // [layers]
	xsa_critical_soil_moisture:       f64,
	stomata_conductance_alpha:        f64, // MONICA's crop module parameters
	saturation_beta:                  f64,
	layer_thickness:                  []f64, // [m]
	permanent_wilting_point:          []f64, // [m3 m-3]
	field_capacity:                   []f64, // [m3 m-3]

	using daily:                      Daily,
	soil_moisture:                    []f64, // per layer, before the step [m3 m-3]
	crop_transpiration:               []f64, // per layer, the crop's [mm]
}

// The daily part of Input: what MONICA's own evapotranspiration has when it
// runs.
Daily :: struct {
	julian_day:                         int,
	tmin, tavg, tmax:                   f64, // [°C]
	globrad:                            f64, // [MJ m-2 d-1]
	relative_humidity:                  f64, // [0-1], <= 0 without one in the climate data
	wind:                               f64, // [m s-1]
	wind_speed_height:                  f64, // [m]
	has_external_et0:                   bool,
	external_et0:                       f64, // the climate data's ET0 [mm]
	has_external_vapor_pressure:        bool,
	external_vapor_pressure:            f64, // the climate data's [kPa]
	co2:                                f64, // [ppm]
	developmental_stage:                int, // the crop's, 0 without one
	carboxylation_pathway:              int, // the crop's
	crop_remaining_evapotranspiration:  f64, // [mm]
	crop_evaporated_from_intercept:     f64, // [mm]
	gross_photosynthesis_reference_mol: f64,
	soil_coverage:                      f64, // [0-1]
	kc_factor:                          f64, // MONICA's current Kc
	snow_depth:                         f64, // [mm]
	surface_water_storage:              f64, // before the step [mm]
}

// What MONICA's own evapotranspiration produces.
Output :: struct {
	reference_evapotranspiration: f64, // [mm]
	potential_evapotranspiration: f64, // [mm]
	net_radiation:                f64, // [MJ m-2]
	evaporated_from_surface:      f64, // [mm]
	actual_evaporation:           f64, // [mm]
	actual_transpiration:         f64, // [mm]
	actual_evapotranspiration:    f64, // [mm]
	surface_water_storage:        f64, // after the step [mm]
	soil_moisture:                []f64, // per layer, after the step [m3 m-3]
	evaporation:                  []f64, // per layer [mm]
	transpiration:                []f64, // per layer [mm]
	evapotranspiration:           []f64, // per layer [mm]
}

make_output :: proc(no_of_layers: int, allocator := context.allocator) -> Output {
	return Output {
		soil_moisture = make([]f64, no_of_layers, allocator),
		evaporation = make([]f64, no_of_layers, allocator),
		transpiration = make([]f64, no_of_layers, allocator),
		evapotranspiration = make([]f64, no_of_layers, allocator),
	}
}

Module :: union {
	^Monica_Et,
}

Module_Entry :: struct {
	name:       string, // as in sim.json and the Crop2ML package
	make:       proc(allocator := context.allocator) -> Module,
	// whether it can replace MONICA's own (computes everything Output has)
	integrated: bool,
}

@(rodata)
modules := [?]Module_Entry{{"Monica_Evapotranspiration", make_monica_et, true}}

find_module :: proc(name: string) -> (Module_Entry, bool) {
	for e in modules {
		if e.name == name {
			return e, true
		}
	}
	return {}, false
}

module_init :: proc(m: Module, in_: ^Input, allocator := context.allocator) -> (err: string) {
	if len(in_.layer_thickness) == 0 {
		return "evapotranspiration module: no soil layers"
	}
	switch b in m {
	case ^Monica_Et:
		monica_et_init(b, in_, allocator)
	}
	return ""
}

module_step :: proc(m: Module, in_: ^Input, out: ^Output, allocator := context.allocator) {
	switch b in m {
	case ^Monica_Et:
		monica_et_step(b, in_, out, allocator)
	}
}

module_destroy :: proc(m: Module, allocator := context.allocator) {
	switch b in m {
	case ^Monica_Et:
		destroy_monica_et(b, allocator)
	}
}
