# v4l2capture
linux v4l2 capture sample

# This lib dep on v4l and glib , must:
sudo apt install libv4lconvert0  libv4l-dev libv4l-0 libglib2.0-dev

# Use sdl2 in deamon to do render and scons as build tools, so should:
sudo apt install libsdl2-dev scons

# build
scons -Q

# two example, first store captured raw data into files , second you can see video directlly

#capture 640x360 420p yuv 150 frames into file
#you can use cmd play: "ffplay -loop 3 -f rawvideo -video_size 640*360 -pixel_format yuv420p -framerate 24 snapshot.yuv" 
./cam_capture_store_example      

#capture and render it

./cam_capture_demon



