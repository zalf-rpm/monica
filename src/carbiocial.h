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

#ifndef _CARBIOCIAL_H_
#define _CARBIOCIAL_H_

#include <vector>
#include <map>
//#include <set>

#include "monica-parameters.h"
#include "soil/soil.h"

namespace Carbiocial
{
  std::pair<const Soil::SoilPMs*, int>
		carbiocialSoilParameters(int profileId, int layerThicknessCm, int maxDepthCm, 
		Monica::GeneralParameters gps, std::string s, Monica::CentralParameterProvider cpp);

	class CarbiocialConfiguration
	{
	public:
		CarbiocialConfiguration() : 
			create2013To2040ClimateData(false),
			climate_file(""),
			ini_file(""),
			input_path(""),
			output_path(""),
			row_id(0),
			col_id(0)
		{}

		~CarbiocialConfiguration() {}

		bool create2013To2040ClimateData;
		std::string pathToClimateDataReorderingFile;

		std::string getClimateFile() const  { return climate_file; }
		std::string getIniFile() const { return ini_file; }
		std::string getInputPath() const { return input_path; }
		std::string getOutputPath() const { return output_path; }
		Tools::Date getStartDate() const { return start_date; }
		Tools::Date getEndDate() const { return end_date; }
		int getRowId() const { return row_id; }
		int getColId() const { return col_id; }
		double getLatitude() const { return latitude; }
		double getElevation() const { return elevation; }
		int getProfileId() const { return profileId; }

		void setClimateFile(std::string climate_file) { this->climate_file = climate_file; }
		void setIniFile(std::string ini_file) { this->ini_file = ini_file; }
		void setInputPath(std::string path) { this->input_path = path; }
		void setOutputPath(std::string path) { this->output_path = path; }
		void setStartDate(std::string date) { this->start_date = Tools::fromMysqlString(date.c_str()); }
		void setEndDate(std::string date) { this->end_date = Tools::fromMysqlString(date.c_str()); }
		void setRowId(int row_id) { this->row_id = row_id; }
		void setColId(int col_id) { this->col_id = col_id; }
		void setLatitude(double lat) { this->latitude = lat; }
		void setElevation(double ele) { this->elevation = ele; }
		void setProfileId(int pid) { profileId = pid; }

	private:
		std::string climate_file;
		std::string ini_file;
		std::string input_path;
		std::string output_path;
		Tools::Date start_date;
		Tools::Date end_date;
		int row_id;
		int col_id;
		double latitude;
		double elevation;
		int profileId;
	};

	std::map<int, double> runCarbiocialSimulation(const CarbiocialConfiguration* simulation_config = 0);
	
	Climate::DataAccessor climateDataFromCarbiocialFiles(const std::string& pathToFile, const Monica::CentralParameterProvider& cpp,
		double latitude, const CarbiocialConfiguration* simulation_config);
}

#endif //_CARBIOCIAL_H_
