
|  | Status | 
| :------ | :------: | 
| Build (Ubuntu) | [![Build Status](https://microsoft.visualstudio.com/DxgkLinux/_apis/build/status/wslg?branchName=master&jobName=Build%20(Ubuntu))](https://microsoft.visualstudio.com/DxgkLinux/_build/latest?definitionId=55786&branchName=master) |
| Package MSI (Windows) | [![Build Status](https://microsoft.visualstudio.com/DxgkLinux/_apis/build/status/wslg?branchName=master&jobName=Package%20(Windows))](https://microsoft.visualstudio.com/DxgkLinux/_build/latest?definitionId=55786&branchName=master) |
# WSLg

## Install WSLg

### Pre-requisites

- Windows 10 Build 20240 or higher
   - Please note that this is not yet released, and you will need to be on an internal Microsoft branch like rsmaster, this will not work on rs_prerelease
- You will need to have a WSL 2 distro installed on your machine. You can install this easily by running `wsl --install` from a command prompt with administrator privileges.

### Install instructions

* Shutdown your WSL instances and the WSL 2 VM by running this command in PowerShell or CMD:

```powershell
    wsl --shutdown
```

* Download and install the latest MSI from the [Releases page](https://github.com/microsoft/wslg/releases). 

* When you first launch WSL, you will be prompted to connect to `weston-terminal`. Click `Connect` to finalize this. (We are working to remove this dialogue box from mstsc)

* Congrats you are done and ready to use GUI apps! 

* Please note that this install process is temporary and will soon be part of `wsl --install` and `wsl --update`

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
