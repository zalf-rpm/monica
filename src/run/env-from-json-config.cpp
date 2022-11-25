/* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
Authors:
Michael Berg <michael.berg@zalf.de>

Maintainers:
Currently maintained by the authors.

This file is part of the MONICA model.
Copyright (C) Leibniz Centre for Agricultural Landscape Research (ZALF)
*/

#include <iostream>
#include <fstream>
#include <string>
#include <set>

#include "env-from-json-config.h"
#include "tools/debug.h"
#include "../run/run-monica.h"
#include "json11/json11-helper.h"
#include "tools/helper.h"
#include "climate/climate-file-io.h"
#include "soil/conversion.h"
#include "soil/soil-from-db.h"
#include "../io/output.h"

using namespace std;
using namespace monica;
using namespace json11;
using namespace Tools;
using namespace Climate;

Env monica::createEnvFromJsonConfigFiles(std::map<std::string, std::string> params)
{
	Env env;
	if(!printPossibleErrors(env.merge(createEnvJsonFromJsonStrings(params)), activateDebug))
		return Env();
	return env;
}

//-----------------------------------------------------------------------------

