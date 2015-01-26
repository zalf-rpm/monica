/**
Authors: 
Xenia Specka <xenia.specka@zalf.de>

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


#ifndef MACSUR_SCALING_INTERFFACE_H_
#define MACSUR_SCALING_INTERFFACE_H_


#include "climate/climate-common.h"
#include "tools/date.h"
#include "monica.h"
#include "src/debug.h"

namespace Monica 
{


class MacsurScalingConfiguration
{
  public:
    MacsurScalingConfiguration()
    : crop_name(""), climate_file(""), ini_file(""), input_path(""), output_path(""), soil_file(""),  project_id(""), lookup_project_id(""), row_id(0), col_id(0), phase(1), step(1)
      {}

    ~MacsurScalingConfiguration() {}

    std::string getCropName() const { return crop_name; }
    std::string getClimateFile() const  { return climate_file; }
    std::string getIniFile() const { return ini_file; }
    std::string getInputPath() const { return input_path; }
    std::string getOutputPath() const { return output_path; }
    Tools::Date getStartDate() const { return start_date; }
    Tools::Date getEndDate() const { return end_date; }
    std::string getProjectId() const { return project_id; }
    std::string getLookupProjectId() const { return lookup_project_id; }
    int getRowId() const { return row_id; }
    int getColId() const { return col_id; }
    int getPhase() const {return phase; }
    int getStep() const {return step; }
    double getLatitude() const {return latitude; }
    double getElevation() const {return elevation; }
    std::string getSoilFile() const {return soil_file; }


    void setClimateFile(std::string climate_file) { this->climate_file = climate_file; }
    void setIniFile(std::string ini_file) { this->ini_file = ini_file; }
    void setInputPath(std::string path) { this->input_path = path; }
    void setOutputPath(std::string path) { this->output_path = path; }
    void setCropName(std::string crop_name) { this->crop_name = crop_name; }
    void setStartDate(std::string date) { this->start_date = Tools::fromMysqlString(date.c_str()); }
    void setEndDate(std::string date) { this->end_date = Tools::fromMysqlString(date.c_str()); }
    void setProjectId(std::string project_id) { this->project_id = project_id; }
    void setLookupProjectId(std::string project_id) { this->lookup_project_id = project_id; }
    void setRowId(int row_id) { this->row_id = row_id; }
    void setColId(int col_id) { this->col_id = col_id; }
    void setPhase(int p) {this->phase = p; }
    void setStep(int s) {this->step = s; }
    void setLatitude(double lat) { this->latitude = lat; }
    void setElevation(double ele) { this->elevation = ele; }
    void setSoilFile(std::string file) { soil_file = file; }

    private:
      std::string crop_name;
      std::string climate_file;
      std::string ini_file;
      std::string input_path;
      std::string output_path;
      std::string soil_file;
      Tools::Date start_date;
      Tools::Date end_date;
      std::string project_id;
      std::string lookup_project_id;
      int row_id;
      int col_id;
      int phase ;
      int step;
      double latitude;
      double elevation;

};

void runMacsurScalingSimulation(const MacsurScalingConfiguration *simulation_config=0);
const SoilPMs* soilParametersFromFile(const std::string pathToFile, const GeneralParameters& gps = GeneralParameters(), double soil_ph=-1.0);
const SoilPMs* phase2SoilParametersFromFile(const std::string pathToFile, const GeneralParameters& gps, CentralParameterProvider &cpp, double soil_ph=-1.0, std::string project_id="");
Climate::DataAccessor climateDataFromMacsurFiles(const std::string pathToFile, const CentralParameterProvider& cpp, double latitude, const MacsurScalingConfiguration *simulation_config);

}

#endif /*MACSUR_SCALING_INTERFFACE_H_*/

