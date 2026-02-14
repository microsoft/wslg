# Introduction

This repository contains a Dockerfile and supporting tools to build the WSL GUI system distro image.

## Quick start

For self-hosting WSLG check use this instructions https://github.com/microsoft/wslg/wiki#installing-self-hosting

## Setup and build WSLG and System Distro

0. Install and start Docker in a Linux or WSL2 environment.

1. Clone the FreeRDP ,Weston and PulseAudio side by side this repo repositories and checkout the "working" branch from each:

    ```bash
    git clone https://github.com/microsoft/FreeRDP-mirror vendor/FreeRDP -b working

    git clone https://github.com/microsoft/weston-mirror.git vendor/weston -b working

    git clone https://github.com/microsoft/pulseaudio-mirror.git vendor/pulseaudio -b working
    ```

2. Download the mesa and directx headers code.

    ```
    wget https://archive.mesa3d.org/mesa-25.3.4.tar.xz
    tar -xf mesa-25.3.4.tar.xz -C vendor
    mv vendor/mesa-25.3.4 vendor/mesa

    wget https://github.com/microsoft/DirectX-Headers/archive/refs/tags/v1.608.0.tar.gz
    tar -xvf v1.608.0.tar.gz -C vendor
    mv vendor/DirectX-Headers-1.608.0 vendor/DirectX-Headers-1.0
    ```

3. Create the VHD:

    3.1 From the parent directory where you cloned `wslg` clone `hcsshim` which contains `tar2ext4` and will be used to create the system distro vhd
    ```
    git clone --branch v0.8.9 --single-branch https://github.com/microsoft/hcsshim.git
    ```
    
    3.1 From the parent directory build and export the docker image:
    ```
    sudo docker build -t system-distro-x64  ./wslg  --build-arg SYSTEMDISTRO_VERSION=`git --git-dir=wslg/.git rev-parse --verify HEAD` --build-arg SYSTEMDISTRO_ARCH=x86_64
    sudo docker export `sudo docker create system-distro-x64` > system_x64.tar
    ```
    
    3.3 Create the system distro vhd using `tar2ext4`
    
    ```bash
    cd hcsshim/cmd/tar2ext4
    go run tar2ext4.go -vhd -i ../../../system_x64.tar -o ../../../system.vhd
    ```
    
    This will create system distro image `system.vhd`

4. Change the system distro:

    4.1 Before replace the system distro you will need to shutdown WSL
    
    ```
    wsl --shutdown
    ```
    
    4.2 By default the system distro is located at `C:\ProgramData\Microsoft\WSL\system.vhd`
    
    If you want to use the system distro from a different path you can change the .wslconfig.

    * Add an entry to your `%USERPROFILE%\.wslconfig`

    ```
    [wsl2]
    systemDistro=C:\\tmp\\system.vhd
    ```
    
    4.3 After update the system distro you should be able to launch any user distro and WSL will automatically launch the system distro along with the user distro.
    

5. Inspecting the system distro:

    If the system distro isn't working correctly or you need to inspect what is running inside the system distro you can do:

    ```
    wsl --system [DistroName]
    ```

    For instance you should check if weston and pulse audio are running inside the system distro using `ps -ax | grep weston` or `ps -ax | grep pulse`
    You should see something like this:
    
    ```bash
    root@DESKTOP-7LJ03SK:/mnt/d# ps -ax | grep weston
   11 ?        Sl     6:51 /usr/local/bin/weston --backend=rdp-backend.so --xwayland --shell=rdprail-shell.so --log=/mnt/wslg/weston.log
    ```
