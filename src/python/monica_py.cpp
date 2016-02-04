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
#include <boost/python.hpp>
#include <boost/python/stl_iterator.hpp>

#include "core/monica.h"
#include "run/env-from-json.h"
#include "tools/date.h"
#include "tools/algorithms.h"

using namespace boost::python;

using namespace std;
using namespace Monica;
using namespace Tools;

dict rm(map<string, string> params)
{
	auto env = Monica::createEnvFromJsonConfigFiles(params);

  auto res = Monica::runMonica(env);
	size_t ey = env.da.endDate().year();
	dict d;
	for (size_t i = 0, size = res.pvrs.size(); i < size; i++)
	{
		size_t year = ey - size + 1 + i;
		double yield = res.pvrs[i].results[primaryYieldTM] / 10.0;
		cout << "year: " << year << " yield: " << yield << " tTM" << endl;
		d[year] = Tools::round(yield, 3);
	}
  return d;
}

dict rm2(dict params)
{
	stl_input_iterator<string> begin(params.keys()), end;
	map<string, string> n2jos;
	for_each(begin, end,
	         [&](string key){ n2jos[key] = extract<string>(params[key]); });

	auto env = Monica::createEnvFromJsonConfigFiles(n2jos);

	auto res = Monica::runMonica(env);
	size_t ey = env.da.endDate().year();
	dict d;
	for (size_t i = 0, size = res.pvrs.size(); i < size; i++)
	{
		size_t year = ey - size + 1 + i;
		double yield = res.pvrs[i].results[primaryYieldTM] / 10.0;
		cout << "year: " << year << " yield: " << yield << " tTM" << endl;
		d[year] = Tools::round(yield, 3);
	}
	return d;
}


BOOST_PYTHON_MODULE(monica_py)
{
	/*
  class_<CarbiocialConfiguration>("CarbiocialConfiguration")
      .add_property("climateFile", &CarbiocialConfiguration::getClimateFile, &CarbiocialConfiguration::setClimateFile)
      .add_property("pathToIniFile", &CarbiocialConfiguration::getPathToIniFile, &CarbiocialConfiguration::setPathToIniFile)
      .add_property("inputPath", &CarbiocialConfiguration::getInputPath, &CarbiocialConfiguration::setInputPath)
      .add_property("outputPath", &CarbiocialConfiguration::getOutputPath, &CarbiocialConfiguration::setOutputPath)
      .add_property("startDate", &CarbiocialConfiguration::getStartDate, &CarbiocialConfiguration::setStartDate)
      .add_property("endDate", &CarbiocialConfiguration::getEndDate, &CarbiocialConfiguration::setEndDate)
      .add_property("rowId", &CarbiocialConfiguration::getRowId, &CarbiocialConfiguration::setRowId)
      .add_property("colId", &CarbiocialConfiguration::getColId, &CarbiocialConfiguration::setColId)
      .add_property("latitude", &CarbiocialConfiguration::getLatitude, &CarbiocialConfiguration::setLatitude)
      .add_property("elevation", &CarbiocialConfiguration::getElevation, &CarbiocialConfiguration::setElevation)
      .add_property("profileId", &CarbiocialConfiguration::getProfileId, &CarbiocialConfiguration::setProfileId)

      .def_readwrite("writeOutputFiles", &CarbiocialConfiguration::writeOutputFiles)
      .def_readwrite("create2014To2040ClimateData", &CarbiocialConfiguration::create2013To2040ClimateData)
      .def_readwrite("pathToClimateDataReorderingFile", &CarbiocialConfiguration::pathToClimateDataReorderingFile);
      */

  def("runMonica", rm);
	def("runMonica2", rm2);
}