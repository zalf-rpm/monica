#!/usr/bin/python
# -*- coding: ISO-8859-15-*-

import sys

sys.path.append('../monica-src')   # path to monica.py

import macsur_config
import mpi_helper
import monica
import os
import datetime
import numpy
import analyse_monica_outputs
import shutil
import time

import csv
from mpi4py import MPI

# MPI related initialisations
comm = MPI.COMM_WORLD
rank = comm.Get_rank()          # number of the processor
size = comm.Get_size()          # number of all participating processes
name = MPI.Get_processor_name() 

path_to_macsur_data = '/media/san1_data1/data1/berg/carbiocial/macsur_scaling'

sep = ","
remove_monica_files_after_simulation = True  # True

"""
main routine of the macsur scaling simulation
"""
def main():
    
    ts = time.time()
    output_path = path_to_macsur_data + "/runs/phase_1/" + datetime.datetime.fromtimestamp(ts).strftime('%Y-%m-%d_%H-%M')

    for crop in macsur_config.crops:

      crop_id = ""
      if (crop == "maize"):
        crop_id = "SM"
      elif (crop == "winter_wheat"):
        crop_id = "WW"

      for simulation_type in macsur_config.simulation_types:
      
          for resolution in macsur_config.resolutions:
          
              print rank, "Simulation type: ", simulation_type, "\tResolution: ", resolution    
              output_prefix, input_path, ini_file = getSimulationConfig(simulation_type, crop)
          
              splitted_meta_info_list = None
              
               
              if (rank == 0):
                  # only one processor reads in the meta information
                  meta_info_list = readMetaInfo(resolution)
                      
                  #for meta in meta_info_list:
                      #print meta.start_date, meta.end_date, meta.project_id, meta.climate_file, meta.row_id, meta.col_id
                      
                  # split the meta info list into "number of processors" sublists 
                  splitted_meta_info_list = mpi_helper.splitListForNodes(meta_info_list, size)
              
              ###################################################
              # parallel part
              ##################################################
              
              # send each sublist of the splitted list to on processor
              node_specific_meta_info_list = comm.scatter(splitted_meta_info_list, root=0)      
          
              # each processor received a specific number of meta_info_objects
              # that he has to process
              print rank, "Received meta info list with", len(node_specific_meta_info_list), "elements" 
          
              
              monica_simulation_config = monica.MacsurScalingConfiguration()
              monica_simulation_config.setInputPath(input_path)
              monica_simulation_config.setIniFile(ini_file)
              monica_simulation_config.setCropName(crop)
              
              node_simulation_results = []
              
              for index, meta in enumerate(node_specific_meta_info_list):
                  print rank, "###################################"
                  print rank, str(index+1) +  "/" + str(len(node_specific_meta_info_list)) 
                  print rank, "Calculating", meta.project_id
                  print rank, meta.start_date, meta.end_date
                  print rank, meta.climate_file
                  monica_simulation_config.setStartDate(meta.start_date)
                  monica_simulation_config.setEndDate(meta.end_date)
                  monica_simulation_config.setClimateFile(meta.climate_file)
                  monica_simulation_config.setProjectId(meta.project_id)
                  monica_simulation_config.setRowId(meta.row_id)
                  monica_simulation_config.setColId(meta.col_id)
                  monica_simulation_config.setPhase(1)
                  
                  path = output_path + "/" + crop + "_" + simulation_type + "/res" + str(meta.resolution) + "/C" + str(meta.col_id) + "-R" + str(meta.row_id)
                  monica_simulation_config.setOutputPath(path) 
                  
                  if not (os.path.exists(path)):
                      print rank, "create_directory: ", path
                      os.makedirs(path)
                  
                  monica_simulation_config.setLatitude(meta.latitude)
                  monica_simulation_config.setElevation(meta.elevation)
                          
                  monica.activateDebugOutput(False);
                  monica.runMacsurScalingSimulation(monica_simulation_config)
                  sim_result = analyseSimulationOutput(path, meta.row_id, meta.col_id, crop)
                  node_simulation_results.extend(sim_result)
                  
                  # remove simulation result dir
                  if (remove_monica_files_after_simulation):
                      shutil.rmtree(path, True)

                  print rank, "###################################"
                  
                  #if (index == 1):
                  #    break
                  
          
              ###################################################
              # end of parallel part
              ##################################################
              
              result_list = comm.gather(node_simulation_results, root=0)
              
              if (rank == 0):
                  
                  
                  output_filename = output_prefix + "_Res" + str(resolution) + "_" + crop + ".csv"       
                  output_filehandle = open(output_path + "/" + output_filename, "wb")
                  output_csv = csv.writer(output_filehandle, delimiter=sep)
                  
                  header = ["gridcell", "year", "Yield (t DM/ha)", "Total above ground biomass (t DM/ha)",
                        "Total ET over the growing season (mm/growing season)", 
                        "Total intercepted PAR over the growing season (MJ/ha/growing season)",
                        "Maximum LAI during the growing season (m2/m2)", 
                        "Anthesis date (DOY)", "Maturity date (DOY)", 
                        "SOC at sowing date at 30 cm (gC/m2)", 
                        "SOC at sowing date at 2.0 m (gC/m2)", 
                        "Total net ecosystem exchange over the growing season (gC/m2/growing season)", 
                        "Total net primary productivity over the growing season (gC/m2/growing season)",
                        "Total N20 over the growing season (kg N/ha/growing season)",
                        "Total annual N20 (kg N/ha/year)",
                        "Total annual N leaching over 1.5 m (kg N/ha/year)", 
                        "Total N leaching over the growing season over 1.5 m (kg N/ha/growing season)",
                        "Total annual water loss below 1.5 m (mm/ha/year)", 
                        "Total water loss below 1.5 m over the growing season (mm/ha/growing season)", 
                        "Maximum rooted soil depth (m)",
                        "Total annual irrigated water amount (mm)"]
                  output_csv.writerow(header)
                  
      
                   
                  for node_list in result_list:   
                      for result_row in node_list:
                          output_csv.writerow(result_row)
                          
                  output_filehandle.close()
    
#############################################################
#############################################################
#############################################################   
   
def analyseSimulationOutput(output_path, row_id, col_id, crop):
    
    rmout_file = output_path + "/rmout.dat"
    smout_file = output_path + "/smout.dat"

    rmout = None
    smout = None
        
    if (os.path.exists(rmout_file) and os.path.exists(smout_file)):
        rmout = read_monica_file(rmout_file)
        smout = read_monica_file(smout_file)  
        
    growth_seasons = find_growing_seasons(smout)
    
    simulation_results = []
    
    for season in growth_seasons:
        
        gridcell = "C" + str(col_id) + ":R" + str(row_id)
        
        sowing_index = season[0]
        harvest_index = season[1]
        
        
         
        d = smout[harvest_index]["Datum"].split("/")
        harvest_date = datetime.date(int(d[2]), int(d[1]), int(d[0]))

        year = harvest_date.year
        
        first_day_of_year = analyse_monica_outputs.get_index_of_date(datetime.date(year, 1, 1), rmout)
        last_day_of_year = analyse_monica_outputs.get_index_of_date(datetime.date(year, 12, 31), rmout)
        
        # biomass outputs
        yield_biomass = float(smout[harvest_index]["Yield"]) / 1000.0 # t DM / ha
        yield_biomass = round(yield_biomass, 2)
        
        above_ground_biomass = float(smout[harvest_index]["AbBiom"]) / 1000.0 # t DM / ha
        above_ground_biomass = round(above_ground_biomass, 2)
        
        # total ET over growing season
        total_et = analyse_monica_outputs.get_cumm_ET(sowing_index, harvest_index, rmout)
        
        total_par = "na"
        
        max_lai = analyse_monica_outputs.get_max_LAI(sowing_index, harvest_index, smout)
        
        # anthesis
        anthesis_date, anthesis_index = analyse_monica_outputs.get_anthesis_date(smout, sowing_index, harvest_index)        
        anthesis_doy = "na"
        if (anthesis_date != None):
            anthesis_doy = anthesis_date.timetuple().tm_yday
            
        # maturity
        maturity_date, maturity_index = analyse_monica_outputs.get_maturity_date(smout, sowing_index, harvest_index, crop)        
        maturity_doy = "na"
        if (maturity_date != None):
            maturity_doy = maturity_date.timetuple().tm_yday  
#        else:
#            maturity_doy = harvest_date.timetuple().tm_yday
        
        
        soc_30 = soc_200 = analyse_monica_outputs.get_soc30(sowing_index, rmout) # g C / m2
        soc_200 = analyse_monica_outputs.get_soc200(sowing_index, rmout)# g C / m2


        total_NEE = analyse_monica_outputs.get_cumm_NEE(sowing_index, harvest_index, rmout) # kg C / ha
        total_NEE = (total_NEE * 1000.0) / (100*100)     # g C / m2
                        
        total_NPP = analyse_monica_outputs.get_cumm_NPP(sowing_index, harvest_index, rmout) # kg C / ha
        total_NPP = (total_NPP * 1000.0) / (100*100)       # kg C / ha --> g C / m2
        
        total_n2o = analyse_monica_outputs.get_cumm_n2o(first_day_of_year, last_day_of_year, rmout) # kg N / ha       
        growing_season_n2o = analyse_monica_outputs.get_cumm_n2o(sowing_index, harvest_index, rmout) # kg N / ha        
        
        total_n_leaching = analyse_monica_outputs.get_cumm_n_leached(first_day_of_year, last_day_of_year, rmout)
        n_leaching_growing_season = analyse_monica_outputs.get_cumm_n_leached(sowing_index, harvest_index, rmout)
        
        total_waterloss = analyse_monica_outputs.get_cumm_waterloss(first_day_of_year, last_day_of_year, rmout)
        growing_season_waterloss = analyse_monica_outputs.get_cumm_waterloss(sowing_index, harvest_index, rmout)
        
        maximum_rooted_soil_depth = analyse_monica_outputs.get_max_rooting_depth(sowing_index, harvest_index, rmout)
        
        irrigation_amount = analyse_monica_outputs.get_cumm_irrigation_amount(first_day_of_year, last_day_of_year, smout)
        
        row = [gridcell, year, yield_biomass, above_ground_biomass, total_et, total_par, max_lai, anthesis_doy,
               maturity_doy, soc_30, soc_200, total_NEE, total_NPP, growing_season_n2o, total_n2o, total_n_leaching, 
               n_leaching_growing_season, total_waterloss, growing_season_waterloss, maximum_rooted_soil_depth,
               irrigation_amount]
        
        simulation_results.append(row)
        
        #print gridcell,"\t", year,"\t", yield_biomass,"\t", above_ground_biomass, "\t",total_et, "\t", total_par,
        #print "\t", max_lai, "\t", anthesis_doy, "\t", maturity_doy, "\t", soc_30, "\t", soc_200, "\t", total_NEE, "\t",
        #print total_NPP, "\t", total_n2o, "\t", total_n_leaching, "\t", n_leaching_growing_season, "\t", total_waterloss, 
        #print "\t", growing_season_waterloss, "\t", maximum_rooted_soil_depth
        
    return simulation_results
   
#############################################################
#############################################################
#############################################################

"""
Reads in MONICA files and saves the results into a list of maps
"""
def read_monica_file(file):
         
    data = csv.reader(open(file), delimiter="\t")  

    # Read the column names from the first line of the file  
    fields = data.next()
    
    # skip the line with units
    data.next()
    
    map_list = []  
    for row in data:  
        # Zip together the field names and values  
        items = zip(fields, row)  
        item = {}  
        # Add the value to our dictionary  
        
        for (name, value) in items:
            #print name, value  
            item[name.strip()] = value.strip()
        map_list.append(item)
    
    return map_list

#############################################################
#############################################################
#############################################################

def find_growing_seasons(smout):
    
    seasons = []
    
    old_name = ""
    sowing_index = 0
    harvest_index = 0
    for index, row in enumerate(smout):
        
        name = row["Crop"]
        
                
        if (name != "" and old_name == ""):
            sowing_index = index
            
        if (name == "" and old_name != ""):
            harvest_index = index-1
            seasons.append([sowing_index, harvest_index])
        
        old_name = name
            
    #for s in seasons:
        #print s
            
    return seasons   
 
 
#############################################################
#############################################################
#############################################################

"""
Reads all relevant information from the provided 
meta information file
""" 
def readMetaInfo(resolution):
    
    # open meta info file
    csv_file = open(path_to_macsur_data + "/input_data/phase_1/NRW_Climate_Metadata.csv", "rb")
    
    # use csv.reader to read the csv file
    meta_csv = csv.reader(csv_file, delimiter="\t")
    
    # skip header of csv file
    meta_csv.next()
   
    # list that will store all meta info objects 
    meta_info_list = []
    
    # go through each row of the csv file
    for row in meta_csv:
        res = int(row[1])

        if (res == resolution):
            # parse columns of the csv row
            start_date = row[7]
            end_date = row[8]
            project_id = row[5]
            climate_file = None 
            row_id = int(row[2])
            col_id = int(row[3])
            lat = float(row[13])
            ele = float(row[14])
           
            if (res == 1):
                climate_file = path_to_macsur_data + "/input_data/phase_1/NRW_Climate_Data_1x1_UBN/"  + row[9]
            else:
                climate_file = path_to_macsur_data+ "/input_data/phase_1/NRW_Climate_Data_nxn_UBN/"  + row[9]
                
            # create a meta config objects that stores all relevant information
            meta_config = macsur_meta_config(start_date, end_date, project_id, climate_file, row_id, col_id, lat, ele, res)
            
            # test only specific point
            #if (row_id == 7 and col_id == 3):
                #meta_info_list.append(meta_config)
                
            # add object to the meta info list
            meta_info_list.append(meta_config)
            
    csv_file.close()
    
    return meta_info_list
            
            
#############################################################
#############################################################
#############################################################

def getSimulationConfig(simulation_type, crop):
    
    ini_file = ""
    input_path = ""
    output_prefix = ""

    # potential
    if (simulation_type == "potential"):
        
        output_prefix = "PLP"
        
        if (crop == "maize"):
            
            input_path = path_to_macsur_data + "/input_data/phase_1/crop_management/maize/maize_PLP/"
            ini_file = "maize_PLP.ini"        
            
        elif (crop == "winter_wheat"):
            
            input_path = path_to_macsur_data + "/input_data/phase_1/crop_management/winter_wheat/winter_wheat_PLP/"
            ini_file = "winter_wheat_PLP.ini"
            
    
    # water limited        
    elif (simulation_type == "water_limited"):
        
        output_prefix = "PLW"
        
        if (crop == "maize"):
        
            input_path = path_to_macsur_data + "/input_data/phase_1/crop_management/maize/maize_PLW/"
            ini_file = "maize_PLW.ini"
        
        elif (crop == "winter_wheat"):
            
            input_path = path_to_macsur_data + "/input_data/phase_1/crop_management/winter_wheat/winter_wheat_PLW/" 
            ini_file = "winter_wheat_PLW.ini"
        
    # nutrient water limited    
    elif (simulation_type == "nutrient_water_limited"):
        
        output_prefix = "PLN"
        
        if (crop == "maize"):
            
            input_path = path_to_macsur_data + "/input_data/phase_1/crop_management/maize/maize_PLN/"
            ini_file = "maize_PLN.ini"
            
        elif (crop == "winter_wheat"):
            
            input_path = path_to_macsur_data + "/input_data/phase_1/crop_management/winter_wheat/winter_wheat_PLN/"
            ini_file = "winter_wheat_PLN.ini"

    return output_prefix, input_path, ini_file
    
"""
Datastructure for the relevant meta information
"""
class macsur_meta_config():
    def __init__(self, start_date, end_date, project_id, climate_file, row_id, col_id, lat, elevation, resolution):
        
        self.start_date = start_date
        self.end_date = end_date
        self.project_id = project_id        
        self.climate_file = climate_file
        self.row_id = row_id
        self.col_id=col_id
        self.latitude = lat
        self.elevation = elevation
        self.resolution = resolution
        
        
    
    
main()
