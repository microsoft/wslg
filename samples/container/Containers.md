# Containerizing GUI applications with WSLg

For containerized applications to work properly under WSLg developers need to be aware of a few peculiarity of our environment in order to allow applications to properly connect to our X11, Wayland or PulseAudio server or to use the vGPU.

## Containerized GUI applications connecting to X11, Wayland or Pulse server

In order for a containerized application to access the servers provided by WSLg, the following mount location must be made visible inside the container.

| Server | Mount |
|---|---|
| X11 | ```/tmp/.X11-unix``` |
| Wayland | ```/mnt/wslg``` |
| PulseAudio | ```/mnt/wslg``` |

And the following environment variable must be share with the container.

| Server | Environment variables |
|---|---|
| X11 | ```DISPLAY``` |
| Wayland | ```WAYLAND_DISPLAY``` && ```XDG_RUNTIME_DIR``` |
| PulseAudio | ```PULSE_SERVER``` |

For example, to run ```xclock``` as a containerized application, the following docker file can be use. 

```
FROM ubuntu:20.04 as runtime

ARG DEBIAN_FRONTEND=noninteractive

RUN apt update && \
   apt install -y x11-apps

CMD /usr/bin/xclock
```

The container can be build then launch as follow.

```
sudo docker build -t xclock -f Dockerfile.xclock .
sudo docker run -it -v /tmp/.X11-unix:/tmp/.X11-unix -v /mnt/wslg:/mnt/wslg \
    -e DISPLAY=$DISPLAY -e WAYLAND_DISPLAY=$WAYLAND_DISPLAY \
    -e XDG_RUNTIME_DIR=$XDG_RUNTIME_DIR -e PULSE_SERVER=$PULSE_SERVER xclock
```

Please note that in this example we make all servers visible to ```xclock``` even though it only uses the X11 server and will not make use of the Wayland or PulseAudio servers. This is for illustrative purposes only. There is no real harm in exposing a server that is unused by an application. However it is good practice to only exposed containerized application to the resource they need. In this case we could have launch the containerized version of ```xclock``` with the following minimal command line.

```
sudo docker run -it -v /tmp/.X11-unix:/tmp/.X11-unix -e DISPLAY=$DISPLAY xclock
```

## Containerized applications access to the vGPU

For a containerized application to use the vGPU provided by WSL2, the following must be done.

The following device must be shared with the container.

```/dev/dxg```

The following mount location must be mapped in the container.

```/usr/lib/wsl```

The following path must be added to the ```LD_LIBRARY_PATH``` environment variable inside the container.

```/usr/lib/wsl/lib```

For example the following dockerfile containerized glxinfo. 

```
FROM ubuntu:20.04 as runtime

ARG DEBIAN_FRONTEND=noninteractive

RUN apt update && \
   apt install -y mesa-utils

ENV LD_LIBRARY_PATH=/usr/lib/wsl/lib
CMD /usr/bin/glxinfo -B
```

This container can be build and launch as follow.

```
sudo docker build -t glxinfo -f Dockerfile.glxinfo .
sudo docker run -it -v /tmp/.X11-unix:/tmp/.X11-unix -v /mnt/wslg:/mnt/wslg \
    -v /usr/lib/wsl:/usr/lib/wsl --device=/dev/dxg -e DISPLAY=$DISPLAY \
    -e WAYLAND_DISPLAY=$WAYLAND_DISPLAY -e XDG_RUNTIME_DIR=$XDG_RUNTIME_DIR \
    -e PULSE_SERVER=$PULSE_SERVER --gpus all glxinfo
```

## Containerized applications access to vGPU accelerated video

For a containerized application to use vGPU video acceleration, the following must be done.

The following devices must be shared with the container.

```/dev/dxg```

```/dev/dri/card0```

```/dev/dri/renderD128```

The following mount location must be mapped in the container.

```/usr/lib/wsl```

The following path must be added to the ```LD_LIBRARY_PATH``` environment variable inside the container.

```/usr/lib/wsl/lib```

The following VA-API driver name must be set to the ```LIBVA_DRIVER_NAME``` environment variable inside the container.

```d3d12```

The following libraries must be installed in the container.

```
  vainfo
  mesa-va-drivers
```
For example the following dockerfile containerized `videoaccel`.

```
FROM ubuntu:22.10 as runtime

ARG DEBIAN_FRONTEND=noninteractive

# Uncomment the lines below to use a 3rd party repository
# to get the latest (unstable from mesa/main) mesa library version
# RUN apt-get update && apt install -y software-properties-common
# RUN add-apt-repository ppa:oibaf/graphics-drivers -y

RUN apt update && apt install -y \
    vainfo \
    mesa-va-drivers

ENV LIBVA_DRIVER_NAME=d3d12
ENV LD_LIBRARY_PATH=/usr/lib/wsl/lib
CMD vainfo --display drm --device /dev/dri/card0
```

This container can be build and launch as follow.

```
sudo docker build -t videoaccel -f Dockerfile.videoaccel .
sudo docker run -it -v /tmp/.X11-unix:/tmp/.X11-unix -v /mnt/wslg:/mnt/wslg \
    -v /usr/lib/wsl:/usr/lib/wsl --device=/dev/dxg -e DISPLAY=$DISPLAY \
    --device /dev/dri/card0 --device /dev/dri/renderD128 \
    -e WAYLAND_DISPLAY=$WAYLAND_DISPLAY -e XDG_RUNTIME_DIR=$XDG_RUNTIME_DIR \
    -e PULSE_SERVER=$PULSE_SERVER --gpus all videoaccel
```
