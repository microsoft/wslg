# Support

WSLg (Windows Subsystem for Linux GUI) is an open source project maintained
by Microsoft. This file describes the kinds of help available and where to
take different kinds of requests.

## Reporting bugs

Open a [bug report](https://github.com/microsoft/wslg/issues/new?template=bug_report.yml).
Please include:

* The output of `wsl --version` from a Windows command prompt.
* Your Windows build number (`[Environment]::OSVersion.Version.Build` in
  PowerShell, or `ver` in cmd).
* Your distribution and version (`lsb_release -r` on Debian/Ubuntu).
* WSLg logs from `\\wsl$\<Distro>\mnt\wslg` (`weston.log`, `stderr.log`,
  and `pulseaudio.log` where applicable).
* Crash dumps from `%tmp%\wsl-crashes` (or, on older WSL releases,
  `/mnt/wslg/dumps`) if WSLg crashed.

A current `wsl --version` is the single most useful piece of information.
A large number of historic reports cannot be acted on because they were
filed against versions of WSL or Windows that are no longer supported.

## Feature requests

Open a [feature request](https://github.com/microsoft/wslg/issues/new?template=feature_request.yml).
Upvote (👍) the top comment on existing issues rather than commenting "+1";
the maintainer team uses reaction counts as a prioritization signal.

## Reporting WSL bugs (not WSLg)

If the issue isn't specific to graphical applications, audio, clipboard,
or window integration, please file it at
[microsoft/WSL](https://github.com/microsoft/WSL/issues/new/choose) instead.

## Security issues

Please do **not** open public issues for security vulnerabilities. Follow
the process in [SECURITY.md](../SECURITY.md).

## Response expectations

WSLg is maintained by a small team. We aim to:

* Triage new issues within 30 days.
* Respond to pull requests within 30 days.
* Ship a release roughly every 1–3 months.

Issues that go a year without activity may be closed by the stale bot.
Reopen with current `wsl --version` output if the problem persists.

## Commercial support

WSLg ships with Windows. For commercial support of WSL itself, see
[Microsoft support options](https://support.microsoft.com/).
