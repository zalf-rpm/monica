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

#ifndef CROP_H_
#define CROP_H_

#include <map>
#include <string>
#include <vector>
#include <memory>

#include "json11/json11.hpp"

#include "tools/date.h"
#include "json11/json11-helper.h"
#include "../core/monica-parameters.h"

namespace Monica
{
  class Crop : public Tools::Json11Serializable
  {
	public:
    Crop(const std::string& speciesName = "fallow");

    Crop(const std::string& speciesName,
         const std::string& cultivarName,
         const CropParametersPtr cps = CropParametersPtr(),
         const CropResidueParametersPtr rps = CropResidueParametersPtr(),
         double crossCropAdaptionFactor = 1);

    Crop(const std::string& speciesName,
         const std::string& cultivarName,
         const Tools::Date& seedDate,
         const Tools::Date& harvestDate,
         const CropParametersPtr cps = CropParametersPtr(),
         const CropResidueParametersPtr rps = CropResidueParametersPtr(),
         double crossCropAdaptionFactor = 1);

    Crop(json11::Json object);

		virtual Tools::Errors merge(json11::Json j);

		virtual json11::Json to_json() const { return to_json(true); }

    json11::Json to_json(bool includeFullCropParameters) const;

    int dbId() const { return _dbId; }

    void setDbId(int dbId){ _dbId = dbId; }

    std::string id() const { return speciesName() + "/" + cultivarName(); }

    std::string speciesName() const { return _speciesName; }

    std::string cultivarName() const { return _cultivarName; }

    bool isValid() const { return cropParameters() && residueParameters(); }

    const CropParametersPtr cropParameters() const { return _cropParams; }

		CropParametersPtr cropParameters() { return _cropParams; }

    void setCropParameters(CropParametersPtr cps) { _cropParams = cps; }

    CropParametersPtr perennialCropParameters() const { return _perennialCropParams; }

    void setPerennialCropParameters(CropParametersPtr cps) { _perennialCropParams = cps; }

		CropResidueParametersPtr residueParameters() const { return _residueParams; }

    void setResidueParameters(CropResidueParametersPtr rps) { _residueParams = rps; }

		Tools::Date seedDate() const { return _seedDate; }

    void setSeedDate(Tools::Date sd){ _seedDate = sd; }

		Tools::Date harvestDate() const { return _harvestDate; }

    void setHarvestDate(Tools::Date hd){ _harvestDate = hd; }

		bool isWinterCrop() const;

		void setIsWinterCrop(bool isWC = true) { _isWinterCrop = isWC; }

		bool isPerennialCrop() const;

		void setIsPerennialCrop(bool isPC = true) { _isPerennialCrop = isPC; }

		std::vector<Tools::Date> getCuttingDates() const { return _cuttingDates; }

    void setSeedAndHarvestDate(const Tools::Date& sd, const Tools::Date& hd)
		{
      setSeedDate(sd);
      setHarvestDate(hd);
			
		}

		void addCuttingDate(const Tools::Date cd) { _cuttingDates.push_back(cd); }

		std::string toString(bool detailed = false) const;

		void setEva2TypeUsage(int type) { eva2_typeUsage = type; }
		int getEva2TypeUsage() const { return eva2_typeUsage; }
		
    // Automatic yield trigger parameters
		bool useAutomaticHarvestTrigger() { return _automaticHarvest; }
    void activateAutomaticHarvestTrigger(AutomaticHarvestParameters params)
    {
      _automaticHarvest = true;
      _automaticHarvestParams = params;
    }
		AutomaticHarvestParameters getAutomaticHarvestParams() { return _automaticHarvestParams; }

	private:
    int _dbId{-1};
    std::string _speciesName;
    std::string _cultivarName;
		Tools::Date _seedDate;
		Tools::Date _harvestDate;
		Tools::Maybe<bool> _isWinterCrop;
		Tools::Maybe<bool> _isPerennialCrop;
		std::vector<Tools::Date> _cuttingDates;
    CropParametersPtr _cropParams;
    CropParametersPtr _perennialCropParams;
    CropResidueParametersPtr _residueParams;

    double _crossCropAdaptionFactor{1.0};

    int eva2_typeUsage{NUTZUNG_UNDEFINED};

    bool _automaticHarvest{false};
		AutomaticHarvestParameters _automaticHarvestParams;
	};
	
  typedef std::shared_ptr<Crop> CropPtr;
}

#endif
