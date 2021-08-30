# Contributing

This project welcomes contributions and suggestions. Most contributions require you to
agree to a Contributor License Agreement (CLA) declaring that you have the right to,
and actually do, grant us the rights to use your contribution. For details, visit
https://cla.microsoft.com.

When you submit a pull request, a CLA-bot will automatically determine whether you need
to provide a CLA and decorate the PR appropriately (e.g., label, comment). Simply follow the
instructions provided by the bot. You will only need to do this once across all repositories using our CLA.

This project has adopted the [Microsoft Open Source Code of Conduct](https://opensource.microsoft.com/codeofconduct/).
For more information see the [Code of Conduct FAQ](https://opensource.microsoft.com/codeofconduct/faq/)
or contact [opencode@microsoft.com](mailto:opencode@microsoft.com) with any additional questions or comments.

# Building the WSLg System Distro

The heart of WSLg is what we call the WSL system distro. This is where the Weston compositor, XWayland and the PulseAudio server are running. The system distro runs these components and projects their communication sockets into the user distro. Every user distro is paired with a unique instance of the system distro. There is a single version of the system distro on disk which is instantiated in memory when a user distro is launched.

The system distro is essentially a Linux container packaged and distributed as a vhd. The system distro is accessible to the user, but is mounted read-only. Any changes made by the user to the system distro while it is running are discarded when WSL is restarted. Although a user can log into the system distro, it is not meant to be used as a general purpose user distro. The reason behind this choice is due to the way we service WSLg. When updating WSLg we simply replace the existing system distro with a new one. If the user had data embeded into the system distro vhd, this data would be lost.

For folks who want to tinker with or customize their system distro, we give the ability to run a private version of the system distro. When running a private version of WSLg, Windows will load and run your private and ignore the Microsoft published one. If you update your WSL setup (`wsl --update`), the Microsoft published WSLg vhd will be updated, but you will continue to be running your private. You can switch between the Microsoft pulished WSLg system distro and a private one at any time although it does require restarting WSL (`wsl --shutdown`).

The WSLg system distro is built using docker build. We essentially start from a [CBL-Mariner](https://github.com/microsoft/CBL-MarinerDemo) base image, install various packages, then build and install version of Weston, FreeRDP and PulseAudio from our mirror repo. This repository contains a Dockerfile and supporting tools to build the WSLg container and convert the container into an ext4 vhd that Windows will load as the system distro.

## Build instructions

0. Install and start Docker in a Linux or WSL 2 environment.

```
    sudo apt-get update
    sudo apt install docker.io golang-go
    sudo dockerd
```

1. Clone the WSLg project:

```
    git clone https://github.com/microsoft/wslg wslg
```

2. Clone the FreeRDP, Weston and PulseAudio mirror. These need to be located in a **vendor** sub-directory where you clone the wslg project (e.g. wslg/vendor), this is where our docker build script expect to find the source code. Make sure to checkout the **working** branch from each of these projects, the **main** branch references the upstream code.

    ```bash
    git clone https://github.com/microsoft/FreeRDP-mirror wslg/vendor/FreeRDP -b working
    git clone https://github.com/microsoft/weston-mirror wslg/vendor/weston -b working
    git clone https://github.com/microsoft/PulseAudio-mirror wslg/vendor/pulseaudio -b working
    git clone https://github.com/mesa3d/mesa wslg/vendor/mesa -b 21.0
    ```

2. Create the VHD:

    2.1 From the parent directory where you cloned `wslg` clone `hcsshim` which contains `tar2ext4` and will be used to create the system distro vhd
    ```
    git clone --branch v0.8.9 --single-branch https://github.com/microsoft/hcsshim.git
    ```
    
    2.2 From the parent directory build and export the docker image:
    ```
    sudo docker build -t system-distro-x64  ./wslg  --build-arg SYSTEMDISTRO_VERSION=`git --git-dir=wslg/.git rev-parse --verify HEAD` --build-arg SYSTEMDISTRO_ARCH=x86_64
    sudo docker export `sudo docker create system-distro-x64` > system_x64.tar
    ```
    
    2.3 Create the system distro vhd using `tar2ext4`
    
    ```bash
    cd hcsshim/cmd/tar2ext4
    go run tar2ext4.go -vhd -i ../../../system_x64.tar -o ../../../system.vhd
    ```
    
    This will create system distro image `system.vhd`

## Installing a private version of the WSLg system distro

You can tell WSL to load a private version of WSLg by adding the folowing option in your `.wslconfig` file (located in *c:\users\MyUser\.wslconfig*).

```
    [wsl2]
    ;Do not place VHD under user profile folder for now.
    ;The issue is tracked by https://github.com/microsoft/wslg/issues/427.
    ;systemDistro=C:\\Users\\MyUser\\system.vhd
    systemDistro=C:\\tmp\\system.vhd

```    
    
You need to restart WSL for this change to take effect. From an elevated command prompt execute `wsl --shutdown`. When WSL is launched again, Windows will load your private vhd as the system distro. 
    
## Inspecting the WSLg system distro at runtime

If the system distro isn't working correctly or you need to inspect what is running inside the system distro you can get a terminal into the system distro by running the following command from an elevated command prompt.

```
    wsl --system -d [DistroName]
```
There is an instance of the system distro running for every user distro running. `DistroName` refers to the name of the user distro for which you want the paired system distro. If you omit `DistroName`, you will get a terminal into the system distro paired with your default WSL user distro.

Please keep in mind that the system distro is loaded read-only from it's backing VHD. For example, if you need to install tools (say a debugger or an editor) in the system distro, you want to do this in the Dockerfile that builds the system distro so it gets into the private vhd that you are running. You can dynamically install new packages once your have a terminal into the system distro, but any changes you make will be discarded when WSL is restarted.

## Building a debug version

To build a debug version of the system distro, the docker build argument SYSTEMDISTRO_DEBUG_BUILD needs to be set and passed the value of "true". The following command would substitute the docker build command in step 3.2.2 of the "Build Instructions" section.

```
    sudo docker build -t system-distro-x64  ./wslg  --build-arg SYSTEMDISTRO_VERSION=`git --git-dir=wslg/.git rev-parse --verify HEAD` --build-arg SYSTEMDISTRO_ARCH=x86_64 --build-arg SYSTEMDISTRO_DEBUG_BUILD=true
```
The resulting system distro VHD will have useful development packages installed like gdb and will have compiled all runtime dependencies with the "debug" buildtype for Meson, rather than "release".

# mstsc plugin

On the Windows side of the world, WSLg leverages the native `mstsc.exe` RDP client and a plugin for that client which handles WSLg integration into the start menu. The source code for this plugin is available as open source as part of the WSLg repo [here](https://github.com/microsoft/wslg/tree/main/WSLDVCPlugin).

It was important for us building WSLg to ensure that all protocols between Linux and Windows be fully documented and available to everyone to reuse. While almost all of the communication over RDP between Linux/Weston and Windows goes through standard and officially documented [Windows Protocols](https://docs.microsoft.com/en-us/openspecs/windows_protocols/MS-WINPROTLP/92b33e19-6fff-496b-86c3-d168206f9845) associated with the RDP standard, we needed just a bit of custom communication between Linux and Windows to handle integration into the start menu. We thought about adding some official RDP protocol for this, but this was too specific to WSLg and not broadly applicable to arbitrary RDP based solution.

So instead we opted to use a custom RDP channel between the WSLg RDP Server running inside of Weston and the WSLg RDP plugin hosted by mstsc. Such custom dynamic channel are part of the RDP specification, but requires that both the RDP server and RDP client support that channel for it to be used. This is the path we took for WSLg where Weston exposes a custom RDP channel for WSLg integration. In the spirit of fully documenting all channels of communication between Linux and Windows, we're making the source code for plugin which handles the Windows side of this custom RDP channel available as part of the WSLg project.

This custom channel and associated plugin are quite small and simple. In a nutshell, Weston enumerates all installed applications inside of the user Linux distro (i.e. application which have an explicit [desktop file](https://specifications.freedesktop.org/desktop-entry-spec/desktop-entry-spec-latest.html)) and exposes this list of applications, along with command line to launch them and icon to represent them, over this custom RDP channel. The mstsc plugin processes that list and creates links in the Windows Start Menu for these applications so they can be launch directly from it.

## Building the mstsc plugin

The [source code](https://github.com/microsoft/wslg/tree/main/WSLDVCPlugin) for the plugin has a visual studio project file that can be use to build it. You can download and install the free [Visual Studio Community Edition](https://visualstudio.microsoft.com/vs/community/) to build it.

## Registering a private mstsc plugin

The plugin is registered with mstsc through the registry. By default this is set to load the plugin that ships as part of the official WSLg package. If you need to run a private, you'll nee to modify this registry key to reference your privately built plugin, for example using a registry file like below.

```
Windows Registry Editor Version 5.00

[HKEY_CURRENT_USER\Software\Microsoft\Terminal Server Client\Default\AddIns\WSLDVCPlugin]
"Name"="C:\\users\\MyUser\\Privates\\WSLDVCPlugin.dll"
```
