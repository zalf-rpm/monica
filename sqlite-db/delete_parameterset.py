#!/usr/bin/python
# -*- coding: UTF-8

######################################

db= "monica.sqlite"
crop_id = 26

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

    
    delete_entry("crop", "id",  cursor, conn)
    delete_entry("crop2ods_dependent_param", "crop_id",  cursor, conn)
    delete_entry("cutting_parts", "crop_id",  cursor, conn)
    delete_entry("dev_stage", "crop_id",  cursor, conn)
    delete_entry("organ", "crop_id",  cursor, conn)
    delete_entry("residue_table", "crop_id",  cursor, conn)
    delete_entry("yield_parts", "crop_id",  cursor, conn)
    
    conn.close()
    
####################################################################
####################################################################
####################################################################

def delete_entry(table, id, cursor, conn):
    
    print "Deleting rows data from table " + table
    
    # get row of data
    query_string = """DELETE FROM """ + table + """ WHERE """ + id + """ = """ + str(crop_id)
    print query_string, "\n"
    cursor.execute(query_string)
    conn.commit()
    
####################################################################
####################################################################
####################################################################      
    
main()