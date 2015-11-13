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

#ifndef HERMES_FILE_IO_H_
#define HERMES_FILE_IO_H_

#include <string>

#include "climate/climate-common.h"

#include "../core/monica-parameters.h"
#include "../run/cultivation-method.h"

namespace Monica
{
	CropPtr hermesCropId2Crop(const std::string& hermesCropId);

	std::pair<FertiliserType, std::string> hermesFertiliserName2monicaFertiliserId(const std::string& name);

	void attachFertiliserApplicationsToCropRotation(std::vector<CultivationMethod>& cropRotation,
		const std::string& pathToFertiliserFile);

	std::vector<CultivationMethod> attachFertiliserSA(std::vector<CultivationMethod> cropRotation,
		const std::string pathToFertiliserFile);

	void attachIrrigationApplicationsToCropRotation(std::vector<CultivationMethod>& cropRotation,
		const std::string& pathToIrrigationFile);

	/*!
	* - create cropRotation from hermes file
	* - the returned cultivation method contain absolute dates
	*/
	std::vector<CultivationMethod> cropRotationFromHermesFile(const std::string& pathToFile,
																														bool useAutomaticHarvestTrigger = false,
																														AutomaticHarvestParameters autoHarvestParameters = AutomaticHarvestParameters());

	Climate::DataAccessor climateDataFromHermesFiles(const std::string& pathToFile,
																									 int fromYear,
																									 int toYear,
																									 const CentralParameterProvider& cpp,
																									 bool useLeapYears = true,
																									 double latitude = 51.2);

}  

#endif 
