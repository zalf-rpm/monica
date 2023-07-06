rem set MONICA_PARAMETERS environment variable to the location of the monica-parameters folder
set MONICA_PARAMETERS=..\..\monica-parameters
rem start monica with the simulation setup file (sim.json)
..\..\bin\monica-run.exe -o .\sim-out.csv sim.json
