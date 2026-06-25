# Issue triage guide

This file documents how issues and pull requests are triaged in
microsoft/wslg. It exists so that external contributors can understand
why a label was applied and so that a new maintainer can pick up the
queue without re-deriving the conventions.

## Labels

### Type

| Label | Meaning |
|---|---|
| `bug` | Something doesn't work as designed. Applied automatically by the bug-report issue template. |
| `enhancement` | A request for new behavior or a roadmap item. Applied automatically by the feature-request template. |
| `documentation` | Docs-only change or docs-only gap. |
| `question` | Support question rather than a defect; usually redirected to discussions. |
| `duplicate` | Duplicate of another issue. Close with a comment that links the canonical issue. |
| `invalid` | Not actionable as filed (missing information, off-topic, etc.). |
| `wontfix` | Acknowledged but the team has decided not to take it. |

### Area

| Label | Meaning |
|---|---|
| `audio-redirection` | PulseAudio / PipeWire / ALSA audio path. |
| `clipboard-integration` | Clipboard sync between Windows and Linux. |
| `GPU` | DirectX/dxcore GPU passthrough, Mesa, dzn. |
| `keyboard-layout` | Keyboard, IME, layout mapping. |
| `rdp-client-connection` | mstsc / msrdc.exe client behavior. |
| `server-connection` | Wayland/X server side connection issues. |
| `start-menu-integration` | App shortcuts, icons, launchers. |
| `window-management` | Sizing, snapping, focus, decorations. |
| `Missing-Desktop-Dependencies` | User's distro is missing required packages. |
| `Install-Issue` | Problem occurs during install/setup, not at runtime. |

### Process

| Label | Meaning |
|---|---|
| `Waiting User Info` | We've asked the author for more information. Auto-closed after 44 days of silence by the stale workflow. |
| `fixinbound` | A fix has been committed internally and is on its way to a public release. |
| `weston-upstream-bug` | Root cause is in upstream Weston; tracked there. |
| `help wanted` | Maintainers welcome a community PR for this. Exempt from stale bot. |
| `good first issue` | Suitable starting task for new contributors. Exempt from stale bot. |
| `do-not-merge` | PR is held intentionally; do not merge even if green. |
| `stale` | Applied by the stale workflow after a long period of inactivity. |
| `hacktoberfest`, `hacktoberfest-accepted` | Used during Hacktoberfest only. |

## Workflow

1. **New issue arrives.** The issue templates auto-apply `bug` or
   `enhancement`. Apply one or more *area* labels based on the
   reported symptom.
2. **Missing information.** If you can't act on the report, ask for
   the missing pieces and apply `Waiting User Info`. The stale bot
   will close it if the author doesn't respond within ~44 days.
3. **Duplicate.** Apply `duplicate`, comment with a link to the
   canonical issue, and close. Prefer keeping the oldest open issue
   as the canonical one unless a newer one has materially better
   information.
4. **Upstream.** If the root cause is in Weston or another upstream
   project, apply `weston-upstream-bug` (or open one) and link the
   upstream tracker. These issues are exempt from the stale bot.
5. **Internally fixed.** When a fix lands in an internal build but
   hasn't shipped yet, apply `fixinbound`. Remove and close once the
   public release is out.
6. **Stale bot.** Issues with no activity for 365 days are marked
   `stale` and closed 30 days later. Pull requests are marked stale
   at 120 days and closed 30 days later. Draft PRs are exempt. See
   [`.github/workflows/stale.yml`](workflows/stale.yml) for the full
   exemption list.

## Pull requests

* Run the linters (`Lint` workflow) and the full Azure Pipelines
  build (triggered internally) before merging.
* Squash-merge unless the PR is split into logically separate commits
  that should be preserved.
* Use a release-branch PR (`user/<alias>/release-<version>`) to bump
  the version and publish the next build.

## Known cross-cutting issue clusters

The same underlying defect generates many duplicate reports. When
triaging, check whether the new report belongs to one of these
clusters before opening or keeping a new tracking issue:

* **Window focus loss / disappearance after network change or sleep**
  — see #1092, #1098, #1108, #294, #1268.
* **mstsc / msrdc.exe stealing focus or relaunching** — see #894,
  #895, #655, #841.
* **Fractional / HiDPI scaling** — see #23, #388, #584.
* **Windows snap layouts and snapping** — see #22, #727.
* **Ubuntu 24.04 regressions (slow startup, large cursor, Wayland
  fallback)** — see #1244, #1250, #1290.
