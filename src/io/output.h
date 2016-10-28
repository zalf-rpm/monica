/* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
Authors: 
Claas Nendel <claas.nendel@zalf.de>
Xenia Specka <xenia.specka@zalf.de>
Michael Berg <michael.berg@zalf.de>

Maintainers: 
Currently maintained by the authors.

This file is part of the MONICA model. 
Copyright (C) Leibniz Centre for Agricultural Landscape Research (ZALF)
*/

#ifndef OUTPUT_H_
#define OUTPUT_H_

#include <string>

#include "json11/json11.hpp"

#include "common/dll-exports.h"
#include "tools/json11-helper.h"
#include "climate/climate-common.h"
#include "tools/date.h"
#include "../core/monica-model.h"


namespace Monica
{
	struct DLL_API OId : public Tools::Json11Serializable
	{
		enum OP { AVG, MEDIAN, SUM, MIN, MAX, FIRST, LAST, NONE, _UNDEFINED_OP_ };

		enum ORGAN { ROOT = 0, LEAF, SHOOT, FRUIT, STRUCT, SUGAR, _UNDEFINED_ORGAN_ };

		OId() {}

		//! just name 
		OId(int id)
			: id(id)
		{}

		//! id and organ
		OId(int id, ORGAN organ)
			: id(id), organ(organ)
		{}

		//! id and layer aggregation
		OId(int id, OP layerAgg)
			: id(id), layerAggOp(layerAgg), fromLayer(0), toLayer(20)
		{}

		//! id, layer aggregation and time aggregation, shortcut for aggregating all layers in non daily setting
		OId(int id, OP layerAgg, OP timeAgg)
			: id(id), layerAggOp(layerAgg), timeAggOp(timeAgg), fromLayer(0), toLayer(20)
		{}

		//! id, layer aggregation of from to (incl) to layers
		OId(int id, int from, int to, OP layerAgg)
			: id(id), layerAggOp(layerAgg), fromLayer(from), toLayer(to)
		{}

		//! aggregate layers from to (incl) to in a non daily setting
		OId(int id, int from, int to, OP layerAgg, OP timeAgg)
			: id(id), layerAggOp(layerAgg), timeAggOp(timeAgg), fromLayer(from), toLayer(to)
		{}

		OId(json11::Json object);

		virtual Tools::Errors merge(json11::Json j);

		virtual json11::Json to_json() const;

		bool isRange() const { return fromLayer >= 0 && toLayer >= 0; }// && fromLayer < toLayer; }

		bool isOrgan() const { return organ != _UNDEFINED_ORGAN_; }

		virtual std::string toString(bool includeTimeAgg = false) const;

		std::string toString(OId::OP op) const;
		std::string toString(OId::ORGAN organ) const;

		int id{-1};
		std::string name;
		std::string unit;
		std::string jsonInput;
		OP layerAggOp{NONE}; //! aggregate values on potentially daily basis (e.g. soil layers)
		OP timeAggOp{AVG}; //! aggregate values in a second time range (e.g. monthly)
		ORGAN organ{_UNDEFINED_ORGAN_};
		int fromLayer{-1}, toLayer{-1};
	};

	//---------------------------------------------------------------------------

	struct DLL_API Output : public Tools::Json11Serializable
	{
		Output() {}

		Output(json11::Json object);

		virtual Tools::Errors merge(json11::Json j);

		virtual json11::Json to_json() const;
		
		std::string customId;

		struct Data
		{
			std::string origSpec;
			std::vector<OId> outputIds;
			std::vector<Tools::J11Array> results;
		};

		std::vector<Data> data;
	};
}  

#endif 
