#!/usr/bin/python
# -*- coding: ISO-8859-15-*-

import sys

import os
import datetime
import time

from mpi4py import MPI

# MPI related initialisations
comm = MPI.COMM_WORLD
rank = comm.Get_rank()          # number of the processor
size = comm.Get_size()          # number of all participating processes
name = MPI.Get_processor_name()

def main():
  print "test"
  
  #ts = time.time()
  
  print "processor #", rank
  
  work = None

  if (rank == 0):
    work = createWork(size)

  ###################################################
  # parallel part
  ##################################################

  # send each sublist of the splitted list to on processor
  workPerNode = comm.scatter(work, root=0)

  # each processor received a specific number of meta_info_objects
  # that he has to process
  print rank, "Received data map with ", len(workPerNode), " elements"

  workResult = sum(workPerNode)

  ###################################################
  # end of parallel part
  ##################################################

  workResults = comm.gather(workResult, root=0)

  if rank == 0:
    print "outputing results ..."

    for res in workResults:
        print "sum: ", res

def createWork(noOfNodes):
    work = []

    for i in range(noOfNodes):
        work.append(range(100))

    return work

main()
