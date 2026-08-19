rem Start "monica server" start_monica_server.cmd
set MONICA_PARAMETERS=..\..\..\..\monica-parameters
Start ..\..\..\build\monica-zmq-server -bi -i tcp://localhost:6666 -bo -o tcp://localhost:7777

rem Start "monica producer" start_producer.cmd
python run-producer.py

rem Start /wait "monica consumer" start_consumer.cmd
python run-consumer.py

rem monica-zmq-server blocks forever, so it has to be killed explicitly once the consumer is done
taskkill /F /IM monica-zmq-server.exe >nul 2>&1
