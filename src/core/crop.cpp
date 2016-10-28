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

#include <map>
#include <sstream>
#include <iostream>
#include <fstream>
#include <cmath>
#include <utility>
#include <mutex>

#include "tools/helper.h"
#include "tools/json11-helper.h"
#include "tools/algorithms.h"
#include "../core/monica-parameters.h"
#include "tools/debug.h"
#include "../io/database-io.h"

#include "crop.h"

using namespace std;
using namespace Monica;
using namespace Tools;

//----------------------------------------------------------------------------

Crop::Crop(const std::string& speciesName)
  : _speciesName(speciesName)
{}

Crop::Crop(const std::string& species,
           const string& cultivarName,
           const CropParametersPtr cps,
           const CropResidueParametersPtr rps,
           double crossCropAdaptionFactor)
  : _speciesName(species)
  , _cultivarName(cultivarName)
  , _cropParams(cps)
  , _residueParams(rps)
  , _crossCropAdaptionFactor(crossCropAdaptionFactor)
{}

Crop::Crop(const string& speciesName,
           const string& cultivarName,
           const Tools::Date& seedDate,
           const Tools::Date& harvestDate,
           const CropParametersPtr cps,
           const CropResidueParametersPtr rps,
           double crossCropAdaptionFactor)
  : _speciesName(speciesName)
  , _cultivarName(cultivarName)
  , _seedDate(seedDate)
  , _harvestDate(harvestDate)
  , _cropParams(cps)
  , _residueParams(rps)
  , _crossCropAdaptionFactor(crossCropAdaptionFactor)
{}

Crop::Crop(json11::Json j)
{
	merge(j);
}

Errors Crop::merge(json11::Json j)
{
	Errors res;

	set_iso_date_value(_seedDate, j, "seedDate");
	set_iso_date_value(_harvestDate, j, "havestDate");
	set_int_value(_dbId, j, "id");
	set_string_value(_speciesName, j, "species");
	set_string_value(_cultivarName, j, "cultivar");

	string err;
	if(j.has_shape({{"cropParams", json11::Json::OBJECT}}, err))
	{
		auto jcps = j["cropParams"];
		if(jcps.has_shape({{"species", json11::Json::OBJECT}}, err)
			 && jcps.has_shape({{"cultivar", json11::Json::OBJECT}}, err))
			_cropParams = make_shared<CropParameters>(j["cropParams"]);
		else
			res.errors.push_back(string("Couldn't find 'species' or 'cultivar' key in JSON object 'cropParams':\n") + j.dump());

		if(_speciesName.empty() && _cropParams)
			_speciesName = _cropParams->speciesParams.pc_SpeciesId;
		if(_cultivarName.empty() && _cropParams)
			_cultivarName = _cropParams->cultivarParams.pc_CultivarId;
	}
	else
		res.errors.push_back(string("Couldn't find 'cropParams' key in JSON object:\n") + j.dump());

	err = "";
	if(j.has_shape({{"perennialCropParams", json11::Json::OBJECT}}, err))
	{
		auto jcps = j["perennialCropParams"];
		if(jcps.has_shape({{"species", json11::Json::OBJECT}}, err)
			 && jcps.has_shape({{"cultivar", json11::Json::OBJECT}}, err))
			_perennialCropParams = make_shared<CropParameters>(j["cropParams"]);
	}

	err = "";
	if(j.has_shape({{"residueParams", json11::Json::OBJECT}}, err))
		_residueParams = make_shared<CropResidueParameters>(j["residueParams"]);
	else
		res.errors.push_back(string("Couldn't find 'residueParams' key in JSON object:\n") + j.dump());

	err = "";
	if(j.has_shape({{"cuttingDates", json11::Json::ARRAY}}, err))
		for(auto cd : j["cuttingDates"].array_items())
			_cuttingDates.push_back(Tools::Date::fromIsoDateString(cd.string_value()));

	return res;
}


json11::Json Crop::to_json(bool includeFullCropParameters) const
{
  J11Array cds;
  for(auto cd : _cuttingDates)
    cds.push_back(cd.toIsoDateString());

  J11Object o{
    {"type", "Crop"},
    {"id", _dbId},
    {"species", _speciesName},
    {"cultivar", _cultivarName},
    {"seedDate", _seedDate.toIsoDateString()},
    {"harvestDate", _harvestDate.toIsoDateString()},
    {"cuttingDates", cds},
    {"automaticHarvest", _automaticHarvest},
    {"AutomaticHarvestParams", _automaticHarvestParams}};

  if(includeFullCropParameters)
  {
    if(_cropParams)
      o["cropParams"] = cropParameters()->to_json();
    if(_perennialCropParams)
      o["perennialCropParams"] = perennialCropParameters()->to_json();
    if(_residueParams)
      o["residueParams"] = residueParameters()->to_json();
  }

  return o;
}

string Crop::toString(bool detailed) const
{
  ostringstream s;
  s << "id: " << dbId()
		<< " species/cultivar: " << speciesName() << "/" << cultivarName() 
		<< " seedDate: " << seedDate().toString() 
		<< " harvestDate: "	<< harvestDate().toString();
	if(detailed)
    s << endl << "CropParameters: " << endl << cropParameters()->toString()
      << endl << "ResidueParameters: " << endl << residueParameters()->toString() << endl;
  
  return s.str();
}

//------------------------------------------------------------------------------

