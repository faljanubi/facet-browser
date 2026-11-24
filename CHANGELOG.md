# Changelog

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
