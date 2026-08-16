// Port of the pwp/fc/sat interpolation half of src/soil/soil.cpp:
// VanGenuchtenParams, the four fcSatPwpFrom* functions (KA5textureClass,
// VanGenuchtenVereecken, VanGenuchtenToth, Toth), the two calcVanGenuchten*Params
// functions, and the four updateUnsetPwpFcSatFrom* wrappers + the
// apply_pwp_fc_sat_method dispatch that replaces the C++'s
// calculateAndSetPwpFcSatFunctions map (see soil_parameters.odin).
//
// fcSatPwpFromKA5textureClass and the three fcSatPwpFromVanGenuchten*/Toth
// siblings, plus updateUnsetPwpFcSatFromKA5textureClass, are C++ anonymous-
// namespace (file-local) functions - not reachable from outside soil.cpp, so
// there is nothing to keep name-parity with beyond the comments below; they're
// exported here since Odin has no equivalent restriction and later phases need
// them.
package soil

import libc "core:c/libc"
import "core:strings"
import tl "../../support/tools"

// C++: struct Soil::VanGenuchtenParams
Van_Genuchten_Params :: struct {
	thetaR:                              f64,
	thetaS:                              f64,
	alpha:                               f64,
	m:                                   f64,
	n:                                   f64,
	volumetricWaterContentAtMatricHead:  f64,
}

// C++: struct FcSatPwp (anonymous namespace)
Fc_Sat_Pwp :: struct {
	fc:  f64,
	sat: f64,
	pwp: f64,
}

// C++: EResult<FcSatPwp> fcSatPwpFromKA5textureClass(pathToSoilDir, texture,
//        stoneContent, soilRawDensity, soilOrganicMatter)
fc_sat_pwp_from_ka5_texture_class :: proc(
	path_to_soil_dir: string,
	texture_in: string,
	stone_content: f64,
	soil_raw_density: f64,
	soil_organic_matter: f64,
	allocator := context.allocator,
) -> tl.EResult(Fc_Sat_Pwp) {
	texture := strings.to_upper(texture_in, allocator)

	res: tl.EResult(Fc_Sat_Pwp)
	res.allocator = allocator

	if len(texture) == 0 {
		tl.append_error(&res, "No soil texture given.")
		return res
	}

	srd := soil_raw_density / 1000.0 // [kg m-3] -> [g cm-3]
	som := soil_organic_matter * 100.0 // [kg kg-1] -> [%]

	// ***************************************************************************
	// *** The following boundaries are extracted from:                       ***
	// *** Wessolek, G., M. Kaupenjohann, M. Renger (2009) Bodenphysikalische ***
	// *** Kennwerte und Berechnungsverfahren fuer die Praxis. Bodenoekologie ***
	// *** und Bodengenese 40, Selbstverlag Technische Universitaet Berlin    ***
	// *** (Tab. 4).                                                          ***
	// ***************************************************************************
	srd_lower_bound := 0.0
	srd_upper_bound := 0.0
	if srd < 1.1 {
		srd_lower_bound = 1.1
		srd_upper_bound = 1.1
	} else if srd >= 1.1 && srd < 1.3 {
		srd_lower_bound = 1.1
		srd_upper_bound = 1.3
	} else if srd >= 1.3 && srd < 1.5 {
		srd_lower_bound = 1.3
		srd_upper_bound = 1.5
	} else if srd >= 1.5 && srd < 1.7 {
		srd_lower_bound = 1.5
		srd_upper_bound = 1.7
	} else if srd >= 1.7 && srd < 1.9 {
		srd_lower_bound = 1.7
		srd_upper_bound = 1.9
	} else if srd >= 1.9 {
		srd_lower_bound = 1.9
		srd_upper_bound = 1.9
	}

	// special treatment for "torf" soils
	if texture == "HH" || texture == "HN" {
		srd_lower_bound = -1
		srd_upper_bound = -1
	}

	// Boundaries for linear interpolation
	lb_res := read_principal_soil_characteristic_data(
		path_to_soil_dir,
		texture,
		srd_lower_bound,
		allocator,
	)
	if tl.failure(lb_res.errs) {
		tl.append_errors(&res, lb_res.errs)
		return res
	}
	sat_lower_bound := lb_res.result.sat
	fc_lower_bound := lb_res.result.fc
	pwp_lower_bound := lb_res.result.pwp

	ub_res := read_principal_soil_characteristic_data(
		path_to_soil_dir,
		texture,
		srd_upper_bound,
		allocator,
	)
	if tl.failure(ub_res.errs) {
		tl.append_errors(&res, ub_res.errs)
		return res
	}
	sat_upper_bound := ub_res.result.sat
	fc_upper_bound := ub_res.result.fc
	pwp_upper_bound := ub_res.result.pwp

	// ***************************************************************************
	// *** The following boundaries are extracted from:                       ***
	// *** Wessolek, G., M. Kaupenjohann, M. Renger (2009) Bodenphysikalische ***
	// *** Kennwerte und Berechnungsverfahren fuer die Praxis. Bodenoekologie ***
	// *** und Bodengenese 40, Selbstverlag Technische Universitaet Berlin    ***
	// *** (Tab. 5).                                                          ***
	// ***************************************************************************
	som_lower_bound := 0.0
	som_upper_bound := 0.0
	if som >= 0.0 && som < 1.0 {
		som_lower_bound = 0.0
		som_upper_bound = 0.0
	} else if som >= 1.0 && som < 1.5 {
		som_lower_bound = 0.0
		som_upper_bound = 1.5
	} else if som >= 1.5 && som < 3.0 {
		som_lower_bound = 1.5
		som_upper_bound = 3.0
	} else if som >= 3.0 && som < 6.0 {
		som_lower_bound = 3.0
		som_upper_bound = 6.0
	} else if som >= 6.0 && som < 11.5 {
		som_lower_bound = 6.0
		som_upper_bound = 11.5
	} else if som >= 11.5 {
		som_lower_bound = 11.5
		som_upper_bound = 11.5
	}

	// special treatment for "torf" soils
	if texture == "HH" || texture == "HN" {
		som_lower_bound = 0.0
		som_upper_bound = 0.0
	}

	// Boundaries for linear interpolation
	fc_mod_lower_bound := 0.0
	sat_mod_lower_bound := 0.0
	pwp_mod_lower_bound := 0.0
	// modifier values are given only for organic matter > 1.0% (class h2)
	if som_lower_bound != 0.0 {
		lb_res2 := read_soil_characteristic_modifier(
			path_to_soil_dir,
			texture,
			som_lower_bound,
			allocator,
		)
		if tl.failure(lb_res2.errs) {
			tl.append_errors(&res, lb_res2.errs)
			return res
		}
		sat_mod_lower_bound = lb_res2.result.sat
		fc_mod_lower_bound = lb_res2.result.fc
		pwp_mod_lower_bound = lb_res2.result.pwp
	}

	fc_mod_upper_bound := 0.0
	sat_mod_upper_bound := 0.0
	pwp_mod_upper_bound := 0.0
	if som_upper_bound != 0.0 {
		ub_res2 := read_soil_characteristic_modifier(
			path_to_soil_dir,
			texture,
			som_upper_bound,
			allocator,
		)
		if tl.failure(ub_res2.errs) {
			tl.append_errors(&res, ub_res2.errs)
			return res
		}
		sat_mod_upper_bound = ub_res2.result.sat
		fc_mod_upper_bound = ub_res2.result.fc
		pwp_mod_upper_bound = ub_res2.result.pwp
	}

	// Linear interpolation
	fc_unmod := fc_lower_bound
	if fc_upper_bound < 0.5 && fc_lower_bound >= 1.0 {
		fc_unmod = fc_lower_bound
	} else if fc_lower_bound < 0.5 && fc_upper_bound >= 1.0 {
		fc_unmod = fc_upper_bound
	} else if srd_upper_bound != srd_lower_bound {
		fc_unmod =
			(srd - srd_lower_bound) / (srd_upper_bound - srd_lower_bound) *
				(fc_upper_bound - fc_lower_bound) +
			fc_lower_bound
	}

	sat_unmod := sat_lower_bound
	if sat_upper_bound < 0.5 && sat_lower_bound >= 1.0 {
		sat_unmod = sat_lower_bound
	} else if sat_lower_bound < 0.5 && sat_upper_bound >= 1.0 {
		sat_unmod = sat_upper_bound
	} else if srd_upper_bound != srd_lower_bound {
		sat_unmod =
			(srd - srd_lower_bound) / (srd_upper_bound - srd_lower_bound) *
				(sat_upper_bound - sat_lower_bound) +
			sat_lower_bound
	}

	pwp_unmod := pwp_lower_bound
	if pwp_upper_bound < 0.5 && pwp_lower_bound >= 1.0 {
		pwp_unmod = pwp_lower_bound
	} else if pwp_lower_bound < 0.5 && pwp_upper_bound >= 1.0 {
		pwp_unmod = pwp_upper_bound
	} else if srd_upper_bound != srd_lower_bound {
		pwp_unmod =
			(srd - srd_lower_bound) / (srd_upper_bound - srd_lower_bound) *
				(pwp_upper_bound - pwp_lower_bound) +
			pwp_lower_bound
	}

	// in this case upper and lower boundary are equal, so doesn't matter.
	fc_mod := fc_mod_lower_bound
	sat_mod := sat_mod_lower_bound
	pwp_mod := pwp_mod_lower_bound
	if som_upper_bound != som_lower_bound {
		fc_mod =
			(som - som_lower_bound) / (som_upper_bound - som_lower_bound) *
				(fc_mod_upper_bound - fc_mod_lower_bound) +
			fc_mod_lower_bound

		sat_mod =
			(som - som_lower_bound) / (som_upper_bound - som_lower_bound) *
				(sat_mod_upper_bound - sat_mod_lower_bound) +
			sat_mod_lower_bound

		pwp_mod =
			(som - som_lower_bound) / (som_upper_bound - som_lower_bound) *
				(pwp_mod_upper_bound - pwp_mod_lower_bound) +
			pwp_mod_lower_bound
	}

	out: Fc_Sat_Pwp
	// Modifying the principal values by organic matter
	out.fc = (fc_unmod + fc_mod) / 100.0 // [m3 m-3]
	out.sat = (sat_unmod + sat_mod) / 100.0 // [m3 m-3]
	out.pwp = (pwp_unmod + pwp_mod) / 100.0 // [m3 m-3]

	// Modifying the principal values by stone content
	out.fc *= (1.0 - stone_content)
	out.sat *= (1.0 - stone_content)
	out.pwp *= (1.0 - stone_content)

	res.result = out
	return res
}

// C++: FcSatPwp fcSatPwpFromVanGenuchtenVereecken(sandFrac, clayFrac, stoneFrac,
//        bulkDensityKgPerM3, organicCarbonFrac)
fc_sat_pwp_from_van_genuchten_vereecken :: proc(
	sand_frac, clay_frac, stone_frac, bulk_density_kg_per_m3, organic_carbon_frac: f64,
) -> Fc_Sat_Pwp {
	res: Fc_Sat_Pwp
	res.pwp = (0.015 + 0.5 * clay_frac + 1.4 * organic_carbon_frac) * (1.0 - stone_frac)
	res.sat =
		(0.81 - 0.283 * (bulk_density_kg_per_m3 / 1000.0) + 0.1 * clay_frac) * (1.0 - stone_frac)

	// Van Genuchten retention curve to calculate volumetric water content at
	// moisture equivalent (Field capacity definition KA5)
	fc_pF := 2.1
	if sand_frac > 0.48 && sand_frac <= 0.9 && clay_frac <= 0.12 {
		fc_pF = 2.1 - (0.476 * (sand_frac - 0.48))
	} else if sand_frac > 0.9 && clay_frac <= 0.05 {
		fc_pF = 1.9
	} else if clay_frac > 0.45 {
		fc_pF = 2.5
	} else if clay_frac > 0.30 && sand_frac < 0.2 {
		fc_pF = 2.4
	} else if clay_frac > 0.35 {
		fc_pF = 2.3
	} else if clay_frac > 0.25 && sand_frac < 0.1 {
		fc_pF = 2.3
	} else if clay_frac > 0.17 && sand_frac > 0.68 {
		fc_pF = 2.2
	} else if clay_frac > 0.17 && sand_frac < 0.33 {
		fc_pF = 2.2
	} else if clay_frac > 0.08 && sand_frac < 0.27 {
		fc_pF = 2.2
	} else if clay_frac > 0.25 && sand_frac < 0.25 {
		fc_pF = 2.2
	}

	matric_head := libc.pow(10.0, fc_pF)

	vgps := calc_van_genuchten_vereecken_params(
		res.pwp,
		res.sat,
		sand_frac,
		clay_frac,
		bulk_density_kg_per_m3,
		organic_carbon_frac,
		stone_frac,
		matric_head,
	)
	res.fc = vgps.volumetricWaterContentAtMatricHead
	return res
}

// C++: FcSatPwp fcSatPwpFromVanGenuchtenToth(isTopSoil, sandFrac, clayFrac,
//        stoneFrac, bulkDensityKgPerM3, organicCarbonFrac)
fc_sat_pwp_from_van_genuchten_toth :: proc(
	is_top_soil: bool,
	sand_frac, clay_frac, stone_frac, bulk_density_kg_per_m3, organic_carbon_frac: f64,
) -> Fc_Sat_Pwp {
	res: Fc_Sat_Pwp

	// hFC = -100 for coarse soils, hFC = -330 for medium/fine soils
	fc_matric_head := sand_frac > 0.60 ? -100.0 : -330.0
	fc_vgps := calc_van_genuchten_toth_params(
		is_top_soil,
		sand_frac,
		clay_frac,
		bulk_density_kg_per_m3,
		organic_carbon_frac,
		stone_frac,
		fc_matric_head,
	)
	res.sat = fc_vgps.thetaS
	res.fc = fc_vgps.volumetricWaterContentAtMatricHead

	pwp_matric_head := -15000.0
	pwp_vgps := calc_van_genuchten_toth_params(
		is_top_soil,
		sand_frac,
		clay_frac,
		bulk_density_kg_per_m3,
		organic_carbon_frac,
		stone_frac,
		pwp_matric_head,
	)
	res.pwp = pwp_vgps.volumetricWaterContentAtMatricHead

	// safeguard (thetaR is the same for both calls - only depends on sand_frac)
	if res.pwp <= pwp_vgps.thetaR {
		res.pwp = fc_vgps.thetaR + 0.01
	}

	return res
}

// C++: FcSatPwp fcSatPwpFromToth(sandContent, clayContent, stoneContent,
//        soilBulkDensity, soilOrganicCarbon)
fc_sat_pwp_from_toth :: proc(
	sand_content, clay_content, stone_content, soil_bulk_density, soil_organic_carbon: f64,
) -> Fc_Sat_Pwp {
	res: Fc_Sat_Pwp
	// sat function from MONICA, maybe not necessary
	res.sat =
		(0.81 - 0.283 * (soil_bulk_density / 1000.0) + 0.1 * clay_content) * (1.0 - stone_content)

	// transform from [0 to 1] to [0 to 100]
	sluf := 100.0 - clay_content * 100.0 - sand_content * 100.0
	ton := clay_content * 100.0
	// The SOC was 0.001 from the input, that's why I added this line
	oc := soil_organic_carbon * 100.0

	res.fc =
		0.24490 - 0.1887 * (1.0 / (oc + 1.0)) + 0.0045270 * ton + 0.001535 * sluf +
		0.001442 * sluf * (1.0 / (oc + 1.0)) -
		0.0000511 * sluf * ton +
		0.0008676 * ton * (1.0 / (oc + 1.0))

	res.pwp =
		0.09878 + 0.002127 * ton - 0.0008366 * sluf - 0.0767 * (1.0 / (oc + 1.0)) +
		0.00003853 * sluf * ton +
		0.00233 * ton * (1.0 / (oc + 1.0)) +
		0.0009498 * sluf * (1.0 / (oc + 1.0))

	return res
}

// C++: (anonymous namespace) Errors updateUnsetPwpFcSatFromKA5textureClass(
//        pathToSoilDir, sp)
update_unset_pwp_fc_sat_from_ka5_texture_class :: proc(
	path_to_soil_dir: string,
	sp: ^Soil_Parameters,
	allocator := context.allocator,
) -> tl.Errors {
	if len(sp.vs_SoilTexture) == 0 {
		return tl.make_errors_from_error("No soil texture defined!", allocator)
	}

	// we only need to update something, if any of the values is not already set
	if sp.vs_FieldCapacity < 0 || sp.vs_Saturation < 0 || sp.vs_PermanentWiltingPoint < 0 {
		res := fc_sat_pwp_from_ka5_texture_class(
			path_to_soil_dir,
			sp.vs_SoilTexture,
			sp.vs_SoilStoneContent,
			soil_raw_density(sp),
			soil_organic_matter(sp),
			allocator,
		)
		if tl.failure(res.errs) {
			return res.errs
		}
		if sp.vs_FieldCapacity < 0 {
			sp.vs_FieldCapacity = res.result.fc
		}
		if sp.vs_Saturation < 0 {
			sp.vs_Saturation = res.result.sat
		}
		if sp.vs_PermanentWiltingPoint < 0 {
			sp.vs_PermanentWiltingPoint = res.result.pwp
		}
	}
	return tl.Errors{}
}

// C++: VanGenuchtenParams Soil::calcVanGenuchtenVereeckenParams(pwp, sat,
//        sandFrac, clayFrac, bulkDensityKgPerM3, organicCarbonFrac, stoneFrac,
//        matricHead)
calc_van_genuchten_vereecken_params :: proc(
	pwp, sat, sand_frac, clay_frac, bulk_density_kg_per_m3, organic_carbon_frac: f64,
	stone_frac: f64 = -1,
	matric_head: f64 = -1,
) -> Van_Genuchten_Params {
	res: Van_Genuchten_Params
	res.volumetricWaterContentAtMatricHead = -1 // C++ in-class initialiser default
	res.thetaR = pwp
	res.thetaS = sat
	res.alpha = libc.exp(
		-2.486 + 2.5 * sand_frac - 35.1 * organic_carbon_frac -
		2.617 * (bulk_density_kg_per_m3 / 1000.0) -
		2.3 * clay_frac,
	)

	res.m = 1.0
	res.n = libc.exp(0.053 - 0.9 * sand_frac - 1.3 * clay_frac + 1.5 * libc.pow(sand_frac, 2.0))

	if stone_frac >= 0 {
		res.volumetricWaterContentAtMatricHead =
			(res.thetaR +
					((res.thetaS - res.thetaR) /
							(libc.pow(1.0 + libc.pow(res.alpha * matric_head, res.n), res.m)))) *
			(1.0 - stone_frac)
	}
	return res
}

// C++: VanGenuchtenParams Soil::calcVanGenuchtenTothParams(isTopSoil, sandFrac,
//        clayFrac, bulkDensityKgPerM3, organicCarbonFrac, stoneFrac, matricHead)
calc_van_genuchten_toth_params :: proc(
	is_top_soil: bool,
	sand_frac, clay_frac, bulk_density_kg_per_m3, organic_carbon_frac: f64,
	stone_frac: f64 = -1,
	matric_head: f64 = -1,
) -> Van_Genuchten_Params {
	res: Van_Genuchten_Params
	sand_content_perc := sand_frac * 100
	clay_content_perc := clay_frac * 100
	silt_content_perc := 100 - sand_content_perc - clay_content_perc
	soil_organic_carbon_perc := organic_carbon_frac * 100
	soil_bulk_density_g_per_cm3 := bulk_density_kg_per_m3 / 1000

	if sand_content_perc >= 20 {
		res.thetaR = 0.041
	} else if sand_content_perc < 20 {
		res.thetaR = 0.179
	}
	res.thetaS =
		0.8308 - 0.28217 * soil_bulk_density_g_per_cm3 + 0.0002728 * clay_content_perc +
		0.000187 * silt_content_perc

	is_top_soil_f: f64 = is_top_soil ? 1 : 0
	res.alpha = libc.pow(
		10.0,
		-0.43348 - 0.41729 * soil_bulk_density_g_per_cm3 -
		0.04762 * soil_organic_carbon_perc -
		0.2181 * is_top_soil_f -
		0.01581 * clay_content_perc -
		0.01207 * silt_content_perc,
	)
	res.n =
		libc.pow(
			10.0,
			0.22236 - 0.30189 * soil_bulk_density_g_per_cm3 - 0.05558 * is_top_soil_f -
			0.005306 * clay_content_perc -
			0.003084 * silt_content_perc -
			0.01072 * soil_organic_carbon_perc,
		) +
		1
	res.m = 1 - 1 / res.n

	// Van Genuchten Toth retention curve to calculate volumetric water content at
	// moisture equivalent, Field capacity and wilting point
	res.volumetricWaterContentAtMatricHead =
		(res.thetaR +
				((res.thetaS - res.thetaR) /
						(libc.pow(
									1.0 + libc.pow(res.alpha * abs(matric_head), res.n),
									res.m,
								)))) *
		(1.0 - stone_frac)
	return res
}

// C++: std::function<Errors(SoilParameters*,int)>
//        Soil::getInitializedUpdateUnsetPwpFcSatfromKA5textureClassFunction(pathToSoilDir)
//
// The C++ wraps update_unset_pwp_fc_sat_from_ka5_texture_class in a closure
// that captures pathToSoilDir and ignores the int (layer number) parameter.
// apply_pwp_fc_sat_method below plays that role instead - see soil_parameters.odin.

// C++: Errors Soil::updateUnsetPwpFcSatFromVanGenuchtenVereecken(SoilParameters*, int)
//
// The int (layer number) parameter is unused in the C++ too.
update_unset_pwp_fc_sat_from_van_genuchten_vereecken :: proc(sp: ^Soil_Parameters) -> tl.Errors {
	if sp.vs_FieldCapacity < 0 || sp.vs_Saturation < 0 || sp.vs_PermanentWiltingPoint < 0 {
		res := fc_sat_pwp_from_van_genuchten_vereecken(
			sp.vs_SoilSandContent,
			sp.vs_SoilClayContent,
			sp.vs_SoilStoneContent,
			soil_bulk_density(sp),
			soil_organic_carbon(sp),
		)
		if sp.vs_FieldCapacity < 0 {
			sp.vs_FieldCapacity = res.fc
		}
		if sp.vs_Saturation < 0 {
			sp.vs_Saturation = res.sat
		}
		if sp.vs_PermanentWiltingPoint < 0 {
			sp.vs_PermanentWiltingPoint = res.pwp
		}
	}
	return tl.Errors{}
}

// C++: Errors Soil::updateUnsetPwpFcSatFromVanGenuchtenToth(SoilParameters*, int layerNo = 0)
update_unset_pwp_fc_sat_from_van_genuchten_toth :: proc(
	sp: ^Soil_Parameters,
	layer_no: int = 0,
) -> tl.Errors {
	if sp.vs_FieldCapacity < 0 || sp.vs_Saturation < 0 || sp.vs_PermanentWiltingPoint < 0 {
		res := fc_sat_pwp_from_van_genuchten_toth(
			layer_no <= 3,
			sp.vs_SoilSandContent,
			sp.vs_SoilClayContent,
			sp.vs_SoilStoneContent,
			soil_bulk_density(sp),
			soil_organic_carbon(sp),
		)
		if sp.vs_FieldCapacity < 0 {
			sp.vs_FieldCapacity = res.fc
		}
		if sp.vs_Saturation < 0 {
			sp.vs_Saturation = res.sat
		}
		if sp.vs_PermanentWiltingPoint < 0 {
			sp.vs_PermanentWiltingPoint = res.pwp
		}
	}
	return tl.Errors{}
}

// C++: Errors Soil::updateUnsetPwpFcSatFromToth(SoilParameters*, int)
//
// The int (layer number) parameter is unused in the C++ too.
update_unset_pwp_fc_sat_from_toth :: proc(sp: ^Soil_Parameters) -> tl.Errors {
	if sp.vs_FieldCapacity < 0 || sp.vs_Saturation < 0 || sp.vs_PermanentWiltingPoint < 0 {
		res := fc_sat_pwp_from_toth(
			sp.vs_SoilSandContent,
			sp.vs_SoilClayContent,
			sp.vs_SoilStoneContent,
			soil_bulk_density(sp),
			soil_organic_carbon(sp),
		)
		if sp.vs_FieldCapacity < 0 {
			sp.vs_FieldCapacity = res.fc
		}
		if sp.vs_Saturation < 0 {
			sp.vs_Saturation = res.sat
		}
		if sp.vs_PermanentWiltingPoint < 0 {
			sp.vs_PermanentWiltingPoint = res.pwp
		}
	}
	return tl.Errors{}
}

// The enum+switch replacement for calculateAndSetPwpFcSatFunctions - see
// Pwp_Fc_Sat_Method's doc comment in soil_parameters.odin.
apply_pwp_fc_sat_method :: proc(
	method: Pwp_Fc_Sat_Method,
	sp: ^Soil_Parameters,
	layer_no: int,
	path_to_soil_dir: string,
	allocator := context.allocator,
) -> tl.Errors {
	switch method {
	case .NONE:
		return no_set_pwp_fc_sat(sp)
	case .WESSOLEK2009:
		return update_unset_pwp_fc_sat_from_ka5_texture_class(path_to_soil_dir, sp, allocator)
	case .VAN_GENUCHTEN_VEREECKEN:
		return update_unset_pwp_fc_sat_from_van_genuchten_vereecken(sp)
	case .VAN_GENUCHTEN_TOTH:
		return update_unset_pwp_fc_sat_from_van_genuchten_toth(sp, layer_no)
	case .TOTH:
		return update_unset_pwp_fc_sat_from_toth(sp)
	}
	return tl.Errors{}
}
