// Port of src/core/frost-component.{h,cpp}: FrostComponent and every
// frostcomponent:: proc. Fully self-contained, like snow_component.odin - no
// MonicaModel/CropModule dependency anywhere in the C++ file.
package core

import libc "core:c/libc"

// C++: struct monica::FrostComponent
Frost_Component :: struct {
	soilColumn:                   ^Soil_Column,
	vm_FrostDepth:                f64,
	vm_accumulatedFrostDepth:     f64,
	vm_NegativeDegreeDays:        f64, // negative degree-days under snow
	vm_ThawDepth:                 f64,
	vm_FrostDays:                 int,
	vm_LambdaRedux:               [dynamic]f64, // reduction factor for Lambda []
	vm_TemperatureUnderSnow:      f64,
	vm_HydraulicConductivityRedux: f64,
	pt_TimeStep:                  f64,
	pm_HydraulicConductivityRedux: f64,
}

// C++: void monica::frostcomponent::initialize(FrostComponent*, SoilColumn*,
//        double, double)
initialize_frost_component :: proc(
	fc: ^Frost_Component,
	soil_column: ^Soil_Column,
	pm_hydraulic_conductivity_redux, p_time_step: f64,
	allocator := context.allocator,
) {
	fc.soilColumn = soil_column
	fc.vm_FrostDepth = 0.0
	fc.vm_accumulatedFrostDepth = 0.0
	fc.vm_NegativeDegreeDays = 0.0
	fc.vm_ThawDepth = 0.0
	fc.vm_FrostDays = 0
	resize(&fc.vm_LambdaRedux, number_of_layers(soil_column) + 1)
	for i in 0 ..< len(fc.vm_LambdaRedux) {
		fc.vm_LambdaRedux[i] = 1.0
	}
	fc.vm_TemperatureUnderSnow = 0.0
	fc.vm_HydraulicConductivityRedux = pm_hydraulic_conductivity_redux
	fc.pt_TimeStep = p_time_step
	fc.pm_HydraulicConductivityRedux = pm_hydraulic_conductivity_redux
}

// C++: void monica::frostcomponent::calcSoilFrost(FrostComponent*, double, double)
calc_soil_frost :: proc(fc: ^Frost_Component, mean_air_temperature, snow_depth: f64) {
	mean_field_capacity := get_mean_field_capacity(fc)
	mean_bulk_density := get_mean_bulk_density(fc)

	sii := calc_sii(mean_field_capacity)
	heat_conductivity_frozen := calc_heat_conductivity_frozen(fc, mean_bulk_density, sii)
	heat_conductivity_unfrozen := calc_heat_conductivity_unfrozen(
		fc,
		mean_bulk_density,
		mean_field_capacity,
	)

	fc.vm_TemperatureUnderSnow = calc_temperature_under_snow(fc, mean_air_temperature, snow_depth)

	fc.vm_FrostDepth = calc_frost_depth(
		fc,
		mean_field_capacity,
		heat_conductivity_frozen,
		fc.vm_TemperatureUnderSnow,
	)
	fc.vm_accumulatedFrostDepth += fc.vm_FrostDepth

	fc.vm_ThawDepth = calc_thaw_depth(
		fc,
		fc.vm_TemperatureUnderSnow,
		heat_conductivity_unfrozen,
		mean_field_capacity,
	)

	update_lambda_redux(fc)
}

// C++: double monica::frostcomponent::getMeanBulkDensity(const FrostComponent*)
get_mean_bulk_density :: proc(fc: ^Frost_Component) -> f64 {
	sc := fc.soilColumn
	vs_number_of_layers := number_of_layers(sc)
	bulk_density_accu := 0.0
	for i_layer in 0 ..< vs_number_of_layers {
		bulk_density_accu += soil_bulk_density(&sc.layers[i_layer])
	}
	return bulk_density_accu / f64(vs_number_of_layers) / 1000.0 // [Mg m-3]
}

// C++: double monica::frostcomponent::getMeanFieldCapacity(const FrostComponent*)
get_mean_field_capacity :: proc(fc: ^Frost_Component) -> f64 {
	sc := fc.soilColumn
	vs_number_of_layers := number_of_layers(sc)
	mean_field_capacity_accu := 0.0
	for i_layer in 0 ..< vs_number_of_layers {
		mean_field_capacity_accu += sc.layers[i_layer].vs_FieldCapacity
	}
	return mean_field_capacity_accu / f64(vs_number_of_layers)
}

// C++: double monica::frostcomponent::calcSii(double)
calc_sii :: proc(mean_field_capacity: f64) -> f64 {
	pt_F1 := 13.05 // Hansson et al. 2004
	pt_F2 := 1.06  // Hansson et al. 2004

	sii :=
		(mean_field_capacity +
				(1.0 + (pt_F1 * libc.pow(mean_field_capacity, pt_F2)) * mean_field_capacity)) *
		100.0
	return sii
}

// C++: double monica::frostcomponent::calcHeatConductivityFrozen(const
//        FrostComponent*, double, double)
calc_heat_conductivity_frozen :: proc(fc: ^Frost_Component, mean_bulk_density, sii: f64) -> f64 {
	cond_frozen :=
		((3.0 * mean_bulk_density - 1.7) * 0.001) /
		(1.0 +
				(11.5 - 5.0 * mean_bulk_density) *
					libc.exp((-50.0) * libc.pow((sii / mean_bulk_density), 1.5))) *
		86400.0 *
		fc.pt_TimeStep *
		4.184 /
		1000000.0 *
		100

	return cond_frozen
}

// C++: double monica::frostcomponent::calcHeatConductivityUnfrozen(const
//        FrostComponent*, double, double)
calc_heat_conductivity_unfrozen :: proc(
	fc: ^Frost_Component,
	mean_bulk_density, mean_field_capacity: f64,
) -> f64 {
	cond_unfrozen :=
		((3.0 * mean_bulk_density - 1.7) * 0.001) /
		(1.0 +
				(11.5 - 5.0 * mean_bulk_density) *
					libc.exp((-50.0) * libc.pow(((mean_field_capacity * 100.0) / mean_bulk_density), 1.5))) *
		fc.pt_TimeStep *
		4.184 *
		100.0

	return cond_unfrozen
}

// C++: double monica::frostcomponent::calcThawDepth(const FrostComponent*,
//        double, double, double)
calc_thaw_depth :: proc(
	fc: ^Frost_Component,
	temperature_under_snow, heat_conductivity_unfrozen, mean_field_capacity: f64,
) -> f64 {
	thaw_helper1 := 0.0
	thaw_helper2 := 0.0
	thaw_helper3 := 0.0
	thaw_helper4 := 0.0

	thaw_depth := 0.0

	if temperature_under_snow < 0.0 {
		thaw_helper1 = temperature_under_snow * -1.0
	} else {
		thaw_helper1 = temperature_under_snow
	}

	if fc.vm_FrostDepth == 0.0 {
		thaw_helper2 = 0.0
	} else {
		thaw_helper2 = libc.sqrt(
			2.0 * heat_conductivity_unfrozen * thaw_helper1 /
			(1000.0 * 79.0 * (mean_field_capacity * 100.0) / 100.0),
		)
	}

	if temperature_under_snow < 0.0 {
		thaw_helper3 = thaw_helper2 * -1.0
	} else {
		thaw_helper3 = thaw_helper2
	}

	thaw_helper4 = fc.vm_ThawDepth + thaw_helper3

	if thaw_helper4 < 0.0 {
		thaw_depth = 0.0
	} else {
		thaw_depth = thaw_helper4
	}
	return thaw_depth
}

// C++: double monica::frostcomponent::calcFrostDepth(FrostComponent*, double,
//        double, double)
calc_frost_depth :: proc(
	fc: ^Frost_Component,
	mean_field_capacity, heat_conductivity_frozen, temperature_under_snow: f64,
) -> f64 {
	frost_depth := 0.0

	latent_heat := 1000.0 * (mean_field_capacity * 100.0) / 100.0 * 0.335

	if fc.vm_FrostDepth > 0.0 {
		fc.vm_FrostDays += 1
	}

	latent_heat_transfer := 0.3 * f64(fc.vm_FrostDays) / latent_heat

	if temperature_under_snow < 0.0 {
		fc.vm_NegativeDegreeDays -= temperature_under_snow
	}

	if fc.vm_NegativeDegreeDays < 0.01 {
		frost_depth = 0.0
	} else {
		frost_depth =
			libc.sqrt(
				((latent_heat_transfer / 2.0) * (latent_heat_transfer / 2.0)) +
				(2.0 * heat_conductivity_frozen * fc.vm_NegativeDegreeDays / latent_heat),
			) -
			(latent_heat_transfer / 2.0)
	}
	return frost_depth
}

// C++: double monica::frostcomponent::calcTemperatureUnderSnow(const
//        FrostComponent*, double, double)
calc_temperature_under_snow :: proc(
	fc: ^Frost_Component,
	mean_air_temperature, snow_depth: f64,
) -> f64 {
	temperature_under_snow := 0.0
	if snow_depth / 100.0 < 0.01 {
		temperature_under_snow = mean_air_temperature
	} else if fc.vm_FrostDepth < 0.01 {
		temperature_under_snow = mean_air_temperature
	} else {
		temperature_under_snow =
			mean_air_temperature / (1.0 + (10.0 * snow_depth / 100.0) / fc.vm_FrostDepth)
	}
	return temperature_under_snow
}

// C++: void monica::frostcomponent::updateLambdaRedux(FrostComponent*)
update_lambda_redux :: proc(fc: ^Frost_Component) {
	sc := fc.soilColumn
	vs_number_of_layers := number_of_layers(sc)

	for i_layer in 0 ..< vs_number_of_layers {
		if f64(i_layer) < libc.floor((fc.vm_FrostDepth/sc.layers[i_layer].vs_LayerThickness)+0.5) {
			// soil layer is frozen
			sc.layers[i_layer].vs_SoilFrozen = true
			fc.vm_LambdaRedux[i_layer] = 0.0

			if i_layer == 0 {
				fc.vm_HydraulicConductivityRedux = 0.0
			}
		}

		if f64(i_layer) < libc.floor((fc.vm_ThawDepth/sc.layers[i_layer].vs_LayerThickness)+0.5) {
			// soil layer is thawing
			if fc.vm_ThawDepth < (f64(i_layer + 1) * sc.layers[i_layer].vs_LayerThickness) &&
			   (fc.vm_ThawDepth < fc.vm_FrostDepth) {
				// soil layer is thawing but there is more frost than thaw
				sc.layers[i_layer].vs_SoilFrozen = true
				fc.vm_LambdaRedux[i_layer] = 0.0
				if i_layer == 0 {
					fc.vm_HydraulicConductivityRedux = 0.0
				}
			} else {
				// soil is thawing
				sc.layers[i_layer].vs_SoilFrozen = false
				fc.vm_LambdaRedux[i_layer] = 1.0
				if i_layer == 0 {
					fc.vm_HydraulicConductivityRedux = 0.1
				}
			}
		}

		// no more frost, because all layers are thawing
		if fc.vm_ThawDepth >= fc.vm_FrostDepth {
			fc.vm_ThawDepth = 0.0
			fc.vm_FrostDepth = 0.0
			fc.vm_NegativeDegreeDays = 0.0
			fc.vm_FrostDays = 0

			fc.vm_HydraulicConductivityRedux = fc.pm_HydraulicConductivityRedux
			for j_layer in 0 ..< vs_number_of_layers {
				sc.layers[j_layer].vs_SoilFrozen = false
				fc.vm_LambdaRedux[j_layer] = 1.0
			}
		}
	}
}
