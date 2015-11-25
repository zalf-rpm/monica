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
#include "tools/json11-helper.h"
#include "../core/monica-parameters.h"

namespace Monica
{
  class Crop
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

    Crop(json11::Json j);

    json11::Json to_json(bool includeFullCropParameters = true) const;

    int dbId() const { return _dbId; }

    void setDbId(int dbId){ _dbId = dbId; }

    std::string id() const { return speciesName() + "/" + cultivarName(); }

    std::string speciesName() const { return _speciesName; }

    std::string cultivarName() const { return _cultivarName; }

    bool isValid() const { return cropParameters() && residueParameters(); }

    const CropParametersPtr cropParameters() const { return _cropParams; }

    void setCropParameters(CropParametersPtr cps) { _cropParams = cps; }

    CropParametersPtr perennialCropParameters() const { return _perennialCropParams; }

    void setPerennialCropParameters(CropParametersPtr cps) { _perennialCropParams = cps; }

		CropResidueParametersPtr residueParameters() const { return _residueParams; }

    void setResidueParameters(CropResidueParametersPtr rps) { _residueParams = rps; }

		Tools::Date seedDate() const { return _seedDate; }

    void setSeedDate(Tools::Date sd){ _seedDate = sd; }

		Tools::Date harvestDate() const { return _harvestDate; }

    void setHarvestDate(Tools::Date hd){ _harvestDate = hd; }

		std::vector<Tools::Date> getCuttingDates() const { return _cuttingDates; }

    void setSeedAndHarvestDate(const Tools::Date& sd, const Tools::Date& hd)
		{
      setSeedDate(sd);
      setHarvestDate(hd);
		}

		void addCuttingDate(const Tools::Date cd) { _cuttingDates.push_back(cd); }

		std::string toString(bool detailed = false) const;

		void setHarvestYields(double primaryYield, double secondaryYield)
		{
			_primaryYield += primaryYield;
			_secondaryYield += secondaryYield;
		}

		void setHarvestYieldsTM(double primaryYieldTM, double secondaryYieldTM)
		{
			_primaryYieldTM += primaryYieldTM;
			_secondaryYieldTM += secondaryYieldTM;
		}

		void setYieldNContent(double primaryYieldN, double secondaryYieldN)
		{
			_primaryYieldN += primaryYieldN;
			_secondaryYieldN += secondaryYieldN;
		}

		void addAppliedIrrigationWater(double amount) { _appliedAmountIrrigation += amount; }
		void setSumTotalNUptake(double sum) { _sumTotalNUptake = sum; }
		void setCropHeight(double height) {_cropHeight = height; }
		void setAccumulatedETa(double eta) { _accumulatedETa = eta; }
		void setAccumulatedTranspiration(double transp) { _accumulatedTranspiration = transp;  }

		double appliedIrrigationWater() const { return _appliedAmountIrrigation; }
		double sumTotalNUptake() const {return _sumTotalNUptake; }
		double primaryYield() const { return _primaryYield * _crossCropAdaptionFactor; }
		double aboveGroundBiomass() const { return _primaryYield * _crossCropAdaptionFactor + _secondaryYield * _crossCropAdaptionFactor; }
		double secondaryYield() const { return _secondaryYield * _crossCropAdaptionFactor; }
		double primaryYieldTM() const {  return _primaryYieldTM * _crossCropAdaptionFactor; }
		double secondaryYieldTM() const { return _secondaryYieldTM * _crossCropAdaptionFactor; }
		double primaryYieldN() const { return _primaryYieldN; }
		double aboveGroundBiomasseN() const { return _primaryYieldN + _secondaryYieldN; }
		double secondaryYieldN() const { return _secondaryYieldN; }
		double cropHeight() const { return _cropHeight; }

		void reset();

		void setEva2TypeUsage(int type) { eva2_typeUsage = type; }
		int getEva2TypeUsage() const { return eva2_typeUsage; }

		double get_AccumulatedETa() const {return _accumulatedETa;}
		double get_AccumulatedTranspiration() const { return _accumulatedTranspiration; }

		void setAnthesisDay(int day) { _anthesisDay = day; }
		int getAnthesisDay() const { return _anthesisDay; }

		void setMaturityDay(int day) { _maturityDay = day; }
		int getMaturityDay() const { return _maturityDay; }
		
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
		std::vector<Tools::Date> _cuttingDates;
    CropParametersPtr _cropParams;
    CropParametersPtr _perennialCropParams;
    CropResidueParametersPtr _residueParams;

    double _primaryYield{0.0};
    double _secondaryYield{0.};
    double _primaryYieldTM{0.0};
    double _secondaryYieldTM{0.0};
    double _appliedAmountIrrigation{0.0};
    double _primaryYieldN{0.0};
    double _secondaryYieldN{0.0};
    double _sumTotalNUptake{0.0};

    double _crossCropAdaptionFactor{1.0};
    double _cropHeight{0.0};
    double _accumulatedETa{0.0};
    double _accumulatedTranspiration{0.0};

    int eva2_typeUsage{NUTZUNG_UNDEFINED};

    int _anthesisDay{-1};
    int _maturityDay{-1};
		
    bool _automaticHarvest{false};
		AutomaticHarvestParameters _automaticHarvestParams;
	};
	
  typedef std::shared_ptr<Crop> CropPtr;
}

#endif
