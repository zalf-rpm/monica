// Odin side of the phase 3 pwp/fc/sat interpolation sweep.
//
// Must emit byte-identical output to
// odin/tests/cpp_ref/soil_pwp_fc_sat_ref_main.cpp. Run
// odin/tests/cpp_ref/run_soil_pwp_fc_sat.sh to build both and diff them.
//
// Doubles are formatted via jx.dump(jx.f(v)), which phase 1a validated to
// match MSVC's "%.17g" byte-for-byte - the same formatting the C++ driver uses
// directly via printf("%.17g", ...).
package soil_pwp_fc_sat_ref

import "core:fmt"
import "core:os"
import soil "../../monica/soil"
import jx "../../support/jsonx"
import tl "../../support/tools"

main :: proc() {
	arena: jx.Arena
	if !jx.arena_init(&arena) {
		fmt.eprintln("arena init failed")
		os.exit(1)
	}
	defer jx.arena_destroy(&arena)
	a := jx.arena_allocator(&arena)

	path_to_soil_dir := tl.fix_system_separator(
		tl.replace_env_vars("${MONICA_PARAMETERS}/soil/", a),
		a,
	)

	g :: proc(v: f64, a: jx.Allocator) -> string {
		return jx.dump(jx.f(v), a)
	}

	textures := []string {
		"HH", "HN", "LS2", "LS3", "LS4", "LT2", "LT3", "LTS", "LU", "SL2", "SL3", "SL4", "SLU",
		"SS", "ST2", "ST3", "SU2", "SU3", "SU4", "TL", "TS2", "TS3", "TS4", "TT", "TU2", "TU3",
		"TU4", "ULS", "US", "UT2", "UT3", "UT4", "UU", "FS", "FSGS", "FSMS", "GS", "MS", "MSFS",
		"MSGS", "XX", // unknown - error path
	}
	raw_densities := make([dynamic]f64, 0, a)
	for rd: f64 = 900; rd <= 2100; rd += 50 {
		append(&raw_densities, rd)
	}
	organic_matters := []f64{0.0, 0.005, 0.015, 0.03, 0.06, 0.115, 0.15}

	for tex in textures {
		for rd in raw_densities {
			for om in organic_matters {
				sp := soil.make_soil_parameters()
				sp.vs_SoilTexture = tex
				sp.vs_SoilStoneContent = 0.1
				sp._vs_SoilRawDensity = rd
				sp._vs_SoilOrganicMatter = om
				sp.vs_FieldCapacity = -1
				sp.vs_Saturation = -1
				sp.vs_PermanentWiltingPoint = -1
				e := soil.update_unset_pwp_fc_sat_from_ka5_texture_class(path_to_soil_dir, &sp, a)
				fmt.printf(
					"KA5\t%s\t%s\t%s\t%d\t%s\t%s\t%s\n",
					tex,
					g(rd, a),
					g(om, a),
					tl.success(e) ? 1 : 0,
					g(sp.vs_FieldCapacity, a),
					g(sp.vs_Saturation, a),
					g(sp.vs_PermanentWiltingPoint, a),
				)
			}
		}
	}

	{
		clays_for_fallback := []f64{0.05, 0.15, 0.3}
		for clay in clays_for_fallback {
			sp := soil.make_soil_parameters()
			sp.vs_SoilTexture = "LS2"
			sp.vs_SoilClayContent = clay
			sp.vs_SoilStoneContent = 0.0
			sp._vs_SoilBulkDensity = 1500
			sp._vs_SoilOrganicCarbon = 0.01
			sp.vs_FieldCapacity = -1
			sp.vs_Saturation = -1
			sp.vs_PermanentWiltingPoint = -1
			e := soil.update_unset_pwp_fc_sat_from_ka5_texture_class(path_to_soil_dir, &sp, a)
			fmt.printf(
				"KA5FB\t%s\t%d\t%s\t%s\t%s\n",
				g(clay, a),
				tl.success(e) ? 1 : 0,
				g(sp.vs_FieldCapacity, a),
				g(sp.vs_Saturation, a),
				g(sp.vs_PermanentWiltingPoint, a),
			)
		}
	}

	sands := []f64{0.0, 0.1, 0.2, 0.3, 0.4, 0.5, 0.6, 0.7, 0.8, 0.9, 1.0}
	clays := []f64{0.0, 0.05, 0.1, 0.2, 0.3, 0.4}
	bulk_densities := []f64{1100, 1300, 1500, 1700, 1900}
	org_carbons := []f64{0.005, 0.01, 0.02, 0.03}
	stones := []f64{0.0, 0.15}
	layer_nos := []int{1, 5}

	for sand in sands {
		for clay in clays {
			if sand + clay > 1.0 {
				continue
			}
			for bd in bulk_densities {
				for oc in org_carbons {
					for stone in stones {
						{
							sp := soil.make_soil_parameters()
							sp.vs_SoilSandContent = sand
							sp.vs_SoilClayContent = clay
							sp.vs_SoilStoneContent = stone
							sp._vs_SoilBulkDensity = bd
							sp._vs_SoilOrganicCarbon = oc
							sp.vs_FieldCapacity = -1
							sp.vs_Saturation = -1
							sp.vs_PermanentWiltingPoint = -1
							e := soil.update_unset_pwp_fc_sat_from_van_genuchten_vereecken(&sp)
							fmt.printf(
								"VGV\t%s\t%s\t%s\t%s\t%s\t%d\t%s\t%s\t%s\n",
								g(sand, a),
								g(clay, a),
								g(bd, a),
								g(oc, a),
								g(stone, a),
								tl.success(e) ? 1 : 0,
								g(sp.vs_FieldCapacity, a),
								g(sp.vs_Saturation, a),
								g(sp.vs_PermanentWiltingPoint, a),
							)
						}
						for layer_no in layer_nos {
							sp := soil.make_soil_parameters()
							sp.vs_SoilSandContent = sand
							sp.vs_SoilClayContent = clay
							sp.vs_SoilStoneContent = stone
							sp._vs_SoilBulkDensity = bd
							sp._vs_SoilOrganicCarbon = oc
							sp.vs_FieldCapacity = -1
							sp.vs_Saturation = -1
							sp.vs_PermanentWiltingPoint = -1
							e := soil.update_unset_pwp_fc_sat_from_van_genuchten_toth(&sp, layer_no)
							fmt.printf(
								"VGT\t%s\t%s\t%s\t%s\t%s\t%d\t%d\t%s\t%s\t%s\n",
								g(sand, a),
								g(clay, a),
								g(bd, a),
								g(oc, a),
								g(stone, a),
								layer_no,
								tl.success(e) ? 1 : 0,
								g(sp.vs_FieldCapacity, a),
								g(sp.vs_Saturation, a),
								g(sp.vs_PermanentWiltingPoint, a),
							)
						}
						{
							sp := soil.make_soil_parameters()
							sp.vs_SoilSandContent = sand
							sp.vs_SoilClayContent = clay
							sp.vs_SoilStoneContent = stone
							sp._vs_SoilBulkDensity = bd
							sp._vs_SoilOrganicCarbon = oc
							sp.vs_FieldCapacity = -1
							sp.vs_Saturation = -1
							sp.vs_PermanentWiltingPoint = -1
							e := soil.update_unset_pwp_fc_sat_from_toth(&sp)
							fmt.printf(
								"TOTH\t%s\t%s\t%s\t%s\t%s\t%d\t%s\t%s\t%s\n",
								g(sand, a),
								g(clay, a),
								g(bd, a),
								g(oc, a),
								g(stone, a),
								tl.success(e) ? 1 : 0,
								g(sp.vs_FieldCapacity, a),
								g(sp.vs_Saturation, a),
								g(sp.vs_PermanentWiltingPoint, a),
							)
						}
					}
				}
			}
		}
	}

	// -1 sentinel combinations (plan-odin.md phase 3's "required second oracle"):
	// every case above leaves vs_FieldCapacity/vs_Saturation/vs_PermanentWiltingPoint
	// ALL unset (-1). But update_unset_pwp_fc_sat_from_* checks each of the three
	// independently after the disjunctive "is anything unset" guard - the exact
	// sentinel-with-fallback shape that caused the ff0f0fc regression (plan.md)
	// elsewhere. Sweep all 8 combinations of which of the three are preset
	// (non-negative) vs -1, through each of the four entry points. Mirrors
	// odin/tests/cpp_ref/soil_pwp_fc_sat_ref_main.cpp's SENT section exactly.
	{
		fc_preset, sat_preset, pwp_preset :: 0.35, 0.45, 0.12
		for mask in 0 ..< 8 {
			fc_set := mask & 1 != 0
			sat_set := mask & 2 != 0
			pwp_set := mask & 4 != 0

			{
				sp := soil.make_soil_parameters()
				sp.vs_SoilTexture = "LS2"
				sp.vs_SoilStoneContent = 0.1
				sp._vs_SoilRawDensity = 1500
				sp._vs_SoilOrganicMatter = 0.03
				sp.vs_FieldCapacity = fc_set ? fc_preset : -1
				sp.vs_Saturation = sat_set ? sat_preset : -1
				sp.vs_PermanentWiltingPoint = pwp_set ? pwp_preset : -1
				e := soil.update_unset_pwp_fc_sat_from_ka5_texture_class(path_to_soil_dir, &sp, a)
				fmt.printf(
					"KA5SENT\t%d\t%d\t%s\t%s\t%s\n",
					mask,
					tl.success(e) ? 1 : 0,
					g(sp.vs_FieldCapacity, a),
					g(sp.vs_Saturation, a),
					g(sp.vs_PermanentWiltingPoint, a),
				)
			}
			{
				sp := soil.make_soil_parameters()
				sp.vs_SoilSandContent = 0.3
				sp.vs_SoilClayContent = 0.15
				sp.vs_SoilStoneContent = 0.1
				sp._vs_SoilBulkDensity = 1500
				sp._vs_SoilOrganicCarbon = 0.01
				sp.vs_FieldCapacity = fc_set ? fc_preset : -1
				sp.vs_Saturation = sat_set ? sat_preset : -1
				sp.vs_PermanentWiltingPoint = pwp_set ? pwp_preset : -1
				e := soil.update_unset_pwp_fc_sat_from_van_genuchten_vereecken(&sp)
				fmt.printf(
					"VGVSENT\t%d\t%d\t%s\t%s\t%s\n",
					mask,
					tl.success(e) ? 1 : 0,
					g(sp.vs_FieldCapacity, a),
					g(sp.vs_Saturation, a),
					g(sp.vs_PermanentWiltingPoint, a),
				)
			}
			{
				sp := soil.make_soil_parameters()
				sp.vs_SoilSandContent = 0.3
				sp.vs_SoilClayContent = 0.15
				sp.vs_SoilStoneContent = 0.1
				sp._vs_SoilBulkDensity = 1500
				sp._vs_SoilOrganicCarbon = 0.01
				sp.vs_FieldCapacity = fc_set ? fc_preset : -1
				sp.vs_Saturation = sat_set ? sat_preset : -1
				sp.vs_PermanentWiltingPoint = pwp_set ? pwp_preset : -1
				e := soil.update_unset_pwp_fc_sat_from_van_genuchten_toth(&sp, 1)
				fmt.printf(
					"VGTSENT\t%d\t%d\t%s\t%s\t%s\n",
					mask,
					tl.success(e) ? 1 : 0,
					g(sp.vs_FieldCapacity, a),
					g(sp.vs_Saturation, a),
					g(sp.vs_PermanentWiltingPoint, a),
				)
			}
			{
				sp := soil.make_soil_parameters()
				sp.vs_SoilSandContent = 0.3
				sp.vs_SoilClayContent = 0.15
				sp.vs_SoilStoneContent = 0.1
				sp._vs_SoilBulkDensity = 1500
				sp._vs_SoilOrganicCarbon = 0.01
				sp.vs_FieldCapacity = fc_set ? fc_preset : -1
				sp.vs_Saturation = sat_set ? sat_preset : -1
				sp.vs_PermanentWiltingPoint = pwp_set ? pwp_preset : -1
				e := soil.update_unset_pwp_fc_sat_from_toth(&sp)
				fmt.printf(
					"TOTHSENT\t%d\t%d\t%s\t%s\t%s\n",
					mask,
					tl.success(e) ? 1 : 0,
					g(sp.vs_FieldCapacity, a),
					g(sp.vs_Saturation, a),
					g(sp.vs_PermanentWiltingPoint, a),
				)
			}
		}
	}
}
