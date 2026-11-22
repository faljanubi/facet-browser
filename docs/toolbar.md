---
title: Toolbar
nav_order: 6
---

# Toolbar
{: .no_toc }

```
[Library v] [Filter v] [Search..................] [Send to selection playlist v] [?]
```

Right-click an empty part of the toolbar to hide it or move it to the top. To bring a hidden
toolbar back, right-click an item › **Show toolbar**.

1. TOC
{:toc}

## Source

Chooses which tracks Facet Browser browses:

| Source | Browses |
|:--|:--|
| **Library** | The whole Media Library (default) |
| **Playlist** | The active playlist |
| **Automatic** | The active playlist, or the library when the active playlist is the selection playlist |

With *Automatic* you browse whichever playlist you switch to, but sending results to the
selection playlist doesn't make Facet Browser browse its own output.

## Filter

Click **Filter** to turn saved filters on or off. Active filters hide every track that doesn't
match them. The button shows the active filter's name, or *n filters*.

- Normally only one filter is active at a time; picking another one replaces it.
- Tick **Multiple filters** to combine filters. A track then has to match all of them.
- **Preferences** opens the page where you add and edit filters.

The built-in filters:

| Filter | Query |
|:--|:--|
| Lossless | `"$info(encoding)" IS lossless` |
| Lossy | `"$info(encoding)" IS lossy` |
| Missing replaygain | `%replaygain_track_gain% MISSING` |
| Most played | `ALL SORT DESCENDING BY %play_count%` |
| Never played | `%play_count% MISSING` |
| Played often | `%play_count% GREATER 9` |
| Random order | `ALL SORT BY "$rand()"` |
| Recently added | `%added% DURING LAST 1 WEEK` |
| Recently played | `%last_played% DURING LAST 1 WEEK` |
| Top rated | `%rating% IS 5` |

## Search

The search box filters all panes as you type, after a short pause.

- **Plain words** are looked up in artist, album artist, album, title, genre and file path.
  Every word must match: `floyd live` finds tracks containing both words.
- **Query syntax** is used as it is. If the text is a valid foobar2000 query, such as
  `%date% GREATER 1999` or `artist IS Björk`, it's run as a query instead of as plain words.
- **Enter** saves the search in the history. The 15 most recent searches are kept in the
  drop-down list, and **Wipe history** at the bottom of the list clears them.
- **Esc** clears the search.

Search works on top of the source and the active filters.

## Action button

The button shows the action that runs when you **double-click** an item, press **Enter**, or
click the button itself. Click the **▾** part to choose a different action:

| Action | Effect |
|:--|:--|
| Send to selection playlist, keep playing playlist | Replaces the selection playlist's content without interrupting playback. If the selection playlist is the one playing, it is kept as "Library selection (playing)" and a fresh selection playlist is made, like the Library Viewer does |
| Send to selection playlist | Replaces the selection playlist's content and starts playing it (default) |
| Send to current playlist | Replaces the content of the active playlist |
| Send to new playlist | Creates a new playlist named after your selection, e.g. *Jazz, Blues* |
| Send to new autoplaylist | Creates an autoplaylist that keeps up with your library |
| Add to selection playlist | Adds the tracks to the end of the selection playlist |
| Add to current playlist | Adds the tracks to the end of the active playlist |

All actions are in the item right-click menu too.

**Run on single click** (at the bottom of the **▾** menu) runs the action every time the
selection changes, so the selection playlist follows your clicks. It works best with *Send to
selection playlist, keep playing playlist*, which doesn't restart playback. The *new playlist*
and *new autoplaylist* actions still need a double-click, so a click doesn't create a playlist.

- The **selection playlist** is called *Library selection* by default. You can rename it in
  [Preferences](preferences#library-selection).
- Tracks are sorted by the **sort pattern** from Preferences (`%path_sort%` by default) before
  they're added.
- Locked playlists are left alone.

### Autoplaylists

*Send to new autoplaylist* turns your selection, active filters and search into a foobar2000
query. The autoplaylist then updates itself as your library changes. It can take a few seconds
to fill after it's created.

## Help button

**?** opens the
[foobar2000 query syntax](https://wiki.hydrogenaud.io/index.php?title=Foobar2000:Query_syntax)
reference, which helps with writing searches and filters.
