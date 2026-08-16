// Odin side of the phase 5 checkpoint 1 (satellite modules) differential
// test. Must emit byte-identical output to
// odin/tests/cpp_ref/phase5_satellite_ref_main.cpp.
// Run odin/tests/cpp_ref/run_phase5_satellite.sh to build both and diff them.
package phase5_satellite_ref

import "core:fmt"
import "core:math"
import "core:strconv"
import "core:strings"
import core "../../monica/core"

// NaN normalized to the literal "NAN" - see the C++ driver's fmtd for why.
g :: proc(v: f64) -> string {
	if math.is_nan(v) {
		return "NAN"
	}
	buf: [64]byte
	s := strconv.write_float(buf[:], v, 'g', 17, 64)
	if len(s) > 0 && s[0] == '+' {
		s = s[1:]
	}
	return strings.clone(s)
}

row :: proc(fields: ..string) {
	fmt.println(strings.join(fields, "\t"))
}

main :: proc() {
	// --- FvCB: full grid over the 7 scalar inputs ---
	{
		global_rads := []f64{0.0, 2.0, 8.0}
		extra_terr_rads := []f64{5.0, 20.0}
		solar_els := []f64{-0.2, 0.0, 0.5, 1.3}
		LAIs := []f64{0.0, 1.0, 4.0}
		leaf_temps := []f64{-5.0, 15.0, 30.0}
		VPDs := []f64{0.2, 2.0}
		Cas := []f64{300.0, 600.0}

		par := core.make_fvcb_canopy_hourly_params()
		par.Vcmax_25 = 60.0

		for global_rad in global_rads {
			for extra_terr_rad in extra_terr_rads {
				for solar_el in solar_els {
					for LAI in LAIs {
						for leaf_temp in leaf_temps {
							for VPD in VPDs {
								for Ca in Cas {
									in_: core.Fvcb_Canopy_Hourly_In
									in_.global_rad = global_rad
									in_.extra_terr_rad = extra_terr_rad
									in_.solar_el = solar_el
									in_.LAI = LAI
									in_.leaf_temp = leaf_temp
									in_.VPD = VPD
									in_.Ca = Ca

									out := core.fvcb_canopy_hourly_c3(in_, par)
									row(
										"FVCB",
										g(global_rad), g(extra_terr_rad), g(solar_el), g(LAI), g(leaf_temp), g(VPD), g(Ca),
										g(out.canopy_net_photos), g(out.canopy_resp), g(out.canopy_gross_photos), g(out.jmax_c),
										g(out.sunlit.LAI), g(out.sunlit.gs), g(out.sunlit.kc), g(out.sunlit.ko), g(out.sunlit.oi),
										g(out.sunlit.ci), g(out.sunlit.cc), g(out.sunlit.comp), g(out.sunlit.vcMax),
										g(out.sunlit.jMax), g(out.sunlit.rad), g(out.sunlit.jj), g(out.sunlit.jv),
										g(out.shaded.LAI), g(out.shaded.gs), g(out.shaded.kc), g(out.shaded.ko), g(out.shaded.oi),
										g(out.shaded.ci), g(out.shaded.cc), g(out.shaded.comp), g(out.shaded.vcMax),
										g(out.shaded.jMax), g(out.shaded.rad), g(out.shaded.jj), g(out.shaded.jv),
									)
								}
							}
						}
					}
				}
			}
		}
	}

	// --- O3-impact: curated scenarios, varying a few dims at a time ---
	{
		par := core.make_o3_impact_params()

		Scenario :: struct {
			FC, WP, SWC, ET0, O3a, gs: f64,
			h:                         int,
			reldev, GDD_flo, GDD_mat, fO3s_d_prev, sum_O3_up: f64,
			waterDeficit:              bool,
		}
		scenarios := make([dynamic]Scenario, 0, context.temp_allocator)
		hs := []int{0, 1, 12}
		reldevs := []f64{0.05, 0.2, 0.5, 0.9}
		wds := []bool{false, true}
		for h in hs {
			for reldev in reldevs {
				for wd in wds {
					append(&scenarios, Scenario{0.30, 0.10, 0.20, 4.0, 40.0, 0.3, h, reldev, 500.0, 1200.0, 0.8, 10.0, wd})
				}
			}
		}
		SWCs := []f64{0.05, 0.10, 0.15, 0.20, 0.25, 0.35}
		sum_O3_ups := []f64{0.0, 5.0, 50.0}
		for SWC in SWCs {
			for sum_O3_up in sum_O3_ups {
				append(&scenarios, Scenario{0.30, 0.10, SWC, 4.0, 40.0, 0.3, 6, 0.4, 500.0, 1200.0, 0.9, sum_O3_up, true})
			}
		}

		for s in scenarios {
			in_: core.O3_Impact_In
			in_.FC = s.FC
			in_.WP = s.WP
			in_.SWC = s.SWC
			in_.ET0 = s.ET0
			in_.O3a = s.O3a
			in_.gs = s.gs
			in_.h = s.h
			in_.reldev = s.reldev
			in_.GDD_flo = s.GDD_flo
			in_.GDD_mat = s.GDD_mat
			in_.fO3s_d_prev = s.fO3s_d_prev
			in_.sum_O3_up = s.sum_O3_up

			out := core.o3_impact_hourly(in_, par, s.waterDeficit)
			row(
				"O3",
				g(s.FC), g(s.WP), g(s.SWC), g(s.ET0), g(s.O3a), g(s.gs), fmt.tprintf("%d", s.h),
				g(s.reldev), g(s.GDD_flo), g(s.GDD_mat), g(s.fO3s_d_prev), g(s.sum_O3_up),
				fmt.tprintf("%d", s.waterDeficit ? 1 : 0),
				g(out.hourly_O3_up), g(out.fO3s_d), g(out.fO3l), g(out.fLS), g(out.WS_st_clos),
			)
		}
	}

	// --- voc-guenther: vary species/microclimate over a curated set ---
	{
		mFols := []f64{0.0, 0.05, 0.3}
		slas := []f64{15.0, 25.0}
		lais := []f64{0.5, 3.0}
		ef_isos := []f64{0.0, 5.0, 20.0}
		ef_monos := []f64{0.0, 2.0}
		rads := []f64{0.0, 100.0, 600.0}
		tFols := []f64{-5.0, 15.0, 35.0}

		for mFol in mFols {
			for sla in slas {
				for lai in lais {
					for ef_iso in ef_isos {
						for ef_mono in ef_monos {
							for rad in rads {
								for tFol in tFols {
									sd := core.make_voc_species_data()
									sd.id = 1
									sd.mFol = mFol
									sd.sla = sla
									sd.lai = lai
									sd.EF_ISO = ef_iso
									sd.EF_MONO = ef_mono
									sd.EF_MONOS = ef_mono * 0.5

									mcd: core.Voc_Micro_Climate_Data
									mcd.rad = rad
									mcd.tFol = tFol

									ems := core.voc_guenther_emissions(sd, &mcd, 1.0, context.temp_allocator)
									row(
										"GUENTHER",
										g(mFol), g(sla), g(lai), g(ef_iso), g(ef_mono), g(rad), g(tFol),
										g(ems.isoprene_emission), g(ems.monoterpene_emission),
									)
								}
							}
						}
					}
				}
			}
		}
	}

	// --- voc-jjv: vary species/cpdata/microclimate over a curated set ---
	{
		mFols := []f64{0.0, 0.05, 0.3}
		slas := []f64{15.0, 25.0}
		tFols := []f64{-5.0, 15.0, 35.0}
		rads := []f64{0.0, 100.0, 600.0}
		co2s := []f64{350.0, 400.0, 450.0}
		jMaxs := []f64{0.0, 50.0, 150.0}
		vcMaxs := []f64{0.0, 30.0, 80.0}

		for mFol in mFols {
			for sla in slas {
				for tFol in tFols {
					for rad in rads {
						for co2 in co2s {
							for jMax in jMaxs {
								for vcMax in vcMaxs {
									sd := core.make_voc_species_data()
									sd.id = 2
									sd.mFol = mFol
									sd.sla = sla
									sd.lai = 2.0

									mcd: core.Voc_Micro_Climate_Data
									mcd.rad = rad
									mcd.rad24 = rad * 0.9
									mcd.rad240 = rad * 0.8
									mcd.tFol = tFol
									mcd.tFol24 = tFol - 1.0
									mcd.tFol240 = tFol - 2.0
									mcd.sunlitfoliagefraction24 = 0.6
									mcd.co2concentration = co2

									cpd: core.Voc_Cp_Data
									cpd.kc = 260.0
									cpd.ko = 179000.0
									cpd.oi = 210000.0
									cpd.ci = 280.0
									cpd.comp = 40.0
									cpd.vcMax = vcMax
									cpd.jMax = jMax

									ems := core.voc_jjv_emissions(sd, &mcd, cpd, 1.0, true, context.temp_allocator)
									row(
										"JJV",
										g(mFol), g(sla), g(tFol), g(rad), g(co2), g(jMax), g(vcMax),
										g(ems.isoprene_emission), g(ems.monoterpene_emission),
									)
								}
							}
						}
					}
				}
			}
		}
	}
}
