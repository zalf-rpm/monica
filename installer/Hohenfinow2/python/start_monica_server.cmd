rem set MONICA_PARAMETERS environment variable to the location of the monica-parameters folder
set MONICA_PARAMETERS=..\..\monica-parameters

..\..\..\bin\monica-zmq-server -bi -i tcp://localhost:6666 -bo -o tcp://localhost:7777 

