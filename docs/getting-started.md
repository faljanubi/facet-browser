---
title: Getting started
nav_order: 3
---

# Getting started
{: .no_toc }

1. TOC
{:toc}

## The default layout

A new Facet Browser has three panes, **Genre**, **Album artist** and **Album**, with a toolbar
along the bottom.

```
+---------------+-------------------+-------------------+
| Genre         | Album artist      | Album             |
| All (12 ...)  | All (230 ...)     | All (512 ...)     |
| Jazz          | ...               | ...               |
| Rock          |                   |                   |
+---------------+-------------------+-------------------+
| [Library v] [Filter v] [Search........] [Send... v] [?] |
+-------------------------------------------------------+
```

## Selecting

- **Click** an item to select it. Every pane to its right now only shows what matches.
- **Ctrl+click** adds or removes items, **Shift+click** selects a range.
- The first row, **All (n ...)**, is the *summary item*. Selecting it means "everything in this
  pane".
- With several items selected, a pane uses **OR** logic by default: *Jazz* **or** *Blues*.
  You can switch a pane to **AND**, see [Selection logic](panes#selection-logic).

## Playing

- **Double-click** or press **Enter** to run the *action* shown on the toolbar's action button.
  The default action, *Send to selection playlist*, fills a playlist named
  **Library selection** with the matching tracks and starts playing.
- **Right-click** items for all actions, plus foobar2000's normal track menu
  (Properties, ReplayGain, Convert and so on).
- **Drag** items into any playlist.

## Changing a pane

Right-click a pane's **column header** to open the header menu:

| Menu | What it does |
|:--|:--|
| **Columns** | Pick what the pane lists: Genre, Artist, Album, Folder... |
| **Statistics** | Add number columns: Items, Duration, Rating... |
| **Sort by** | Sort by any column or statistic, ascending or descending |
| **Display** | List or album art, summary item on/off, art type |

To add, remove or move panes, right-click an item and use the **Pane** menu. Drag the line
between two panes to resize them.

## Searching

Type in the **Search** box. After a short pause, every pane only shows matching tracks.
Plain words are matched against artist, album artist, album, title, genre and file path.
Press **Enter** to save the search in the history, and **Esc** to clear it.
See [Toolbar › Search](toolbar#search).

## Next steps

- [Panes](panes): everything about columns, the Folder column and album art.
- [Example layouts](examples): ready-made setups to copy.
