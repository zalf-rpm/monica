// Correctly rounded transcendental functions: mas_cpp_misc/cr_math, i.e.
// CORE-MATH (https://core-math.gitlabpages.inria.fr/, MIT). Each returns the
// double nearest to the exact result, so results depend neither on the
// platform's math library (glibc vs. Microsoft's UCRT round differently in
// the last bit) nor on the compiler. The C++ MONICA calls the same library
// (`cr::exp` ...), which is what lets Odin and C++ match bit for bit on Linux
// and Windows alike. See ../../CONVENTIONS.md §1.
//
// The static library is built from ../../../mas_cpp_misc/cr_math by
// tools/odinw.py (`build`/`test` do it on demand) into ../../build/cr_math/.
// Its objects use FMA instructions (CR_MATH_FMA, as in the C++ build), so the
// executables need an x86-64 CPU with FMA (Intel 2013+, AMD 2012+).
package cr_math

// Off Windows by name, with the directory passed as -L (odinw.py; in Nix the
// cr-math derivation is a build input): a path import would become
// `-l:/abs/path/libcr_math_lib.a`, which not every GNU ld accepts.
when ODIN_OS == .Windows {
	foreign import lib "../../build/cr_math/cr_math_lib.lib"
} else {
	foreign import lib "system:cr_math_lib"
}

@(default_calling_convention = "c", link_prefix = "cr_")
foreign lib {
	exp   :: proc(x: f64) -> f64 ---
	log   :: proc(x: f64) -> f64 ---
	log2  :: proc(x: f64) -> f64 ---
	log10 :: proc(x: f64) -> f64 ---
	pow   :: proc(x, y: f64) -> f64 ---
	sin   :: proc(x: f64) -> f64 ---
	cos   :: proc(x: f64) -> f64 ---
	tan   :: proc(x: f64) -> f64 ---
	asin  :: proc(x: f64) -> f64 ---
	acos  :: proc(x: f64) -> f64 ---
	atan  :: proc(x: f64) -> f64 ---
}
