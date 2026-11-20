#include "stdafx.h"
#include "globals.h"

namespace fb {

	static const GUID guid_cfg_columns = { 0xc26e5deb, 0x1e3f, 0x48dc, { 0x87, 0xcc, 0x01, 0xbf, 0xe0, 0x6d, 0xeb, 0x99 } };
	static const GUID guid_cfg_filters = { 0x7614221a, 0xe09c, 0x4fd5, { 0x90, 0xdb, 0xe6, 0x9b, 0x25, 0x40, 0x92, 0x28 } };
	static const GUID guid_cfg_selpl = { 0x2ab6a07a, 0x0efc, 0x4727, { 0xb7, 0xe9, 0x26, 0x4c, 0x36, 0xea, 0xac, 0x04 } };
	static const GUID guid_cfg_sort = { 0xd37e8f49, 0xfe80, 0x49ec, { 0x80, 0x63, 0x18, 0x3a, 0xfd, 0xbe, 0xf3, 0x0a } };
	static const GUID guid_cfg_history = { 0xcf544eae, 0x1b2f, 0x4875, { 0x8b, 0x15, 0x0d, 0x09, 0xf6, 0xbc, 0x24, 0x27 } };
	extern const GUID guid_prefs_page;
	const GUID guid_prefs_page = { 0xd45f11eb, 0x9e09, 0x44e9, { 0xbc, 0x97, 0xa9, 0x66, 0x40, 0x2a, 0xb8, 0xe1 } };

	const char* const kFolderColumn = "Folder";

	// Lists are stored one entry per line as "name<TAB>value".
	static const char kSep = '\t';

	static cfg_string cfg_columns(guid_cfg_columns, "");
	static cfg_string cfg_filters(guid_cfg_filters, "");
	static cfg_string cfg_selpl(guid_cfg_selpl, "Library selection");
	static cfg_string cfg_sort(guid_cfg_sort, "%path_sort%");
	static cfg_string cfg_history(guid_cfg_history, "");

	static std::vector<std::pair<pfc::string8, pfc::string8>> parsePairs(const char* text) {
		std::vector<std::pair<pfc::string8, pfc::string8>> out;
		pfc::string_list_impl lines;
		pfc::splitStringByLines(lines, text);
		for (size_t i = 0; i < lines.get_count(); ++i) {
			const char* line = lines[i];
			const char* tab = strchr(line, kSep);
			if (!tab) continue;
			out.emplace_back(pfc::string8(line, tab - line), pfc::string8(tab + 1));
		}
		return out;
	}

	template<typename T>
	static pfc::string8 joinPairs(const std::vector<T>& v, pfc::string8 T::* a, pfc::string8 T::* b) {
		pfc::string8 s;
		for (auto& e : v) {
			// tabs/newlines would break the format
			pfc::string8 x = e.*a, y = e.*b;
			x.replace_char('\t', ' '); x.replace_char('\n', ' '); x.replace_char('\r', ' ');
			y.replace_char('\t', ' '); y.replace_char('\n', ' '); y.replace_char('\r', ' ');
			s << x; s.add_byte(kSep); s << y << "\n";   // add_byte: pfc strings print a char with << as a number
		}
		return s;
	}

	std::vector<ColumnDef> defaultColumns() {
		// The column set the original Facets shipped with.
		return {
			{ "Genre", "$if2(%<genre>%,'('Unknown')')" },
			{ "Album artist", "[%album artist%]" },
			{ "Artist", "[%artist%]" },
			{ "Date", "[%date%]" },
			{ "Album", "[%album%]" },
			{ "Track", "[%tracknumber%. ]%title%" },
			{ "Title", "%title%" },
			{ "Filename", "%filename%" },
			{ "Filename.ext", "%filename_ext%" },
		};
	}

	std::vector<FilterDef> defaultFilters() {
		return {
			{ "Lossless", "\"$info(encoding)\" IS lossless" },
			{ "Lossy", "\"$info(encoding)\" IS lossy" },
			{ "Missing replaygain", "%replaygain_track_gain% MISSING" },
			{ "Most played", "ALL SORT DESCENDING BY %play_count%" },
			{ "Never played", "%play_count% MISSING" },
			{ "Played often", "%play_count% GREATER 9" },
			{ "Random order", "ALL SORT BY \"$rand()\"" },
			{ "Recently added", "%added% DURING LAST 1 WEEK" },
			{ "Recently played", "%last_played% DURING LAST 1 WEEK" },
			{ "Top rated", "%rating% IS 5" },
		};
	}

	std::vector<ColumnDef> getColumns() {
		pfc::string8 s = cfg_columns.get();
		if (s.is_empty()) return defaultColumns();
		std::vector<ColumnDef> out;
		for (auto& p : parsePairs(s)) out.push_back({ p.first, p.second });
		return out;
	}
	void setColumns(const std::vector<ColumnDef>& v) {
		pfc::string8 s = joinPairs(v, &ColumnDef::name, &ColumnDef::pattern);
		cfg_columns = s.is_empty() ? "\n" : s.c_str();   // keep "no columns" distinct from "never set"
	}

	bool findColumn(const char* name, ColumnDef& out) {
		for (auto& c : getColumns()) if (c.name == name) { out = c; return true; }
		return false;
	}

	std::vector<FilterDef> getFilters() {
		pfc::string8 s = cfg_filters.get();
		if (s.is_empty()) return defaultFilters();
		std::vector<FilterDef> out;
		for (auto& p : parsePairs(s)) out.push_back({ p.first, p.second });
		return out;
	}
	void setFilters(const std::vector<FilterDef>& v) {
		pfc::string8 s = joinPairs(v, &FilterDef::name, &FilterDef::query);
		cfg_filters = s.is_empty() ? "\n" : s.c_str();   // keep "no filters" distinct from "never set"
	}

	pfc::string8 getSelectionPlaylist() { pfc::string8 s = cfg_selpl.get(); return s.is_empty() ? pfc::string8("Library selection") : s; }
	void setSelectionPlaylist(const char* s) { cfg_selpl = s; }
	pfc::string8 getSortPattern() { return cfg_sort.get(); }
	void setSortPattern(const char* s) { cfg_sort = s; }

	std::vector<pfc::string8> getSearchHistory() {
		std::vector<pfc::string8> out;
		pfc::string_list_impl lines;
		pfc::splitStringByLines(lines, cfg_history.get());
		for (size_t i = 0; i < lines.get_count(); ++i) if (*lines[i]) out.emplace_back(lines[i]);
		return out;
	}
	void addSearchHistory(const char* s) {
		if (!s || !*s) return;
		auto h = getSearchHistory();
		h.erase(std::remove_if(h.begin(), h.end(), [&](const pfc::string8& x) { return x == s; }), h.end());
		h.insert(h.begin(), pfc::string8(s));
		if (h.size() > 15) h.resize(15);
		pfc::string8 out;
		for (auto& x : h) { pfc::string8 y = x; y.replace_char('\n', ' '); y.replace_char('\r', ' '); out << y << "\n"; }
		cfg_history = out;
	}
	void wipeSearchHistory() { cfg_history = ""; }

	static std::vector<IGlobalsListener*> g_listeners;
	void registerListener(IGlobalsListener* l) { g_listeners.push_back(l); }
	void unregisterListener(IGlobalsListener* l) { g_listeners.erase(std::remove(g_listeners.begin(), g_listeners.end(), l), g_listeners.end()); }
	void notifyGlobalsChanged(const GlobalsChange& change) { auto copy = g_listeners; for (auto* l : copy) l->globalsChanged(change); }

	pfc::string8 applyRenames(const RenameList& renames, const pfc::string8& name) {
		for (auto& r : renames) if (r.first == name) return r.second;
		return name;
	}

	void showPreferences() { ui_control::get()->show_preferences(guid_prefs_page); }
}
