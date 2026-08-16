// Port of src/core/voc-guenther.{h,cpp}: the Guenther et al. biogenic VOC
// (isoprene/monoterpene) emission model.
//
// Fully self-contained - confirmed by reading the header: takes SpeciesData/
// MicroClimateData (plain data, voc_common.odin), nothing here touches
// CropModule or MonicaModel.
package core

import libc "core:c/libc"
import tl "../../support/tools"

// C++: LeafEmissions calcLeafEmission(const leaf_emission_t&, double)
//
// C++ locals `sqr(x)`/`bound_max(val,max)` (mas_cpp_misc/tools/helper.h)
// inlined directly (x*x / val>max?max:val) - both are one-line, used only
// here in the whole port, not worth a shared helper.
@(private)
voc_guenther_calc_leaf_emission :: proc(lemi: ^Voc_Leaf_Emission_T, _species_EF_MONOS: f64) -> Voc_Leaf_Emissions {
	lems: Voc_Leaf_Emissions
	{
		// isoprene, Guenther et al. 1999 (from Harley et al. 2004)
		x30 := (1.0/VOC_TOPT - 1.0/(30.0+VOC_D_IN_K)) / VOC_RGAS
		cti30 := VOC_CT2 * libc.exp(VOC_CT1*x30) / (VOC_CT2 - VOC_CT1*(1.0-libc.exp(VOC_CT2*x30)))

		eopt := tl.flt_equal_zero(cti30) ? lemi.enz_act.ef_iso : (lemi.enz_act.ef_iso / cti30)
		x := (1.0/VOC_TOPT - 1.0/lemi.fol.tempK) / VOC_RGAS

		// emission scaling factor of isoprenes to temperature
		cti := VOC_CT2 * libc.exp(VOC_CT1*x) / (VOC_CT2 - VOC_CT1*(1.0-libc.exp(VOC_CT2*x)))
		// emission scaling factor to light
		cl :=
			VOC_ALPHA * VOC_CL1 * lemi.pho.par /
			libc.sqrt(1.0 + (VOC_ALPHA*VOC_ALPHA)*(lemi.pho.par*lemi.pho.par))

		cl_bounded := cl > 1.0 ? 1.0 : cl
		lems.isoprene = eopt * cl_bounded * cti
	}

	{
		// monoterpene, Guenther et al. (1993, 1995 (ctm), 1997 (factor 0.961,
		// cit. in Lindfors et al. 2000))
		ctm := libc.exp(VOC_BETA * (lemi.fol.tempK - VOC_TREF))
		cti :=
			libc.exp(VOC_CT1*(lemi.fol.tempK-VOC_TREF)/(VOC_RGAS*VOC_TREF*lemi.fol.tempK)) /
			(0.961 + libc.exp(VOC_CT2*(lemi.fol.tempK-VOC_TOPT)/(VOC_RGAS*VOC_TREF*lemi.fol.tempK)))
		cl :=
			VOC_ALPHA * VOC_CL1 * lemi.pho.par /
			libc.sqrt(1.0 + (VOC_ALPHA*VOC_ALPHA)*(lemi.pho.par*lemi.pho.par))

		cl_bounded := cl > 1.0 ? 1.0 : cl
		lems.monoterp = _species_EF_MONOS*ctm + lemi.enz_act.ef_mono*cl_bounded*cti
	}

	return lems
}

// C++: Voc::Emissions Voc::calculateGuentherVOCEmissionsMultipleSpecies(
//        std::vector<SpeciesData>, const MicroClimateData&, double)
voc_guenther_emissions_multiple_species :: proc(
	sds: []Voc_Species_Data,
	mcd: ^Voc_Micro_Climate_Data,
	dayFraction: f64 = 1.0,
	allocator := context.allocator,
) -> Voc_Emissions {
	ems: Voc_Emissions
	ems.speciesId_2_isoprene_emission = make(map[int]f64, allocator)
	ems.speciesId_2_monoterpene_emission = make(map[int]f64, allocator)

	tslength := f64(VOC_SEC_IN_DAY) * dayFraction

	for species in sds {
		if species.mFol > 0.0 {
			lemi: Voc_Leaf_Emission_T

			// conversion of enzyme activity (umol m-2 s-1) in emission factor
			// (ugC g-1 h-1); specific leaf weight (g m-2)
			lsw := VOC_G_IN_KG / species.sla
			lemi.enz_act.ef_iso = species.EF_ISO
			lemi.enz_act.ef_mono = species.EF_MONO

			// conversion of microclimate variables
			lemi.pho.par = mcd.rad * VOC_FPAR * VOC_W_IN_UMOL // par [umol m-2 s-1 pa-radiation] = rad_fl [W m-2 global radiation] * 0.45 * 4.57
			lemi.fol.tempK = mcd.tFol + VOC_D_IN_K

			// emission in dependence on light and temperature, weighted over
			// canopy layers
			lems := voc_guenther_calc_leaf_emission(&lemi, species.EF_MONOS)

			// conversion from (ugC g-1 h-1) to (umol m-2 s-1) and weighting with
			// leaf area and time
			C1 := (lsw / (f64(VOC_SEC_IN_HR) * VOC_MC)) * species.lai * tslength
			ts_isoprene_em := (1.0 / VOC_C_ISO) * C1 * lems.isoprene
			ts_monoterpene_em := (1.0 / VOC_C_MONO) * C1 * lems.monoterp

			ems.speciesId_2_isoprene_emission[species.id] = ts_isoprene_em
			ems.isoprene_emission += ts_isoprene_em
			ems.speciesId_2_monoterpene_emission[species.id] = ts_monoterpene_em
			ems.monoterpene_emission += ts_monoterpene_em
		} else {
			ems.speciesId_2_isoprene_emission[species.id] = 0.0
			ems.speciesId_2_monoterpene_emission[species.id] = 0.0
		}
	}

	return ems
}

// C++: inline Emissions Voc::calculateGuentherVOCEmissions(const
//        SpeciesData&, const MicroClimateData&, double)
voc_guenther_emissions :: proc(
	species: Voc_Species_Data,
	mcd: ^Voc_Micro_Climate_Data,
	dayFraction: f64 = 1.0,
	allocator := context.allocator,
) -> Voc_Emissions {
	sds := []Voc_Species_Data{species}
	return voc_guenther_emissions_multiple_species(sds, mcd, dayFraction, allocator)
}
