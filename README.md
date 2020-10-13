
|  | Status | 
| :------ | :------: | 
| Build (Ubuntu) | [![Build Status](https://microsoft.visualstudio.com/DxgkLinux/_apis/build/status/wslg?branchName=master&jobName=Build%20(Ubuntu))](https://microsoft.visualstudio.com/DxgkLinux/_build/latest?definitionId=55786&branchName=master) |
| Package MSI (Windows) | [![Build Status](https://microsoft.visualstudio.com/DxgkLinux/_apis/build/status/wslg?branchName=master&jobName=Package%20(Windows))](https://microsoft.visualstudio.com/DxgkLinux/_build/latest?definitionId=55786&branchName=master) |
# WSLg

## Install WSLg

### Pre-requisites

- Windows 10 Build 20236 or higher on the `rs_onecore_base2_hyp` branch
- The Windows Subsystem for Linux enabled and installed (Install instructions [here](http://aka.ms/install-wsl))
- A WSL 2 distro (Instructions [here](https://docs.microsoft.com/en-us/windows/wsl/install-win10#step-2---update-to-wsl-2))

### Install instructions

* Download and install the latest MSI from the [Releases page](https://github.com/microsoft/wslg/releases). 

* Restart your WSL instances by running this command in PowerShell or CMD:

```powershell
    wsl --shutdown
```

* When you first launch WSL, you will be prompted to connect to `weston-terminal`. Click `Connect` to finalize this. (We are working to remove this dialogue box from mstsc)

* Congrats you are done and ready to use GUI apps! 

### Install and run GUI apps

If you want to get started with some GUI apps, please download our [sample setup script](./docs/install-sample-gui-apps.sh) and run `sudo ./install-sample-gui-apps.sh` to run it. This script is targeted to Ubuntu 18.04 and higher.

Once you have run that script try some of these commands: 
* `xcalc` 
* `gimp`
* `gedit ~/.bashrc` 
* `nautilus`
* `google-chrome`

## Contribute

For instructions on how to build WSLg please check [BUILD.md](./config/BUILD.md)
