---
title: Example layouts
nav_order: 8
---

# Example layouts
{: .no_toc }

Ideas for setting up your panes. Each one only uses the menus described in [Panes](panes).

1. TOC
{:toc}

## Classic Facets

**Genre › Album artist › Album**, the default layout. Add *Statistics › Items* and *Duration* to
the Album pane to see how long each album is.

## Folder browser

For libraries organised in `Artist\Album\` folders:

| Pane | Columns | Statistics | Display |
|:--|:--|:--|:--|
| 1 | Folder | | Columns |
| 2 | Folder | | Columns |
| 3 | Folder | Last modified, Bitrate | Album art (bottom labels) |
| 4 | Filename | Duration, Bitrate | Columns |

Use *Pane › Append new pane* to add panes, then pick *Columns › Folder* in each one.

## Album wall

1. Remove all panes but one (*Pane › Remove pane*).
2. Pick *Columns › Album*, then *Display › Album art (bottom labels)*.
3. Pick *Sort by › Last added*, then *Descending*, to see your newest albums first.

Add an **Album artist** pane on the left to filter the wall by artist.

## Rediscovery

| Pane | Columns | Sort by |
|:--|:--|:--|
| 1 | Album artist | Last played, ascending |
| 2 | Album | Played, ascending |

Albums you haven't played for a long time come first. Needs Playback Statistics.
Try it with the **Never played** filter.

## Library health

One pane with *Columns › Album* and these statistics: Items, Bitrate, Lossless ratio, Total size,
Total peak. Turn on the **Missing replaygain** filter to see which albums still need a
ReplayGain scan.

## Years and decades

Add a **Decade** column (`$left($year(%date%),3)0s`) and a **Year** column (`$year(%date%)`) in
[Preferences](preferences#useful-patterns), then use **Decade › Year › Album**.
