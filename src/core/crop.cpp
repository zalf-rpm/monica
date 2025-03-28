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

#include "crop.h"

#include <map>
#include <sstream>
#include <iostream>
#include <fstream>
#include <cmath>
#include <utility>
#include <mutex>

#include "tools/helper.h"
#include "json11/json11-helper.h"
#include "tools/algorithms.h"
#include "tools/debug.h"

using namespace std;
using namespace monica;
using namespace Tools;

//----------------------------------------------------------------------------

Crop::Crop(const Crop& other)
  : _isValid(other._isValid)
  , _speciesName(other._speciesName)
  , _cultivarName(other._cultivarName)
  , _seedDate(other._seedDate)
  , _harvestDate(other._harvestDate)
  , _isWinterCrop(other._isWinterCrop)
  , _isPerennialCrop(other._isPerennialCrop)
  , _cuttingDates(other._cuttingDates)
  , _cropParams(other._cropParams)
  , _separatePerennialCropParams(other._separatePerennialCropParams ? kj::heap<CropParameters>(other._perennialCropParams) : kj::Own<CropParameters>())
  , _perennialCropParams(other._separatePerennialCropParams ? *_separatePerennialCropParams.get() : _cropParams)
  , _residueParams(other._residueParams)
  , _crossCropAdaptionFactor(other._crossCropAdaptionFactor)
  , _automaticHarvest(other._automaticHarvest)
  , _automaticHarvestParams(other._automaticHarvestParams)
{}

//Crop::Crop(const std::string& species,
//           const string& cultivarName,
//           const CropParametersPtr cps,
//           const CropResidueParametersPtr rps,
//           double crossCropAdaptionFactor)
//  : _speciesName(species)
//  , _cultivarName(cultivarName)
//  , _cropParams(cps)
//	, _perennialCropParams(_cropParams)
//  , _residueParams(rps)
//  , _crossCropAdaptionFactor(crossCropAdaptionFactor)
//{}

//Crop::Crop(const string& speciesName,
//           const string& cultivarName,
//           const Tools::Date& seedDate,
//           const Tools::Date& harvestDate,
//           const CropParametersPtr cps,
//           const CropResidueParametersPtr rps,
//           double crossCropAdaptionFactor)
//  : _speciesName(speciesName)
//  , _cultivarName(cultivarName)
//  , _seedDate(seedDate)
//  , _harvestDate(harvestDate)
//  , _cropParams(cps)
//	, _perennialCropParams(_cropParams)
//  , _residueParams(rps)
//  , _crossCropAdaptionFactor(crossCropAdaptionFactor)
//{}

Crop::Crop(json11::Json j)
	: _perennialCropParams(_cropParams)
{
	merge(j);
}

void Crop::deserialize(mas::schema::model::monica::CropState::Reader reader) {
	_speciesName = reader.getSpeciesName();
	_cultivarName = reader.getCultivarName();
	_seedDate.deserialize(reader.getSeedDate());
	_harvestDate.deserialize(reader.getHarvestDate());
	if (reader.hasIsWinterCrop()) _isWinterCrop.setValue(reader.getIsWinterCrop().getValue());
	if (reader.hasIsPerennialCrop()) _isPerennialCrop.setValue(reader.getIsPerennialCrop().getValue());
	setFromComplexCapnpList(_cuttingDates, reader.getCuttingDates());
	if (!reader.hasCropParams()) _isValid = false;
	else _cropParams.deserialize(reader.getCropParams());
	if (reader.hasPerennialCropParams()) {
		_separatePerennialCropParams = kj::heap<CropParameters>(reader.getPerennialCropParams());
		_perennialCropParams = *_separatePerennialCropParams.get();
	}
	_residueParams.deserialize(reader.getResidueParams());
	_crossCropAdaptionFactor = reader.getCrossCropAdaptionFactor();
	_automaticHarvest = reader.getAutomaticHarvest();
	_automaticHarvestParams.deserialize(reader.getAutomaticHarvestParams());
}

void Crop::serialize(mas::schema::model::monica::CropState::Builder builder) const {
	builder.setSpeciesName(_speciesName);
  builder.setCultivarName(_cultivarName);
  _seedDate.serialize(builder.initSeedDate());
	_harvestDate.serialize(builder.initHarvestDate());
  if (_isWinterCrop.isValue()) builder.initIsWinterCrop().setValue(_isWinterCrop.value());
  if (_isPerennialCrop.isValue()) builder.initIsPerennialCrop().setValue(_isPerennialCrop.value());
  setComplexCapnpList(_cuttingDates, builder.initCuttingDates((capnp::uint)_cuttingDates.size()));
  if (isValid()) _cropParams.serialize(builder.initCropParams());
	if (_separatePerennialCropParams) _separatePerennialCropParams->serialize(builder.initPerennialCropParams());
  _residueParams.serialize(builder.initResidueParams());
	builder.setCrossCropAdaptionFactor(_crossCropAdaptionFactor);
	builder.setAutomaticHarvest(_automaticHarvest);
	_automaticHarvestParams.serialize(builder.initAutomaticHarvestParams());
}

Errors Crop::merge(json11::Json j)
{
	Errors res = Json11Serializable::merge(j);

	set_iso_date_value(_seedDate, j, "seedDate");
	set_iso_date_value(_harvestDate, j, "harvestDate");
	set_string_value(_speciesName, j, "species");
	set_string_value(_cultivarName, j, "cultivar");

	if(j["is-winter-crop"].is_bool())
		_isWinterCrop.setValue(j["is-winter-crop"].bool_value());

	if(j["is-perennial-crop"].is_bool())
		_isPerennialCrop.setValue(j["is-perennial-crop"].bool_value());

	string err;
	if(j.has_shape({{"cropParams", json11::Json::OBJECT}}, err))
	{
		auto jcps = j["cropParams"];
		if(jcps.has_shape({{"species", json11::Json::OBJECT}}, err)
			 && jcps.has_shape({{"cultivar", json11::Json::OBJECT}}, err))
			_cropParams.merge(j["cropParams"]);
		else
			res.errors.push_back(string("Couldn't find 'species' or 'cultivar' key in JSON object 'cropParams':\n") + j.dump());

		if(_speciesName.empty())
			_speciesName = _cropParams.speciesParams.pc_SpeciesId;
		if(_cultivarName.empty())
			_cultivarName = _cropParams.cultivarParams.pc_CultivarId;

		if(_isPerennialCrop.isValue())
			_cropParams.cultivarParams.pc_Perennial = _isPerennialCrop.value();
		else
			_isPerennialCrop.setValue(_cropParams.cultivarParams.pc_Perennial);

		_isValid = true;
	}
	else {
		res.errors.push_back(string("Couldn't find 'cropParams' key in JSON object:\n") + j.dump());
		_isValid = false;
	}

	if(_isPerennialCrop.isValue() && _isPerennialCrop.value())
	{
		err = "";
		if(j.has_shape({{"perennialCropParams", json11::Json::OBJECT}}, err))
		{
			auto jcps = j["perennialCropParams"];
			if (jcps.has_shape({ {"species", json11::Json::OBJECT} }, err)
				&& jcps.has_shape({ {"cultivar", json11::Json::OBJECT} }, err)) {
				_separatePerennialCropParams = kj::heap<CropParameters>();
				_separatePerennialCropParams->merge(j["cropParams"]);
				_perennialCropParams = *_separatePerennialCropParams.get();
			}
		}
	}

	err = "";
	if (j.has_shape({ {"residueParams", json11::Json::OBJECT} }, err)) {
		_residueParams.merge(j["residueParams"]);
	} else {
		res.errors.push_back(string("Couldn't find 'residueParams' key in JSON object:\n") + j.dump());
		_isValid = false;
	}

	err = "";
	if(j.has_shape({{"cuttingDates", json11::Json::ARRAY}}, err))
	{
		_cuttingDates.clear();
		for(auto cd : j["cuttingDates"].array_items())
			_cuttingDates.push_back(Tools::Date::fromIsoDateString(cd.string_value()));
	}

	return res;
}


json11::Json Crop::to_json(bool includeFullCropParameters) const
{
  J11Array cds;
  for(auto cd : _cuttingDates)
    cds.push_back(cd.toIsoDateString());

  J11Object o
	{{"type", "Crop"}
  ,{"species", _speciesName}
  ,{"cultivar", _cultivarName}
  ,{"seedDate", _seedDate.toIsoDateString()}
  ,{"harvestDate", _harvestDate.toIsoDateString()}
	,{"cuttingDates", cds}
  ,{"automaticHarvest", _automaticHarvest}
  ,{"AutomaticHarvestParams", _automaticHarvestParams}};

	if(_isWinterCrop.isValue())
		o["is-winter-crop"] = _isWinterCrop.value();

  if(includeFullCropParameters)
  {
    if(_isValid)
      o["cropParams"] = cropParameters().to_json();
    if(_separatePerennialCropParams)
      o["perennialCropParams"] = perennialCropParameters().to_json();
    if(_isValid)
      o["residueParams"] = residueParameters().to_json();
  }

  return o;
}

bool Crop::isWinterCrop() const
{
	if(_isWinterCrop.isValue())
		return _isWinterCrop.value();
	return cropParameters().cultivarParams.winterCrop;
	//else if(seedDate().isValid() && harvestDate().isValid())
	//	return seedDate().dayOfYear() > harvestDate().dayOfYear();
	//return false;
}

bool Crop::isPerennialCrop() const
{
	if(_isPerennialCrop.isValue())
		return _isPerennialCrop.value();

	return false;
}

string Crop::toString(bool detailed) const
{
  ostringstream s;
  s << " species/cultivar: " << speciesName() << "/" << cultivarName() 
		<< " seedDate: " << seedDate().toString() 
		<< " harvestDate: "	<< harvestDate().toString();
	if(detailed)
    s << endl << "CropParameters: " << endl << cropParameters().toString()
      << endl << "ResidueParameters: " << endl << residueParameters().toString() << endl;
  
  return s.str();
}

//------------------------------------------------------------------------------

