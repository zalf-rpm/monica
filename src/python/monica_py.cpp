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
#include "../run/env-from-json-config.h"
#include "../run/run-monica.h"
#include "tools/date.h"
#include "tools/algorithms.h"
#include "tools/debug.h"
#include "tools/json11-helper.h"
#include "../io/output.h"
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
		
	auto res = Monica::runMonica(env);
	
	map<string, Json> m;
	addOutputToResultMessage(res.out, m);

	dict d;
	for(auto& p : m)
		d[p.first] = p.second.dump();

	return d;
}

string parseOutputIdsToJsonString(string oidArrayString)
{
	string err;
	J11Array oidArray = Json::parse(oidArrayString, err).array_items();
	return Json(parseOutputIds(oidArray)).dump();
}

string readClimateDataFromCSVStringViaHeadersToJsonString(string climateCSVString,
																													string optionsJsonString)
{
	string err;
	J11Object options = Json::parse(optionsJsonString, err).object_items();
	CSVViaHeaderOptions os;
	os.separator = options["separator"].string_value();
	os.startDate = Date::fromIsoDateString(options["startDate"].string_value());
	os.endDate = Date::fromIsoDateString(options["endDate"].string_value());
	os.noOfHeaderLines = options["noOfHeaderLines"].int_value();
	
	map<string, string> hn2acdn;
	for(auto p : options["headerName2ACDName"].object_items())
		os.headerName2ACDName[p.first] = p.second.string_value();

	//os.separator = extract<string>(options["separator"]);
	//os.startDate = Date::fromIsoDateString(extract<string>(options["startDate"]));
	//os.endDate = Date::fromIsoDateString(extract<string>(options["endDate"]));
	//os.noOfHeaderLines = extract<int>(options["noOfHeaderLines"]);
	//stl_input_iterator<string> begin(options.keys()), end;
	//map<string, string> hn2acdn;
	//for_each(begin, end,
	//				 [&](string key){ hn2acdn[key] = extract<string>(options[key]); });
	//os.headerName2ACDName = hn2acdn;

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
