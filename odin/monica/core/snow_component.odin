// Port of src/core/snow-component.{h,cpp}: SnowComponent and every
// snowcomponent:: proc. Fully self-contained - unlike soiltemperature, this
// file has no MonicaModel/CropModule dependency at all (confirmed by reading
// the whole C++ file: the only external state it touches is SoilColumn, via
// the same back-pointer-for-mirroring-output pattern SoilColumn.vm_SnowDepth
// already uses - see calc_snow_depth below), so nothing here needed the
// explicit-parameter deviation soil_temperature.odin needed.
package core

import p "../params"
import libc "core:c/libc"

// C++: struct monica::SnowComponent
Snow_Component :: struct {
	soil_column:                             ^Soil_Column,
	snow_density:                            f64, // [kg dm-3]
	snow_depth:                              f64, // [mm]
	frozen_water_in_snow:                    f64, // [mm]
	liquid_water_in_snow:                    f64, // [mm]
	water_to_infiltrate:                     f64, // [mm]
	max_snow_depth:                          f64, // [mm]
	accumulated_snow_depth:                  f64, // [mm]
	snowmelt_temperature:                    f64,
	snow_accumulation_threshold_temperature: f64,
	temperature_limit_for_liquid_water:      f64,
	correction_rain:                         f64,
	correction_snow:                         f64,
	refreeze_temperature:                    f64,
	refreeze_p1:                             f64,
	refreeze_p2:                             f64,
	new_snow_density_min:                    f64,
	snow_max_additional_density:             f64,
	snow_packing:                            f64,
	snow_retention_capacity_min:             f64,
	snow_retention_capacity_max:             f64,
}

// C++: void monica::snowcomponent::initialize(SnowComponent*, SoilColumn*,
//        const SoilMoistureModuleParameters&)
initialize_snow_component :: proc(
	sc: ^Snow_Component,
	soil_column: ^Soil_Column,
	smps: ^p.Soil_Moisture_Module_Parameters,
) {
	sc.soil_column = soil_column
	sc.snow_density = 0.0
	sc.snow_depth = 0.0
	sc.frozen_water_in_snow = 0.0
	sc.liquid_water_in_snow = 0.0
	sc.water_to_infiltrate = 0.0
	sc.max_snow_depth = 0.0
	sc.accumulated_snow_depth = 0.0
	sc.snowmelt_temperature = smps.snow_melt_temperature
	sc.snow_accumulation_threshold_temperature = smps.snow_accumulation_threshold_temperature
	sc.temperature_limit_for_liquid_water = smps.temperature_limit_for_liquid_water
	sc.correction_rain = smps.correction_rain
	sc.correction_snow = smps.correction_snow
	sc.refreeze_temperature = smps.refreeze_temperature
	sc.refreeze_p1 = smps.refreeze_parameter1
	sc.refreeze_p2 = smps.refreeze_parameter2
	sc.new_snow_density_min = smps.new_snow_density_min
	sc.snow_max_additional_density = smps.snow_max_additional_density
	sc.snow_packing = smps.snow_packing
	sc.snow_retention_capacity_min = smps.snow_retention_capacity_min
	sc.snow_retention_capacity_max = smps.snow_retention_capacity_max
}

// C++: void monica::snowcomponent::calcSnowLayer(SnowComponent*, double, double)
calc_snow_layer :: proc(sc: ^Snow_Component, mean_air_temperature, net_precipitation_: f64) {
	net_precipitation_snow := 0.0
	net_precipitation_water := 0.0
	net_precipitation := calc_net_precipitation(
		sc,
		mean_air_temperature,
		net_precipitation_,
		&net_precipitation_water,
		&net_precipitation_snow,
	)

	snow_melt := calc_snow_melt(sc, mean_air_temperature)
	refreeze := calc_refreeze(sc, mean_air_temperature)
	new_snow_density := calc_new_snow_density(sc, mean_air_temperature, net_precipitation_snow)
	sc.snow_density = calc_average_snow_density(sc, net_precipitation_snow, new_snow_density)

	sc.frozen_water_in_snow =
		sc.frozen_water_in_snow + net_precipitation_snow - snow_melt + refreeze
	sc.liquid_water_in_snow =
		sc.liquid_water_in_snow + net_precipitation_water + snow_melt - refreeze
	snow_water_equivalent := sc.frozen_water_in_snow + sc.liquid_water_in_snow

	liquid_water_retained_in_snow := calc_liquid_water_retained_in_snow(
		sc,
		sc.frozen_water_in_snow,
		snow_water_equivalent,
	)

	snow_layer_water_release := 0.0
	if refreeze > 0.0 {
		snow_layer_water_release = 0.0
	} else if sc.liquid_water_in_snow <= liquid_water_retained_in_snow {
		snow_layer_water_release = 0
	} else {
		snow_layer_water_release = sc.liquid_water_in_snow - liquid_water_retained_in_snow
		sc.liquid_water_in_snow -= snow_layer_water_release
		snow_water_equivalent = sc.frozen_water_in_snow + sc.liquid_water_in_snow
	}

	calc_snow_depth(sc, snow_water_equivalent)

	sc.water_to_infiltrate = calc_potential_infiltration(
		sc,
		net_precipitation,
		snow_layer_water_release,
		sc.snow_depth,
	)
}

// C++: double monica::snowcomponent::calcSnowMelt(const SnowComponent*, double)
calc_snow_melt :: proc(sc: ^Snow_Component, mean_air_temperature: f64) -> f64 {
	melting_factor := 1.4 * (sc.snow_density / 0.1)
	snow_melt := 0.0

	if melting_factor > 4.7 {
		melting_factor = 4.7
	}

	if sc.frozen_water_in_snow <= 0.0 {
		snow_melt = 0.0
	} else if mean_air_temperature < sc.snowmelt_temperature {
		snow_melt = 0.0
	} else {
		snow_melt = melting_factor * (mean_air_temperature - sc.snowmelt_temperature)
		if snow_melt > sc.frozen_water_in_snow {
			snow_melt = sc.frozen_water_in_snow
		}
	}

	return snow_melt
}

// C++: double monica::snowcomponent::calcNetPrecipitation(const SnowComponent*,
//        double, double, double&, double&)
calc_net_precipitation :: proc(
	sc: ^Snow_Component,
	mean_air_temperature, net_precipitation_: f64,
	net_precipitation_water, net_precipitation_snow: ^f64,
) -> f64 {
	liquid_water_precipitation := 0.0

	if mean_air_temperature >= sc.snow_accumulation_threshold_temperature {
		liquid_water_precipitation = 1.0
	} else if mean_air_temperature <= sc.temperature_limit_for_liquid_water {
		liquid_water_precipitation = 0.0
	} else {
		liquid_water_precipitation =
			(mean_air_temperature - sc.temperature_limit_for_liquid_water) /
			(sc.snow_accumulation_threshold_temperature - sc.temperature_limit_for_liquid_water)
	}

	net_precipitation_water^ = liquid_water_precipitation * sc.correction_rain * net_precipitation_
	net_precipitation_snow^ =
		(1.0 - liquid_water_precipitation) * sc.correction_snow * net_precipitation_

	net_precipitation := net_precipitation_snow^ + net_precipitation_water^

	return net_precipitation
}

// C++: double monica::snowcomponent::calcRefreeze(const SnowComponent*, double)
calc_refreeze :: proc(sc: ^Snow_Component, mean_air_temperature: f64) -> f64 {
	refreeze := 0.0
	refreeze_helper := 0.0

	if mean_air_temperature > 0 {
		refreeze_helper = 0
	} else {
		refreeze_helper = mean_air_temperature
	}

	if refreeze_helper < sc.refreeze_temperature {
		if sc.liquid_water_in_snow > 0.0 {
			refreeze =
				sc.refreeze_p1 *
				libc.pow(sc.refreeze_temperature - refreeze_helper, sc.refreeze_p2)
		}
		if refreeze > sc.liquid_water_in_snow {
			refreeze = sc.liquid_water_in_snow
		}
	} else {
		refreeze = 0
	}
	return refreeze
}

// C++: double monica::snowcomponent::calcNewSnowDensity(const SnowComponent*,
//        double, double)
calc_new_snow_density :: proc(
	sc: ^Snow_Component,
	mean_air_temperature, net_precipitation_snow: f64,
) -> f64 {
	new_snow_density := 0.0
	snow_density_factor := 0.0

	if net_precipitation_snow <= 0.0 {
		new_snow_density = 0.0
	} else {
		snow_density_factor =
			(mean_air_temperature - sc.temperature_limit_for_liquid_water) /
			(sc.snow_accumulation_threshold_temperature - sc.temperature_limit_for_liquid_water)
		if snow_density_factor > 1.0 {
			snow_density_factor = 1.0
		}
		if snow_density_factor < 0.0 {
			snow_density_factor = 0.0
		}
		new_snow_density =
			sc.new_snow_density_min + sc.snow_max_additional_density * snow_density_factor
	}
	return new_snow_density
}

// C++: double monica::snowcomponent::calcAverageSnowDensity(const
//        SnowComponent*, double, double)
calc_average_snow_density :: proc(
	sc: ^Snow_Component,
	net_precipitation_snow, new_snow_density: f64,
) -> f64 {
	snow_density := 0.0
	if (sc.snow_depth + net_precipitation_snow) <= 0.0 {
		snow_density = 0.0
	} else {
		snow_density =
			(((1.0 + sc.snow_packing) * sc.snow_density * sc.snow_depth) +
				(new_snow_density * net_precipitation_snow)) /
			(sc.snow_depth + net_precipitation_snow)
		if snow_density > (sc.new_snow_density_min + sc.snow_max_additional_density) {
			snow_density = sc.new_snow_density_min + sc.snow_max_additional_density
		}
	}
	return snow_density
}

// C++: double monica::snowcomponent::calcLiquidWaterRetainedInSnow(const
//        SnowComponent*, double, double)
calc_liquid_water_retained_in_snow :: proc(
	sc: ^Snow_Component,
	frozen_water_in_snow, snow_water_equivalent: f64,
) -> f64 {
	snow_retention_capacity: f64
	liquid_water_retained_in_snow: f64

	if (frozen_water_in_snow <= 0.0) || (sc.snow_density <= 0.0) {
		snow_retention_capacity = 0.0
	} else {
		snow_retention_capacity = sc.snow_retention_capacity_max / 10.0 / sc.snow_density

		if snow_retention_capacity < sc.snow_retention_capacity_min {
			snow_retention_capacity = sc.snow_retention_capacity_min
		}
		if snow_retention_capacity > sc.snow_retention_capacity_max {
			snow_retention_capacity = sc.snow_retention_capacity_max
		}
	}

	liquid_water_retained_in_snow = snow_retention_capacity * snow_water_equivalent
	return liquid_water_retained_in_snow
}

// C++: double monica::snowcomponent::calcPotentialInfiltration(SnowComponent*,
//        double, double, double)
calc_potential_infiltration :: proc(
	sc: ^Snow_Component,
	net_precipitation, snow_layer_water_release, snow_depth: f64,
) -> f64 {
	water_to_infiltrate := net_precipitation
	if snow_depth >= 0.01 {
		sc.water_to_infiltrate = snow_layer_water_release
	}
	return water_to_infiltrate
}

// C++: void monica::snowcomponent::calcSnowDepth(SnowComponent*, double)
calc_snow_depth :: proc(sc: ^Snow_Component, snow_water_equivalent: f64) {
	water_density := 1.0 // [kg dm-3]
	if snow_water_equivalent <= 0.0 {
		sc.snow_depth = 0.0
	} else {
		sc.snow_depth = snow_water_equivalent * water_density / sc.snow_density

		if sc.snow_depth > sc.max_snow_depth {
			sc.max_snow_depth = sc.snow_depth
		}

		if sc.snow_depth < 0.01 {
			sc.snow_depth = 0.0
		}
	}
	if sc.snow_depth == 0.0 {
		sc.snow_density = 0.0
		sc.frozen_water_in_snow = 0.0
		sc.liquid_water_in_snow = 0.0
	}

	if sc.soil_column != nil {
		sc.soil_column.snow_depth_mm = sc.snow_depth
	}
	sc.accumulated_snow_depth += sc.snow_depth
}
