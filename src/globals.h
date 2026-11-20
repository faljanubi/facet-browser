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
	bool findColumn(const char* name, ColumnDef& out);   // false for Folder or unknown names

	std::vector<FilterDef> getFilters();
	void setFilters(const std::vector<FilterDef>& v);
	std::vector<FilterDef> defaultFilters();

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
	struct GlobalsChange { RenameList columns, filters; };
	class IGlobalsListener { public: virtual void globalsChanged(const GlobalsChange& change) = 0; };
	void registerListener(IGlobalsListener* l);
	void unregisterListener(IGlobalsListener* l);
	void notifyGlobalsChanged(const GlobalsChange& change = GlobalsChange());
	// Applies a rename list to one name (all renames at once, so swapping two names works).
	pfc::string8 applyRenames(const RenameList& renames, const pfc::string8& name);

	// Opens the Facet Browser preferences page.
	void showPreferences();
}
