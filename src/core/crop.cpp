/**
Authors: 
Dr. Claas Nendel <claas.nendel@zalf.de>
Xenia Specka <xenia.specka@zalf.de>
Michael Berg <michael.berg@zalf.de>

Maintainers: 
Currently maintained by the authors.

This file is part of the MONICA model. 
Copyright (C) 2007-2013, Leibniz Centre for Agricultural Landscape Research (ZALF)

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
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

Crop::Crop(const std::string& name)
  : _name(name)
{}

Crop::Crop(CropId id,
           const std::string& name,
           const CropParameters* cps,
           const OrganicMatterParameters* rps,
           double crossCropAdaptionFactor)
  : _id(id),
    _name(name),
    _cropParams(cps),
    _residueParams(rps),
    _crossCropAdaptionFactor(crossCropAdaptionFactor)
{}

Crop::Crop(CropId id,
           const std::string& name,
           const Tools::Date& seedDate,
           const Tools::Date& harvestDate,
           const CropParameters* cps,
           const OrganicMatterParameters* rps,
           double crossCropAdaptionFactor)
  : _id(id),
    _name(name),
    _seedDate(seedDate),
    _harvestDate(harvestDate),
    _cropParams(cps),
    _residueParams(rps),
    _crossCropAdaptionFactor(crossCropAdaptionFactor)
{}

Crop::Crop(json11::Json j)
  : _id(int_value(j, "id")),
    _name(string_value(j, "name")),
    _seedDate(Tools::Date::fromIsoDateString(string_value(j, "seedDate"))),
    _harvestDate(Tools::Date::fromIsoDateString(string_value(j, "havestDate")))
{
  if(j.has_shape({{"cropParams", json11::Json::OBJECT}}, string()))
    _cropParamsPtr = make_shared<CropParameters>(j["cropParams"]);
  if(j.has_shape({{"perennialCropParams", json11::Json::OBJECT}}, string()))
    _perennialCropParamsPtr = make_shared<CropParameters>(j["perennialCropParams"]);
  if(j.has_shape({{"residueParams", json11::Json::OBJECT}}, string()))
    _residueParamsPtr = make_shared<OrganicMatterParameters>(j["residueParams"]);

//  if(_id > -1)
//  {
//    _cropParams = getCropParametersFromMonicaDB(_id);
//    _residueParams = getResidueParametersFromMonicaDB(_id);
//  }

  if(j.has_shape({{"cuttingDates", json11::Json::ARRAY}}, string()))
    for(auto cd : j["cuttingDates"].array_items())
      _cuttingDates.push_back(Tools::Date::fromIsoDateString(cd.string_value()));
}

json11::Json Crop::to_json(bool includeCropAndResidueParams) const
{
  J11Array cds;
  for(auto cd : _cuttingDates)
    cds.push_back(cd.toIsoDateString());

  J11Object o{
    {"type", "Crop"},
    {"id", _id},
    {"name", _name},
    {"seedDate", _seedDate.toIsoDateString()},
    {"harvestDate", _harvestDate.toIsoDateString()},
    {"cuttingDates", cds},
    {"automaticHarvest", _automaticHarvest},
    {"AutomaticHarvestParams", _automaticHarvestParams}};

  if(includeCropAndResidueParams)
  {
    if(_cropParams)
      o["cropParams"] = *cropParameters();
    if(_perennialCropParams)
      o["perennialCropParams"] = *perennialCropParameters();
    if(_residueParams)
      o["residueParams"] = *residueParameters();
  }

  return o;
}

string Crop::toString(bool detailed) const
{
  ostringstream s;
  s << "id: " << id() << " name: " << name() << " seedDate: " << seedDate().toString() << " harvestDate: "
      << harvestDate().toString();
  if (detailed)
  {
    s << endl << "CropParameters: " << endl << cropParameters()->toString() << endl << "ResidueParameters: " << endl
        << residueParameters()->toString() << endl;
  }
  return s.str();
}

//------------------------------------------------------------------------------

