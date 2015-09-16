/**
Authors: 
Dr. Claas Nendel <claas.nendel@zalf.de>
Xenia Specka <xenia.specka@zalf.de>
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

#ifndef SIMULATION_H_
#define SIMULATION_H_

// must be activated when building monica for python
//#define RUN_HERMES
// #define RUN_EVA
//#define RUN_CC_GERMANY
//#define RUN_GIS

#include <string>
#include "tools/date.h"
#include "monica-parameters.h"
#include "monica.h"
#include "../run/run-monica.h"
//#include "gis_simulation_methods.h"

namespace Monica 
{

#ifdef RUN_EVA

/**
 * Class that contains all information for running a eva2 simulation
 */
class Eva2SimulationConfiguration
{
  public:
    Eva2SimulationConfiguration() {}
    ~Eva2SimulationConfiguration(){}

    void setClassification(int classification) { this->classification = classification; }
    void setFruchtArt(std::string fruchtArt) { this->fruchtArt.push_back(fruchtArt); }
    void setFruchtFolge(std::string fruchtFolge) { this->fruchtFolge = fruchtFolge; }
    void setFruchtfolgeGlied(int fruchtfolgeGlied) { this->fruchtfolgeGlied.push_back(fruchtfolgeGlied); }
    void addFfgAnlage(int anl) { this->ffg_anlagen.push_back(anl); }
    void setFruchtfolgeYear(std::string year) { this->fruchtfolgeYear.push_back(year); fruchtfolgeYearInt.push_back(atoi(year.c_str()));}
    void setLocation(int location) { this->location = location; }
    void setLocationName(std::string location) { this->locationName = location; }
    void setProfil_number(int profil_number) { this->profil_number = profil_number; }
    void setVariante(int variante) { this->variante = variante; }
    void setOutputPath(std::string output_path) { this->outputPath = output_path; }
    void setStartDate(int year, int month, int day, bool useLeapYears) { this->startDate =  Tools::Date(day, month, year, useLeapYears); }
    void setEndDate(int year, int month, int day, bool useLeapYears) { this->endDate = Tools::Date(day, month, year, useLeapYears); }
    void setPseudoStartDate(int year, int month, int day, bool useLeapYears) { this->pseudoStartDate = Tools::Date(day, month, year, useLeapYears); }
    void setPseudoSimulation(bool state) { this->pseudoSimulation = state; }


    int getClassification() const { return classification; }
    std::vector<std::string> getFruchtArt() const { return fruchtArt; }
    std::string getFruchtFolge() const { return fruchtFolge; }
    std::vector<int> getFruchtfolgeGlied() const { return fruchtfolgeGlied; }
    std::vector<int> getFfgAnlage() const { return ffg_anlagen; }
    std::vector<std::string> getFruchtfolgeYear() const { return fruchtfolgeYear; }
    std::vector<int> getFruchtfolgeYearInt() const { return fruchtfolgeYearInt; }
    int getLocation() const { return location; }
    std::string getLocationName() const { return locationName; }
    int getProfil_number() const { return profil_number; }
    int getVariante() const { return variante; }
    std::string getOutputPath() const { return outputPath; }
    Tools::Date getStartDate() const { return startDate; }
    std::string getEndDateMySQL() const { return endDate.toMysqlString(""); }
    std::string getStartDateMySQL() const { return startDate.toMysqlString(""); }
    Tools::Date getEndDate() const { return endDate; }
    Tools::Date getPseudoStartDate() const { return pseudoStartDate; }
    bool isPseudoSimulation() const { return this->pseudoSimulation; }


  private:
    int location;
    int classification;
    int variante;
    std::vector<int> fruchtfolgeGlied;
    std::vector<int> ffg_anlagen;
    std::vector<std::string> fruchtfolgeYear;
    int profil_number;

    std::string fruchtFolge;
    std::vector<std::string> fruchtArt;
    std::string outputPath;
    std::string locationName;
    std::vector<int> fruchtfolgeYearInt;

    Tools::Date startDate;
    Tools::Date endDate;

    bool pseudoSimulation;
    Tools::Date pseudoStartDate;

};

#endif /*#ifdef RUN_EVA*/


#ifdef RUN_HERMES

class HermesSimulationConfiguration
{
public:
	HermesSimulationConfiguration()
		: startYear(0),
			endYear(0),
			organicFertiliserID(0),
			mineralFertiliserID(0),
			precipManipulator(1.0),
			NDeposition(20),
			minGWDepth(-1.0),
			maxGWDepth(-1.0),
			latitude(-1.0),
			slope(-1.0),
			heightNN(-1.0),
			soilCNRatio(-1.0),
			drainageCoeff(-1.0),
			atmosphericCO2(-1.0),
			pH(-1.0),
			windSpeedHeight(-1.0),
			leachingDepth(-1.0),
			minGWDepthMonth(-1),
			maxEffectiveRootingDepth(-1.0),
	    automaticIrrigation(false),
	    NMinFertiliser(false),
		nitrogenResponseOn(true),
		waterDeficitResponseOn(true),
		emergenceFloodingControlOn(true),
		emergenceMoistureControlOn(true),
		automaticHarvest(false)
		
	{}
	~HermesSimulationConfiguration(){}



    // setter
    void setOutputPath(std::string output_path) { this->outputPath = output_path; }
    void setSoilParametersFile(std::string file) { this->soilParametersFile = file; }
    void setWeatherFile(std::string file) { this->weatherFile = file; }
    void setFertiliserFile(std::string fertiliserFile) { this->fertiliserFile = fertiliserFile; }
    void setIrrigationFile(std::string irrigationFile) { this->irrigationFile = irrigationFile; }
    void setRotationFile(std::string rotationFile) { this->rotationFile = rotationFile; }
    void setGroundwaterTableFile(std::string gwFile) { this->groundwaterFile = gwFile; }
    void setPrecipManipulationValue(double value) {precipManipulator = value; }

    void setMinGWDepth(double v) { minGWDepth=v; }
    void setMaxGWDepth(double v) { maxGWDepth=v; }
    void setlatitude(double v) { latitude=v; }
    void setSlope(double v) { slope=v; }
    void setHeightNN(double v) { heightNN=v; }
    void setSoilCNRatio(double v) { soilCNRatio=v; }
    void setDrainageCoeff(double v) { drainageCoeff=v; }
	void setAtmosphericCO2(double v) { atmosphericCO2=v; }
    void setPH(double v) {pH = v; }
    void setWindSpeedHeight(double v) { windSpeedHeight=v; }
    void setLeachingDepth(double v) { leachingDepth=v; }
    void setMinGWDepthMonth(int v) { minGWDepthMonth=v; }
    void setNDeposition(double v) { NDeposition=v; }
    void setMaxEffectiveRootingDepth(double v) { maxEffectiveRootingDepth=v; }


    void setGroundwaterDischarge(double v) {groundwaterDischarge = v; }
    void setLayerThickness(double v) {layerThickness = v; }
    void setNumberOfLayers(double v) {numberOfLayers = v; }
    void setCriticalMoistureDepth(double v) {criticalMoistureDepth = v; }
    void setSurfaceRoughness(double v) {surfaceRoughness = v; }
    void setDispersionLength(double v) {dispersionLength = v; }
    void setMaxPercolationRate(double v) {maxPercolationRate = v; }

    void setSecondaryYields(bool v) {secondaryYields = v; }
	void setNitrogenResponseOn(bool v) {nitrogenResponseOn = v; }
	void setWaterDeficitResponseOn(bool v) {waterDeficitResponseOn = v; }
    void setEmergenceFloodingControlOn(bool v) {emergenceFloodingControlOn = v; } 
	void setEmergenceMoistureControlOn(bool v) {emergenceMoistureControlOn = v; } 

    void setStartYear(int year) { this->startYear = year; }
    void setEndYear(int year) { this->endYear = year; }
    void setOrganicFertiliserID(int organicFertiliserID) { this->organicFertiliserID = organicFertiliserID; }
    void setMineralFertiliserID(int mineralFertiliserID) { this->mineralFertiliserID = mineralFertiliserID; }

    void setNMinUserParameters(double min, double max, int delayInDays) { this->nMinUserParameters = NMinUserParameters(min,max,delayInDays); }

	/**
	* @brief
	* @param amout
	* @param treshold
	* @param nitrateConcentration
	* @param sulfateConcentration
	*/
    void setAutomaticIrrigationParameters(double amount=0,
                                          double treshold=0.1,
                                          double nitrateConcentration=0,
                                          double sulfateConcentration=0)
    {
      this->automaticIrrigationParameters = AutomaticIrrigationParameters(amount, treshold, nitrateConcentration, sulfateConcentration);
    }

	/**
	* @brief Setter for automatic yield parameters.
	* 
	*/
	void setAutomaticHarvestParameters(Monica::AutomaticHarvestTime yt) { this->automaticHarvestParameters = AutomaticHarvestParameters(yt); }


    void setNMinFertiliser(bool state) { this->NMinFertiliser = state;}
    void setAutomaticIrrigation(bool state) { this->automaticIrrigation = state;}
	void setAutomaticHarvest(bool state) { this->automaticHarvest = state; }

    void setInitPercentageFC(double perc) { this->initPercentageFC = perc; }
    void setInitSoilNitrate(double nitrate) { this->initSoilNitrate = nitrate; }
    void setInitSoilAmmonium(double ammonium) { this->initSoilAmmonium = ammonium; }


    // getter
    std::string getOutputPath() const { return outputPath; }
    std::string getSoilParametersFile() const { return soilParametersFile; }
    std::string getWeatherFile() const { return weatherFile; }
    std::string getFertiliserFile() const { return fertiliserFile; }
    std::string getIrrigationFile() const { return irrigationFile; }
    std::string getRotationFile() const { return rotationFile; }
    std::string getGroundwaterFile() const { return groundwaterFile; }

    int getStartYear() const { return startYear; }
    int getEndYear() const { return endYear; }
    int getOrganicFertiliserID() const { return organicFertiliserID; }
    int getMineralFertiliserID() const { return mineralFertiliserID; }

    double precipManipulationValue() const {return precipManipulator; }

    double getMinGWDepth() const {return minGWDepth; }
    double getMaxGWDepth() const {return maxGWDepth; }
    double getLatitude() const {return latitude; }
    double getSlope() const {return slope; }
    double getHeightNN() const {return heightNN; }
    double getSoilCNRatio() const {return soilCNRatio; }
	double getDrainageCoeff() const {return drainageCoeff; }
    double getAtmosphericCO2() const {return atmosphericCO2; }
    double getPH() const { return pH; }
    double getWindSpeedHeight() const {return windSpeedHeight; }
    double getLeachingDepth() const {return leachingDepth; }
    int getMinGWDepthMonth() const {return minGWDepthMonth; }
    double getNDeposition() const {return NDeposition; }
    double getMaxEffectiveRootingDepth() const {return maxEffectiveRootingDepth; }

    double getGroundwaterDischarge() {return groundwaterDischarge; }
    double getLayerThickness() { return layerThickness; }
    double getNumberOfLayers() { return numberOfLayers; }
    double getCriticalMoistureDepth() { return criticalMoistureDepth; }
    double getSurfaceRoughness() { return surfaceRoughness; }
    double getDispersionLength() { return dispersionLength; }
    double getMaxPercolationRate() { return maxPercolationRate; }

    bool getSecondaryYields() { return secondaryYields; }
    bool getNitrogenResponseOn() { return nitrogenResponseOn; }
	bool getWaterDeficitResponseOn() { return waterDeficitResponseOn; }
	bool getEmergenceFloodingControlOn() { return emergenceFloodingControlOn; }
	bool getEmergenceMoistureControlOn() { return emergenceMoistureControlOn; }

    bool useNMinFertiliser() { return this->NMinFertiliser;}
    bool useAutomaticIrrigation() { return this->automaticIrrigation;}
	bool useAutomaticHarvest() { return this->automaticHarvest; }

    Monica::NMinUserParameters getNMinUserParameters() const { return nMinUserParameters; }
    Monica::AutomaticIrrigationParameters getAutomaticIrrigationParameters() const { return automaticIrrigationParameters; }
	Monica::AutomaticHarvestParameters getAutomaticHarvestParameters() const { return automaticHarvestParameters; }


    double getInitPercentageFC() { return this->initPercentageFC; }
    double getInitSoilNitrate() { return this->initSoilNitrate; }
    double getInitSoilAmmonium() { return this->initSoilAmmonium; }

	

  private:
    std::string outputPath;
    std::string soilParametersFile;
    std::string weatherFile;
    std::string rotationFile;
    std::string fertiliserFile;
    std::string irrigationFile;
    std::string groundwaterFile;

    int startYear;
    int endYear;
    int organicFertiliserID;
    int mineralFertiliserID;

    double precipManipulator;
    double NDeposition;
    double minGWDepth;
    double maxGWDepth;
    double latitude;
    double slope;
    double heightNN;
    double soilCNRatio;
    double drainageCoeff;
	double atmosphericCO2;
    double pH;
    double windSpeedHeight;
    double leachingDepth;
    int minGWDepthMonth;

    double groundwaterDischarge;
    double layerThickness;
    double numberOfLayers;
    double criticalMoistureDepth;
    double surfaceRoughness;
    double dispersionLength;
    double maxPercolationRate;
    double maxEffectiveRootingDepth;

    bool secondaryYields;
	bool nitrogenResponseOn;
	bool waterDeficitResponseOn;
	bool emergenceFloodingControlOn;
	bool emergenceMoistureControlOn;
    int julianDayOfIrrigation;

    bool automaticIrrigation;
    bool NMinFertiliser;
	bool automaticHarvest;
	

    // initialisation values
    double initPercentageFC;    // Initial soil moisture content in percent field capacity
    double initSoilNitrate;     // Initial soil nitrate content [kg NO3-N m-3]
    double initSoilAmmonium;    // Initial soil ammonium content [kg NH4-N m-3]


    Monica::NMinUserParameters nMinUserParameters;
    Monica::AutomaticIrrigationParameters automaticIrrigationParameters;
	Monica::AutomaticHarvestParameters automaticHarvestParameters;

	
	
};





#endif

#ifdef RUN_CC_GERMANY

class CCGermanySimulationConfiguration
{
  public:
    CCGermanySimulationConfiguration()
      : buekId(-1),
        statId(377),
        julianSowingDate(-1),
        groundwaterDepth(20),
        cropId(1)
    {
    }

    ~CCGermanySimulationConfiguration(){}

    void setBuekId(int id) { this->buekId = id; }
    void setStatId(int id) { this->statId = id; }
    void setJulianSowingDate(double day) { this->julianSowingDate = day; }
    void setGroundwaterDepth(double depth) {this->groundwaterDepth = depth; }
    void setOutputPath(std::string output_path) { this->outputPath = output_path; }
    void setStartDate(std::string mysql_date) { this->startDate = Tools::fromMysqlString(mysql_date.c_str());}
    void setEndDate(std::string mysql_date) { this->endDate = Tools::fromMysqlString(mysql_date.c_str());}

    int getBuekId() const { return buekId; }
    int getStatId() const { return this->statId; }
    double getJulianSowingDate() const { return this->julianSowingDate; }
    double getGroundwaterDepth() const {return this->groundwaterDepth; }
    std::string getOutputPath() const { return outputPath; }
    Tools::Date getStartDate() const { return this->startDate; }
    Tools::Date getEndDate() const { return this->endDate; }

  private:
    int buekId;
    int statId;
    double julianSowingDate;
    double groundwaterDepth;
    std::string outputPath;

    Tools::Date startDate;
    Tools::Date endDate;

    int cropId;
};

#endif /*#ifdef RUN_CC_GERMANY*/

#ifdef RUN_GIS

class GISSimulationConfiguration
{
  public:
    GISSimulationConfiguration()
      : julianSowingDate(-1),
        row(-1),
        col(-1),
        scenario("A1B"),
        realisierung("feu_a"),
        cropId(1)
    {

    }

    ~GISSimulationConfiguration(){}

    void setJulianSowingDate(double day) { this->julianSowingDate = day; }
    void setRow(double r) {this->row = r; }
    void setCol(double c) {this->col = c; }
    void setOutputPath(std::string output_path) { this->outputPath = output_path; }
    void setStartDate(std::string mysql_date) { this->startDate = Tools::fromMysqlString(mysql_date.c_str());}
    void setEndDate(std::string mysql_date) { this->endDate = Tools::fromMysqlString(mysql_date.c_str());}

    double getJulianSowingDate() const { return this->julianSowingDate; }
    double getRow() const {return this->row; }
    double getCol() const {return this->col; }
    std::string getOutputPath() const { return outputPath; }
    Tools::Date getStartDate() const { return this->startDate; }
    Tools::Date getEndDate() const { return this->endDate; }
    std::string getScenario() const { return this->scenario; }
    std::string getRealisierung() const { return this->realisierung; }

  private:
    double julianSowingDate;
    double row;
    double col;
    std::string scenario;
    std::string realisierung;
    
    std::string outputPath;
    
    Tools::Date startDate;
    Tools::Date endDate;

    int cropId;
};

#endif /*#ifdef RUN_GIS*/

#ifdef RUN_EVA
const Monica::Result runEVA2Simulation(const Eva2SimulationConfiguration *simulation_config=0);
#endif

#ifdef RUN_CC_GERMANY
const Monica::Result runCCGermanySimulation(const CCGermanySimulationConfiguration *simulation_config=0);
#endif

#ifdef RUN_GIS
const Monica::Result runGISSimulation(const GISSimulationConfiguration *simulation_config=0);
#endif

#ifdef RUN_HERMES
const Monica::Result runWithHermesData( HermesSimulationConfiguration *hermes_config=0);
const Monica::Result runWithHermesData(const std::string);
HermesSimulationConfiguration * getHermesConfigFromIni(std::string output_path);
Monica::Env getHermesEnvFromConfiguration(HermesSimulationConfiguration*);
#endif

//void writeSoilPMsToFile(std::string path, const std::vector<SoilParameters> *soil_pms, int mode);
void activateDebugOutput(bool);

}

#endif /* SIMULATION_H_ */
