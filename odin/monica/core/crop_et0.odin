package core

// The crop's reference evapotranspiration (plan-component-interfaces.md E3):
// FAO-56 Penman-Monteith with the crop's stomata resistance. A pure function:
// crop_module_step stores its results in Crop_Et0, and the evapotranspiration
// modules (evapotranspiration_impl.odin) call it for the ET0 terms instead of
// running fcReferenceEvapotranspiration on a copy of the whole crop.

// The crop's reference evapotranspiration (crop_et0).
Crop_Et0 :: struct {
	reference_evapotranspiration: f64 `unit:"mm"`,
	stomata_resistance:           f64 `unit:"s m-1"`, //! old RSTOM
	net_radiation:                f64 `unit:"MJ m-2"`, // of the ET0 above, for output (soil moisture takes it on crop days)
}

// What crop_et0 reads.
Crop_Et0_In :: struct {
	max_air_temperature:                f64 `unit:"°C" src:"weather"`,
	min_air_temperature:                f64 `unit:"°C" src:"weather"`,
	relative_humidity:                  f64 `unit:"0-1" src:"weather"`, // <= 0: unknown
	mean_air_temperature:               f64 `unit:"°C" src:"weather"`,
	wind_speed:                         f64 `unit:"m s-1" src:"weather"`,
	wind_speed_height:                  f64 `unit:"m" src:"site"`,
	atmospheric_co2_concentration:      f64 `unit:"ppm" src:"weather"`,
	height_nn:                          f64 `unit:"m" src:"site"`,
	gross_photosynthesis_reference_mol: f64 `unit:"mol m-2 s-1" src:"Photosynthesis"`,
	extraterrestrial_radiation:         f64 `unit:"MJ m-2" src:"Radiation"`,
	global_radiation:                   f64 `unit:"MJ m-2" src:"Radiation"`,
	saturation_beta:                    f64 `unit:"kPa" src:"crop module parameters"`,
	stomata_conductance_alpha:          f64 `unit:"1" src:"crop module parameters"`,
	reference_albedo:                   f64 `unit:"0-1" src:"crop module parameters"`,
}

// Fills Crop_Et0_In from the crop (after the day's photosynthesis) and the weather.
crop_et0_in :: proc(
	cm: ^Crop_Module,
	max_air_temperature, min_air_temperature, relative_humidity, mean_air_temperature: f64,
	wind_speed, wind_speed_height, atmospheric_co2_concentration: f64,
) -> Crop_Et0_In {
	return {
		max_air_temperature = max_air_temperature,
		min_air_temperature = min_air_temperature,
		relative_humidity = relative_humidity,
		mean_air_temperature = mean_air_temperature,
		wind_speed = wind_speed,
		wind_speed_height = wind_speed_height,
		atmospheric_co2_concentration = atmospheric_co2_concentration,
		height_nn = cm.site_params.height_nn,
		gross_photosynthesis_reference_mol = cm.gross_photosynthesis_reference_mol,
		extraterrestrial_radiation = cm.extraterrestrial_radiation,
		global_radiation = cm.global_radiation,
		saturation_beta = cm.mod_params.saturation_beta,
		stomata_conductance_alpha = cm.mod_params.stomata_conductance_alpha,
		reference_albedo = cm.mod_params.reference_albedo,
	}
}

// C++: double monica::cropmodule::fcReferenceEvapotranspiration(CropModule*,
// double vw_MaxAirTemperature, double vw_MinAirTemperature, double
// vw_RelativeHumidity, double vw_MeanAirTemperature, double vw_WindSpeed,
// double vw_WindSpeedHeight, double vw_AtmosphericCO2Concentration)
//
// FAO-56 Penman-Monteith (fao56_et0, et0.odin - shared with soil moisture's
// ET0) with the crop's stomata resistance.
//
// NOTE(deviation): returns the stomata resistance with the other terms
// (Et0_Terms) instead of writing it into the crop.
crop_et0 :: proc(input: Crop_Et0_In) -> (et0: f64, terms: Et0_Terms) {
	et0_in := Et0_In {
		max_air_temperature        = input.max_air_temperature,
		min_air_temperature        = input.min_air_temperature,
		relative_humidity          = input.relative_humidity,
		mean_air_temperature       = input.mean_air_temperature,
		wind_speed                 = input.wind_speed,
		wind_speed_height          = input.wind_speed_height,
		height_nn                  = input.height_nn,
		extraterrestrial_radiation = input.extraterrestrial_radiation,
		global_radiation           = input.global_radiation,
		reference_albedo           = input.reference_albedo,
	}
	vapour_pressure, saturated_vapour_pressure := et0_vapour_pressures(et0_in)
	return fao56_et0(et0_in, crop_stomata_resistance(input, saturated_vapour_pressure - vapour_pressure))
}

// The crop's stomata resistance [s m-1] from its reference gross
// photosynthesis and the saturation deficit [kPa] (Yu et al. 2001:
// saturation_beta = 3.5, stomata_conductance_alpha = 0.06). C++ has the same
// formula for C3 and C4.
crop_stomata_resistance :: proc(input: Crop_Et0_In, saturation_deficit: f64) -> f64 {
	if input.gross_photosynthesis_reference_mol <= 0.0 {
		return 999999.9
	}
	return(
		(input.atmospheric_co2_concentration * (1.0 + saturation_deficit / input.saturation_beta)) /
		(input.stomata_conductance_alpha * input.gross_photosynthesis_reference_mol)
	)
}
