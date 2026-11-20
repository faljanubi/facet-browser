#include "stdafx.h"
#include "browser.h"

namespace fb {

	// {F3AE6A91-882B-4737-B944-2D787217AF90}
	static const GUID guid_element = { 0xf3ae6a91, 0x882b, 0x4737, { 0xb9, 0x44, 0x2d, 0x78, 0x72, 0x17, 0xaf, 0x90 } };
	static const wchar_t* const kHelpUrl = L"https://wiki.hydrogenaud.io/index.php?title=Foobar2000:Query_syntax";

	// ================================================================ pane list

	void paintHeaderDividers(HWND header, HDC dc) {
		CHeaderCtrl hdr(header);
		CRect client; hdr.GetClientRect(client);
		const int n = hdr.GetItemCount();
		CDCHandle d(dc);
		const COLORREF line = ::GetSysColor(COLOR_WINDOWFRAME);
		for (int i = 0; i < n; ++i) {
			CRect r; if (!hdr.GetItemRect(i, r)) continue;
			d.FillSolidRect(CRect(r.right - 1, client.top, r.right, client.bottom), line);
		}
	}

	LRESULT CPaneList::OnHeaderDraw(LPNMHDR hdr) {
		if (hdr->hwndFrom != GetHeaderCtrl().m_hWnd || !GetDarkMode()) { SetMsgHandled(FALSE); return 0; }
		auto cd = reinterpret_cast<LPNMCUSTOMDRAW>(hdr);
		switch (cd->dwDrawStage) {
		case CDDS_PREPAINT: return CDRF_NOTIFYITEMDRAW | CDRF_NOTIFYPOSTPAINT;
		case CDDS_ITEMPREPAINT: return DarkMode::CustomDrawHeader(hdr, GetDarkModeParam());   // light label text, as libPPUI does
		case CDDS_POSTPAINT: paintHeaderDividers(hdr->hwndFrom, cd->hdc); return CDRF_DODEFAULT;
		default: return CDRF_DODEFAULT;
		}
	}

	void CPaneList::RenderSubItemText(t_size item, t_size subItem, const CRect& rc, const CRect& update, CDCHandle dc, bool allowColors) {
		if (subItem == 0 && hasChildren && hasChildren(item)) {
			const int arrowW = MulDiv(14, GetDPI().cx, 96);
			CRect textRc = rc; textRc.right = std::max(textRc.left, textRc.right - arrowW);
			__super::RenderSubItemText(item, subItem, textRc, update, dc, allowColors);
			CRect arrow(textRc.right, rc.top, rc.right, rc.bottom);
			dc.SetBkMode(TRANSPARENT);
			dc.DrawText(L"›", 1, arrow, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
			return;
		}
		__super::RenderSubItemText(item, subItem, rc, update, dc, allowColors);
	}

	// ================================================================ element glue

	GUID CFacetBrowser::g_get_guid() { return guid_element; }

	ui_element_config::ptr CFacetBrowser::makeConfig(const BrowserConfig& cfg) {
		ui_element_config_builder out;
		cfg.write(out);
		return out.finish(guid_element);
	}

	BrowserConfig CFacetBrowser::parseConfig(ui_element_config::ptr cfg) {
		BrowserConfig c = BrowserConfig::makeDefault();
		try {
			ui_element_config_parser in(cfg);
			if (in.get_remaining() > 0) c.read(in);
		} catch (...) {
			c = BrowserConfig::makeDefault();
		}
		return c;
	}

	ui_element_config::ptr CFacetBrowser::g_get_default_configuration() { return makeConfig(BrowserConfig::makeDefault()); }

	CFacetBrowser::CFacetBrowser(ui_element_config::ptr cfg, ui_element_instance_callback::ptr callback)
		: playlist_callback_single_impl_base(flag_on_items_added | flag_on_items_removed | flag_on_items_modified | flag_on_items_replaced | flag_on_playlist_switch),
		m_callback(callback), m_cfg(parseConfig(cfg)), m_searchEdit(this, 1), m_searchCombo(this, 2) {}

	CFacetBrowser::~CFacetBrowser() {}

	void CFacetBrowser::initialize_window(HWND parent) {
		WIN32_OP(Create(parent, nullptr, nullptr, WS_CHILD | WS_CLIPCHILDREN | WS_CLIPSIBLINGS, 0) != NULL);
	}

	void CFacetBrowser::set_configuration(ui_element_config::ptr cfg) {
		m_cfg = parseConfig(cfg);
		if (m_hWnd) ApplyConfig();
	}

	void CFacetBrowser::notify(const GUID& what, t_size, const void*, t_size) {
		if (!m_hWnd) return;
		if (what == ui_element_notify_colors_changed) ApplyColors();
		else if (what == ui_element_notify_font_changed) { ApplyFonts(); Layout(); }
	}

	// ================================================================ window

	int CFacetBrowser::OnCreate(LPCREATESTRUCT) {
		CRect rc(0, 0, 10, 10), rcCombo(0, 0, 10, 300);
		m_btnSource.Create(*this, rc, L"Library", WS_CHILD | WS_TABSTOP | BS_PUSHBUTTON, 0, IDC_SOURCE);
		m_btnFilter.Create(*this, rc, L"Filter", WS_CHILD | WS_TABSTOP | BS_PUSHBUTTON, 0, IDC_FILTER);
		m_search.Create(*this, rcCombo, nullptr, WS_CHILD | WS_TABSTOP | WS_VSCROLL | CBS_DROPDOWN | CBS_AUTOHSCROLL, 0, IDC_SEARCH);
		m_btnAction.Create(*this, rc, L"", WS_CHILD | WS_TABSTOP | BS_SPLITBUTTON, 0, IDC_ACTION);
		m_btnHelp.Create(*this, rc, L"?", WS_CHILD | WS_TABSTOP | BS_PUSHBUTTON, 0, IDC_HELPBTN);
		COMBOBOXINFO info = { sizeof(info) };
		if (m_search.GetComboBoxInfo(&info) && info.hwndItem) {
			m_searchEdit.SubclassWindow(info.hwndItem);
		m_searchCombo.SubclassWindow(m_search);
			CEdit(info.hwndItem).SetCueBannerText(L"Search", TRUE);
		}
		// toolbar buttons are drawn by OnButtonCustomDraw (the themed ones have a white edge in dark mode)
		m_dark.AddComboBox(m_search);
		library_manager_v3::get()->register_callback(this);
		registerListener(this);
		FillSearchHistory();
		ApplyConfig();
		return 0;
	}

	void CFacetBrowser::OnDestroy() {
		unregisterListener(this);
		library_manager_v3::get()->unregister_callback(this);
		DestroyPanes();
	}

	void CFacetBrowser::DestroyPanes() {
		for (auto& p : m_panes) {
			if (p.art) p.art->dead = true;
			if (p.list && p.list->m_hWnd) p.list->DestroyWindow();
			if (p.grid && p.grid->m_hWnd) p.grid->DestroyWindow();
			m_graveyard.push_back(std::move(p));
		}
		m_panes.clear();
		if (m_hWnd && !m_graveyard.empty()) SetTimer(kFreePanesTimer, 50);
	}

	void CFacetBrowser::ApplyConfig() {
		DestroyPanes();
		CreatePanes();
		const int show = m_cfg.showToolbar ? SW_SHOWNA : SW_HIDE;
		for (CWindow w : { (CWindow)m_btnSource, (CWindow)m_btnFilter, (CWindow)m_search, (CWindow)m_btnAction, (CWindow)m_btnHelp }) w.ShowWindow(show);
		UpdateToolbarText();
		ApplyFonts();
		ApplyColors();
		RebuildSource();
		Layout();
	}

	void CFacetBrowser::globalsChanged(const GlobalsChange& change) {
		// follow columns and filters renamed on the Preferences page
		for (auto& pc : m_cfg.panes) for (auto& c : pc.columns) c = applyRenames(change.columns, c);
		for (auto& f : m_cfg.filters) f = applyRenames(change.filters, f);
		if (!m_hWnd) return;
		UpdateToolbarText();
		CreatePanesKeepSelection();
	}

	void CFacetBrowser::CreatePanesKeepSelection() {
		std::vector<std::vector<pfc::string8>> sel;
		for (auto& p : m_panes) sel.push_back(p.selected);
		DestroyPanes();
		CreatePanes();
		for (size_t i = 0; i < m_panes.size() && i < sel.size(); ++i) m_panes[i].selected = sel[i];
		ApplyFonts();
		ApplyColors();
		RebuildSource();
		Layout();
	}

	std::vector<PaneSpec> CFacetBrowser::ResolveSpecs() const {
		std::vector<PaneSpec> specs;
		uint32_t depth = 0;
		for (auto& pc : m_cfg.panes) {
			PaneSpec spec;
			bool folderHere = false;
			for (auto& name : pc.columns) {
				ColumnSpec cs;
				cs.name = name;
				if (name == kFolderColumn) {
					if (folderHere) continue;   // one folder level per pane
					folderHere = true;
					cs.folder = true;
					cs.depth = depth + 1;
				} else {
					ColumnDef def;
					if (!findColumn(name, def)) continue;   // column was removed in Preferences
					cs.pattern = def.pattern;
				}
				spec.columns.push_back(cs);
			}
			if (folderHere) depth++;
			if (spec.columns.empty()) {
				ColumnSpec cs; cs.name = "Title"; cs.pattern = "%title%";
				spec.columns.push_back(cs);
			}
			specs.push_back(spec);
		}
		return specs;
	}

	void CFacetBrowser::CreatePanes() {
		auto specs = ResolveSpecs();
		m_panes.resize(m_cfg.panes.size());
		CRect rc0(0, 0, 10, 10);
		for (size_t i = 0; i < m_panes.size(); ++i) {
			auto& pane = m_panes[i];
			pane.spec = specs[i];
			const auto& pc = m_cfg.panes[i];
			if (pc.display != Display::Columns) {
				pane.art = std::make_unique<ArtAdapter>();
				pane.art->owner = this; pane.art->pane = i;
				pane.grid = std::make_unique<CArtGrid>(pane.art.get());
				pane.grid->SetMode(pc.display);
				pane.grid->SetArtType(pc.artType);
				pane.grid->Create(*this, rc0, nullptr, WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_TABSTOP | WS_CLIPSIBLINGS | WS_CLIPCHILDREN, 0, IDC_PANE0 + (UINT)i);
				m_dark.AddGeneric(*pane.grid);
				m_dark.AddHeader(pane.grid->GetHeader());
				pane.grid->isSplitterZone = [this](CPoint pt) { return InSplitterZone(pt); };
			} else {
				pane.list = std::make_unique<CPaneList>(m_callback, static_cast<IListControlOwnerDataSource*>(this));
				pane.list->Create(*this, rc0, nullptr, 0, 0, IDC_PANE0 + (UINT)i);
				pane.list->SetSelectionModeMulti();
				pane.list->hasChildren = [this, i](size_t row) { const FacetItem* it = RowItem(i, row); return it && it->hasChildren; };
				pane.list->isSplitterZone = [this](CPoint pt) { return InSplitterZone(pt); };
			}
			UpdateColumns(i);
		}
	}

	// Column that shows the sort indicator: the sorted column, or the column of the sort statistic
	// when it is displayed; -1 when the sort statistic is hidden.
	static int sortIndicatorColumn(const PaneConfig& pc, size_t keyColumns) {
		if (pc.sortStat < 0) return pc.sortColumn;
		auto it = std::find(pc.stats.begin(), pc.stats.end(), (uint32_t)pc.sortStat);
		return it == pc.stats.end() ? -1 : (int)(keyColumns + (it - pc.stats.begin()));
	}

	static bool paneUsesStat(const PaneConfig& pc, Stat s) {
		return pc.sortStat == (int32_t)s || std::find(pc.stats.begin(), pc.stats.end(), (uint32_t)s) != pc.stats.end();
	}

	void CFacetBrowser::UpdateColumns(size_t i) {
		auto& pane = m_panes[i];
		const auto& pc = m_cfg.panes[i];
		std::vector<std::pair<pfc::string8, bool>> cols;
		for (auto& c : pane.spec.columns) cols.emplace_back(c.name, false);
		for (auto s : pc.stats) cols.emplace_back(statName((Stat)s), true);
		const int sortCol = sortIndicatorColumn(pc, pane.spec.columns.size());
		if (pane.grid) { pane.grid->SetColumns(cols, sortCol, pc.sortDescending); return; }
		if (!pane.list) return;
		auto& list = *pane.list;
		if (list.IsHeaderEnabled()) list.ResetColumns(false);
		for (size_t c = 0; c < cols.size(); ++c) {
			uint32_t w = 100;
			if (cols[c].second) {
				const Stat st = (Stat)pc.stats[c - pane.spec.columns.size()];
				const char* sample = "00000";
				switch (st) {
				case Stat::FirstAdded: case Stat::LastAdded: case Stat::FirstPlayed: case Stat::LastPlayed: case Stat::LastModified: case Stat::FileCreated:
					sample = "0000-00-00 00:00:00"; break;
				case Stat::FirstRelease: case Stat::LastRelease: sample = "0000-00-00"; break;
				case Stat::Duration: case Stat::PlayedDuration: sample = "00d 00:00:00"; break;
				case Stat::TotalSize: sample = "000.00 GB"; break;
				case Stat::Bitrate: sample = "0000 kbps"; break;
				case Stat::TotalPeak: sample = "0.000000"; break;
				default: break;
				}
				w = std::max(list.GetOptimalColumnWidthFixed(sample), list.GetOptimalColumnWidthFixed(cols[c].first));
			}
			list.AddColumn(cols[c].first, w, cols[c].second ? HDF_RIGHT : HDF_LEFT, false);
		}
		if (sortCol >= 0 && sortCol < (int)cols.size()) list.SetSortIndicator(sortCol, !pc.sortDescending);
		list.ReloadData();
	}

	void CFacetBrowser::ApplyFonts() {
		HFONT listFont = m_callback->query_font_ex(ui_font_lists);
		HFONT uiFont = m_callback->query_font_ex(ui_font_default);
		for (CWindow w : { (CWindow)m_btnSource, (CWindow)m_btnFilter, (CWindow)m_search, (CWindow)m_btnAction, (CWindow)m_btnHelp }) w.SetFont(uiFont);
		for (size_t i = 0; i < m_panes.size(); ++i) {
			auto& p = m_panes[i];
			if (p.list) { p.list->SetFont(listFont); UpdateColumns(i); }
			if (p.grid) {
				p.grid->SetFont(listFont);
				HFONT hdrFont = uiFont;
				for (auto& q : m_panes) if (q.list) { if (HWND h = q.list->GetHeaderCtrl()) { if (HFONT hf = CWindow(h).GetFont()) { hdrFont = hf; break; } } }
				p.grid->SetHeaderFont(hdrFont);
			}
		}
		CClientDC dc(*this);
		HFONT old = dc.SelectFont(uiFont);
		TEXTMETRIC tm = {}; dc.GetTextMetrics(&tm);
		dc.SelectFont(old);
		const int dpi = QueryScreenDPI(*this);
		m_toolbarH = tm.tmHeight + MulDiv(12, dpi, 96);
		m_gap = MulDiv(4, dpi, 96);
		m_border = 1;                         // foobar2000's own frames are 1px at any scaling
		m_grab = MulDiv(3, dpi, 96);          // extra pixels either side of a divider that still grab it
	}

	void CFacetBrowser::ApplyColors() {
		if (!m_editBrush.IsNull()) m_editBrush.DeleteObject();
		m_editBrush.CreateSolidBrush(m_callback->query_std_color(ui_color_background));
		for (auto& p : m_panes) {
			if (p.list) p.list->ui_colors_changed();
			if (p.grid) p.grid->ColorsChanged();
		}
		Invalidate();
	}

	HBRUSH CFacetBrowser::OnCtlColorEdit(CDCHandle dc, CEdit) {
		dc.SetTextColor(m_callback->query_std_color(ui_color_text));
		dc.SetBkColor(m_callback->query_std_color(ui_color_background));
		return m_editBrush;
	}

	BOOL CFacetBrowser::OnEraseBkgnd(CDCHandle dc) {
		CRect rc; GetClientRect(rc);
		dc.FillSolidRect(rc, m_callback->query_std_color(ui_color_background));
		return TRUE;
	}

	void CFacetBrowser::OnPaint(CDCHandle) {
		CPaintDC dc(*this);
		{
			const COLORREF line = LineColor();
			// Outer frame around all panes.
			if (!m_panes.empty() && !m_frameRect.IsRectEmpty()) {
				const CRect& f = m_frameRect; const int w = m_border;
				dc.FillSolidRect(CRect(f.left, f.top, f.right, f.top + w), line);
				dc.FillSolidRect(CRect(f.left, f.bottom - w, f.right, f.bottom), line);
				dc.FillSolidRect(CRect(f.left, f.top, f.left + w, f.bottom), line);
				dc.FillSolidRect(CRect(f.right - w, f.top, f.right, f.bottom), line);
			}
			// One divider centred in each gap between neighbouring panes.
			for (size_t i = 0; i + 1 < m_panes.size(); ++i) {
				const CRect& a = m_panes[i].rect; const CRect& b = m_panes[i + 1].rect;
				if (a.IsRectEmpty() || b.IsRectEmpty()) continue;
				const int x = (a.right + b.left - m_border) / 2;
				dc.FillSolidRect(CRect(x, a.top, x + m_border, a.bottom), line);
			}
		}
		if (m_error.length() > 0) {
			CRect rc; GetClientRect(rc);
			rc.DeflateRect(m_gap * 3, m_gap * 3);
			dc.SetBkMode(TRANSPARENT);
			dc.SetTextColor(m_callback->query_std_color(ui_color_text));
			HFONT old = dc.SelectFont(m_callback->query_font_ex(ui_font_default));
			dc.DrawText(pfc::stringcvt::string_wide_from_utf8(m_error), -1, rc, DT_WORDBREAK | DT_NOPREFIX);
			dc.SelectFont(old);
		}
	}

	void CFacetBrowser::Layout() {
		if (!m_hWnd) return;
		CRect rc; GetClientRect(rc);
		CRect area = rc;
		m_toolbarRect.SetRectEmpty();
		if (m_cfg.showToolbar) {
			const int dpi = QueryScreenDPI(*this);
			if (m_cfg.toolbarBottom) { m_toolbarRect = CRect(rc.left, rc.bottom - m_toolbarH, rc.right, rc.bottom); area.bottom = m_toolbarRect.top; }
			else { m_toolbarRect = CRect(rc.left, rc.top, rc.right, rc.top + m_toolbarH); area.top = m_toolbarRect.bottom; }
			CRect bar = m_toolbarRect; bar.DeflateRect(m_gap, m_gap);
			CClientDC dc(*this);
			HFONT old = dc.SelectFont(m_callback->query_font_ex(ui_font_default));
			auto textW = [&](CWindow w, int extra) {
				CString s; w.GetWindowText(s);
				CSize sz; dc.GetTextExtent(s, s.GetLength(), &sz);
				return sz.cx + MulDiv(extra, dpi, 96);
			};
			const int wSource = textW(m_btnSource, 28), wFilter = textW(m_btnFilter, 28);
			const int wAction = textW(m_btnAction, 44), wHelp = MulDiv(26, dpi, 96);
			dc.SelectFont(old);
			int x = bar.left;
			m_btnSource.SetWindowPos(NULL, x, bar.top, wSource, bar.Height(), SWP_NOZORDER | SWP_NOACTIVATE); x += wSource + m_gap;
			m_btnFilter.SetWindowPos(NULL, x, bar.top, wFilter, bar.Height(), SWP_NOZORDER | SWP_NOACTIVATE); x += wFilter + m_gap;
			int right = bar.right;
			m_btnHelp.SetWindowPos(NULL, right - wHelp, bar.top, wHelp, bar.Height(), SWP_NOZORDER | SWP_NOACTIVATE); right -= wHelp + m_gap;
			m_btnAction.SetWindowPos(NULL, right - wAction, bar.top, wAction, bar.Height(), SWP_NOZORDER | SWP_NOACTIVATE); right -= wAction + m_gap;
			m_search.SetWindowPos(NULL, x, bar.top, std::max(40, right - x), bar.Height() * 12, SWP_NOZORDER | SWP_NOACTIVATE);
			CRect cr; m_search.GetWindowRect(cr);
			m_search.SetWindowPos(NULL, x, bar.top + (bar.Height() - cr.Height()) / 2, std::max(40, right - x), bar.Height() * 12, SWP_NOZORDER | SWP_NOACTIVATE);
		}

		const size_t n = m_panes.size();
		m_frameRect = area;
		area.DeflateRect(m_border, m_border); // outer frame, painted in OnPaint
		if (n == 0) return;
		uint64_t total = 0;
		for (auto& p : m_cfg.panes) total += p.weight;
		const int span = area.Width() - (int)(n - 1) * m_border;
		int pos = area.left;
		HDWP dwp = BeginDeferWindowPos((int)n);
		for (size_t i = 0; i < n; ++i) {
			int size = (i + 1 == n) ? (area.right - pos) : (int)((uint64_t)std::max(0, span) * m_cfg.panes[i].weight / std::max<uint64_t>(1, total));
			CRect r(pos, area.top, pos + size, area.bottom);
			m_panes[i].rect = r;
			if (CWindow w = m_panes[i].wnd()) dwp = ::DeferWindowPos(dwp, w, NULL, r.left, r.top, r.Width(), r.Height(), SWP_NOZORDER | SWP_NOACTIVATE);
			pos += size + m_border;
		}
		EndDeferWindowPos(dwp);

		for (size_t i = 0; i < n; ++i) FitKeyColumns(i);
		Invalidate();
	}

	// Key columns share the width left over by the statistics columns. Called again after every
	// reload, since loading rows can add a vertical scrollbar and narrow the list.
	void CFacetBrowser::FitKeyColumns(size_t i) {
		auto& p = m_panes[i];
		if (!p.list) return;
		CRect lr; p.list->GetClientRect(lr);
		const size_t keys = p.spec.columns.size(), total = keys + m_cfg.panes[i].stats.size();
		int stats = 0;
		for (size_t c = keys; c < total; ++c) stats += p.list->GetHeaderItemWidth((int)c);
		int each = (lr.Width() - stats) / (int)std::max<size_t>(1, keys);
		if (each > 20) for (size_t c = 0; c < keys; ++c) p.list->ResizeColumn(c, each, c + 1 == keys);
	}

	int CFacetBrowser::SplitterAt(CPoint pt) const {
		for (size_t i = 0; i + 1 < m_panes.size(); ++i) {
			const CRect& r = m_panes[i].rect;
			if (pt.x >= r.right - m_grab && pt.x < r.right + m_border + m_grab && pt.y >= r.top && pt.y < r.bottom) return (int)i;
		}
		return -1;
	}

	void CFacetBrowser::OnLButtonDown(UINT, CPoint pt) {
		m_drag = SplitterAt(pt);
		if (m_drag < 0) return;
		m_dragStart = pt;
		m_dragWeights.clear();
		for (auto& p : m_cfg.panes) m_dragWeights.push_back(p.weight);
		SetCapture();
	}

	void CFacetBrowser::OnMouseMove(UINT, CPoint pt) {
		if (m_drag < 0 || GetCapture() != m_hWnd) return;
		const size_t a = (size_t)m_drag, b = a + 1;
		uint64_t total = 0; for (auto w : m_dragWeights) total += w;
		CRect rc; GetClientRect(rc);
		const int span = rc.Width() - (int)(m_panes.size() - 1) * m_border;
		const double pxPerWeight = span > 0 ? (double)span / total : 1;
		const int delta = pt.x - m_dragStart.x;
		const double wa = m_dragWeights[a] + delta / pxPerWeight, wb = m_dragWeights[b] - delta / pxPerWeight;
		const double minW = 30 / pxPerWeight;
		if (wa < minW || wb < minW) return;
		m_cfg.panes[a].weight = (uint32_t)wa;
		m_cfg.panes[b].weight = (uint32_t)wb;
		Layout();
	}

	void CFacetBrowser::OnLButtonUp(UINT, CPoint) {
		if (m_drag >= 0) { m_drag = -1; ReleaseCapture(); }
	}

	BOOL CFacetBrowser::OnSetCursor(CWindow wnd, UINT hit, UINT) {
		if (wnd == m_hWnd && hit == HTCLIENT) {
			CPoint pt; GetCursorPos(&pt); ScreenToClient(&pt);
			if (SplitterAt(pt) >= 0 || m_drag >= 0) { SetCursor(LoadCursor(NULL, IDC_SIZEWE)); return TRUE; }
		}
		SetMsgHandled(FALSE);
		return FALSE;
	}

	void CFacetBrowser::OnSetFocus(CWindow) {
		if (!m_panes.empty()) {
			size_t i = std::min(m_lastPane, m_panes.size() - 1);
			if (CWindow w = m_panes[i].wnd()) w.SetFocus();
		}
	}

	void CFacetBrowser::OnTimer(UINT_PTR id) {
		if (id == kFreePanesTimer) { FreeDeadPanes(); return; }
		KillTimer(id);
		if (id == TIMER_REBUILD || id == TIMER_SEARCH) RebuildSource();
	}

	static COLORREF mixColor(COLORREF a, COLORREF b, int pct) {
		return RGB((GetRValue(a) * (100 - pct) + GetRValue(b) * pct) / 100, (GetGValue(a) * (100 - pct) + GetGValue(b) * pct) / 100, (GetBValue(a) * (100 - pct) + GetBValue(b) * pct) / 100);
	}

	COLORREF CFacetBrowser::LineColor() const {
		// Dark mode: the frame colour foobar2000's own controls use (tab control, headers): COLOR_WINDOWFRAME, #646464.
		if (m_callback->is_dark_mode()) return ::GetSysColor(COLOR_WINDOWFRAME);
		return mixColor(m_callback->query_std_color(ui_color_background), m_callback->query_std_color(ui_color_text), 12);
	}

	LRESULT CFacetBrowser::OnButtonCustomDraw(LPNMHDR hdr) {
		const HWND from = hdr->hwndFrom;
		if (from != m_btnSource && from != m_btnFilter && from != m_btnAction && from != m_btnHelp) { SetMsgHandled(FALSE); return CDRF_DODEFAULT; }
		auto cd = reinterpret_cast<LPNMCUSTOMDRAW>(hdr);
		if (cd->dwDrawStage != CDDS_PREPAINT) return CDRF_DODEFAULT;

		CWindow btn(from);
		CDCHandle dc(cd->hdc);
		CRect rc = cd->rc;
		const COLORREF bg = m_callback->query_std_color(ui_color_background);
		const COLORREF tx = m_callback->query_std_color(ui_color_text);
		const bool dark = m_callback->is_dark_mode();
		const UINT st = cd->uItemState;
		const LRESULT bstate = btn.SendMessage(BM_GETSTATE);
		const bool pressed = (st & CDIS_SELECTED) || (bstate & BST_PUSHED);
		const bool hot = (st & CDIS_HOT) != 0;
		const bool disabled = (st & CDIS_DISABLED) != 0;
		const COLORREF fill = pressed ? mixColor(bg, tx, dark ? 16 : 14) : hot ? mixColor(bg, tx, dark ? 9 : 7) : bg;

		dc.FillSolidRect(rc, fill);
		{ CBrush frame; frame.CreateSolidBrush(LineColor()); CRect f = rc; for (int k = 0; k < m_border; ++k) { dc.FrameRect(f, frame); f.DeflateRect(1, 1); } }

		CRect textRc = rc;
		const bool split = from == m_btnAction;
		if (split) {
			BUTTON_SPLITINFO si = {}; si.mask = BCSIF_SIZE;
			int splitW = MulDiv(16, QueryScreenDPI(*this), 96);
			if (btn.SendMessage(BCM_GETSPLITINFO, 0, (LPARAM)&si) && si.size.cx > 0) splitW = si.size.cx;
			CRect arrowRc(rc.right - splitW, rc.top, rc.right, rc.bottom);
			if (bstate & BST_DROPDOWNPUSHED) { CRect a = arrowRc; a.DeflateRect(m_border, m_border); dc.FillSolidRect(a, mixColor(bg, tx, dark ? 16 : 14)); }
			dc.FillSolidRect(CRect(arrowRc.left, rc.top + m_border, arrowRc.left + m_border, rc.bottom - m_border), LineColor());
			// down-pointing triangle
			const int cx = (arrowRc.left + arrowRc.right) / 2, cy = (rc.top + rc.bottom) / 2;
			const int h = std::max(2, MulDiv(3, QueryScreenDPI(*this), 96));
			POINT tri[3] = { { cx - h, cy - h / 2 }, { cx + h + 1, cy - h / 2 }, { cx, cy + h / 2 + 1 } };
			CBrush tb; tb.CreateSolidBrush(disabled ? mixColor(bg, tx, 45) : tx);
			CPen tp; tp.CreatePen(PS_SOLID, 1, disabled ? mixColor(bg, tx, 45) : tx);
			HBRUSH ob = dc.SelectBrush(tb); HPEN op = dc.SelectPen(tp);
			dc.Polygon(tri, 3);
			dc.SelectBrush(ob); dc.SelectPen(op);
			textRc.right = arrowRc.left;
		}

		CString text; btn.GetWindowText(text);
		HFONT old = dc.SelectFont(btn.GetFont());
		dc.SetBkMode(TRANSPARENT);
		dc.SetTextColor(disabled ? mixColor(bg, tx, 45) : tx);
		dc.DrawText(text, text.GetLength(), textRc, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX | DT_END_ELLIPSIS);
		dc.SelectFont(old);
		if ((st & CDIS_FOCUS) && !(btn.SendMessage(WM_QUERYUISTATE) & UISF_HIDEFOCUS)) {
			CRect f = rc; f.DeflateRect(3, 3); dc.SetTextColor(tx); dc.DrawFocusRect(f);
		}
		return CDRF_SKIPDEFAULT;
	}

	bool CFacetBrowser::InSplitterZone(CPoint screenPt) const {
		if (!m_hWnd) return false;
		CPoint pt = screenPt; ScreenToClient(&pt);
		return SplitterAt(pt) >= 0;
	}

	LRESULT CFacetBrowser::OnSearchComboPaint(UINT msg, WPARAM wp, LPARAM lp) {
		const LRESULT r = m_searchCombo.DefWindowProc(msg, wp, lp);
		CClientDC dc(m_searchCombo);
		CRect rc; m_searchCombo.GetClientRect(rc);
		CBrush frame; frame.CreateSolidBrush(LineColor());
		dc.FrameRect(rc, frame);
		// the themed combo also draws an inner light ring; cover it so there is one line only
		CBrush fill; fill.CreateSolidBrush(m_callback->query_std_color(ui_color_background));
		CRect in = rc; in.DeflateRect(1, 1); dc.FrameRect(in, fill); in.DeflateRect(1, 1); dc.FrameRect(in, fill);
		return r;
	}

	void CFacetBrowser::FreeDeadPanes() {
		auto busy = [](const Pane& p) {
			return (p.list && p.list->m_hWnd != NULL) || (p.grid && p.grid->m_hWnd != NULL);
		};
		m_graveyard.erase(std::remove_if(m_graveyard.begin(), m_graveyard.end(), [&](const Pane& p) { return !busy(p); }), m_graveyard.end());
		if (m_graveyard.empty()) KillTimer(kFreePanesTimer);
	}

	// ================================================================ toolbar

	static const wchar_t* const kDrop = L" ▾";   // small down triangle

	void CFacetBrowser::UpdateToolbarText() {
		const char* src = m_cfg.source == Source::Playlist ? "Playlist" : m_cfg.source == Source::Automatic ? "Automatic" : "Library";
		m_btnSource.SetWindowText(pfc::stringcvt::string_wide_from_utf8(PFC_string_formatter() << src) + CString(kDrop));
		pfc::string8 filt = "Filter";
		if (m_cfg.filters.size() == 1) filt = m_cfg.filters[0];
		else if (m_cfg.filters.size() > 1) filt = PFC_string_formatter() << m_cfg.filters.size() << " filters";
		m_btnFilter.SetWindowText(pfc::stringcvt::string_wide_from_utf8(filt) + CString(kDrop));
		m_btnAction.SetWindowText(pfc::stringcvt::string_wide_from_utf8(actionName(m_cfg.action)));
		if (m_hWnd) Layout();
	}

	void CFacetBrowser::OnSourceButton(UINT, int, CWindow) {
		CRect r; m_btnSource.GetWindowRect(r);
		CMenu menu; menu.CreatePopupMenu();
		menu.AppendMenu(MF_STRING, 1, L"Library");
		menu.AppendMenu(MF_STRING, 2, L"Playlist");
		menu.AppendMenu(MF_STRING, 3, L"Automatic");
		menu.CheckMenuRadioItem(1, 3, 1 + (UINT)m_cfg.source, MF_BYCOMMAND);
		CMenuDescriptionMap desc(*this);
		desc.Set(1, "Activates \"Library\" source.");
		desc.Set(2, "Activates \"Playlist\" source.");
		desc.Set(3, "Automatically switches between library and playlist.");
		int cmd = menu.TrackPopupMenuEx(TPM_RETURNCMD | TPM_NONOTIFY | TPM_RIGHTBUTTON, r.left, m_cfg.toolbarBottom ? r.top : r.bottom, desc, nullptr);
		if (cmd <= 0) return;
		m_cfg.source = (Source)(cmd - 1);
		UpdateToolbarText();
		RebuildSource();
	}

	void CFacetBrowser::OnFilterButton(UINT, int, CWindow) {
		auto filters = getFilters();
		CRect r; m_btnFilter.GetWindowRect(r);
		CMenu menu; menu.CreatePopupMenu();
		CMenuDescriptionMap desc(*this);
		for (size_t i = 0; i < filters.size(); ++i) {
			bool on = std::find(m_cfg.filters.begin(), m_cfg.filters.end(), filters[i].name) != m_cfg.filters.end();
			menu.AppendMenu(MF_STRING | (on ? MF_CHECKED : 0), 100 + (UINT)i, pfc::stringcvt::string_wide_from_utf8(filters[i].name));
			desc.Set(100 + (UINT)i, PFC_string_formatter() << "Toggles \"" << filters[i].name << "\" filter with query: " << filters[i].query);
		}
		if (!filters.empty()) menu.AppendMenu(MF_SEPARATOR);
		menu.AppendMenu(MF_STRING | (m_cfg.multipleFilters ? MF_CHECKED : 0), 1, L"Multiple filters");
		desc.Set(1, "Toggles multiple filters.");
		menu.AppendMenu(MF_SEPARATOR);
		menu.AppendMenu(MF_STRING, 2, L"Preferences");
		desc.Set(2, "Opens filters preferences page.");
		int cmd = menu.TrackPopupMenuEx(TPM_RETURNCMD | TPM_NONOTIFY | TPM_RIGHTBUTTON, r.left, m_cfg.toolbarBottom ? r.top : r.bottom, desc, nullptr);
		if (cmd <= 0) return;
		if (cmd == 1) {
			m_cfg.multipleFilters = !m_cfg.multipleFilters;
			if (!m_cfg.multipleFilters && m_cfg.filters.size() > 1) m_cfg.filters.resize(1);
		} else if (cmd == 2) {
			showPreferences();
			return;
		} else if (cmd >= 100 && cmd - 100 < (int)filters.size()) {
			const auto& name = filters[cmd - 100].name;
			auto it = std::find(m_cfg.filters.begin(), m_cfg.filters.end(), name);
			if (it != m_cfg.filters.end()) m_cfg.filters.erase(it);
			else {
				if (!m_cfg.multipleFilters) m_cfg.filters.clear();
				m_cfg.filters.push_back(name);
			}
		}
		UpdateToolbarText();
		RebuildSource();
	}

	LRESULT CFacetBrowser::OnActionDropDown(LPNMHDR) {
		CRect r; m_btnAction.GetWindowRect(r);
		CMenu menu; menu.CreatePopupMenu();
		CMenuDescriptionMap desc(*this);
		for (uint32_t a = 0; a < (uint32_t)Action::Count; ++a) {
			menu.AppendMenu(MF_STRING, 1 + a, pfc::stringcvt::string_wide_from_utf8(actionName((Action)a)));
			desc.Set(1 + a, actionDescription((Action)a));
		}
		menu.CheckMenuRadioItem(1, (UINT)Action::Count, 1 + (UINT)m_cfg.action, MF_BYCOMMAND);
		int cmd = menu.TrackPopupMenuEx(TPM_RETURNCMD | TPM_NONOTIFY | TPM_RIGHTBUTTON | TPM_RIGHTALIGN, r.right, m_cfg.toolbarBottom ? r.top : r.bottom, desc, nullptr);
		if (cmd > 0) { m_cfg.action = (Action)(cmd - 1); UpdateToolbarText(); }
		return 0;
	}

	void CFacetBrowser::OnHelpButton(UINT, int, CWindow) {
		ShellExecute(m_hWnd, L"open", kHelpUrl, nullptr, nullptr, SW_SHOWNORMAL);
	}

	pfc::string8 CFacetBrowser::SearchText() const {
		if (!m_search.m_hWnd) return "";
		CString s; m_search.GetWindowText(s);
		return pfc::stringcvt::string_utf8_from_wide(s).get_ptr();
	}

	void CFacetBrowser::FillSearchHistory() {
		CString cur; m_search.GetWindowText(cur);
		m_search.ResetContent();
		auto h = getSearchHistory();
		for (auto& s : h) m_search.AddString(pfc::stringcvt::string_wide_from_utf8(s));
		if (!h.empty()) m_search.AddString(L"Wipe history");
		m_search.SetWindowText(cur);
	}

	void CFacetBrowser::OnSearchPick(UINT, int, CWindow) {
		int sel = m_search.GetCurSel();
		if (sel < 0) return;
		CString text; m_search.GetLBText(sel, text);
		if (sel == m_search.GetCount() - 1 && text == L"Wipe history") {
			wipeSearchHistory();
			PostMessage(WM_TIMER, 0, 0);   // no-op tick; refill after the combo closes
			m_search.SetWindowText(L"");
			FillSearchHistory();
			KillTimer(TIMER_SEARCH);
			RebuildSource();
			return;
		}
		m_search.SetWindowText(text);
		KillTimer(TIMER_SEARCH);
		RebuildSource();
	}

	void CFacetBrowser::OnSearchKeyDown(UINT ch, UINT, UINT) {
		if (ch == VK_ESCAPE) { m_search.SetWindowText(L""); KillTimer(TIMER_SEARCH); RebuildSource(); return; }   // reset search
		if (ch == VK_RETURN) {
			KillTimer(TIMER_SEARCH);
			addSearchHistory(SearchText());
			FillSearchHistory();
			RebuildSource();
			return;
		}
		SetMsgHandled(FALSE);
	}

	void CFacetBrowser::OnSearchChar(UINT ch, UINT, UINT) {
		if (ch == VK_RETURN || ch == VK_ESCAPE) return;
		SetMsgHandled(FALSE);
	}

	void CFacetBrowser::ShowToolbarMenu(CPoint pt) {
		CMenu menu; menu.CreatePopupMenu();
		menu.AppendMenu(MF_STRING | (m_cfg.showToolbar ? MF_CHECKED : 0), 1, L"Show toolbar");
		menu.AppendMenu(MF_STRING | (m_cfg.toolbarBottom ? MF_CHECKED : 0), 2, L"Bottom-dock toolbar");
		CMenuDescriptionMap desc(*this);
		desc.Set(1, "Shows or hides the toolbar.");
		desc.Set(2, "Toggles between top and bottom docking of the toolbar.");
		int cmd = menu.TrackPopupMenuEx(TPM_RETURNCMD | TPM_NONOTIFY | TPM_RIGHTBUTTON, pt.x, pt.y, desc, nullptr);
		if (cmd == 1) m_cfg.showToolbar = !m_cfg.showToolbar;
		else if (cmd == 2) m_cfg.toolbarBottom = !m_cfg.toolbarBottom;
		else return;
		const int show = m_cfg.showToolbar ? SW_SHOWNA : SW_HIDE;
		for (CWindow w : { (CWindow)m_btnSource, (CWindow)m_btnFilter, (CWindow)m_search, (CWindow)m_btnAction, (CWindow)m_btnHelp }) w.ShowWindow(show);
		Layout();
	}

	// ================================================================ data

	static bool isValidQuery(const char* q) {
		try { return search_filter_manager::get()->create(q).is_valid(); } catch (...) { return false; }
	}

	// The query a search box text stands for: the text itself when it is valid query syntax,
	// otherwise each word matched in the common fields. Empty if nothing usable.
	static pfc::string8 searchQuery(const char* text) {
		if (!text || !*text) return "";
		if (isValidQuery(text)) return text;
		pfc::string8 q, word;
		auto flush = [&] {
			if (word.is_empty()) return;
			pfc::string8 w;
			for (const char* c = word.c_str(); *c; ++c) if (*c != '"') w.add_byte(*c);
			if (!q.is_empty()) q << " AND ";
			q << "\"$meta_sep(artist,' ') $meta_sep(album artist,' ') %album% %title% $meta_sep(genre,' ') %path%\" HAS " << w;
			word.reset();
		};
		for (const char* c = text; *c; ++c) { if (*c == ' ') flush(); else word.add_byte(*c); }
		flush();
		return isValidQuery(q) ? q : pfc::string8();
	}

	static search_filter::ptr makeFilter(const char* text) {
		pfc::string8 q = searchQuery(text);
		if (q.is_empty()) return nullptr;
		try { return search_filter_manager::get()->create(q); } catch (...) {}
		return nullptr;
	}

	Source CFacetBrowser::EffectiveSource() const {
		if (m_cfg.source != Source::Automatic) return m_cfg.source;
		// Automatic: browse the active playlist, unless it is the selection playlist we fill ourselves
		auto pm = playlist_manager::get();
		size_t active = pm->get_active_playlist();
		if (active == pfc_infinite) return Source::Library;
		pfc::string8 name;
		pm->playlist_get_name(active, name);
		return pfc::stringEqualsI_utf8(name, getSelectionPlaylist()) ? Source::Library : Source::Playlist;
	}

	void CFacetBrowser::RebuildSource() {
		metadb_handle_list tracks;
		if (EffectiveSource() == Source::Playlist) playlist_manager::get()->activeplaylist_get_all_items(tracks);
		else library_manager::get()->get_all_items(tracks);

		auto applyFilter = [&](const char* query) {
			if (!query || !*query || tracks.get_count() == 0) return;
			auto f = makeFilter(query);
			if (f.is_empty()) return;
			pfc::array_t<bool> mask; mask.set_size(tracks.get_count());
			f->test_multi(tracks, mask.get_ptr());
			tracks.remove_mask(pfc::bit_array_not(pfc::bit_array_table(mask.get_ptr(), mask.get_size())));
		};
		auto defs = getFilters();
		for (auto& active : m_cfg.filters)
			for (auto& d : defs) if (d.name == active) applyFilter(d.query);
		applyFilter(SearchText());

		m_error.reset();
		std::vector<PaneSpec> specs;
		for (auto& p : m_panes) specs.push_back(p.spec);
		bool wantCreated = false;
		for (auto& pc : m_cfg.panes) wantCreated |= paneUsesStat(pc, Stat::FileCreated);
		if (!m_model.build(tracks, specs, wantCreated, m_error)) {
			for (size_t i = 0; i < m_panes.size(); ++i) { m_panes[i].items.clear(); m_panes[i].input.clear(); m_panes[i].output.clear(); RefreshPaneUI(i); }
			Invalidate();
			return;
		}
		if (EffectiveSource() == Source::Library && !library_manager::get()->is_library_enabled())
			m_error = "The Media Library is not configured. Add your music folders in File > Preferences > Media Library.";
		Cascade(0);
	}

	void CFacetBrowser::Cascade(size_t from, bool regroupFirst) {
		for (size_t i = from; i < m_panes.size(); ++i) {
			auto& p = m_panes[i];
			const auto& pc = m_cfg.panes[i];
			const bool regroup = !(i == from && !regroupFirst);
			if (regroup) {
				p.input = (i == 0) ? m_model.allTracks() : m_panes[i - 1].output;
				p.items = m_model.group(i, p.input);
				p.total = m_model.totals(i, p.input);
			}
			std::vector<uint32_t> ids;
			std::vector<pfc::string8> keep;
			for (auto& key : p.selected) {
				uint32_t id;
				if (!m_model.findValue(i, key, id)) continue;
				for (auto& it : p.items) if (it.valueId == id) { ids.push_back(id); keep.push_back(key); break; }
			}
			p.selected = keep;
			p.output = ids.empty() ? p.input : m_model.filter(i, p.input, ids, pc.andLogic);
			if (regroup) {
				const bool wantSub = paneUsesStat(pc, Stat::Subitems);
				if (wantSub) m_model.computeSubitems(i, p.input, p.items);
				m_model.sortItems(i, p.items, pc.sortColumn, pc.stats, pc.sortDescending, pc.sortStat);
				if (wantSub) { p.total.subitems = 0; }
				RefreshPaneUI(i);
			}
		}
		// subitems of a pane depend on the next pane's values, which only change with regrouping above
	}

	size_t CFacetBrowser::RowCount(size_t pane) const {
		if (pane >= m_panes.size()) return 0;
		return m_panes[pane].items.size() + (m_cfg.panes[pane].summary ? 1 : 0);
	}

	bool CFacetBrowser::IsSummaryRow(size_t pane, size_t row) const {
		return pane < m_panes.size() && m_cfg.panes[pane].summary && row == 0;
	}

	const FacetItem* CFacetBrowser::RowItem(size_t pane, size_t row) const {
		if (pane >= m_panes.size()) return nullptr;
		const auto& p = m_panes[pane];
		if (m_cfg.panes[pane].summary) { if (row == 0) return nullptr; row--; }
		return row < p.items.size() ? &p.items[row] : nullptr;
	}

	void CFacetBrowser::RefreshPaneUI(size_t i) {
		auto& p = m_panes[i];
		auto toggle = pfc::autoToggle(m_updating, true);
		const size_t count = RowCount(i);
		const size_t base = m_cfg.panes[i].summary ? 1 : 0;
		std::vector<size_t> rows;
		if (p.selected.empty()) { if (base) rows.push_back(0); }
		else {
			for (size_t r = 0; r < p.items.size(); ++r) {
				const auto& key = m_model.valueKey(i, p.items[r].valueId);
				for (auto& s : p.selected) if (s == key) { rows.push_back(r + base); break; }
			}
		}
		if (p.list) {
			p.list->ReloadData();
			FitKeyColumns(i);
			pfc::bit_array_bittable mask(count);
			for (size_t r : rows) mask.set(r, true);
			p.list->SetSelection(pfc::bit_array_true(), mask);
			if (!rows.empty()) p.list->SetFocusItem(rows.front());
		}
		if (p.grid) {
			p.grid->Reload();
			p.grid->SetSelection(rows);
		}
	}

	void CFacetBrowser::ReadSelectionFromUI(size_t i) {
		auto& p = m_panes[i];
		const size_t base = m_cfg.panes[i].summary ? 1 : 0;
		std::vector<size_t> rows;
		if (p.list) {
			const size_t count = p.list->GetItemCount();
			for (size_t r = 0; r < count; ++r) if (p.list->IsItemSelected(r)) rows.push_back(r);
		} else if (p.grid) rows = p.grid->GetSelection();
		const bool hadAll = p.selected.empty();
		const bool allSelected = base && std::find(rows.begin(), rows.end(), (size_t)0) != rows.end();
		std::vector<pfc::string8> keys;
		for (size_t r : rows) if (r >= base && r - base < p.items.size()) keys.push_back(m_model.valueKey(i, p.items[r - base].valueId));
		// "All" and specific items are mutually exclusive; whichever was picked last wins
		if (allSelected && (!hadAll || keys.empty())) keys.clear();
		p.selected = keys;
		if (allSelected && rows.size() > 1) {
			auto toggle = pfc::autoToggle(m_updating, true);
			std::vector<size_t> want;
			if (keys.empty()) want.push_back(0);
			else for (size_t r = 0; r < p.items.size(); ++r) {
				const auto& k = m_model.valueKey(i, p.items[r].valueId);
				for (auto& s : keys) if (s == k) { want.push_back(r + base); break; }
			}
			if (p.list) {
				pfc::bit_array_bittable mask(RowCount(i));
				for (size_t r : want) mask.set(r, true);
				p.list->SetSelection(pfc::bit_array_true(), mask);
			}
			if (p.grid) p.grid->SetSelection(want);
		}
	}

	void CFacetBrowser::OnPaneSelectionChanged(size_t i) {
		if (m_updating || i >= m_panes.size()) return;
		m_lastPane = i;
		ReadSelectionFromUI(i);
		Cascade(i, false);
		if (i + 1 < m_panes.size()) Cascade(i + 1);
	}

	void CFacetBrowser::OnHeaderClick(size_t pane, size_t col) {
		if (pane >= m_panes.size()) return;
		auto& pc = m_cfg.panes[pane];
		if (sortIndicatorColumn(pc, m_panes[pane].spec.columns.size()) == (int)col) pc.sortDescending = !pc.sortDescending;
		else pc.sortDescending = false;
		pc.sortColumn = (int32_t)col;
		pc.sortStat = -1;
		m_model.sortItems(pane, m_panes[pane].items, pc.sortColumn, pc.stats, pc.sortDescending, pc.sortStat);
		UpdateColumns(pane);
		Layout();
		RefreshPaneUI(pane);
	}

	metadb_handle_list CFacetBrowser::PaneTracks(size_t i) const {
		if (i >= m_panes.size()) return metadb_handle_list();
		return m_model.handles(m_panes[i].output);
	}

	size_t CFacetBrowser::PaneIndex(const void* ctx) const {
		for (size_t i = 0; i < m_panes.size(); ++i) if (m_panes[i].list.get() == ctx) return i;
		return SIZE_MAX;
	}

	size_t CFacetBrowser::PaneIndexFromWnd(HWND wnd) const {
		for (size_t i = 0; i < m_panes.size(); ++i) {
			HWND w = m_panes[i].wnd();
			if (w && (w == wnd || ::IsChild(w, wnd))) return i;
		}
		return SIZE_MAX;
	}

	// ================================================================ cells

	pfc::string8 CFacetBrowser::SummaryLabel(size_t i) const {
		const auto& p = m_panes[i];
		pfc::string8 noun = p.spec.columns.empty() ? pfc::string8("item") : p.spec.columns[0].name;
		pfc::string8 lower;
		for (const char* c = noun.c_str(); *c; ++c) lower.add_byte((char)tolower((unsigned char)*c));
		return PFC_string_formatter() << "All (" << p.items.size() << " " << lower << (p.items.size() == 1 ? "" : "s") << ")";
	}

	pfc::string8 CFacetBrowser::CellText(size_t i, size_t row, size_t col) const {
		const auto& p = m_panes[i];
		const auto& pc = m_cfg.panes[i];
		const size_t keys = p.spec.columns.size();
		const bool summary = IsSummaryRow(i, row);
		const FacetItem* it = summary ? &p.total : RowItem(i, row);
		if (!it) return "";
		if (col < keys) {
			if (summary) return col == 0 ? SummaryLabel(i) : pfc::string8();
			const auto& parts = m_model.valueParts(i, it->valueId);
			return col < parts.size() ? parts[col] : pfc::string8();
		}
		size_t s = col - keys;
		if (s >= pc.stats.size()) return "";
		const Stat st = (Stat)pc.stats[s];
		if (summary && (st == Stat::Index || st == Stat::Subitems)) return "";
		const size_t index = summary ? 0 : row + (pc.summary ? 0 : 1);
		return m_model.statText(st, *it, index);
	}

	std::vector<pfc::string8> CFacetBrowser::ArtLines(size_t pane, size_t row) {
		std::vector<pfc::string8> lines;
		if (pane >= m_panes.size()) return lines;
		const size_t total = m_panes[pane].spec.columns.size() + m_cfg.panes[pane].stats.size();
		for (size_t c = 0; c < total; ++c) lines.push_back(CellText(pane, row, c));
		return lines;
	}

	metadb_handle_ptr CFacetBrowser::ArtTrack(size_t pane, size_t row) {
		const FacetItem* it = RowItem(pane, row);
		if (!it) return nullptr;
		return m_model.track(it->firstTrack);
	}

	// ================================================================ list source

	size_t CFacetBrowser::listGetItemCount(ctx_t ctx) { return RowCount(PaneIndex(ctx)); }

	pfc::string8 CFacetBrowser::listGetSubItemText(ctx_t ctx, size_t item, size_t sub) {
		size_t i = PaneIndex(ctx);
		if (i >= m_panes.size()) return "";
		return CellText(i, item, sub);
	}

	void CFacetBrowser::listSelChanged(ctx_t ctx) { OnPaneSelectionChanged(PaneIndex(ctx)); }

	void CFacetBrowser::listItemAction(ctx_t ctx, size_t) {
		size_t i = PaneIndex(ctx);
		if (i < m_panes.size()) RunToolbarAction(i);
	}

	void CFacetBrowser::listColumnHeaderClick(ctx_t ctx, size_t sub) { OnHeaderClick(PaneIndex(ctx), sub); }

	pfc::com_ptr_t<IDataObject> CFacetBrowser::listMakeDataObject(ctx_t ctx) {
		size_t i = PaneIndex(ctx);
		if (i >= m_panes.size()) return nullptr;
		auto tracks = PaneTracks(i);
		SortTracks(tracks);
		return playlist_incoming_item_filter::get()->create_dataobject_ex(tracks);
	}

	// ================================================================ menus

	enum {
		CMD_ACTION = 1,                 // + Action
		CMD_LOGIC_OR = 20, CMD_LOGIC_AND,
		CMD_PANE_INSERT = 30, CMD_PANE_APPEND, CMD_PANE_REMOVE, CMD_PANE_CUT, CMD_PANE_COPY, CMD_PANE_PASTE,
		CMD_SHOW_TOOLBAR = 40, CMD_TOOLBAR_BOTTOM,
		CMD_COL_MULTI = 50, CMD_COL_PREFS,
		CMD_DISPLAY = 60,               // + Display
		CMD_SUMMARY = 70,
		CMD_ART = 80,                   // + ArtType
		CMD_COLUMN = 100,               // + column index (Folder last)
		CMD_STAT = 300,                 // + Stat
		CMD_SORT_COL = 400,             // + key column index
		CMD_SORT_STAT = 450,            // + Stat
		CMD_SORT_ASC = 500, CMD_SORT_DESC,
		CMD_CONTEXT = 1000,
	};

	void CFacetBrowser::BuildHeaderMenu(size_t i, CMenu& menu, CMenuDescriptionHybrid& desc) {
		const auto& pc = m_cfg.panes[i];
		auto has = [&](const char* name) { return std::find(pc.columns.begin(), pc.columns.end(), pfc::string8(name)) != pc.columns.end(); };
		auto cols = getColumns();

		CMenu mCols; mCols.CreatePopupMenu();
		for (size_t c = 0; c <= cols.size(); ++c) {
			const char* name = c < cols.size() ? cols[c].name.c_str() : kFolderColumn;
			mCols.AppendMenu(MF_STRING, CMD_COLUMN + (UINT)c, pfc::stringcvt::string_wide_from_utf8(name));
			if (has(name)) {
				if (pc.columns.size() > 1 || !pc.columns.empty()) {
					if (pc.columns.size() > 1) mCols.CheckMenuItem(CMD_COLUMN + (UINT)c, MF_CHECKED);
					else mCols.CheckMenuRadioItem(CMD_COLUMN + (UINT)c, CMD_COLUMN + (UINT)c, CMD_COLUMN + (UINT)c, MF_BYCOMMAND);
				}
			}
			desc.Set(CMD_COLUMN + (UINT)c, PFC_string_formatter() << "Toggles \"" << name << "\" column.");
		}
		mCols.AppendMenu(MF_SEPARATOR);
		mCols.AppendMenu(MF_STRING | (pc.columns.size() > 1 ? MF_CHECKED : 0), CMD_COL_MULTI, L"Multiple columns");
		desc.Set(CMD_COL_MULTI, "Toggles multiple columns.");
		mCols.AppendMenu(MF_SEPARATOR);
		mCols.AppendMenu(MF_STRING, CMD_COL_PREFS, L"Preferences");
		desc.Set(CMD_COL_PREFS, "Displays column preferences.");

		CMenu mStats; mStats.CreatePopupMenu();
		for (uint32_t s : statMenuOrder()) {
			bool on = std::find(pc.stats.begin(), pc.stats.end(), s) != pc.stats.end();
			mStats.AppendMenu(MF_STRING | (on ? MF_CHECKED : 0), CMD_STAT + s, pfc::stringcvt::string_wide_from_utf8(statName((Stat)s)));
			desc.Set(CMD_STAT + s, statDescription((Stat)s));
		}

		// Sort by: any key column or any statistic, displayed or not
		CMenu mSort; mSort.CreatePopupMenu();
		{
			const size_t keys = pc.columns.size();
			UINT current = 0;
			for (size_t c = 0; c < keys; ++c) {
				mSort.AppendMenu(MF_STRING, CMD_SORT_COL + (UINT)c, pfc::stringcvt::string_wide_from_utf8(pc.columns[c]));
				desc.Set(CMD_SORT_COL + (UINT)c, PFC_string_formatter() << "Sorts this pane by \"" << pc.columns[c] << "\".");
			}
			if (pc.sortStat >= 0) current = CMD_SORT_STAT + (UINT)pc.sortStat;
			else if (pc.sortColumn >= 0 && pc.sortColumn < (int)keys) current = CMD_SORT_COL + (UINT)pc.sortColumn;
			else if (pc.sortColumn >= (int)keys && pc.sortColumn - keys < pc.stats.size()) current = CMD_SORT_STAT + pc.stats[pc.sortColumn - keys];
			mSort.AppendMenu(MF_SEPARATOR);
			for (uint32_t s : statMenuOrder()) {
				if (s == (uint32_t)Stat::Index) continue;
				mSort.AppendMenu(MF_STRING, CMD_SORT_STAT + s, pfc::stringcvt::string_wide_from_utf8(statName((Stat)s)));
				desc.Set(CMD_SORT_STAT + s, PFC_string_formatter() << "Sorts this pane by the \"" << statName((Stat)s) << "\" statistic, even when it is not displayed.");
			}
			if (current) mSort.CheckMenuRadioItem(current, current, current, MF_BYCOMMAND);
			mSort.AppendMenu(MF_SEPARATOR);
			mSort.AppendMenu(MF_STRING, CMD_SORT_ASC, L"Ascending");
			mSort.AppendMenu(MF_STRING, CMD_SORT_DESC, L"Descending");
			mSort.CheckMenuRadioItem(CMD_SORT_ASC, CMD_SORT_DESC, pc.sortDescending ? CMD_SORT_DESC : CMD_SORT_ASC, MF_BYCOMMAND);
			desc.Set(CMD_SORT_ASC, "Sorts from lowest to highest (oldest first for dates).");
			desc.Set(CMD_SORT_DESC, "Sorts from highest to lowest (newest first for dates).");
		}

		CMenu mDisplay; mDisplay.CreatePopupMenu();
		const wchar_t* displays[] = { L"Columns", L"Album art (bottom labels)", L"Album art (right labels)", L"Album art (no labels)" };
		for (UINT d = 0; d < 4; ++d) mDisplay.AppendMenu(MF_STRING, CMD_DISPLAY + d, displays[d]);
		mDisplay.CheckMenuRadioItem(CMD_DISPLAY, CMD_DISPLAY + 3, CMD_DISPLAY + (UINT)pc.display, MF_BYCOMMAND);
		mDisplay.AppendMenu(MF_SEPARATOR);
		mDisplay.AppendMenu(MF_STRING | (pc.summary ? MF_CHECKED : 0), CMD_SUMMARY, L"Summary item");
		desc.Set(CMD_SUMMARY, "Toggles the summary item.");
		mDisplay.AppendMenu(MF_SEPARATOR);
		const wchar_t* arts[] = { L"Front", L"Back", L"Disc", L"Artist" };
		const UINT artFlags = pc.display == Display::Columns ? MF_GRAYED : 0;
		for (UINT a = 0; a < 4; ++a) mDisplay.AppendMenu(MF_STRING | artFlags, CMD_ART + a, arts[a]);
		mDisplay.CheckMenuRadioItem(CMD_ART, CMD_ART + 3, CMD_ART + (UINT)pc.artType, MF_BYCOMMAND);

		menu.AppendMenu(MF_POPUP, (UINT_PTR)mCols.Detach(), L"Columns");
		menu.AppendMenu(MF_POPUP, (UINT_PTR)mStats.Detach(), L"Statistics");
		menu.AppendMenu(MF_POPUP, (UINT_PTR)mSort.Detach(), L"Sort by");
		menu.AppendMenu(MF_POPUP, (UINT_PTR)mDisplay.Detach(), L"Display");
	}

	bool CFacetBrowser::RunHeaderCommand(size_t i, int cmd) {
		auto& pc = m_cfg.panes[i];
		auto cols = getColumns();
		// The sort column indexes key columns followed by statistics, so after the key columns
		// change, point it at the same column (by name) or the same statistic again.
		const std::vector<pfc::string8> oldColumns = pc.columns;
		auto keepSort = [&] {
			if (pc.sortStat >= 0) return;
			const int oldKeys = (int)oldColumns.size();
			if (pc.sortColumn >= oldKeys) {
				const size_t k = (size_t)(pc.sortColumn - oldKeys);
				pc.sortColumn = k < pc.stats.size() ? (int32_t)(pc.columns.size() + k) : 0;
			} else if (pc.sortColumn >= 0) {
				auto it = std::find(pc.columns.begin(), pc.columns.end(), oldColumns[pc.sortColumn]);
				pc.sortColumn = it != pc.columns.end() ? (int32_t)(it - pc.columns.begin()) : 0;
			} else pc.sortColumn = 0;
		};
		if (cmd >= CMD_COLUMN && cmd <= CMD_COLUMN + (int)cols.size()) {
			size_t c = (size_t)(cmd - CMD_COLUMN);
			pfc::string8 name = c < cols.size() ? cols[c].name : pfc::string8(kFolderColumn);
			auto it = std::find(pc.columns.begin(), pc.columns.end(), name);
			if (pc.columns.size() > 1) {   // multiple columns: toggle
				if (it != pc.columns.end()) pc.columns.erase(it); else pc.columns.push_back(name);
			} else {
				pc.columns.assign(1, name);
			}
			keepSort();
		} else if (cmd == CMD_COL_MULTI) {
			if (pc.columns.size() > 1) pc.columns.resize(1);
			else {
				// switch on multiple columns by adding the next column after the current one
				pfc::string8 next = kFolderColumn;
				for (size_t c = 0; c < cols.size(); ++c) if (cols[c].name == pc.columns[0] && c + 1 < cols.size()) next = cols[c + 1].name;
				if (next == pc.columns[0]) next = cols.empty() ? pfc::string8(kFolderColumn) : cols[0].name;
				pc.columns.push_back(next);
			}
			keepSort();
		} else if (cmd == CMD_COL_PREFS) {
			showPreferences();
			return false;
		} else if (cmd >= CMD_STAT && cmd < CMD_STAT + (int)Stat::Count) {
			uint32_t s = (uint32_t)(cmd - CMD_STAT);
			auto it = std::find(pc.stats.begin(), pc.stats.end(), s);
			if (it != pc.stats.end()) {
				int removed = (int)(pc.columns.size() + (it - pc.stats.begin()));
				pc.stats.erase(it);
				if (pc.sortColumn == removed && pc.sortStat < 0) { pc.sortStat = (int32_t)s; pc.sortColumn = 0; }   // keep the order; still listed under Sort by
				else if (pc.sortColumn > removed) pc.sortColumn--;
			} else pc.stats.push_back(s);
		} else if (cmd >= CMD_SORT_COL && cmd < CMD_SORT_COL + (int)pc.columns.size()) {
			pc.sortColumn = cmd - CMD_SORT_COL;
			pc.sortStat = -1;
		} else if (cmd >= CMD_SORT_STAT && cmd < CMD_SORT_STAT + (int)Stat::Count) {
			const uint32_t s = (uint32_t)(cmd - CMD_SORT_STAT);
			auto it = std::find(pc.stats.begin(), pc.stats.end(), s);
			if (it != pc.stats.end()) { pc.sortColumn = (int32_t)(pc.columns.size() + (it - pc.stats.begin())); pc.sortStat = -1; }
			else pc.sortStat = (int32_t)s;
		} else if (cmd == CMD_SORT_ASC || cmd == CMD_SORT_DESC) {
			pc.sortDescending = (cmd == CMD_SORT_DESC);
		} else if (cmd >= CMD_DISPLAY && cmd < CMD_DISPLAY + 4) {
			pc.display = (Display)(cmd - CMD_DISPLAY);
		} else if (cmd == CMD_SUMMARY) {
			pc.summary = !pc.summary;
		} else if (cmd >= CMD_ART && cmd < CMD_ART + 4) {
			pc.artType = (ArtType)(cmd - CMD_ART);
		} else return false;
		return true;
	}

	void CFacetBrowser::OnContextMenu(CWindow wnd, CPoint pt) {
		if (m_callback->is_edit_mode_enabled()) { SetMsgHandled(FALSE); return; }
		// toolbar area
		if (wnd == m_hWnd || wnd == m_btnSource || wnd == m_btnFilter || wnd == m_btnAction || wnd == m_btnHelp) {
			if (pt.x == -1 && pt.y == -1) { pt = m_toolbarRect.TopLeft(); ClientToScreen(&pt); }
			CPoint local = pt; ScreenToClient(&local);
			if (wnd != m_hWnd || m_toolbarRect.PtInRect(local)) { ShowToolbarMenu(pt); return; }
		}
		size_t i = PaneIndexFromWnd(wnd);
		if (i == SIZE_MAX) {
			if (wnd == m_hWnd && !m_panes.empty()) {
				// empty area: fall back to the toolbar menu so it can always be shown again
				ShowToolbarMenu(pt);
			} else SetMsgHandled(FALSE);
			return;
		}
		bool onHeader = false;
		if (m_panes[i].grid) {
			CHeaderCtrl hdr = m_panes[i].grid->GetHeader();
			onHeader = hdr.m_hWnd && (wnd == hdr.m_hWnd || ::IsChild(hdr, wnd));
		}
		if (m_panes[i].list) {
			CHeaderCtrl hdr = m_panes[i].list->GetHeaderCtrl();
			onHeader = hdr.m_hWnd && (wnd == hdr.m_hWnd || ::IsChild(hdr, wnd));
			// A right-click on the header reaches us via the list's DefWindowProc, which
			// passes the list as the source window, so also go by where the mouse is.
			if (!onHeader && hdr.m_hWnd && hdr.IsWindowVisible() && !(pt.x == -1 && pt.y == -1)) {
				CRect hr; hdr.GetWindowRect(hr);
				onHeader = hr.PtInRect(pt) != FALSE;
			}
			if (!onHeader) pt = m_panes[i].list->GetContextMenuPoint(pt);
		}
		ShowPaneMenu(i, wnd, pt, onHeader);
	}

	void CFacetBrowser::ShowPaneMenu(size_t i, CWindow wnd, CPoint pt, bool onHeader) {
		if (i >= m_panes.size()) return;
		m_lastPane = i;
		CMenu menu; menu.CreatePopupMenu();
		CMenuDescriptionHybrid desc(*this);
		contextmenu_manager::ptr cm;
		metadb_handle_list tracks;

		if (onHeader) {
			BuildHeaderMenu(i, menu, desc);
		} else {
			tracks = PaneTracks(i);
			SortTracks(tracks);
			const UINT dis = tracks.get_count() ? 0 : MF_GRAYED;
			for (uint32_t a = 0; a < (uint32_t)Action::Count; ++a) {
				menu.AppendMenu(MF_STRING | dis, CMD_ACTION + a, pfc::stringcvt::string_wide_from_utf8(actionName((Action)a)));
				desc.Set(CMD_ACTION + a, actionDescription((Action)a));
			}
			menu.SetMenuDefaultItem(CMD_ACTION + (UINT)m_cfg.action, FALSE);
			if (tracks.get_count()) {
				menu.AppendMenu(MF_SEPARATOR);
				cm = contextmenu_manager::g_create();
				cm->init_context(tracks, 0);
				cm->win32_build_menu(menu, CMD_CONTEXT, 0x7FFF - CMD_CONTEXT);
			}
			menu.AppendMenu(MF_SEPARATOR);
			CMenu mLogic; mLogic.CreatePopupMenu();
			mLogic.AppendMenu(MF_STRING, CMD_LOGIC_OR, L"OR (default)");
			mLogic.AppendMenu(MF_STRING, CMD_LOGIC_AND, L"AND");
			mLogic.CheckMenuRadioItem(CMD_LOGIC_OR, CMD_LOGIC_AND, m_cfg.panes[i].andLogic ? CMD_LOGIC_AND : CMD_LOGIC_OR, MF_BYCOMMAND);
			desc.Set(CMD_LOGIC_OR, "Toggles OR selection logic.");
			desc.Set(CMD_LOGIC_AND, "Toggles AND selection logic.");
			menu.AppendMenu(MF_POPUP, (UINT_PTR)mLogic.Detach(), L"Selection logic");
			CMenu mPane; mPane.CreatePopupMenu();
			mPane.AppendMenu(MF_STRING, CMD_PANE_INSERT, L"Insert new pane");
			mPane.AppendMenu(MF_STRING, CMD_PANE_APPEND, L"Append new pane");
			mPane.AppendMenu(MF_STRING | (m_panes.size() > 1 ? 0 : MF_GRAYED), CMD_PANE_REMOVE, L"Remove pane");
			mPane.AppendMenu(MF_SEPARATOR);
			mPane.AppendMenu(MF_STRING | (m_panes.size() > 1 ? 0 : MF_GRAYED), CMD_PANE_CUT, L"Cut pane\tCtrl+X");
			mPane.AppendMenu(MF_STRING, CMD_PANE_COPY, L"Copy pane\tCtrl+C");
			mPane.AppendMenu(MF_STRING, CMD_PANE_PASTE, L"Paste pane\tCtrl+V");
			desc.Set(CMD_PANE_INSERT, "Inserts a new pane before the selected one.");
			desc.Set(CMD_PANE_APPEND, "Inserts a new pane after the selected one.");
			desc.Set(CMD_PANE_REMOVE, "Removes the selected pane");
			desc.Set(CMD_PANE_CUT, "Copies this pane to Windows Clipboard and removes it.");
			desc.Set(CMD_PANE_COPY, "Copies the selected pane to Windows Clipboard.");
			desc.Set(CMD_PANE_PASTE, "Inserts pane from Windows Clipboard before this one. Hold Shift to insert after it.");
			menu.AppendMenu(MF_POPUP, (UINT_PTR)mPane.Detach(), L"Pane");
			BuildHeaderMenu(i, menu, desc);
			menu.AppendMenu(MF_SEPARATOR);
			menu.AppendMenu(MF_STRING | (m_cfg.showToolbar ? MF_CHECKED : 0), CMD_SHOW_TOOLBAR, L"Show toolbar");
			desc.Set(CMD_SHOW_TOOLBAR, "Shows or hides the toolbar.");
		}

		if (cm.is_valid()) desc.SetCM(cm, CMD_CONTEXT, ~0u);
		int cmd = menu.TrackPopupMenuEx(TPM_RIGHTBUTTON | TPM_NONOTIFY | TPM_RETURNCMD, pt.x, pt.y, desc, nullptr);
		if (cmd <= 0) return;
		if (cmd >= CMD_CONTEXT) { if (cm.is_valid()) cm->execute_by_id(cmd - CMD_CONTEXT); return; }
		if (cmd >= CMD_ACTION && cmd < CMD_ACTION + (int)Action::Count) { RunAction((Action)(cmd - CMD_ACTION), tracks, i); return; }
		switch (cmd) {
		case CMD_LOGIC_OR: case CMD_LOGIC_AND:
			m_cfg.panes[i].andLogic = (cmd == CMD_LOGIC_AND);
			Cascade(i, false); if (i + 1 < m_panes.size()) Cascade(i + 1);
			return;
		case CMD_PANE_INSERT: InsertPane(i, BrowserConfig::makePane("Genre")); return;
		case CMD_PANE_APPEND: InsertPane(i + 1, BrowserConfig::makePane("Genre")); return;
		case CMD_PANE_REMOVE: RemovePane(i); return;
		case CMD_PANE_CUT: CopyPane(i); RemovePane(i); return;
		case CMD_PANE_COPY: CopyPane(i); return;
		case CMD_PANE_PASTE: PastePane(GetKeyState(VK_SHIFT) < 0 ? i + 1 : i); return;
		case CMD_SHOW_TOOLBAR:
			m_cfg.showToolbar = !m_cfg.showToolbar;
			for (CWindow w : { (CWindow)m_btnSource, (CWindow)m_btnFilter, (CWindow)m_search, (CWindow)m_btnAction, (CWindow)m_btnHelp }) w.ShowWindow(m_cfg.showToolbar ? SW_SHOWNA : SW_HIDE);
			Layout();
			return;
		}
		if (RunHeaderCommand(i, cmd)) CreatePanesKeepSelection();
	}

	// ================================================================ panes

	void CFacetBrowser::InsertPane(size_t at, const PaneConfig& p) {
		at = std::min(at, m_cfg.panes.size());
		m_cfg.panes.insert(m_cfg.panes.begin() + at, p);
		CreatePanesKeepSelection();
	}

	void CFacetBrowser::RemovePane(size_t i) {
		if (m_cfg.panes.size() <= 1 || i >= m_cfg.panes.size()) return;
		m_cfg.panes.erase(m_cfg.panes.begin() + i);
		if (m_lastPane >= m_cfg.panes.size()) m_lastPane = m_cfg.panes.size() - 1;
		CreatePanesKeepSelection();
	}

	bool CFacetBrowser::PaneKeyDown(size_t pane, UINT ch) {
		if (pane >= m_panes.size() || GetKeyState(VK_CONTROL) >= 0 || GetKeyState(VK_MENU) < 0) return false;
		int cmd = 0;
		switch (ch) {
		case 'X': cmd = m_panes.size() > 1 ? CMD_PANE_CUT : -1; break;
		case 'C': cmd = CMD_PANE_COPY; break;
		case 'V': cmd = CMD_PANE_PASTE; break;
		default: return false;
		}
		if (cmd > 0) PostMessage(WM_FB_PANE_COMMAND, (WPARAM)cmd, (LPARAM)(pane + (GetKeyState(VK_SHIFT) < 0 ? 1 : 0) * 0x10000));
		return true;
	}

	LRESULT CFacetBrowser::OnPaneCommand(UINT, WPARAM cmd, LPARAM lp) {
		const size_t i = (size_t)(lp & 0xFFFF);
		const bool shift = (lp & 0x10000) != 0;
		if (i >= m_panes.size()) return 0;
		m_lastPane = i;
		switch ((int)cmd) {
		case CMD_PANE_CUT: CopyPane(i); RemovePane(i); break;
		case CMD_PANE_COPY: CopyPane(i); break;
		case CMD_PANE_PASTE: PastePane(shift ? i + 1 : i); break;
		}
		return 0;
	}

	static UINT paneClipboardFormat() {
		static UINT fmt = RegisterClipboardFormat(L"FacetBrowserPane");
		return fmt;
	}

	void CFacetBrowser::CopyPane(size_t i) {
		if (i >= m_cfg.panes.size()) return;
		ui_element_config_builder out;
		BrowserConfig::writePane(out, m_cfg.panes[i]);
		auto data = out.finish(guid_element);
		if (!OpenClipboard()) return;
		EmptyClipboard();
		HGLOBAL mem = GlobalAlloc(GMEM_MOVEABLE, data->get_data_size());
		if (mem) {
			memcpy(GlobalLock(mem), data->get_data(), data->get_data_size());
			GlobalUnlock(mem);
			SetClipboardData(paneClipboardFormat(), mem);
		}
		CloseClipboard();
	}

	bool CFacetBrowser::PastePane(size_t at) {
		if (!OpenClipboard()) return false;
		bool ok = false;
		HANDLE h = GetClipboardData(paneClipboardFormat());
		if (h) {
			const void* p = GlobalLock(h);
			size_t size = GlobalSize(h);
			try {
				auto cfg = ui_element_config::g_create(guid_element, p, size);
				ui_element_config_parser in(cfg);
				PaneConfig pane = BrowserConfig::readPane(in, 3);   // clipboard data is always written in the current format
				GlobalUnlock(h);
				CloseClipboard();
				InsertPane(at, pane);
				return true;
			} catch (...) {}
			GlobalUnlock(h);
		}
		CloseClipboard();
		return ok;
	}

	// ================================================================ actions

	void CFacetBrowser::SortTracks(metadb_handle_list& tracks) {
		pfc::string8 pattern = getSortPattern();
		if (pattern.is_empty()) return;
		titleformat_object::ptr script;
		if (titleformat_compiler::get()->compile(script, pattern)) tracks.sort_by_format(script, nullptr);
	}

	pfc::string8 CFacetBrowser::SelectionLabel(size_t i) const {
		const auto& p = m_panes[i];
		if (p.selected.empty()) return PFC_string_formatter() << "All " << (p.spec.columns.empty() ? "" : p.spec.columns[0].name.c_str());
		pfc::string8 s;
		for (size_t k = 0; k < p.selected.size() && k < 3; ++k) {
			if (k) s << ", ";
			pfc::string8 v = p.selected[k]; v.replace_char('\x02', ' ');
			s << v;
		}
		if (p.selected.size() > 3) s << ", ...";
		return s;
	}

	// The value a column pattern gives when its multi-value field is missing (e.g. "(Unknown)" for
	// $if2(%<genre>%,'('Unknown')')), so that item can be queried as "<field> MISSING".
	static pfc::string8 missingValue(const char* pattern) {
		titleformat_object::ptr script;
		if (!titleformat_compiler::get()->compile(script, expandMultiValue(pattern))) return "";
		file_info_impl info;
		pfc::string8 out;
		script->run_simple(make_playable_location("", 0), &info, out);
		return out;
	}

	// "<what> IS <value>", or for a value containing double quotes (which the query syntax cannot
	// quote) one HAS per quote-free piece.
	static pfc::string8 isCondition(const pfc::string8& what, const pfc::string8& value) {
		if (!strchr(value.c_str(), '"')) return PFC_string_formatter() << what << " IS \"" << value << "\"";
		pfc::string8 out, piece;
		auto flush = [&] {
			if (piece.is_empty()) return;
			if (!out.is_empty()) out << " AND ";
			out << what << " HAS \"" << piece << "\"";
			piece.reset();
		};
		for (const char* c = value.c_str(); *c; ++c) { if (*c == '"') flush(); else piece.add_byte(*c); }
		flush();
		return out;
	}

	// A query without its "SORT ..." clause (which cannot sit inside parentheses); empty if only
	// "ALL" is left, since that adds nothing to an AND.
	static pfc::string8 withoutSort(const pfc::string8& q) {
		pfc::string8 out = q;
		for (size_t i = 0; i + 6 <= q.length(); ++i) {
			if ((i == 0 || q[i - 1] == ' ') && pfc::stricmp_ascii_partial(q.c_str() + i, "SORT ") == 0) { out = pfc::string8(q.c_str(), i); break; }
		}
		while (out.length() && out[out.length() - 1] == ' ') out.truncate(out.length() - 1);
		if (pfc::stringEqualsI_ascii(out, "ALL")) out.reset();
		return out;
	}

	// Query describing the selection of the panes up to and including this one, plus the active
	// filters and the search (for "Send to new autoplaylist"). It matches the same tracks the pane
	// shows, as long as the source is the Media Library.
	pfc::string8 CFacetBrowser::SelectionQuery(size_t upTo) const {
		pfc::string8 query;
		auto addAnd = [&](const pfc::string8& part) {
			if (part.is_empty()) return;
			if (!query.is_empty()) query << " AND ";
			query << "( " << part << " )";   // spaces: the query parser does not take "MISSING)"
		};
		for (size_t i = 0; i < m_panes.size() && i <= upTo; ++i) {
			const auto& p = m_panes[i];
			if (p.selected.empty()) continue;
			pfc::string8 group;
			for (auto& key : p.selected) {
				uint32_t id;
				if (!m_model.findValue(i, key, id)) continue;
				const auto& parts = m_model.valueParts(i, id);
				pfc::string8 cond;
				for (size_t c = 0; c < p.spec.columns.size() && c < parts.size(); ++c) {
					const auto& cs = p.spec.columns[c];
					pfc::string8 one;
					if (cs.folder) {
						// the exact folders this item stands for: every distinct path up to this level
						std::vector<pfc::string8> prefixes;
						for (uint32_t t : m_model.filter(i, p.input, { id }, false)) {
							pfc::string8 pre = folderPrefix(m_model.track(t), cs.depth);
							if (pre.is_empty()) { prefixes.clear(); break; }
							if (std::find(prefixes.begin(), prefixes.end(), pre) == prefixes.end()) prefixes.push_back(pre);
							if (prefixes.size() > 200) { prefixes.clear(); break; }
						}
						if (prefixes.empty()) one << "%path% HAS \"\\" << parts[c] << "\\\"";   // fallback: that folder name anywhere
						else for (auto& pre : prefixes) { if (!one.is_empty()) one << " OR "; one << "%path% HAS \"" << pre << "\""; }
					} else {
						const char* lt = strstr(cs.pattern, "%<");
						const char* gt = lt ? strstr(lt + 2, ">%") : nullptr;
						if (lt && gt) {
							// multi-value column: the item matches any value of the field
							const pfc::string8 field(lt + 2, gt - (lt + 2));
							if (parts[c] == missingValue(cs.pattern)) one << field << " MISSING";
							else one << isCondition(field, parts[c]);
						} else if (!strchr(cs.pattern, '"')) {
							// compare the formatted pattern; the trailing | keeps empty values comparable
							one << isCondition(PFC_string_formatter() << "\"" << cs.pattern << "|\"", PFC_string_formatter() << parts[c] << "|");
						}
					}
					if (one.is_empty()) continue;
					if (!cond.is_empty()) cond << " AND ";
					cond << "( " << one << " )";
				}
				if (cond.is_empty()) continue;
				if (!group.is_empty()) group << (m_cfg.panes[i].andLogic ? " AND " : " OR ");
				group << "( " << cond << " )";
			}
			addAnd(group);
		}
		auto defs = getFilters();
		for (auto& active : m_cfg.filters) for (auto& d : defs) if (d.name == active) addAnd(withoutSort(d.query));
		addAnd(withoutSort(searchQuery(SearchText())));
		return query.is_empty() ? pfc::string8("ALL") : query;
	}

	void CFacetBrowser::RunToolbarAction(size_t pane) {
		if (pane >= m_panes.size()) return;
		RunAction(m_cfg.action, PaneTracks(pane), pane);
	}

	void CFacetBrowser::RunAction(Action a, metadb_handle_list tracks, size_t pane) {
		SortTracks(tracks);
		auto pm = playlist_manager::get();
		auto toggle = pfc::autoToggle(m_sending, true);
		const pfc::string8 selName = getSelectionPlaylist();
		auto locked = [&](size_t idx) {
			return pm->playlist_lock_is_present(idx) && (pm->playlist_lock_get_filter_mask(idx) & (playlist_lock::filter_add | playlist_lock::filter_remove));
		};
		switch (a) {
		case Action::SendToSelection:
		case Action::SendToSelectionKeepPlaying: {
			size_t idx = pm->find_or_create_playlist(selName);
			if (locked(idx)) return;
			// "keep playing": just don't restart playback; the current track plays on and the
			// playing playlist stays what it was
			pm->playlist_undo_backup(idx);
			pm->playlist_clear(idx);
			pm->playlist_add_items(idx, tracks, pfc::bit_array_false());
			pm->set_active_playlist(idx);
			if (a == Action::SendToSelection && tracks.get_count() > 0) {
				pm->playlist_set_focus_item(idx, 0);
				pm->set_playing_playlist(idx);
				playback_control::get()->start(playback_control::track_command_settrack);
			}
			break;
		}
		case Action::SendToCurrent: {
			size_t idx = pm->get_active_playlist();
			if (idx == pfc_infinite) idx = pm->find_or_create_playlist(selName);
			if (locked(idx)) return;
			pm->playlist_undo_backup(idx);
			pm->playlist_clear(idx);
			pm->playlist_add_items(idx, tracks, pfc::bit_array_false());
			break;
		}
		case Action::SendToNew: {
			size_t idx = pm->create_playlist(SelectionLabel(pane), pfc_infinite, pfc_infinite);
			pm->playlist_add_items(idx, tracks, pfc::bit_array_false());
			pm->set_active_playlist(idx);
			break;
		}
		case Action::SendToNewAutoplaylist: {
			const pfc::string8 query = SelectionQuery(pane);
			if (!isValidQuery(query)) {
				popup_message::g_show(PFC_string_formatter() << "Could not create the autoplaylist, the query is not valid:\n" << query, "Facet Browser");
				break;
			}
			size_t idx = pm->create_playlist(SelectionLabel(pane), pfc_infinite, pfc_infinite);
			try {
				autoplaylist_manager::get()->add_client_simple(query, getSortPattern(), idx, autoplaylist_flag_sort);
			} catch (std::exception const& e) {
				popup_message::g_show(PFC_string_formatter() << "Could not create the autoplaylist: " << e.what(), "Facet Browser");
			}
			pm->set_active_playlist(idx);
			break;
		}
		case Action::AddToSelection: {
			size_t idx = pm->find_or_create_playlist(selName);
			if (locked(idx)) return;
			pm->playlist_undo_backup(idx);
			pm->playlist_add_items(idx, tracks, pfc::bit_array_false());
			break;
		}
		case Action::AddToCurrent: {
			size_t idx = pm->get_active_playlist();
			if (idx == pfc_infinite) idx = pm->find_or_create_playlist(selName);
			if (locked(idx)) return;
			pm->playlist_undo_backup(idx);
			pm->playlist_add_items(idx, tracks, pfc::bit_array_false());
			break;
		}
		default: break;
		}
	}

	// ================================================================ registration

	class FacetBrowserElement : public ui_element_impl<CFacetBrowser, ui_element_v2> {
	public:
		t_uint32 get_flags() override { return KFlagHavePopupCommand; }
		bool bump() override { return false; }
		bool get_popup_specs(ui_size& defSize, pfc::string_base& title) override {
			defSize.cx = 900; defSize.cy = 520;
			title = "Facet Browser";
			return true;
		}
	};

	static service_factory_single_t<FacetBrowserElement> g_element_factory;
}
