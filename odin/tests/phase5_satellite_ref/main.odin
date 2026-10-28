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
		lais := []f64{0.0, 1.0, 4.0}
		leaf_temps := []f64{-5.0, 15.0, 30.0}
		vpds := []f64{0.2, 2.0}
		cas := []f64{300.0, 600.0}

		par := core.make_fvcb_canopy_hourly_params()
		par.vcmax_25 = 60.0

		for global_rad in global_rads {
			for extra_terr_rad in extra_terr_rads {
				for solar_el in solar_els {
					for lai in lais {
						for leaf_temp in leaf_temps {
							for vpd in vpds {
								for ca in cas {
									in_: core.Fvcb_Canopy_Hourly_In
									in_.global_rad = global_rad
									in_.extra_terr_rad = extra_terr_rad
									in_.solar_el = solar_el
									in_.lai = lai
									in_.leaf_temp = leaf_temp
									in_.vpd = vpd
									in_.ca = ca

									out := core.fvcb_canopy_hourly_c3(in_, par)
									row(
										"FVCB",
										g(global_rad), g(extra_terr_rad), g(solar_el), g(lai), g(leaf_temp), g(vpd), g(ca),
										g(out.canopy_net_photos), g(out.canopy_resp), g(out.canopy_gross_photos), g(out.jmax_c),
										g(out.sunlit.lai), g(out.sunlit.gs), g(out.sunlit.kc), g(out.sunlit.ko), g(out.sunlit.oi),
										g(out.sunlit.ci), g(out.sunlit.cc), g(out.sunlit.comp), g(out.sunlit.vc_max),
										g(out.sunlit.j_max), g(out.sunlit.rad), g(out.sunlit.jj), g(out.sunlit.jv),
										g(out.shaded.lai), g(out.shaded.gs), g(out.shaded.kc), g(out.shaded.ko), g(out.shaded.oi),
										g(out.shaded.ci), g(out.shaded.cc), g(out.shaded.comp), g(out.shaded.vc_max),
										g(out.shaded.j_max), g(out.shaded.rad), g(out.shaded.jj), g(out.shaded.jv),
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
			fc, wp, swc, et0, o3a, gs:                         f64,
			h:                                                 int,
			reldev, gdd_flo, gdd_mat, f_o3s_d_prev, sum_o3_up: f64,
			water_deficit:                                     bool,
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
		swcs := []f64{0.05, 0.10, 0.15, 0.20, 0.25, 0.35}
		sum_o3_ups := []f64{0.0, 5.0, 50.0}
		for swc in swcs {
			for sum_o3_up in sum_o3_ups {
				append(&scenarios, Scenario{0.30, 0.10, swc, 4.0, 40.0, 0.3, 6, 0.4, 500.0, 1200.0, 0.9, sum_o3_up, true})
			}
		}

		for s in scenarios {
			in_: core.O3_Impact_In
			in_.fc = s.fc
			in_.wp = s.wp
			in_.swc = s.swc
			in_.et0 = s.et0
			in_.o3a = s.o3a
			in_.gs = s.gs
			in_.h = s.h
			in_.reldev = s.reldev
			in_.gdd_flo = s.gdd_flo
			in_.gdd_mat = s.gdd_mat
			in_.f_o3s_d_prev = s.f_o3s_d_prev
			in_.sum_o3_up = s.sum_o3_up

			out := core.o3_impact_hourly(in_, par, s.water_deficit)
			row(
				"O3",
				g(s.fc), g(s.wp), g(s.swc), g(s.et0), g(s.o3a), g(s.gs), fmt.tprintf("%d", s.h),
				g(s.reldev), g(s.gdd_flo), g(s.gdd_mat), g(s.f_o3s_d_prev), g(s.sum_o3_up),
				fmt.tprintf("%d", s.water_deficit ? 1 : 0),
				g(out.hourly_o3_up), g(out.f_o3s_d), g(out.f_o3l), g(out.f_ls), g(out.ws_st_clos),
			)
		}
	}

	// --- voc-guenther: vary species/microclimate over a curated set ---
	{
		m_fols := []f64{0.0, 0.05, 0.3}
		slas := []f64{15.0, 25.0}
		lais := []f64{0.5, 3.0}
		ef_isos := []f64{0.0, 5.0, 20.0}
		ef_monos := []f64{0.0, 2.0}
		rads := []f64{0.0, 100.0, 600.0}
		t_fols := []f64{-5.0, 15.0, 35.0}

		for m_fol in m_fols {
			for sla in slas {
				for lai in lais {
					for ef_iso in ef_isos {
						for ef_mono in ef_monos {
							for rad in rads {
								for t_fol in t_fols {
									sd := core.make_voc_species_data()
									sd.id = 1
									sd.m_fol = m_fol
									sd.sla = sla
									sd.lai = lai
									sd.ef_iso = ef_iso
									sd.ef_mono = ef_mono
									sd.ef_monos = ef_mono * 0.5

									mcd: core.Voc_Micro_Climate_Data
									mcd.rad = rad
									mcd.t_fol = t_fol

									ems := core.voc_guenther_emissions(sd, &mcd, 1.0, context.temp_allocator)
									row(
										"GUENTHER",
										g(m_fol), g(sla), g(lai), g(ef_iso), g(ef_mono), g(rad), g(t_fol),
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
		m_fols := []f64{0.0, 0.05, 0.3}
		slas := []f64{15.0, 25.0}
		t_fols := []f64{-5.0, 15.0, 35.0}
		rads := []f64{0.0, 100.0, 600.0}
		co2s := []f64{350.0, 400.0, 450.0}
		j_maxs := []f64{0.0, 50.0, 150.0}
		vc_maxs := []f64{0.0, 30.0, 80.0}

		for m_fol in m_fols {
			for sla in slas {
				for t_fol in t_fols {
					for rad in rads {
						for co2 in co2s {
							for j_max in j_maxs {
								for vc_max in vc_maxs {
									sd := core.make_voc_species_data()
									sd.id = 2
									sd.m_fol = m_fol
									sd.sla = sla
									sd.lai = 2.0

									mcd: core.Voc_Micro_Climate_Data
									mcd.rad = rad
									mcd.rad24 = rad * 0.9
									mcd.rad240 = rad * 0.8
									mcd.t_fol = t_fol
									mcd.t_fol24 = t_fol - 1.0
									mcd.t_fol240 = t_fol - 2.0
									mcd.sunlitfoliagefraction24 = 0.6
									mcd.co2concentration = co2

									cpd: core.Voc_Cp_Data
									cpd.kc = 260.0
									cpd.ko = 179000.0
									cpd.oi = 210000.0
									cpd.ci = 280.0
									cpd.comp = 40.0
									cpd.vc_max = vc_max
									cpd.j_max = j_max

									ems := core.voc_jjv_emissions(sd, &mcd, cpd, 1.0, true, context.temp_allocator)
									row(
										"JJV",
										g(m_fol), g(sla), g(t_fol), g(rad), g(co2), g(j_max), g(vc_max),
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
