/**
Authors: 
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

#ifndef HERMES_FILE_IO_H_
#define HERMES_FILE_IO_H_

#include <string>

#include "climate/climate-common.h"

#include "../core/monica-parameters.h"

namespace Monica
{
	CropPtr hermesCropId2Crop(const std::string& hermesCropId);

	std::pair<FertiliserType, int> hermesFertiliserName2monicaFertiliserId(const std::string& name);

	void attachFertiliserApplicationsToCropRotation(std::vector<ProductionProcess>& cropRotation,
		const std::string& pathToFertiliserFile);

	std::vector<ProductionProcess> attachFertiliserSA(std::vector<ProductionProcess> cropRotation,
		const std::string pathToFertiliserFile);

	void attachIrrigationApplicationsToCropRotation(std::vector<ProductionProcess>& cropRotation,
		const std::string& pathToIrrigationFile);

	/*!
	* - create cropRotation from hermes file
	* - the returned production processes contain absolute dates
	*/
	std::vector<ProductionProcess> cropRotationFromHermesFile(const std::string& pathToFile, 
		bool useAutomaticHarvestTrigger = false, AutomaticHarvestParameters autoHarvestParameters = AutomaticHarvestParameters());

	Climate::DataAccessor climateDataFromHermesFiles(const std::string& pathToFile,
		int fromYear, int toYear,
		const CentralParameterProvider& cpp,
		bool useLeapYears = true, double latitude = 51.2);

}  

#endif 
