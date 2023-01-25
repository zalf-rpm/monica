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

#include "env-from-json-config.h"

#include "tools/debug.h"
#include "tools/helper.h"
#include "env-json-from-json-config.h"

using namespace std;
using namespace monica;

Env monica::createEnvFromJsonConfigFiles(std::map<std::string, std::string> params) {
  Env env;
  if(!Tools::printPossibleErrors(env.merge(createEnvJsonFromJsonStrings(params)), Tools::activateDebug))
    return Env();
  return env;
}

