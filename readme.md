# Wearable Device for Situational Awareness Using Stereo Sound

This has been presented as an Honours Thesis to QUT.

This project presents a technique for increasing situational awareness for the vision impaired, and a prototype of a device to utilise such a technique. A head-mounted Intel RealSense D435i camera is used to capture a depth “image” in front of the user, with a horizontal field of view of approximately 80 degrees. Samples are taken from the horizon of this depth image, and three audio localisation pointers are produced utilising “clap” sounds, with each sound panned in stereo to represent their azimuth relative to the user and delayed by an amount of time proportional to the distance.
Processing of the depth information is performed using a Raspberry Pi system running Ubuntu MATE. Sound output is performed using the ALSA audio libraries, and a technique for producing low-latency sound output for the audio localisation pointers is presented. The device is “on-demand” (only producing audio when the user requests it) and user input is through a Logitech wireless USB pointer.

## Hardware Requirements

This project utilises a Raspberry 3 B+, Intel RealSense D435i depth camera, and Logitech D400 wireless USB pointer. Any changes from that will require alterations to the code and environment.

## Software Requirements

This project uses Ubuntu and has been tested against Ubuntu 18 on an i5 x86, as well as Ubuntu Mint 18 on the RPi 3 B+.

To build the project, you will need to install:
- g++
- make
- cmake
- ALSA
- ncurses
- Intel RealSense SDK 2 as per here: https://github.com/IntelRealSense/librealsense/blob/master/doc/installation.md
- OpenCV - note that there is no RPi package for apt and should be built from source https://github.com/opencv/opencv

A build is performed by navigating to the build folder and performing:
cmake && make

Launch on PC (assuming your default ALSA device can produce sound) by launching build/theo-thesis

On the RPi, you can use build/picomprun.sh to perform a differential build and launch with the correct ALSA device attached.
