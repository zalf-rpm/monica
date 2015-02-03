#!/usr/bin/python
# -*- coding: ISO-8859-15-*-

import sys

sys.path.append('.')   # path to monica.py

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

pathToCarbiocialData = "/media/san1_data1/data1/berg/carbiocial/macsur_scaling/"
pathToClimateData = "/media/archiv/md/berg/carbiocial/climate-data-out-0-2544/"
pathToClimateDataReorderingFile = "/media/archiv/md/berg/carbiocial/final_order_dates_l9_sres_a1b_2013-2040.dat" 

sep = ","
remove_monica_files_after_simulation = False  # True

startDate = "1981-01-01"
endDate = "2012-12-31"

asciiGridHeaders = []
noOfGridRows = 2545
noOfGridCols = 1928
noDataValue = -9999
noSoilValue = -8888

"""
main routine of the carbiocial cluster simulation
"""
def main():
  ts = time.time()
  output_path = pathToCarbiocialData + "runs/" + datetime.datetime.fromtimestamp(ts).strftime('%Y-%m-%d_%H-%M') + "/"

  print "processor #", rank
  input_path = pathToCarbiocialData + "input_data/"
  #ini_file = "soybean.ini"
  ini_file = "maize.ini"
  
  splittedGridDataMap = None

  if (rank == 0):
    # only one processor reads in the meta information
    splittedGridDataMap = splitAsciiGrid(pathToCarbiocialData + "input_data/solos-profile-ids_brazil_900.asc", size)

  ###################################################
  # parallel part
  ##################################################

  # send each sublist of the splitted list to on processor
  nodeSpecificDataMap = comm.scatter(splittedGridDataMap, root=0)

  # each processor received a specific number of meta_info_objects
  # that he has to process
  print rank, "Received data map with ", len(nodeSpecificDataMap), " elements"

  monica_simulation_config = monica.CarbiocialConfiguration()
  monica_simulation_config.setInputPath(input_path)
  monica_simulation_config.setIniFile(ini_file)
  #monica_simulation_config.pathToClimateDataReorderingFile = pathToClimateDataReorderingFile;
  #monica_simulation_config.create2013To2040ClimateData = True
  #monica_simulation_config.setCropName(crop)

  #node_simulation_results = []

  coord2year2yield = {}

  index = 0
  for coord, profileId in nodeSpecificDataMap.iteritems():
    row, col = coord
    #row, col = (86, 820)
    monica_simulation_config.setStartDate(startDate)
    monica_simulation_config.setEndDate(endDate)
    monica_simulation_config.setClimateFile(pathToClimateData + "row-" + str(row) + "/col-" + str(col) + ".asc")
    #monica_simulation_config.setClimateFile(pathToCarbiocialData+"input_data/row-0/col-0.asc")
    monica_simulation_config.setRowId(row)
    monica_simulation_config.setColId(col)
    monica_simulation_config.setProfileId(profileId)

    print rank, "###################################"
    print rank, "coord: ", coord, " profileId: ", monica_simulation_config.getProfileId()
    print rank, "startDate: ", startDate, " endDate: ", endDate
    print rank, "climateFile: ", monica_simulation_config.getClimateFile()

    path = output_path + "row-" + str(row) + "/col-" + str(col) + "/"
    monica_simulation_config.setOutputPath(path)

    #if not (os.path.exists(path)):
    #  print rank, "create_directory: ", path
    #  os.makedirs(path)

    monica_simulation_config.setLatitude(-9.41)
    monica_simulation_config.setElevation(300.0)

    #monica.activateDebugOutput(True);
    monica.activateDebugOutput(False);
    #monica.activateDebugFileOutput(False);
    #monica.setPathToDebugFile(output_path + "row-" + str(row) + "/col-" + str(col) + "-debug.out");
    year2yield = monica.runCarbiocialSimulation(monica_simulation_config)
    #print rank, "type(year2yield): ", type(year2yield)
    #simResult = getYieldsFromSimulationOutput(path, row, col)
    #coord2year2yield[simResult[0]] = simResult[1]
    
    y2y = {}
    
    if len(year2yield) > 0:
      #outputFile = open(output_path + "row-" + str(row) + "/col-" + str(col) + "-yields.txt", "wb")    
      #outputFile.write("year yield\n")
      for year, yield_ in year2yield.iteritems():
      #  outputFile.write(str(year) + " " + str(yield_) + "\n")
        y2y[year] = yield_
      #outputFile.close()

      coord2year2yield[(row, col)] = y2y
    
      # remove simulation result dir
      #if remove_monica_files_after_simulation:
      #  shutil.rmtree(path, True)

    print rank, "###################################"

    #if index == 1:
    #  break

    index = index + 1


  ###################################################
  # end of parallel part
  ##################################################

  resultList = comm.gather(coord2year2yield, root=0)

  if rank == 0:

    print "outputing results ..."

    #sorted values for creation of yearly grids
    row2col2year2yield = {}
    #sorted values for creation of avg yield grid over all years
    row2col2yields = {}
    #print "resultList: ", resultList
    years = resultList[0].items()[0][1].keys();
    print "years: ", years

    #collect data into nested maps to access them below
    for c2y2y in resultList:
      for (row, col), y2y in c2y2y.iteritems():
        if not row in row2col2year2yield:
          row2col2year2yield[row] = {}
          row2col2yields[row] = {}
        row2col2year2yield[row][col] = y2y
        row2col2yields[row][col] = y2y.values()

    if not (os.path.exists(output_path)):
      print "creating output directory: ", output_path
      os.makedirs(output_path)    
        
    outputGridFilename = "yields-year-"
    outputAvgGridFile = open(output_path + "yields-avg.asc", "wb")
    outputAvgGridFile.writelines(asciiGridHeaders)
    currentColAvgYields = []
    year2openFile = {}
    year2currentColYields = {}
    #open for every available year a file
    for year in years:
      year2openFile[year] = open(output_path + outputGridFilename + str(year) + ".asc", "wb")
      year2openFile[year].writelines(asciiGridHeaders)

    #iterate over all rows and cols, avg years, and assemble a ascii grid line with the column values
    for row in range(noOfGridRows):
      for col in range(noOfGridCols):
        if row in row2col2year2yield and col in row2col2year2yield[row]:
          #collect column values for single years
          for year, yield_ in row2col2year2yield[row][col].iteritems():
            if not year in year2currentColYields:
              year2currentColYields[year] = []
            year2currentColYields[year].append(yield_)
        else:
          for year in years:
            if not year in year2currentColYields:
              year2currentColYields[year] = []
            year2currentColYields[year].append(noDataValue)

        #collect column values for the averaged years
        if row in row2col2yields and col in row2col2yields[row]:
          yields = row2col2yields[row][col]
          if len(yields) > 0:
            currentColAvgYields.append(sum(yields) / len(yields))
          else:
            currentColAvgYields.append(0)
        else:
          currentColAvgYields.append(noDataValue)

      #write the yearly column values to the according file
      for year, f in year2openFile.iteritems():
        line = " ".join([str(ys) for ys in year2currentColYields[year]]) + "\n"
        f.write(line)
        year2currentColYields[year] = []

      #write the averaged column values to the file
      avgLine = " ".join([str(ys) for ys in currentColAvgYields]) + "\n"
      outputAvgGridFile.write(avgLine)
      currentColAvgYields = []

    for year, f in year2openFile.iteritems():
      f.close()

    outputAvgGridFile.close()

def splitAsciiGrid(pathToFile, noOfNodes):

  #pathToFile = "B:\development\cluster\macsur-scaling-code\solos-profile-ids_brazil_900.asc"

  f = open(pathToFile)
  lines = f.readlines();
  f.close()

  #store grid header for reuse when creating the output grid
  [asciiGridHeaders.append(lines[i]) for i in range(6)]
  #print "stored grid header: ", asciiGridHeader

  # list that will store all meta info objects
  coord2cell = {}

  # go through each row of the ascii grid file
  dataCellCount = 0
  for rowNo in range(6, len(lines)):
    for colNo, col in enumerate(lines[rowNo].rstrip().split(" ")):
      if int(col) >= 0:
        coord2cell[(rowNo-6, colNo)] = int(col)
        dataCellCount = dataCellCount + 1

  maxCellsPerNode = dataCellCount / noOfNodes
  restCells = dataCellCount % noOfNodes

  nodeCount = 0
  cellCount = 0
  c2c = {}
  splittedList = []

  for coord, cellValue in coord2cell.iteritems():
    cellCount = cellCount + 1
    c2c[coord] = cellValue
    if cellCount + 1 > maxCellsPerNode:
      nodeCount = nodeCount + 1
      splittedList.append(c2c)
      c2c = {}
      if nodeCount == noOfNodes:
        #the last node does a few more cells, negligible given the large amount of cells
        cellCount = -restCells
      else:
        cellCount = 0

  return splittedList

  
  
main()
