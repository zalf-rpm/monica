#!/usr/bin/python
# -*- coding: ISO-8859-15-*-

import sys
sys.path.append('../../../')	 # path to monica.py
import monica_py
import os
import datetime
import shutil
import time
import json
import csv

def main():

	with open("sim.json") as f:
		sim = json.load(f)

	with open("site-soil-profile-from-db.json") as f:
		site = json.load(f)

	with open("../crop.json") as f:
		crop = json.load(f)

	sim["path-to-output"] = "."
	sim["climate.csv"] = "../climate.csv"
	site["SiteParameters"]["SoilProfileParameters"][1]["id"] = 2

	#print "startDate: " + sim["start-date"]
	#print "sim: " + json.dumps(sim)

	y2y = monica_py.runMonica({
		"sim-json-str" : json.dumps(sim),
		"site-json-str" : json.dumps(site),
		"crop-json-str" : json.dumps(crop)
	})

	#print "len(year2yield): " + str(len(y2y))

	if len(y2y) > 0:
		for year, yield_ in y2y.iteritems():
			print "year: " + str(year) + " yield: " + str(yield_)

main()
