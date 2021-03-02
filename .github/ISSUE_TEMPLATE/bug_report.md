---
name: Bug report
about: Report a bug in WSLG
title: ''
labels: 'bug'
assignees: ''

---

# Environment

```none
Windows build number: [run `[Environment]::OSVersion` for powershell, or `ver` for cmd]
Your Distribution version: [On Debian or Ubuntu run `lsb_release -r` in WSL]
Your WSLG version: [Open 'Apps and Features' and check the version of 'Windows Subsystem for Linux GUI app support', e.g: 0.2.3.13]
```

# Steps to reproduce

<!--
Collect WSL logs if needed by following these instructions: https://github.com/Microsoft/WSL/blob/master/CONTRIBUTING.md#8-detailed-logs  
-->

**WSL logs**: 

* Attach WSLG logs from  `/mnt/wslg`

You can access the wslg logs using explorer at: `\\wsl\<Distro-Name>\mnt\wslg` (eg: `\\wsl\Ubuntu-20.04\mnt\wslg`)

* `puseaudio.log`
* `weston.log`
* `versions.txt`


#  Expected behavior

<!-- A description of what you're expecting, possibly containing screenshots or reference material. -->

# Actual behavior

<!-- What's actually happening? -->


