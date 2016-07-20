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

#ifndef CSV_FORMAT_H_
#define CSV_FORMAT_H_

#include <string>
#include <iostream>

#include "json11/json11.hpp"
#include "tools/json11-helper.h"
#include "../run/run-monica.h"

namespace Monica
{
	void writeOutputHeaderRows(std::ostream& out,
														 const std::vector<OId>& outputIds,
														 std::string csvSep,
														 bool includeHeaderRow,
														 bool includeUnitsRow,
														 bool includeTimeAgg = true);

	void writeOutput(std::ostream& out,
									 const std::vector<OId>& outputIds,
									 const std::vector<Tools::J11Array>& values,
									 std::string csvSep,
									 bool includeHeaderRow,
									 bool includeUnitsRow);
}  

#endif 
