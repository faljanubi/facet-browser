---
title: Preferences
nav_order: 7
---

# Preferences
{: .no_toc }

Open *File › Preferences › Media Library › Facet Browser*. It has three pages below it:
**Columns**, **Filters** and **Statistics**. *Header menu › Columns › Preferences*,
*header menu › Statistics › Preferences* and *Filter › Preferences* open the matching page
directly. These settings are shared by every Facet Browser panel.

1. TOC
{:toc}

## Columns

*Preferences › Media Library › Facet Browser › Columns*. The list on the left holds the column
definitions shown in *header menu › Columns*.

- Select a column to edit its **Name** and **Pattern**.
- **Add new** creates a column, **Remove** deletes the selected one.
- If you rename a column, panes that use it switch to the new name automatically.

The **pattern** is foobar2000
[title formatting](https://wiki.hydrogenaud.io/index.php?title=Foobar2000:Title_Formatting_Reference).
Tracks that produce the same text are grouped into one row.

### Useful patterns

| Name | Pattern | Lists |
|:--|:--|:--|
| Year | `$year(%date%)` | Release years |
| Decade | `$left($year(%date%),3)0s` | 1970s, 1980s... |
| Album (with year) | `[%date% - ]%album%` | 1973 - The Dark Side of the Moon |
| Composer | `%<composer>%` | Each composer, multi-value |
| Codec | `$info(codec)` | FLAC, MP3, AAC... |
| Rating | `$if2(%rating%,Unrated)` | 1–5 or Unrated |
| Artist initial | `$upper($left($stripprefix(%album artist%),1))` | A, B, C... |
| Mood | `$if2(%<mood>%,'('Unknown')')` | Each mood; untagged tracks under (Unknown) |

{: .tip }
Write a field as `%<field>%` to list each value of a multi-value tag on its own. Wrap it in
`$if2(...)` to give tracks without the tag a name, like the built-in Genre column does.

**Folder** is built in and can't be edited. See [Folder column](panes#folder-column).

## Filters

*Preferences › Media Library › Facet Browser › Filters*. This list holds the saved filters shown
under the toolbar's **Filter** button.

- Each filter has a **Name** and a **Query** in
  [foobar2000 query syntax](https://wiki.hydrogenaud.io/index.php?title=Foobar2000:Query_syntax),
  for example `%rating% IS 5` or `%added% DURING LAST 1 WEEK`.
- **Add new** and **Remove** work as they do for columns. Renamed filters stay active.

More example queries:

| Name | Query |
|:--|:--|
| Hi-res | `%samplerate% GREATER 48000` |
| Long tracks | `%length_seconds% GREATER 600` |
| This year | `%date% IS 2026` |
| Unrated | `%rating% MISSING` |
| FLAC only | `%codec% IS FLAC` |

## Statistics

*Preferences › Media Library › Facet Browser › Statistics* holds your **custom statistics**.
They appear in *header menu › Statistics* and *Sort by*, below the built-in ones.

- **Name**: the column title.
- **Pattern**: title formatting that gives a number for each track. Tracks without a number
  (empty, or text) are skipped.
- **Combine**: how the numbers of a row's tracks become one value: **Total**, **Average**,
  **Lowest** or **Highest**.

Renaming a statistic keeps it in the panes that show it; removing it removes its column.
See [Custom statistics](statistics#custom-statistics) for examples.

## Library selection

The *Facet Browser* page itself.

| Setting | Default | Meaning |
|:--|:--|:--|
| **Playlist** | `Library selection` | Name of the playlist the *selection playlist* actions use. It's created when needed. |
| **Sort pattern** | `%path_sort%` | Title formatting used to sort tracks before they're added to a playlist. Leave it empty to keep library order. |

For example, `%album artist% | %date% | %album% | %discnumber% | %tracknumber%` sorts by artist,
then by album release date, then by track order.

**Reset page** at the bottom of the window restores the defaults of the page you're on (custom
statistics: none).
