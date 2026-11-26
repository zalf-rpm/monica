// A module plugin MONICA must refuse, module by module (tests/check_plugins.py):
// another ABI version, an unknown process, another Input layout, the name of a
// built-in module. One good module, which still loads.
package test_broken_plugin

import pl "../../../monica/modules/plugin"
import stm "../../../monica/modules/soil_temperature"
import "base:runtime"

@(private = "file")
dssat_st := stm.dssat_st_entry
@(private = "file")
infos: [5]pl.Module_Info

@(export, link_name = pl.ENTRY_SYMBOL)
monica_module_plugin :: proc "c" (n: ^i32) -> [^]pl.Module_Info {
	context = runtime.default_context()
	if infos[0].abi_version == 0 {
		infos[0] = stm.plugin_module_info(&dssat_st, "Broken_Abi")
		infos[0].abi_version = pl.ABI_VERSION + 1
		infos[1] = stm.plugin_module_info(&dssat_st, "Broken_Process")
		infos[1].process = "NoSuchProcess"
		infos[2] = stm.plugin_module_info(&dssat_st, "Broken_Layout")
		infos[2].input_hash += 1
		infos[3] = stm.plugin_module_info(&dssat_st) // DSSAT_ST_standalone, a built-in name
		infos[4] = stm.plugin_module_info(&dssat_st, "Broken_Plugin_Good_Module")
	}
	n^ = len(infos)
	return &infos[0]
}
