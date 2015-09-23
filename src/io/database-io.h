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
	const CropParameters* getCropParametersFromMonicaDB(int cropId);

	void writeCropParameters(std::string path);

	//-----------------------------------------------------------

	const std::map<int, MineralFertiliserParameters>& getAllMineralFertiliserParametersFromMonicaDB();

	MineralFertiliserParameters getMineralFertiliserParametersFromMonicaDB(int mineralFertiliserId);

	void writeMineralFertilisers(std::string path);

	//-----------------------------------------------------------

	const std::map<int, OMPPtr>& getAllOrganicFertiliserParametersFromMonicaDB();

	OrganicMatterParameters* getOrganicFertiliserParametersFromMonicaDB(int organ_fert_id);

	void writeOrganicFertilisers(std::string path);

	//-----------------------------------------------------------

	const std::map<int, OMPPtr>& getAllResidueParametersFromMonicaDB();

	const OrganicMatterParameters* getResidueParametersFromMonicaDB(int crop_id);

	void writeResidues(std::string path);

	//-----------------------------------------------------------

	CentralParameterProvider readUserParameterFromDatabase(int type = 0);

	void writeUserParameters(int type, std::string path);

	//-----------------------------------------------------------

	const std::map<int, std::string>& availableMonicaCrops();
}  

#endif 
