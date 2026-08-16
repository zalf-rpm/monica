// Port of src/core/soilmoisture.{h,cpp}: SoilMoisture, make_soil_moisture,
// initialize_from_params, step, and every soilmoisture:: proc.
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

import libc "core:c/libc"
import "core:fmt"
import p "../params"
import "../soil"
import tl "../../support/tools"

// C++: struct monica::SoilMoisture
Soil_Moisture :: struct {
	vm_EvaporatedFromSurface:  f64,
	soilColumn:                ^Soil_Column,
	siteParameters:            ^p.Site_Parameters,
	params:                    p.Soil_Moisture_Module_Parameters,
	envPs:                     ^p.Environment_Parameters,
	cropPs:                    ^p.Crop_Module_Parameters,
	numberOfMoistureLayers:    int,
	numberOfSoilLayers:        int,

	vm_ActualEvaporation:          f64,
	vm_ActualEvapotranspiration:   f64,
	vm_ActualTranspiration:        f64,
	vm_AvailableWater:             [dynamic]f64,
	vm_CapillaryRise:              f64,
	pm_CapillaryRiseRate:          [dynamic]f64,
	vm_CapillaryWater:             [dynamic]f64,
	vm_CapillaryWater70:           [dynamic]f64,
	vm_Evaporation:                [dynamic]f64,
	vm_Evapotranspiration:         [dynamic]f64,
	vm_FieldCapacity:              [dynamic]f64,
	vm_FluxAtLowerBoundary:        f64,
	vm_GravitationalWater:         [dynamic]f64,
	vm_GrossPrecipitation:         f64,
	vm_GroundwaterAdded:           f64,
	vm_GroundwaterDischarge:       f64,
	vm_GroundwaterTableLayer:      int, // C++ size_t
	vm_HeatConductivity:           [dynamic]f64,
	vm_HydraulicConductivityRedux: f64,
	vm_Infiltration:               f64,
	vm_Interception:               f64,
	vc_KcFactor:                   f64,
	vm_Lambda:                     [dynamic]f64,
	vs_Latitude:                   f64,
	vm_LayerThickness:             [dynamic]f64,
	pm_LayerThickness:             f64,
	pm_LeachingDepth:              f64,
	pm_LeachingDepthLayer:         int,
	pm_MaxPercolationRate:         f64,
	vc_NetPrecipitation:           f64,
	vm_LastWettingWasRain:         bool,
	vm_Ke:                         f64,
	vm_irrigFwEvent:               f64,
	vm_irrigIsDripEvent:           bool,
	vw_NetRadiation:               f64,
	vm_PermanentWiltingPoint:      [dynamic]f64,
	vc_PercentageSoilCoverage:     f64,
	vm_PercolationRate:            [dynamic]f64,
	vm_ReferenceEvapotranspiration: f64,
	vm_ResidualEvapotranspiration: [dynamic]f64,
	vm_SaturatedHydraulicConductivity: [dynamic]f64,

	vm_SoilMoisture:        [dynamic]f64,
	vm_SoilMoisture_crit:   f64,
	vm_SoilMoistureDeficit: f64,
	vm_SoilPoreVolume:      [dynamic]f64,
	vc_StomataResistance:   f64,
	vm_SurfaceRoughness:    f64,
	vm_SurfaceRunOff:       f64,
	vm_SumSurfaceRunOff:    f64,
	vm_SurfaceWaterStorage: f64,
	pt_TimeStep:            f64,
	vm_TotalWaterRemoval:   f64,
	vm_Transpiration:       [dynamic]f64,
	vm_WaterFlux:           [dynamic]f64,
	vm_XSACriticalSoilMoisture: f64,

	snowComponent: Snow_Component,
	frostComponent: Frost_Component,
	cropModule:    ^Crop_Module,
}

// C++ in-class initialisers not covered by Odin's zero value: vc_KcFactor{0.6},
// vm_irrigFwEvent{1.0}, vm_ReferenceEvapotranspiration{6.0}.
@(private)
make_default_soil_moisture :: proc() -> Soil_Moisture {
	sm: Soil_Moisture
	sm.vc_KcFactor = 0.6
	sm.vm_irrigFwEvent = 1.0
	sm.vm_ReferenceEvapotranspiration = 6.0
	return sm
}

// C++: kj::Own<SoilMoisture> monica::makeSoilMoisture(MonicaModel&, const
//        SoilMoistureModuleParameters&)
//
// Takes soil_column/site_parameters/env_ps/crop_ps/p_layer_thickness directly
// instead of a MonicaModel& - see the package comment. Returns by value,
// matching make_soil_column/make_soil_temperature's precedent.
make_soil_moisture :: proc(
	soil_column: ^Soil_Column,
	site_parameters: ^p.Site_Parameters,
	params: p.Soil_Moisture_Module_Parameters,
	env_ps: ^p.Environment_Parameters,
	crop_ps: ^p.Crop_Module_Parameters,
	p_layer_thickness: f64,
	allocator := context.allocator,
) -> Soil_Moisture {
	sm := make_default_soil_moisture()
	sm.soilColumn = soil_column
	sm.siteParameters = site_parameters
	sm.params = params
	sm.envPs = env_ps
	sm.cropPs = crop_ps
	initialize_from_params(&sm, p_layer_thickness, allocator)
	return sm
}

// C++: void monica::soilmoisture::initializeFromParams(SoilMoisture*)
//
// p_layer_thickness stands in for the C++'s `mm.simPs.p_LayerThickness` - see
// the package comment.
initialize_from_params :: proc(sm: ^Soil_Moisture, p_layer_thickness: f64, allocator := context.allocator) {
	sc := sm.soilColumn
	smPs := &sm.params
	envPs := sm.envPs

	sm.numberOfMoistureLayers = len(sc.layers) + 1
	sm.numberOfSoilLayers = len(sc.layers)

	resize(&sm.vm_AvailableWater, sm.numberOfMoistureLayers)
	resize(&sm.pm_CapillaryRiseRate, sm.numberOfMoistureLayers)
	resize(&sm.vm_CapillaryWater, sm.numberOfMoistureLayers)
	resize(&sm.vm_CapillaryWater70, sm.numberOfMoistureLayers)
	resize(&sm.vm_Evaporation, sm.numberOfMoistureLayers)
	resize(&sm.vm_Evapotranspiration, sm.numberOfMoistureLayers)
	resize(&sm.vm_FieldCapacity, sm.numberOfMoistureLayers)
	resize(&sm.vm_GravitationalWater, sm.numberOfMoistureLayers)
	resize(&sm.vm_HeatConductivity, sm.numberOfMoistureLayers)
	resize(&sm.vm_Lambda, sm.numberOfMoistureLayers)
	resize(&sm.vm_LayerThickness, sm.numberOfMoistureLayers)
	for i in 0 ..< len(sm.vm_LayerThickness) {
		sm.vm_LayerThickness[i] = 0.01
	}
	resize(&sm.vm_PermanentWiltingPoint, sm.numberOfMoistureLayers)
	resize(&sm.vm_PercolationRate, sm.numberOfMoistureLayers)
	resize(&sm.vm_ResidualEvapotranspiration, sm.numberOfMoistureLayers)
	resize(&sm.vm_SoilMoisture, sm.numberOfMoistureLayers)
	for i in 0 ..< len(sm.vm_SoilMoisture) {
		sm.vm_SoilMoisture[i] = 0.20
	}
	resize(&sm.vm_SoilPoreVolume, sm.numberOfMoistureLayers)
	resize(&sm.vm_Transpiration, sm.numberOfMoistureLayers)
	resize(&sm.vm_WaterFlux, sm.numberOfMoistureLayers)

	sm.vs_Latitude = sm.siteParameters.vs_Latitude
	sm.vm_HydraulicConductivityRedux = smPs.pm_HydraulicConductivityRedux
	sm.pt_TimeStep = envPs.p_timeStep
	sm.vm_SurfaceRoughness = smPs.pm_SurfaceRoughness
	sm.vm_GroundwaterDischarge = smPs.pm_GroundwaterDischarge
	sm.pm_MaxPercolationRate = smPs.pm_MaxPercolationRate
	sm.pm_LeachingDepth = envPs.p_LeachingDepth

	sm.pm_LayerThickness = p_layer_thickness

	sm.pm_LeachingDepthLayer =
		int(libc.floor(0.5 + (sm.pm_LeachingDepth / sm.pm_LayerThickness))) - 1

	resize(&sm.vm_SaturatedHydraulicConductivity, sm.numberOfMoistureLayers)
	for i in 0 ..< sm.numberOfMoistureLayers {
		sm.vm_SaturatedHydraulicConductivity[i] = smPs.pm_SaturatedHydraulicConductivity
	}

	initialize_snow_component(&sm.snowComponent, sc, smPs)
	initialize_frost_component(&sm.frostComponent, sc, smPs.pm_HydraulicConductivityRedux, envPs.p_timeStep, allocator)
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
	vs_GroundwaterDepth, vw_Precipitation, vw_MaxAirTemperature, vw_MinAirTemperature,
	vw_RelativeHumidity, vw_MeanAirTemperature, vw_WindSpeed, vw_WindSpeedHeight,
	vw_GlobalRadiation: f64,
	vs_JulianDay: int,
	vw_ReferenceEvapotranspiration: f64,
	dual_kc_method: bool = false,
	daily_sum_irrigation_water: f64 = 0.0,
) {
	sc := sm.soilColumn

	for i in 0 ..< sm.numberOfSoilLayers {
		sm.vm_SoilMoisture[i] = sc.layers[i].vs_SoilMoisture_m3
		sm.vm_WaterFlux[i] = 0.0
		sm.vm_FieldCapacity[i] = sc.layers[i].vs_FieldCapacity
		sm.vm_SoilPoreVolume[i] = sc.layers[i].vs_Saturation
		sm.vm_PermanentWiltingPoint[i] = sc.layers[i].vs_PermanentWiltingPoint
		sm.vm_LayerThickness[i] = sc.layers[i].vs_LayerThickness
		sm.vm_Lambda[i] = sc.layers[i].vs_Lambda
	}

	sm.vm_SoilMoisture[sm.numberOfMoistureLayers - 1] =
		sc.layers[sm.numberOfMoistureLayers - 2].vs_SoilMoisture_m3
	sm.vm_WaterFlux[sm.numberOfMoistureLayers - 1] = 0.0
	sm.vm_FieldCapacity[sm.numberOfMoistureLayers - 1] =
		sc.layers[sm.numberOfMoistureLayers - 2].vs_FieldCapacity
	sm.vm_SoilPoreVolume[sm.numberOfMoistureLayers - 1] =
		sc.layers[sm.numberOfMoistureLayers - 2].vs_Saturation
	sm.vm_LayerThickness[sm.numberOfMoistureLayers - 1] =
		sc.layers[sm.numberOfMoistureLayers - 2].vs_LayerThickness
	sm.vm_Lambda[sm.numberOfMoistureLayers - 1] = sc.layers[sm.numberOfMoistureLayers - 2].vs_Lambda

	sm.vm_SurfaceWaterStorage = sc.vs_SurfaceWaterStorage

	vc_CropPlanted := false
	vc_CropHeight := 0.0
	vc_DevelopmentalStage := 0

	// C++: `sm->monica.currentCropModule.get()` - see the package comment.
	if sm.cropModule != nil {
		vc_CropPlanted = true
		sm.vc_PercentageSoilCoverage = sm.cropModule.vc_SoilCoverage
		sm.vc_KcFactor = sm.cropModule.vc_KcFactor
		vc_CropHeight = sm.cropModule.vc_CropHeight
		vc_DevelopmentalStage = sm.cropModule.vc_DevelopmentalStage
		if vc_DevelopmentalStage > 0 {
			sm.vc_NetPrecipitation = sm.cropModule.vc_NetPrecipitation
		} else {
			sm.vc_NetPrecipitation = vw_Precipitation
		}
	} else {
		vc_CropPlanted = false
		sm.vc_KcFactor = sm.params.pm_KcFactor
		sm.vc_NetPrecipitation = vw_Precipitation
		sm.vc_PercentageSoilCoverage = 0.0
	}
	_ = vc_CropPlanted
	_ = vc_CropHeight

	// Recalculates current depth of groundwater table
	sm.vm_GroundwaterTableLayer = sm.numberOfSoilLayers + 2
	i := sm.numberOfSoilLayers - 1
	for i >= 0 && int(sm.vm_SoilMoisture[i] * 10000) == int(sm.vm_SoilPoreVolume[i] * 10000) {
		sm.vm_GroundwaterTableLayer = i
		i -= 1
	}

	oscillGroundWaterLayer := int(vs_GroundwaterDepth / sc.layers[0].vs_LayerThickness)
	if (sm.vm_GroundwaterTableLayer > oscillGroundWaterLayer &&
		   sm.vm_GroundwaterTableLayer < sm.numberOfSoilLayers + 2) ||
	   sm.vm_GroundwaterTableLayer >= sm.numberOfSoilLayers + 2 {
		sm.vm_GroundwaterTableLayer = oscillGroundWaterLayer
	}

	sc.vm_GroundwaterTableLayer = sm.vm_GroundwaterTableLayer

	// calculates snow layer water storage and release
	calc_snow_layer(&sm.snowComponent, vw_MeanAirTemperature, sm.vc_NetPrecipitation)
	vm_WaterToInfiltrate := sm.snowComponent.vm_WaterToInfiltrate

	// Calculates frost and thaw depth and switches lambda
	calc_soil_frost(&sm.frostComponent, vw_MeanAirTemperature, sm.snowComponent.vm_SnowDepth)

	// calculates infiltration of water from surface
	infiltration(sm, vm_WaterToInfiltrate)

	if 0.0 < vs_GroundwaterDepth && vs_GroundwaterDepth <= 10.0 {
		percolation_with_groundwater(sm, oscillGroundWaterLayer)
		groundwater_replenishment(sm)
	} else {
		percolation_without_groundwater(sm)
		backwater_replenishment(sm)
	}

	// Cache gross precipitation for Dual Kc fw logic (accessible in evapotranspiration)
	sm.vm_GrossPrecipitation = vw_Precipitation

	evapotranspiration(
		sm,
		sm.vc_PercentageSoilCoverage,
		sm.vc_KcFactor,
		sm.siteParameters.vs_HeightNN,
		vw_MaxAirTemperature,
		vw_MinAirTemperature,
		vw_RelativeHumidity,
		vw_MeanAirTemperature,
		vw_WindSpeed,
		vw_WindSpeedHeight,
		vw_GlobalRadiation,
		vc_DevelopmentalStage,
		vs_JulianDay,
		sm.vs_Latitude,
		vw_ReferenceEvapotranspiration,
		dual_kc_method,
		daily_sum_irrigation_water,
	)

	capillary_rise(sm)

	for i_Layer in 0 ..< sm.numberOfSoilLayers {
		sc.layers[i_Layer].vs_SoilMoisture_m3 = sm.vm_SoilMoisture[i_Layer]
		sc.layers[i_Layer].vs_SoilWaterFlux = sm.vm_WaterFlux[i_Layer]
	}
	sc.vs_SurfaceWaterStorage = sm.vm_SurfaceWaterStorage
	sc.vs_FluxAtLowerBoundary = sm.vm_FluxAtLowerBoundary
}

// C++: void monica::soilmoisture::infiltration(SoilMoisture*, double)
infiltration :: proc(sm: ^Soil_Moisture, vm_WaterToInfiltrate: f64) {
	sc := sm.soilColumn

	sm.vm_Infiltration = 0.0
	sm.vm_Interception = 0.0
	sm.vm_SurfaceRunOff = 0.0
	sm.vm_CapillaryRise = 0.0
	sm.vm_GroundwaterAdded = 0.0
	sm.vm_ActualTranspiration = 0.0

	vm_SurfaceWaterStorageOld := sm.vm_SurfaceWaterStorage

	sm.vm_SurfaceWaterStorage += vm_WaterToInfiltrate

	sm.vm_SoilMoistureDeficit = (sm.vm_SoilPoreVolume[0] - sm.vm_SoilMoisture[0]) / sm.vm_SoilPoreVolume[0]
	vm_ReducedHydraulicConductivity := sm.vm_SaturatedHydraulicConductivity[0] * sm.vm_HydraulicConductivityRedux

	if vm_ReducedHydraulicConductivity > 0.0 {
		vm_PotentialInfiltration :=
			vm_ReducedHydraulicConductivity * 0.2 * sm.vm_SoilMoistureDeficit * sm.vm_SoilMoistureDeficit

		sm.vm_Infiltration = min(sm.vm_SurfaceWaterStorage, vm_PotentialInfiltration)

		sm.vm_Infiltration = min(
			sm.vm_Infiltration,
			((sm.vm_SoilPoreVolume[0] - sm.vm_SoilMoisture[0]) * 1000.0 * sc.layers[0].vs_LayerThickness),
		)

		sm.vm_Infiltration = max(0.0, sm.vm_Infiltration)
	} else {
		sm.vm_Infiltration = 0.0
	}

	if sm.vm_Infiltration > 0.0 {
		sm.vm_SurfaceWaterStorage -= sm.vm_Infiltration
	}

	if sm.vm_SurfaceWaterStorage > (10.0 * sm.vm_SurfaceRoughness / (sm.siteParameters.vs_Slope + 0.001)) {
		vm_RunOffFactor := 0.02 + (sm.vm_SurfaceRoughness / 4.0) + (sm.vc_PercentageSoilCoverage / 15.0)
		if sm.siteParameters.vs_Slope < 0.0 || sm.siteParameters.vs_Slope > 1.0 {
			fmt.eprintln("Slope value out ouf boundary")
		} else if sm.siteParameters.vs_Slope == 0.0 {
			sm.vm_SurfaceRunOff = 0.0
		} else if sm.siteParameters.vs_Slope > vm_RunOffFactor {
			sm.vm_SurfaceRunOff += sm.vm_SurfaceWaterStorage
		} else {
			sm.vm_SurfaceRunOff +=
				((sm.siteParameters.vs_Slope * vm_RunOffFactor) / (vm_RunOffFactor * vm_RunOffFactor)) *
				sm.vm_SurfaceWaterStorage
		}

		sm.vm_SurfaceWaterStorage -= sm.vm_SurfaceRunOff
	}

	sm.vm_SoilMoisture[0] += (sm.vm_Infiltration / 1000.0 / sm.vm_LayerThickness[0])

	sm.vm_WaterFlux[0] = sm.vm_Infiltration

	if sm.vm_SoilMoisture[0] > sm.vm_FieldCapacity[0] {
		sm.vm_GravitationalWater[0] = (sm.vm_SoilMoisture[0] - sm.vm_FieldCapacity[0]) * 1000.0 * sm.vm_LayerThickness[0]
		vm_LambdaReduced := sm.vm_Lambda[0] * sm.frostComponent.vm_LambdaRedux[0]
		vm_PercolationFactor := 1 + vm_LambdaReduced * sm.vm_GravitationalWater[0]
		sm.vm_PercolationRate[0] =
			(sm.vm_GravitationalWater[0] * sm.vm_GravitationalWater[0] * vm_LambdaReduced) / vm_PercolationFactor
		if sm.vm_PercolationRate[0] > sm.pm_MaxPercolationRate {
			sm.vm_PercolationRate[0] = sm.pm_MaxPercolationRate
		}
		sm.vm_GravitationalWater[0] = sm.vm_GravitationalWater[0] - sm.vm_PercolationRate[0]
		sm.vm_GravitationalWater[0] = max(0.0, sm.vm_GravitationalWater[0])

		sm.vm_SoilMoisture[0] = sm.vm_FieldCapacity[0] + (sm.vm_GravitationalWater[0] / 1000.0 / sm.vm_LayerThickness[0])

		if sm.vm_GroundwaterTableLayer <= 1 {
			sm.vm_PercolationRate[0] = 0.0
		}

		if sm.vm_GroundwaterTableLayer == 0 {
			sm.vm_PercolationRate[0] = 0.0

			if sm.vm_SoilMoisture[0] > sm.vm_SoilPoreVolume[0] {
				sm.vm_SurfaceRunOff += (sm.vm_SoilMoisture[0] - sm.vm_SoilPoreVolume[0]) * 1000.0 * sm.vm_LayerThickness[0]
				sm.vm_SoilMoisture[0] = sm.vm_SoilPoreVolume[0]
				return
			}
		}
	} else if sm.vm_SoilMoisture[0] <= sm.vm_FieldCapacity[0] {
		sm.vm_PercolationRate[0] = 0.0
		sm.vm_GravitationalWater[0] = 0.0
	}

	// Check water balance
	if libc.fabs(
		   (vm_SurfaceWaterStorageOld + vm_WaterToInfiltrate) -
			   (sm.vm_SurfaceRunOff + sm.vm_Infiltration + sm.vm_SurfaceWaterStorage),
	   ) >
	   0.01 {
		fmt.eprintln("water balance wrong!")
	}

	sm.vm_WaterFlux[1] = sm.vm_PercolationRate[0]
	sm.vm_SumSurfaceRunOff += sm.vm_SurfaceRunOff
}

// C++: void monica::soilmoisture::capillaryRise(SoilMoisture*)
capillary_rise :: proc(sm: ^Soil_Moisture, allocator := context.allocator) {
	sc := sm.soilColumn

	vc_RootingDepth := sm.cropModule != nil ? sm.cropModule.vc_RootingDepth : 0

	// NOTE(c++-quirk): C++ computes this as size_t arithmetic
	// (max(size_t(1), vm_GroundwaterTableLayer - vc_RootingDepth)): if
	// vc_RootingDepth ever exceeds vm_GroundwaterTableLayer the subtraction
	// wraps to a huge positive number rather than going negative, reproduced
	// here with explicit uint arithmetic. Dormant while cropModule is nil
	// (vc_RootingDepth always 0); matters once phase 5 plants a crop whose
	// roots reach below a shallow water table.
	vm_GroundwaterDistance := max(uint(1), uint(sm.vm_GroundwaterTableLayer) - uint(vc_RootingDepth))

	if f64(vm_GroundwaterDistance) * sm.vm_LayerThickness[0] <= 2.70 {
		for i_Layer in 0 ..< sm.numberOfSoilLayers {
			sm.vm_CapillaryWater[i_Layer] = sm.vm_FieldCapacity[i_Layer] - sm.vm_PermanentWiltingPoint[i_Layer]
			sm.vm_AvailableWater[i_Layer] = sm.vm_SoilMoisture[i_Layer] - sm.vm_PermanentWiltingPoint[i_Layer]

			if sm.vm_AvailableWater[i_Layer] < 0.0 {
				sm.vm_AvailableWater[i_Layer] = 0.0
			}

			sm.vm_CapillaryWater70[i_Layer] = 0.7 * sm.vm_CapillaryWater[i_Layer]
		}

		vm_StartLayer := min(sm.vm_GroundwaterTableLayer, sm.numberOfSoilLayers-1)
		cr := soil.read_capillary_rise_rates(allocator)
		for i := vm_StartLayer; i >= 0; i -= 1 {
			vs_SoilTexture := sc.layers[i].vs_SoilTexture
			assert(len(vs_SoilTexture) > 0)
			vm_CapillaryRiseRate := min(
				0.01,
				soil.capillary_rise_rates_get_rate(cr, vs_SoilTexture, int(vm_GroundwaterDistance)),
			)
			if sm.vm_AvailableWater[i] < sm.vm_CapillaryWater70[i] {
				vm_WaterAddedFromCapillaryRise := vm_CapillaryRiseRate
				sm.vm_SoilMoisture[i] += vm_WaterAddedFromCapillaryRise / sm.vm_LayerThickness[i]
				for j_Layer := vm_StartLayer; j_Layer >= i; j_Layer -= 1 {
					sm.vm_WaterFlux[j_Layer] -= vm_WaterAddedFromCapillaryRise * 1000.0
				}
				break
			}
		}
	}
}

// C++: void monica::soilmoisture::percolationWithGroundwater(SoilMoisture*, size_t)
percolation_with_groundwater :: proc(sm: ^Soil_Moisture, oscillGroundwaterLayer: int) {
	sm.vm_GroundwaterAdded = 0.0

	for i in 0 ..< sm.numberOfMoistureLayers - 1 {
		indexOfLayerBelow := i + 1
		if sm.vm_GroundwaterTableLayer > indexOfLayerBelow {
			// well above groundwater table
			sm.vm_SoilMoisture[indexOfLayerBelow] += sm.vm_PercolationRate[i] / 1000.0 / sm.vm_LayerThickness[i]
			sm.vm_WaterFlux[indexOfLayerBelow] = sm.vm_PercolationRate[i]

			if sm.vm_SoilMoisture[indexOfLayerBelow] > sm.vm_FieldCapacity[indexOfLayerBelow] {
				sm.vm_GravitationalWater[indexOfLayerBelow] =
					(sm.vm_SoilMoisture[indexOfLayerBelow] - sm.vm_FieldCapacity[indexOfLayerBelow]) * 1000.0 *
					sm.vm_LayerThickness[i + 1]

				vm_LambdaReduced := sm.vm_Lambda[indexOfLayerBelow] * sm.frostComponent.vm_LambdaRedux[indexOfLayerBelow]
				vm_PercolationFactor := 1 + vm_LambdaReduced * sm.vm_GravitationalWater[indexOfLayerBelow]
				sm.vm_PercolationRate[indexOfLayerBelow] =
					(sm.vm_GravitationalWater[indexOfLayerBelow] * sm.vm_GravitationalWater[indexOfLayerBelow] * vm_LambdaReduced) /
					vm_PercolationFactor

				sm.vm_GravitationalWater[indexOfLayerBelow] =
					sm.vm_GravitationalWater[indexOfLayerBelow] - sm.vm_PercolationRate[indexOfLayerBelow]

				if sm.vm_GravitationalWater[indexOfLayerBelow] < 0 {
					sm.vm_GravitationalWater[indexOfLayerBelow] = 0.0
				}

				sm.vm_SoilMoisture[indexOfLayerBelow] =
					sm.vm_FieldCapacity[indexOfLayerBelow] +
					(sm.vm_GravitationalWater[indexOfLayerBelow] / 1000.0 / sm.vm_LayerThickness[indexOfLayerBelow])

				if sm.vm_SoilMoisture[indexOfLayerBelow] > sm.vm_SoilPoreVolume[indexOfLayerBelow] {
					sm.vm_GravitationalWater[indexOfLayerBelow] =
						(sm.vm_SoilMoisture[indexOfLayerBelow] - sm.vm_SoilPoreVolume[indexOfLayerBelow]) * 1000.0 *
						sm.vm_LayerThickness[indexOfLayerBelow]
					sm.vm_SoilMoisture[indexOfLayerBelow] = sm.vm_SoilPoreVolume[indexOfLayerBelow]
					sm.vm_PercolationRate[indexOfLayerBelow] += sm.vm_GravitationalWater[indexOfLayerBelow]
				}
			} else {
				sm.vm_PercolationRate[indexOfLayerBelow] = 0.0
				sm.vm_GravitationalWater[indexOfLayerBelow] = 0.0
			}
		} else if sm.vm_GroundwaterTableLayer == indexOfLayerBelow {
			// when the layer directly above groundwater table is reached
			if sm.vm_GroundwaterTableLayer >= oscillGroundwaterLayer {
				sm.vm_SoilMoisture[indexOfLayerBelow] += sm.vm_PercolationRate[i] / 1000.0 / sm.vm_LayerThickness[i]
				sm.vm_PercolationRate[indexOfLayerBelow] = sm.vm_GroundwaterDischarge
				sm.vm_WaterFlux[indexOfLayerBelow] = sm.vm_PercolationRate[i]
			} else {
				sm.vm_SoilMoisture[indexOfLayerBelow] +=
					(sm.vm_PercolationRate[i] - sm.vm_GroundwaterDischarge) / 1000.0 / sm.vm_LayerThickness[i]
				sm.vm_PercolationRate[indexOfLayerBelow] = sm.vm_GroundwaterDischarge
				sm.vm_WaterFlux[indexOfLayerBelow] = sm.vm_GroundwaterDischarge
			}

			if sm.vm_SoilMoisture[indexOfLayerBelow] >= sm.vm_SoilPoreVolume[indexOfLayerBelow] {
				sm.vm_GroundwaterAdded =
					(sm.vm_SoilMoisture[indexOfLayerBelow] - sm.vm_SoilPoreVolume[indexOfLayerBelow]) * 1000.0 *
					sm.vm_LayerThickness[indexOfLayerBelow]

				sm.vm_SoilMoisture[indexOfLayerBelow] = sm.vm_SoilPoreVolume[indexOfLayerBelow]

				if sm.vm_GroundwaterAdded <= 0.0 {
					sm.vm_GroundwaterAdded = 0.0
				}
			}
		} else if sm.vm_GroundwaterTableLayer < indexOfLayerBelow {
			// when the groundwater table is reached
			sm.vm_SoilMoisture[indexOfLayerBelow] = sm.vm_SoilPoreVolume[indexOfLayerBelow]

			if sm.vm_GroundwaterTableLayer >= oscillGroundwaterLayer {
				sm.vm_PercolationRate[indexOfLayerBelow] = sm.vm_PercolationRate[i]
				sm.vm_WaterFlux[i] = sm.vm_PercolationRate[indexOfLayerBelow]
			} else {
				sm.vm_PercolationRate[indexOfLayerBelow] = sm.vm_GroundwaterDischarge
				sm.vm_WaterFlux[i] = sm.vm_GroundwaterDischarge
			}
		}
	}

	sm.vm_FluxAtLowerBoundary = sm.vm_WaterFlux[sm.pm_LeachingDepthLayer]
}

// C++: void monica::soilmoisture::groundwaterReplenishment(SoilMoisture*)
groundwater_replenishment :: proc(sm: ^Soil_Moisture) {
	vm_StartLayer := sm.vm_GroundwaterTableLayer

	if vm_StartLayer > sm.numberOfMoistureLayers-2 {
		vm_StartLayer = sm.numberOfMoistureLayers - 2
	}

	for i := vm_StartLayer; i >= 0; i -= 1 {
		indexOfLayerBelow := i + 1
		sm.vm_SoilMoisture[i] += sm.vm_GroundwaterAdded / 1000.0 / sm.vm_LayerThickness[indexOfLayerBelow]

		if i == vm_StartLayer {
			sm.vm_PercolationRate[i] = sm.vm_GroundwaterDischarge
		} else {
			sm.vm_PercolationRate[i] -= sm.vm_GroundwaterAdded
			sm.vm_WaterFlux[indexOfLayerBelow] = sm.vm_PercolationRate[i]
		}

		if sm.vm_SoilMoisture[i] > sm.vm_SoilPoreVolume[i] {
			sm.vm_GroundwaterAdded =
				(sm.vm_SoilMoisture[i] - sm.vm_SoilPoreVolume[i]) * 1000.0 * sm.vm_LayerThickness[indexOfLayerBelow]
			sm.vm_SoilMoisture[i] = sm.vm_SoilPoreVolume[i]
			sm.vm_GroundwaterTableLayer -= 1 // Groundwater table rises

			if i == 0 && sm.vm_GroundwaterTableLayer == 0 {
				// if groundwater reaches surface
				sm.vm_SurfaceWaterStorage += sm.vm_GroundwaterAdded
				sm.vm_GroundwaterAdded = 0.0
			}
		} else {
			sm.vm_GroundwaterAdded = 0.0
		}
	}

	if sm.pm_LeachingDepthLayer > sm.vm_GroundwaterTableLayer-1 {
		if sm.vm_GroundwaterTableLayer-1 < 0 {
			sm.vm_FluxAtLowerBoundary = 0.0
		} else {
			sm.vm_FluxAtLowerBoundary = sm.vm_WaterFlux[sm.vm_GroundwaterTableLayer-1]
		}
	} else {
		sm.vm_FluxAtLowerBoundary = sm.vm_WaterFlux[sm.pm_LeachingDepthLayer]
	}
}

// C++: void monica::soilmoisture::percolationWithoutGroundwater(SoilMoisture*)
percolation_without_groundwater :: proc(sm: ^Soil_Moisture) {
	for i in 0 ..< sm.numberOfMoistureLayers - 1 {
		indexOfLayerBelow := i + 1
		sm.vm_SoilMoisture[indexOfLayerBelow] += sm.vm_PercolationRate[i] / 1000.0 / sm.vm_LayerThickness[i]

		if sm.vm_SoilMoisture[indexOfLayerBelow] > sm.vm_FieldCapacity[indexOfLayerBelow] {
			// too much water for this layer so some water is released to layers below
			sm.vm_GravitationalWater[indexOfLayerBelow] =
				(sm.vm_SoilMoisture[indexOfLayerBelow] - sm.vm_FieldCapacity[indexOfLayerBelow]) * 1000.0 *
				sm.vm_LayerThickness[0]
			vm_LambdaReduced := sm.vm_Lambda[indexOfLayerBelow] * sm.frostComponent.vm_LambdaRedux[indexOfLayerBelow]
			vm_PercolationFactor := 1.0 + (vm_LambdaReduced * sm.vm_GravitationalWater[indexOfLayerBelow])
			sm.vm_PercolationRate[indexOfLayerBelow] =
				(sm.vm_GravitationalWater[indexOfLayerBelow] * sm.vm_GravitationalWater[indexOfLayerBelow] * vm_LambdaReduced) /
				vm_PercolationFactor

			if sm.vm_PercolationRate[indexOfLayerBelow] > sm.pm_MaxPercolationRate {
				sm.vm_PercolationRate[indexOfLayerBelow] = sm.pm_MaxPercolationRate
			}

			sm.vm_GravitationalWater[indexOfLayerBelow] =
				sm.vm_GravitationalWater[indexOfLayerBelow] - sm.vm_PercolationRate[indexOfLayerBelow]

			if sm.vm_GravitationalWater[indexOfLayerBelow] < 0.0 {
				sm.vm_GravitationalWater[indexOfLayerBelow] = 0.0
			}

			sm.vm_SoilMoisture[indexOfLayerBelow] =
				sm.vm_FieldCapacity[indexOfLayerBelow] +
				(sm.vm_GravitationalWater[indexOfLayerBelow] / 1000.0 / sm.vm_LayerThickness[indexOfLayerBelow])
		} else {
			// no water will be released in other layers
			sm.vm_PercolationRate[indexOfLayerBelow] = 0.0
			sm.vm_GravitationalWater[indexOfLayerBelow] = 0.0
		}

		sm.vm_WaterFlux[indexOfLayerBelow] = sm.vm_PercolationRate[i]
		sm.vm_GroundwaterAdded = sm.vm_PercolationRate[indexOfLayerBelow]
	}

	if sm.pm_LeachingDepthLayer > 0 && sm.pm_LeachingDepthLayer < sm.numberOfMoistureLayers-1 {
		sm.vm_FluxAtLowerBoundary = sm.vm_WaterFlux[sm.pm_LeachingDepthLayer]
	} else {
		sm.vm_FluxAtLowerBoundary = sm.vm_WaterFlux[sm.numberOfMoistureLayers-2]
	}
}

// C++: void monica::soilmoisture::backwaterReplenishment(SoilMoisture*)
backwater_replenishment :: proc(sm: ^Soil_Moisture) {
	vm_StartLayer := sm.numberOfMoistureLayers - 1
	vm_BackwaterTable := sm.numberOfMoistureLayers - 1
	vm_BackwaterAdded := 0.0

	// find first layer from top where the water content exceeds pore volume
	for i in 0 ..< sm.numberOfMoistureLayers - 1 {
		if sm.vm_SoilMoisture[i] > sm.vm_SoilPoreVolume[i] {
			vm_StartLayer = i
			vm_BackwaterTable = i
		}
	}

	// if there is no such thing nothing will happen
	if vm_BackwaterTable == 0 {
		return
	}

	// Backwater replenishment upwards
	for i := vm_StartLayer; i >= 0; i -= 1 {
		sm.vm_SoilMoisture[i] += vm_BackwaterAdded / 1000.0 / sm.vm_LayerThickness[i]
		if i > 0 {
			sm.vm_WaterFlux[i-1] -= vm_BackwaterAdded
		}

		if sm.vm_SoilMoisture[i] > sm.vm_SoilPoreVolume[i] {
			vm_BackwaterAdded = (sm.vm_SoilMoisture[i] - sm.vm_SoilPoreVolume[i]) * 1000.0 * sm.vm_LayerThickness[i]
			sm.vm_SoilMoisture[i] = sm.vm_SoilPoreVolume[i]
			vm_BackwaterTable -= 1 // Backwater table rises

			if i == 0 && vm_BackwaterTable == 0 {
				// if backwater reaches surface
				sm.vm_SurfaceWaterStorage += vm_BackwaterAdded
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
	windSpeed, tmin, tmax: f64,
	daily_sum_irrigation_water: f64,
) -> f64 {
	E_pot_dualKc := 0.0 // [mm d-1] replaces (1-beta)*PET per layer

	// ET0 is already in vm_ReferenceEvapotranspiration [mm d-1]
	ET0 := sm.vm_ReferenceEvapotranspiration

	// --- Kcb: basal crop coefficient (interpolated in crop module) ---
	Kcb := sm.cropModule.vc_KcbFactor

	// --- Calculate Depletion first (FAO-56 §8.3) to inform memory logic ---
	FC0 := sm.vm_FieldCapacity[0]
	WP0 := sm.vm_PermanentWiltingPoint[0]
	SWC0 := sm.vm_SoilMoisture[0]
	Ze := 0.1 // evaporation depth [m], FAO-56 typical top layer
	TEW := 1000.0 * (FC0 - 0.5*WP0) * Ze

	// A. Dynamic REW (FAO-56 Table 19 Mapping via Pedology)
	//
	// NOTE(c++-quirk): these comparisons are against mixed-case strings
	// ("Ss", "Su2", ...) while real KA5 textures in this port's fixtures are
	// always uppercase ("SS", "SU2", ...) - so none of these branches ever
	// match on real data and REW always falls through to the FC0-based
	// fallback below. Reproduced as-is, not fixed.
	REW := 0.0
	ka5Texture := sm.soilColumn.layers[0].vs_SoilTexture

	switch {
	case ka5Texture == "Ss":
		REW = 2.5
	case ka5Texture == "Su2" || ka5Texture == "Sl2":
		REW = 3.5
	case ka5Texture == "Su3" || ka5Texture == "Sl3":
		REW = 4.5
	case ka5Texture == "Su4" || ka5Texture == "Sl4" || ka5Texture == "St2":
		REW = 5.5
	case ka5Texture == "St3" || ka5Texture == "Ls2" || ka5Texture == "Us2":
		REW = 8.0
	case ka5Texture == "Uu" || ka5Texture == "Us3" || ka5Texture == "Us4" ||
	     ka5Texture == "Ul2" || ka5Texture == "Ul3" || ka5Texture == "Ul4":
		REW = 8.5
	case ka5Texture == "Ls3" || ka5Texture == "Ls4" || ka5Texture == "Lu2" ||
	     ka5Texture == "Lu3" || ka5Texture == "Lu4":
		REW = 9.0
	case ka5Texture == "Ut2" || ka5Texture == "Ut3":
		REW = 9.5
	case ka5Texture == "Ut4" || ka5Texture == "Lt2" || ka5Texture == "Lt3" || ka5Texture == "Lts":
		REW = 10.5
	case ka5Texture == "Ts2" || ka5Texture == "Ts3" || ka5Texture == "Ts4" ||
	     ka5Texture == "Tu2" || ka5Texture == "Tu3" || ka5Texture == "Tu4" || ka5Texture == "Tl":
		REW = 11.5
	case ka5Texture == "Tt":
		REW = 12.0
	}

	// Fallback if KA5 lookup is unavailable or fails:
	if REW == 0.0 {
		if FC0 < 0.18 {
			// Coarse soils (Sand, Sandy Loams): REW ranges from 2.0 to 7.0 mm
			REW = 2.5 + 25.0*(FC0-0.05)
			REW = max(2.0, min(REW, 7.0))
		} else if FC0 >= 0.28 {
			// Fine soils (Clays): REW ranges from 8.0 to 12.0 mm
			REW = 8.0 + 20.0*(FC0-0.28)
			REW = max(8.0, min(REW, 12.0))
		} else {
			REW = 8.0
		}
	}
	REW = min(REW, TEW) // Hard boundary safety clamp

	// Current depletion De [mm] = what the top layer is missing compared to FC
	De := 1000.0 * max(0.0, FC0-SWC0) * Ze

	// --- Memory state update for wetting events ---
	precip := sm.vm_GrossPrecipitation
	irrigApplied := daily_sum_irrigation_water
	if precip > 0.0 {
		sm.vm_LastWettingWasRain = true
	} else if irrigApplied > 0.0 {
		sm.vm_LastWettingWasRain = false
	}

	// --- fw: fraction of wetted soil surface (event-level, FAO-56 §8.3) ---
	fw_today: f64
	if precip > 0.0 {
		fw_today = 1.0 // rain wets the full surface
	} else if De <= REW && sm.vm_LastWettingWasRain {
		fw_today = 1.0 // still in Stage 1 drying from recent rain
	} else {
		fw_today = sm.vm_irrigFwEvent // carry/use the last Irrigation workstep fw
	}
	fw_today = max(0.0, min(fw_today, 1.0))

	// Drip irrigation shading adjustment (FAO-56 §8.3)
	fw_adj := fw_today
	if sm.vm_irrigIsDripEvent && !sm.vm_LastWettingWasRain && precip == 0.0 {
		fw_adj = fw_today * (1.0 - (2.0/3.0)*sm.vc_PercentageSoilCoverage)
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
	RHmin := max(5.0, min(100.0, (eo_Tmin/eo_Tmax)*100.0))
	baseline := 1.2 // FAO-56 §6 default for most crops
	h := max(0.01, sm.cropModule.vc_CropHeight) // native simulated height [m]
	Kc_max := baseline + (0.04*(u2-2.0)-0.004*(RHmin-45.0))*libc.pow(h/3.0, 0.3)
	Kc_max = max(Kc_max, Kcb+0.05)

	// C. few: fraction of exposed and wetted soil (FAO-56 Eq. 74)
	fc := max(0.0, min(sm.vc_PercentageSoilCoverage, 0.99))
	few := min(1.0-fc, fw_adj)
	few = max(0.001, few) // guard against zero denominator

	// --- Ke: soil evaporation coefficient (FAO-56 eq. 71) ---
	Ke := Kr * (Kc_max - Kcb)
	Ke = min(Ke, few*Kc_max)
	Ke = max(0.0, Ke)

	// Potential soil evaporation [mm d-1]
	E_pot_dualKc = ET0 * Ke
	// Respect the hard cap from HERMES (6.5 mm/day applies to total, cap E too)
	E_pot_dualKc = min(E_pot_dualKc, 6.5)

	sm.vm_Ke = Ke

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
	vc_PercentageSoilCoverage, vc_KcFactor, vs_HeightNN, vw_MaxAirTemperature,
	vw_MinAirTemperature, vw_RelativeHumidity, vw_MeanAirTemperature, vw_WindSpeed,
	vw_WindSpeedHeight, vw_GlobalRadiation: f64,
	vc_DevelopmentalStage: int,
	vs_JulianDay: int,
	vs_Latitude, vw_ReferenceEvapotranspiration: f64,
	dual_kc_method: bool,
	daily_sum_irrigation_water: f64,
) {
	vm_EReducer_1 := 0.0
	vm_EReducer_2 := 0.0
	vm_EReducer_3 := 0.0
	pm_EvaporationZeta: f64
	pm_MaximumEvaporationImpactDepth: f64
	vm_EReducer := 0.0
	vm_PotentialEvapotranspiration := 0.0
	vc_EvaporatedFromIntercept := 0.0
	sm.vm_EvaporatedFromSurface = 0.0
	vm_EvaporationFromSurface := false

	vm_SnowDepth := sm.snowComponent.vm_SnowDepth

	// Berechnung der Bodenevaporation bis max. 4dm Tiefe
	pm_EvaporationZeta = sm.params.pm_EvaporationZeta

	sm.vm_XSACriticalSoilMoisture = sm.params.pm_XSACriticalSoilMoisture

	pm_MaximumEvaporationImpactDepth = sm.params.pm_MaximumEvaporationImpactDepth

	// If a crop grows, ETp is taken from crop module
	if vc_DevelopmentalStage > 0 {
		// C++: `monica.currentCropModule.get()` - see the package comment.
		if vw_ReferenceEvapotranspiration < 0.0 {
			sm.vm_ReferenceEvapotranspiration = sm.cropModule.vc_ReferenceEvapotranspiration
		} else {
			sm.vm_ReferenceEvapotranspiration = vw_ReferenceEvapotranspiration
		}

		vm_PotentialEvapotranspiration = sm.cropModule.vc_RemainingEvapotranspiration
		vc_EvaporatedFromIntercept = sm.cropModule.vc_EvaporatedFromIntercept
	} else { // if no crop grows ETp is calculated from ET0 * kc
		if vw_ReferenceEvapotranspiration < 0.0 {
			sm.vm_ReferenceEvapotranspiration = reference_evapotranspiration(
				sm,
				vs_HeightNN,
				vw_MaxAirTemperature,
				vw_MinAirTemperature,
				vw_RelativeHumidity,
				vw_MeanAirTemperature,
				vw_WindSpeed,
				vw_WindSpeedHeight,
				vw_GlobalRadiation,
				vs_JulianDay,
				vs_Latitude,
			)
		} else {
			sm.vm_ReferenceEvapotranspiration = vw_ReferenceEvapotranspiration
		}

		vm_PotentialEvapotranspiration = sm.vm_ReferenceEvapotranspiration * vc_KcFactor
	}

	sm.vm_ActualEvaporation = 0.0
	sm.vm_ActualTranspiration = 0.0

	// from HERMES:
	if vm_PotentialEvapotranspiration > 6.5 {
		vm_PotentialEvapotranspiration = 6.5
	}

	if vm_PotentialEvapotranspiration > 0.0 {
		// If surface is water-logged, subsequent evaporation from surface water sources
		if sm.vm_SurfaceWaterStorage > 0.0 {
			vm_EvaporationFromSurface = true
			// Water surface evaporates with Kc = 1.1.
			vm_PotentialEvapotranspiration = vm_PotentialEvapotranspiration * (1.1 / vc_KcFactor)

			// If a snow layer is present no water evaporates from surface water sources
			if vm_SnowDepth > 0.0 {
				sm.vm_EvaporatedFromSurface = 0.0
			} else {
				if sm.vm_SurfaceWaterStorage < vm_PotentialEvapotranspiration {
					vm_PotentialEvapotranspiration -= sm.vm_SurfaceWaterStorage
					sm.vm_EvaporatedFromSurface = sm.vm_SurfaceWaterStorage
					sm.vm_SurfaceWaterStorage = 0.0
				} else {
					sm.vm_SurfaceWaterStorage -= vm_PotentialEvapotranspiration
					sm.vm_EvaporatedFromSurface = vm_PotentialEvapotranspiration
					vm_PotentialEvapotranspiration = 0.0
				}
			}
			vm_PotentialEvapotranspiration = vm_PotentialEvapotranspiration * (vc_KcFactor / 1.1)
		}

		if vm_PotentialEvapotranspiration > 0 { // Evaporation from soil
			// -----------------------------------------------------------------
			// FAO-56 Dual Kc pathway - precompute potential soil evaporation
			// (E_pot) using the Ke coefficient. Runs ONLY when dualKcMethod is
			// enabled, a crop is present (vc_DevelopmentalStage > 0), and
			// sm.cropModule != nil. Otherwise the Single Kc loop runs unchanged.
			// -----------------------------------------------------------------
			E_pot_dualKc := 0.0
			useDualKc := dual_kc_method && vc_DevelopmentalStage > 0 && sm.cropModule != nil
			if useDualKc {
				E_pot_dualKc = dual_kc_precomputation(
					sm,
					vw_WindSpeed,
					vw_MinAirTemperature,
					vw_MaxAirTemperature,
					daily_sum_irrigation_water,
				)
			} else {
				sm.vm_Ke = 0.0
			}

			for i_Layer in 0 ..< sm.numberOfSoilLayers {
				vm_EReducer_1 = get_e_reducer_1(sm, i_Layer, vc_PercentageSoilCoverage, vm_PotentialEvapotranspiration)

				if f64(i_Layer) >= pm_MaximumEvaporationImpactDepth {
					// layer is too deep for evaporation
					vm_EReducer_2 = 0.0
				} else {
					vm_EReducer_2 = get_deprivation_factor(
						i_Layer + 1,
						pm_MaximumEvaporationImpactDepth,
						pm_EvaporationZeta,
						sm.vm_LayerThickness[i_Layer],
					)
				}

				if i_Layer > 0 {
					if sm.vm_SoilMoisture[i_Layer] < sm.vm_SoilMoisture[i_Layer-1] {
						vm_EReducer_3 = 0.1
					} else {
						vm_EReducer_3 = 1.0
					}
				} else {
					vm_EReducer_3 = 1.0
				}
				vm_EReducer = vm_EReducer_1 * vm_EReducer_2 * vm_EReducer_3

				if vc_DevelopmentalStage > 0 {
					// vegetation is present
					if useDualKc {
						sm.vm_Evaporation[i_Layer] = vm_EReducer * E_pot_dualKc
					} else {
						// Single Kc: original (1 - beta) * EReducer * PET partitioning
						if vc_PercentageSoilCoverage >= 0.0 && vc_PercentageSoilCoverage < 1.0 {
							sm.vm_Evaporation[i_Layer] =
								((1.0 - vc_PercentageSoilCoverage) * vm_EReducer) * vm_PotentialEvapotranspiration
						} else {
							if vc_PercentageSoilCoverage >= 1.0 {
								sm.vm_Evaporation[i_Layer] = 0.0
							}
						}
					}

					if vm_SnowDepth > 0.0 {
						sm.vm_Evaporation[i_Layer] = 0.0
					}

					// C++: `monica.currentCropModule.get()->vc_Transpiration` - see the package comment.
					sm.vm_Transpiration[i_Layer] = sm.cropModule.vc_Transpiration[i_Layer]

					// Transpiration is capped in case potential ET after surface
					// and interception evaporation has occurred on same day
					if vm_EvaporationFromSurface {
						sm.vm_Transpiration[i_Layer] =
							vc_PercentageSoilCoverage * vm_EReducer * vm_PotentialEvapotranspiration
					}
				} else {
					// no vegetation present - Single Kc / bare soil, always unchanged
					if vm_SnowDepth > 0.0 {
						sm.vm_Evaporation[i_Layer] = 0.0
					} else {
						sm.vm_Evaporation[i_Layer] = vm_PotentialEvapotranspiration * vm_EReducer
					}
					sm.vm_Transpiration[i_Layer] = 0.0
				}

				sm.vm_Evapotranspiration[i_Layer] = sm.vm_Evaporation[i_Layer] + sm.vm_Transpiration[i_Layer]
				sm.vm_SoilMoisture[i_Layer] -= (sm.vm_Evapotranspiration[i_Layer] / 1000.0 / sm.vm_LayerThickness[i_Layer])

				// Generelle Begrenzung des Evaporationsentzuges
				if sm.vm_SoilMoisture[i_Layer] < 0.01 {
					sm.vm_SoilMoisture[i_Layer] = 0.01
				}

				sm.vm_ActualTranspiration += sm.vm_Transpiration[i_Layer]
				sm.vm_ActualEvaporation += sm.vm_Evaporation[i_Layer]
			}
		}
	}
	sm.vm_ActualEvapotranspiration =
		sm.vm_ActualTranspiration + sm.vm_ActualEvaporation + vc_EvaporatedFromIntercept + sm.vm_EvaporatedFromSurface
}

// C++: double monica::soilmoisture::referenceEvapotranspiration(SoilMoisture*,
//        double, double, double, double, double, double, double, double, int,
//        double)
//
// Penman-Monteith method, FAO Irrigation and Drainage Paper 56.
reference_evapotranspiration :: proc(
	sm: ^Soil_Moisture,
	vs_HeightNN, vw_MaxAirTemperature, vw_MinAirTemperature, vw_RelativeHumidity,
	vw_MeanAirTemperature, vw_WindSpeed, vw_WindSpeedHeight, vw_GlobalRadiation: f64,
	vs_JulianDay: int,
	vs_Latitude: f64,
) -> f64 {
	pc_ReferenceAlbedo := sm.cropPs.pc_ReferenceAlbedo
	PI := 3.14159265358979323

	vc_Declination := -23.4 * libc.cos(2.0 * PI * ((f64(vs_JulianDay) + 10.0) / 365.0))
	vc_DeclinationSinus := libc.sin(vc_Declination*PI/180.0) * libc.sin(vs_Latitude*PI/180.0)
	vc_DeclinationCosinus := libc.cos(vc_Declination*PI/180.0) * libc.cos(vs_Latitude*PI/180.0)

	arg_AstroDayLength := vc_DeclinationSinus / vc_DeclinationCosinus
	// The argument of asin must be in the range of -1 to 1
	arg_AstroDayLength = tl.bound(-1.0, arg_AstroDayLength, 1.0)
	vc_AstronomicDayLenght := 12.0 * (PI + 2.0*libc.asin(arg_AstroDayLength)) / PI

	arg_EffectiveDayLength := (-libc.sin(8.0*PI/180.0) + vc_DeclinationSinus) / vc_DeclinationCosinus
	arg_EffectiveDayLength = tl.bound(-1.0, arg_EffectiveDayLength, 1.0)
	vc_EffectiveDayLenght := 12.0 * (PI + 2.0*libc.asin(arg_EffectiveDayLength)) / PI

	arg_PhotoDayLength := (-libc.sin(-6.0*PI/180.0) + vc_DeclinationSinus) / vc_DeclinationCosinus
	arg_PhotoDayLength = tl.bound(-1.0, arg_PhotoDayLength, 1.0)
	vc_PhotoperiodicDaylength := 12.0 * (PI + 2.0*libc.asin(arg_PhotoDayLength)) / PI
	_ = vc_EffectiveDayLenght
	_ = vc_PhotoperiodicDaylength

	arg_PhotAct := min(
		1.0,
		((vc_DeclinationSinus / vc_DeclinationCosinus) * (vc_DeclinationSinus / vc_DeclinationCosinus)),
	)
	// The argument of sqrt must be >= 0
	vc_PhotActRadiationMean :=
		3600.0 *
		(vc_DeclinationSinus*vc_AstronomicDayLenght +
				24.0 / PI * vc_DeclinationCosinus * libc.sqrt(1.0-arg_PhotAct))

	vc_ClearDayRadiation := 0.0
	if vc_PhotActRadiationMean > 0 && vc_AstronomicDayLenght > 0 {
		vc_ClearDayRadiation =
			0.5 * 1300.0 * vc_PhotActRadiationMean *
			libc.exp(-0.14 / (vc_PhotActRadiationMean / (vc_AstronomicDayLenght * 3600.0)))
	}
	// vc_OvercastDayRadiation, like vc_EffectiveDayLenght/vc_PhotoperiodicDaylength/
	// vm_AerodynamicResistance below, is computed by the C++ but never read
	// afterward (dead in the C++ itself) - kept for literal fidelity.
	vc_OvercastDayRadiation := 0.2 * vc_ClearDayRadiation
	_ = vc_OvercastDayRadiation

	SC := 24.0 * 60.0 / PI * 8.20 * (1.0 + 0.033*libc.cos(2.0*PI*f64(vs_JulianDay)/365.0))
	arg_SHA := tl.bound(-1.0, -libc.tan(vs_Latitude*PI/180.0)*libc.tan(vc_Declination*PI/180.0), 1.0)
	// The argument of acos must be in the range of -1 to 1
	SHA := libc.acos(arg_SHA)

	// [J cm-2] --> [MJ m-2]
	vc_ExtraterrestrialRadiation := SC * (SHA*vc_DeclinationSinus + vc_DeclinationCosinus*libc.sin(SHA)) / 100.0

	// Calculation of atmospheric pressure
	vm_AtmosphericPressure := 101.3 * libc.pow(((293.0-(0.0065*vs_HeightNN))/293.0), 5.26)

	// Calculation of psychrometer constant - Luftfeuchtigkeit
	vm_PsycrometerConstant := 0.000665 * vm_AtmosphericPressure

	// Calc. of saturated water vapour pressure at daily max temperature
	vm_SaturatedVapourPressureMax := 0.6108 * libc.exp((17.27*vw_MaxAirTemperature)/(237.3+vw_MaxAirTemperature))

	// Calc. of saturated water vapour pressure at daily min temperature
	vm_SaturatedVapourPressureMin := 0.6108 * libc.exp((17.27*vw_MinAirTemperature)/(237.3+vw_MinAirTemperature))

	// Calculation of the saturated water vapour pressure
	vm_SaturatedVapourPressure := (vm_SaturatedVapourPressureMax + vm_SaturatedVapourPressureMin) / 2.0

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
		(4098.0 * (0.6108 * libc.exp((17.27*vw_MeanAirTemperature)/(vw_MeanAirTemperature+237.3)))) /
		((vw_MeanAirTemperature + 237.3) * (vw_MeanAirTemperature + 237.3))

	// Calculation of wind speed in 2m height
	// 0.5 minimum allowed windspeed for Penman-Monteith-Method FAO
	vm_WindSpeed_2m := max(0.5, vw_WindSpeed*(4.87/libc.log(67.8*vw_WindSpeedHeight-5.42)))

	// Calculation of the aerodynamic resistance
	vm_AerodynamicResistance := 208.0 / vm_WindSpeed_2m
	_ = vm_AerodynamicResistance

	sm.vc_StomataResistance = 100 // FAO default value [s m-1]

	vm_SurfaceResistance := sm.vc_StomataResistance / 1.44

	vc_ClearSkySolarRadiation := (0.75 + 0.00002*vs_HeightNN) * vc_ExtraterrestrialRadiation
	vc_RelativeShortwaveRadiation :=
		vc_ClearSkySolarRadiation > 0 ? min(vw_GlobalRadiation/vc_ClearSkySolarRadiation, 1.0) : 1.0

	pc_BolzmannConstant := 0.0000000049
	vc_ShortwaveRadiation := (1.0 - pc_ReferenceAlbedo) * vw_GlobalRadiation
	vc_LongwaveRadiation :=
		pc_BolzmannConstant *
		((libc.pow(vw_MinAirTemperature+273.16, 4.0) + libc.pow(vw_MaxAirTemperature+273.16, 4.0)) / 2.0) *
		(1.35*vc_RelativeShortwaveRadiation - 0.35) *
		(0.34 - 0.14*libc.sqrt(vm_VapourPressure))
	sm.vw_NetRadiation = vc_ShortwaveRadiation - vc_LongwaveRadiation

	// Calculation of the reference evapotranspiration - Penman-Monteith-Methode FAO
	vm_ReferenceEvapotranspiration :=
		((0.408*vm_SaturatedVapourPressureSlope*sm.vw_NetRadiation) +
				(vm_PsycrometerConstant * (900.0 / (vw_MeanAirTemperature + 273.0)) * vm_WindSpeed_2m * vm_SaturationDeficit)) /
		(vm_SaturatedVapourPressureSlope +
				vm_PsycrometerConstant * (1.0 + (vm_SurfaceResistance/208.0)*vm_WindSpeed_2m))

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
	vm_SoilMoisture_m3 := sm.soilColumn.layers[i_Layer].vs_SoilMoisture_m3
	vm_PWP := sm.soilColumn.layers[i_Layer].vs_PermanentWiltingPoint
	vm_FK := sm.soilColumn.layers[i_Layer].vs_FieldCapacity
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
				vm_XSA = (0.65*vm_FK - vm_PWP) * (vm_FK - vm_PWP)
				vm_Reducer = vm_XSA + (((1 - vm_XSA) / 17.5) * (vm_ReferenceEvapotranspiration - 2.5))
			} else {
				vm_Reducer = sm.vm_XSACriticalSoilMoisture / 2.5 * vm_ReferenceEvapotranspiration
			}
			vm_CriticalSoilMoisture = sm.soilColumn.layers[i_Layer].vs_FieldCapacity * vm_Reducer
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
			vm_EReductionFactor = 0.9 - (0.625 * (0.33 - vm_RelativeEvaporableWater) / (0.33 - 0.22))
		} else if vm_RelativeEvaporableWater > 0.2 {
			vm_EReductionFactor = 0.275 - (0.225 * (0.22 - vm_RelativeEvaporableWater) / (0.22 - 0.2))
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
			(1.0 / (layerThicknessFactor * layerThicknessFactor)) * f64(2*layerNo-1)
		return deprivationFactor
	} else {
		c2 := libc.log((layerThicknessFactor + zeta*f64(layerNo)) / (layerThicknessFactor + zeta*f64(layerNo-1)))
		c3 := zeta / (layerThicknessFactor * (zeta + 1.0))
		deprivationFactor = (c2 - c3) / (libc.log(zeta+1.0) - zeta/(zeta+1.0))
		return deprivationFactor
	}
}

// C++: double monica::soilmoisture::meanWaterContent(const SoilMoisture*, double)
mean_water_content_to_depth :: proc(sm: ^Soil_Moisture, depth_m: f64) -> f64 {
	lsum := 0.0
	sum := 0.0
	count := 0

	for i in 0 ..< sm.numberOfSoilLayers {
		count += 1
		smm3 := sm.soilColumn.layers[i].vs_SoilMoisture_m3
		fc := sm.soilColumn.layers[i].vs_FieldCapacity
		pwp := sm.soilColumn.layers[i].vs_PermanentWiltingPoint
		sum += smm3 / (fc - pwp) //[%nFK]
		lsum += sm.soilColumn.layers[i].vs_LayerThickness
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

	if layer+number_of_layers > sm.numberOfSoilLayers {
		return -1
	}

	for i in layer ..< layer + number_of_layers {
		count += 1
		smm3 := sm.soilColumn.layers[i].vs_SoilMoisture_m3
		fc := sm.soilColumn.layers[i].vs_FieldCapacity
		pwp := sm.soilColumn.layers[i].vs_PermanentWiltingPoint
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
	snowDepth = sm.snowComponent.vm_SnowDepth
	temperatureUnderSnow = calc_temperature_under_snow(&sm.frostComponent, avgAirTemp, snowDepth)
	return
}
