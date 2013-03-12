#!/usr/bin/python
# -*- coding: UTF-8

######################################

db= "monica.sqlite"
old_crop_id = 6
new_crop_id = 10

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

    
    change_id("crop", "id",  cursor, conn)
    change_id("crop2ods_dependent_param", "crop_id",  cursor, conn)
    change_id("cutting_parts", "crop_id",  cursor, conn)
    change_id("dev_stage", "crop_id",  cursor, conn)
    change_id("organ", "crop_id",  cursor, conn)
    change_id("residue_table", "crop_id",  cursor, conn)
    change_id("yield_parts", "crop_id",  cursor, conn)
    
    conn.close()
    
####################################################################
####################################################################
####################################################################

def change_id(table, id, cursor, conn):
    
    print "Changing ids from table " + table
    


    query_string = """UPDATE """ + table + """ SET """ + id + """ = """ + str(new_crop_id) + """
                      WHERE """ + id + """ = """ + str(old_crop_id)
    print query_string, "\n"
    cursor.execute(query_string)
    conn.commit()
    
####################################################################
####################################################################
####################################################################      
    
main()