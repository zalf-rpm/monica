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
