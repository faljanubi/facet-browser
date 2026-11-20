---
title: FAQ & troubleshooting
nav_order: 9
---

# FAQ & troubleshooting
{: .no_toc }

1. TOC
{:toc}

## "The Media Library is not configured"

Facet Browser shows the Media Library. Add your music folders in *File › Preferences ›
Media Library* and click **Apply**. To browse a playlist instead, set the toolbar's **Source**
to *Playlist*.

## Played, First added and other columns are empty

These statistics need the **Playback Statistics** component (`foo_playcount`). Install it and
the columns fill in. Tracks you've never played stay empty in *Played* and the *played* dates.

## I can't find Facet Browser in the Replace UI Element list

It's under **Media Library Viewers**. Check in *File › Preferences › Components* that it's
installed, and that foobar2000 restarted after installing.

## Does it read my old Facets settings?

No. Facets only runs in 32-bit foobar2000 and stores its settings in a different format.
Set up your panes again; it only takes a minute with the header menu, and the
[example layouts](examples) can help.

## Does it work with Columns UI?

Facet Browser is a Default UI element. Whatever UI you use, you can always open it in its own
window with *View › Facet Browser*.

## A new autoplaylist is empty

foobar2000 fills autoplaylists in the background. Give it a few seconds.

## The search doesn't find what I typed

If your text happens to be valid query syntax (for example it contains `IS`, `HAS` or
`MISSING`), it's run as a query rather than as plain words. Check the
[query syntax](https://wiki.hydrogenaud.io/index.php?title=Foobar2000:Query_syntax), or
reword the search.

## The Folder column starts at the wrong level

The first Folder pane starts at the first level **inside** your Media Library folder. If your
music is in `D:\Music\Artist\Album\` and you want the first pane to list artists, add `D:\Music`
as the Media Library folder.

## Reporting a bug

Please open an [issue on GitHub](https://github.com/faljanubi/facet-browser/issues) and include:

- your foobar2000 version (*Help › About*) and whether it's 64-bit or 32-bit,
- the Facet Browser version (*File › Preferences › Components*),
- what you did, what you expected, and what happened instead.
