// Port of src/run/capnp-helper.{h,cpp} - converting Cap'n Proto climate and soil
// data into what run_monica expects.
//
// The C++ functions are promise-shaped: dataAccessorFromTimeSeries sends
// range/header/dataT and chains their .then()s; fromCapnpSoilProfile sends data
// and chains one. Here the RPC half lives in run_monica_capnp.odin (the async
// handler and its continuations, which is where the shim's threading rules
// apply), and what is left in this file is the pure conversion each .then() body
// performs, taking the already-received result. Same code, split at the promise
// boundary rather than at the C++ function boundary - there is no way to keep
// both halves together without a promise type.
//
// Not ported: the two dailyClimateDataToDailyClimateMap overloads. They serve
// monica-capnp-fbp-component-main.cpp / daily-monica-fbp-component-main.cpp (the
// FBP components), which are not part of this server.
package capnp

import capnp_dyn "../../support/capnp/odin/capnp_dynamic"
import clim "../../support/climate"
import d "../../support/date"
import jx "../../support/jsonx"

// C++: Climate::ACD monica::climateElementToACD(mas::schema::climate::Element e)
//
// Matched on the enumerant NAME rather than the ordinal: Enum_Value carries both,
// but climate.capnp's Element has grown entries since (dewpointTemp,
// specificHumidity, snowfallFlux, ...) and names are what survive a schema
// version skew. Everything the C++ does not name maps to skip, exactly as its
// `default:` does.
climate_element_to_acd :: proc(e: capnp_dyn.Enum_Value) -> clim.ACD {
	switch e.name {
	case "tmin":
		return .tmin
	case "tavg":
		return .tavg
	case "tmax":
		return .tmax
	case "precip":
		return .precip
	case "relhumid":
		return .relhumid
	case "wind":
		return .wind
	case "globrad":
		return .globrad
	}
	return .skip
}

// C++: Climate::DataAccessor monica::fromCapnpData(const Tools::Date &startDate,
//        const Tools::Date &endDate, List<Element>::Reader header,
//        List<List<float>>::Reader data)
//
// `data` is the TRANSPOSED form (one inner list per climate element, matching
// `header`), i.e. what TimeSeries.dataT returns - which is what
// dataAccessorFromTimeSeries feeds it.
from_capnp_data :: proc(
	start_date: d.Date,
	end_date: d.Date,
	header: []capnp_dyn.Value,
	data: []capnp_dyn.Value,
	allocator := context.allocator,
) -> clim.Data_Accessor {
	if len(data) == 0 {
		return clim.Data_Accessor{}
	}

	da := clim.make_data_accessor_range(start_date, end_date, allocator)

	// NOTE(c++-quirk): the C++ sizes every column from data[0].size() but fills
	// it from vs.size(), so a short column silently keeps trailing zeros and a
	// long one runs off the end. Reproduced for the short case (zero-filled to
	// n_steps); the long case is truncated rather than reproduced as the C++'s
	// out-of-bounds write.
	n_steps := 0
	if first, ok := data[0].([]capnp_dyn.Value); ok {
		n_steps = len(first)
	}

	for i in 0 ..< len(header) {
		// The C++ indexes data[i] for every header entry without checking that
		// the two lists agree in length; a shorter `data` would be an
		// out-of-bounds read there.
		if i >= len(data) {
			break
		}
		element, is_enum := header[i].(capnp_dyn.Enum_Value)
		if !is_enum {
			continue
		}
		acd := climate_element_to_acd(element)
		if acd == .skip {
			continue // C++: `default:;` - the element is simply not stored
		}

		vs, is_list := data[i].([]capnp_dyn.Value)
		if !is_list {
			continue
		}
		col := make([]f64, n_steps, allocator)
		for k in 0 ..< min(len(vs), n_steps) {
			col[k], _ = vs[k].(f64)
		}
		clim.data_accessor_add_climate_data(&da, acd, col, allocator)
	}
	return da
}

// C++: the .then() body of kj::Promise<J11Array> monica::fromCapnpSoilProfile(
//        mas::schema::soil::Profile::Client profile)
//
// `data` is soil.capnp's ProfileData, i.e. the result struct of Profile.data().
// Returns the J11Array of layer objects the C++ merges under
// "SoilProfileParameters".
soil_layers_from_capnp_profile_data :: proc(
	data: []capnp_dyn.Field,
	allocator := context.allocator,
) -> jx.Value {
	ls := make(jx.Array, 0, allocator)

	layers_value, has_layers := capnp_dyn.field_get(data, "layers")
	if !has_layers {
		return jx.Value(ls)
	}
	layers, layers_is_list := layers_value.([]capnp_dyn.Value)
	if !layers_is_list {
		return jx.Value(ls)
	}

	for layer_value in layers {
		layer, layer_is_struct := layer_value.([]capnp_dyn.Field)
		if !layer_is_struct {
			continue
		}

		l := make(jx.Object, 0, allocator)
		// C++: l["Thickness"] = layer.getSize();
		if v, got := capnp_dyn.field_get(layer, "size"); got {
			if size, is_f := v.(f64); is_f {
				jx.obj_set(&l, "Thickness", jx.f(size), allocator)
			}
		}

		props_value, has_props := capnp_dyn.field_get(layer, "properties")
		props, props_is_list := props_value.([]capnp_dyn.Value)
		if has_props && props_is_list {
			for prop_value in props {
				prop, prop_is_struct := prop_value.([]capnp_dyn.Field)
				if !prop_is_struct {
					continue
				}
				soil_property_to_layer_json(&l, prop, allocator)
			}
		}

		append(&ls, jx.Value(l))
	}
	return jx.Value(ls)
}

// C++: the `switch (prop.getName())` inside fromCapnpSoilProfile's loop.
//
// Layer.Property is a union (f32Value | bValue | type | unset) next to `name`;
// the shim only reports the ACTIVE union member as a field (capnp's has() is
// false for the others), so the C++'s isF32Value()/isBValue()/isType() guards
// become "did that field come through at all". Where the C++ omits the guard
// (AMMONIUM, SOIL_MOISTURE - it calls getF32Value() unconditionally, which
// returns garbage for a non-f32 union member) this reproduces the intent, not
// the bug: an inactive member is simply not written.
@(private)
soil_property_to_layer_json :: proc(l: ^jx.Object, prop: []capnp_dyn.Field, allocator: jx.Allocator) {
	name_value, has_name := capnp_dyn.field_get(prop, "name")
	if !has_name {
		return
	}
	name, name_is_enum := name_value.(capnp_dyn.Enum_Value)
	if !name_is_enum {
		return
	}

	f32_value, has_f32 := f64_field(prop, "f32Value")
	b_value, has_b := bool_field(prop, "bValue")
	type_value, has_type := string_field(prop, "type")

	switch name.name {
	case "sand":
		if has_f32 {jx.obj_set(l, "Sand", jx.f(f32_value / 100.0), allocator)}
	case "clay":
		if has_f32 {jx.obj_set(l, "Clay", jx.f(f32_value / 100.0), allocator)}
	case "silt":
		if has_f32 {jx.obj_set(l, "Silt", jx.f(f32_value / 100.0), allocator)}
	case "organicCarbon":
		if has_f32 {jx.obj_set(l, "SoilOrganicCarbon", jx.f(f32_value), allocator)}
	case "organicMatter":
		if has_f32 {jx.obj_set(l, "SoilOrganicMatter", jx.f(f32_value / 100.0), allocator)}
	case "bulkDensity":
		if has_f32 {jx.obj_set(l, "SoilBulkDensity", jx.f(f32_value), allocator)}
	case "rawDensity":
		if has_f32 {jx.obj_set(l, "SoilRawDensity", jx.f(f32_value), allocator)}
	case "pH":
		if has_f32 {jx.obj_set(l, "pH", jx.f(f32_value), allocator)}
	case "soilType":
		if has_type {jx.obj_set(l, "KA5TextureClass", jx.s(type_value, allocator), allocator)}
	case "permanentWiltingPoint":
		if has_f32 {jx.obj_set(l, "PermanentWiltingPoint", jx.f(f32_value / 100.0), allocator)}
	case "fieldCapacity":
		if has_f32 {jx.obj_set(l, "FieldCapacity", jx.f(f32_value / 100.0), allocator)}
	case "saturation":
		if has_f32 {jx.obj_set(l, "PoreVolume", jx.f(f32_value / 100.0), allocator)}
	case "soilWaterConductivityCoefficient":
		if has_f32 {jx.obj_set(l, "Lambda", jx.f(f32_value), allocator)}
	case "sceleton":
		if has_f32 {jx.obj_set(l, "Sceleton", jx.f(f32_value / 100.0), allocator)}
	case "ammonium":
		// C++ has no isF32Value() guard here - see this procedure's comment.
		if has_f32 {jx.obj_set(l, "SoilAmmonium", jx.f(f32_value), allocator)}
	case "nitrate":
		if has_f32 {jx.obj_set(l, "SoilNitrate", jx.f(f32_value), allocator)}
	case "cnRatio":
		if has_f32 {jx.obj_set(l, "CN", jx.f(f32_value), allocator)}
	case "soilMoisture":
		// C++ has no isF32Value() guard here either.
		if has_f32 {jx.obj_set(l, "SoilMoisturePercentFC", jx.f(f32_value), allocator)}
	case "inGroundwater":
		if has_b {jx.obj_set(l, "is_in_groundwater", jx.b(b_value), allocator)}
	case "impenetrable":
		if has_b {jx.obj_set(l, "is_impenetrable", jx.b(b_value), allocator)}
	}
}

@(private)
f64_field :: proc(fields: []capnp_dyn.Field, name: string) -> (f64, bool) {
	if v, got := capnp_dyn.field_get(fields, name); got {
		if f, is_f := v.(f64); is_f {
			return f, true
		}
	}
	return 0, false
}

@(private)
bool_field :: proc(fields: []capnp_dyn.Field, name: string) -> (bool, bool) {
	if v, got := capnp_dyn.field_get(fields, name); got {
		if b, is_b := v.(bool); is_b {
			return b, true
		}
	}
	return false, false
}

@(private)
string_field :: proc(fields: []capnp_dyn.Field, name: string) -> (string, bool) {
	if v, got := capnp_dyn.field_get(fields, name); got {
		if s, is_s := v.(string); is_s {
			return s, true
		}
	}
	return "", false
}

// C++: Tools::Date(sd.getDay(), sd.getMonth(), sd.getYear()) applied to a
// common.capnp Date, as dataAccessorFromTimeSeries does for range's start/end.
@(private)
date_from_capnp :: proc(fields: []capnp_dyn.Field) -> d.Date {
	day, month, year: int
	if v, got := capnp_dyn.field_get(fields, "day"); got {
		if u, is_u := v.(u64); is_u {day = int(u)}
	}
	if v, got := capnp_dyn.field_get(fields, "month"); got {
		if u, is_u := v.(u64); is_u {month = int(u)}
	}
	if v, got := capnp_dyn.field_get(fields, "year"); got {
		// Int16 in the schema, so it reads back as a signed Int value.
		if s, is_s := v.(i64); is_s {year = int(s)}
	}
	return d.make_date(u8(day), u8(month), u16(year))
}
