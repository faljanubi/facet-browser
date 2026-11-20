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
