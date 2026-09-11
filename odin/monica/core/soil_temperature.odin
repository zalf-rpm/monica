// Port of src/core/soiltemperature.{h,cpp}: SoilTemperature, makeSoilTemperature,
// soiltemperature::step, soiltemperature::calcSoilSurfaceTemperature.
//
// Deliberate deviation from the C++ struct: no `monica: *MonicaModel` field.
// SoilTemperature only ever reads three things through that back-pointer:
//   - makeSoilTemperature (constructor): monica.envPs.p_timeStep
//   - calcSoilSurfaceTemperature: monica.currentCropModule ? ->vc_SoilCoverage : 0.0,
//     monica.soilMoisture->snowComponent->vm_SnowDepth,
//     monica.soilMoisture->frostComponent->vm_TemperatureUnderSnow
// MonicaModel (phase 6) and CropModule (phase 5) don't exist yet, and SoilMoisture
// is a phase-4 sibling not yet ported either. Rather than invent a placeholder
// MonicaModel now - the plan flags that ownership/back-pointer layout as a
// decision every later phase copies, "wrong choice = rewrite" - these three
// values are threaded as explicit parameters instead, the same move checkpoint
// 3a made for pathToSoilDir ("needed only at merge time... keeps merge
// self-contained and testable without a side-channel setup step"). Phase 6's
// real orchestration call site becomes a mechanical
// `soiltemperature.step(st, tmin, tmax, globrad, model.currentCropModule != nil
// ? model.currentCropModule.vc_SoilCoverage : 0, model.soilMoisture.snowComponent
// .vm_SnowDepth, model.soilMoisture.frostComponent.vm_TemperatureUnderSnow)` -
// not a redesign.
package core

import libc "core:c/libc"
import p "../params"

// C++: struct monica::SoilTemperature
Soil_Temperature :: struct {
	soil_column:               ^Soil_Column,
	soil_column_ground_layer:  Soil_Layer,
	soil_column_bottom_layer:  Soil_Layer,
	params:                    p.Soil_Temperature_Module_Parameters,

	no_of_temp_layers:         int,
	no_of_soil_layers:         int,
	soil_temperature:          [dynamic]f64,
	v:                         [dynamic]f64,
	volume_matrix:             [dynamic]f64,
	volume_matrix_old:         [dynamic]f64,
	b:                         [dynamic]f64,
	matrix_primary_diagonal:   [dynamic]f64,
	matrix_secondary_diagonal: [dynamic]f64,
	heat_conductivity:         [dynamic]f64,
	heat_conductivity_mean:    [dynamic]f64,
	heat_capacity:             [dynamic]f64,
	damping_factor:            f64,
	soil_surface_temperature:  f64,
	solution:                  [dynamic]f64,
	matrix_diagonal:           [dynamic]f64,
	matrix_lower_triangle:     [dynamic]f64,
	heat_flow:                 [dynamic]f64,
}

// C++: (anonymous namespace) SoilLayer &soilTemperatureLayerAt(SoilTemperature*, size_t)
@(private)
soil_temperature_layer_at :: proc(st: ^Soil_Temperature, i: int) -> ^Soil_Layer {
	if i < st.no_of_soil_layers {
		return &st.soil_column.layers[i]
	}
	if i < st.no_of_soil_layers + 1 {
		return &st.soil_column_ground_layer
	}
	return &st.soil_column_bottom_layer
}

// C++: kj::Own<SoilTemperature> monica::makeSoilTemperature(MonicaModel&,
//        const SoilTemperatureModuleParameters&)
//
// Takes soil_column/p_time_step directly instead of a MonicaModel& - see the
// package comment. Returns by value rather than a heap-owned pointer, matching
// make_soil_column's precedent (phase 6 is when back-pointers into this start
// mattering).
make_soil_temperature :: proc(
	soil_column: ^Soil_Column,
	params: p.Soil_Temperature_Module_Parameters,
	p_time_step: f64,
) -> Soil_Temperature {
	st: Soil_Temperature
	st.damping_factor = 0.8 // C++ in-class initialiser: double damping_factor{0.8}
	st.soil_column = soil_column
	st.params = params
	st.no_of_temp_layers = len(st.soil_column.layers) + 2
	st.no_of_soil_layers = len(st.soil_column.layers)
	resize(&st.soil_temperature, st.no_of_temp_layers)
	resize(&st.v, st.no_of_temp_layers)
	resize(&st.volume_matrix, st.no_of_temp_layers)
	resize(&st.volume_matrix_old, st.no_of_temp_layers)
	resize(&st.b, st.no_of_temp_layers)
	resize(&st.matrix_primary_diagonal, st.no_of_temp_layers)
	resize(&st.matrix_secondary_diagonal, st.no_of_temp_layers + 1)
	resize(&st.heat_conductivity, st.no_of_temp_layers)
	resize(&st.heat_conductivity_mean, st.no_of_temp_layers)
	resize(&st.heat_capacity, st.no_of_temp_layers)
	resize(&st.solution, st.no_of_temp_layers)
	resize(&st.matrix_diagonal, st.no_of_temp_layers)
	resize(&st.matrix_lower_triangle, st.no_of_temp_layers)
	resize(&st.heat_flow, st.no_of_temp_layers)
	for i in 0 ..< len(st.heat_flow) {
		st.heat_flow[i] = 0.0
	}

	if len(st.soil_column.layers) > 0 {
		last := st.soil_column.layers[len(st.soil_column.layers) - 1]
		st.soil_column_ground_layer = last
		st.soil_column_bottom_layer = last
	}

	soil_moisture_const := st.params.pt_SoilMoisture

	base_temp := st.params.pt_BaseTemperature
	initial_surface_temp := st.params.pt_InitialSurfaceTemperature
	soil_nols := st.no_of_soil_layers

	for i in 0 ..< soil_nols {
		st.soil_temperature[i] =
			((1.0 - (f64(i) / f64(soil_nols))) * initial_surface_temp) +
			((f64(i) / f64(soil_nols)) * base_temp)
	}

	ground_layer := st.no_of_temp_layers - 2
	bottom_layer := st.no_of_temp_layers - 1
	soil_temperature_layer_at(&st, ground_layer).layer_thickness =
		2.0 * soil_temperature_layer_at(&st, ground_layer - 1).layer_thickness
	soil_temperature_layer_at(&st, bottom_layer).layer_thickness = 1.0
	st.soil_temperature[ground_layer] = (st.soil_temperature[ground_layer - 1] + base_temp) * 0.5
	st.soil_temperature[bottom_layer] = base_temp

	st.v[0] = soil_temperature_layer_at(&st, 0).layer_thickness
	st.b[0] = 2.0 / soil_temperature_layer_at(&st, 0).layer_thickness
	ntau := st.params.pt_NTau
	for i in 1 ..< st.no_of_temp_layers {
		lti_1 := soil_temperature_layer_at(&st, i - 1).layer_thickness
		lti := soil_temperature_layer_at(&st, i).layer_thickness
		st.b[i] = 2.0 / (lti + lti_1)
		st.v[i] = lti * ntau
	}

	ts := p_time_step
	dw := st.params.pt_DensityWater
	cw := st.params.pt_SpecificHeatCapacityWater
	dq := st.params.pt_QuartzRawDensity
	cq := st.params.pt_SpecificHeatCapacityQuartz
	da := st.params.pt_DensityAir
	ca := st.params.pt_SpecificHeatCapacityAir
	dh := st.params.pt_DensityHumus
	ch := st.params.pt_SpecificHeatCapacityHumus

	for i in 0 ..< st.no_of_soil_layers {
		sbdi := soil_bulk_density(soil_temperature_layer_at(&st, i))
		smi := soil_moisture_const
		st.heat_conductivity[i] =
			((3.0 * (sbdi / 1000.0) - 1.7) * 0.001) /
			(1.0 +
					(11.5 - 5.0 * (sbdi / 1000.0)) *
						libc.exp((-50.0) * libc.pow((smi / (sbdi / 1000.0)), 1.5))) *
			86400.0 *
			ts *
			100.0 *
			4.184

		sati := soil_temperature_layer_at(&st, i).saturation
		somi := soil_organic_matter(soil_temperature_layer_at(&st, i)) / da * sbdi
		st.heat_capacity[i] =
			(smi * dw * cw) + ((sati - smi) * da * ca) + (somi * dh * ch) +
			((1.0 - sati - somi) * dq * cq)
	}

	st.heat_capacity[ground_layer] = st.heat_capacity[ground_layer - 1]
	st.heat_capacity[bottom_layer] = st.heat_capacity[ground_layer]
	st.heat_conductivity[ground_layer] = st.heat_conductivity[ground_layer - 1]
	st.heat_conductivity[bottom_layer] = st.heat_conductivity[ground_layer]
	st.soil_surface_temperature = initial_surface_temp

	st.heat_conductivity_mean[0] = st.heat_conductivity[0]
	for i in 1 ..< st.no_of_temp_layers {
		lti_1 := soil_temperature_layer_at(&st, i - 1).layer_thickness
		lti := soil_temperature_layer_at(&st, i).layer_thickness
		hci_1 := st.heat_conductivity[i - 1]
		hci := st.heat_conductivity[i]
		st.heat_conductivity_mean[i] = ((lti_1 * hci_1) + (lti * hci)) / (lti + lti_1)
	}

	for i in 0 ..< st.no_of_temp_layers {
		st.volume_matrix[i] = st.v[i] * st.heat_capacity[i]
		st.volume_matrix_old[i] = st.volume_matrix[i]
		st.matrix_secondary_diagonal[i] = -st.b[i] * st.heat_conductivity_mean[i]
	}

	st.matrix_secondary_diagonal[bottom_layer + 1] = 0.0

	for i in 0 ..< st.no_of_temp_layers {
		st.matrix_primary_diagonal[i] =
			st.volume_matrix[i] - st.matrix_secondary_diagonal[i] - st.matrix_secondary_diagonal[i + 1]
	}

	return st
}

// C++: void monica::soiltemperature::step(SoilTemperature*, double tmin, double
//        tmax, double globrad)
//
// Named soil_temperature_step, not step: this package mirrors src/core/ (one
// Odin package per C++ *directory*, not per C++ *namespace* - see
// CONVENTIONS.md §2), so soiltemperature::step and soilmoisture::step would
// otherwise collide. soil_coverage/snow_depth/temperature_under_snow stand in
// for the C++'s st->monica->... reads inside calcSoilSurfaceTemperature - see
// the package comment.
soil_temperature_step :: proc(
	st: ^Soil_Temperature,
	tmin, tmax, globrad: f64,
	soil_coverage, snow_depth, temperature_under_snow: f64,
) {
	ground_layer := st.no_of_temp_layers - 2
	bottom_layer := st.no_of_temp_layers - 1

	st.soil_surface_temperature = calc_soil_surface_temperature(
		st,
		st.soil_surface_temperature,
		tmin,
		tmax,
		globrad,
		soil_coverage,
		snow_depth,
		temperature_under_snow,
	)
	st.soil_column.vt_SoilSurfaceTemperature = st.soil_surface_temperature
	st.heat_flow[0] = st.soil_surface_temperature * st.b[0] * st.heat_conductivity_mean[0]

	for i in 0 ..< st.no_of_temp_layers {
		st.solution[i] =
			(st.volume_matrix_old[i] +
					(st.volume_matrix[i] - st.volume_matrix_old[i]) /
						soil_temperature_layer_at(st, i).layer_thickness) *
				st.soil_temperature[i] +
			st.heat_flow[i]
	}

	st.matrix_diagonal[0] = st.matrix_primary_diagonal[0]
	for i in 1 ..< st.no_of_temp_layers {
		st.matrix_lower_triangle[i] = st.matrix_secondary_diagonal[i] / st.matrix_diagonal[i - 1]
		st.matrix_diagonal[i] =
			st.matrix_primary_diagonal[i] - (st.matrix_lower_triangle[i] * st.matrix_secondary_diagonal[i])
	}

	for i in 1 ..< st.no_of_temp_layers {
		st.solution[i] = st.solution[i] - (st.matrix_lower_triangle[i] * st.solution[i - 1])
	}

	st.solution[bottom_layer] = st.solution[bottom_layer] / st.matrix_diagonal[bottom_layer]
	for i in 0 ..< bottom_layer {
		j := (bottom_layer - 1) - i
		j_1 := j + 1
		st.solution[j] =
			(st.solution[j] / st.matrix_diagonal[j]) - (st.matrix_lower_triangle[j_1] * st.solution[j_1])
	}

	for i in 0 ..< st.no_of_temp_layers {
		st.soil_temperature[i] = st.solution[i]
	}

	for i in 0 ..< st.no_of_soil_layers {
		st.volume_matrix_old[i] = st.volume_matrix[i]
		soil_temperature_layer_at(st, i).soil_temperature = st.soil_temperature[i]
	}

	st.volume_matrix_old[ground_layer] = st.volume_matrix[ground_layer]
	st.volume_matrix_old[bottom_layer] = st.volume_matrix[bottom_layer]
}

// C++: double monica::soiltemperature::calcSoilSurfaceTemperature(const
//        SoilTemperature*, double prevDaySoilSurfaceTemperature, double tmin,
//        double tmax, double globrad)
calc_soil_surface_temperature :: proc(
	st: ^Soil_Temperature,
	prev_day_soil_surface_temperature, tmin, tmax, globrad_: f64,
	soil_coverage, snow_depth, temperature_under_snow: f64,
) -> f64 {
	globrad := max(8.33, globrad_)

	shading_coefficient :=
		0.1 +
		((soil_coverage * st.damping_factor) + ((1 - soil_coverage) * (1 - st.damping_factor)))

	soil_surface_temperature :=
		(1.0 - shading_coefficient) * (tmin + ((tmax - tmin) * libc.pow((0.03 * globrad), 0.5))) +
		shading_coefficient * prev_day_soil_surface_temperature

	if soil_surface_temperature < 0.0 {
		soil_surface_temperature = soil_surface_temperature * 0.5
	}

	if snow_depth > 0.0 {
		soil_surface_temperature = temperature_under_snow
	}

	return soil_surface_temperature
}
