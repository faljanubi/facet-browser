#include "stdafx.h"
#include "globals.h"
#include "resource.h"

namespace fb {

	extern const GUID guid_prefs_page;

	static pfc::string8 getText(CWindow w) {
		CString s; w.GetWindowText(s);
		return pfc::stringcvt::string_utf8_from_wide(s).get_ptr();
	}
	static void setText(CWindow w, const char* s) { w.SetWindowText(pfc::stringcvt::string_wide_from_utf8(s)); }

	// Edits a list of name/value pairs with a list box, Add/Remove/Up/Down and two edit boxes.
	struct PairEditor {
		std::vector<std::pair<pfc::string8, pfc::string8>> items;
		std::vector<pfc::string8> origin;   // per item: its name when last saved, empty for new items
		CListBox list;
		CWindow nameEdit, valueEdit, btnRemove, btnUp, btnDown;
		bool loading = false;

		void Fill() {
			auto g = pfc::autoToggle(loading, true);
			int sel = list.GetCurSel();
			list.ResetContent();
			for (auto& it : items) list.AddString(pfc::stringcvt::string_wide_from_utf8(it.first));
			if (!items.empty()) list.SetCurSel(std::max(0, std::min(sel, (int)items.size() - 1)));
			LoadCurrent();
		}
		void LoadCurrent() {
			auto g = pfc::autoToggle(loading, true);
			int i = list.GetCurSel();
			const bool have = i >= 0 && i < (int)items.size();
			nameEdit.EnableWindow(have); valueEdit.EnableWindow(have); btnRemove.EnableWindow(have);
			btnUp.EnableWindow(have && i > 0); btnDown.EnableWindow(have && i + 1 < (int)items.size());
			setText(nameEdit, have ? items[i].first.c_str() : "");
			setText(valueEdit, have ? items[i].second.c_str() : "");
		}
		bool OnEdit(bool nameChanged) {
			if (loading) return false;
			int i = list.GetCurSel();
			if (i < 0 || i >= (int)items.size()) return false;
			items[i].first = getText(nameEdit);
			items[i].second = getText(valueEdit);
			if (nameChanged) {
				auto g = pfc::autoToggle(loading, true);
				list.DeleteString(i);
				list.InsertString(i, pfc::stringcvt::string_wide_from_utf8(items[i].first));
				list.SetCurSel(i);
			}
			return true;
		}
		void Add(const char* name, const char* value) {
			items.emplace_back(name, value);
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
			items.erase(items.begin() + i);
			origin.erase(origin.begin() + i);
			Fill();
		}
		void Load(const std::vector<std::pair<pfc::string8, pfc::string8>>& v, bool saved) {
			items = v;
			origin.clear();
			for (auto& it : items) origin.push_back(saved ? it.first : pfc::string8());
		}
		// Saved items whose name changed (old -> new); then the current names count as saved.
		RenameList TakeRenames() {
			RenameList out;
			for (size_t i = 0; i < items.size(); ++i)
				if (!origin[i].is_empty() && !items[i].first.is_empty() && origin[i] != items[i].first) out.emplace_back(origin[i], items[i].first);
			for (size_t i = 0; i < items.size(); ++i) origin[i] = items[i].first;
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

	class CPrefsDialog : public CDialogImpl<CPrefsDialog>, public preferences_page_instance {
	public:
		enum { IDD = IDD_PREFS };
		CPrefsDialog(preferences_page_callback::ptr cb) : m_callback(cb) {}

		BEGIN_MSG_MAP_EX(CPrefsDialog)
			MSG_WM_INITDIALOG(OnInitDialog)
			MSG_WM_SIZE(OnSize)
			COMMAND_HANDLER_EX(IDC_COLUMNS, LBN_SELCHANGE, OnSelect)
			COMMAND_HANDLER_EX(IDC_FILTERS, LBN_SELCHANGE, OnSelect)
			COMMAND_HANDLER_EX(IDC_COL_NAME, EN_CHANGE, OnEdit)
			COMMAND_HANDLER_EX(IDC_COL_PATTERN, EN_CHANGE, OnEdit)
			COMMAND_HANDLER_EX(IDC_FLT_NAME, EN_CHANGE, OnEdit)
			COMMAND_HANDLER_EX(IDC_FLT_QUERY, EN_CHANGE, OnEdit)
			COMMAND_HANDLER_EX(IDC_SELPL, EN_CHANGE, OnEdit)
			COMMAND_HANDLER_EX(IDC_SORT, EN_CHANGE, OnEdit)
			COMMAND_RANGE_HANDLER_EX(IDC_COL_ADD, IDC_COL_DOWN, OnButton)
			COMMAND_RANGE_HANDLER_EX(IDC_FLT_ADD, IDC_FLT_DOWN, OnButton)
		END_MSG_MAP()

		t_uint32 get_state() override {
			t_uint32 s = preferences_state::resettable | preferences_state::dark_mode_supported;
			if (m_changed) s |= preferences_state::changed;
			return s;
		}

		void apply() override {
			std::vector<ColumnDef> cols;
			for (auto& p : m_cols.items) if (!p.first.is_empty() && p.first != kFolderColumn) cols.push_back({ p.first, p.second });
			std::vector<FilterDef> flts;
			for (auto& p : m_flts.items) if (!p.first.is_empty()) flts.push_back({ p.first, p.second });
			setColumns(cols);
			setFilters(flts);
			GlobalsChange change;
			change.columns = m_cols.TakeRenames();
			change.filters = m_flts.TakeRenames();
			setSelectionPlaylist(getText(GetDlgItem(IDC_SELPL)));
			setSortPattern(getText(GetDlgItem(IDC_SORT)));
			m_changed = false;
			m_callback->on_state_changed();
			notifyGlobalsChanged(change);
		}

		void reset() override {
			std::vector<std::pair<pfc::string8, pfc::string8>> v;
			for (auto& c : defaultColumns()) v.emplace_back(c.name, c.pattern);
			m_cols.Load(v, false);
			v.clear();
			for (auto& f : defaultFilters()) v.emplace_back(f.name, f.query);
			m_flts.Load(v, false);
			m_cols.Fill(); m_flts.Fill();
			auto g = pfc::autoToggle(m_loading, true);
			setText(GetDlgItem(IDC_SELPL), "Library selection");
			setText(GetDlgItem(IDC_SORT), "%path_sort%");
			Changed();
		}

	private:
		BOOL OnInitDialog(CWindow, LPARAM) {
			auto g = pfc::autoToggle(m_loading, true);
			m_cols.list = GetDlgItem(IDC_COLUMNS);
			m_cols.nameEdit = GetDlgItem(IDC_COL_NAME); m_cols.valueEdit = GetDlgItem(IDC_COL_PATTERN);
			m_cols.btnRemove = GetDlgItem(IDC_COL_REMOVE); m_cols.btnUp = GetDlgItem(IDC_COL_UP); m_cols.btnDown = GetDlgItem(IDC_COL_DOWN);
			m_flts.list = GetDlgItem(IDC_FILTERS);
			m_flts.nameEdit = GetDlgItem(IDC_FLT_NAME); m_flts.valueEdit = GetDlgItem(IDC_FLT_QUERY);
			m_flts.btnRemove = GetDlgItem(IDC_FLT_REMOVE); m_flts.btnUp = GetDlgItem(IDC_FLT_UP); m_flts.btnDown = GetDlgItem(IDC_FLT_DOWN);
			std::vector<std::pair<pfc::string8, pfc::string8>> v;
			for (auto& c : getColumns()) v.emplace_back(c.name, c.pattern);
			m_cols.Load(v, true);
			v.clear();
			for (auto& f : getFilters()) v.emplace_back(f.name, f.query);
			m_flts.Load(v, true);
			m_cols.Fill(); m_flts.Fill();
			setText(GetDlgItem(IDC_SELPL), getSelectionPlaylist());
			setText(GetDlgItem(IDC_SORT), getSortPattern());
			m_dark.AddDialogWithControls(*this);
			Layout();
			return FALSE;
		}

		void OnSize(UINT, CSize) { Layout(); }

		// Fits the controls to the page area foobar2000 gives us (it is smaller than the dialog
		// template on some setups): the Library selection group keeps its height at the bottom,
		// Columns and Filters share the rest; edit boxes and help texts take the remaining width.
		// All measurements are dialog units, so they follow font and DPI.
		void Layout() {
			CRect client; GetClientRect(client);
			if (client.IsRectEmpty()) return;
			CRect unit(0, 0, 400, 400); MapDialogRect(unit);   // pixels per 400 dialog units
			const int W = std::max(240, (int)(client.Width() * 400LL / unit.Width()));
			const int H = std::max(180, (int)(client.Height() * 400LL / unit.Height()));
			HDWP dwp = BeginDeferWindowPos(32);
			auto place = [&](int id, int x, int y, int w, int h) {
				CRect r(x, y, x + std::max(0, w), y + std::max(0, h));
				MapDialogRect(r);
				if (CWindow c = GetDlgItem(id)) dwp = ::DeferWindowPos(dwp, c, nullptr, r.left, r.top, r.Width(), r.Height(), SWP_NOZORDER | SWP_NOACTIVATE);
			};
			const int gap = 3, libH = 46;
			const int groupH = (H - libH - 2 * gap) / 2;
			const int listW = 110, rx = 124, ex = 158;   // list width, right-hand labels, edit boxes
			struct Group { int grp, list, add, remove, up, down, lblName, name, lblValue, value, help; };
			const Group groups[2] = {
				{ IDC_GRP_COLS, IDC_COLUMNS, IDC_COL_ADD, IDC_COL_REMOVE, IDC_COL_UP, IDC_COL_DOWN, IDC_LBL_COL_NAME, IDC_COL_NAME, IDC_LBL_COL_PATTERN, IDC_COL_PATTERN, IDC_HELP_COLS },
				{ IDC_GRP_FLT, IDC_FILTERS, IDC_FLT_ADD, IDC_FLT_REMOVE, IDC_FLT_UP, IDC_FLT_DOWN, IDC_LBL_FLT_NAME, IDC_FLT_NAME, IDC_LBL_FLT_QUERY, IDC_FLT_QUERY, IDC_HELP_FLT },
			};
			for (int g = 0; g < 2; ++g) {
				const Group& c = groups[g];
				const int y = g * (groupH + gap);
				const int btnY = y + groupH - 19;
				place(c.grp, 0, y, W, groupH);
				place(c.list, 7, y + 12, listW, btnY - 3 - (y + 12));
				place(c.add, 7, btnY, 38, 13);
				place(c.remove, 47, btnY, 34, 13);
				place(c.up, 83, btnY, 16, 13);
				place(c.down, 101, btnY, 16, 13);
				place(c.lblName, rx, y + 14, ex - rx - 2, 8);
				place(c.name, ex, y + 12, W - 7 - ex, 13);
				place(c.lblValue, rx, y + 31, ex - rx - 2, 8);
				place(c.value, ex, y + 29, W - 7 - ex, 13);
				place(c.help, rx, y + 46, W - 7 - rx, groupH - 46 - 4);
			}
			const int ly = 2 * (groupH + gap);
			place(IDC_GRP_LIB, 0, ly, W, libH);
			place(IDC_LBL_SELPL, 7, ly + 14, 44, 8);
			place(IDC_SELPL, 52, ly + 12, W - 7 - 52, 13);
			place(IDC_LBL_SORT, 7, ly + 30, 44, 8);
			place(IDC_SORT, 52, ly + 28, W - 7 - 52, 13);
			EndDeferWindowPos(dwp);
			Invalidate();
		}

		void OnSelect(UINT, int id, CWindow) { (id == IDC_COLUMNS ? m_cols : m_flts).LoadCurrent(); }

		void OnEdit(UINT, int id, CWindow) {
			if (m_loading) return;
			bool changed = true;
			if (id == IDC_COL_NAME || id == IDC_COL_PATTERN) changed = m_cols.OnEdit(id == IDC_COL_NAME);
			else if (id == IDC_FLT_NAME || id == IDC_FLT_QUERY) changed = m_flts.OnEdit(id == IDC_FLT_NAME);
			if (changed) Changed();
		}

		void OnButton(UINT, int id, CWindow) {
			switch (id) {
			case IDC_COL_ADD: m_cols.Add("New column", "%title%"); break;
			case IDC_COL_REMOVE: m_cols.Remove(); break;
			case IDC_COL_UP: m_cols.Move(-1); break;
			case IDC_COL_DOWN: m_cols.Move(1); break;
			case IDC_FLT_ADD: m_flts.Add("New filter", ""); break;
			case IDC_FLT_REMOVE: m_flts.Remove(); break;
			case IDC_FLT_UP: m_flts.Move(-1); break;
			case IDC_FLT_DOWN: m_flts.Move(1); break;
			default: return;
			}
			Changed();
		}

		void Changed() { m_changed = true; m_callback->on_state_changed(); }

		const preferences_page_callback::ptr m_callback;
		PairEditor m_cols, m_flts;
		bool m_changed = false, m_loading = false;
		fb2k::CDarkModeHooks m_dark;
	};

	class PrefsPage : public preferences_page_impl<CPrefsDialog> {
	public:
		const char* get_name() override { return "Facet Browser"; }
		GUID get_guid() override { return guid_prefs_page; }
		GUID get_parent_guid() override { return guid_media_library; }
	};

	static preferences_page_factory_t<PrefsPage> g_prefs_factory;
}
