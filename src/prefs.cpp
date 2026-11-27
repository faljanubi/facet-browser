#include "stdafx.h"
#include "globals.h"
#include "resource.h"

// Preferences: Media Library > Facet Browser (the Library selection settings), with child pages
// for the global column, filter and custom statistic definitions.

namespace fb {

	extern const GUID guid_prefs_page, guid_prefs_columns, guid_prefs_filters, guid_prefs_stats;

	static pfc::string8 getText(CWindow w) {
		CString s; w.GetWindowText(s);
		return pfc::stringcvt::string_utf8_from_wide(s).get_ptr();
	}
	static void setText(CWindow w, const char* s) { w.SetWindowText(pfc::stringcvt::string_wide_from_utf8(s)); }

	// Pixels-per-dialog-unit helper: places controls in dialog units, so the layout follows font and DPI.
	struct DluPlacer {
		CWindow dlg;
		HDWP dwp;
		explicit DluPlacer(CWindow d) : dlg(d), dwp(BeginDeferWindowPos(24)) {}
		~DluPlacer() { EndDeferWindowPos(dwp); }
		void operator()(int id, int x, int y, int w, int h) {
			CRect r(x, y, x + std::max(0, w), y + std::max(0, h));
			::MapDialogRect(dlg, r);
			if (CWindow c = dlg.GetDlgItem(id)) dwp = ::DeferWindowPos(dwp, c, nullptr, r.left, r.top, r.Width(), r.Height(), SWP_NOZORDER | SWP_NOACTIVATE);
		}
	};

	// The page size in dialog units (foobar2000 sizes the page to its page area, which can be
	// smaller than the dialog template).
	static CSize pageDlu(CWindow dlg, int minW, int minH) {
		CRect client; dlg.GetClientRect(client);
		CRect unit(0, 0, 400, 400); ::MapDialogRect(dlg, unit);   // pixels per 400 dialog units
		return CSize(std::max(minW, (int)(client.Width() * 400LL / unit.Width())), std::max(minH, (int)(client.Height() * 400LL / unit.Height())));
	}

	// ================================================================ Facet Browser (general)

	class CGeneralPage : public CDialogImpl<CGeneralPage>, public preferences_page_instance {
	public:
		enum { IDD = IDD_PREFS };
		CGeneralPage(preferences_page_callback::ptr cb) : m_callback(cb) {}

		BEGIN_MSG_MAP_EX(CGeneralPage)
			MSG_WM_INITDIALOG(OnInitDialog)
			MSG_WM_SIZE(OnSize)
			COMMAND_HANDLER_EX(IDC_SELPL, EN_CHANGE, OnEdit)
			COMMAND_HANDLER_EX(IDC_SORT, EN_CHANGE, OnEdit)
		END_MSG_MAP()

		t_uint32 get_state() override {
			t_uint32 s = preferences_state::resettable | preferences_state::dark_mode_supported;
			if (m_changed) s |= preferences_state::changed;
			return s;
		}
		void apply() override {
			setSelectionPlaylist(getText(GetDlgItem(IDC_SELPL)));
			setSortPattern(getText(GetDlgItem(IDC_SORT)));
			m_changed = false;
			m_callback->on_state_changed();
			notifyGlobalsChanged();
		}
		void reset() override {
			{
				auto g = pfc::autoToggle(m_loading, true);
				setText(GetDlgItem(IDC_SELPL), "Library selection");
				setText(GetDlgItem(IDC_SORT), "%path_sort%");
			}
			Changed();
		}

	private:
		BOOL OnInitDialog(CWindow, LPARAM) {
			auto g = pfc::autoToggle(m_loading, true);
			setText(GetDlgItem(IDC_SELPL), getSelectionPlaylist());
			setText(GetDlgItem(IDC_SORT), getSortPattern());
			setText(GetDlgItem(IDC_HELP_GENERAL),
				"Playlist: the playlist the Send to / Add to selection playlist actions fill.\r\n"
				"Sort pattern: the order in which tracks are sent to playlists, as title formatting.\r\n\r\n"
				"Columns, filters and custom statistics are defined on the pages below Facet Browser.");
			m_dark.AddDialogWithControls(*this);
			Layout();
			return FALSE;
		}
		void OnSize(UINT, CSize) { Layout(); }
		void Layout() {
			CRect client; GetClientRect(client);
			if (client.IsRectEmpty()) return;
			const CSize sz = pageDlu(*this, 240, 120);
			const int W = sz.cx;
			{
				DluPlacer place(*this);
				place(IDC_GRP_LIB, 0, 0, W, 50);
				place(IDC_LBL_SELPL, 7, 15, 44, 8);
				place(IDC_SELPL, 52, 13, W - 7 - 52, 13);
				place(IDC_LBL_SORT, 7, 32, 44, 8);
				place(IDC_SORT, 52, 30, W - 7 - 52, 13);
				place(IDC_HELP_GENERAL, 0, 58, W, sz.cy - 58);
			}
			Invalidate();
		}
		void OnEdit(UINT, int, CWindow) { if (!m_loading) Changed(); }
		void Changed() { m_changed = true; m_callback->on_state_changed(); }

		const preferences_page_callback::ptr m_callback;
		bool m_changed = false, m_loading = false;
		fb2k::CDarkModeHooks m_dark;
	};

	// ================================================================ list pages

	enum ListKind { kColumns, kFilters, kStats };

	struct Entry { pfc::string8 name, value; Combine combine = Combine::Total; };

	// Edits a list of named definitions with a list box, Add/Remove/Up/Down, two edit boxes and
	// (custom statistics) a Combine drop-down.
	struct ListEditor {
		std::vector<Entry> items;
		std::vector<pfc::string8> origin;   // per item: its name (columns: key) when last saved, empty for new items
		std::vector<pfc::string8> removed;  // columns: keys of saved items removed since the last save
		bool keyed = false;                 // columns: panes refer to them by key (see columnKey)
		CListBox list;
		CWindow nameEdit, valueEdit, btnRemove, btnUp, btnDown;
		CComboBox combine;                  // null unless custom statistics
		bool loading = false;

		void Fill() {
			auto g = pfc::autoToggle(loading, true);
			int sel = list.GetCurSel();
			list.ResetContent();
			for (auto& it : items) list.AddString(pfc::stringcvt::string_wide_from_utf8(it.name));
			if (!items.empty()) list.SetCurSel(std::max(0, std::min(sel, (int)items.size() - 1)));
			LoadCurrent();
		}
		void LoadCurrent() {
			auto g = pfc::autoToggle(loading, true);
			int i = list.GetCurSel();
			const bool have = i >= 0 && i < (int)items.size();
			nameEdit.EnableWindow(have); valueEdit.EnableWindow(have); btnRemove.EnableWindow(have);
			btnUp.EnableWindow(have && i > 0); btnDown.EnableWindow(have && i + 1 < (int)items.size());
			setText(nameEdit, have ? items[i].name.c_str() : "");
			setText(valueEdit, have ? items[i].value.c_str() : "");
			if (combine.m_hWnd) {
				combine.EnableWindow(have);
				combine.SetCurSel(have ? (int)items[i].combine : -1);
			}
		}
		bool OnEdit(bool nameChanged) {
			if (loading) return false;
			int i = list.GetCurSel();
			if (i < 0 || i >= (int)items.size()) return false;
			items[i].name = getText(nameEdit);
			items[i].value = getText(valueEdit);
			if (combine.m_hWnd) {
				const int c = combine.GetCurSel();
				if (c >= 0 && c < (int)Combine::Count) items[i].combine = (Combine)c;
			}
			if (nameChanged) {
				auto g = pfc::autoToggle(loading, true);
				list.DeleteString(i);
				list.InsertString(i, pfc::stringcvt::string_wide_from_utf8(items[i].name));
				list.SetCurSel(i);
			}
			return true;
		}
		void Add(const Entry& e) {
			items.push_back(e);
			origin.emplace_back();
			Fill();
			list.SetCurSel((int)items.size() - 1);
			LoadCurrent();
			nameEdit.SetFocus();
			CEdit(nameEdit).SetSelAll();
		}
		void Remove() {
			int i = list.GetCurSel();
			if (i < 0 || i >= (int)items.size()) return;
			if (keyed && !origin[i].is_empty()) removed.push_back(origin[i]);
			items.erase(items.begin() + i);
			origin.erase(origin.begin() + i);
			Fill();
		}
		// What panes will call each item once saved: its name, or for columns its key, counted over
		// the columns as they are saved (unnamed ones and "Folder" are left out). Empty = not saved.
		std::vector<pfc::string8> CurrentKeys() const {
			std::vector<pfc::string8> out(items.size());
			if (!keyed) { for (size_t i = 0; i < items.size(); ++i) out[i] = items[i].name; return out; }
			std::vector<ColumnDef> saved;
			std::vector<size_t> at;
			for (size_t i = 0; i < items.size(); ++i)
				if (!items[i].name.is_empty() && items[i].name != kFolderColumn) { saved.push_back({ items[i].name, items[i].value }); at.push_back(i); }
			for (size_t k = 0; k < saved.size(); ++k) out[at[k]] = columnKey(saved, k);
			return out;
		}
		void Load(const std::vector<Entry>& v, bool saved) {
			items = v;
			removed.clear();
			origin = saved ? CurrentKeys() : std::vector<pfc::string8>(items.size());
		}
		// Saved items whose name (key) changed, old -> new; then the current ones count as saved.
		// Columns also report removed ones (new key kRemovedKey, which never matches), because with
		// several same-named columns a removal renumbers the others and a later one takes its key.
		RenameList TakeRenames() {
			static const char* const kRemovedKey = "\t-";
			RenameList out;
			const auto keys = CurrentKeys();
			for (size_t i = 0; i < items.size(); ++i) {
				if (origin[i].is_empty() || origin[i] == keys[i]) continue;
				if (!keys[i].is_empty()) out.emplace_back(origin[i], keys[i]);
				else if (keyed) out.emplace_back(origin[i], kRemovedKey);
			}
			for (auto& r : removed) out.emplace_back(r, kRemovedKey);
			removed.clear();
			origin = keys;
			return out;
		}
		void Move(int delta) {
			int i = list.GetCurSel(), j = i + delta;
			if (i < 0 || j < 0 || j >= (int)items.size()) return;
			std::swap(items[i], items[j]);
			std::swap(origin[i], origin[j]);
			Fill();
			list.SetCurSel(j);
			LoadCurrent();
		}
	};

	template<int Kind>
	class CListPage : public CDialogImpl<CListPage<Kind>>, public preferences_page_instance {
	public:
		enum { IDD = IDD_PREFS_LIST };
		CListPage(preferences_page_callback::ptr cb) : m_callback(cb) {}

		BEGIN_MSG_MAP_EX(CListPage)
			MSG_WM_INITDIALOG(OnInitDialog)
			MSG_WM_SIZE(OnSize)
			COMMAND_HANDLER_EX(IDC_LIST, LBN_SELCHANGE, OnSelect)
			COMMAND_HANDLER_EX(IDC_NAME, EN_CHANGE, OnEdit)
			COMMAND_HANDLER_EX(IDC_VALUE, EN_CHANGE, OnEdit)
			COMMAND_HANDLER_EX(IDC_COMBINE, CBN_SELCHANGE, OnEdit)
			COMMAND_RANGE_HANDLER_EX(IDC_ADD, IDC_DOWN, OnButton)
		END_MSG_MAP()

		t_uint32 get_state() override {
			t_uint32 s = preferences_state::resettable | preferences_state::dark_mode_supported;
			if (m_changed) s |= preferences_state::changed;
			return s;
		}

		void apply() override {
			GlobalsChange change;
			if (Kind == kColumns) {
				std::vector<ColumnDef> v;
				for (auto& e : m_ed.items) if (!e.name.is_empty() && e.name != kFolderColumn) v.push_back({ e.name, e.value });
				setColumns(v);
				change.columns = m_ed.TakeRenames();
			} else if (Kind == kFilters) {
				std::vector<FilterDef> v;
				for (auto& e : m_ed.items) if (!e.name.is_empty()) v.push_back({ e.name, e.value });
				setFilters(v);
				change.filters = m_ed.TakeRenames();
			} else {
				std::vector<StatDef> v;
				for (auto& e : m_ed.items) if (!e.name.is_empty()) { StatDef d; d.name = e.name; d.pattern = e.value; d.combine = e.combine; v.push_back(d); }
				setCustomStats(v);
				change.stats = m_ed.TakeRenames();
			}
			m_changed = false;
			m_callback->on_state_changed();
			notifyGlobalsChanged(change);
		}

		void reset() override {
			m_ed.Load(Defaults(), false);
			m_ed.Fill();
			Changed();
		}

	private:
		static std::vector<Entry> Defaults() {
			std::vector<Entry> v;
			if (Kind == kColumns) for (auto& c : defaultColumns()) v.push_back({ c.name, c.pattern });
			else if (Kind == kFilters) for (auto& f : defaultFilters()) v.push_back({ f.name, f.query });
			return v;   // no custom statistics by default
		}
		static std::vector<Entry> Current() {
			std::vector<Entry> v;
			if (Kind == kColumns) for (auto& c : getColumns()) v.push_back({ c.name, c.pattern });
			else if (Kind == kFilters) for (auto& f : getFilters()) v.push_back({ f.name, f.query });
			else for (auto& s : getCustomStats()) { Entry e; e.name = s.name; e.value = s.pattern; e.combine = s.combine; v.push_back(e); }
			return v;
		}

		BOOL OnInitDialog(CWindow, LPARAM) {
			auto g = pfc::autoToggle(m_loading, true);
			const char* group = Kind == kColumns ? "Columns" : Kind == kFilters ? "Filters" : "Custom statistics";
			const char* valueLabel = Kind == kFilters ? "Query:" : "Pattern:";
			const char* help =
				Kind == kColumns ? "Title formatting. %<field>% lists each value of a multi-value tag separately. "
				                   "A | outside functions lists a track under each part, e.g. %<artist>%|%<album artist>%. "
				                   "The built-in Folder column shows one folder level per pane.\r\n\r\n"
				                   "Show a column in a pane from the pane's column header menu > Columns."
				: Kind == kFilters ? "foobar2000 query syntax, e.g. %rating% IS 5 or %added% DURING LAST 1 WEEK.\r\n\r\n"
				                     "Turn filters on with the Filter button on the toolbar."
				: "Title formatting that gives a number for each track, e.g. %play_count% or "
				  "$max(%play_count%,%lastfm_play_count%). Tracks without a number are skipped.\r\n\r\n"
				  "Combine: Total adds the numbers of an item's tracks, Average gives their mean, "
				  "Lowest and Highest the smallest and largest.\r\n\r\n"
				  "Show a statistic in a pane from the pane's column header menu > Statistics; it can be "
				  "sorted by like the built-in ones.";
			setText(this->GetDlgItem(IDC_GRP_LIST), group);
			setText(this->GetDlgItem(IDC_LBL_VALUE), valueLabel);
			setText(this->GetDlgItem(IDC_HELP_LIST), help);
			m_ed.list = this->GetDlgItem(IDC_LIST);
			m_ed.keyed = (Kind == kColumns);
			m_ed.nameEdit = this->GetDlgItem(IDC_NAME); m_ed.valueEdit = this->GetDlgItem(IDC_VALUE);
			m_ed.btnRemove = this->GetDlgItem(IDC_REMOVE); m_ed.btnUp = this->GetDlgItem(IDC_UP); m_ed.btnDown = this->GetDlgItem(IDC_DOWN);
			CComboBox combo(this->GetDlgItem(IDC_COMBINE));
			if (Kind == kStats) {
				for (uint32_t c = 0; c < (uint32_t)Combine::Count; ++c) combo.AddString(pfc::stringcvt::string_wide_from_utf8(combineName((Combine)c)));
				m_ed.combine = combo;
			} else {
				combo.ShowWindow(SW_HIDE);
				this->GetDlgItem(IDC_LBL_COMBINE).ShowWindow(SW_HIDE);
			}
			m_ed.Load(Current(), true);
			m_ed.Fill();
			m_dark.AddDialogWithControls(*this);
			Layout();
			return FALSE;
		}

		void OnSize(UINT, CSize) { Layout(); }

		// One group filling the page: the list on the left (buttons below it), the fields and the
		// help text on the right.
		void Layout() {
			CRect client; this->GetClientRect(client);
			if (client.IsRectEmpty()) return;
			const CSize sz = pageDlu(*this, 240, 120);
			const int W = sz.cx, H = sz.cy;
			const int listW = 110, rx = 124, ex = 158;   // list width, right-hand labels, edit boxes
			const int btnY = H - 19;
			{
				DluPlacer place(*this);
				place(IDC_GRP_LIST, 0, 0, W, H);
				place(IDC_LIST, 7, 12, listW, btnY - 3 - 12);
				place(IDC_ADD, 7, btnY, 38, 13);
				place(IDC_REMOVE, 47, btnY, 34, 13);
				place(IDC_UP, 83, btnY, 16, 13);
				place(IDC_DOWN, 101, btnY, 16, 13);
				place(IDC_LBL_NAME, rx, 14, ex - rx - 2, 8);
				place(IDC_NAME, ex, 12, W - 7 - ex, 13);
				place(IDC_LBL_VALUE, rx, 31, ex - rx - 2, 8);
				place(IDC_VALUE, ex, 29, W - 7 - ex, 13);
				int helpY = 47;
				if (Kind == kStats) {
					place(IDC_LBL_COMBINE, rx, 48, ex - rx - 2, 8);
					place(IDC_COMBINE, ex, 46, 70, 80);
					helpY = 65;
				}
				place(IDC_HELP_LIST, rx, helpY, W - 7 - rx, btnY + 13 - helpY);
			}
			this->Invalidate();
		}

		void OnSelect(UINT, int, CWindow) { m_ed.LoadCurrent(); }

		void OnEdit(UINT, int id, CWindow) {
			if (m_loading) return;
			if (m_ed.OnEdit(id == IDC_NAME)) Changed();
		}

		void OnButton(UINT, int id, CWindow) {
			switch (id) {
			case IDC_ADD:
				if (Kind == kColumns) m_ed.Add({ "New column", "%title%" });
				else if (Kind == kFilters) m_ed.Add({ "New filter", "" });
				else m_ed.Add({ "New statistic", "%play_count%" });
				break;
			case IDC_REMOVE: m_ed.Remove(); break;
			case IDC_UP: m_ed.Move(-1); break;
			case IDC_DOWN: m_ed.Move(1); break;
			default: return;
			}
			Changed();
		}

		void Changed() { m_changed = true; m_callback->on_state_changed(); }

		const preferences_page_callback::ptr m_callback;
		ListEditor m_ed;
		bool m_changed = false, m_loading = false;
		fb2k::CDarkModeHooks m_dark;
	};

	// ================================================================ registration

	class GeneralPage : public preferences_page_impl<CGeneralPage> {
	public:
		const char* get_name() override { return "Facet Browser"; }
		GUID get_guid() override { return guid_prefs_page; }
		GUID get_parent_guid() override { return guid_media_library; }
	};

	class ColumnsPage : public preferences_page_impl<CListPage<kColumns>> {
	public:
		const char* get_name() override { return "Columns"; }
		GUID get_guid() override { return guid_prefs_columns; }
		GUID get_parent_guid() override { return guid_prefs_page; }
		double get_sort_priority() override { return 1; }
	};

	class FiltersPage : public preferences_page_impl<CListPage<kFilters>> {
	public:
		const char* get_name() override { return "Filters"; }
		GUID get_guid() override { return guid_prefs_filters; }
		GUID get_parent_guid() override { return guid_prefs_page; }
		double get_sort_priority() override { return 2; }
	};

	class StatsPage : public preferences_page_impl<CListPage<kStats>> {
	public:
		const char* get_name() override { return "Statistics"; }
		GUID get_guid() override { return guid_prefs_stats; }
		GUID get_parent_guid() override { return guid_prefs_page; }
		double get_sort_priority() override { return 3; }
	};

	static preferences_page_factory_t<GeneralPage> g_general_factory;
	static preferences_page_factory_t<ColumnsPage> g_columns_factory;
	static preferences_page_factory_t<FiltersPage> g_filters_factory;
	static preferences_page_factory_t<StatsPage> g_stats_factory;
}
