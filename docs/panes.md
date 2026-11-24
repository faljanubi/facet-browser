---
title: Panes
nav_order: 4
---

# Panes
{: .no_toc }

Each pane lists the values of one or more columns, for the tracks left over after the panes to
its left. Right-click a pane's column header for the header menu.

1. TOC
{:toc}

## Columns

*Header menu › Columns* lists every column definition. The built-in ones are:

| Column | Shows |
|:--|:--|
| Genre | Each genre; tracks without a genre are listed as *(Unknown)* |
| Album artist | Album artist |
| Artist | Artist |
| Date | Date tag |
| Album | Album |
| Track | Track number and title |
| Title | Title |
| Filename | File name without extension |
| Filename.ext | File name with extension |
| Folder | One folder level, see [Folder column](#folder-column) |

You can edit these and add your own in [Preferences](preferences#columns).
**Columns › Preferences** opens that page.

### Multiple columns

Normally, picking a column replaces the pane's column. Tick **Columns › Multiple columns** to
show several columns side by side, for example *Album artist* and *Album*. The pane then lists
each unique combination.

### Multi-value tags

A column written with `%<field>%`, like Genre's `%<genre>%`, lists each value of a multi-value
tag separately. A track tagged *Jazz; Soul* appears under both *Jazz* and *Soul*.

### Several values in one column: `|`

A `|` in a column pattern lists every track under each part. `A|%<album artist>%` puts a track
under its letter *and* under its album artist, so the pane shows group entries mixed in with
the artists. Selecting a group entry selects everything under it. Examples:

```
<$upper($left($stripprefix(%album artist%),1))>|%<album artist>%
    A, Abba, Adele, ... B, Beck, ...
<$cut(%date%,3)0s>|[$year(%date%)]
    1990, 1990s, 1991, ... 2000, 2000s, 2001, ...
```

A `|` inside a function's brackets or in `'quoted'` text is plain text.

### Dim and highlight

As in foobar2000's playlist view, text between `<` and `>` is drawn dimmed and text between
`>` and `<` highlighted. Repeat them for a stronger effect (`<<<very dim>>>`). The markers are
ignored when sorting, so the group entries above sort among the values they stand for.

## Folder column

The **Folder** column shows one folder level per pane, starting from your Media Library folders.
Put several Folder panes next to each other to walk down your folder tree:

```
Folder        | Folder           | Folder                  | Filename
Rock >        | Pink Floyd >     | 1973 - The Dark Side... | 01 Speak to Me
Jazz >        | Radiohead >      | 1975 - Wish You Were... | 02 Breathe
```

- Folders that contain more folders are marked with **›**.
- The first Folder pane shows the first level inside your Media Library folder, the next Folder
  pane the level below that, and so on.

## Statistics

*Header menu › Statistics* adds number columns such as Items, Duration or Rating, calculated over
each row's tracks. See [Statistics](statistics) for the full list.

Click a column title to sort by it; click again to reverse the order.

Drag the line between two column titles to change a column's width. The pane remembers it;
columns you haven't resized share the space left over. *Header menu › Columns › Reset column
widths* goes back to automatic widths.

## Sort by

*Header menu › Sort by* sorts the pane by:

- any of its columns, or
- **any statistic, even one that isn't shown**. For example, sort albums by *Last added* without
  showing the date.

Pick **Ascending** or **Descending** at the bottom of the menu. Descending puts the newest dates
first. Text is sorted naturally, so *Disc 2* comes before *Disc 10*.

## Display

*Header menu › Display*:

| Option | Effect |
|:--|:--|
| **Columns** | A normal list (default) |
| **Album art (bottom labels)** | A grid of covers with the name under each one |
| **Album art (right labels)** | Covers with the name to the right |
| **Album art (no labels)** | Covers only |
| **Summary item** | Show or hide the *All (n ...)* row |
| **Front / Back / Disc / Artist** | Which picture the album art views use |

In album art views, use the arrow keys, Page Up / Page Down, Home and End to move,
Ctrl / Shift to select several, and Enter or double-click to run the action. Covers load in the
background.

## Selection logic

*Header menu › Selection logic*:

- **OR (default):** tracks matching *any* selected item. Selecting *Jazz* and *Blues* shows
  both genres.
- **AND:** tracks matching *all* selected items. This is useful with multi-value tags: selecting
  *Jazz* and *Live* in a tag pane shows only tracks tagged with both.

## Managing panes

*Header menu › Pane*:

| Command | Shortcut | Effect |
|:--|:--|:--|
| Insert new pane | | New pane before this one |
| Append new pane | | New pane after this one |
| Remove pane | | Removes this pane (not possible for the last one) |
| Cut pane | Ctrl+X | Copies the pane to the clipboard and removes it |
| Copy pane | Ctrl+C | Copies the pane with all its settings |
| Paste pane | Ctrl+V | Inserts the copied pane before this one; hold **Shift** to insert it after |

Cut and paste is how you **move** a pane. Copy and paste also works between different Facet
Browser panels.

To resize panes, drag the line between them.
