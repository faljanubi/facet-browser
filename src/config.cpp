#include "stdafx.h"
#include "config.h"
#include "globals.h"

namespace fb {

	static const uint32_t kConfigVersion = 5;   // 3: + PaneConfig::sortStat; 4: + PaneConfig::widths, actionOnSelect; 5: custom stats, artScale, toolbarItems
	const uint32_t BrowserConfig::kVersion = kConfigVersion;

	const char* statName(Stat s) {
		static const char* names[] = {
			"Bitrate", "Duration", "First added", "First played", "First release", "Index", "Items",
			"Last added", "Last modified", "Last played", "Last release", "Lossless ratio", "Played",
			"Played duration", "Rating", "Subitems", "Total peak", "Total size", "File created",
		};
		static_assert(PFC_TABSIZE(names) == (size_t)Stat::Count, "stat names");
		return (uint32_t)s < (uint32_t)Stat::Count ? names[(uint32_t)s] : "";
	}

	const char* statDescription(Stat s) {
		switch (s) {
		case Stat::Bitrate: return "Displays average bitrate.";
		case Stat::Duration: return "Displays total duration.";
		case Stat::FirstAdded: return "Displays when the first track was added. Playback statistics component required.";
		case Stat::FirstPlayed: return "Displays when a track was first played. Playback statistics component required.";
		case Stat::FirstRelease: return "Displays earliest date or release date tag.";
		case Stat::Index: return "Displays list index for each item.";
		case Stat::Items: return "Displays number of tracks.";
		case Stat::LastAdded: return "Displays when the last track was added. Playback statistics component required.";
		case Stat::LastModified: return "Displays the most recent file modification time.";
		case Stat::LastPlayed: return "Displays when a track was last played. Playback statistics component required.";
		case Stat::LastRelease: return "Displays latest date or release date tag.";
		case Stat::LosslessRatio: return "Displays percentage of lossless tracks.";
		case Stat::Played: return "Displays total play count. Playback statistics component required.";
		case Stat::PlayedDuration: return "Multiplies play count and duration for each track. Playback statistics component required.";
		case Stat::Rating: return "Displays average rating.";
		case Stat::Subitems: return "Displays number of items in the next facet.";
		case Stat::TotalPeak: return "Displays ReplayGain peak.";
		case Stat::TotalSize: return "Displays total file size.";
		case Stat::FileCreated: return "Displays the earliest file creation time.";
		default: return "";
		}
	}

	pfc::string8 statLabel(uint32_t id) {
		if (!isCustomStat(id)) return statName((Stat)id);
		auto defs = getCustomStats();
		const uint32_t k = id - kCustomStatBase;
		return k < defs.size() ? defs[k].name : pfc::string8();
	}

	// Custom statistics are stored by name (their position in the global list can change).
	static const uint32_t kCustomMarker = 0xFFFFFFFF;
	static const int32_t kCustomSortMarker = -2;
	static bool customStatId(const char* name, uint32_t& out) {
		auto defs = getCustomStats();
		for (size_t k = 0; k < defs.size(); ++k) if (defs[k].name == name) { out = kCustomStatBase + (uint32_t)k; return true; }
		return false;
	}

	const std::vector<uint32_t>& statMenuOrder() {
		static const std::vector<uint32_t> order = [] {
			std::vector<uint32_t> v;
			for (uint32_t s = 0; s < (uint32_t)Stat::Count; ++s) v.push_back(s);
			std::sort(v.begin(), v.end(), [](uint32_t a, uint32_t b) { return _stricmp(statName((Stat)a), statName((Stat)b)) < 0; });
			return v;
		}();
		return order;
	}

	const char* actionName(Action a) {
		switch (a) {
		case Action::SendToSelectionKeepPlaying: return "Send to selection playlist, keep playing playlist";
		case Action::SendToSelection: return "Send to selection playlist";
		case Action::SendToCurrent: return "Send to current playlist";
		case Action::SendToNew: return "Send to new playlist";
		case Action::SendToNewAutoplaylist: return "Send to new autoplaylist";
		case Action::AddToSelection: return "Add to selection playlist";
		case Action::AddToCurrent: return "Add to current playlist";
		default: return "";
		}
	}

	const char* actionDescription(Action a) {
		switch (a) {
		case Action::SendToSelectionKeepPlaying: return "Replaces content of the selection playlist with selected items, without interrupting playback.";
		case Action::SendToSelection: return "Replaces content of the selection playlist with selected items and starts playback.";
		case Action::SendToCurrent: return "Replaces content of the active playlist with selected items.";
		case Action::SendToNew: return "Creates a new playlist containing selected items.";
		case Action::SendToNewAutoplaylist: return "Creates an autoplaylist based on selected items.";
		case Action::AddToSelection: return "Adds selected items to the selection playlist.";
		case Action::AddToCurrent: return "Adds selected items to active playlist.";
		default: return "";
		}
	}

	PaneConfig BrowserConfig::makePane(const char* column) {
		PaneConfig p;
		p.columns.push_back(column);
		return p;
	}

	BrowserConfig BrowserConfig::makeDefault() {
		BrowserConfig c;
		c.panes.push_back(makePane("Genre"));
		c.panes.push_back(makePane("Album artist"));
		c.panes.push_back(makePane("Album"));
		return c;
	}

	void BrowserConfig::writePane(stream_writer_formatter<>& out, const PaneConfig& p) {
		out << (uint32_t)p.columns.size();
		for (auto& c : p.columns) out << c;
		out << (uint32_t)p.stats.size();
		for (auto s : p.stats) {
			if (isCustomStat(s)) out << kCustomMarker << statLabel(s);
			else out << s;
		}
		const bool customSort = p.sortStat >= 0 && isCustomStat((uint32_t)p.sortStat);
		out << (uint32_t)p.display << (uint32_t)p.artType << p.summary << p.andLogic << p.sortColumn << p.sortDescending << p.weight << (customSort ? kCustomSortMarker : p.sortStat);
		out << (uint32_t)p.widths.size();
		for (auto& w : p.widths) out << w.first << w.second;
		out << p.artScale << (customSort ? statLabel((uint32_t)p.sortStat) : pfc::string8());
	}

	PaneConfig BrowserConfig::readPane(stream_reader_formatter<>& in, uint32_t version) {
		PaneConfig p;
		uint32_t n; in >> n;
		if (n > 32) throw exception_io_data();
		for (uint32_t i = 0; i < n; ++i) { pfc::string8 c; in >> c; p.columns.push_back(c); }
		in >> n;
		if (n > 64) throw exception_io_data();
		for (uint32_t i = 0; i < n; ++i) {
			uint32_t s; in >> s;
			if (s == kCustomMarker && version >= 5) {
				pfc::string8 name; in >> name;
				uint32_t id;
				if (customStatId(name, id) && std::find(p.stats.begin(), p.stats.end(), id) == p.stats.end()) p.stats.push_back(id);
			} else if (s < (uint32_t)Stat::Count) p.stats.push_back(s);
		}
		uint32_t d, a;
		in >> d >> a >> p.summary >> p.andLogic >> p.sortColumn >> p.sortDescending >> p.weight;
		if (version >= 3) in >> p.sortStat;
		if (version >= 4) {
			in >> n;
			if (n > 256) throw exception_io_data();
			for (uint32_t i = 0; i < n; ++i) { pfc::string8 k; uint32_t w; in >> k >> w; if (w > 0 && w < 10000) p.widths.emplace_back(k, w); }
		}
		if (version >= 5) {
			pfc::string8 sortName;
			in >> p.artScale >> sortName;
			p.artScale = std::max(kArtScaleMin, std::min(kArtScaleMax, p.artScale));
			uint32_t id;
			if (p.sortStat == kCustomSortMarker) p.sortStat = customStatId(sortName, id) ? (int32_t)id : -1;
		}
		const bool customSort = p.sortStat >= (int32_t)kCustomStatBase;
		if (!customSort && (p.sortStat >= (int32_t)Stat::Count || p.sortStat == (int32_t)Stat::Index)) p.sortStat = -1;
		if (p.sortStat < -1) p.sortStat = -1;
		// a sort column past the end (a custom statistic that no longer exists) falls back to the first column
		if (p.sortColumn < 0 || p.sortColumn >= (int32_t)(p.columns.size() + p.stats.size())) p.sortColumn = 0;
		p.display = (Display)(d <= 3 ? d : 0);
		p.artType = (ArtType)(a <= 3 ? a : 0);
		if (p.weight == 0) p.weight = 1000;
		if (p.columns.empty()) p.columns.push_back("Genre");
		return p;
	}

	void BrowserConfig::write(stream_writer_formatter<>& out) const {
		out << kConfigVersion;
		out << (uint32_t)panes.size();
		for (auto& p : panes) writePane(out, p);
		out << (uint32_t)source;
		out << (uint32_t)filters.size();
		for (auto& f : filters) out << f;
		out << multipleFilters << (uint32_t)action << showToolbar << toolbarBottom << actionOnSelect << toolbarItems;
	}

	void BrowserConfig::read(stream_reader_formatter<>& in) {
		uint32_t version; in >> version;
		if (version < 2 || version > kConfigVersion) throw exception_io_data();   // 1.x layouts start fresh
		uint32_t n; in >> n;
		if (n == 0 || n > 32) throw exception_io_data();
		panes.clear();
		for (uint32_t i = 0; i < n; ++i) panes.push_back(readPane(in, version));
		uint32_t s; in >> s;
		source = (Source)(s <= 2 ? s : 0);
		in >> n;
		if (n > 1000) throw exception_io_data();
		filters.clear();
		for (uint32_t i = 0; i < n; ++i) { pfc::string8 f; in >> f; filters.push_back(f); }
		uint32_t a;
		in >> multipleFilters >> a >> showToolbar >> toolbarBottom;
		action = (Action)(a < (uint32_t)Action::Count ? a : 1);
		if (version >= 4) in >> actionOnSelect;
		if (version >= 5) { in >> toolbarItems; toolbarItems &= TB_ALL; }
	}
}
