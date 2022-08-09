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

