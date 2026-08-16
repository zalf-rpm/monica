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
	soilColumn:             ^Soil_Column,
	soilColumnGroundLayer:  Soil_Layer,
	soilColumnBottomLayer:  Soil_Layer,
	params:                 p.Soil_Temperature_Module_Parameters,

	noOfTempLayers:         int,
	noOfSoilLayers:         int,
	soilTemperature:        [dynamic]f64,
	V:                      [dynamic]f64,
	volumeMatrix:           [dynamic]f64,
	volumeMatrixOld:        [dynamic]f64,
	B:                      [dynamic]f64,
	matrixPrimaryDiagonal:  [dynamic]f64,
	matrixSecondaryDiagonal: [dynamic]f64,
	heatConductivity:       [dynamic]f64,
	heatConductivityMean:   [dynamic]f64,
	heatCapacity:           [dynamic]f64,
	dampingFactor:          f64,
	soilSurfaceTemperature: f64,
	solution:               [dynamic]f64,
	matrixDiagonal:         [dynamic]f64,
	matrixLowerTriangle:    [dynamic]f64,
	heatFlow:               [dynamic]f64,
}

// C++: (anonymous namespace) SoilLayer &soilTemperatureLayerAt(SoilTemperature*, size_t)
@(private)
soil_temperature_layer_at :: proc(st: ^Soil_Temperature, i: int) -> ^Soil_Layer {
	if i < st.noOfSoilLayers {
		return &st.soilColumn.layers[i]
	}
	if i < st.noOfSoilLayers + 1 {
		return &st.soilColumnGroundLayer
	}
	return &st.soilColumnBottomLayer
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
	st.dampingFactor = 0.8 // C++ in-class initialiser: double dampingFactor{0.8}
	st.soilColumn = soil_column
	st.params = params
	st.noOfTempLayers = len(st.soilColumn.layers) + 2
	st.noOfSoilLayers = len(st.soilColumn.layers)
	resize(&st.soilTemperature, st.noOfTempLayers)
	resize(&st.V, st.noOfTempLayers)
	resize(&st.volumeMatrix, st.noOfTempLayers)
	resize(&st.volumeMatrixOld, st.noOfTempLayers)
	resize(&st.B, st.noOfTempLayers)
	resize(&st.matrixPrimaryDiagonal, st.noOfTempLayers)
	resize(&st.matrixSecondaryDiagonal, st.noOfTempLayers + 1)
	resize(&st.heatConductivity, st.noOfTempLayers)
	resize(&st.heatConductivityMean, st.noOfTempLayers)
	resize(&st.heatCapacity, st.noOfTempLayers)
	resize(&st.solution, st.noOfTempLayers)
	resize(&st.matrixDiagonal, st.noOfTempLayers)
	resize(&st.matrixLowerTriangle, st.noOfTempLayers)
	resize(&st.heatFlow, st.noOfTempLayers)
	for i in 0 ..< len(st.heatFlow) {
		st.heatFlow[i] = 0.0
	}

	if len(st.soilColumn.layers) > 0 {
		last := st.soilColumn.layers[len(st.soilColumn.layers) - 1]
		st.soilColumnGroundLayer = last
		st.soilColumnBottomLayer = last
	}

	soil_moisture_const := st.params.pt_SoilMoisture

	base_temp := st.params.pt_BaseTemperature
	initial_surface_temp := st.params.pt_InitialSurfaceTemperature
	soil_nols := st.noOfSoilLayers

	for i in 0 ..< soil_nols {
		st.soilTemperature[i] =
			((1.0 - (f64(i) / f64(soil_nols))) * initial_surface_temp) +
			((f64(i) / f64(soil_nols)) * base_temp)
	}

	ground_layer := st.noOfTempLayers - 2
	bottom_layer := st.noOfTempLayers - 1
	soil_temperature_layer_at(&st, ground_layer).vs_LayerThickness =
		2.0 * soil_temperature_layer_at(&st, ground_layer - 1).vs_LayerThickness
	soil_temperature_layer_at(&st, bottom_layer).vs_LayerThickness = 1.0
	st.soilTemperature[ground_layer] = (st.soilTemperature[ground_layer - 1] + base_temp) * 0.5
	st.soilTemperature[bottom_layer] = base_temp

	st.V[0] = soil_temperature_layer_at(&st, 0).vs_LayerThickness
	st.B[0] = 2.0 / soil_temperature_layer_at(&st, 0).vs_LayerThickness
	ntau := st.params.pt_NTau
	for i in 1 ..< st.noOfTempLayers {
		lti_1 := soil_temperature_layer_at(&st, i - 1).vs_LayerThickness
		lti := soil_temperature_layer_at(&st, i).vs_LayerThickness
		st.B[i] = 2.0 / (lti + lti_1)
		st.V[i] = lti * ntau
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

	for i in 0 ..< st.noOfSoilLayers {
		sbdi := soil_bulk_density(soil_temperature_layer_at(&st, i))
		smi := soil_moisture_const
		st.heatConductivity[i] =
			((3.0 * (sbdi / 1000.0) - 1.7) * 0.001) /
			(1.0 +
					(11.5 - 5.0 * (sbdi / 1000.0)) *
						libc.exp((-50.0) * libc.pow((smi / (sbdi / 1000.0)), 1.5))) *
			86400.0 *
			ts *
			100.0 *
			4.184

		sati := soil_temperature_layer_at(&st, i).vs_Saturation
		somi := soil_organic_matter(soil_temperature_layer_at(&st, i)) / da * sbdi
		st.heatCapacity[i] =
			(smi * dw * cw) + ((sati - smi) * da * ca) + (somi * dh * ch) +
			((1.0 - sati - somi) * dq * cq)
	}

	st.heatCapacity[ground_layer] = st.heatCapacity[ground_layer - 1]
	st.heatCapacity[bottom_layer] = st.heatCapacity[ground_layer]
	st.heatConductivity[ground_layer] = st.heatConductivity[ground_layer - 1]
	st.heatConductivity[bottom_layer] = st.heatConductivity[ground_layer]
	st.soilSurfaceTemperature = initial_surface_temp

	st.heatConductivityMean[0] = st.heatConductivity[0]
	for i in 1 ..< st.noOfTempLayers {
		lti_1 := soil_temperature_layer_at(&st, i - 1).vs_LayerThickness
		lti := soil_temperature_layer_at(&st, i).vs_LayerThickness
		hci_1 := st.heatConductivity[i - 1]
		hci := st.heatConductivity[i]
		st.heatConductivityMean[i] = ((lti_1 * hci_1) + (lti * hci)) / (lti + lti_1)
	}

	for i in 0 ..< st.noOfTempLayers {
		st.volumeMatrix[i] = st.V[i] * st.heatCapacity[i]
		st.volumeMatrixOld[i] = st.volumeMatrix[i]
		st.matrixSecondaryDiagonal[i] = -st.B[i] * st.heatConductivityMean[i]
	}

	st.matrixSecondaryDiagonal[bottom_layer + 1] = 0.0

	for i in 0 ..< st.noOfTempLayers {
		st.matrixPrimaryDiagonal[i] =
			st.volumeMatrix[i] - st.matrixSecondaryDiagonal[i] - st.matrixSecondaryDiagonal[i + 1]
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
	ground_layer := st.noOfTempLayers - 2
	bottom_layer := st.noOfTempLayers - 1

	st.soilSurfaceTemperature = calc_soil_surface_temperature(
		st,
		st.soilSurfaceTemperature,
		tmin,
		tmax,
		globrad,
		soil_coverage,
		snow_depth,
		temperature_under_snow,
	)
	st.soilColumn.vt_SoilSurfaceTemperature = st.soilSurfaceTemperature
	st.heatFlow[0] = st.soilSurfaceTemperature * st.B[0] * st.heatConductivityMean[0]

	for i in 0 ..< st.noOfTempLayers {
		st.solution[i] =
			(st.volumeMatrixOld[i] +
					(st.volumeMatrix[i] - st.volumeMatrixOld[i]) /
						soil_temperature_layer_at(st, i).vs_LayerThickness) *
				st.soilTemperature[i] +
			st.heatFlow[i]
	}

	st.matrixDiagonal[0] = st.matrixPrimaryDiagonal[0]
	for i in 1 ..< st.noOfTempLayers {
		st.matrixLowerTriangle[i] = st.matrixSecondaryDiagonal[i] / st.matrixDiagonal[i - 1]
		st.matrixDiagonal[i] =
			st.matrixPrimaryDiagonal[i] - (st.matrixLowerTriangle[i] * st.matrixSecondaryDiagonal[i])
	}

	for i in 1 ..< st.noOfTempLayers {
		st.solution[i] = st.solution[i] - (st.matrixLowerTriangle[i] * st.solution[i - 1])
	}

	st.solution[bottom_layer] = st.solution[bottom_layer] / st.matrixDiagonal[bottom_layer]
	for i in 0 ..< bottom_layer {
		j := (bottom_layer - 1) - i
		j_1 := j + 1
		st.solution[j] =
			(st.solution[j] / st.matrixDiagonal[j]) - (st.matrixLowerTriangle[j_1] * st.solution[j_1])
	}

	for i in 0 ..< st.noOfTempLayers {
		st.soilTemperature[i] = st.solution[i]
	}

	for i in 0 ..< st.noOfSoilLayers {
		st.volumeMatrixOld[i] = st.volumeMatrix[i]
		soil_temperature_layer_at(st, i).vs_SoilTemperature = st.soilTemperature[i]
	}

	st.volumeMatrixOld[ground_layer] = st.volumeMatrix[ground_layer]
	st.volumeMatrixOld[bottom_layer] = st.volumeMatrix[bottom_layer]
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
		((soil_coverage * st.dampingFactor) + ((1 - soil_coverage) * (1 - st.dampingFactor)))

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
