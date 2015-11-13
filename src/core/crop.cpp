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

Crop::Crop(CropId id,
           const std::string& species,
           const CropParametersPtr cps,
           const OrganicMatterParametersPtr rps,
           double crossCropAdaptionFactor)
  : _id(id),
    _speciesName(species),
    _cropParams(cps),
    _residueParams(rps),
    _crossCropAdaptionFactor(crossCropAdaptionFactor)
{}

Crop::Crop(CropId id,
           const std::string& species,
           const Tools::Date& seedDate,
           const Tools::Date& harvestDate,
           const CropParametersPtr cps,
           const OrganicMatterParametersPtr rps,
           double crossCropAdaptionFactor)
  : _id(id),
    _speciesName(species),
    _seedDate(seedDate),
    _harvestDate(harvestDate),
    _cropParams(cps),
    _residueParams(rps),
    _crossCropAdaptionFactor(crossCropAdaptionFactor)
{}

Crop::Crop(json11::Json j)
  : _seedDate(Tools::Date::fromIsoDateString(string_value(j, "seedDate")))
  , _harvestDate(Tools::Date::fromIsoDateString(string_value(j, "havestDate")))
{
  set_int_value(_id, j, "id");
  set_string_value(_speciesName, j, "species");
  set_string_value(_cultivarName, j, "cultivar");

	string err;
  if(j.has_shape({{"cropParams", json11::Json::OBJECT}}, err))
  {
    auto jcps = j["cropParams"];
    //load all crop data from json object
    if(jcps.has_shape({{"species", json11::Json::OBJECT}}, err) &&
       jcps.has_shape({{"cultivar", json11::Json::OBJECT}}, err))
      _cropParams = make_shared<CropParameters>(j["cropParams"]);
    //load data from database
    else
      _cropParams = getCropParametersFromMonicaDB(_id);
  }

  if(j.has_shape({{"perennialCropParams", json11::Json::OBJECT}}, err))
  {
    auto jcps = j["perennialCropParams"];
    //load all crop data from json object
    if(jcps.has_shape({{"species", json11::Json::OBJECT}}, err) &&
       jcps.has_shape({{"cultivar", json11::Json::OBJECT}}, err))
      _perennialCropParams = make_shared<CropParameters>(j["cropParams"]);
    //load data from database
    else
      _perennialCropParams = getCropParametersFromMonicaDB(_id);
  }

  //load all crop data from json object
  if(j.has_shape({{"residueParams", json11::Json::OBJECT}}, err))
    _residueParams = make_shared<OrganicMatterParameters>(j["residueParams"]);
  //load data from database
  else
    _residueParams = getResidueParametersFromMonicaDB(_id);

  if(j.has_shape({{"cuttingDates", json11::Json::ARRAY}}, err))
    for(auto cd : j["cuttingDates"].array_items())
      _cuttingDates.push_back(Tools::Date::fromIsoDateString(cd.string_value()));
}

json11::Json Crop::to_json(bool includeFullCropParameters) const
{
  J11Array cds;
  for(auto cd : _cuttingDates)
    cds.push_back(cd.toIsoDateString());

  J11Object o{
    {"type", "Crop"},
    {"id", _id},
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
  s << "id: " << id() << " name: " << speciesName() << " seedDate: " << seedDate().toString() << " harvestDate: "
      << harvestDate().toString();
  if (detailed)
  {
    s << endl << "CropParameters: " << endl << cropParameters()->toString() << endl << "ResidueParameters: " << endl
        << residueParameters()->toString() << endl;
  }
  return s.str();
}

//------------------------------------------------------------------------------

