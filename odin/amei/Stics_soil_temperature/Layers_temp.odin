package stics_soil_temperature

import "core:math"
import "core:slice"

// - Name: layers_temp -Version: 1.0, -Time step: 1
// - Description:
//             * Title: layers mean temperature model
//             * Authors: None
//             * Reference: doi:http://dx.doi.org/10.1016/j.agrformet.2014.05.002
//             * Institution: INRAE
//             * ExtendedDescription: None
//             * ShortDescription: None
Layers_Temp :: struct {
	layer_thick: [dynamic]int `unit:"cm"`, // layers thickness
}

make_layers_temp :: proc(allocator := context.allocator) -> (m: Layers_Temp) {
	context.allocator = allocator
	return
}

destroy_layers_temp :: proc(m: ^Layers_Temp) {
	delete(m.layer_thick)
}

// - inputs:
//             * name: temp_profile
//                           ** description : soil temperature profile
//                           ** inputtype : variable
//                           ** variablecategory : state
//                           ** datatype : DOUBLEARRAY
//                           ** len :
//                           ** max : 50.0
//                           ** min : -50.0
//                           ** default : 0.0
//                           ** unit : degC
//             * name: layer_thick
//                           ** description : layers thickness
//                           ** inputtype : parameter
//                           ** parametercategory : constant
//                           ** datatype : INTARRAY
//                           ** len :
//                           ** max :
//                           ** min :
//                           ** default :
//                           ** unit : cm
// - outputs:
//             * name: layer_temp
//                           ** description : soil layers temperature
//                           ** datatype : DOUBLEARRAY
//                           ** variablecategory : state
//                           ** len :
//                           ** max : 50.0
//                           ** min : -50.0
//                           ** unit : degC
layers_temp_calculate_model :: proc(m: ^Layers_Temp, s: ^Soil_Temp_State, s1: ^Soil_Temp_State, r: ^Soil_Temp_Rate, a: ^Soil_Temp_Auxiliary, ex: ^Soil_Temp_Exogenous, allocator := context.allocator) {
	context.allocator = allocator
	z: int
	layers_nb: int
	up_depth: [dynamic]int
	defer delete(up_depth)
	layer_depth: [dynamic]int
	defer delete(layer_depth)
	depth_value: int
	layers_nb = get_layers_number(m.layer_thick[:])
	resize(&s.layer_temp, layers_nb)
	resize(&up_depth, layers_nb + 1)
	resize(&layer_depth, layers_nb)
	resize(&up_depth, layers_nb + 1)
	slice.fill(up_depth[:], 0)
	{
		_r0 := layer_thickness2depth(m.layer_thick[:])
		resize(&layer_depth, len(_r0))
		copy(layer_depth[:], _r0)
		delete(_r0)
	}
	for z = 1; z < layers_nb + 1; z += 1 {
		depth_value = layer_depth[z - 1]
		up_depth[z + 1 - 1] = depth_value
	}
	for z = 1; z < layers_nb + 1; z += 1 {
		s.layer_temp[z - 1] = math.sum(s.temp_profile[up_depth[z - 1] + 1 - 1:up_depth[z + 1 - 1]]) / f64(m.layer_thick[z - 1])
	}
}

@(private="file")
get_layers_number :: proc(layer_thick_or_depth: []int) -> int {
	layers_number: int
	z: int
	layers_number = 0
	for z = 1; z < len(layer_thick_or_depth) + 1; z += 1 {
		if layer_thick_or_depth[z - 1] != 0 {
			layers_number = layers_number + 1
		}
	}
	return layers_number
}

@(private="file")
layer_thickness2depth :: proc(layer_thick: []int) -> []int {
	layer_depth: [dynamic]int
	layers_nb: int
	z: int
	layers_nb = len(layer_thick)
	resize(&layer_depth, layers_nb)
	resize(&layer_depth, layers_nb)
	slice.fill(layer_depth[:], 0)
	for z = 1; z < layers_nb + 1; z += 1 {
		if layer_thick[z - 1] != 0 {
			layer_depth[z - 1] = math.sum(layer_thick[1 - 1:z])
		}
	}
	return layer_depth[:]
}