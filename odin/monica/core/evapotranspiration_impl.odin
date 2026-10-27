// Evapotranspiration as a swappable process (plan-crop2ml-modules.md, phase
// 5). No C++ counterpart in master; the C++ AMEI branch replaced
// SoilMoisture::fm_Evapotranspiration by a module (soilmoisture.cpp, "if
// (evapotranspiration) evapotranspiration->run();").
//
// The same place here: soil_moisture_step calls MONICA's own
// `evapotranspiration` exactly as before as long as Soil_Moisture.modules is
// nil, i.e. without "Evapotranspiration"/"EvapotranspirationDrivers" in
// sim.json (R1). Otherwise evapotranspiration_with_modules gathers the
// modules' input there, after infiltration and percolation:
//   - integrated (Modules.evapotranspiration), the module's results replace
//     MONICA's own: soil moisture, surface water storage, ET0, the actual
//     evaporation/transpiration/ET and their layers, net radiation
//   - drivers run on the same input and are only reported; MONICA's own (or
//     the integrated module) has changed the state, not they.
//
// The FAO-56 dual Kc pathway (sim.json "evapotranspiration-method") is
// MONICA's own only; check_modules_config refuses it with an integrated module.
package core

import etm "../modules/evapotranspiration"
import p "../params"
import "core:fmt"
import "core:mem"

Evapotranspiration_Module :: struct {
	name:        string,
	entry:       etm.Module_Entry, // its table row: init, step, destroy
	module:      etm.Module,
	input:       etm.Input,
	output:      etm.Output,
	initialized: bool,
}

// Called by make_monica_model after the soil moisture is made; does nothing
// without configured evapotranspiration modules.
make_evapotranspiration_modules :: proc(
	model: ^Monica_Model,
	cpp: ^p.Central_Parameter_Provider,
	allocator := context.allocator,
) {
	mc := &model.sim_params.modules
	impl := mc.evapotranspiration.impl
	integrated := impl != "" && impl != p.MODULE_IMPL_INTERNAL
	if !integrated && len(mc.evapotranspiration_drivers) == 0 {
		return
	}
	ms := &model.modules
	modules_init(ms, allocator)
	ms.model = model
	model.soil_moisture.modules = ms
	if integrated {
		ms.evapotranspiration = make_evapotranspiration_module(model, cpp, impl, allocator)
	}
	names := mc.evapotranspiration_drivers
	ms.evapotranspiration_drivers = make(map[string]^Evapotranspiration_Module, len(names), allocator)
	ms.evapotranspiration_stepping = make([dynamic]^Evapotranspiration_Module, 0, len(names), allocator)
	for name in names {
		d := make_evapotranspiration_module(model, cpp, name, allocator)
		ms.evapotranspiration_drivers[name] = d
		append(&ms.evapotranspiration_stepping, d)
	}
}

@(private = "file")
make_evapotranspiration_module :: proc(
	model: ^Monica_Model,
	cpp: ^p.Central_Parameter_Provider,
	name: string,
	allocator: mem.Allocator,
) -> ^Evapotranspiration_Module {
	entry, ok := etm.find_module(name)
	assert(ok, "unknown evapotranspiration module - check_modules_config wasn't called")
	m := new(Evapotranspiration_Module, allocator)
	m.name = name
	m.entry = entry
	m.module = entry.make(allocator)
	in_ := &m.input
	in_.latitude = model.site_params.latitude
	in_.height_nn = model.site_params.height_nn
	in_.reference_albedo = model.crop_mod_params.reference_albedo
	smp := &cpp.soil_moisture_mod_params
	in_.evaporation_zeta = smp.evaporation_zeta
	in_.maximum_evaporation_impact_depth = smp.maximum_evaporation_impact_depth
	in_.xsa_critical_soil_moisture = smp.xsa_critical_soil_moisture
	in_.stomata_conductance_alpha = model.crop_mod_params.stomata_conductance_alpha
	in_.saturation_beta = model.crop_mod_params.saturation_beta
	n := len(model.soil_column.layers)
	in_.layer_thickness = make([]f64, n, allocator)
	in_.permanent_wilting_point = make([]f64, n, allocator)
	in_.field_capacity = make([]f64, n, allocator)
	for &l, i in model.soil_column.layers {
		in_.layer_thickness[i] = l.layer_thickness_m
		in_.permanent_wilting_point[i] = l.permanent_wilting_point
		in_.field_capacity[i] = l.field_capacity
	}
	in_.soil_moisture = make([]f64, n, allocator)
	in_.crop_transpiration = make([]f64, n, allocator)
	m.output = etm.make_output(n, allocator)
	return m
}

// Instead of soil_moisture_step's call of `evapotranspiration` when modules
// are configured; the same arguments.
evapotranspiration_with_modules :: proc(
	sm: ^Soil_Moisture,
	max_air_temp, min_air_temp, relative_humidity, avg_air_temp: f64,
	wind_speed, wind_speed_height, global_radiation, precipitation: f64,
	developmental_stage, julian_day: int,
	reference_evapotranspiration_mm: f64,
	dual_kc_method: bool,
	daily_sum_irrigation_water_mm: f64,
) {
	ms := sm.modules
	model := ms.model
	sc := sm.soil_column

	// what MONICA's own would get, before anything changes it
	day: etm.Daily
	day.julian_day = julian_day
	day.tmin, day.tavg, day.tmax = min_air_temp, avg_air_temp, max_air_temp
	day.globrad = global_radiation
	day.relative_humidity = relative_humidity
	day.wind = wind_speed
	day.wind_speed_height = wind_speed_height
	day.has_external_et0 = reference_evapotranspiration_mm >= 0.0
	day.external_et0 = reference_evapotranspiration_mm
	if vp, ok := model.climate_data[len(model.climate_data) - 1][.vaporpress]; ok {
		day.has_external_vapor_pressure = true
		day.external_vapor_pressure = vp
	}
	day.co2 = model.atmospheric_co2_concentration
	day.kc_factor = sm.kc_factor
	day.soil_coverage = sm.soil_coverage_percent
	day.snow_depth = sm.snow_component.snow_depth
	day.surface_water_storage = sc.surface_water_storage
	cm := sm.crop_module
	if cm != nil {
		day.developmental_stage = developmental_stage
		day.carboxylation_pathway = cm.crop_params.species_params.carboxylation_pathway
		day.crop_remaining_evapotranspiration = cm.remaining_evapotranspiration
		day.crop_evaporated_from_intercept = cm.evaporated_from_intercept
		day.gross_photosynthesis_reference_mol = cm.gross_photosynthesis_reference_mol
	}
	gather :: proc(m: ^Evapotranspiration_Module, day: ^etm.Daily, sc: ^Soil_Column, cm: ^Crop_Module) {
		m.input.daily = day^
		for &l, i in sc.layers {
			m.input.soil_moisture[i] = l.soil_moisture_m3
			m.input.permanent_wilting_point[i] = l.permanent_wilting_point
			m.input.field_capacity[i] = l.field_capacity
		}
		if cm != nil {
			copy(m.input.crop_transpiration, cm.transpiration[:])
		} else {
			for &t in m.input.crop_transpiration {
				t = 0
			}
		}
	}
	for d in ms.evapotranspiration_stepping {
		gather(d, &day, sc, cm)
	}

	if m := ms.evapotranspiration; m != nil {
		gather(m, &day, sc, cm)
		evapotranspiration_module_step(ms, m)
		// the module's results are MONICA's
		o := &m.output
		for &l, i in sc.layers {
			l.soil_moisture_m3 = o.soil_moisture[i]
			sm.evaporation[i] = o.evaporation[i]
			sm.transpiration[i] = o.transpiration[i]
			sm.evapotranspiration[i] = o.evapotranspiration[i]
		}
		sc.surface_water_storage = o.surface_water_storage
		sm.reference_evapotranspiration = o.reference_evapotranspiration
		sm.net_radiation = o.net_radiation
		sm.evaporated_from_surface = o.evaporated_from_surface
		sm.actual_evaporation = o.actual_evaporation
		sm.actual_transpiration = o.actual_transpiration
		sm.actual_evapotranspiration = o.actual_evapotranspiration
	} else {
		evapotranspiration(
			sm,
			max_air_temp,
			min_air_temp,
			relative_humidity,
			avg_air_temp,
			wind_speed,
			wind_speed_height,
			global_radiation,
			precipitation,
			developmental_stage,
			julian_day,
			reference_evapotranspiration_mm,
			dual_kc_method,
			daily_sum_irrigation_water_mm,
		)
	}

	for d in ms.evapotranspiration_stepping {
		evapotranspiration_module_step(ms, d)
	}
}

@(private = "file")
evapotranspiration_module_step :: proc(ms: ^Modules, m: ^Evapotranspiration_Module) {
	if !m.initialized {
		m.entry.init(m.module, &m.input, ms.allocator)
		m.initialized = true
		m.entry.step(m.module, &m.input, &m.output, ms.allocator) // see soil_temperature_module_step
		return
	}
	m.entry.step(m.module, &m.input, &m.output, mem.dynamic_arena_allocator(&ms.scratch))
	mem.dynamic_arena_reset(&ms.scratch)
}

// Part of check_modules_config.
check_evapotranspiration_config :: proc(
	mc: ^p.Modules_Config,
	dual_kc_method: bool,
	errs: ^[dynamic]string,
	allocator := context.allocator,
) {
	names := make([dynamic]string, context.temp_allocator)
	for e in etm.modules {
		append(&names, e.name)
	}
	impl := mc.evapotranspiration.impl
	if impl != "" && impl != p.MODULE_IMPL_INTERNAL {
		if e, ok := etm.find_module(impl); !ok {
			append(
				errs,
				fmt.aprintf(
					"Error: Modules.Evapotranspiration: unknown impl %q (known: %v)",
					impl,
					names[:],
					allocator = allocator,
				),
			)
		} else if !e.integrated {
			append(
				errs,
				fmt.aprintf(
					"Error: Modules.Evapotranspiration: %q can only run as a driver",
					impl,
					allocator = allocator,
				),
			)
		} else if dual_kc_method {
			append(
				errs,
				fmt.aprintf(
					"Error: Modules.Evapotranspiration: %q has no FAO-56 dual Kc (evapotranspiration-method)",
					impl,
					allocator = allocator,
				),
			)
		}
	}
	for name, i in mc.evapotranspiration_drivers {
		if _, ok := etm.find_module(name); !ok {
			append(
				errs,
				fmt.aprintf(
					"Error: Modules.EvapotranspirationDrivers: unknown module %q (known: %v)",
					name,
					names[:],
					allocator = allocator,
				),
			)
		}
		for other in mc.evapotranspiration_drivers[:i] {
			if other == name {
				append(
					errs,
					fmt.aprintf(
						"Error: Modules.EvapotranspirationDrivers: %q is listed twice",
						name,
						allocator = allocator,
					),
				)
				break
			}
		}
	}
}
