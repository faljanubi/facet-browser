#pragma once

// Per-panel settings of a Facet Browser element (each pane's columns, statistics and display,
// plus the toolbar state). Column and filter *definitions* are global, see globals.h.

namespace fb {

	// Statistics, in the order Facets lists them.
	enum class Stat : uint32_t {
		Bitrate, Duration, FirstAdded, FirstPlayed, FirstRelease, Index, Items, LastAdded,
		LastModified, LastPlayed, LastRelease, LosslessRatio, Played, PlayedDuration, Rating,
		Subitems, TotalPeak, TotalSize,
		FileCreated,   // appended later; keeps stored values of the others stable
		Count
	};
	const char* statName(Stat s);
	const char* statDescription(Stat s);
	const std::vector<uint32_t>& statMenuOrder();   // all stats, alphabetical

	enum class Display : uint32_t { Columns = 0, ArtBottom = 1, ArtRight = 2, ArtNone = 3 };
	enum class ArtType : uint32_t { Front = 0, Back = 1, Disc = 2, Artist = 3 };
	enum class Source : uint32_t { Library = 0, Playlist = 1, Automatic = 2 };

	enum class Action : uint32_t {
		SendToSelectionKeepPlaying = 0,
		SendToSelection = 1,
		SendToCurrent = 2,
		SendToNew = 3,
		SendToNewAutoplaylist = 4,
		AddToSelection = 5,
		AddToCurrent = 6,
		Count
	};
	const char* actionName(Action a);
	const char* actionDescription(Action a);

	struct PaneConfig {
		std::vector<pfc::string8> columns;   // column names (global definitions or "Folder")
		std::vector<uint32_t> stats;         // Stat values, in display order
		Display display = Display::Columns;
		ArtType artType = ArtType::Front;
		bool summary = true;                 // "All (n ...)" item
		bool andLogic = false;               // selection logic; OR is the default
		int32_t sortColumn = 0;              // index into columns followed by stats
		bool sortDescending = false;
		int32_t sortStat = -1;               // >= 0: sort by this Stat (shown or not), overrides sortColumn
		uint32_t weight = 1000;              // relative width
	};

	struct BrowserConfig {
		std::vector<PaneConfig> panes;
		Source source = Source::Library;
		std::vector<pfc::string8> filters;   // active filter names
		bool multipleFilters = false;
		Action action = Action::SendToSelection;
		bool showToolbar = true;
		bool toolbarBottom = true;

		static BrowserConfig makeDefault();
		static PaneConfig makePane(const char* column);

		void write(stream_writer_formatter<>& out) const;
		void read(stream_reader_formatter<>& in);
		static void writePane(stream_writer_formatter<>& out, const PaneConfig& p);
		static PaneConfig readPane(stream_reader_formatter<>& in, uint32_t version);
	};
}
