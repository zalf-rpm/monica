SET no_of_MONICAs=2

@echo off
echo starting monica-zmq-proxy for the input side
START /B monica-zmq-proxy -p -f 50771 -b 50781

echo starting monica-zmq-proxy for the output side
START /B monica-zmq-proxy -prs -f 50782 -b 50772

ECHO starting %no_of_MONICAs% monicas
FOR /L %%i IN (1,1,%no_of_MONICAS%) DO (
	START /B monica-zmq-server -i tcp://localhost:50781 -o tcp://localhost:50782
) 

ECHO starting monica-zmq-run as server for assembling the json config files
ECHO but start blockingly, so the user won't close the window (maybe)
ECHO !!!! CLOSE COMMANDLINE WINDOW to stop MONICAS !!!!
monica-zmq-run -ces -p 50773

