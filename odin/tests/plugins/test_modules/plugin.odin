// A module plugin for the checks (plan-module-plugins.md): two built-in
// modules again, under other names, so that MONICA can run each next to its
// built-in twin and tests/check_drivers.py can compare them.
//
//     python tools/odinw.py build-plugin test_modules
//
// A plugin library is a package like this one: it imports the process
// packages and its modules' bindings, and exports monica_module_plugin,
// which lists the modules as Module_Info (the process package's
// plugin_module_info).
package test_modules_plugin

import etm "../../../monica/modules/evapotranspiration"
import pl "../../../monica/modules/plugin"
import stm "../../../monica/modules/soil_temperature"
import "base:runtime"

@(private = "file")
ref_et := etm.monica_ref_et_entry
@(private = "file")
dssat_st := stm.dssat_st_entry
@(private = "file")
infos: [2]pl.Module_Info

@(export, link_name = pl.ENTRY_SYMBOL)
monica_module_plugin :: proc "c" (n: ^i32) -> [^]pl.Module_Info {
	context = runtime.default_context()
	if infos[0].abi_version == 0 {
		infos[0] = etm.plugin_module_info(&ref_et, "Plugin_Monica_Reference_Evapotranspiration")
		infos[1] = stm.plugin_module_info(&dssat_st, "Plugin_DSSAT_ST_standalone")
	}
	n^ = len(infos)
	return &infos[0]
}
