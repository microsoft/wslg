
|  | Status | 
| :------ | :------: | 
| Build (Ubuntu) | [![Build Status](https://microsoft.visualstudio.com/DxgkLinux/_apis/build/status/wslg?branchName=master&jobName=Build%20(Ubuntu))](https://microsoft.visualstudio.com/DxgkLinux/_build/latest?definitionId=55786&branchName=master) |
| Package MSI (Windows) | [![Build Status](https://microsoft.visualstudio.com/DxgkLinux/_apis/build/status/wslg?branchName=master&jobName=Package%20(Windows))](https://microsoft.visualstudio.com/DxgkLinux/_build/latest?definitionId=55786&branchName=master) |

# Install WSLg

## Pre-requisites

- Windows 10 *Cobalt* build 21254 or higher
   - Please note that this is not yet released to Windows Insiders, and you will need to be on an internal Microsoft branch like rs_onecore_sigma, this will not work on *release* branch such as rs_prerelease, fe_release, fe_release_sigma, etc...

- It is strongly recommended to run WSLg on a system with vGPU enabled by installing the recommend GPU driver below, otherwise performance will suffer. See `Enabling Shared Memory Remoting Optimization` for more details.

   - [Intel GPU driver for WSL](https://downloadcenter.intel.com/download/29526)

   - [NVIDIA GPU driver for WSL](https://developer.nvidia.com/cuda/wsl)

   - [AMD GPU driver for WSL](https://community.amd.com/community/radeon-pro-graphics/blog/2020/06/17/announcing-amd-support-for-gpu-accelerated-machine-learning-training-on-windows-10)
   
   - Note that Qualcomm does not currently offer support for vGPU in WSL, thus shared memory optimization will not be available at this time on arm64 devices such as Surface Pro X.

## Install instructions (Fresh Install - no prior WSL installation)

From a command prompt with administrator privileges, run the command `wsl --install`, then reboot when prompted.

After reboot the installation will continue. You'll be asked to enter a username and password. These will be your Linux credential, they can be anything you want and don't have to match your Windows credentials.

Once installation is complete, run the command `wsl --update` from an elevated command prompt. This will put you onto the latest WSL+WSLg train and ensure you have the latest selfhost bits.

Voila! WSL and WSLg are installed and ready to be used!

## Install instructions (Existing WSL install or pre-selfhost WSLg install)

If you have an existing WSL installation without WSLg or have installed a pre-selfhost WSLg package (pre 0.2.9) you will need to do these one time manual steps in order to get onto the correct WSL + WSLg update train. After these steps, you'll be able to continue updating both WSL + WSLg through the simple `wsl --update` mechanism and will not have to repeat these.

* Verify that your Linux distro are configured for running in WSL 2 mode, if not switch to WSL 2. While you can continue to run Linux distro in WSL 1 mode after installing WSLg if you so desired, distro configured to run in WSL 1 mode will not be able to communicate with WSLg and will not be able to run GUI applications.

```powershell
   wsl --list -v
```
If running in version 1 mode, switch to version 2. This can take a while.

```powershell
   wsl --set-version _distro_name_ 2
```

* Shutdown your WSL instance and the WSL 2 VM by running this command from an elevated PowerShell or CMD:

```powershell
    wsl --shutdown
```

* If you have a previous version of WSLg installed, uninstall it. Go to `Add or removed programs` and remove the application called `Windows Subsystem for Linux GUI App Support`.

* Update your base WSL install by using the `wsl --update` from an elevated command prompt. This is important to pick up the latest WSL 2 Linux Kernel version.

* Manually download and install the WSLg package [x64](https://wslstorestorage.blob.core.windows.net/wslblob/wsl_graphics_update_x64.msi) or [arm64](https://wslstorestorage.blob.core.windows.net/wslblob/wsl_graphics_update_arm64.msi)

## Updating WSL + WSLg

To update to the latest version of WSL and WSLg released for selfhost, simply run `wsl --update` from an elevated command prompt or powershell. 

You'll need to restart WSL for the changes to take effect. You can restart WSL type running `wsl --shutdown`. If WSL was currently running, it will shutdown, make sure to first save any in progress work! WSL will be automatically restarted the next time you launch a WSL application or terminal.

## First Launch

If you have installed the default Linux distro per these instructions, you'll find a `Ubuntu` icon in your start menu, launch it. This will launch the WSL 2 VM, launch the Ubuntu WSL distro in that VM and give you a terminal to interact with it. Voila! You're running Linux on Windows! 

Ubuntu is the default distro if you took the easy `wsl --install` route to setup your WSL environment. If you would like to try additional Linux distribution, search in the Microsoft Store for "WSL distribution" and install your favorite.

Congrats you are done and ready to use GUI apps! 

## Install and run GUI apps

If you want to get started with some GUI apps, you can run the following commands from your Linux terminal. If you are using a different distribution than Ubuntu, it may be using a different package manager. 

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
sudo curl https://packages.microsoft.com/repos/edge/pool/main/m/microsoft-edge-dev/microsoft-edge-dev_88.0.673.0-1_amd64.deb -o /tmp/edge.deb
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

## Known issues

* When launching an application from the start menu, there will be an extra command prompt launch and sticking around until you close the GUI application. Ignore this for the time being, we're waiting on new console functionality that will allow us to avoid this while keeping wsl.exe a console application.
* All graphics rendering on the Linux side is done on the CPU, no vGPU acceleration are currently available.
* Performance is not representive of the final solution. It is strongly recommended to run with shared memory enabled. Even in shared memory mode, the remoting path is not currently optimal and still involve extra copies on critical threads. This is being worked on.
* No High-DPI support / scaling yet. If you run on a high-dpi device (e.g. Surface Book), things will be small!
* Touch support is not yet completed. Better to stick with mouse and keyboard, your mileage with touch will vary.
* When uninstalling WSL, start menu isn't cleaned up.

## Enabling Shared Memory Remoting Optimization

To improve the current performance of WSLg it is strongly recommended to follow these steps to enable shared memory on your WSLg installation.

Although WSLg will not currently leverage vGPU for accelerated graphics rendering, it leverages vGPU for shared memory based graphics remoting between the Linux guest and Windows host while the Hyper-V team is working on a more official, works everywhere, no dependencies on vGPU, equivalent solution. Once this official support is available, remoting based on shared memory will be enabled ubiquitously on all systems, irrespective of the availability of vGPU. Enabling shared memory is especially important if you are running on a High DPI Laptop such as a Surface Book or Surface Pro as otherwise performance will not be acceptable for a good experience. Note that vGPU is only supported on recent GPU from each of our partners. If your GPU is too old, shared memory optimization will unfortunately not be available to you at this time. 

### Install a vGPU WSL enabled driver

Download the latest WSL enabled driver from one of the link below depending on your GPU manufacturer. These are preview / beta driver that enables vGPU inside of WSL. This functionality is getting streamlined into WDDMv3.0 driver for Cobalt and will eventually be included in drivers pre-installed on new system and on driver downloaded from Windows Update on supported GPU. For the time being you'll need to manually install those drivers. Make sure to save these drivers in a local folder for easy access. When taking a new flight, Windows Update will often reset your driver to the default one curently being flighted. You'll need to re-install the driver again when this happens. You can see the version of driver you are currently using by running `dxdiag.exe` and looking under the display tab at the Drivers Version.

[Intel GPU driver for WSL](https://downloadcenter.intel.com/download/29526)

[NVIDIA GPU driver for WSL](https://developer.nvidia.com/cuda/wsl)

[AMD GPU driver for WSL](https://community.amd.com/community/radeon-pro-graphics/blog/2020/06/17/announcing-amd-support-for-gpu-accelerated-machine-learning-training-on-windows-10)

Once the driver is installed you can verify that vGPU is available in WSL by using the following command in your Linux terminal.

```
ls /dev/dxg
```
If the device is listed, you are running with vGPU enabled! 

You can further verify that shared memory was properly enabled by running the following command in a Linux terminal.

```
cat /mnt/wslg/weston.log | grep GuestAllocInitialize
```

You should see something like this:

```
spronovo@spronovo-book:/tmp$ cat /mnt/wslg/weston.log | grep GuestAllocInitialize
[21:22:52.789] GuestAllocInitialize succeeded
```

## Contribute

For instructions on how to build WSLg please check [BUILD.md](./config/BUILD.md)
