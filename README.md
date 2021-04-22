# Welcome to WSLg
WSLg is short for *Windows Subsystem for Linux GUI* and the purpose of the project is to enable support for running Linux GUI applications (X11 and Wayland) on Windows in a fully integrated desktop experience. 

WSLg provides an integrated experience for developers, scientists or enthusiasts that prefer or need to run Windows on their PC but also need the ability to run tools or applications which works best, or exclusively, in a Linux environment. While users can accomplish this today using a multiple system setup, with individual PC dedicated to Windows and Linux, virtual machine hosting either Windows or Linux, or an XServer running on Windows and projected into WSL, WSLg provides a more integrated, user friendly and productive alternative.

WSLg strives to make Linux GUI applications feel native and natural to use on Windows. From integration into the Start Menu for launch to appearing in the task bar, alt-tab experience to enabling cut/paste accross Windows and Linux applications, WSLg enables a seamless desktop experience and workflow leveraging Windows and Linux applications.

![WSLg Integrated Desktop](/docs/WSLg_IntegratedDesktop.png)

# Installing WSLg

## Pre-requisites

- Windows 10 Insider Preview build 21362+
   - WSLg is going to be generally available alongside the upcoming release of Windows. To get access to a preview of WSLg, you'll need to join the [Windows Insider Program](https://insider.windows.com/en-us/) and be running a Windows 10 Insider Preview build from the dev channel.

- It recommended to run WSLg on a system with virtual GPU (vGPU) enabled for WSL so you can benefit from hardware accelerated OpenGL rendering. You can find preview driver supporting WSL from each of our partners below.

   - [AMD GPU driver for WSL](https://community.amd.com/community/radeon-pro-graphics/blog/2020/06/17/announcing-amd-support-for-gpu-accelerated-machine-learning-training-on-windows-10)
   - [Intel GPU driver for WSL](https://downloadcenter.intel.com/download/29526)

   - [NVIDIA GPU driver for WSL](https://developer.nvidia.com/cuda/wsl)
     
   
## Install instructions (Fresh Install - no prior WSL installation)

From a command prompt with administrator privileges, run the command `wsl --install -d Ubuntu`, then reboot when prompted.

After reboot the installation will continue. You'll be asked to enter a username and password. These will be your Linux credential, they can be anything you want and don't have to match your Windows credentials.

Voila! WSL and WSLg are installed and ready to be used!

## Install instructions (Existing WSL install)

If you have an existing WSL installation without WSLg and want to update to the latest version of WSL which includes WSLg, run the command `wsl --update` from an elevated command prompt. 

Please note that WSLg is only compatible with WSL 2 and will not work for WSL distribution configured to work in WSL 1 mode. Verify that your Linux distro is configured for running in WSL 2 mode, if not switch to WSL 2. While you can continue to run Linux distro in WSL 1 mode after installing WSLg if you so desired, a distro configured to run in WSL 1 mode will not be able to communicate with WSLg and will not be able to run GUI applications.

You can list your currently installed distro and the version of WSL they are configured for using the following command from an elevated command prompt.

```powershell
   wsl --list -v
```
If running in version 1 mode, switch to version 2. This can take a while.

```powershell
   wsl --set-version _distro_name_ 2
```

Restart WSL by running this command from an elevated command prompt, make sure to save any pending work first:

```powershell
    wsl --shutdown
```

## Updating WSL + WSLg

To update to the latest version of WSL and WSLg released for preview, simply run `wsl --update` from an elevated command prompt or powershell. 

You'll need to restart WSL for the changes to take effect. You can restart WSL by running `wsl --shutdown` from an elevated command prompt. If WSL was currently running, it will shutdown, make sure to first save any in progress work! WSL will be automatically restarted the next time you launch a WSL application or terminal.

## First Launch

If you have installed the `Ubuntu` Linux distro per these instructions, you'll find an `Ubuntu` icon in your start menu, launch it. This will launch the WSL 2 VM, launch the Ubuntu WSL distro in that VM and give you a terminal to interact with it. Voila! You're running Linux on Windows! 

If you would like to explore additional Linux distributions built for WSL, you can use the `wsl --list --online` command from an elevated command prompt to enumerate the list of available distributions for your system. You can have multiple Linux distributions installed within WSL and they will happily coexist side-by-side, so don't be scare to experiment and try things out.

Congrats you are done and ready to use GUI apps! 

## Install and run GUI apps

If you want to get started with some GUI apps, you can run the following commands from your Linux terminal to download and install some popular applications. If you are using a different distribution than Ubuntu, it may be using a different package manager. 

```powershell

## Update all packages in your distro
sudo apt update

## Gedit
sudo apt install gedit -y

## GIMP
sudo apt install gimp -y

## Nautilus
sudo apt install nautilus -y

## VLC
sudo apt install vlc -y

## X11 apps
sudo apt install x11-apps -y

## Google Chrome
cd /tmp
sudo wget https://dl.google.com/linux/direct/google-chrome-stable_current_amd64.deb
sudo dpkg -i google-chrome-stable_current_amd64.deb 
sudo apt install --fix-broken -y
sudo dpkg -i google-chrome-stable_current_amd64.deb

## Microsoft Teams
cd /tmp
sudo curl -L -o "./teams.deb" "https://teams.microsoft.com/downloads/desktopurl?env=production&plat=linux&arch=x64&download=true&linuxArchiveType=deb"
sudo apt install ./teams.deb -y

## Microsoft Edge Browser
sudo curl https://packages.microsoft.com/repos/edge/pool/main/m/microsoft-edge-dev/microsoft-edge-dev_91.0.852.0-1_amd64.deb -o /tmp/edge.deb
sudo apt install /tmp/edge.deb -y
```

Once these applications are installed, you'll find them in your start menu under the distro name. For example `Ubuntu -> Microsoft Edge`. You can also launch these from your terminal window using the commands:

* `xcalc`, `xclock`, `xeyes` 
* `gimp`
* `gedit ~/.bashrc` 
* `nautilus`
* `vlc`
* `google-chrome`
* `teams`
* `microsoft-edge`

# WSLg Architecture Overview

![WSLg Architecture Overview](/docs/WSLg_ArchitectureOverview.png)

## User Distro
The user distro is essentially the WSL distribution you are using for your Linux work. You can use the command `wsl --list --online` from an elevated Windows command prompt to list the WSL distributions available on your system. You can run multiple user distros side-by-side and they will peacefully coexist, so don't be afraid of trying out new distro. Each user distro will be paired with a unique instance of the system distro, but you can still interact accross GUI applications running in different user distro seamlessly, such as cut/paste between them. The underlying containerization of the various userspace should be invisible to you.

All user and system distros for a particular Windows user run within the same WSL virtual machine against a single instance of the Linux kernel. Different Windows users on a PC have their own VM and instance of WSL. Your Linux environment is guaranteed to always be your own and not shared with other Windows users on the same PC.

## WSLg System Distro
The system distro is where all of the magic happens. The sytem distro is a containerized Linux environment where the WSLg XServer, Wayland server and Pulse Audio server are running. Communication socket for each of these servers are projected into the user distro so client applications can connect to them. We preconfigure the user distro environment variables DISPLAY, WAYLAND_DISPLAY and PULSE_SERVER to refer these servers by default so WSLg lights up out of the box.

Users wanting to use different servers than the one provided by WSLg can change these environment variables. User can also choose to turn off the system distro entirely by adding the following entry in their `.wslconfig` file (located at `c:\users\MyUser\.wslconfig`). This will turn off support for GUI applications in WSL.

```
[wsl2]
guiApplications=false
```

The system distro is based on the Microsoft [CBL-Mariner Linux](https://github.com/microsoft/CBL-Mariner). This is a minimal Linux environment, just enough to run the various pieces of WSLg. For details on how to build and deploy a private system distro please see our [build instructions](CONTRIBUTING.md).

Every WSL 2 user distro is paired with its own instance of the system distro. The system distro runs partially isolated from the user distro to which it is paired, in it's own NS/PID/UTS namespace but shares other namespaces such as IPC, to allow for shared memory optimization across the boundary. 

While a user can get a terminal into the system distro, the system distro is not meant to be used directly by users. Every instance of the system distro is loaded read-only from it's backing VHD. Any modifications, made to the in-memory instance of the system distro (such as installing new packages or creating a new file), is effectively discarded when WSL is restarted. The reason we do this is to enable a servicing model for the system distro where we replace the old one with the new one without having to worry about migrating any user data contained within. We use a read-only mapping such that the user gets a well known discard behavior on any changes, every time WSL is restarted, instead of getting a surprise when WSL is serviced. 

Although the Microsoft published WSLg system distro as read-only, we do want to encourage folks to tinker with it and experiment. Although we expect very few folks to actually need or want to do that, we've shared detailed instruction on our [contributing](CONTRIBUTING.md) page on how to both build and deploy a private version of the system distro. Most users who just want to use GUI applications in WSL don't need to worry about those details.

## WSLGd
**WSLGd** is the first process to launch after **init**. **WSLGd** launches **Weston** (with Xwayland), **PulseAudio** and establishes the RDP connection by launching **mstsc.exe** on the host in silent mode. The RDP connection will remain active and ready to show a new GUI applications being launch on a moment's notice, without any connection establishment delays. **WSLGd** then monitors these processes and if they exit by error (say as a result of a crash), it automatically restarts them.

## Weston
Weston is the Wayland project reference compositor and the heart of WSLg. For WSLg, we've extended the existing RDP backend of libweston to teach it how to remote applications rather than monitor/desktop. We've also added various functionality to it, such as support for multi-monitor, cut/paste, audio in/out, etc...

The application integration is achieved through an RDP technology called RAIL (Remote Application Integrated Locally) and VAIL (Virtualized Application Integrated Locally). The main difference between RAIL and VAIL is how pixels are transported across from the RDP server to the RDP client. In RAIL, it is assumed that the Server and Client are running on different physical systems communicating over the network and thus pixels need to be copied over the RDP transport. In VAIL, it is understood that the Server and Client are on the same physical system and can share memory across the Guest/Host VM boundary. We've added support for both RAIL and VAIL to the libweston RDP backend, although for WSLg only the VAIL support is effectively used. While building WSLg, we first implemented RAIL while the necessary pieces enabling the switch to VAIL were being developed in parallel. We decided to keep that support in as it could reuse in other interesting scenarios outside of WSLg, for example for remoting application from a Pi running Linux. To share memory between the Linux guest and Windows host we use virtio-fs.

### RAIL-Shell
Weston is modular and has various shells today, such as the desktop shell, fullscreen shell (aka kiosk), and automative shell. For WSLg we introduced a new shell called the RAIL Shell. The purpose of the RAIL Shell is to help with the remoting of individual windows from Linux to Windows, as such the shell is very simplistic and doesn't involve any actual widgets or shell owned pixels.

## FreeRDP
Weston leverages FreeRDP to implement its backend RDP Server. FreeRDP is used to encode all communications going from the RDP Server (in Weston) to the RDP Client (mstsc on Windows) according to the RDP protocol specifications. It is also used to decode all traffic coming from the RDP Client into the RDP server.

## Pulse Audio Plugin
For audio in (microphone) and out (speakers/headphone) WSLg runs a PulseAudio server. WSLg uses a [sink plugin](https://github.com/microsoft/pulseaudio-mirror/blob/working/src/modules/rdp/module-rdp-sink.c) for audio out, and a [source plugin](https://github.com/microsoft/pulseaudio-mirror/blob/working/src/modules/rdp/module-rdp-source.c) for audio in. These plugins effectively transfer audio samples between the PulseServer and the Weston RDP Server. The audio streams are merged by the Weston RDP Server onto the RDP transport, effectively enabling audio in/out in the Weston RDP backend across all scenarios (Desktop/RAIL/VAIL style remoting), including WSLg.

## WSL Dynamic Virtual Channel Plugin (WSLDVCPlugin)
WSLg makes use of a custom RDP virtual channel between the Weston RDP Server and the mstsc RDP Client running on the Windows host. This channel is used by Weston to enumerate all Linux GUI applications (i.e. applications which have a desktop file entry of type gui) along with their launch command line and icon. The open source [WSLDVCPlugin](https://github.com/microsoft/wslg/tree/main/WSLDVCPlugin) processes the list of Linux GUI applications sent over this channel and creates links for them in the Windows start menu.

# OpenGL accelerated rendering in WSLg

While WSLg works with or without virtual GPU support, if you intend to run graphics intensive applications such as Blender or Gazebo, it is best to be running on a system with a GPU and driver that can support WSL. An overview of our vGPU architecture and how we make it possible for Linux applications to access the GPU in WSL is available at our [DirectX blog](https://devblogs.microsoft.com/directx/directx-heart-linux/).

Support for OpenGL accelerated rendering is made possible through the work our D3D team has done with Collabora and the Mesa community on creating a [d3d12 Gallium driver](https://devblogs.microsoft.com/directx/in-the-works-opencl-and-opengl-mapping-layers-to-directx/). 

Support for Linux, including support for WSLg, has been upstream and part of the Mesa 21.0 release. To take advantage of this acceleration, you'll need to update the version of Mesa installed in your user distro. It also requires that your distro vendor chose to build and publish the new d3d12 Gallium driver to their package repository. We're working with the various WSL distro publishers to inform them of these changes.

Please note that for the first release of WSLg, vGPU interops with the Weston compositor through system memory. If running on a discrete GPU, this effectively means that the rendered data is copied from VRAM to system memory before being presented to the compositor within WSLg, and uploaded onto the GPU again on the Windows side. As a result, there is a performance penalty proportionate to the presentation rate. At very high frame rates such as 600fps on a discrete GPU, that overhead can be as high as 50%. At lower frame rate or on integrated GPU, performance much closer to native can be achieved depending on the workload. Using a vGPU still provides a very significant performance and experience improvement over using a software renderer despite this v1 limitation.

# WSLg Code Flow
WSLg builds on the great work of the Linux community and makes use of a large number of open source projects. Most components are used as-is from their upstream version and didn't require any changes to light up in WSLg. Some components at the heart of WSLg, in particular Weston, FreeRDP and PulseAudio, required changes to enable the rich WSLg integration. These changes aren't yet upstream. Microsoft is working with the community to share these contributions back with each project such that overtime WSLg can be built from upstream component directly, without the need for any WSLg specific modifications.

All of these in-flight contributions are kept in Microsoft mirror repos. We keep these mirrors up to date with upstream releases and stage our WSLg changes in those repos. WSLg pulls and builds code from these mirror repos as part of our Insider WSLg Preview releases. These mirrors are public and accessible to everyone. Curious developer can take a peek at early stages of our contribution by looking at code in those mirrors, keeping in mind that the final version of the code will likely look different once the contribution reaches the upstream project and is adapted based on the feedback receives by the various project owners. All of our mirrors follow the same model. There is a **main** branch which correspond to the upstream branch at our last synchronization point. We update the **main** branch from time to time to pick update from the upstream project. There is also a **working** branch that contains all of our in-flight changes. WSLg is built using the **working** branch from each of the mirror projects.

The projects that WSLg maintains mirrors for will change over time as in-flight contributions evolve. Once some contributions are upstream, it may no longer be necessary to maintain a mirror, at which point it will be removed and WSLg will start to leverage the upstream version of the component directly. As we light up new functionality in WSLg, new mirrors may be introduced to stage contributions to new components. As such, expect the list of mirrors to change overtime.

At this point in time, we have the following project mirrors for currently in-flight contributions.

| Project | Upstream Repo | WSLg Mirror |
|---|---|---|
| Weston | https://github.com/wayland-project/weston | https://github.com/microsoft/Weston-mirror|
| FreeRDP | https://github.com/FreeRDP/FreeRDP | https://github.com/microsoft/FreeRDP-mirror |
| PulseAudio | https://github.com/pulseaudio/pulseaudio | https://github.com/microsoft/PulseAudio-mirror |

The following is a high level overview of the currently in-flight contributions to each project contained within these mirrors.

## Weston
WSLg leverages Weston as the Wayland compositor bridging the Linux and Windows worlds using RDP technology to remote application content between them. Weston already had an RDP backend, but it was limited to single-monitor-desktop remoting. We've greatly enchanced that RDP backend to include advanced functionality, such as multi-monitor support, clipboard integration for copy/paste, and audio in/out. We've enabled new remoting modes called RAIL (Remote Application Integrated Locally) and VAIL (Virtualized Application Integrated Locally), where individual applications, rather than desktops/monitors, are remoted. These changes are not specific to WSLg; they add functionality to the existing RDP backend and are reusable in other scenarios as well (i.e. using the new Weston RDP backend to remote application running on a Raspberry Pi to another device running an RDP client).

To enable rich integration in WSLg, we've also added a small plugin to the RDP backend specific to WSLg. In Weston, the plugin is responsible for attaching to the user distro and searching for installed applications (aka the desktop file). The plugin sends the Windows host a list of all applications found along with their launch commands and icons. On the Windows host, an open source [mstsc plugin](https://github.com/microsoft/wslg/tree/main/WSLDVCPlugin) part of the WSLg project uses that information to create shortcuts for these Linux applications to the Windows Start Menu.

We've also fixed several bugs impacting various applications. Generally, these were problems that impacted Weston in all modes and were not specific to WSLg.

## FreeRDP
Weston currently uses FreeRDP for its RDP Backend. WSLg continues to leverage FreeRDP and we have added support for a new RDP Protocol/Channel to enable VAIL optimized scenario as well as support for the WSLg plugin. We've also fixed various bugs that were impacting interops with mstsc or causing instability.

## PulseAudio
For PulseAudio, our contributions focused on a sink and a source plugin that shuffle audio data between PulseAudio and the Weston RDP backend such that the audio data can be integrated over the RDP connection back to the host. There are no changes to the core of PulseAudio outside of adding these new plugins.

# Contributing

If you would like to tinker with or contribute to WSLg, please see our [CONTRIBUTING](CONTRIBUTING.md) page for details, including how to build and run private a version of WSLg.

# Reporting a non-security issues

For non-security related issues, such as reporting a bug or making a suggestion for a new feature, please use this project's [issues tracker](https://github.com/microsoft/wslg/issues).

# Reporting security issues

To report security issues with WSLg or any other Microsoft products, please follow the instructions detailed [here](SECURITY.md).

# Trademarks
This project may contain trademarks or logos for projects, products, or services. Authorized use of Microsoft trademarks or logos is subject to and must follow Microsoft's Trademark & Brand Guidelines. Use of Microsoft trademarks or logos in modified versions of this project must not cause confusion or imply Microsoft sponsorship. Any use of third-party trademarks or logos are subject to those third-party's policies.
