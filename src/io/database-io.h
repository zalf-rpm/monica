/* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
Authors: 
Michael Berg <michael.berg@zalf.de>
Xenia Specka <xenia.specka@zalf.de>
Claas Nendel <claas.nendel@zalf.de>

Maintainers: 
Currently maintained by the authors.

This file is part of the MONICA model. 
Copyright (C) Leibniz Centre for Agricultural Landscape Research (ZALF)
*/

#ifndef DATABASE_IO_H_
#define DATABASE_IO_H_

#include <string>

#include "../core/monica-parameters.h"

namespace Monica
{
	enum
	{
		MODE_LC_DSS = 0,
		MODE_ACTIVATE_OUTPUT_FILES,
		MODE_HERMES,
		MODE_EVA2,
		MODE_SENSITIVITY_ANALYSIS,
		MODE_CC_GERMANY,
		MODE_MACSUR_SCALING,
		MODE_MACSUR_SCALING_CALIBRATION,
		MODE_CARBIOCIAL_CLUSTER
	};
	
  SpeciesParametersPtr getSpeciesParametersFromMonicaDB(const std::string& species,
                                                        std::string abstractDbSchema = "monica");

  CultivarParametersPtr getCultivarParametersFromMonicaDB(const std::string& species, 
																													const std::string& cultivar,
																													std::string abstractDbSchema = "monica");

  CropParametersPtr getCropParametersFromMonicaDB(const std::string& species, 
																									const std::string& cultivar,
																									std::string abstractDbSchema = "monica");

  CropParametersPtr getCropParametersFromMonicaDB(int cropId,
                                                  std::string abstractDbSchema = "monica");

	void writeCropParameters(std::string path, std::string abstractDbSchema = "monica");

	//-----------------------------------------------------------

  MineralFertiliserParameters 
		getMineralFertiliserParametersFromMonicaDB(const std::string& mineralFertiliserId,
		                                           std::string abstractDbSchema = "monica");

	void writeMineralFertilisers(std::string path, std::string abstractDbSchema = "monica");

	//-----------------------------------------------------------

  OrganicFertiliserParametersPtr 
		getOrganicFertiliserParametersFromMonicaDB(const std::string& id,
		                                           std::string abstractDbSchema = "monica");

	void writeOrganicFertilisers(std::string path, std::string abstractDbSchema = "monica");

	//-----------------------------------------------------------

	CropResidueParametersPtr
		getResidueParametersFromMonicaDB(const std::string& species,
																		 const std::string& residueType = "",
																		 std::string abstractDbSchema = "monica");
		
  void writeCropResidues(std::string path, std::string abstractDbSchema = "monica");

	//-----------------------------------------------------------

	CropModuleParameters readUserCropParametersFromDatabase(std::string type,
	                                                      std::string abstractDbSchema = "monica");
	EnvironmentParameters readUserEnvironmentParametersFromDatabase(std::string type,
	                                                                    std::string abstractDbSchema = "monica");
	SoilMoistureModuleParameters readUserSoilMoistureParametersFromDatabase(std::string type,
	                                                                      std::string abstractDbSchema = "monica");
	SoilOrganicModuleParameters readUserSoilOrganicParametersFromDatabase(std::string type,
	                                                                    std::string abstractDbSchema = "monica");
	SoilTemperatureModuleParameters readUserSoilTemperatureParametersFromDatabase(std::string type,
	                                                                            std::string abstractDbSchema = "monica");
	SoilTransportModuleParameters readUserSoilTransportParametersFromDatabase(std::string type,
	                                                                        std::string abstractDbSchema = "monica");
	SimulationParameters readUserSimParametersFromDatabase(std::string type,
	                                                       std::string abstractDbSchema = "monica");

	CentralParameterProvider readUserParameterFromDatabase(int type = 0,
	                                                       std::string abstractDbSchema = "monica");

	void writeUserParameters(int type, std::string path, std::string abstractDbSchema = "monica");

	//-----------------------------------------------------------

  struct AMCRes
  {
    std::string speciesId, cultivarId, name;
  };
  const std::map<int, AMCRes>& availableMonicaCropsM();
	std::vector<AMCRes> availableMonicaCrops();
}  

#endif 
