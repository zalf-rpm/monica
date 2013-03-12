#!/usr/bin/python
# -*- coding: UTF-8

######################################

db= "monica.sqlite"

######################################

import sqlite3
import sys

####################################################################
####################################################################
####################################################################

def main():
    conn = sqlite3.connect(db)    
    conn.text_factory = str
    cursor = conn.cursor()

    
    add_row(cursor, conn)
    
    conn.close()
    
####################################################################
####################################################################
####################################################################

def add_row(cursor, conn):
    
    table = "crop2ods_dependent_param"
    
    print "Adding rows ids from table " + table
    

    query_string1 = """SELECT * 
                        FROM crop2ods_dependent_param 
                        GROUP BY crop_id, organ_id, dev_stage_id  
                        ORDER BY crop_id, organ_id, dev_stage_id"""
                         
    cursor.execute(query_string1)
    
    print query_string1, "\n"
    cursor.execute(query_string1)
    rows = cursor.fetchall()
    
    for row in rows:
        crop_id = row[0]
        organ_id = row[1]
        dev_stage_id = row[2]
        
        print crop_id, organ_id, dev_stage_id
        
        query_string2 = """INSERT INTO crop2ods_dependent_param 
                         (crop_id, organ_id, dev_stage_id, ods_dependent_param_id, value)
                         VALUES(""" + str(crop_id) + """, """ + str(organ_id) + """, """ + str(dev_stage_id) + """, 3, 0.0) """
        
        print query_string2
        cursor.execute(query_string2)
        
        
    conn.commit()
    
####################################################################
####################################################################
####################################################################      
    
main()