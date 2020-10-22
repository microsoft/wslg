
|  | Status | 
| :------ | :------: | 
| Build (Ubuntu) | [![Build Status](https://microsoft.visualstudio.com/DxgkLinux/_apis/build/status/wslg?branchName=master&jobName=Build%20(Ubuntu))](https://microsoft.visualstudio.com/DxgkLinux/_build/latest?definitionId=55786&branchName=master) |
| Package MSI (Windows) | [![Build Status](https://microsoft.visualstudio.com/DxgkLinux/_apis/build/status/wslg?branchName=master&jobName=Package%20(Windows))](https://microsoft.visualstudio.com/DxgkLinux/_build/latest?definitionId=55786&branchName=master) |
# WSLg

## Install WSLg

### Pre-requisites

- Windows 10 Build 20240 or higher
   - Please note that this is not yet released, and you will need to be on an internal Microsoft branch like rs_onecore_sigma, this will not work on rs_prerelease

### Install instructions (WSL is not already installed)

From a command prompt with administrator privileges, run the command `wsl --install`, then reboot.

After reboot the installation will continue. You'll be asked to enter a username and password. These will be your Linux credential, they can be anything you want and don't have to match your Windows credentials.

### Install instructions (WSL is currently installed or to upgrade WSLg)

Soon you'll be able to simply run `wsl --update` from an elevated command prompt and get the latest released version of WSLg installed.

For the time being, you'll need to manually install the WSLg MSI package on top of your existing WSL installation, but don't worry it's easy.

* Verify that you are running in WSL 2 mode. If not switch to WSL 2

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

* Download and install the latest MSI from the [Releases page](https://github.com/microsoft/wslg/releases). 

### First Launch

If you have installed the default Linux distro per these instructions, you'll find a `Ubuntu` icon in your start menu, launch it. This will launch the WSL 2 VM, launch the Ubuntu WSL distro in that VM and give you a terminal to interact with it. Voila! You're running Linux on Windows! 

When you first launch WSL, you may be prompted to connect to `weston-terminal`. Click `Connect` to finalize this. We are working to remove this dialogue box from the RDP client that is launched in the background when WSLg is launched.

Ubuntu is the default distro if you took the easy `wsl --install` route to setup your WSL environment. If you would like to try additional Linux distribution, search in the Microsoft Store for "WSL distribution" and install your favorite.

Congrats you are done and ready to use GUI apps! 

### Install and run GUI apps

If you want to get started with some GUI apps, you can run the following command from your Linux terminal. If you are using a different distribution than Ubuntu, it may be using a different package manager.

```powershell

## Update all packages in your distro
sudo apt update

## Gedit
sudo apt install gedit -y

## GIMP
sudo apt install gimp -y

## Nautilus
sudo apt install nautilus -y

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

* `xcalc` 
* `gimp`
* `gedit ~/.bashrc` 
* `nautilus`
* `google-chrome`
* `teams`
* `microsoft-edge`

## Known issues

* When launching an application from the start menu, there will be an extra command prompt launch and sticking around until you close the GUI application. Ignore this for the time being, we're waiting on new console functionality that will allow us to avoid this while keeping wsl.exe a console application.
* All graphics rendering on the Linux side is done on the CPU, no vGPU acceleration are currently available.
* Performance is not representive of the shipping solution. The current version of WSLg you've installed run in RDP RAIL mode and copy pixels from Linux to Windows over the RDP transport / HvSocket. We have a version coming soon which uses shared memory but require special vGPU drivers. There is no vGPU rendering acceleration, but vGPU is used to provide shared memory between Linux and Windows. We're currenlty waiting on fixes in the Linux kernel to propagate to enable this mode of operation. Official, works everywhere with no special driver, support for shared memory between Linux and Windows is being worked on. Once it is available it will be used to share memory and more efficiently move pixels from the Linux guest to the host.


## Contribute

For instructions on how to build WSLg please check [BUILD.md](./config/BUILD.md)
