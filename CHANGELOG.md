# Changelog

## 1.1.0 (2026-11-27)

Settings from 1.0.3 carry over.

### New

- Several columns can have the same name, as in Facets (e.g. three *A-Z* columns for albums,
  artists and titles). Panes and the *Columns* menu keep them apart, also when columns are
  reordered, renamed or removed in Preferences
- Layout editing mode and scratchboxes: right-clicking a pane adds its options (*Columns*,
  *Statistics*, *Sort by*, *Display*, *Selection logic*, *Pane*, *Show toolbar*) to
  foobar2000's layout menu, so Facet Browser can be set up there
- Album art views show a tooltip with the full name when a label is cut off

### Changed

- Much faster when a pane's columns change on large libraries: only the panes that changed
  are read again, the others and all statistics are kept, and panes are grouped in parallel.
  With 230,000 tracks a column change takes about 0.15 s instead of about 1 s, and selecting
  an item updates the other panes about four times faster
- Uses about 30% less memory with large libraries (about 350 MB instead of 510 MB with
  230,000 tracks)
- The summary item shows its *All (n ...)* label in every column of a multi-column pane, each
  counting that column's own values, so it stays visible when the first column is hidden
- Album art views load the covers on screen first; covers scrolled past are no longer loaded
  before them
- Labels under album art are centred, as in Facets

### Fixed

- Statistic columns were wider than needed: their title was measured with the list's font
  instead of the header's smaller one
- The drop-down arrows on the Source and Filter buttons showed as boxes under Wine; they are
  now drawn instead of using a text character
- A column removed in Preferences stayed behind in panes as a hidden placeholder: *Multiple
  columns* stayed ticked and sorting by a statistic could pick the column next to it
- Summary item labels no longer double a final "s" ("All (19 letters)", not "letterss")
- Changing one pane's columns, statistics or display no longer sends the other panes back to the
  top: they keep their scroll position, and album art panes keep the covers already loaded
  instead of loading them all again
- In album art panes the column header cut off statistic titles in narrow panes; each title now
  gets the room it needs
- Changing the album art size (Ctrl+mouse wheel, Larger/Smaller art) no longer shows the covers
  stretched and then sharpening a moment later: the covers on screen are redrawn sharp at the
  new size right away
## 1.0.3 (2026-11-26)

Settings from 1.0.2 carry over.

### New

- Custom statistics: define your own in *Preferences › Media Library › Facet Browser ›
  Statistics* with a name, a title formatting pattern that gives a number per track (e.g.
  `$max(%play_count%,%lastfm_play_count%)`) and how to combine the numbers: Total, Average,
  Lowest or Highest. They are listed under the header menu's *Statistics* and *Sort by*, and
  work like the built-in statistics
- Album art size per pane: *Display › Larger art / Smaller art / Reset art size*, or
  Ctrl+mouse wheel over an album art pane
- Toolbar items can be hidden one by one: right-click the toolbar and untick Source button,
  Filter button, Search box, Action button or Help button
- With the *Playlist* source, selecting items in a pane selects the matching tracks in the
  playlist and scrolls to the first one; *All* clears the playlist selection

### Changed

- The Preferences page is split up: *Facet Browser* keeps the Library selection settings,
  and Columns, Filters and Statistics have a page each below it, with room for longer lists.
  *Columns › Preferences* and *Filter › Preferences* open their own page

### Fixed

- Flicker while the panel or window is resized: the list panes were wiped to an empty
  background before every repaint
- Album art panes keep their cover size when the panel is resized, as in Facets: the columns
  reflow and the grid is centred, so covers show at once instead of loading again

## 1.0.2 (2026-11-24)

Settings from 1.0.1 carry over.

### New

- Show now playing: selects the playing track pane by pane and scrolls to it. Where a track
  has several entries in a pane, the most specific one is picked. A search that hides the
  track is cleared; filters are left alone. In the column header's right-click menu
- Panels that follow the selection, such as Album Art and Properties, now show what is
  selected in Facet Browser, even when nothing is playing
- With "Run on single click" on, a double-click or Enter starts playback: single clicks
  browse, double-click plays

### Changed

- All of Facet Browser's own commands are in the column header's right-click menu: the
  actions (for the pane's selection), Show now playing, Columns, Statistics, Sort by,
  Display, Selection logic, Pane and Show toolbar. Right-clicking an item shows
  foobar2000's track commands only

## 1.0.1 (2026-11-22)

Fixes and features from the feedback on Hydrogenaudio. Settings from 1.0.0 carry over.

### New

- Run on single click (bottom of the action button's ▾ menu): the action runs on every
  selection change. Not used for the new playlist / new autoplaylist actions
- `|` in column patterns, like Facets: `A|B` lists each track under A and under B
- foobar2000's `<dim>` and `>highlight<` markers in column patterns are drawn as in the
  playlist view, and ignored for sorting
- Column widths are remembered per pane; *Header › Columns › Reset column widths* returns
  to automatic widths

### Fixed

- Clicking a column header now sorts; click again to reverse
- Sort, statistics and display changes no longer reload the whole library
- "Send to selection playlist, keep playing playlist": if the selection playlist is the one
  playing, it's kept as "Library selection (playing)" and a fresh selection playlist is made
- Album art views: the header spans the pane again after resizing
- Narrow panels: the action button shrinks instead of covering the search box
- The search history drop-down follows dark mode, fits its entries, and doesn't open when
  the history is empty

## 1.0.0 (2026-11-20)

First public release.

- Linked panes with summary items, multi-select and OR / AND selection logic
- Folder column (one folder level per pane, relative to the Media Library folder)
- Columns, multiple columns, statistics and "Sort by" any column or statistic
- Album art views (bottom labels, right labels, no labels)
- Toolbar with Source, Filter, Search with history, and the action button
- Send to / add to playlists, new autoplaylist with an exact query
- Preferences page for shared column and filter definitions
- 64-bit and 32-bit foobar2000 2.x, follows foobar2000's dark mode
