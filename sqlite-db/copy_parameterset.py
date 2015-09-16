#!/usr/bin/python
# -*- coding: UTF-8

######################################

#db_source = "monica_07122011.sqlite" 
db_source = "monica-macsur-phase3.sqlite"
original_crop_id = 7

db_target = "monica-macsur-phase3-a.sqlite"
new_crop_id = 61

######################################

import sqlite3
import sys

####################################################################
####################################################################
####################################################################

def main():
    conn_src = sqlite3.connect(db_source)
    conn_target = sqlite3.connect(db_target)
    
    conn_src.text_factory = str
    conn_target.text_factory = str
        
    cursor_src = conn_src.cursor()
    cursor_target = conn_target.cursor()
    

    
    copy_crop_entry("crop", "id",  cursor_src, cursor_target, conn_src, conn_target)
    copy_crop_entry("crop2ods_dependent_param", "crop_id", cursor_src, cursor_target, conn_src, conn_target)
    copy_crop_entry("cutting_parts", "crop_id", cursor_src, cursor_target, conn_src, conn_target)
    copy_crop_entry("dev_stage", "crop_id", cursor_src, cursor_target, conn_src, conn_target)
    copy_crop_entry("organ", "crop_id", cursor_src, cursor_target, conn_src, conn_target)
    copy_crop_entry("residue_table", "crop_id", cursor_src, cursor_target, conn_src, conn_target)
    copy_crop_entry("yield_parts", "crop_id", cursor_src, cursor_target, conn_src, conn_target)
    
    conn_src.close()
    conn_target.close()
   
####################################################################
####################################################################
#################################################################### 
    
def get_residue_id(cursor_target):
    query_string = """SELECT max(ID) FROM residue_table"""
    cursor_target.execute(query_string)
    for row in cursor_target:
        return row[0]+1
    
    return 0

####################################################################
####################################################################
####################################################################

def copy_crop_entry(table, id, cursor_src, cursor_target, conn_src, conn_target):
    
    print ("Copying data from table " + table)
    
    column_names = test_column_names(table, cursor_src, cursor_target)
    
    residue_id = 0
    if (table == "residue_table"):
        residue_id = get_residue_id(cursor_target)
    
    # get row of data
    query_string = """SELECT * FROM """ + table + """ WHERE """ + id + """ = """ + str(original_crop_id)
    
    print (query_string, "\n")
    cursor_src.execute(query_string)
    
    
    for row in cursor_src:
        
        new_row = []   
        column_header = '('     
        placeholder = '('
            
        for index, item in enumerate(row):            
            placeholder += '?'
            column_header += str(column_names[index]) 
            if (index<(len(row)-1)):                
                placeholder += ','
                column_header += ','
            
              
            # replacement of some values    
            if (column_names[index] == id):
                # replace rowid                
                new_row.append(new_crop_id)
            elif (column_names[index] == "ID" and table == "residue_table"):
                new_row.append(residue_id)
            else:
                new_row.append(item)

        placeholder += ')'
        column_header += ')'
        
        
        query_string2 = 'INSERT INTO ' + table + ' ' + column_header + ' VALUES ' + placeholder 
         
        #print query_string2, "\n"
        print ("Adding ", new_row)
        cursor_target.execute(query_string2, new_row)
        conn_target.commit()
    
    print()
    
        
####################################################################
####################################################################
####################################################################

def test_column_names(table, c_src, c_target):
    
    print ("Testing column headers of table \"" +  table + "\"")
    
    c_src.execute('PRAGMA table_info(' + table + ')')
    names_src = []
    types_src = []
    for row in c_src:
        names_src.append(row[1])
        types_src.append(row[2])
        
    names_target = []
    c_target.execute('PRAGMA table_info(' + table + ')')
    for row in c_target:
        names_target.append(row[1])    
        
    for i,n in enumerate(names_src):
        #print "Testing", n
        if (n not in names_target):
            print ("Did not find column " + n + " in target table. Adding new column ...\n")
            query_string = "ALTER table " + table + " ADD COLUMN "+ n + " " + types_src[i]
            print (query_string, n)
            c_target.execute(query_string)
            
            
    return names_src
    
####################################################################
####################################################################
####################################################################
    
main()
