
|  | Status | 
| :------ | :------: | 
| Build (Ubuntu) | [![Build Status](https://microsoft.visualstudio.com/DxgkLinux/_apis/build/status/wslg?branchName=master&jobName=Build%20(Ubuntu))](https://microsoft.visualstudio.com/DxgkLinux/_build/latest?definitionId=55786&branchName=master) |
| Package MSI (Windows) | [![Build Status](https://microsoft.visualstudio.com/DxgkLinux/_apis/build/status/wslg?branchName=master&jobName=Package%20(Windows))](https://microsoft.visualstudio.com/DxgkLinux/_build/latest?definitionId=55786&branchName=master) |
# WSLg

## Install WSLg

### Pre-requisites

- Windows 10 Build 20240 or higher
   - Please note that this is not yet released, and you will need to be on an internal Microsoft branch like rs_onecore_sigma, this will not work on rs_prerelease

### Install instructions

#### If you don't have WSL already installed

From a command prompt with administrator privileges, run the command `wsl --install`, then reboot.

#### If you have WSL currently installed

Soon you'll be able to simply run `wsl --update` from an elevated command prompt and get the latest released version of WSLg installed and configured for your WSL installation automatically.

For the time being, you'll need to manually install the WSLg MSI on top of your existing WSL installation.

* Verify that you are running in WSL 2 mode. If not switch to WSL 2

```powershell
   wsl --list -v
   wsl --set-version _distro_name_ 2
```

* Shutdown your WSL instances and the WSL 2 VM by running this command from an elevated PowerShell or CMD:

```powershell
    wsl --shutdown
```

* Download and install the latest MSI from the [Releases page](https://github.com/microsoft/wslg/releases). 

### First Launch

Find the "Ubuntu" icon in the start menu and launch it if you have installed the default Linux distro per these instruction. This will launch a terminal into the WSL distro that you installed. Ubuntu is the default distro if you just went with `wsl --install`. If you would like to try additional Linux distribution, search in the Windows Store for "Linux Distro" and pick your favorite.

When you first launch WSL, you will be prompted to connect to `weston-terminal`. Click `Connect` to finalize this. (We are working to remove this dialogue box from mstsc)

* Congrats you are done and ready to use GUI apps! 

### Install and run GUI apps

If you want to get started with some GUI apps, please download our [sample setup script](./docs/install-sample-gui-apps.sh) and run `sudo ./install-sample-gui-apps.sh` to run it. This script is targeted to Ubuntu 18.04 and higher.

Once you have run that script try some of these commands: 
* `xcalc` 
* `gimp`
* `gedit ~/.bashrc` 
* `nautilus`
* `google-chrome`
* `teams`
* `microsoft-edge`

You can also find these apps inside of your Windows Start menu under a folder that has your distro name. For example you could open GIMP through 'Ubuntu -> GIMP'. 

## Contribute

For instructions on how to build WSLg please check [BUILD.md](./config/BUILD.md)
