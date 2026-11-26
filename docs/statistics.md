---
title: Statistics
nav_order: 5
---

# Statistics
{: .no_toc }

Add statistics with *header menu › Statistics*. Each value is calculated over the tracks in that
row. The **summary item** shows the totals for the whole pane.

1. TOC
{:toc}

## All statistics

| Statistic | Shows | Needs Playback Statistics |
|:--|:--|:--:|
| Bitrate | Average bitrate | |
| Duration | Total duration | |
| File created | Earliest file creation time | |
| First added | When the first track was added to the library | ✔ |
| First played | When a track was first played | ✔ |
| First release | Earliest date tag | |
| Index | Row number | |
| Items | Number of tracks | |
| Last added | When the last track was added | ✔ |
| Last modified | Most recent file modification time | |
| Last played | When a track was last played | ✔ |
| Last release | Latest date tag | |
| Lossless ratio | Percentage of lossless tracks | |
| Played | Total play count | ✔ |
| Played duration | Play count × duration, added up over all tracks | ✔ |
| Rating | Average rating | |
| Subitems | Number of entries this row has in the next pane | |
| Total peak | Highest ReplayGain peak | |
| Total size | Total file size | |

## Custom statistics

Define your own statistics in *Preferences › Media Library › Facet Browser › Statistics*
(or *header menu › Statistics › Preferences*): a name, a title formatting pattern that gives a
number for each track, and how to combine the numbers of a row's tracks.

| Name | Pattern | Combine | Shows |
|:--|:--|:--|:--|
| Plays (all) | `$max(%play_count%,%lastfm_play_count%)` | Total | Local or Last.fm play count, whichever is higher, added up |
| Avg BPM | `%bpm%` | Average | Average tempo |
| Highest rating | `%rating%` | Highest | Best rating among the tracks |
| Oldest year | `$year(%date%)` | Lowest | Earliest release year |
| Five-star tracks | `$ifequal(%rating%,5,1,0)` | Total | Number of tracks rated 5 |

- Tracks whose pattern gives no number (an empty or missing tag, or text) are skipped. Rows
  where no track has a number stay empty.
- `$year(%date%)` gives `0` for tracks without a date; write `[$year(%date%)]` to skip them.
- Custom statistics are listed below the built-in ones and can be sorted by like them.
  Rows without a number sort below every number.

## Sorting

Click a statistic's column title to sort by it. Use *header menu › Sort by* to sort by a
statistic without showing it. *Index* can't be sorted by, because it's just the row number.

## Playback Statistics

Play counts and "added" / "played" dates come from foobar2000's **Playback Statistics**
component (`foo_playcount`). Without it, those columns stay empty. Install it from the
foobar2000 website, then add it in *File › Preferences › Components*.

## Notes

- **File created** reads file times from disk, so it's only worked out when a pane shows or
  sorts by it. For tracks inside an archive, the archive's own creation time is used.
- **Lossless ratio** uses the codec information foobar2000 stores for each file.
