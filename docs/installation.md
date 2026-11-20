---
title: Installation
nav_order: 2
---

# Installation
{: .no_toc }

1. TOC
{:toc}

## Requirements

- **foobar2000 2.x** for Windows, 64-bit or 32-bit. One component file contains both builds.
- Windows 10 or 11.
- Optional: the **Playback Statistics** component (`foo_playcount`). Without it, play counts,
  "added" dates and "played" dates are empty.
  [Details](statistics#playback-statistics).

## Install the component

1. Download `foo_facet_browser.fb2k-component` from the
   [Releases page](https://github.com/faljanubi/facet-browser/releases/latest).
2. Double-click the file. foobar2000 opens its *Components* page with Facet Browser added.
   (Or open *File › Preferences › Components*, click **Install...** and pick the file.)
3. Click **Apply** and let foobar2000 restart.

{: .note }
Windows SmartScreen may warn about a downloaded file from an unknown publisher, because the
component isn't code-signed. You can read the source on GitHub or build it yourself.

## Set up your Media Library

Facet Browser shows the contents of foobar2000's Media Library. If you haven't set it up yet,
open *File › Preferences › Media Library*, add your music folders and click **Apply**.
The Folder column also uses these folders as its starting point.

## Open Facet Browser

**As its own window:** *View › Facet Browser*.

**As part of your layout (Default UI):**

1. *View › Layout › Enable layout editing mode*.
2. Right-click the panel you want to replace and choose **Replace UI Element...**.
3. Pick **Facet Browser** under *Media Library Viewers*.
4. Turn layout editing mode off again.

You can add more than one Facet Browser. Each has its own panes and toolbar settings.
Columns, filters and the selection playlist are shared by all of them.

## Update

Install the new `.fb2k-component` file the same way. foobar2000 replaces the old version and
keeps your settings.

## Uninstall

*File › Preferences › Components*, right-click **Facet Browser**, choose **Remove**, then
**Apply**.

## Build from source

You need **Visual Studio 2022** or **Build Tools for Visual Studio 2022** with the
*Desktop development with C++* workload. The foobar2000 SDK and WTL are included in the repository.

```bat
git clone https://github.com/faljanubi/facet-browser.git
cd facet-browser
build.cmd
```

This builds the 64-bit and 32-bit versions and writes `dist\foo_facet_browser.fb2k-component`.
