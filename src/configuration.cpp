/**
Authors: 
Jan Vaillant <jan.vaillant@zalf.de>

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
#include <iostream>
#include <fstream>
#include <sys/stat.h>

#include "monica-parameters.h"
#include "monica.h"
#include "soilcolumn.h"
#include "soiltemperature.h"
#include "soilmoisture.h"
#include "soilorganic.h"
#include "soiltransport.h"
#include "crop.h"

#include "conversion.h"
#include "climate/climate-common.h"
#include "db/abstract-db-connections.h"
#include "tools/algorithms.h"
#include "tools/read-ini.h"


#include "configuration.h"

#ifdef __unix__
  #include <sys/io.h>
#else
  #include <io.h>
#endif

#include <boost/foreach.hpp>
#include <boost/format.hpp>

/* write cson to string */
extern "C" {
  static int cson_data_dest_string(void * state, void const * src, unsigned int n )
  {
    if( ! state ) return cson_rc.ArgError;
    if( !src || !n ) return 0;
    else
    {
      std::string *str = static_cast<std::string*>(state);
      str->append((char*) src, n);
      return 0;
    }
  }
}

const cson_value* Monica::Configuration::metaSim = NULL;
const cson_value* Monica::Configuration::metaSite = NULL;
const cson_value* Monica::Configuration::metaCrop = NULL;

using namespace Monica;

Configuration::Configuration(const std::string& outPath, const std::string& dirNameMet, const std::string& preMetFiles, const std::string& dbIniName)
  : _sim(NULL), _site(NULL), _crop(NULL), _outPath(outPath), _dirNameMet(dirNameMet), _preMetFiles(preMetFiles)
{
  Db::dbConnectionParameters(dbIniName);
}

Configuration::~Configuration() 
{
  freeCson();
}

bool Configuration::setJSON(const std::string& sim, const std::string& site, const std::string& crop)
{
  freeCson();
  cson_parse_info info = cson_parse_info_empty;
  int rc;

  rc = cson_parse_string( &_sim, sim.c_str(), sim.length(), NULL, &info );
  if( 0 != rc ) {
    printCsonError(rc, info);
    return false;
  }

  /* reset info. Otherwise start line number > 0 */
  info = cson_parse_info_empty;

  rc = cson_parse_string( &_site, site.c_str(), site.length(), NULL, &info );
  if( 0 != rc ) {
    /* all or nothing */
    freeCson();
    printCsonError(rc, info);
    return false;
  }

  /* reset info. Otherwise start line number > 0 */
  info = cson_parse_info_empty;

  rc = cson_parse_string( &_crop, crop.c_str(), crop.length(), NULL, &info );
  if( 0 != rc ) {
    /* all or nothing */
    freeCson();
    printCsonError(rc, info);
    return false;
  }

  if (!isValidated()) {
    freeCson();
    return false;
  }

  return true;
}

bool Configuration::setJSON(FILE* sim, FILE* site, FILE* crop)
{
  assert( sim && site && crop);
  freeCson();
  cson_parse_info info = cson_parse_info_empty;
  int rc;

  rc = cson_parse_FILE( &_sim, sim, NULL, &info );
  if( 0 != rc ) {
    printCsonError(rc, info);
    return false;
  }

  /* reset info. Otherwise start line number > 0 */ 
  info = cson_parse_info_empty;

  rc = cson_parse_FILE( &_site, site, NULL, &info );
  if( 0 != rc )
  {
    /* all or nothing */
    freeCson();
    printCsonError(rc, info);
    return false;
  }

  /* reset info. Otherwise start line number > 0 */
  info = cson_parse_info_empty;

  rc = cson_parse_FILE( &_crop, crop, NULL, &info );
  if( 0 != rc )
  {
    /* all or nothing */
    freeCson();
    printCsonError(rc, info);
    return false;
  }

  if (!isValidated()) {
    freeCson();
    return false;
  }

  return true;
}

bool Configuration::setJSON(cson_value* sim, cson_value* site, cson_value* crop)
{
  freeCson();

  _sim = sim;
  _site = site;
  _crop = crop;

  if (!isValidated()) {
    freeCson();
    return false;
  }

  return true;
}

const Result Configuration::run()
{

  if (!_sim || !_site || !_crop) {
    std::cerr << "Configuration is empty" << std::endl;
    return Result();
  }

  
  /* fetch root objects */
  cson_object* simObj = cson_value_get_object(_sim);
  cson_object* siteObj = cson_value_get_object(_site);
  cson_object* cropObj = cson_value_get_object(_crop);
  
  SiteParameters sp;
  CentralParameterProvider cpp = readUserParameterFromDatabase(Env::MODE_HERMES);
  GeneralParameters gp(cpp.userEnvironmentParameters.p_LayerThickness,
    cpp.userEnvironmentParameters.p_LayerThickness * double(cpp.userEnvironmentParameters.p_NumberOfLayers));

  /* fetch soil horizon array */
  cson_array* horizonsArr = cson_value_get_array(cson_object_get(siteObj, "horizons"));
  /* fetch crop rray */
  cson_array* cropsArr = cson_value_get_array(cson_object_get(cropObj, "crops"));

  /* sim */
	int startYear = getIsoDate(simObj, "time.startDate").year();
  //int startYear = getInt(simObj, "time.startYear");
  int endYear = getIsoDate(simObj, "time.endDate").year();
  //int endYear = getInt(simObj, "time.endYear");

	cpp.userEnvironmentParameters.p_UseSecondaryYields = 
		getBool(simObj, "switch.useSecondaryYieldOn", cpp.userEnvironmentParameters.p_UseSecondaryYields);
	gp.pc_NitrogenResponseOn = getBool(simObj, "switch.nitrogenResponseOn", gp.pc_NitrogenResponseOn);
	gp.pc_WaterDeficitResponseOn = getBool(simObj, "switch.waterDeficitResponseOn", gp.pc_WaterDeficitResponseOn);
	gp.pc_EmergenceMoistureControlOn = getBool(simObj, "switch.emergenceMoistureControlOn", gp.pc_EmergenceMoistureControlOn);
	gp.pc_EmergenceFloodingControlOn = getBool(simObj, "switch.emergenceFloodingControlOn", gp.pc_EmergenceFloodingControlOn);

	cpp.userInitValues.p_initPercentageFC = getDbl(simObj, "init.percentageFC", cpp.userInitValues.p_initPercentageFC);
	cpp.userInitValues.p_initSoilNitrate = getDbl(simObj, "init.soilNitrate", cpp.userInitValues.p_initSoilNitrate);
	cpp.userInitValues.p_initSoilAmmonium = getDbl(simObj, "init.soilAmmonium", cpp.userInitValues.p_initSoilAmmonium);

  std::cout << "fetched sim data"  << std::endl;
  
  /* site */
	sp.vq_NDeposition = getDbl(siteObj, "NDeposition", sp.vq_NDeposition);
	sp.vs_Latitude = getDbl(siteObj, "latitude", sp.vs_Latitude);
	sp.vs_Slope = getDbl(siteObj, "slope", sp.vs_Slope);
	sp.vs_HeightNN = getDbl(siteObj, "heightNN", sp.vs_HeightNN);
  sp.vs_Soil_CN_Ratio = 10; //TODO: per layer?
  sp.vs_DrainageCoeff = -1; //TODO: ?

  cpp.userEnvironmentParameters.p_AthmosphericCO2 = getDbl(siteObj, "atmosphericCO2");

	cpp.userEnvironmentParameters.p_MinGroundwaterDepth = getDbl(siteObj, "groundwaterDepthMin", cpp.userEnvironmentParameters.p_MinGroundwaterDepth);
	cpp.userEnvironmentParameters.p_MaxGroundwaterDepth = getDbl(siteObj, "groundwaterDepthMax", cpp.userEnvironmentParameters.p_MaxGroundwaterDepth);
	cpp.userEnvironmentParameters.p_MinGroundwaterDepthMonth = getDbl(siteObj, "groundwaterDepthMinMonth", cpp.userEnvironmentParameters.p_MinGroundwaterDepthMonth);

	cpp.userEnvironmentParameters.p_WindSpeedHeight = getDbl(siteObj, "windSpeedHeight", cpp.userEnvironmentParameters.p_WindSpeedHeight);
	cpp.userEnvironmentParameters.p_LeachingDepth = getDbl(siteObj, "leachingDepth", cpp.userEnvironmentParameters.p_LeachingDepth);
//  cpp.userEnvironmentParameters.p_NumberOfLayers = getInt(siteObj, "numberOfLayers"); ; // JV! currently not present in json

  // TODO: maxMineralisationDepth? (Fehler in gp ps_MaxMineralisationDepth und ps_MaximumMineralisationDepth?)
  gp.ps_MaxMineralisationDepth = 0.4;

  std::cout << "fetched site data"  << std::endl;

  /* soil */
  double lThicknessCm = 100.0 * cpp.userEnvironmentParameters.p_LayerThickness;
  // TODO: Bedeutung unklar. Weder die max. Layer Dicke (die ist ja 10 cm oder?) noch Boden Dicke. p_NumberOfLayers = 3
  double maxDepthCm =  200.0; // 100.0 * cpp.userEnvironmentParameters.p_LayerThickness * double(cpp.userEnvironmentParameters.p_NumberOfLayers);
  // TODO: Bedeutung unklar. Scheint weder die max. Anzahl je Horizont noch je Boden zu sein
  int maxNoOfLayers = int(maxDepthCm / lThicknessCm);

  SoilPMsPtr layers(new SoilPMs());
  if (!createLayers(*(layers.get()), horizonsArr, lThicknessCm, maxNoOfLayers)) {
    std::cerr << "Error fetching soil data"  << std::endl;
    return Result();
  }
  
  std::cout << "fetched soil data"  << std::endl;

  /* weather */
  Climate::DataAccessor da(Tools::Date(1, 1, startYear, true), Tools::Date(31, 12, endYear, true));
  if (!createClimate(da, cpp, sp.vs_Latitude)) {
    std::cerr << "Error fetching climate data"  << std::endl;
    return Result();
  }
  
  std::cout << "fetched climate data"  << std::endl;

  /* crops */
  std::vector<ProductionProcess> pps;
  if (!createProcesses(pps, cropsArr)) {
    std::cerr << "Error fetching crop data"  << std::endl;
    return Result();
  }
  
  std::cout << "fetched crop data"  << std::endl;

	Env env(layers, cpp);
  env.general = gp;
  env.pathToOutputDir = _outPath;
  /* TODO: kein output, wenn nicht gesetzt */
  env.setMode(Env::MODE_HERMES);
  env.site = sp;
  env.da = da;
  env.cropRotation = pps;
 
  // TODO:
  // if (hermes_config->useAutomaticIrrigation()) {
  //   env.useAutomaticIrrigation = true;
  //   env.autoIrrigationParams = hermes_config->getAutomaticIrrigationParameters();
  // }

  // if (hermes_config->useNMinFertiliser()) {
  //   env.useNMinMineralFertilisingMethod = true;
  //   env.nMinUserParams = hermes_config->getNMinUserParameters();
  //   env.nMinFertiliserPartition = getMineralFertiliserParametersFromMonicaDB(hermes_config->getMineralFertiliserID());
  // }

  std::cout << "run monica" << std::endl;

#ifndef MONICA_GUI
	return runMonica(env);
#else
	return runMonica(env, this);
#endif
}

void Configuration::setProgress(double progress)
{
  std::cout << progress;
}

bool Configuration::isValidated() {

  if (!metaSim || !metaSite || !metaCrop) {
    std::cerr << "Meta configuration is empty" << std::endl;
    return false;
  }

  if (!isValid(_sim, metaSim, "sim")) {
    std::cerr << "sim.json not valid" << std::endl;
    return false;
  }
  
  if (!isValid(_site, metaSite, "site")) {
    std::cerr << "site.json not valid" << std::endl;
    return false;
  }
  
  if (!isValid(_crop, metaCrop, "crop")) {
    std::cerr << "crop.json not valid" << std::endl;
    return false;
  }

  return true;
}

bool Configuration::createLayers(std::vector<SoilParameters> &layers, cson_array* horizonsArr, double lThicknessCm, int maxNoOfLayers)
{
  bool ok = true;
  unsigned int hs = cson_array_length_get(horizonsArr);
  std::cout << "fetching " << hs << " horizons" << std::endl;
  /* track total depth in case input horizons exceed total layer depth */
  double depth = 0.0;
  unsigned int h;

  for( h = 0; h < hs; ++h )
  {
    cson_object* horizonObj = cson_value_get_object(cson_array_get(horizonsArr, h));

    //int hLoBoundaryCm = 100 * getDbl(horizonObj, "lowerBoundary");
    //int hUpBoundaryCm = layers.size() * lThicknessCm;
		//int hThicknessCm = std::max(0, hLoBoundaryCm - hUpBoundaryCm);
    int hThicknessCm = std::max(0, Tools::roundRT<int>(100*getDbl(horizonObj, "thickness"), 0));
    int lInHCount = Tools::roundRT<int>(double(hThicknessCm) / double(lThicknessCm), 0);
    /* fill all (maxNoOfLayers) layers if available horizons depth < lThicknessCm * maxNoOfLayers */
    if (h == (hs - 1) && (int(layers.size()) + lInHCount) < maxNoOfLayers)
      lInHCount += maxNoOfLayers - layers.size() - lInHCount;

    SoilParameters layer;
    layer.set_vs_SoilOrganicCarbon(getDbl(horizonObj, "Corg", -1.0)); //TODO: / 100 ?
    layer.set_vs_SoilBulkDensity(getDbl(horizonObj, "bulkDensity", -1.0));
		layer.vs_SoilSandContent = getDbl(horizonObj, "sand", layer.vs_SoilSandContent);
		layer.vs_SoilClayContent = getDbl(horizonObj, "clay", layer.vs_SoilClayContent);
    layer.vs_SoilStoneContent = getDbl(horizonObj, "sceleton"); //TODO: / 100 ?
    layer.vs_Lambda = Tools::texture2lambda(layer.vs_SoilSandContent, layer.vs_SoilClayContent);
    // TODO: Wo wird textureClass verwendet?
    layer.vs_SoilTexture = getStr(horizonObj, "textureClass");
		layer.vs_SoilpH = getDbl(horizonObj, "pH", layer.vs_SoilpH);
    /* TODO: ? lambda = drainage_coeff ? */
    layer.vs_Lambda = Tools::texture2lambda(layer.vs_SoilSandContent, layer.vs_SoilClayContent);
    layer.vs_FieldCapacity = getDbl(horizonObj, "fieldCapacity");
    /* TODO: name? */
    layer.vs_Saturation = getDbl(horizonObj, "poreVolume");
    layer.vs_PermanentWiltingPoint = getDbl(horizonObj, "permanentWiltingPoint");

    /* TODO: hinter readJSON verschieben */ 
    if (!layer.isValid()) {
      ok = false;
      std::cerr << "Error in soil parameters." << std::endl;
    }

    for (int l = 0; l < lInHCount; l++) {
      
      /* stop if we reach max. depth */
      if (depth >= maxNoOfLayers * lThicknessCm) {
        std::cout << "Maximum soil layer depth (" << (maxNoOfLayers * lThicknessCm) 
          << " cm) reached. Remaining layers in horizon " << h << " ignored." << std::endl;
        break;
      }

      depth += lThicknessCm;

      layers.push_back(layer);
      std::cout << "fetched layer " << layers.size() << " in horizon " << h << std::endl;
    }

    std::cout << "fetched horizon " << h << std::endl;
  }  

  return ok;
}

bool Configuration::createProcesses(std::vector<ProductionProcess> &pps, cson_array* cropsArr)
{
  bool ok = true;
  unsigned int cs = cson_array_length_get(cropsArr);
  std::cout << "fetching " << cs << " crops" << std::endl;
  unsigned int c;
  for (c = 0; c < cs; ++c) {

    cson_object* cropObj = cson_value_get_object(cson_array_get(cropsArr, c));
    int cropId = getInt(cropObj, "name.id");
    std::string name = getStr(cropObj, "name.name");
    std::string genType = getStr(cropObj, "name.gen_type");
    int permCropId = -1;

    if (!cropId || cropId < 0) {
      ok = false;
      std::cerr << "Invalid crop id:" << name << genType << std::endl;
    }


    Db::DB *con = Db::newConnection("monica");
    if (con) {
      bool ok = con->select(
        "SELECT permanent_crop_id " 
        "FROM crop "
        "WHERE id=" + std::to_string(cropId)
      );

      if (ok)
      {
        Db::DBRow row = con->getRow();
        permCropId = Tools::satoi(row[0], -1);
      }

      delete con;
    }    
    

		Tools::Date sd = getIsoDate(cropObj, "sowingDate");
		Tools::Date hd = getIsoDate(cropObj, "finalHarvestDate");

    if (!sd.isValid() || !hd.isValid()) {
      ok = false;
      std::cerr << "Invalid date" << std::endl;
    }

    CropPtr crop = CropPtr(new Crop(cropId, name));
    crop->setSeedAndHarvestDate(sd, hd);
    crop->setCropParameters(getCropParametersFromMonicaDB(crop->id()));
    crop->setResidueParameters(getResidueParametersFromMonicaDB(crop->id()));

		if (permCropId > -1)
		{
			crop->setPerennialCropParameters(getCropParametersFromMonicaDB(permCropId));
		}

    ProductionProcess pp(name, crop);

		/* harvest */
		cson_array* harvArr = cson_value_get_array(cson_object_get(cropObj, "harvestOperations"));
		if (harvArr) { /* in case no intermediate harvest has been added */
			if (!addHarvestOps(pp, harvArr)) {
				ok = false;
				std::cerr << "Error adding harvest events" << std::endl;
			}
		}

    /* tillage */
    cson_array* tillArr = cson_value_get_array(cson_object_get(cropObj, "tillageOperations"));
    if (tillArr) { /* in case no tillage has been added */
      if (!addTillageOps(pp, tillArr)) {
        ok = false;
        std::cerr << "Error adding tillages" << std::endl;
      }
    }

    /* mineral fertilizer */
    cson_array* minFertArr = cson_value_get_array(cson_object_get(cropObj, "mineralFertilisers"));
    if (minFertArr) { /* in case no min fertilizer has been added */
      if (!addFertilizers(pp, minFertArr, false)) {
        ok = false;
        std::cerr << "Error adding mineral fertilisers" << std::endl;
      }
    }

    /* organic fertilizer */
    cson_array* orgFertArr = cson_value_get_array(cson_object_get(cropObj, "organicFertilisers"));
    if (orgFertArr) { /* in case no org fertilizer has been added */
      if (!addFertilizers(pp, orgFertArr, true)) {
        ok = false;
        std::cerr << "Error adding organic fertilisers" << std::endl;
      }
    }

    /* irrigations */
    cson_array* irriArr = cson_value_get_array(cson_object_get(cropObj, "irrigations"));
    if (irriArr) {  /* in case no irrigation has been added */
      if (!addIrrigations(pp, irriArr)) {
        ok = false;
        std::cerr << "Error adding irrigations" << std::endl;
      }
    }

    pps.push_back(pp);

    std::cout << "fetched crop " << c << ", name: " << name.c_str() << ", id: " << cropId << std::endl;
  }

  return ok;
}


bool Configuration::addHarvestOps(ProductionProcess &pp, cson_array* harvArr)
{
	bool ok = true;

	unsigned int hs = cson_array_length_get(harvArr);
	std::cout << "fetching " << hs << " harvests" << std::endl;
	unsigned int h;
	for (h = 0; h < hs; ++h) {
		cson_object* harvObj = cson_value_get_object(cson_array_get(harvArr, h));
		Tools::Date hDate = getIsoDate(harvObj, "date");
		double percentage = getDbl(harvObj, "percentage") / 100; // [%] -> [kg/kg]
		bool exported = getDbl(harvObj, "exported");
		std::string method = getStr(harvObj, "method");

		if (!hDate.isValid()) {
			ok = false;
			std::cerr << "Invalid harvest date" << method << std::endl;
		}

		Harvest h(hDate, pp.crop(), pp.cropResultPtr(), method);
		h.setPercentage(percentage);
		h.setExported(exported);
		pp.addApplication(h);

	}

	return ok;
}


bool Configuration::addTillageOps(ProductionProcess &pp, cson_array* tillArr)
{
  bool ok = true;

  unsigned int ts = cson_array_length_get(tillArr);
  std::cout << "fetching " << ts << " tillages" << std::endl;
  unsigned int t;
  for (t = 0; t < ts; ++t) {
    cson_object* tillObj = cson_value_get_object(cson_array_get(tillArr, t));
		Tools::Date tDate = getIsoDate(tillObj, "date");
    double depth = getDbl(tillObj, "depth") / 100; // TODO: cm -> m ????
    std::string method = "bla"; //getStr(tillObj, "method"))));

    if (!tDate.isValid()) {
      ok = false;
      std::cerr << "Invalid tillage date" << method << std::endl;
    }

    pp.addApplication(TillageApplication(tDate, depth));
  }

  return ok;
}

bool Configuration::addFertilizers(ProductionProcess &pp, cson_array* fertArr, bool isOrganic)
{
  // TODO: 
  /*
  //get data parsed and to use leap years if the crop rotation uses them
  Date fDateate = parseDate(sfDateate).toDate(it->crop()->seedDate().useLeapYears());

  if (!fDateate.isValid())
  {
    std::cout << "Error - Invalid date in \"" << pathToFile.c_str() << "\"" << endl;
    std::cout << "Line: " << s.c_str() << endl;
    ok = false;
  }

 //if the currently read fertiliser date is after the current end
  //of the crop, move as long through the crop rotation as
  //we find an end date that lies after the currently read fertiliser date
  while (fDateate > currentEnd)
  {
    //move to next crop and possibly exit at the end
    it++;
    if (it == cr.end())
      break;

    currentEnd = it->end();

    //cout << "new PP start: " << it->start().toString()
    //<< " new PP end: " << it->end().toString() << endl;
    //cout << "new currentEnd: " << currentEnd.toString() << endl;
  }
  */
  bool ok = true;

  Db::DB *con = Db::newConnection("monica");
  unsigned int fs = cson_array_length_get(fertArr);
  std::cout << "fetching " << fs << " fertilizers" << std::endl;
  unsigned int f;
  for (f = 0; f < fs; ++f) {
    cson_object* fertObj = cson_value_get_object(cson_array_get(fertArr, f));
    Tools::Date fDate = getIsoDate(fertObj, "date");
    std::string method = getStr(fertObj, "method");
    int fertId = getInt(fertObj, "type.id");
    std::string fertName = isOrganic ? getStr(fertObj, "type.om_type") : getStr(fertObj, "type.name");
    double amount = getDbl(fertObj, "amount");

    if (!fDate.isValid()) {
      ok = false;
      std::cerr << "Invalid fertilization date" << fertName << method << std::endl;
    }

    if (isOrganic)  {
      if (con && con->select(
        "SELECT om_Type, dm, nh4_n, no3_n, nh2_n, k_slow, k_fast, part_s, part_f, cn_s, cn_f, smb_s, smb_f, id "
        "FROM organic_fertiliser "
        "WHERE id=" + std::to_string(fertId)
      )) {
        Db::DBRow row = con->getRow();
        if (!(row).empty()) {
          OrganicMatterParameters omp;

          omp.name = row[0];
          omp.vo_AOM_DryMatterContent = Tools::satof(row[1]);
          omp.vo_AOM_NH4Content = Tools::satof(row[2]);
          omp.vo_AOM_NO3Content = Tools::satof(row[3]);
          omp.vo_AOM_CarbamidContent = Tools::satof(row[4]);
          omp.vo_AOM_SlowDecCoeffStandard = Tools::satof(row[5]);
          omp.vo_AOM_FastDecCoeffStandard = Tools::satof(row[6]);
          omp.vo_PartAOM_to_AOM_Slow = Tools::satof(row[7]);
          omp.vo_PartAOM_to_AOM_Fast = Tools::satof(row[8]);
          omp.vo_CN_Ratio_AOM_Slow = Tools::satof(row[9]);
          omp.vo_CN_Ratio_AOM_Fast = Tools::satof(row[10]);
          omp.vo_PartAOM_Slow_to_SMB_Slow = Tools::satof(row[11]);
          omp.vo_PartAOM_Slow_to_SMB_Fast = Tools::satof(row[12]);

          pp.addApplication(OrganicFertiliserApplication(fDate, new OrganicMatterParameters(omp), amount, true));
        }
        else {
          ok = false;
          std::cerr << "Invalid organic fertilizer type" << fertName << method << std::endl;        
        }
      }
    }
    else { // not organic
      if (con && con->select(
        "SELECT id, name, no3, nh4, carbamid " 
        "FROM mineral_fertilisers "
        "WHERE id=" + std::to_string(fertId)
      )) {
        Db::DBRow row = con->getRow();
        if (!(row).empty()) {
          //int id = Tools::satoi(row[0]);
          std::string name = row[1];
          double no3 = Tools::satof(row[2]);
          double nh4 = Tools::satof(row[3]);
          double carbamid = Tools::satof(row[4]);

          MineralFertiliserParameters mfp(name, carbamid, no3, nh4);
          pp.addApplication(MineralFertiliserApplication(fDate, mfp, amount));
        }
        else {
          ok = false;
          std::cerr << "Invalid mineral fertilizer type" << fertName << method << std::endl;        
        }
      }
    }
  }
   
  return ok;
}

bool Configuration::addIrrigations(ProductionProcess &pp, cson_array* irriArr)
{
  bool ok = true;

  // TODO:
  //get data parsed and to use leap years if the crop rotation uses them
  /*Date idate = parseDate(irrDate).toDate(it->crop()->seedDate().useLeapYears());
  if (!idate.isValid())
  {
    std::cout << "Error - Invalid date in \"" << pathToFile.c_str() << "\"" << endl;
    std::cout << "Line: " << s.c_str() << endl;
    std::cout << "Aborting simulation now!" << endl;
    exit(-1);
  }

  //cout << "PP start: " << it->start().toString()
  //<< " PP end: " << it->end().toString() << endl;
  //cout << "irrigationDate: " << idate.toString()
  //<< " currentEnd: " << currentEnd.toString() << endl;

  //if the currently read irrigation date is after the current end
  //of the crop, move as long through the crop rotation as
  //we find an end date that lies after the currently read irrigation date
  while (idate > currentEnd)
  {
    //move to next crop and possibly exit at the end
    it++;
    if (it == cr.end())
      break;

    currentEnd = it->end();

    //cout << "new PP start: " << it->start().toString()
    //<< " new PP end: " << it->end().toString() << endl;
    //cout << "new currentEnd: " << currentEnd.toString() << endl;
  }*/

  unsigned int is = cson_array_length_get(irriArr);
  std::cout << "fetching " << is << " irrigations" << std::endl;
  unsigned int i;
  for (i = 0; i < is; ++i) {
    cson_object* irriObj = cson_value_get_object(cson_array_get(irriArr, i));
    std::string method = getStr(irriObj, "method");
    std::string eventType = getStr(irriObj, "eventType");
    double threshold = getDbl(irriObj, "threshold");
    double area = getDbl(irriObj, "area");
    double amount = getDbl(irriObj, "amount");
    double NConc = getDbl(irriObj, "NConc");
    // TODO: eigentlich kein date in JSON vorgesehen
    Tools::Date iDate = getIsoDate(irriObj, "date");

    if (!iDate.isValid()) {
      ok = false;
      std::cerr << "Invalid irrigation date" << method << eventType << std::endl;
    }

    pp.addApplication(IrrigationApplication(iDate, amount, IrrigationParameters(NConc, 0.0)));
  }

  return ok;
}

bool Configuration::createClimate(Climate::DataAccessor &da, CentralParameterProvider &cpp, double latitude, bool useLeapYears)
{
  bool ok = true;
  std::string pathToFile = _dirNameMet + pathSeparator() + _preMetFiles;

  std::vector<double> _tmin;
  std::vector<double> _tavg;
  std::vector<double> _tmax;
  std::vector<double> _globrad;
  std::vector<double> _relhumid;
  std::vector<double> _wind;
  std::vector<double> _precip;
  std::vector<double> _sunhours;

  Tools::Date date = Tools::Date(1, 1, da.startDate().year(), useLeapYears);

  for (int y = da.startDate().year(); y <= da.endDate().year(); y++) {
    std::ostringstream yss;
    yss << y;
    std::string ys = yss.str();
    std::ostringstream oss;
    oss << pathToFile << ys.substr(1, 3);
    std::cout << "File: " << oss.str().c_str() << std::endl;
    std::ifstream ifs(oss.str().c_str(), std::ios::binary);
    if (! ifs.good()) {
      std::cerr << "Could not open file " << oss.str().c_str() << " . Aborting now!" << std::endl;
      ok = false;
    }
    if (ok) {
      std::string s;

      //skip first line(s)
      getline(ifs, s);
      getline(ifs, s);
      getline(ifs, s);

      int daysCount = 0;
      int allowedDays = Tools::Date(31, 12, y, useLeapYears).dayOfYear();
      //    cout << "tavg\t" << "tmin\t" << "tmax\t" << "wind\t"
      std::cout << "allowedDays: " << allowedDays << " " << y<< "\t" << useLeapYears << "\tlatitude:\t" << latitude << std::endl;
      //<< "sunhours\t" << "globrad\t" << "precip\t" << "ti\t" << "relhumid\n";
      while (getline(ifs, s)) {

        //Tp_av Tpmin Tpmax T_s10 T_s20 vappd wind sundu radia prec jday RF
        double td;
        int ti;
        double tmin, tmax, tavg, wind, sunhours, globrad, precip, relhumid;
        std::istringstream ss(s);

        ss >> tavg >> tmin >> tmax >> td >> td >> td >> wind
            >> sunhours >> globrad >> precip >> ti >> relhumid;

        // test if globrad or sunhours should be used
        if(globrad >=0.0) {
          // use globrad
          // HERMES weather files deliver global radiation as [J cm-2]
          // Here, we push back [MJ m-2 d-1]
          double globradMJpm2pd = globrad * 100.0 * 100.0 / 1000000.0;
          _globrad.push_back(globradMJpm2pd);
        } 
        else if(sunhours >= 0.0) {
          // invalid globrad use sunhours
          // convert sunhours into globrad
          // std::cout << "Invalid globrad - use sunhours instead" << endl;
          _globrad.push_back(Tools::sunshine2globalRadiation(date.dayOfYear(), sunhours, latitude, true));    
          _sunhours.push_back(sunhours);
        } 
        else {
          // error case
          std::cerr << "Error: No global radiation or sunhours specified for day " << date.toString().c_str() << std::endl;
          ok = false;
        }

        if (relhumid>=0.0)
            _relhumid.push_back(relhumid);
                
        // precipitation correction by Richter values
        precip*=cpp.getPrecipCorrectionValue(date.month()-1);

        //      cout << tavg << "\t"<< tmin << "\t" << tmax << "\t" << wind
        //<< "\t" << sunhours <<"\t" << globrad <<"\t" << precip <<"\t" << ti <<"\t" << relhumid << endl;

        _tavg.push_back(tavg);
        _tmin.push_back(tmin);
        _tmax.push_back(tmax);
        _wind.push_back(wind);
        _precip.push_back(precip);
        
        daysCount++;
        date++;
      }

      if (daysCount != allowedDays) {
        std::cerr << "Wrong number of days in " << oss.str().c_str() << " ." << " Found " << daysCount << " days but should have been ";
        ok = false;
      }
    }
  }

  da.addClimateData(Climate::tmin, _tmin);
  da.addClimateData(Climate::tmax, _tmax);
  da.addClimateData(Climate::tavg, _tavg);
  da.addClimateData(Climate::globrad, _globrad);
  da.addClimateData(Climate::wind, _wind);
  da.addClimateData(Climate::precip, _precip);

  if(!_sunhours.empty())
    da.addClimateData(Climate::sunhours, _sunhours);

  if (!_relhumid.empty())
      da.addClimateData(Climate::relhumid, _relhumid);

  return ok;
}

void Configuration::freeCson()
{
  if (_sim)
    cson_value_free(_sim);
  if (_site)
    cson_value_free(_site);
  if (_crop)
    cson_value_free(_crop);

  _sim = NULL;
  _site = NULL;
  _crop = NULL;
}

int Configuration::readJSON(FILE* file, cson_value** val)
{
  assert(file);
  cson_parse_info info = cson_parse_info_empty;
  int rc;

  rc = cson_parse_FILE(val, file, NULL, &info);
  if( 0 != rc )
    printCsonError(rc, info);

  return rc;
}

int Configuration::readJSON(const std::string& str, cson_value** val)
{
  cson_parse_info info = cson_parse_info_empty;
  int rc;

  rc = cson_parse_string(val, str.c_str(), str.length(), NULL, &info);
  if( 0 != rc )
    printCsonError(rc, info);

  return rc;
}

int Configuration::writeJSON(FILE* file, const cson_value* val)
{
  cson_output_opt opt;
  opt.addNewline = 1;
  opt.indentation = 2;
  opt.addSpaceAfterColon = 1;
  opt.maxDepth = 25;
  return cson_output_FILE(val, file, &opt);
}

int Configuration::printJSON(const cson_value* val, std::string *str)
{
  cson_output_opt opt;
  opt.addNewline = 1;
  opt.indentation = 2;
  opt.addSpaceAfterColon = 1;
  opt.maxDepth = 25;
  int rc = 0;

  if (!str) {
    std::string json = "";
    rc = cson_output(val, cson_data_dest_string, (void*)&json, &opt);
    std::cout << json << std::endl;
  }
  else
    rc = cson_output(val, cson_data_dest_string, (void*)str, &opt);

  return rc;
}

bool Configuration::isValid(const cson_value* val, const cson_value* meta, const std::string& path)
{
  /* TODO: check min, max, type? */
  bool ok = true;

  if (cson_value_is_object(meta)) {
    
    cson_object* valObj = cson_value_get_object(val);
    cson_object* metaObj = cson_value_get_object(meta);
    
    if (!valObj || !metaObj) {
      std::cout << "valObj: " << valObj << " metaObj: " << metaObj << path << std::endl;
      return false;
    }

    cson_object_iterator iter;
    int rc = cson_object_iter_init(metaObj, &iter);
    cson_kvp* kvp;
    while ((kvp = cson_object_iter_next(&iter))) {
      cson_string const* key = cson_kvp_key(kvp);
      std::cout << "meta key: " << cson_string_cstr(key) << std::endl;
      cson_value* metaVal = cson_kvp_value(kvp);
      std::string valPath = path + "." + std::string(cson_string_cstr(key));
      /* check if key exists */
      if (!cson_value_is_array(metaVal) && !cson_value_is_object(metaVal)) {
        if (cson_object_get(valObj, cson_string_cstr(key)) == NULL) {
          std::cerr << "key \"" << path << "." << cson_string_cstr(key) << "\"" << " does not exist" << std::endl;
          return false;
        }
      }
      else if (cson_value_is_array(metaVal)) {
        std::cout << "meta key is array: " << cson_string_cstr(key) << std::endl;
        ok = isValid(cson_object_get(valObj, cson_string_cstr(key)), metaVal, valPath);
        if (!ok)
          return ok;
      } /* TODO: Umweg um unterste Ebene zu finden. Immer Objekte...*/
      else if (cson_value_is_object(metaVal) && cson_object_get(cson_value_get_object(metaVal), "desc") != NULL) {
        if (cson_object_get(valObj, cson_string_cstr(key)) == NULL) {
          std::cerr << "key \"" << path << "." << cson_string_cstr(key) << "\"" << " does not exist" << std::endl;
          return false;
        }
      }
      else if (cson_value_is_object(metaVal)) {
        std::cout << "meta key is object: " << cson_string_cstr(key) << std::endl;
        ok = isValid(cson_object_get(valObj, cson_string_cstr(key)), metaVal, valPath);
        if (!ok)
          return ok;
      }
    }
  }
  else if (cson_value_is_array(meta)) {
    
    cson_array* valArr = cson_value_get_array(val);
    cson_array* metaArr = cson_value_get_array(meta);

    /* missing array allowed? */
    if (/*!valArr || */!metaArr) {
      std::cout << "valArr: " << valArr << " metaArr: " << metaArr << path << std::endl;
      return false;
    }

    /* empty array allowed */
    unsigned int len = cson_array_length_get(valArr);
    if (len == 0)
      ok = true;
    else if (cson_array_length_get(metaArr) > 0) {
      cson_value* metaVal = cson_array_get(metaArr, 0);
      /* check objects in array */
      for (int i = 0; i < len; i++) {
        ok = isValid(cson_array_get(valArr, i), metaVal, path + '[' + std::to_string(static_cast<long long>(i) ) + ']');
        if (!ok) {
          break;
        }
      }
    }
  } 

  return ok;      
}

bool Configuration::getBool(const cson_object* obj, const std::string& path, bool def)
{
	auto value = path.find('.') < std::string::npos
		? cson_object_get_sub(obj, path.c_str(), '.')
		: cson_object_get(obj, path.c_str());

	return (!cson_value_is_null(value) && value) ? cson_value_get_bool(value) : def;
}

int Configuration::getInt(const cson_object* obj, const std::string& path, int def)
{
	auto value = path.find('.') < std::string::npos
		? cson_object_get_sub(obj, path.c_str(), '.')
		: cson_object_get(obj, path.c_str());

	return (!cson_value_is_null(value) && value) ? cson_value_get_integer(value) : def;
}

double Configuration::getDbl(const cson_object* obj, const std::string& path, double def)
{
	auto value = path.find('.') < std::string::npos
		? cson_object_get_sub(obj, path.c_str(), '.')
		: cson_object_get(obj, path.c_str());

	return (!cson_value_is_null(value) && value) ? cson_value_get_double(value) : def;
}

std::string Configuration::getStr(const cson_object* obj, const std::string& path, std::string def)
{
	auto value = path.find('.') < std::string::npos
		? cson_object_get_sub(obj, path.c_str(), '.')
		: cson_object_get(obj, path.c_str());

	return (!cson_value_is_null(value) && value) ? std::string(cson_string_cstr(cson_value_get_string(value))) : def;
}

Tools::Date Configuration::getIsoDate(const cson_object* obj, const std::string& path, Tools::Date def)
{
	auto value = path.find('.') < std::string::npos
		? cson_object_get_sub(obj, path.c_str(), '.')
		: cson_object_get(obj, path.c_str());

	return (!cson_value_is_null(value) && value) ? Tools::fromMysqlString(getStr(obj, path)) : def;
}

bool Configuration::isNull(const cson_object* obj, const std::string& path)
{
  if (path.find('.') < std::string::npos)
    return bool(cson_value_is_null(cson_object_get_sub(obj, path.c_str(), '.')));
  else
    return bool(cson_value_is_null(cson_object_get(obj, path.c_str())));
}

void Configuration::printCsonError(int rc, const cson_parse_info &info)
{
  std::cerr << boost::format("JSON parse error, code=%1% (%2%), at line %3%, column %4%.") %
          info.errorCode % cson_rc_string(rc) % info.line % info.col << std::endl;
}
