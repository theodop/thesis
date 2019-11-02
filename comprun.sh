clear

device="default"
echo Launched with arg1 $1

if [ ! -z $1 ]
then   
	device="plughw:CARD=ALSA,DEV=1"
fi

echo Launching with ALSA device $device

g++ -std=c++11 -pthread thesis.cpp -lrealsense2 -lasound -lncurses -o thesis-output && echo compiled && ./thesis-output $device