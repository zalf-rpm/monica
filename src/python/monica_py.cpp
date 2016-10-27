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

#include "../core/monica-model.h"
#include "../run/env-from-json-config.h"
#include "../run/run-monica.h"
#include "tools/date.h"
#include "tools/algorithms.h"
#include "tools/debug.h"
#include "tools/json11-helper.h"
#include "../io/build-output.h"
#include "climate/climate-file-io.h"

using namespace boost::python;

using namespace std;
using namespace Monica;
using namespace Tools;
using namespace json11;
using namespace Climate;

dict rm(dict params)
{
	stl_input_iterator<string> begin(params.keys()), end;
	map<string, string> n2jos;
	for_each(begin, end,
	         [&](string key){ n2jos[key] = extract<string>(params[key]); });

	auto env = Monica::createEnvFromJsonConfigFiles(n2jos);
	activateDebug = env.debugMode;
		
	auto out = Monica::runMonica(env);
	
	dict d;
	for(auto& p : out.to_json().object_items())
		d[p.first] = p.second.dump();

	return d;
}

string parseOutputIdsToJsonString(string oidArrayString)
{
	string res;
	auto r = parseJsonString(oidArrayString);
	if(r.success())
		res = Json(parseOutputIds(r.result.array_items())).dump();
	return res;
}

string readClimateDataFromCSVStringViaHeadersToJsonString(string climateCSVString,
																													string optionsJsonString)
{
	string res;
	auto r = parseJsonString(optionsJsonString);
	CSVViaHeaderOptions os;
	if(r.success())
	{
		J11Object options = r.result.object_items();
		os.separator = options["csv-separator"].string_value();
		os.startDate = Date::fromIsoDateString(options["start-date"].string_value());
		os.endDate = Date::fromIsoDateString(options["end-date"].string_value());
		os.noOfHeaderLines = options["no-of-climate-file-header-line"].int_value();
				
		map<string, string> hn2acdn;
		for(auto p : options["header-to-acd-names"].object_items())
			os.headerName2ACDName[p.first] = p.second.string_value();
	}

	return Json(readClimateDataFromCSVStringViaHeaders(climateCSVString, os)).dump();
}

string test(string t)
{
	return t;
}

BOOST_PYTHON_MODULE(monica_python)
{
  def("runMonica", rm);
	def("parseOutputIdsToJsonString", parseOutputIdsToJsonString);
	def("readClimateDataFromCSVStringViaHeadersToJsonString", readClimateDataFromCSVStringViaHeadersToJsonString);
	def("test", test);
}
