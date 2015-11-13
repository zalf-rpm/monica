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
  SpeciesParametersPtr getSpeciesParametersFromMonicaDB(const std::string& species);

  CultivarParametersPtr getCultivarParametersFromMonicaDB(const std::string& species, const std::string& cultivar);

  CropParametersPtr getCropParametersFromMonicaDB(const std::string& species, const std::string& cultivar);

  CropParametersPtr getCropParametersFromMonicaDB(int cropId);

	void writeCropParameters(std::string path);

	//-----------------------------------------------------------

  MineralFertiliserParameters getMineralFertiliserParametersFromMonicaDB(const std::string& mineralFertiliserId);

	void writeMineralFertilisers(std::string path);

	//-----------------------------------------------------------

  OrganicFertiliserParametersPtr getOrganicFertiliserParametersFromMonicaDB(const std::string& id);

	void writeOrganicFertilisers(std::string path);

	//-----------------------------------------------------------

  CropResidueParametersPtr getResidueParametersFromMonicaDB(const std::string& species, const std::string& cultivar, int cropId = -1);

	inline CropResidueParametersPtr getResidueParametersFromMonicaDB(int cropId) { return getResidueParametersFromMonicaDB("", "", cropId); }

  void writeCropResidues(std::string path);

	//-----------------------------------------------------------

	CentralParameterProvider readUserParameterFromDatabase(int type = 0);

	void writeUserParameters(int type, std::string path);

	//-----------------------------------------------------------

	const std::map<int, std::string>& availableMonicaCrops();
}  

#endif 
