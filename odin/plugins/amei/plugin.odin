// The AMEI modules (plan-crop2ml-modules.md) as a plugin library
// (plan-module-plugins.md): the Crop2ML soil temperature and evapotranspiration
// modules other than MONICA's own two, which are built in
// (core/builtin_modules.odin).
//
//     pixi run python tools/odinw.py build-plugin amei
//
// then "Modules": {"Plugins": ["<odin>/build/plugins/amei"], ...} in sim.json
// (no extension: .so or .dll), or MONICA_MODULE_PLUGINS.
package amei_plugin

import et_amei_pot "../../monica/modules/evapotranspiration/monica_amei_pot_et"
import et_ref "../../monica/modules/evapotranspiration/monica_ref_et"
import etm "../../monica/modules/evapotranspiration"
import pl "../../monica/modules/plugin"
import stm "../../monica/modules/soil_temperature"
import st_apsim "../../monica/modules/soil_temperature/apsim_campbell"
import st_bps "../../monica/modules/soil_temperature/bioma_parton_swat"
import st_bss "../../monica/modules/soil_temperature/bioma_swat_swat"
import st_epic "../../monica/modules/soil_temperature/dssat_epicst"
import st_dssat "../../monica/modules/soil_temperature/dssat_st"
import st_simplace "../../monica/modules/soil_temperature/simplace"
import st_sq "../../monica/modules/soil_temperature/sq"
import st_stics "../../monica/modules/soil_temperature/stics"
import "base:runtime"

// the entries as variables: plugin_module_info keeps pointers to them
@(private = "file")
st_entries := [?]stm.Module_Entry {
	st_dssat.dssat_st_entry,
	st_epic.dssat_epicst_entry,
	st_apsim.apsim_campbell_entry,
	st_bps.bioma_parton_swat_entry,
	st_bss.bioma_swat_swat_entry,
	st_simplace.simplace_entry,
	st_sq.sq_entry,
	st_stics.stics_entry,
}
@(private = "file")
et_entries := [?]etm.Module_Entry{et_amei_pot.monica_amei_pot_et_entry, et_ref.monica_ref_et_entry}
@(private = "file")
infos: [len(st_entries) + len(et_entries)]pl.Module_Info

@(export, link_name = pl.ENTRY_SYMBOL)
monica_module_plugin :: proc "c" (n: ^i32) -> [^]pl.Module_Info {
	context = runtime.default_context()
	if infos[0].abi_version == 0 {
		for &e, i in st_entries {
			infos[i] = stm.plugin_module_info(&e)
		}
		for &e, i in et_entries {
			infos[len(st_entries) + i] = etm.plugin_module_info(&e)
		}
	}
	n^ = len(infos)
	return &infos[0]
}
