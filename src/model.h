#pragma once
#include "config.h"

// Data engine: formats every pane's columns for every source track once, then groups,
// filters and aggregates statistics cheaply from that cache.

namespace fb {

	// One column of a pane, resolved against the global definitions.
	struct ColumnSpec {
		pfc::string8 name;
		pfc::string8 pattern;   // title formatting; unused for folder columns
		bool folder = false;
		bool markers = false;   // pattern writes Default UI dim/highlight markers (<dim> >highlight<)
		uint32_t depth = 0;     // folder level (1 = first folder inside the Media Library folder)
	};

	struct PaneSpec { std::vector<ColumnSpec> columns; };

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
	};

	class FacetModel {
	public:
		// wantCreated: also read file creation times from disk (only needed for the File created stat)
		bool build(metadb_handle_list_cref tracks, const std::vector<PaneSpec>& panes, bool wantCreated, pfc::string8& error);

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
		bool findValue(size_t pane, const char* key, uint32_t& out) const;
		bool findTrack(const metadb_handle_ptr& h, uint32_t& out) const;
		// the value ids a track has in a pane (several for multi-value and "|" columns)
		std::vector<uint32_t> trackValues(size_t pane, uint32_t track) const;

		pfc::string8 statText(Stat s, const FacetItem& it, size_t rowIndex) const;

		std::vector<uint32_t> allTracks() const;
		metadb_handle_list handles(const std::vector<uint32_t>& idx) const;

	private:
		struct TrackInfo {
			double length = 0, peak = -1;
			uint64_t size = 0;
			uint32_t bitrate = 0, plays = 0, rating = 0;
			bool lossless = false;
			pfc::string8 added, firstPlayed, lastPlayed, date, modified, created;
		};
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
		void accumulate(FacetItem& it, uint32_t t) const;

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
