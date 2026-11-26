// A module plugin for the checks (plan-module-plugins.md): MONICA's two
// built-in modules again, under other names, so that MONICA can run each next
// to its built-in twin and tests/check_plugins.py can compare them.
//
//     python tools/odinw.py build-plugin test_modules
//
// A plugin library is a package like this one: it imports the process
// packages and its modules' binding packages, and exports monica_module_plugin,
// which lists the modules as Module_Info (the process package's
// plugin_module_info). plugins/amei is the real one.
package test_modules_plugin

import etm "../../../monica/modules/evapotranspiration"
import et_monica "../../../monica/modules/evapotranspiration/monica_et"
import pl "../../../monica/modules/plugin"
import stm "../../../monica/modules/soil_temperature"
import st_monica "../../../monica/modules/soil_temperature/monica_soiltemp"
import "base:runtime"

@(private = "file")
monica_et := et_monica.monica_et_entry
@(private = "file")
monica_soiltemp := st_monica.monica_soil_temp_entry
@(private = "file")
infos: [2]pl.Module_Info

@(export, link_name = pl.ENTRY_SYMBOL)
monica_module_plugin :: proc "c" (n: ^i32) -> [^]pl.Module_Info {
	context = runtime.default_context()
	if infos[0].abi_version == 0 {
		infos[0] = etm.plugin_module_info(&monica_et, "Plugin_Monica_Evapotranspiration")
		infos[1] = stm.plugin_module_info(&monica_soiltemp, "Plugin_Monica_SoilTemp")
	}
	n^ = len(infos)
	return &infos[0]
}
