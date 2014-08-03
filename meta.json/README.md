HOWTO meta
==========

## Files ##

  - meta.sim.json (simulation settings)
  - meta.site.json (site description and soil horizons)
  - meta.crop.json (crop rotation and operations)


## Format ##

### Required properties ###

  * *desc*: not null, string: any

**Example**
```javascript
"myParameter" : {
  "desc": "a discription"
}
```

### Optional properties ###

  * *unit*: not null, string: "-", "bool", "date" (default date format is "YYYY-MM-DD") or any other physical unit.
    Usally not necessary if enum or db are used.
  * *default*: null, string or number
  * *min*: null or number,
  * *max*: null or number,
  * *advanced*: not null, true or false  (for "advanced" users or probably not implemented or experimental)
  * *enum* **OR** *db*
    * enum: not null, array with strings or numbers
    * db: not null, object (format below)

**Example**
```javascript
"myParameter" : {
  "desc": "a discription",
  "unit": "kg kg-1",
  "min": 0.0,
  "max": 1.0
}
```

**Example**
```javascript
"myParameter" : {
  "desc": "a discription",
  "default": "option2",
  "enum": ["option1", "option2"]
}
```
**Example**
```javascript
"myParameter" : {
  "desc": "a discription",
  "db": {
    "table": "a_table",
    "columns": [
      "column1",
      "column2"
    ],
    "keys": [
      "column1"
    ]
  }
}
```
