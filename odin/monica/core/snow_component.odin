// Port of src/core/snow-component.{h,cpp}: SnowComponent and every
// snowcomponent:: proc. Fully self-contained - unlike soiltemperature, this
// file has no MonicaModel/CropModule dependency at all (confirmed by reading
// the whole C++ file: the only external state it touches is SoilColumn, via
// the same back-pointer-for-mirroring-output pattern SoilColumn.vm_SnowDepth
// already uses - see calc_snow_depth below), so nothing here needed the
// explicit-parameter deviation soil_temperature.odin needed.
package core

import libc "core:c/libc"
import p "../params"

// C++: struct monica::SnowComponent
Snow_Component :: struct {
	soilColumn:                          ^Soil_Column,
	vm_SnowDensity:                      f64, // [kg dm-3]
	vm_SnowDepth:                        f64, // [mm]
	vm_FrozenWaterInSnow:                f64, // [mm]
	vm_LiquidWaterInSnow:                f64, // [mm]
	vm_WaterToInfiltrate:                f64, // [mm]
	vm_maxSnowDepth:                     f64, // [mm]
	vm_AccumulatedSnowDepth:             f64, // [mm]
	vm_SnowmeltTemperature:              f64,
	vm_SnowAccumulationThresholdTemperature: f64,
	vm_TemperatureLimitForLiquidWater:   f64,
	vm_CorrectionRain:                   f64,
	vm_CorrectionSnow:                   f64,
	vm_RefreezeTemperature:              f64,
	vm_RefreezeP1:                       f64,
	vm_RefreezeP2:                       f64,
	vm_NewSnowDensityMin:                f64,
	vm_SnowMaxAdditionalDensity:         f64,
	vm_SnowPacking:                      f64,
	vm_SnowRetentionCapacityMin:         f64,
	vm_SnowRetentionCapacityMax:         f64,
}

// C++: void monica::snowcomponent::initialize(SnowComponent*, SoilColumn*,
//        const SoilMoistureModuleParameters&)
initialize_snow_component :: proc(
	sc: ^Snow_Component,
	soil_column: ^Soil_Column,
	smps: ^p.Soil_Moisture_Module_Parameters,
) {
	sc.soilColumn = soil_column
	sc.vm_SnowDensity = 0.0
	sc.vm_SnowDepth = 0.0
	sc.vm_FrozenWaterInSnow = 0.0
	sc.vm_LiquidWaterInSnow = 0.0
	sc.vm_WaterToInfiltrate = 0.0
	sc.vm_maxSnowDepth = 0.0
	sc.vm_AccumulatedSnowDepth = 0.0
	sc.vm_SnowmeltTemperature = smps.pm_SnowMeltTemperature
	sc.vm_SnowAccumulationThresholdTemperature = smps.pm_SnowAccumulationTresholdTemperature
	sc.vm_TemperatureLimitForLiquidWater = smps.pm_TemperatureLimitForLiquidWater
	sc.vm_CorrectionRain = smps.pm_CorrectionRain
	sc.vm_CorrectionSnow = smps.pm_CorrectionSnow
	sc.vm_RefreezeTemperature = smps.pm_RefreezeTemperature
	sc.vm_RefreezeP1 = smps.pm_RefreezeParameter1
	sc.vm_RefreezeP2 = smps.pm_RefreezeParameter2
	sc.vm_NewSnowDensityMin = smps.pm_NewSnowDensityMin
	sc.vm_SnowMaxAdditionalDensity = smps.pm_SnowMaxAdditionalDensity
	sc.vm_SnowPacking = smps.pm_SnowPacking
	sc.vm_SnowRetentionCapacityMin = smps.pm_SnowRetentionCapacityMin
	sc.vm_SnowRetentionCapacityMax = smps.pm_SnowRetentionCapacityMax
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

	vm_Snowmelt := calc_snow_melt(sc, mean_air_temperature)
	vm_Refreeze := calc_refreeze(sc, mean_air_temperature)
	vm_NewSnowDensity := calc_new_snow_density(sc, mean_air_temperature, net_precipitation_snow)
	sc.vm_SnowDensity = calc_average_snow_density(sc, net_precipitation_snow, vm_NewSnowDensity)

	sc.vm_FrozenWaterInSnow =
		sc.vm_FrozenWaterInSnow + net_precipitation_snow - vm_Snowmelt + vm_Refreeze
	sc.vm_LiquidWaterInSnow =
		sc.vm_LiquidWaterInSnow + net_precipitation_water + vm_Snowmelt - vm_Refreeze
	vm_SnowWaterEquivalent := sc.vm_FrozenWaterInSnow + sc.vm_LiquidWaterInSnow

	vm_LiquidWaterRetainedInSnow := calc_liquid_water_retained_in_snow(
		sc,
		sc.vm_FrozenWaterInSnow,
		vm_SnowWaterEquivalent,
	)

	vm_SnowLayerWaterRelease := 0.0
	if vm_Refreeze > 0.0 {
		vm_SnowLayerWaterRelease = 0.0
	} else if sc.vm_LiquidWaterInSnow <= vm_LiquidWaterRetainedInSnow {
		vm_SnowLayerWaterRelease = 0
	} else {
		vm_SnowLayerWaterRelease = sc.vm_LiquidWaterInSnow - vm_LiquidWaterRetainedInSnow
		sc.vm_LiquidWaterInSnow -= vm_SnowLayerWaterRelease
		vm_SnowWaterEquivalent = sc.vm_FrozenWaterInSnow + sc.vm_LiquidWaterInSnow
	}

	calc_snow_depth(sc, vm_SnowWaterEquivalent)

	sc.vm_WaterToInfiltrate = calc_potential_infiltration(
		sc,
		net_precipitation,
		vm_SnowLayerWaterRelease,
		sc.vm_SnowDepth,
	)
}

// C++: double monica::snowcomponent::calcSnowMelt(const SnowComponent*, double)
calc_snow_melt :: proc(sc: ^Snow_Component, vw_MeanAirTemperature: f64) -> f64 {
	vm_MeltingFactor := 1.4 * (sc.vm_SnowDensity / 0.1)
	vm_Snowmelt := 0.0

	if vm_MeltingFactor > 4.7 {
		vm_MeltingFactor = 4.7
	}

	if sc.vm_FrozenWaterInSnow <= 0.0 {
		vm_Snowmelt = 0.0
	} else if vw_MeanAirTemperature < sc.vm_SnowmeltTemperature {
		vm_Snowmelt = 0.0
	} else {
		vm_Snowmelt = vm_MeltingFactor * (vw_MeanAirTemperature - sc.vm_SnowmeltTemperature)
		if vm_Snowmelt > sc.vm_FrozenWaterInSnow {
			vm_Snowmelt = sc.vm_FrozenWaterInSnow
		}
	}

	return vm_Snowmelt
}

// C++: double monica::snowcomponent::calcNetPrecipitation(const SnowComponent*,
//        double, double, double&, double&)
calc_net_precipitation :: proc(
	sc: ^Snow_Component,
	mean_air_temperature, net_precipitation_: f64,
	net_precipitation_water, net_precipitation_snow: ^f64,
) -> f64 {
	liquid_water_precipitation := 0.0

	if mean_air_temperature >= sc.vm_SnowAccumulationThresholdTemperature {
		liquid_water_precipitation = 1.0
	} else if mean_air_temperature <= sc.vm_TemperatureLimitForLiquidWater {
		liquid_water_precipitation = 0.0
	} else {
		liquid_water_precipitation =
			(mean_air_temperature - sc.vm_TemperatureLimitForLiquidWater) /
			(sc.vm_SnowAccumulationThresholdTemperature - sc.vm_TemperatureLimitForLiquidWater)
	}

	net_precipitation_water^ =
		liquid_water_precipitation * sc.vm_CorrectionRain * net_precipitation_
	net_precipitation_snow^ =
		(1.0 - liquid_water_precipitation) * sc.vm_CorrectionSnow * net_precipitation_

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

	if refreeze_helper < sc.vm_RefreezeTemperature {
		if sc.vm_LiquidWaterInSnow > 0.0 {
			refreeze =
				sc.vm_RefreezeP1 *
				libc.pow(sc.vm_RefreezeTemperature - refreeze_helper, sc.vm_RefreezeP2)
		}
		if refreeze > sc.vm_LiquidWaterInSnow {
			refreeze = sc.vm_LiquidWaterInSnow
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
			(mean_air_temperature - sc.vm_TemperatureLimitForLiquidWater) /
			(sc.vm_SnowAccumulationThresholdTemperature - sc.vm_TemperatureLimitForLiquidWater)
		if snow_density_factor > 1.0 {
			snow_density_factor = 1.0
		}
		if snow_density_factor < 0.0 {
			snow_density_factor = 0.0
		}
		new_snow_density = sc.vm_NewSnowDensityMin + sc.vm_SnowMaxAdditionalDensity * snow_density_factor
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
	if (sc.vm_SnowDepth + net_precipitation_snow) <= 0.0 {
		snow_density = 0.0
	} else {
		snow_density =
			(((1.0 + sc.vm_SnowPacking) * sc.vm_SnowDensity * sc.vm_SnowDepth) +
					(new_snow_density * net_precipitation_snow)) /
			(sc.vm_SnowDepth + net_precipitation_snow)
		if snow_density > (sc.vm_NewSnowDensityMin + sc.vm_SnowMaxAdditionalDensity) {
			snow_density = sc.vm_NewSnowDensityMin + sc.vm_SnowMaxAdditionalDensity
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

	if (frozen_water_in_snow <= 0.0) || (sc.vm_SnowDensity <= 0.0) {
		snow_retention_capacity = 0.0
	} else {
		snow_retention_capacity = sc.vm_SnowRetentionCapacityMax / 10.0 / sc.vm_SnowDensity

		if snow_retention_capacity < sc.vm_SnowRetentionCapacityMin {
			snow_retention_capacity = sc.vm_SnowRetentionCapacityMin
		}
		if snow_retention_capacity > sc.vm_SnowRetentionCapacityMax {
			snow_retention_capacity = sc.vm_SnowRetentionCapacityMax
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
		sc.vm_WaterToInfiltrate = snow_layer_water_release
	}
	return water_to_infiltrate
}

// C++: void monica::snowcomponent::calcSnowDepth(SnowComponent*, double)
calc_snow_depth :: proc(sc: ^Snow_Component, snow_water_equivalent: f64) {
	pm_WaterDensity := 1.0 // [kg dm-3]
	if snow_water_equivalent <= 0.0 {
		sc.vm_SnowDepth = 0.0
	} else {
		sc.vm_SnowDepth = snow_water_equivalent * pm_WaterDensity / sc.vm_SnowDensity

		if sc.vm_SnowDepth > sc.vm_maxSnowDepth {
			sc.vm_maxSnowDepth = sc.vm_SnowDepth
		}

		if sc.vm_SnowDepth < 0.01 {
			sc.vm_SnowDepth = 0.0
		}
	}
	if sc.vm_SnowDepth == 0.0 {
		sc.vm_SnowDensity = 0.0
		sc.vm_FrozenWaterInSnow = 0.0
		sc.vm_LiquidWaterInSnow = 0.0
	}

	if sc.soilColumn != nil {
		sc.soilColumn.vm_SnowDepth = sc.vm_SnowDepth
	}
	sc.vm_AccumulatedSnowDepth += sc.vm_SnowDepth
}
