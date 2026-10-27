package monica_evapotranspiration

import "core:slice"
import cr_math "../../support/cr_math"

// - Name: Evapotranspiration -Version: 1, -Time step: 1
// - Description:
//             * Title: MONICA evapotranspiration model
//             * Authors: Claas Nendel (transcription into Crop2ML by Michael Berg-Mohnicke)
//             * Reference: None
//             * Institution: ZALF e.V.
//             * ExtendedDescription: None
//             * ShortDescription: Calculates the MONICA evapotranspiration
Evapotranspiration :: struct {
	evaporation_zeta: f64, // shape factor [dimensionless]
	maximum_evaporation_impact_depth: f64, // maximumEvaporationImpactDepth [dm]
	evaporation_reduction_method: int, // THESEUS (0) or HERMES (1) evaporation reduction method [dimensionless]
	xsa_critical_soil_moisture: f64, // XSACriticalSoilMoisture [m3/m3]
	no_of_soil_layers: int, // number of soil layers [dimensionless]
	no_of_soil_moisture_layers: int, // number of soil layers + 1 [dimensionless]
	layer_thickness: [dynamic]f64, // layer thickness array [m]
	permanent_wilting_point: [dynamic]f64, // permanent wilting point array [m3/m3]
	field_capacity: [dynamic]f64, // field capacity array [m3/m3]
}

make_evapotranspiration :: proc(allocator := context.allocator) -> (m: Evapotranspiration) {
	context.allocator = allocator
	m.evaporation_zeta = 40
	m.maximum_evaporation_impact_depth = 5
	m.evaporation_reduction_method = 1
	m.xsa_critical_soil_moisture = 0.1
	m.no_of_soil_layers = 20
	m.no_of_soil_moisture_layers = 21
	return
}

destroy_evapotranspiration :: proc(m: ^Evapotranspiration) {
	delete(m.layer_thickness)
	delete(m.permanent_wilting_point)
	delete(m.field_capacity)
}

evapotranspiration_init :: proc(m: ^Evapotranspiration, s: ^Et_State, s1: ^Et_State, r: ^Et_Rate, a: ^Et_Auxiliary, ex: ^Et_Exogenous, allocator := context.allocator) {
	context.allocator = allocator
	s.surface_water_storage = 0.0
	s.evaporated_from_surface = 0.0
	s.actual_evaporation = 0.0
	s.actual_transpiration = 0.0
	resize(&s.soil_moisture, m.no_of_soil_moisture_layers)
	resize(&s.evaporation, m.no_of_soil_moisture_layers)
	resize(&s.transpiration, m.no_of_soil_moisture_layers)
	resize(&s.evapotranspiration, m.no_of_soil_moisture_layers)
	s.actual_evapotranspiration = 0.0
	resize(&s.soil_moisture, m.no_of_soil_moisture_layers)
	slice.fill(s.soil_moisture[:], 0.0)
	resize(&s.evaporation, m.no_of_soil_moisture_layers)
	slice.fill(s.evaporation[:], 0.0)
	resize(&s.transpiration, m.no_of_soil_moisture_layers)
	slice.fill(s.transpiration[:], 0.0)
	resize(&s.evapotranspiration, m.no_of_soil_moisture_layers)
	slice.fill(s.evapotranspiration[:], 0.0)
}

// - inputs:
//             * name: evaporation_zeta
//                           ** description : shape factor
//                           ** inputtype : parameter
//                           ** parametercategory : constant
//                           ** datatype : DOUBLE
//                           ** max : 40
//                           ** min : 0
//                           ** default : 40
//                           ** unit : dimensionless
//             * name: maximum_evaporation_impact_depth
//                           ** description : maximumEvaporationImpactDepth
//                           ** inputtype : parameter
//                           ** parametercategory : constant
//                           ** datatype : DOUBLE
//                           ** max :
//                           ** min : 0
//                           ** default : 5
//                           ** unit : dm
//             * name: evaporation_reduction_method
//                           ** description : THESEUS (0) or HERMES (1) evaporation reduction method
//                           ** inputtype : parameter
//                           ** parametercategory : constant
//                           ** datatype : INT
//                           ** max : 1
//                           ** min : 0
//                           ** default : 1
//                           ** unit : dimensionless
//             * name: xsa_critical_soil_moisture
//                           ** description : XSACriticalSoilMoisture
//                           ** inputtype : parameter
//                           ** parametercategory : constant
//                           ** datatype : DOUBLE
//                           ** max : 1.5
//                           ** min : 0
//                           ** default : 0.1
//                           ** unit : m3/m3
//             * name: no_of_soil_layers
//                           ** description : number of soil layers
//                           ** inputtype : parameter
//                           ** parametercategory : constant
//                           ** datatype : INT
//                           ** max :
//                           ** min : 0
//                           ** default : 20
//                           ** unit : dimensionless
//             * name: no_of_soil_moisture_layers
//                           ** description : number of soil layers + 1
//                           ** inputtype : parameter
//                           ** parametercategory : constant
//                           ** datatype : INT
//                           ** max :
//                           ** min : 0
//                           ** default : 21
//                           ** unit : dimensionless
//             * name: layer_thickness
//                           ** description : layer thickness array
//                           ** inputtype : parameter
//                           ** parametercategory : constant
//                           ** datatype : DOUBLEARRAY
//                           ** len : no_of_soil_moisture_layers
//                           ** max :
//                           ** min :
//                           ** default :
//                           ** unit : m
//             * name: permanent_wilting_point
//                           ** description : permanent wilting point array
//                           ** inputtype : parameter
//                           ** parametercategory : constant
//                           ** datatype : DOUBLEARRAY
//                           ** len : no_of_soil_moisture_layers
//                           ** max :
//                           ** min :
//                           ** default :
//                           ** unit : m3/m3
//             * name: field_capacity
//                           ** description : field capacity array
//                           ** inputtype : parameter
//                           ** parametercategory : constant
//                           ** datatype : DOUBLEARRAY
//                           ** len : no_of_soil_moisture_layers
//                           ** max :
//                           ** min :
//                           ** default :
//                           ** unit : m3/m3
//             * name: kc_factor
//                           ** description : crop coefficient ETc/ET0
//                           ** inputtype : variable
//                           ** variablecategory : exogenous
//                           ** datatype : DOUBLE
//                           ** max :
//                           ** min : 0
//                           ** default : 0.75
//                           ** unit : dimensionless
//             * name: has_snow_cover
//                           ** description : is the soil covered by snow
//                           ** inputtype : variable
//                           ** variablecategory : exogenous
//                           ** datatype : BOOLEAN
//                           ** default : false
//                           ** unit : dimensionless
//             * name: developmental_stage
//                           ** description : MONICA crop developmental stage
//                           ** inputtype : variable
//                           ** variablecategory : exogenous
//                           ** datatype : INT
//                           ** max : 6
//                           ** min : 0
//                           ** default : 0
//                           ** unit : dimensionless
//             * name: crop_transpiration
//                           ** description : crop transpiration array
//                           ** inputtype : variable
//                           ** variablecategory : exogenous
//                           ** datatype : DOUBLEARRAY
//                           ** len : no_of_soil_moisture_layers
//                           ** max :
//                           ** min :
//                           ** default :
//                           ** unit : mm
//             * name: crop_evaporated_from_intercepted
//                           ** description : crop evaporated water from intercepted water
//                           ** inputtype : variable
//                           ** variablecategory : exogenous
//                           ** datatype : DOUBLE
//                           ** max :
//                           ** min :
//                           ** default :
//                           ** unit : mm
//             * name: percentage_soil_coverage
//                           ** description : fraction of soil covered by crop
//                           ** inputtype : variable
//                           ** variablecategory : exogenous
//                           ** datatype : DOUBLE
//                           ** max : 1
//                           ** min : 0
//                           ** default : 0
//                           ** unit : m2/m2
//             * name: potential_evapotranspiration
//                           ** description : the potential evapotranspiration
//                           ** inputtype : variable
//                           ** variablecategory : auxiliary
//                           ** datatype : DOUBLE
//                           ** max :
//                           ** min : 0
//                           ** default : 0
//                           ** unit : mm
//             * name: surface_water_storage
//                           ** description : Simulates a virtual layer that contains the surface water
//                           ** inputtype : variable
//                           ** variablecategory : state
//                           ** datatype : DOUBLE
//                           ** max :
//                           ** min : 0
//                           ** default : 0
//                           ** unit : mm
//             * name: evaporated_from_surface
//                           ** description : evaporated_from_surface
//                           ** inputtype : variable
//                           ** variablecategory : state
//                           ** datatype : DOUBLE
//                           ** max :
//                           ** min : 0
//                           ** default : 0
//                           ** unit : mm
//             * name: actual_evaporation
//                           ** description : actual evaporation
//                           ** inputtype : variable
//                           ** variablecategory : state
//                           ** datatype : DOUBLE
//                           ** max :
//                           ** min : 0
//                           ** default : 0
//                           ** unit : mm
//             * name: actual_transpiration
//                           ** description : actual transpiration
//                           ** inputtype : variable
//                           ** variablecategory : state
//                           ** datatype : DOUBLE
//                           ** max :
//                           ** min : 0
//                           ** default : 0
//                           ** unit : mm
//             * name: soil_moisture
//                           ** description : soil moisture array
//                           ** inputtype : variable
//                           ** variablecategory : state
//                           ** datatype : DOUBLEARRAY
//                           ** len : no_of_soil_moisture_layers
//                           ** max :
//                           ** min :
//                           ** default :
//                           ** unit : m3/m3
//             * name: evaporation
//                           ** description : evaporation array
//                           ** inputtype : variable
//                           ** variablecategory : state
//                           ** datatype : DOUBLEARRAY
//                           ** len : no_of_soil_moisture_layers
//                           ** max :
//                           ** min :
//                           ** default :
//                           ** unit : mm
//             * name: transpiration
//                           ** description : transpiration array
//                           ** inputtype : variable
//                           ** variablecategory : state
//                           ** datatype : DOUBLEARRAY
//                           ** len : no_of_soil_moisture_layers
//                           ** max :
//                           ** min :
//                           ** default :
//                           ** unit : mm
//             * name: evapotranspiration
//                           ** description : evapotranspiration array
//                           ** inputtype : variable
//                           ** variablecategory : state
//                           ** datatype : DOUBLEARRAY
//                           ** len : no_of_soil_moisture_layers
//                           ** max :
//                           ** min :
//                           ** default :
//                           ** unit : mm
//             * name: actual_evapotranspiration
//                           ** description : actual evapotranspiration
//                           ** inputtype : variable
//                           ** variablecategory : state
//                           ** datatype : DOUBLE
//                           ** max :
//                           ** min : 0
//                           ** default : 0
//                           ** unit : mm
// - outputs:
//             * name: evaporated_from_surface
//                           ** description :
//                           ** variablecategory : state
//                           ** datatype : DOUBLE
//                           ** max :
//                           ** min : 0
//                           ** unit : mm
//             * name: actual_evapotranspiration
//                           ** description : actual evapotranspiration
//                           ** variablecategory : state
//                           ** datatype : DOUBLE
//                           ** max : 200
//                           ** min : 0
//                           ** unit : mm
//             * name: actual_evaporation
//                           ** description : actual evaporation
//                           ** variablecategory : state
//                           ** datatype : DOUBLE
//                           ** max :
//                           ** min : 0
//                           ** unit : mm
//             * name: actual_transpiration
//                           ** description : actual transpiration
//                           ** variablecategory : state
//                           ** datatype : DOUBLE
//                           ** max :
//                           ** min : 0
//                           ** unit : mm
//             * name: surface_water_storage
//                           ** description : Simulates a virtual layer that contains the surface water
//                           ** variablecategory : state
//                           ** datatype : DOUBLE
//                           ** max :
//                           ** min : 0
//                           ** unit : mm
//             * name: soil_moisture
//                           ** description : soil moisture array
//                           ** variablecategory : state
//                           ** datatype : DOUBLEARRAY
//                           ** len : no_of_soil_moisture_layers
//                           ** max :
//                           ** min :
//                           ** unit : m3/m3
evapotranspiration_calculate_model :: proc(m: ^Evapotranspiration, s: ^Et_State, s1: ^Et_State, r: ^Et_Rate, a: ^Et_Auxiliary, ex: ^Et_Exogenous, allocator := context.allocator) {
	context.allocator = allocator
	evaporated_from_intercept: f64
	if ex.developmental_stage > 0 {
		evaporated_from_intercept = ex.crop_evaporated_from_intercepted
	} else {
		evaporated_from_intercept = 0.0
	}
	s.evaporated_from_surface = 0.0
	s.actual_evaporation = 0.0
	s.actual_transpiration = 0.0
	evaporation_from_surface: bool = false
	e_red1: f64
	e_red2: f64
	e_red3: f64
	e_reducer: f64
	i: int
	if a.potential_evapotranspiration > 0.0 {
		evaporation_from_surface = false
		if s.surface_water_storage > 0.0 {
			evaporation_from_surface = true
			a.potential_evapotranspiration = a.potential_evapotranspiration * (1.1 / ex.kc_factor)
			if ex.has_snow_cover {
				s.evaporated_from_surface = 0.0
			} else if s.surface_water_storage < a.potential_evapotranspiration {
				a.potential_evapotranspiration = a.potential_evapotranspiration - s.surface_water_storage
				s.evaporated_from_surface = s.surface_water_storage
				s.surface_water_storage = 0.0
			} else {
				s.surface_water_storage = s.surface_water_storage - a.potential_evapotranspiration
				s.evaporated_from_surface = a.potential_evapotranspiration
				a.potential_evapotranspiration = 0.0
			}
			a.potential_evapotranspiration = a.potential_evapotranspiration * (ex.kc_factor / 1.1)
		}
		if a.potential_evapotranspiration > 0.0 {
			for i = 0; i < m.no_of_soil_layers; i += 1 {
				e_red1 = e_reducer_1(m.permanent_wilting_point[i], m.field_capacity[i], s.soil_moisture[i], ex.percentage_soil_coverage, a.potential_evapotranspiration, m.evaporation_reduction_method, m.xsa_critical_soil_moisture)
				e_red2 = 0.0
				if f64(i) >= m.maximum_evaporation_impact_depth {
					e_red2 = 0.0
				} else {
					e_red2 = get_deprivation_factor(i + 1, m.maximum_evaporation_impact_depth, m.evaporation_zeta, m.layer_thickness[i])
				}
				e_red3 = 0.0
				if i > 0 && s.soil_moisture[i] < s.soil_moisture[i - 1] {
					e_red3 = 0.1
				} else {
					e_red3 = 1.0
				}
				e_reducer = e_red1 * e_red2 * e_red3
				if ex.developmental_stage > 0 {
					if ex.percentage_soil_coverage >= 0.0 && ex.percentage_soil_coverage < 1.0 {
						s.evaporation[i] = (1.0 - ex.percentage_soil_coverage) * e_reducer * a.potential_evapotranspiration
					} else if ex.percentage_soil_coverage >= 1.0 {
						s.evaporation[i] = 0.0
					}
					if ex.has_snow_cover {
						s.evaporation[i] = 0.0
					}
					s.transpiration[i] = ex.crop_transpiration[i]
					if evaporation_from_surface {
						s.transpiration[i] = ex.percentage_soil_coverage * e_reducer * a.potential_evapotranspiration
					}
				} else {
					if ex.has_snow_cover {
						s.evaporation[i] = 0.0
					} else {
						s.evaporation[i] = a.potential_evapotranspiration * e_reducer
					}
					s.transpiration[i] = 0.0
				}
				s.evapotranspiration[i] = s.evaporation[i] + s.transpiration[i]
				s.soil_moisture[i] = s.soil_moisture[i] - s.evapotranspiration[i] / 1000.0 / m.layer_thickness[i]
				if s.soil_moisture[i] < 0.01 {
					s.soil_moisture[i] = 0.01
				}
				s.actual_transpiration = s.actual_transpiration + s.transpiration[i]
				s.actual_evaporation = s.actual_evaporation + s.evaporation[i]
			}
		}
	}
	s.actual_evapotranspiration = s.actual_transpiration + s.actual_evaporation + evaporated_from_intercept + s.evaporated_from_surface
}

@(private="file")
bound :: proc(lower: f64, value: f64, upper: f64) -> f64 {
	if value < lower {
		return lower
	}
	if value > upper {
		return upper
	}
	return value
}

@(private="file")
get_deprivation_factor :: proc(layer_no: int, deprivation_depth: f64, zeta: f64, layer_thickness: f64) -> f64 {
	ltf: f64
	ltf = deprivation_depth / (layer_thickness * 10.0)
	deprivation_factor: f64
	c2: f64
	c3: f64
	if abs(zeta) < 0.0003 {
		deprivation_factor = 2.0 / ltf - 1.0 / (ltf * ltf) * f64(2 * layer_no - 1)
	} else {
		c2 = cr_math.log((ltf + zeta * f64(layer_no)) / (ltf + zeta * f64(layer_no - 1)))
		c3 = zeta / (ltf * (zeta + 1.0))
		deprivation_factor = (c2 - c3) / (cr_math.log(zeta + 1.0) - zeta / (zeta + 1.0))
	}
	return deprivation_factor
}

@(private="file")
e_reducer_1 :: proc(pwp: f64, fc: f64, sm: f64, percentage_soil_coverage: f64, reference_evapotranspiration: f64, evaporation_reduction_method: int, xsa_critical_soil_moisture: f64) -> f64 {
	sm := sm
	sm = max(0.33 * pwp, sm)
	relative_evaporable_water: f64
	relative_evaporable_water = min(1.0, (sm - 0.33 * pwp) / (fc - 0.33 * pwp))
	e_reduction_factor: f64 = 0.0
	critical_soil_moisture: f64
	reducer: f64
	xsa: f64
	if evaporation_reduction_method == 0 {
		critical_soil_moisture = 0.65 * fc
		if percentage_soil_coverage > 0.0 {
			reducer = 1.0
			if reference_evapotranspiration > 2.5 {
				xsa = (0.65 * fc - pwp) * (fc - pwp)
				reducer = xsa + (1 - xsa) / 17.5 * (reference_evapotranspiration - 2.5)
			} else {
				reducer = xsa_critical_soil_moisture / 2.5 * reference_evapotranspiration
			}
			critical_soil_moisture = fc * reducer
		}
		if sm > critical_soil_moisture {
			e_reduction_factor = 1.0
		} else if sm > 0.33 * pwp {
			e_reduction_factor = relative_evaporable_water
		} else {
			e_reduction_factor = 0.0
		}
	} else {
		if relative_evaporable_water > 0.33 {
			e_reduction_factor = 1.0 - 0.1 * (1.0 - relative_evaporable_water) / (1.0 - 0.33)
		} else if relative_evaporable_water > 0.22 {
			e_reduction_factor = 0.9 - 0.625 * (0.33 - relative_evaporable_water) / (0.33 - 0.22)
		} else if relative_evaporable_water > 0.2 {
			e_reduction_factor = 0.275 - 0.225 * (0.22 - relative_evaporable_water) / (0.22 - 0.2)
		} else {
			e_reduction_factor = 0.05 - 0.05 * (0.2 - relative_evaporable_water) / 0.2
		}
	}
	return e_reduction_factor
}