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

#ifndef BUILD_OUTPUT_H_
#define BUILD_OUTPUT_H_

#include <string>

#include "json11/json11.hpp"

#include "common/dll-exports.h"
#include "tools/json11-helper.h"
#include "climate/climate-common.h"
#include "tools/date.h"
#include "../core/monica-model.h"
#include "output.h"


namespace Monica
{
	struct OutputMetadata
	{
		int id;
		std::string name;
		std::string unit;
		std::string description;
	};

	//---------------------------------------------------------------------------

	double applyOIdOP(OId::OP op, const std::vector<double>& vs);

	json11::Json applyOIdOP(OId::OP op, const std::vector<json11::Json>& js);

	DLL_API std::vector<OId> parseOutputIds(const Tools::J11Array& oidArray);

	struct DLL_API BOTRes
	{
		std::map<int, std::function<json11::Json(const MonicaModel&, OId)>> ofs;
		std::map<std::string, OutputMetadata> name2metadata;
	};
	DLL_API BOTRes& buildOutputTable();
}  
#endif 
