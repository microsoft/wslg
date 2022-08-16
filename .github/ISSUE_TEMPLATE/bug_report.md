---
name: Bug report
about: Report a bug in WSLg
title: ''
labels: 'bug'
assignees: ''

---

# Environment

```none
Windows build number: [run `[Environment]::OSVersion` for powershell, or `ver` for cmd]
Your Distribution version: [On Debian or Ubuntu run `lsb_release -r` in WSL]
Your WSL versions: [run `wsl --version` on Windows command prompt]
```

# Steps to reproduce

**(Optional) Verifiy using the latest release of WSL/WSLg**:

It is always good idea to verify the issue is still reproducible on the latest WSL/WSLg release whenever possible. WSL/WSLg can be updated from https://aka.ms/wslstorepage, or when Microsoft Store is not accessible from your environment, by downloading the latest release package (.msixbundle) from https://github.com/microsoft/WSL/releases.

<!--
Collect WSL logs if needed by following these instructions: https://github.com/Microsoft/WSL/blob/master/CONTRIBUTING.md#8-detailed-logs  
-->

**WSL logs**: 

* Attach WSLg logs from  `/mnt/wslg`

You can access the wslg logs using explorer at: `\\wsl$\<Distro-Name>\mnt\wslg` (e.g.: `\\wsl$\Ubuntu-20.04\mnt\wslg`)

* `pulseaudio.log`
* `weston.log`
* `stderr.log`

**WSL dumps**:

* Attach any dump files from `/mnt/wslg/dumps`, such as core.weston, if exists.

#  Expected behavior

<!-- A description of what you're expecting, possibly containing screenshots or reference material. -->

# Actual behavior

<!-- What's actually happening? -->

