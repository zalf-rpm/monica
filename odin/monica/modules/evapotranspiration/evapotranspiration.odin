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
// Two modules: Monica_Evapotranspiration, MONICA's whole evapotranspiration,
// and Monica_AMEI_potential_Evapotranspiration, just its ET0 and potential ET
// from intermediates (vapour pressures, net radiation, stomata resistance)
// that are inputs here - from outside standalone, from MONICA's own ET0 as a
// driver (core/evapotranspiration_impl.odin). It can't replace MONICA's own,
// which also has to give the evaporation, transpiration and soil water.
//
// A new module (R3): as in monica/modules/soil_temperature, its file with a
// constant Module_Entry at the end, plus one line each in Module and `modules`.
package evapotranspiration

import "base:runtime"

Allocator :: runtime.Allocator

// What the evapotranspiration modules read, in MONICA's units. Layers are top
// down, one per MONICA layer.
Input :: struct {
	// site and parameters, constant over a run
	latitude:                         f64 `unit:"deg"`,
	height_nn:                        f64 `unit:"m"`,
	reference_albedo:                 f64 `unit:"0-1"`, // MONICA's crop module parameter
	evaporation_zeta:                 f64 `unit:"1"`, // MONICA's soil moisture parameters ...
	maximum_evaporation_impact_depth: f64 `unit:"layer#"`,
	xsa_critical_soil_moisture:       f64 `unit:"1"`,
	stomata_conductance_alpha:        f64 `unit:"1"`, // MONICA's crop module parameters
	saturation_beta:                  f64 `unit:"1"`,
	layer_thickness:                  []f64 `unit:"m"`,
	// daily too: MONICA's own reads them from the soil column every day, and
	// a SetValue workstep can change them
	permanent_wilting_point:          []f64 `unit:"m3 m-3"`,
	field_capacity:                   []f64 `unit:"m3 m-3"`,

	using daily:                      Daily,
	soil_moisture:                    []f64 `unit:"m3 m-3"`, // per layer, before the step
	crop_transpiration:               []f64 `unit:"mm"`, // per layer, the crop's
}

// The daily part of Input: what MONICA's own evapotranspiration has when it
// runs.
Daily :: struct {
	julian_day:                         int `unit:"d"`,
	tmin, tavg, tmax:                   f64 `unit:"°C"`,
	globrad:                            f64 `unit:"MJ m-2 d-1"`,
	relative_humidity:                  f64 `unit:"0-1"`, // [0-1], <= 0 without one in the climate data
	wind:                               f64 `unit:"m s-1"`,
	wind_speed_height:                  f64 `unit:"m"`,
	has_external_et0:                   bool `unit:""`,
	external_et0:                       f64 `unit:"mm"`, // the climate data's ET0
	has_external_vapor_pressure:        bool `unit:""`,
	external_vapor_pressure:            f64 `unit:"kPa"`, // the climate data's
	co2:                                f64 `unit:"ppm"`,
	developmental_stage:                int `unit:""`, // the crop's, 0 without one
	carboxylation_pathway:              int `unit:""`, // the crop's
	crop_remaining_evapotranspiration:  f64 `unit:"mm"`,
	crop_evaporated_from_intercept:     f64 `unit:"mm"`,
	gross_photosynthesis_reference_mol: f64 `unit:"µmol m-2 s-1"`,
	soil_coverage:                      f64 `unit:"0-1"`,
	kc_factor:                          f64 `unit:"1"`, // MONICA's current Kc
	snow_depth:                         f64 `unit:"mm"`,
	surface_water_storage:              f64 `unit:"mm"`, // before the step
	// the intermediates of MONICA's ET0 - the crop module's with a crop
	// (developmental_stage > 0), soil moisture's without - which
	// Monica_AMEI_potential_Evapotranspiration takes as inputs
	vapor_pressure:                     f64 `unit:"kPa"`,
	saturated_vapor_pressure:           f64 `unit:"kPa"`,
	stomata_resistance:                 f64 `unit:"s m-1"`,
	net_radiation:                      f64 `unit:"MJ m-2"`,
}

// What MONICA's own evapotranspiration produces.
Output :: struct {
	reference_evapotranspiration:      f64 `unit:"mm"`,
	potential_evapotranspiration:      f64 `unit:"mm"`,
	saturation_vapor_pressure_deficit: f64 `unit:"kPa"`,
	net_radiation:                     f64 `unit:"MJ m-2"`,
	evaporated_from_surface:      f64 `unit:"mm"`,
	actual_evaporation:           f64 `unit:"mm"`,
	actual_transpiration:         f64 `unit:"mm"`,
	actual_evapotranspiration:    f64 `unit:"mm"`,
	surface_water_storage:        f64 `unit:"mm"`, // after the step
	soil_moisture:                []f64 `unit:"m3 m-3"`, // per layer, after the step
	evaporation:                  []f64 `unit:"mm"`, // per layer
	transpiration:                []f64 `unit:"mm"`, // per layer
	evapotranspiration:           []f64 `unit:"mm"`, // per layer
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
	^Monica_Amei_Pot_Et,
}

// A module's table row, as in monica/modules/soil_temperature.
Module_Entry :: struct {
	name:    string, // as in sim.json and the Crop2ML package
	make:    proc(allocator: Allocator) -> Module,
	// in_'s daily values are those of the first day: the modules' init reads
	// them, as in the C++ AMEI adapters, which call init right before the
	// first step
	init:    proc(m: Module, in_: ^Input, allocator: Allocator),
	// `allocator` is for the step's temporaries. A module that sizes state
	// arrays lazily in its first step (ApsimCampbell) gets them from the
	// allocator of that step, so callers pass a lasting one for the first step.
	step:    proc(m: Module, in_: ^Input, out: ^Output, allocator: Allocator),
	destroy: proc(m: Module, allocator: Allocator),
	// whether it can replace MONICA's own (computes everything Output has)
	integrated: bool,
	// the Daily fields it reads (monica-module names those missing in its
	// input), and the Output fields it computes, the others stay 0
	// (monica-module writes only these); nil: all
	inputs:  []string,
	outputs: []string,
	// whether it needs the soil profile (Input's per-layer arrays)
	needs_layers: bool,
}

@(rodata)
modules := [?]Module_Entry {
	monica_et_entry,
	monica_amei_pot_et_entry,
}

find_module :: proc(name: string) -> (Module_Entry, bool) {
	for e in modules {
		if e.name == name {
			return e, true
		}
	}
	return {}, false
}

