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

#pragma once

#include <map>
#include <string>
#include <vector>
#include <memory>

#include "json11/json11.hpp"

//#include "model/monica/monica_state.capnp.h"

#include "tools/date.h"
#include "json11/json11-helper.h"
#include "monica-parameters.h"

namespace monica {

class Crop : public Tools::Json11Serializable {
public:
  Crop() : _perennialCropParams(_cropParams) {}

  Crop(const Crop& other);

  explicit Crop(mas::schema::model::monica::CropState::Reader reader) : _perennialCropParams(_cropParams) { deserialize(reader); }
  void deserialize(mas::schema::model::monica::CropState::Reader reader);
  void serialize(mas::schema::model::monica::CropState::Builder builder) const;

  explicit Crop(json11::Json object);

  Tools::Errors merge(json11::Json j) override;

  json11::Json to_json() const override { return to_json(true); }

  json11::Json to_json(bool includeFullCropParameters) const;

  std::string id() const { return speciesName() + "/" + cultivarName(); }

  std::string speciesName() const { return _speciesName; }

  std::string cultivarName() const { return _cultivarName; }

  bool isValid() const { return _isValid; }

  const CropParameters& cropParameters() const { return _cropParams; }

  CropParameters& cropParameters() { return _cropParams; }

  void setCropParameters(CropParameters&& cps) { _cropParams = cps; }

  bool separatePerennialCropParameters() const { return _separatePerennialCropParams; }

  const CropParameters& perennialCropParameters() const { return _perennialCropParams; }

  void setPerennialCropParameters(CropParameters&& cps) { _perennialCropParams = cps; }

  const CropResidueParameters& residueParameters() const { return _residueParams; }

  void setResidueParameters(CropResidueParameters&& rps) { _residueParams = rps; }

  Tools::Date seedDate() const { return _seedDate; }

  void setSeedDate(Tools::Date sd){ _seedDate = sd; }

  Tools::Date harvestDate() const { return _harvestDate; }

  void setHarvestDate(Tools::Date hd){ _harvestDate = hd; }

  bool isWinterCrop() const;

  void setIsWinterCrop(bool isWC = true) { _isWinterCrop = isWC; }

  bool isPerennialCrop() const;

  void setIsPerennialCrop(bool isPC = true) { _isPerennialCrop = isPC; }

  std::vector<Tools::Date> getCuttingDates() const { return _cuttingDates; }

  void setSeedAndHarvestDate(const Tools::Date& sd, const Tools::Date& hd) {
    setSeedDate(sd);
    setHarvestDate(hd);
    
  }

  void addCuttingDate(const Tools::Date cd) { _cuttingDates.push_back(cd); }

  std::string toString(bool detailed = false) const;

  // Automatic yield trigger parameters
  bool useAutomaticHarvestTrigger() { return _automaticHarvest; }
  void activateAutomaticHarvestTrigger(AutomaticHarvestParameters params) {
    _automaticHarvest = true;
    _automaticHarvestParams = params;
  }
  AutomaticHarvestParameters getAutomaticHarvestParams() { return _automaticHarvestParams; }

private:
  bool _isValid{ false };
  std::string _speciesName;
  std::string _cultivarName;
  Tools::Date _seedDate;
  Tools::Date _harvestDate;
  Tools::Maybe<bool> _isWinterCrop;
  Tools::Maybe<bool> _isPerennialCrop;
  std::vector<Tools::Date> _cuttingDates;
  CropParameters _cropParams;
  kj::Own<CropParameters> _separatePerennialCropParams;
  CropParameters& _perennialCropParams;
  CropResidueParameters _residueParams;

  double _crossCropAdaptionFactor{1.0};

  bool _automaticHarvest{false};
  AutomaticHarvestParameters _automaticHarvestParams;
};

typedef std::shared_ptr<Crop> CropPtr;

} // namespace monica
