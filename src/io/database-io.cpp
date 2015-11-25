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

#include <iostream>
#include <mutex>
#include <memory>
#include <tuple>

#include "tools/debug.h"
#include "db/abstract-db-connections.h"
#include "soil/soil.h"
#include "tools/algorithms.h"
#include "../run/run-monica.h"
#include "../core/monica-typedefs.h"
#include "tools/helper.h"

#include "database-io.h"

using namespace Monica;
using namespace Tools;
using namespace Db;
using namespace std;

namespace
{
  string speciesSelect(const string& species)
  {
    return string()
        + "SELECT "
          "id, "
          "carboxylation_pathway, "
          "minimum_temperature_for_assimilation, "
          "min_n_content, "
          "n_content_pn, "
          "n_content_b0, "
          "n_content_above_ground_biomass, "
          "n_content_root, "
          "initial_kc_factor, "
          "development_acceleration_by_nitrogen_stress, "
          "fixing_n, "
          "luxury_n_coeff, "
          "sampling_depth, "
          "target_n_sampling_depth, "
          "target_n30, "
          "default_radiation_use_efficiency, "
          "stage_at_max_height, "
          "max_stem_diameter, "
          "stage_at_max_diameter, "
          "max_N_uptake_p, "
          "root_distribution_p, "
          "plant_density, "
          "root_growth_lag, "
          "min_temperature_root_growth, "
          "initial_rooting_depth, "
          "root_penetration_rate, "
          "root_form_factor, "
          "specific_root_length, "
          "stage_after_cut, "
          "lim_temperature_heat_stress, "
          "drought_impact_on_fertility_factor, "
          "cutting_delay_days, "
          "field_condition_modifier, "
          "assimilate_reallocation "
          "FROM species "
          "WHERE id = '" + species + "'";
  }

  string cultivarSelect(const string& species, const string& cultivar)
  {
    return string()
        + "SELECT "
          "crop_id, "
          "species_id, "
          "id, "
          "description, "
          "perennial, "
          "permanent_cultivar_id, "
          "max_assimilation_rate, "
          "max_crop_height, "
          "crop_height_P1, "
          "crop_height_P2, "
          "crop_specific_max_rooting_depth, "
          "residue_n_ratio, "
          "heat_sum_irrigation_start, "
          "heat_sum_irrigation_end, "
          "crit_temperature_heat_stress, "
          "begin_sensitive_phase_heat_stress, "
          "end_sensitive_phase_heat_stress, "
          "LT50cultivar, "
          "frost_hardening, "
          "frost_dehardening, "
          "low_temperature_exposure, "
          "respiratory_stress, "
          "latest_harvest_doy "
          "FROM cultivar "
          "WHERE species_id = '" + species + "' and id = '" + cultivar + "'";
  }

  string organSelect(const string& species = string())
  {
    return string()
        + "SELECT "
          "species_id, "
          "id, "
          "initial_organ_biomass, "
          "organ_maintainance_respiration, "
          "is_above_ground, "
          "organ_growth_respiration, "
          "is_storage_organ "
          "FROM organ "
        + (species.empty() ? "" : string("WHERE species_id = '") + species + "' ")
        + "ORDER BY species_id, id";
  }

  string devStageSpeciesSelect(const string& species = string())
  {
    return string()
        + "SELECT "
          "species_id, "
          "id, "
          "base_temperature, "
          "critical_oxygen_content, "
          "stage_max_root_n_content "
          "FROM dev_stage_species "
        + (species.empty() ? " " : string("WHERE species_id = '") + species + "' ")
        + "ORDER BY species_id, id";
  }

  string devStageCultivarSelect(int cropId = -1)
  {
    return string()
        + "SELECT "
          "crop_id, "
          "id, "
          "stage_temperature_sum, "
          "opt_temperature, "
          "vernalisation_requirement, "
          "day_length_requirement, "
          "base_day_length, "
          "drought_stress_threshold, "
          "specific_leaf_area, "
          "stage_kc_factor "
          "FROM dev_stage_cultivar "
        + (cropId == -1 ? "" : string("WHERE crop_id = ") + to_string(cropId) + " ")
        + "ORDER BY crop_id, id";
  }

  string odsDepParamsSelect(int cropId = -1)
  {
    return string()
        + "select "
          "crop_id, "
          "organ_id, "
          "dev_stage_id, "
          "ods_dependent_param_id, "
          "value "
          "from crop_2_ods_dependent_param "
        + (cropId == -1 ? " " : string("where crop_id = ") + to_string(cropId) + " ")
        + "order by crop_id, organ_id desc, dev_stage_id desc, ods_dependent_param_id";
  }

  string yieldPartsSelect(int cropId = -1)
  {
    return string()
        + "SELECT "
          "crop_id, "
          "organ_id, "
          "is_primary, "
          "percentage, "
          "dry_matter "
          "FROM yield_parts "
        + (cropId == -1 ? "" : string("WHERE crop_id = ") + to_string(cropId) + " ")
        + "ORDER BY crop_id, organ_id";
  }

  string cuttingPartsSelect(int cropId = -1)
  {
    return string()
        + "SELECT "
          "crop_id, "
          "organ_id, "
          "is_primary, "
          "percentage, "
          "dry_matter "
          "FROM cutting_parts "
        + (cropId == -1 ? "" : string("WHERE crop_id = ") + to_string(cropId) + " ")
        + "ORDER BY crop_id, organ_id";
  }
}

SpeciesParametersPtr Monica::getSpeciesParametersFromMonicaDB(const string& species)
{
  SpeciesParametersPtr sps = make_shared<SpeciesParameters>();

  DBPtr con(newConnection("monica"));
  DBRow row;
  con->select(speciesSelect(species));
  debug() << speciesSelect(species) << endl;
  if(!(row = con->getRow()).empty())
  {
    int i = 0;

    sps->pc_SpeciesId = row[i++];
    sps->pc_CarboxylationPathway = stoi(row[i++]);
    sps->pc_MinimumTemperatureForAssimilation = stof(row[i++]);
    sps->pc_MinimumNConcentration = stof(row[i++]);
    sps->pc_NConcentrationPN = stof(row[i++]);
    sps->pc_NConcentrationB0 = stof(row[i++]);
    sps->pc_NConcentrationAbovegroundBiomass = stof(row[i++]);
    sps->pc_NConcentrationRoot = stof(row[i++]);
    sps->pc_InitialKcFactor = stof(row[i++]);
    sps->pc_DevelopmentAccelerationByNitrogenStress = stoi(row[i++]);
    sps->pc_PartBiologicalNFixation = stof(row[i++]);
    sps->pc_LuxuryNCoeff = stof(row[i++]);
    sps->pc_SamplingDepth = stof(row[i++]);
    sps->pc_TargetNSamplingDepth = stof(row[i++]);
    sps->pc_TargetN30 = stof(row[i++]);
    sps->pc_DefaultRadiationUseEfficiency = stof(row[i++]);
    sps->pc_StageAtMaxHeight = stof(row[i++]);
    sps->pc_MaxCropDiameter = stof(row[i++]);
    sps->pc_StageAtMaxDiameter = stof(row[i++]);
    sps->pc_MaxNUptakeParam = stof(row[i++]);
    sps->pc_RootDistributionParam = stof(row[i++]);
    sps->pc_PlantDensity = stof(row[i++]);
    sps->pc_RootGrowthLag = stof(row[i++]);
    sps->pc_MinimumTemperatureRootGrowth = stof(row[i++]);
    sps->pc_InitialRootingDepth = stof(row[i++]);
    sps->pc_RootPenetrationRate = stof(row[i++]);
    sps->pc_RootFormFactor = stof(row[i++]);
    sps->pc_SpecificRootLength = stof(row[i++]);
    sps->pc_StageAfterCut = stoi(row[i++]);
    sps->pc_LimitingTemperatureHeatStress = stof(row[i++]);
    sps->pc_DroughtImpactOnFertilityFactor = stof(row[i++]);
    sps->pc_CuttingDelayDays = stoi(row[i++]);
    sps->pc_FieldConditionModifier = stof(row[i++]);
    sps->pc_AssimilateReallocation = stof(row[i++]);
  }

  con->select(organSelect(species));
  debug() << organSelect(species) << endl;
  while (!(row = con->getRow()).empty())
  {
    sps->pc_InitialOrganBiomass.push_back(stod(row[2]));
    sps->pc_OrganMaintenanceRespiration.push_back(stod(row[3]));
    sps->pc_AbovegroundOrgan.push_back(stob(row[4]));
    sps->pc_OrganGrowthRespiration.push_back(stod(row[5]));
    sps->pc_StorageOrgan.push_back(stob(row[6]));
  }

  con->select(devStageSpeciesSelect(species));
  debug() << devStageSpeciesSelect(species) << endl;

  while (!(row = con->getRow()).empty())
  {
    sps->pc_BaseTemperature.push_back(stod(row[2]));
    sps->pc_CriticalOxygenContent.push_back(stod(row[3]));
    sps->pc_StageMaxRootNConcentration.push_back(stod(row[4]));
  }

  return sps;
}

CultivarParametersPtr Monica::getCultivarParametersFromMonicaDB(const string& species, const string& cultivar)
{
  CultivarParametersPtr cps = make_shared<CultivarParameters>();

  int cropId = -1;

  DBPtr con(newConnection("monica"));
  DBRow row;
  con->select(cultivarSelect(species, cultivar));
  debug() << cultivarSelect(species, cultivar) << endl;
  if(!(row = con->getRow()).empty())
  {
    int i = 0;

    cropId = stoi(row[i++]);
    i++;
    cps->pc_CultivarId = row[i++];
    cps->pc_Description = row[i++];
    cps->pc_Perennial = stob(row[i++]);
    cps->pc_PermanentCultivarId = row[i++];
    cps->pc_MaxAssimilationRate = stof(row[i++]);
    cps->pc_MaxCropHeight = stof(row[i++]);
    cps->pc_CropHeightP1 = stof(row[i++]);
    cps->pc_CropHeightP2 = stof(row[i++]);
    cps->pc_CropSpecificMaxRootingDepth = stof(row[i++]);
    cps->pc_ResidueNRatio = stof(row[i++]);
    cps->pc_HeatSumIrrigationStart = stof(row[i++]);
    cps->pc_HeatSumIrrigationEnd = stof(row[i++]);
    cps->pc_CriticalTemperatureHeatStress = stof(row[i++]);
    cps->pc_BeginSensitivePhaseHeatStress = stof(row[i++]);
    cps->pc_EndSensitivePhaseHeatStress = stof(row[i++]);
    cps->pc_LT50cultivar = stof(row[i++]);
    cps->pc_FrostHardening = stof(row[i++]);
    cps->pc_FrostDehardening = stof(row[i++]);
    cps->pc_LowTemperatureExposure = stof(row[i++]);
    cps->pc_RespiratoryStress = stof(row[i++]);
    cps->pc_LatestHarvestDoy = stoi(row[i++]);
  }

  con->select(devStageCultivarSelect(cropId));
  debug() << devStageCultivarSelect(cropId) << endl;
  while (!(row = con->getRow()).empty())
  {
    int i = 2;

    cps->pc_StageTemperatureSum.push_back(stod(row[i++]));
    cps->pc_OptimumTemperature.push_back(stod(row[i++]));
    cps->pc_VernalisationRequirement.push_back(stod(row[i++]));
    cps->pc_DaylengthRequirement.push_back(stod(row[i++]));
    cps->pc_BaseDaylength.push_back(stod(row[i++]));
    cps->pc_DroughtStressThreshold.push_back(stod(row[i++]));
    cps->pc_SpecificLeafArea.push_back(stod(row[i++]));
    cps->pc_StageKcFactor.push_back(stod(row[i++]));
  }

  con->select(odsDepParamsSelect(cropId));
  debug() << odsDepParamsSelect(cropId) << endl;
  while (!(row = con->getRow()).empty())
  {
    size_t organId = stoi(row[1]);
    size_t devStageId = stoi(row[2]);

    auto& sov = stoi(row[3]) == 1 ? cps->pc_AssimilatePartitioningCoeff : cps->pc_OrganSenescenceRate;

    if(sov.size() < devStageId)
      sov.resize(devStageId);
    auto& ds = sov[devStageId - 1];

    if(ds.size() < organId)
      ds.resize(organId);

    ds[organId - 1] = stod(row[4]);
  }

  cps->pc_OrganIdsForPrimaryYield.clear();
  cps->pc_OrganIdsForSecondaryYield.clear();
  con->select(yieldPartsSelect(cropId));
  debug() << yieldPartsSelect(cropId) << endl;
  while(!(row = con->getRow()).empty())
  {
    bool isPrimary = stob(row[2]);

    YieldComponent yc;
    yc.organId = stoi(row[1]);
    yc.yieldPercentage = stod(row[3]) / 100.0;
    yc.yieldDryMatter = stod(row[4]);

    // normal case, uses yield partitioning from crop database
    if(isPrimary)
      cps->pc_OrganIdsForPrimaryYield.push_back(yc);
    else
      cps->pc_OrganIdsForSecondaryYield.push_back(yc);
  }

  // get cutting parts if there are some data available
  cps->pc_OrganIdsForCutting.clear();
  con->select(cuttingPartsSelect(cropId));
  while(!(row = con->getRow()).empty())
  {
    YieldComponent yc;
    yc.organId = stoi(row[1]);
    //bool isPrimary = stoi(row[2]) == 1;
    yc.yieldPercentage = stof(row[3]) / 100.0;
    yc.yieldDryMatter = stof(row[4]);

    cps->pc_OrganIdsForCutting.push_back(yc);
  }

  return cps;
}

CropParametersPtr Monica::getCropParametersFromMonicaDB(const string& species,
                                                        const string& cultivar)
{
  CropParametersPtr cps = make_shared<CropParameters>();
  cps->speciesParams = *getSpeciesParametersFromMonicaDB(species);
  cps->cultivarParams = *getCultivarParametersFromMonicaDB(species, cultivar);
  return cps;
}

const map<int, pair<SpeciesParametersPtr, CultivarParametersPtr>>&
getAllCropParametersFromMonicaDB()
{
  static mutex lockable;

  static bool initialized = false;
  typedef map<int, pair<SpeciesParametersPtr, CultivarParametersPtr>> CPS;

  static CPS cpss;

  // only initialize once
  if (!initialized)
  {
    lock_guard<mutex> lock(lockable);

    //test if after waiting for the lock the other thread
    //already initialized the whole thing
    if (!initialized)
    {
      DBPtr con(newConnection("monica"));
      DBRow row;
      con->select("select crop_id, species_id, id from cultivar order by crop_id");
      while(!(row = con->getRow()).empty())
      {
        int cropId = stoi(row[0]);
        string speciesId = row[1];
        string cultivarId = row[2];

        cpss[cropId] = make_pair(getSpeciesParametersFromMonicaDB(speciesId),
                                 getCultivarParametersFromMonicaDB(speciesId, cultivarId));
      }

      initialized = true;
    }
  }

  return cpss;
}

CropParametersPtr Monica::getCropParametersFromMonicaDB(int cropId)
{
  static CropParametersPtr nothing = make_shared<CropParameters>();

  auto m = getAllCropParametersFromMonicaDB();
  auto ci = m.find(cropId);
  if(ci != m.end())
  {
    CropParametersPtr cps = make_shared<CropParameters>();
    cps->speciesParams = *ci->second.first;
    cps->cultivarParams = *ci->second.second;

    return cps;
  }

  return nothing;
}

void Monica::writeCropParameters(string path)
{
	for(auto p : availableMonicaCrops())
	{
    CropParametersPtr cp = getCropParametersFromMonicaDB(p.first);

		ofstream ofs;
    string speciesDir = path + "/" + cp->speciesParams.pc_SpeciesId;
    ensureDirExists(surround("\"", speciesDir));

		auto s = fixSystemSeparator(path + "/" + cp->speciesParams.pc_SpeciesId + ".json");
    ofs.open(s);
    if(ofs.good())
		{
      auto s = cp->speciesParams.to_json().dump();
      ofs << s;
			ofs.close();
		}
		auto c = fixSystemSeparator(speciesDir + "/" + cp->cultivarParams.pc_CultivarId + ".json");
    ofs.open(c);
    if(ofs.good())
    {
      auto s = cp->cultivarParams.to_json().dump();
      ofs << s;
      ofs.close();
    }
	}
}

//------------------------------------------------------------------------------

const map<string, MineralFertiliserParameters>& getAllMineralFertiliserParametersFromMonicaDB()
{
	static mutex lockable;
	static bool initialized = false;
  static map<string, MineralFertiliserParameters> m;

	if (!initialized)
	{
		lock_guard<mutex> lock(lockable);

		if (!initialized)
		{
      DBPtr con(newConnection("monica"));
			DBRow row;
      con->select("select id, name, no3, nh4, carbamid from mineral_fertiliser");
			while (!(row = con->getRow()).empty())
			{
        string id = row[0];
				string name = row[1];
				double no3 = satof(row[2]);
				double nh4 = satof(row[3]);
				double carbamid = satof(row[4]);

        m[id] = MineralFertiliserParameters(id, name, carbamid, no3, nh4);
			}

			initialized = true;
		}
	}

	return m;
}

/**
* @brief Reads mineral fertiliser parameters from monica DB
* @param id of the fertiliser
* @return mineral fertiliser parameters value object with values from database
*/
MineralFertiliserParameters Monica::getMineralFertiliserParametersFromMonicaDB(const std::string& id)
{
  auto m = getAllMineralFertiliserParametersFromMonicaDB();
	auto ci = m.find(id);
	return ci != m.end() ? ci->second : MineralFertiliserParameters();
}

void Monica::writeMineralFertilisers(string path)
{
	for (auto p : getAllMineralFertiliserParametersFromMonicaDB())
	{
		auto mf = p.second;

		ensureDirExists(surround("\"", path));

		ofstream ofs;
		ofs.open(path + "/" + mf.getId() + ".json");

    if(ofs.good())
		{
      auto s = mf.to_json().dump();
      //cout << "id: " << mf.getId() << " name: " << mf.getName() << " ----> " << endl << s << endl;
      ofs << s;
			ofs.close();
		}
	}
}

//--------------------------------------------------------------------------------------

const map<string, OrganicFertiliserParametersPtr>& getAllOrganicFertiliserParametersFromMonicaDB()
{
	static mutex lockable;
	static bool initialized = false;
  typedef map<string, OrganicFertiliserParametersPtr> Map;
	static Map m;

	if (!initialized)
	{
		lock_guard<mutex> lock(lockable);

		if (!initialized)
		{
			DBPtr con(newConnection("monica"));
			DBRow row;
      con->select(
            "select "
            "id, "
            "name, "
            "dm, "
            "nh4_n, "
            "no3_n, "
            "nh2_n, "
            "k_slow, "
            "k_fast, "
            "part_s, "
            "part_f, "
            "cn_s, "
            "cn_f, "
            "smb_s, "
            "smb_f "
            "from organic_fertiliser");
			while (!(row = con->getRow()).empty())
			{
        OrganicFertiliserParametersPtr omp = make_shared<OrganicFertiliserParameters>();

        int i = 0;

        omp->id = row[i++];
        omp->name = row[i++];
        omp->vo_AOM_DryMatterContent = stof(row[i++]);
        omp->vo_AOM_NH4Content = stof(row[i++]);
        omp->vo_AOM_NO3Content = stof(row[i++]);
        omp->vo_AOM_CarbamidContent = stof(row[i++]);
        omp->vo_AOM_SlowDecCoeffStandard = stof(row[i++]);
        omp->vo_AOM_FastDecCoeffStandard = stof(row[i++]);
        omp->vo_PartAOM_to_AOM_Slow = stof(row[i++]);
        omp->vo_PartAOM_to_AOM_Fast = stof(row[i++]);
        omp->vo_CN_Ratio_AOM_Slow = stof(row[i++]);
        omp->vo_CN_Ratio_AOM_Fast = stof(row[i++]);
        omp->vo_PartAOM_Slow_to_SMB_Slow = stof(row[i++]);
        omp->vo_PartAOM_Slow_to_SMB_Fast = stof(row[i++]);

        m[omp->id] = omp;
			}

			initialized = true;
		}
	}

	return m;
}

/**
* @brief Reads organic fertiliser parameters from monica DB
* @param organ_fert_id ID of fertiliser
* @return organic fertiliser parameters with values from database
*/
OrganicFertiliserParametersPtr Monica::getOrganicFertiliserParametersFromMonicaDB(const std::string& id)
{
  static OrganicFertiliserParametersPtr nothing = make_shared<OrganicFertiliserParameters>();

	auto m = getAllOrganicFertiliserParametersFromMonicaDB();
	auto ci = m.find(id);
  return ci != m.end() ? ci->second : nothing;
}

void Monica::writeOrganicFertilisers(string path)
{
	for (auto p : getAllOrganicFertiliserParametersFromMonicaDB())
	{
    OrganicFertiliserParametersPtr of = p.second;

		ensureDirExists(surround("\"", path));

		ofstream ofs;
		ofs.open(path + "/" + of->id + ".json");

		if (ofs.good())
		{
			auto s = of->to_json().dump();
			//cout << "id: " << of->id << " name: " << of->name << " ----> " << endl << s << endl;
			ofs << s;
			ofs.close();
		}
	}
}


//--------------------------------------------------------------------------------------

CropResidueParametersPtr
Monica::getResidueParametersFromMonicaDB(const string& species,
                                         const string& residueType)
{
	DBPtr con(newConnection("monica"));
	DBRow row;
	string query = string() +
                 "select "
                 "species_id, "
                 "residue_type, "
                 "dm, "
                 "nh4, "
                 "no3, "
                 "nh2, "
                 "k_slow, "
                 "k_fast, "
                 "part_s, "
                 "part_f, "
                 "cn_s, "
                 "cn_f, "
                 "smb_s, "
                 "smb_f "
                 "from crop_residue "
                 "where species_id = '"
                 + species +
                 "' "
                 "and (residue_type = '"
                 + residueType +
                 "' or residue_type is null) "
                 "order by species_id, residue_type desc";

	//cout << "query: " << query << endl;
	con->select(query);
  //take the first (best matching) element
  //at least there should always be the "default" residue parameters with
  //type = NULL be available
  if(!(row = con->getRow()).empty())
	{
		CropResidueParametersPtr omp = make_shared<CropResidueParameters>();

		int i = 0;

		omp->species = row[i++];
    omp->residueType = row[i++];
		omp->vo_AOM_DryMatterContent = stoi(row[i++]);
		omp->vo_AOM_NH4Content = stof(row[i++]);
		omp->vo_AOM_NO3Content = stof(row[i++]);
		omp->vo_AOM_CarbamidContent = stof(row[i++]);
		omp->vo_AOM_SlowDecCoeffStandard = stof(row[i++]);
		omp->vo_AOM_FastDecCoeffStandard = stof(row[i++]);
		omp->vo_PartAOM_to_AOM_Slow = stof(row[i++]);
		omp->vo_PartAOM_to_AOM_Fast = stof(row[i++]);
		omp->vo_CN_Ratio_AOM_Slow = stof(row[i++]);
		omp->vo_CN_Ratio_AOM_Fast = stof(row[i++]);
		omp->vo_PartAOM_Slow_to_SMB_Slow = stof(row[i++]);
		omp->vo_PartAOM_Slow_to_SMB_Fast = stof(row[i++]);
				
		return omp;
	}

	static CropResidueParametersPtr nothing = make_shared<CropResidueParameters>();
	return nothing;
}

vector<CropResidueParametersPtr> getAllCropResidueParametersFromMonicaDB()
{
	vector<CropResidueParametersPtr> acrps;

	DBPtr con(newConnection("monica"));
	DBRow row;
	con->select("select "
							"species_id, "
							"cultivar_id "
							"from crop_residue "
							"order by species_id, cultivar_id");
	while(!(row = con->getRow()).empty())
	{
		acrps.push_back(getResidueParametersFromMonicaDB(row[0], row[1]));
	}

	return acrps;
}

void Monica::writeCropResidues(string path)
{
  for (auto r : getAllCropResidueParametersFromMonicaDB())
	{
    string speciesPath = path + "/" + r->species;
    bool noResidueType = r->residueType.empty();
    string residueTypePath =
        (noResidueType ? speciesPath
                       : speciesPath + "/" + r->residueType) + ".json";

    if(noResidueType)
			ensureDirExists(surround("\"", path));
		else
			ensureDirExists(surround("\"", speciesPath));
			
		ofstream ofs;
    ofs.open(residueTypePath);
		if (ofs.good())
		{
      auto s = r->to_json().dump();
			//cout << "id: " << of->id << " name: " << of->name << " ----> " << endl << s << endl;
			ofs << s;
			ofs.close();
		}
	}
}

//------------------------------------------------------------------------------

CentralParameterProvider Monica::readUserParameterFromDatabase(int type)
{
	static mutex lockable;

	static bool initialized = false;

	static CentralParameterProvider centralParameterProvider;

	if (!initialized)
	{
		lock_guard<mutex> lock(lockable);

		if (!initialized)
		{
			debug() << "DB Conncection user parameters" << endl;
			DB *con = newConnection("monica");
			DBRow row;
			switch (type)
			{
			case MODE_HERMES:
				con->select("select name, value_hermes from user_parameter");
				break;
			case MODE_EVA2:
				con->select("select name, value_eva2 from user_parameter");
				break;
			case MODE_MACSUR_SCALING:
				con->select("select name, value_macsur_scaling from user_parameter");
				break;
			default:
				con->select("select name, value_hermes from user_parameter");
				break;
			}

      UserCropParameters& user_crops = centralParameterProvider.userCropParameters;
      UserEnvironmentParameters& user_env = centralParameterProvider.userEnvironmentParameters;
      UserSoilMoistureParameters& user_soil_moisture = centralParameterProvider.userSoilMoistureParameters;

      user_soil_moisture.getCapillaryRiseRate = [](string soilTexture, int distance)
      {
        return Soil::readCapillaryRiseRates().getRate(soilTexture, distance);
      };

      UserSoilTemperatureParameters& user_soil_temperature = centralParameterProvider.userSoilTemperatureParameters;
      UserSoilTransportParameters& user_soil_transport = centralParameterProvider.userSoilTransportParameters;
      UserSoilOrganicParameters& user_soil_organic = centralParameterProvider.userSoilOrganicParameters;

      SimulationParameters& sim = centralParameterProvider.simulationParameters;

			while (!(row = con->getRow()).empty())
			{
				std::string name = row[0];
				if (name == "tortuosity")
					user_crops.pc_Tortuosity = stof(row[1]);
				else if (name == "canopy_reflection_coefficient")
					user_crops.pc_CanopyReflectionCoefficient = stof(row[1]);
				else if (name == "reference_max_assimilation_rate")
					user_crops.pc_ReferenceMaxAssimilationRate = stof(row[1]);
				else if (name == "reference_leaf_area_index")
					user_crops.pc_ReferenceLeafAreaIndex = stof(row[1]);
				else if (name == "maintenance_respiration_parameter_2")
					user_crops.pc_MaintenanceRespirationParameter2 = stof(row[1]);
				else if (name == "maintenance_respiration_parameter_1")
					user_crops.pc_MaintenanceRespirationParameter1 = stof(row[1]);
				else if (name == "minimum_n_concentration_root")
					user_crops.pc_MinimumNConcentrationRoot = stof(row[1]);
				else if (name == "minimum_available_n")
					user_crops.pc_MinimumAvailableN = stof(row[1]);
				else if (name == "reference_albedo")
					user_crops.pc_ReferenceAlbedo = stof(row[1]);
				else if (name == "stomata_conductance_alpha")
					user_crops.pc_StomataConductanceAlpha = stof(row[1]);
				else if (name == "saturation_beta")
					user_crops.pc_SaturationBeta = stof(row[1]);
				else if (name == "growth_respiration_redux")
					user_crops.pc_GrowthRespirationRedux = stof(row[1]);
				else if (name == "max_crop_n_demand")
					user_crops.pc_MaxCropNDemand = stof(row[1]);
				else if (name == "growth_respiration_parameter_2")
					user_crops.pc_GrowthRespirationParameter2 = stof(row[1]);
				else if (name == "growth_respiration_parameter_1")
					user_crops.pc_GrowthRespirationParameter1 = stof(row[1]);
				else if (name == "use_automatic_irrigation")
          sim.p_UseAutomaticIrrigation = stob(row[1]);
				else if (name == "use_nmin_mineral_fertilising_method")
          sim.p_UseNMinMineralFertilisingMethod = stob(row[1]);
				else if (name == "layer_thickness")
          sim.p_LayerThickness = stod(row[1]);
				else if (name == "number_of_layers")
          sim.p_NumberOfLayers = stoi(row[1]);
				else if (name == "start_pv_index")
          sim.p_StartPVIndex = stoi(row[1]);
				else if (name == "albedo")
					user_env.p_Albedo = stof(row[1]);
				else if (name == "athmospheric_co2")
					user_env.p_AtmosphericCO2 = stof(row[1]);
				else if (name == "wind_speed_height")
					user_env.p_WindSpeedHeight = stof(row[1]);
				else if (name == "use_secondary_yields")
          sim.p_UseSecondaryYields = stob(row[1]);
				else if (name == "julian_day_automatic_fertilising")
          sim.p_JulianDayAutomaticFertilising = stoi(row[1]);
				else if (name == "critical_moisture_depth")
					user_soil_moisture.pm_CriticalMoistureDepth = stof(row[1]);
				else if (name == "saturated_hydraulic_conductivity")
					user_soil_moisture.pm_SaturatedHydraulicConductivity = stof(row[1]);
				else if (name == "surface_roughness")
					user_soil_moisture.pm_SurfaceRoughness = stof(row[1]);
				else if (name == "hydraulic_conductivity_redux")
					user_soil_moisture.pm_HydraulicConductivityRedux = stof(row[1]);
				else if (name == "snow_accumulation_treshold_temperature")
					user_soil_moisture.pm_SnowAccumulationTresholdTemperature = stof(row[1]);
				else if (name == "kc_factor")
					user_soil_moisture.pm_KcFactor = stof(row[1]);
				else if (name == "time_step")
					user_env.p_timeStep = stof(row[1]);
				else if (name == "temperature_limit_for_liquid_water")
					user_soil_moisture.pm_TemperatureLimitForLiquidWater = stof(row[1]);
				else if (name == "correction_snow")
					user_soil_moisture.pm_CorrectionSnow = stof(row[1]);
				else if (name == "correction_rain")
					user_soil_moisture.pm_CorrectionRain = stof(row[1]);
				else if (name == "snow_max_additional_density")
					user_soil_moisture.pm_SnowMaxAdditionalDensity = stof(row[1]);
				else if (name == "new_snow_density_min")
					user_soil_moisture.pm_NewSnowDensityMin = stof(row[1]);
				else if (name == "snow_retention_capacity_min")
					user_soil_moisture.pm_SnowRetentionCapacityMin = stof(row[1]);
				else if (name == "refreeze_parameter_2")
					user_soil_moisture.pm_RefreezeParameter2 = stof(row[1]);
				else if (name == "refreeze_parameter_1")
					user_soil_moisture.pm_RefreezeParameter1 = stof(row[1]);
				else if (name == "refreeze_temperature")
					user_soil_moisture.pm_RefreezeTemperature = stof(row[1]);
				else if (name == "snowmelt_temperature")
					user_soil_moisture.pm_SnowMeltTemperature = stof(row[1]);
				else if (name == "snow_packing")
					user_soil_moisture.pm_SnowPacking = stof(row[1]);
				else if (name == "snow_retention_capacity_max")
					user_soil_moisture.pm_SnowRetentionCapacityMax = stof(row[1]);
				else if (name == "evaporation_zeta")
					user_soil_moisture.pm_EvaporationZeta = stof(row[1]);
				else if (name == "xsa_critical_soil_moisture")
					user_soil_moisture.pm_XSACriticalSoilMoisture = stof(row[1]);
				else if (name == "maximum_evaporation_impact_depth")
					user_soil_moisture.pm_MaximumEvaporationImpactDepth = stof(row[1]);
				else if (name == "ntau")
					user_soil_temperature.pt_NTau = stof(row[1]);
				else if (name == "initial_surface_temperature")
					user_soil_temperature.pt_InitialSurfaceTemperature = stof(row[1]);
				else if (name == "base_temperature")
					user_soil_temperature.pt_BaseTemperature = stof(row[1]);
				else if (name == "quartz_raw_density")
					user_soil_temperature.pt_QuartzRawDensity = stof(row[1]);
				else if (name == "density_air")
					user_soil_temperature.pt_DensityAir = stof(row[1]);
				else if (name == "density_water")
					user_soil_temperature.pt_DensityWater = stof(row[1]);
				else if (name == "specific_heat_capacity_air")
					user_soil_temperature.pt_SpecificHeatCapacityAir = stof(row[1]);
				else if (name == "specific_heat_capacity_quartz")
					user_soil_temperature.pt_SpecificHeatCapacityQuartz = stof(row[1]);
				else if (name == "specific_heat_capacity_water")
					user_soil_temperature.pt_SpecificHeatCapacityWater = stof(row[1]);
				else if (name == "soil_albedo")
					user_soil_temperature.pt_SoilAlbedo = stof(row[1]);
				else if (name == "dispersion_length")
					user_soil_transport.pq_DispersionLength = stof(row[1]);
				else if (name == "AD")
					user_soil_transport.pq_AD = stof(row[1]);
				else if (name == "diffusion_coefficient_standard")
					user_soil_transport.pq_DiffusionCoefficientStandard = stof(row[1]);
				else if (name == "leaching_depth")
					user_env.p_LeachingDepth = stof(row[1]);
				else if (name == "groundwater_discharge")
					user_soil_moisture.pm_GroundwaterDischarge = stof(row[1]);
				else if (name == "density_humus")
					user_soil_temperature.pt_DensityHumus = stof(row[1]);
				else if (name == "specific_heat_capacity_humus")
					user_soil_temperature.pt_SpecificHeatCapacityHumus = stof(row[1]);
				else if (name == "max_percolation_rate")
					user_soil_moisture.pm_MaxPercolationRate = stof(row[1]);
				else if (name == "max_groundwater_depth")
					user_env.p_MaxGroundwaterDepth = stof(row[1]);
				else if (name == "min_groundwater_depth")
					user_env.p_MinGroundwaterDepth = stof(row[1]);
				else if (name == "min_groundwater_depth_month")
					user_env.p_MinGroundwaterDepthMonth = stoi(row[1]);
				else if (name == "SOM_SlowDecCoeffStandard")
					user_soil_organic.po_SOM_SlowDecCoeffStandard = stof(row[1]);
				else if (name == "SOM_FastDecCoeffStandard")
					user_soil_organic.po_SOM_FastDecCoeffStandard = stof(row[1]);
				else if (name == "SMB_SlowMaintRateStandard")
					user_soil_organic.po_SMB_SlowMaintRateStandard = stof(row[1]);
				else if (name == "SMB_FastMaintRateStandard")
					user_soil_organic.po_SMB_FastMaintRateStandard = stof(row[1]);
				else if (name == "SMB_SlowDeathRateStandard")
					user_soil_organic.po_SMB_SlowDeathRateStandard = stof(row[1]);
				else if (name == "SMB_FastDeathRateStandard")
					user_soil_organic.po_SMB_FastDeathRateStandard = stof(row[1]);
				else if (name == "SMB_UtilizationEfficiency")
					user_soil_organic.po_SMB_UtilizationEfficiency = stof(row[1]);
				else if (name == "SOM_SlowUtilizationEfficiency")
					user_soil_organic.po_SOM_SlowUtilizationEfficiency = stof(row[1]);
				else if (name == "SOM_FastUtilizationEfficiency")
					user_soil_organic.po_SOM_FastUtilizationEfficiency = stof(row[1]);
				else if (name == "AOM_SlowUtilizationEfficiency")
					user_soil_organic.po_AOM_SlowUtilizationEfficiency = stof(row[1]);
				else if (name == "AOM_FastUtilizationEfficiency")
					user_soil_organic.po_AOM_FastUtilizationEfficiency = stof(row[1]);
				else if (name == "AOM_FastMaxC_to_N")
					user_soil_organic.po_AOM_FastMaxC_to_N = stof(row[1]);
				else if (name == "PartSOM_Fast_to_SOM_Slow")
					user_soil_organic.po_PartSOM_Fast_to_SOM_Slow = stof(row[1]);
				else if (name == "PartSMB_Slow_to_SOM_Fast")
					user_soil_organic.po_PartSMB_Slow_to_SOM_Fast = stof(row[1]);
				else if (name == "PartSMB_Fast_to_SOM_Fast")
					user_soil_organic.po_PartSMB_Fast_to_SOM_Fast = stof(row[1]);
				else if (name == "PartSOM_to_SMB_Slow")
					user_soil_organic.po_PartSOM_to_SMB_Slow = stof(row[1]);
				else if (name == "PartSOM_to_SMB_Fast")
					user_soil_organic.po_PartSOM_to_SMB_Fast = stof(row[1]);
				else if (name == "CN_Ratio_SMB")
					user_soil_organic.po_CN_Ratio_SMB = stof(row[1]);
				else if (name == "LimitClayEffect")
					user_soil_organic.po_LimitClayEffect = stof(row[1]);
				else if (name == "AmmoniaOxidationRateCoeffStandard")
					user_soil_organic.po_AmmoniaOxidationRateCoeffStandard = stof(row[1]);
				else if (name == "NitriteOxidationRateCoeffStandard")
					user_soil_organic.po_NitriteOxidationRateCoeffStandard = stof(row[1]);
				else if (name == "TransportRateCoeff")
					user_soil_organic.po_TransportRateCoeff = stof(row[1]);
				else if (name == "SpecAnaerobDenitrification")
					user_soil_organic.po_SpecAnaerobDenitrification = stof(row[1]);
				else if (name == "ImmobilisationRateCoeffNO3")
					user_soil_organic.po_ImmobilisationRateCoeffNO3 = stof(row[1]);
				else if (name == "ImmobilisationRateCoeffNH4")
					user_soil_organic.po_ImmobilisationRateCoeffNH4 = stof(row[1]);
				else if (name == "Denit1")
					user_soil_organic.po_Denit1 = stof(row[1]);
				else if (name == "Denit2")
					user_soil_organic.po_Denit2 = stof(row[1]);
				else if (name == "Denit3")
					user_soil_organic.po_Denit3 = stof(row[1]);
				else if (name == "HydrolysisKM")
					user_soil_organic.po_HydrolysisKM = stof(row[1]);
				else if (name == "ActivationEnergy")
					user_soil_organic.po_ActivationEnergy = stof(row[1]);
				else if (name == "HydrolysisP1")
					user_soil_organic.po_HydrolysisP1 = stof(row[1]);
				else if (name == "HydrolysisP2")
					user_soil_organic.po_HydrolysisP2 = stof(row[1]);
				else if (name == "AtmosphericResistance")
					user_soil_organic.po_AtmosphericResistance = stof(row[1]);
				else if (name == "N2OProductionRate")
					user_soil_organic.po_N2OProductionRate = stof(row[1]);
				else if (name == "Inhibitor_NH3")
					user_soil_organic.po_Inhibitor_NH3 = stof(row[1]);
			}
			delete con;
			initialized = true;
#ifdef RUN_MACSUR_SCALING
			initialized = false;
#endif

//			centralParameterProvider.capillaryRiseRates = Soil::readCapillaryRiseRates();
		}
	}

	return centralParameterProvider;
}

void Monica::writeUserParameters(int type, string path)
{
	string typeName = "hermes";
	switch (type)
	{
	case MODE_EVA2: typeName = "eva2"; break;
	case MODE_MACSUR_SCALING: typeName = "macsur"; break;
	default:;
	}
	
	auto ups = readUserParameterFromDatabase(type);

	ensureDirExists(surround("\"", path));

	{
		ofstream ofs;
		ofs.open(path + "/" + typeName + "-crop.json");
		if (ofs.good())
		{
			ofs << ups.userCropParameters.to_json().dump();
			ofs.close();
		}
	}
	{
		ofstream ofs;
		ofs.open(path + "/" + typeName + "-environment.json");
		if (ofs.good())
		{
			ofs << ups.userEnvironmentParameters.to_json().dump();
			ofs.close();
		}
	}
	{
		ofstream ofs;
		ofs.open(path + "/" + typeName + "-soil-moisture.json");
		if (ofs.good())
		{
			ofs << ups.userSoilMoistureParameters.to_json().dump();
			ofs.close();
		}
	}
	{
		ofstream ofs;
		ofs.open(path + "/" + typeName + "-soil-temperature.json");
		if (ofs.good())
		{
			ofs << ups.userSoilTemperatureParameters.to_json().dump();
			ofs.close();
		}
	}
	{
		ofstream ofs;
		ofs.open(path + "/" + typeName + "-soil-transport.json");
		if (ofs.good())
		{
			ofs << ups.userSoilTransportParameters.to_json().dump();
			ofs.close();
		}
	}
	{
		ofstream ofs;
		ofs.open(path + "/" + typeName + "-soil-organic.json");
		if (ofs.good())
		{
			ofs << ups.userSoilOrganicParameters.to_json().dump();
			ofs.close();
		}
	}
}

//----------------------------------------------------------------------------------

const map<int, AMCRes>& Monica::availableMonicaCrops()
{
	static mutex lockable;
  static map<int, AMCRes> m;
	static bool initialized = false;
  if (!initialized)
	{
    lock_guard<mutex> lock(lockable);

    if (!initialized)
    {
      DBPtr con(newConnection("monica"));

      string query(
            "select "
            "id, "
            "species_id, "
            "cultivar_id "
            "from crop "
            "order by id");
      con->select(query.c_str());

      DBRow row;
      while (!(row = con->getRow()).empty())
      {
        AMCRes res;
        res.speciesId = row[1];
        res.cultivarId = row[2];
        res.name = capitalize(row[1]) + "/" + capitalize(row[2]);
        m[stoi(row[0])] = res;
      }

			initialized = true;
		}
	}

	return m;
}
