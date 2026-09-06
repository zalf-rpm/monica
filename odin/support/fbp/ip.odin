// Port of the FBP information-packet helpers from
// mas_cpp_misc/common/common.{h,cpp}: getIPAttr and copyAndSetIPAttrs.
//
// The C++ works on capnp readers/builders directly; here an IP is the shim's
// generic []Field, so "copy the attributes into the new IP" is just building the
// new attribute list rather than initAttributes()-ing a message. The behaviour
// is the same, including the in-place replacement of an existing attribute of
// the same name and the append when there is none.
package fbp

import capnp_dyn "../capnp/odin/capnp_dynamic"

// fbp.capnp: IP.Type
IP_STANDARD :: "standard"
IP_OPEN_BRACKET :: "openBracket"
IP_CLOSE_BRACKET :: "closeBracket"

// C++: kj::Maybe<capnp::AnyPointer::Reader> getIPAttr(IP::Reader ip, kj::StringPtr attrName)
//
// Returns the `value` of the attribute named attr_name. The C++ returns nullptr
// both when there are no attributes and when attrName is null; an empty
// attr_name is that second case.
ip_get_attr :: proc(ip: []capnp_dyn.Field, attr_name: string) -> (value: capnp_dyn.Value, ok: bool) {
	if attr_name == "" {
		return nil, false
	}
	attrs, has_attrs := ip_attributes(ip)
	if !has_attrs {
		return nil, false
	}
	for kv_value in attrs {
		kv, is_struct := kv_value.([]capnp_dyn.Field)
		if !is_struct {
			continue
		}
		key, has_key := capnp_dyn.field_get(kv, "key")
		if key_text, is_text := key.(string); has_key && is_text && key_text == attr_name {
			return capnp_dyn.field_get(kv, "value")
		}
	}
	return nil, false
}

// C++: ip.hasAttributes() ? ip.getAttributes() : nothing
ip_attributes :: proc(ip: []capnp_dyn.Field) -> (attrs: []capnp_dyn.Value, ok: bool) {
	v, has := capnp_dyn.field_get(ip, "attributes")
	if !has {
		return nil, false
	}
	list, is_list := v.([]capnp_dyn.Value)
	if !is_list || len(list) == 0 {
		return nil, false
	}
	return list, true
}

// C++: kj::Maybe<capnp::AnyPointer::Builder> copyAndSetIPAttrs(IP::Reader oldIp,
//        IP::Builder newIp, kj::StringPtr newAttrName)
//
// Copies old_ip's attributes and, if new_attr_name is non-empty, sets that one
// to new_value - replacing an existing attribute of the same name in place, or
// appending it otherwise. Returns the attribute list to put on the new IP, and
// whether there is one at all (the C++ returns nullptr and writes no attributes
// when there is nothing to copy and nothing to set).
//
// The C++ hands back a Builder for the caller to fill in afterwards
// (`builder->setAs<capnp::Text>(resJsonStr)`); passing the value in is the same
// thing one step earlier, and avoids handing out a pointer into a half-built
// message.
ip_copy_and_set_attrs :: proc(
	old_ip: []capnp_dyn.Field,
	new_attr_name: string,
	new_value: capnp_dyn.Value,
	allocator := context.allocator,
) -> (
	attrs: []capnp_dyn.Value,
	ok: bool,
) {
	old_attrs, has_old := ip_attributes(old_ip)
	// C++: if (!oldIp.hasAttributes() && newAttrName == nullptr) return nullptr;
	if !has_old && new_attr_name == "" {
		return nil, false
	}

	// C++: find the index to be replaced, if any.
	replace_at := -1
	if has_old && new_attr_name != "" {
		for kv_value, i in old_attrs {
			kv, is_struct := kv_value.([]capnp_dyn.Field)
			if !is_struct {
				continue
			}
			key, has_key := capnp_dyn.field_get(kv, "key")
			if key_text, is_text := key.(string); has_key && is_text && key_text == new_attr_name {
				replace_at = i
				break
			}
		}
	}

	n := len(old_attrs)
	appended := replace_at < 0 && new_attr_name != ""
	if appended {
		n += 1
	}

	out := make([]capnp_dyn.Value, n, allocator)
	for kv_value, i in old_attrs {
		out[i] = kv_value
	}
	if new_attr_name != "" {
		at := appended ? n - 1 : replace_at
		// Only key and value are set, matching the C++ - desc/valueType of a
		// replaced attribute are not carried over there either.
		kv := make([]capnp_dyn.Field, 2, allocator)
		kv[0] = {name = "key", value = new_attr_name}
		kv[1] = {name = "value", value = new_value}
		out[at] = kv
	}
	return out, true
}

// C++: inIp.getType() - fbp.capnp's IP.Type enum, defaulting to `standard` when
// the field is absent (which is what Cap'n Proto's own default gives a reader).
ip_type :: proc(ip: []capnp_dyn.Field) -> string {
	v, has := capnp_dyn.field_get(ip, "type")
	if !has {
		return IP_STANDARD
	}
	e, is_enum := v.(capnp_dyn.Enum_Value)
	if !is_enum || e.name == "" {
		return IP_STANDARD
	}
	return e.name
}
