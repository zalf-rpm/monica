package core

// VOC emissions as a component (plan-component-interfaces.md E2): cut out of
// fc_crop_photosynthesis's hourly loop. Nothing in the crop reads the
// emissions - they are only reported - so they are computed after the loop,
// from the day's 24 hours. State: Crop_Voc (embedded in Crop_Module): the
// 24/240-step radiation and leaf temperature buffers, the emissions.

import p "../params"

// VOC emissions: the 24/240-step buffers and the emissions (only reported).
Crop_Voc :: struct {
	step_size24:                 int,
	step_size240:                int,
	rad24:                       [dynamic]f64,
	rad240:                      [dynamic]f64,
	tfol24:                      [dynamic]f64,
	tfol240:                     [dynamic]f64,
	index24:                     int,
	index240:                    int,
	full24:                      bool,
	full240:                     bool,
	guenther_emissions:          Voc_Emissions,
	jjv_emissions:               Voc_Emissions,
	voc_species:                 Voc_Species_Data,
	crop_photosynthesis_results: Voc_Cp_Data,
}

// One hour of photosynthesis, as the VOC emissions need it.
Voc_Hour :: struct {
	global_rad: f64 `unit:"MJ m-2"`, // in the hour
	leaf_temp:  f64 `unit:"°C"`,
	sunlit:     Fvcb_Leaf_Fraction,
	shaded:     Fvcb_Leaf_Fraction,
}

// What voc_step reads on a day.
Voc_In :: struct {
	hours:                         [24]Voc_Hour `src:"Photosynthesis"`,
	lai:                           f64 `unit:"m2 m-2" src:"Canopy"`,
	leaf_green_biomass:            f64 `unit:"kgDM ha-1" src:"Growth" lag:"1d"`,
	// at today's stage, for the leaf mass when there is none
	specific_leaf_area:            f64 `unit:"ha kg-1" src:"cultivar"`,
	atmospheric_co2_concentration: f64 `unit:"ppm" src:"weather"`,
	n_stress:                      f64 `unit:"0-1" src:"N_Status" lag:"1d"`,
	water_stress:                  f64 `unit:"0-1" src:"Water_Uptake" lag:"1d"`,
}

voc_step :: proc(v: ^Crop_Voc, ps: ^p.Crop_Parameters, input: Voc_In, allocator := context.allocator) {
	species_ps := &ps.species_params
	v.guenther_emissions = Voc_Emissions{}
	v.jjv_emissions = Voc_Emissions{}

	for h := 0; h < 24; h += 1 {
		hr := input.hours[h]
		// calculate VOC emissions
		globrad_wm2 := hr.global_rad * 1000000.0 / 3600 // MJ m-2 h-1 -> W m-2
		if v.index240 < v.step_size240 - 1 {
			v.index240 += 1
		} else {
			v.index240 = 0
			v.full240 = true
		}
		v.rad240[v.index240] = globrad_wm2
		v.tfol240[v.index240] = hr.leaf_temp

		if v.index24 < v.step_size24 - 1 {
			v.index24 += 1
		} else {
			v.index24 = 0
			v.full24 = true
		}
		v.rad24[v.index24] = globrad_wm2
		v.tfol24[v.index24] = hr.leaf_temp

		mcd: Voc_Micro_Climate_Data
		mcd.rad = globrad_wm2
		rad24_sum := 0.0
		for v in v.rad24 {
			rad24_sum += v
		}
		mcd.rad24 = rad24_sum / (v.full24 ? f64(len(v.rad24)) : f64(v.index24 + 1))
		rad240_sum := 0.0
		for v in v.rad240 {
			rad240_sum += v
		}
		mcd.rad240 = rad240_sum / (v.full240 ? f64(len(v.rad240)) : f64(v.index240 + 1))
		mcd.t_fol = hr.leaf_temp
		tfol24_sum := 0.0
		for v in v.tfol24 {
			tfol24_sum += v
		}
		mcd.t_fol24 = tfol24_sum / (v.full24 ? f64(len(v.tfol24)) : f64(v.index24 + 1))
		tfol240_sum := 0.0
		for v in v.tfol240 {
			tfol240_sum += v
		}
		mcd.t_fol240 = tfol240_sum / (v.full240 ? f64(len(v.tfol240)) : f64(v.index240 + 1))
		mcd.co2concentration = input.atmospheric_co2_concentration

		species: Voc_Species_Data
		species.lai = input.lai
		species.m_fol = input.leaf_green_biomass / (100.0 * 100.0) // kg/ha -> kg/m2
		species.sla =
			species.m_fol > 0 ? species.lai / species.m_fol : input.specific_leaf_area * 100.0 * 100.0 // ha/kg -> m2/kg

		species.ef_mono = species_ps.ef_mono
		species.ef_monos = species_ps.ef_monos
		species.ef_iso = species_ps.ef_iso
		species.vcmax25 = species_ps.vcmax25
		species.aekc = species_ps.aekc
		species.aeko = species_ps.aeko
		species.aevc = species_ps.aevc
		species.kc25 = species_ps.kc25

		ges := voc_guenther_emissions(species, &mcd, 1.0 / 24.0, allocator)
		voc_emissions_add(&v.guenther_emissions, &ges, allocator)

		sun_lai := hr.sunlit.lai
		sh_lai := hr.shaded.lai
		leaf_fractions := []Fvcb_Leaf_Fraction{hr.sunlit, hr.shaded}
		for lf in leaf_fractions {
			species.lai = lf.lai
			species.m_fol =
				input.leaf_green_biomass /
				(100.0 * 100.0) *
				lf.lai /
				(sun_lai + sh_lai) // kg/ha -> kg/m2
			species.sla =
				species.m_fol > 0 ? species.lai / species.m_fol : input.specific_leaf_area * 100.0 * 100.0 // ha/kg -> m2/kg

			mcd.rad = lf.rad // W m-2 global incident

			v.crop_photosynthesis_results.kc = lf.kc
			v.crop_photosynthesis_results.ko = lf.ko * 1000
			v.crop_photosynthesis_results.oi = lf.oi * 1000
			v.crop_photosynthesis_results.ci = lf.ci
			v.crop_photosynthesis_results.vc_max =
				fvcb_vcmax_bernacchi_f(mcd.t_fol, species_ps.vcmax25) *
				input.n_stress *
				input.water_stress
			v.crop_photosynthesis_results.j_max =
				fvcb_jmax_bernacchi_f(mcd.t_fol, 120) *
				input.n_stress *
				input.water_stress
			v.crop_photosynthesis_results.jj = lf.jj
			v.crop_photosynthesis_results.jj1000 = lf.jj1000
			v.crop_photosynthesis_results.jv = lf.jv

			jjves := voc_jjv_emissions(
				species,
				&mcd,
				v.crop_photosynthesis_results,
				1.0 / 24.0,
				false,
				allocator,
			)
			voc_emissions_add(&v.jjv_emissions, &jjves, allocator)
		}
	}
}
