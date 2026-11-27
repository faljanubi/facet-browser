#pragma once

// Settings shared by every Facet Browser panel (edited on the Preferences page),
// matching how Facets kept its column and filter definitions globally.

namespace fb {

	struct ColumnDef { pfc::string8 name, pattern; };
	struct FilterDef { pfc::string8 name, query; };

	// The built-in folder column: one folder level per pane, relative to the Media Library folder.
	extern const char* const kFolderColumn;

	std::vector<ColumnDef> getColumns();          // user columns (Folder is not included)
	void setColumns(const std::vector<ColumnDef>& v);
	std::vector<ColumnDef> defaultColumns();
	// Column keys: how panes refer to a column, unique even when several columns share a name.
	pfc::string8 columnKey(const std::vector<ColumnDef>& cols, size_t index);
	pfc::string8 columnKeyName(const char* key);         // the display name of a key
	bool findColumn(const char* key, ColumnDef& out);    // by key; false for Folder or unknown keys

	std::vector<FilterDef> getFilters();
	void setFilters(const std::vector<FilterDef>& v);
	std::vector<FilterDef> defaultFilters();

	// Custom statistics: a title formatting pattern evaluated per track (numbers only), combined
	// per item as total, average, lowest or highest value.
	enum class Combine : uint32_t { Total = 0, Average = 1, Lowest = 2, Highest = 3, Count };
	const char* combineName(Combine c);
	struct StatDef { pfc::string8 name, pattern; Combine combine = Combine::Total; };
	std::vector<StatDef> getCustomStats();
	void setCustomStats(const std::vector<StatDef>& v);
	// Statistic ids in pane settings: built-in Stat values, or kCustomStatBase + index into getCustomStats().
	const uint32_t kCustomStatBase = 1000;
	const uint32_t kMaxCustomStats = 200;
	inline bool isCustomStat(uint32_t s) { return s >= kCustomStatBase; }

	pfc::string8 getSelectionPlaylist();
	void setSelectionPlaylist(const char* s);
	pfc::string8 getSortPattern();
	void setSortPattern(const char* s);

	std::vector<pfc::string8> getSearchHistory();
	void addSearchHistory(const char* s);
	void wipeSearchHistory();

	// Live panels register here so they refresh when the global settings change.
	// Renames made on the Preferences page (old name -> new name), so panels can follow them.
	typedef std::vector<std::pair<pfc::string8, pfc::string8>> RenameList;
	struct GlobalsChange { RenameList columns, filters, stats; };
	class IGlobalsListener { public: virtual void globalsChanged(const GlobalsChange& change) = 0; };
	void registerListener(IGlobalsListener* l);
	void unregisterListener(IGlobalsListener* l);
	void notifyGlobalsChanged(const GlobalsChange& change = GlobalsChange());
	// Applies a rename list to one name (all renames at once, so swapping two names works).
	pfc::string8 applyRenames(const RenameList& renames, const pfc::string8& name);

	// Opens a Facet Browser preferences page.
	enum class PrefsPage { General, Columns, Filters, Statistics };
	void showPreferences(PrefsPage page = PrefsPage::General);
}
