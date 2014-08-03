/* 
  Views needed in monica.sqlite 

    - used in MONICA GUI to load combobox content
    - used to validate JSON input (not yet implemented)
*/

-- get a distinct list of texture class strings
CREATE VIEW view_texture_class AS 
SELECT DISTINCT soil_type 
FROM soil_characteristic_data 
ORDER BY soil_type COLLATE NOCASE ASC;
