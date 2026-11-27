#pragma once
#include "config.h"
#include "globals.h"

// Data engine: formats every pane's columns for every source track once, then groups,
// filters and aggregates statistics cheaply from that cache.

namespace fb {

	// One column of a pane, resolved against the global definitions.
	struct ColumnSpec {
		pfc::string8 name;      // display name
		pfc::string8 key;       // how the pane refers to it (columnKey; differs from name for duplicates)
		pfc::string8 pattern;   // title formatting; unused for folder columns
		bool folder = false;
		bool markers = false;   // pattern writes Default UI dim/highlight markers (<dim> >highlight<)
		uint32_t depth = 0;     // folder level (1 = first folder inside the Media Library folder)
	};

	struct PaneSpec { std::vector<ColumnSpec> columns; };

	// One custom statistic of an item: the numbers of its tracks combined.
	struct CustomAgg { double sum = 0, lo = 0, hi = 0; uint32_t n = 0; };

	struct FacetItem {
		uint32_t valueId = 0;
		uint32_t count = 0;
		double duration = 0;
		uint64_t size = 0;
		uint64_t bitrateSum = 0; uint32_t bitrateN = 0;
		int32_t firstAdded = -1, lastAdded = -1;        // track indices holding the min/max value
		int32_t firstPlayed = -1, lastPlayed = -1;
		int32_t firstRelease = -1, lastRelease = -1;
		int32_t lastModified = -1;
		int32_t firstCreated = -1;
		uint64_t plays = 0;
		double playedDuration = 0;
		uint64_t ratingSum = 0; uint32_t rated = 0;
		uint32_t lossless = 0;
		double peak = -1;
		uint32_t subitems = 0;
		bool hasChildren = false;                       // folder items with deeper folders
		uint32_t firstTrack = 0;
		std::vector<CustomAgg> custom;                  // per custom statistic (index into the model's list)
	};

	class FacetModel {
	public:
		// wantCreated: also read file creation times from disk (only needed for the File created stat)
		// customs: the custom statistics (getCustomStats()); ids kCustomStatBase + index refer to them
		bool build(metadb_handle_list_cref tracks, const std::vector<PaneSpec>& panes, bool wantCreated, const std::vector<StatDef>& customs, pfc::string8& error);
		// Same tracks, but the panes marked in `changed` have new columns: only those are formatted and
		// regrouped; statistics and the other panes are kept. False on a pattern error (in `error`) or
		// when the pane count differs (then use build()).
		bool rebuildPanes(const std::vector<PaneSpec>& panes, const std::vector<char>& changed, pfc::string8& error);

		size_t paneCount() const { return m_panes.size(); }
		size_t trackCount() const { return m_tracks.get_count(); }
		metadb_handle_ptr track(uint32_t idx) const { return m_tracks[idx]; }
		const pfc::string8& valueKey(size_t pane, uint32_t id) const { return m_panes[pane].names[id]; }
		const std::vector<pfc::string8>& valueParts(size_t pane, uint32_t id) const { return m_panes[pane].parts[id]; }

		std::vector<FacetItem> group(size_t pane, const std::vector<uint32_t>& input) const;
		FacetItem totals(size_t pane, const std::vector<uint32_t>& input) const;
		void computeSubitems(size_t pane, const std::vector<uint32_t>& input, std::vector<FacetItem>& items) const;
		// sortStat >= 0 sorts by that Stat and ignores column
		void sortItems(size_t pane, std::vector<FacetItem>& items, int column, const std::vector<uint32_t>& stats, bool descending, int sortStat = -1) const;
		std::vector<uint32_t> filter(size_t pane, const std::vector<uint32_t>& input, const std::vector<uint32_t>& ids, bool andLogic) const;
		// per value id: whether any of these tracks has it in this pane
		std::vector<char> presentValues(size_t pane, const std::vector<uint32_t>& input) const;
		bool findValue(size_t pane, const char* key, uint32_t& out) const;
		bool findTrack(const metadb_handle_ptr& h, uint32_t& out) const;
		// the value ids a track has in a pane (several for multi-value and "|" columns)
		std::vector<uint32_t> trackValues(size_t pane, uint32_t track) const;

		// s: a built-in Stat, or kCustomStatBase + index of a custom statistic
		pfc::string8 statText(Stat s, const FacetItem& it, size_t rowIndex) const;
		// the combined value of a custom statistic; false when no track of the item has a number
		bool customValue(size_t k, const FacetItem& it, double& out) const;

		std::vector<uint32_t> allTracks() const;
		metadb_handle_list handles(const std::vector<uint32_t>& idx) const;

	private:
		struct TrackInfo {
			double length = 0, peak = -1;
			uint64_t size = 0;
			uint32_t bitrate = 0, plays = 0, rating = 0;
			bool lossless = false;
			// text statistics as ids into m_text (0 = empty); kept small for very large libraries
			uint32_t added = 0, firstPlayed = 0, lastPlayed = 0, date = 0, modified = 0, created = 0;
		};
		std::vector<pfc::string8> m_text;   // the distinct texts of the track statistics; [0] = ""
		const char* text(uint32_t id) const { return m_text[id].c_str(); }
		struct PaneValues {
			std::vector<pfc::string8> names;                 // value key (parts joined)
			std::vector<std::vector<pfc::string8>> parts;    // per column
			std::vector<std::vector<std::wstring>> sortKeys; // per column, for natural sorting
			std::unordered_map<std::string, uint32_t> lookup;
			std::vector<uint32_t> offsets, ids;              // per track: value ids
			std::vector<char> deeper;                        // per track: has folders below this pane's level
			std::vector<char> markers;                       // per column: values carry dim/highlight markers
		};
		uint32_t intern(PaneValues& p, const std::vector<pfc::string8>& parts);
		uint32_t internNew(PaneValues& p, const std::string& key, const std::vector<pfc::string8>& parts);
		void accumulate(FacetItem& it, uint32_t t) const;

		struct ColumnJob { size_t pane, column; titleformat_object::ptr script; };
		static bool compileColumns(const std::vector<PaneSpec>& panes, const std::vector<char>& which, std::vector<ColumnJob>& cols, bool& anyFolder, pfc::string8& error);
		void ensureLocations();
		void buildPanes(const std::vector<PaneSpec>& panes, const std::vector<size_t>& which, const std::vector<ColumnJob>& cols, const std::vector<std::vector<pfc::string8>>& raw);
		void buildPane(size_t p, const PaneSpec& spec, const std::vector<const std::vector<pfc::string8>*>& byColumn);
		// Folder columns: each track's folder relative to the Media Library folder, as an index into
		// the distinct folders (tracks of an album share one entry)
		std::vector<uint32_t> m_folderOf;
		std::vector<pfc::string8> m_folders;
		bool m_locationsReady = false;

		std::vector<std::vector<double>> m_custom;   // [custom statistic][track], NaN = no number
		std::vector<Combine> m_customCombine;
		metadb_handle_list m_tracks;
		std::vector<PaneValues> m_panes;
		std::vector<TrackInfo> m_info;
	};

	pfc::string8 expandMultiValue(const char* pattern);
	// A column pattern split at its top-level "|" (outside function arguments and 'quoted' text).
	std::vector<pfc::string8> splitPipe(const char* pattern);
	// Whether a pattern writes literal < or > (Default UI dim/highlight markers), not counting %<field>%.
	bool patternHasMarkers(const char* pattern);
	pfc::string8 stripMarkers(const char* s);
	// A track's path up to and including its folder at this Folder-column level ("F:\Music\Rock\"), or "".
	pfc::string8 folderPrefix(const metadb_handle_ptr& h, uint32_t depth);
	extern const char kValueSep;
	pfc::string8 formatDuration(double seconds);
	pfc::string8 formatSize(uint64_t bytes);
}
