// The modules built into MONICA (plan-module-plugins.md). A module is a
// binding package in monica/modules/<process>/<name>/; it is built in by a line
// here, else it is built into a plugin library (plugins/, tools/odinw.py
// build-plugin) and loaded at startup. Built in: MONICA's own as Crop2ML
// packages, which check_drivers.py compares MONICA with. The AMEI modules are
// in plugins/amei.
package core

import etm "../modules/evapotranspiration"
import et_monica "../modules/evapotranspiration/monica_et"
import stm "../modules/soil_temperature"
import st_monica "../modules/soil_temperature/monica_soiltemp"
import "base:runtime"

@(init, private = "file")
register_builtin_modules :: proc "contextless" () {
	context = runtime.default_context()
	stm.register_builtin(st_monica.monica_soil_temp_entry)
	etm.register_builtin(et_monica.monica_et_entry)
}
