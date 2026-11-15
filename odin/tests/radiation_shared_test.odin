// Soil moisture's reference ET0 uses the crop's radiation function
// (plan-component-interfaces.md G1). Before that, it computed declination,
// day lengths and radiation itself, with the same formulas written
// separately. The only value of those it uses is the extraterrestrial
// radiation; this test keeps its former code as the reference and checks that
// crop_radiation gives the same bits for every day of the year and latitudes
// from -66 to 66 degrees.
package tests

import "core:testing"
import tl "../support/tools"
import "../support/cr_math"
import mcore "../monica/core"

// soil_moisture.odin's reference_evapotranspiration before G1, the part
// leading to the extraterrestrial radiation, verbatim.
@(private = "file")
former_soil_extraterrestrial_radiation :: proc(latitude: f64, julian_day: int) -> f64 {
	pi := 3.14159265358979323

	declination := -23.4 * cr_math.cos(2.0 * pi * ((f64(julian_day) + 10.0) / 365.0))
	declination_sinus := cr_math.sin(declination * pi / 180.0) * cr_math.sin(latitude * pi / 180.0)
	declination_cosinus := cr_math.cos(declination * pi / 180.0) * cr_math.cos(latitude * pi / 180.0)

	solar_constant := 0.082
	sc := 24.0 * 60.0 / pi * solar_constant * (1.0 + 0.033 * cr_math.cos(2.0 * pi * f64(julian_day) / 365.0))
	arg_sha := tl.bound(
		-1.0,
		-cr_math.tan(latitude * pi / 180.0) * cr_math.tan(declination * pi / 180.0),
		1.0,
	)
	sha := cr_math.acos(arg_sha)

	return sc * (sha * declination_sinus + declination_cosinus * cr_math.sin(sha)) // [MJ m-2]
}

@(test)
test_soil_et0_extraterrestrial_radiation_is_the_crops :: proc(t: ^testing.T) {
	mismatches := 0
	for lat10 := -660; lat10 <= 660; lat10 += 5 {
		latitude := f64(lat10) / 10.0
		for julian_day in 1 ..= 366 {
			want := former_soil_extraterrestrial_radiation(latitude, julian_day)
			got := mcore.crop_radiation({julian_day = f64(julian_day), latitude = latitude}).extraterrestrial_radiation
			if transmute(u64)want != transmute(u64)got {
				mismatches += 1
				if mismatches <= 5 {
					testing.expectf(t, false, "lat %v day %v: soil %v, crop %v", latitude, julian_day, want, got)
				}
			}
		}
	}
	testing.expectf(t, mismatches == 0, "%d mismatches", mismatches)
}
