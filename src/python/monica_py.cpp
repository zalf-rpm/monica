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

#include <string>
#include <algorithm>
#include <boost/python.hpp>
#include <boost/python/stl_iterator.hpp>
#include <boost/python/list.hpp>
#include <boost/python/dict.hpp>

#include "json11/json11.hpp"

#include "../core/monica.h"
#include "../run/env-from-json.h"
#include "../run/run-monica.h"
#include "tools/date.h"
#include "tools/algorithms.h"
#include "tools/debug.h"
#include "tools/json11-helper.h"

using namespace boost::python;

using namespace std;
using namespace Monica;
using namespace Tools;
using namespace json11;

dict rm(dict params)
{
	stl_input_iterator<string> begin(params.keys()), end;
	map<string, string> n2jos;
	for_each(begin, end,
	         [&](string key){ n2jos[key] = extract<string>(params[key]); });

	auto env = Monica::createEnvFromJsonConfigFiles(n2jos);
	activateDebug = env.debugMode;
		
	auto res = Monica::runMonica(env);
	
	map<string, Json> m;
	addOutputToResultMessage(res.out, m);

	dict d;
	for(auto& p : m)
		d[p.first] = p.second.dump();

	return d;
}

BOOST_PYTHON_MODULE(monica_python)
{
  def("runMonica", rm);
}
