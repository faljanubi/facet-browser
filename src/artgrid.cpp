#include "stdafx.h"
#include "artgrid.h"

namespace fb { void paintHeaderDividers(HWND header, HDC dc); }   // browser.cpp

#pragma comment(lib, "windowscodecs.lib")
#pragma comment(lib, "msimg32.lib")

namespace fb {

	static HBITMAP const kNoArt = (HBITMAP)(intptr_t)1;

	static GUID artGuid(ArtType t) {
		switch (t) {
		case ArtType::Back: return album_art_ids::cover_back;
		case ArtType::Disc: return album_art_ids::disc;
		case ArtType::Artist: return album_art_ids::artist;
		default: return album_art_ids::cover_front;
		}
	}

	CArtGrid::~CArtGrid() {
		StopWorker();
		for (auto b : m_cache) if (b && b != kNoArt) DeleteObject(b);
	}

	int CArtGrid::OnCreate(LPCREATESTRUCT) {
		m_header.Create(*this, NULL, nullptr, WS_CHILD | WS_VISIBLE | HDS_BUTTONS | HDS_FULLDRAG | HDS_HORZ, 0);
		m_workerArtId = artGuid(m_artType);
		StartWorker();
		return 0;
	}

	void CArtGrid::OnDestroy() { StopWorker(); }

	COLORREF CArtGrid::Color(int sysIdx) const {
		auto cb = m_host->artCallback();
		return cb.is_valid() ? cb->getSysColor(sysIdx) : GetSysColor(sysIdx);
	}

	void CArtGrid::SetFont(HFONT f) {
		m_font = f;
		Relayout();
	}

	void CArtGrid::SetHeaderFont(HFONT f) {
		// Same font as the column headers of the list panes, so all headers line up.
		if (m_header.m_hWnd) m_header.SetFont(f);
		Relayout();
	}

	void CArtGrid::SetArtType(ArtType t) {
		if (t == m_artType) return;
		m_artType = t;
		{
			std::lock_guard<std::mutex> lock(m_mutex);
			m_workerArtId = artGuid(t);
		}
		Reload();
	}

	void CArtGrid::SetArtScale(uint32_t percent) {
		if (percent == m_scale) return;
		// keep the first visible item at the top while the size changes
		const int anchor = m_hWnd ? FirstVisibleItem() : -1;
		m_scale = percent;
		Relayout();
		if (anchor >= 0) ScrollTo(ItemRect((size_t)anchor).top);
	}

	int CArtGrid::FirstVisibleItem() const {
		const size_t n = m_host->artCount();
		if (n == 0 || m_itemH <= 0) return -1;
		const int row = std::max(0, (m_scrollY - m_top) / m_itemH);
		return (int)std::min(n - 1, (size_t)row * (size_t)std::max(1, m_cols));
	}

	void CArtGrid::SetColumns(const std::vector<std::pair<pfc::string8, bool>>& cols, int sortColumn, bool descending) {
		if (!m_header.m_hWnd) return;
		while (m_header.GetItemCount() > 0) m_header.DeleteItem(0);
		const int dpi = QueryScreenDPI(*this);
		const int statW = MulDiv(90, dpi, 96);
		const int n = (int)cols.size();
		for (int i = 0; i < n; ++i) {
			pfc::stringcvt::string_wide_from_utf8 title(cols[i].first);
			HDITEM item = {};
			item.mask = HDI_TEXT | HDI_FORMAT | HDI_WIDTH;
			item.pszText = const_cast<wchar_t*>(title.get_ptr());
			item.fmt = (cols[i].second ? HDF_RIGHT : HDF_LEFT) | HDF_STRING;
			if (i == sortColumn) item.fmt |= descending ? HDF_SORTDOWN : HDF_SORTUP;
			item.cxy = cols[i].second ? statW : 60;
			m_header.InsertItem(i, &item);
		}
		FitHeader();
	}

	// Statistics columns keep their width; the other columns share the rest of the pane width.
	// Called again on every resize, so the header always spans the whole pane.
	void CArtGrid::FitHeader() {
		if (!m_header.m_hWnd) return;
		const int n = m_header.GetItemCount();
		if (n <= 0) return;
		CRect rc; GetClientRect(rc);
		int used = 0, keys = 0;
		for (int i = 0; i < n; ++i) {
			HDITEM it = {}; it.mask = HDI_FORMAT | HDI_WIDTH; m_header.GetItem(i, &it);
			if (it.fmt & HDF_RIGHT) used += it.cxy; else ++keys;
		}
		if (keys == 0) return;
		const int each = std::max(MulDiv(40, QueryScreenDPI(*this), 96), (rc.Width() - used) / keys);
		int left = rc.Width() - used - each * (keys - 1);
		for (int i = 0, k = 0; i < n; ++i) {
			HDITEM it = {}; it.mask = HDI_FORMAT | HDI_WIDTH; m_header.GetItem(i, &it);
			if (it.fmt & HDF_RIGHT) continue;
			it.mask = HDI_WIDTH; it.cxy = (++k == keys) ? std::max(each, left) : each;
			m_header.SetItem(i, &it);
		}
	}

	// ---------------------------------------------------------------- layout

	CRect CArtGrid::ContentRect() const {
		CRect rc; GetClientRect(rc);
		rc.top += m_headerH;
		return rc;
	}

	void CArtGrid::Relayout() {
		if (!m_hWnd) return;
		const int dpi = QueryScreenDPI(*this);
		CRect rc; GetClientRect(rc);
		// header
		if (m_header.m_hWnd) {
			WINDOWPOS wp = {};
			HDLAYOUT hl = { &rc, &wp };
			CRect tmp = rc;
			hl.prc = &tmp;
			m_header.Layout(&hl);
			m_header.SetWindowPos(NULL, wp.x, wp.y, wp.cx, wp.cy, SWP_NOZORDER | SWP_NOACTIVATE);
			m_headerH = wp.cy;
			FitHeader();
		}
		{
			CClientDC dc(*this);
			HFONT old = dc.SelectFont(m_font ? m_font : (HFONT)GetStockObject(DEFAULT_GUI_FONT));
			TEXTMETRIC tm = {}; dc.GetTextMetrics(&tm);
			dc.SelectFont(old);
			m_lineH = tm.tmHeight + MulDiv(1, dpi, 96);
		}
		m_lines = m_host->artCount() > 0 ? std::max<int>(1, (int)m_host->artLines(0).size()) : 1;
		m_gap = MulDiv(8, dpi, 96);
		CRect content = ContentRect();
		// The album art views copy the geometry of Library Tree's album art browser (the JSplitter script
		// panel) with its default thumbnail settings. Sizes below were measured at 150% scaling (144 DPI)
		// and scale with the DPI: a cell of 243 px (kept fixed and centred, as in Facets), a 13 px gap
		// plus a 3 px frame around each cover, 13 px side margins.
		auto px = [dpi](int at144) { return MulDiv(at144, dpi, 144); };
		const int cell = std::max(px(40), MulDiv(px(243), (int)m_scale, 100));   // target cell, scaled by the art size setting
		// Library Tree's scrollbar sits inside its margin, so the grid uses the full pane width;
		// ours is a real scrollbar, which the right margin overlaps the same way.
		const int fullW = content.Width() + ((GetStyle() & WS_VSCROLL) ? GetSystemMetrics(SM_CXVSCROLL) : 0);
		auto fitColumns = [&](int block, int margin) {
			// Like Facets: the cells keep their size and a resize only reflows the columns, with the
			// grid centred in the pane. The covers never change size, so nothing has to be loaded again
			// (only a pane narrower than one cell shrinks it).
			const int panelW = std::max(1, fullW - 2 * margin);
			m_cols = std::max(1, panelW / block);
			m_itemW = std::max(10, std::min(block, panelW));
			m_left = margin + std::max(0, (panelW - m_cols * m_itemW) / 2);
		};
		switch (m_mode) {
		case Display::ArtRight:
			m_art = std::max(MulDiv(24, dpi, 96), MulDiv(MulDiv(64, dpi, 96), (int)m_scale, 100));
			m_cols = 1;
			m_left = m_gap;
			m_itemW = content.Width() - 2 * m_gap;
			m_itemH = std::max(m_art, m_lines * m_lineH) + m_gap;
			m_top = m_gap / 2;
			break;
		case Display::ArtNone: {   // Library Tree "no labels": 3 px compact gap, no frame
			const int cov = px(3);
			fitColumns(cell, px(13));
			m_side = 0;
			m_art = std::max(10, m_itemW - cov);
			m_inset = (m_itemW - m_art) / 2;
			m_itemH = m_art + cov;
			m_top = (cov + 1) / 2;
			break;
		}
		default: {   // bottom labels
			const int cov = px(13);
			m_side = px(3);
			fitColumns(cell, px(13));
			m_inset = cov + m_side;
			m_art = std::max(10, m_itemW - 2 * m_inset);
			m_lineH = std::max(m_lineH, px(27));
			m_labelGap = (cov + 1) / 2;
			m_itemH = m_art + m_lines * m_lineH + 2 * m_inset;
			m_top = px(2);
			break;
		}
		}
		const size_t n = m_host->artCount();
		m_rows = (int)((n + m_cols - 1) / m_cols);
		m_contentH = m_top + m_rows * m_itemH + m_top;

		SCROLLINFO si = { sizeof(si), SIF_RANGE | SIF_PAGE | SIF_POS | SIF_DISABLENOSCROLL };
		si.nMin = 0; si.nMax = std::max(0, m_contentH - 1); si.nPage = content.Height();
		m_scrollY = std::max(0, std::min(m_scrollY, std::max(0, m_contentH - content.Height())));
		si.nPos = m_scrollY;
		SetScrollInfo(SB_VERT, &si, TRUE);
		if (m_workerArt != m_art) {
			// size changed: new covers are decoded at the new size; the ones already shown are drawn
			// scaled until the size settles, then decoded again sharply (no flashing placeholders)
			{
				std::lock_guard<std::mutex> lock(m_mutex);
				m_workerArt = m_art;
			}
			SetTimer(TIMER_RERENDER, 250);
		}
		QueueVisible();
		Invalidate();
	}

	void CArtGrid::OnTimer(UINT_PTR id) {
		if (id != TIMER_RERENDER) { SetMsgHandled(FALSE); return; }
		KillTimer(id);
		Rerender();
	}

	void CArtGrid::Rerender() {
		{
			std::lock_guard<std::mutex> lock(m_mutex);
			m_queue.clear();
			++m_generation;
		}
		// keep the current bitmaps on screen; each is replaced when its sharp version arrives
		m_requested.assign(m_cache.size(), 0);
		for (size_t i = 0; i < m_cache.size(); ++i) if (m_cache[i] == kNoArt) m_requested[i] = 1;   // still no art
		QueueVisible();
	}

	CRect CArtGrid::ItemRect(size_t idx) const {
		const int col = (int)(idx % m_cols), row = (int)(idx / m_cols);
		const int x = m_left + col * m_itemW;
		const int y = m_top + row * m_itemH;
		return CRect(x, y, x + m_itemW, y + m_itemH);
	}

	CRect CArtGrid::ArtRect(const CRect& item) const {
		if (m_mode == Display::ArtRight) {
			const int pad = m_gap / 2;
			return CRect(item.left, item.top + pad, item.left + m_art, item.top + pad + m_art);
		}
		const int top = m_mode == Display::ArtNone ? item.top : item.top + m_inset;
		return CRect(item.left + m_inset, top, item.left + m_inset + m_art, top + m_art);
	}

	int CArtGrid::HitTest(CPoint pt) const {
		CRect content = ContentRect();
		if (!content.PtInRect(pt)) return -1;
		pt.y = pt.y - content.top + m_scrollY;
		const size_t n = m_host->artCount();
		for (size_t i = 0; i < n; ++i) if (ItemRect(i).PtInRect(pt)) return (int)i;
		return -1;
	}

	void CArtGrid::ScrollTo(int y) {
		CRect content = ContentRect();
		y = std::max(0, std::min(y, std::max(0, m_contentH - content.Height())));
		if (y == m_scrollY) return;
		m_scrollY = y;
		SetScrollPos(SB_VERT, y, TRUE);
		QueueVisible();
		InvalidateRect(content);
	}

	void CArtGrid::EnsureVisible(size_t idx) {
		CRect t = ItemRect(idx);
		CRect content = ContentRect();
		if (t.top < m_scrollY) ScrollTo(t.top - m_gap / 2);
		else if (t.bottom > m_scrollY + content.Height()) ScrollTo(t.bottom - content.Height() + m_gap / 2);
	}

	void CArtGrid::OnVScroll(UINT code, UINT, CScrollBar) {
		SCROLLINFO si = { sizeof(si), SIF_ALL };
		GetScrollInfo(SB_VERT, &si);
		CRect content = ContentRect();
		int y = m_scrollY;
		switch (code) {
		case SB_LINEUP: y -= m_itemH / 3; break;
		case SB_LINEDOWN: y += m_itemH / 3; break;
		case SB_PAGEUP: y -= content.Height(); break;
		case SB_PAGEDOWN: y += content.Height(); break;
		case SB_THUMBTRACK: case SB_THUMBPOSITION: y = si.nTrackPos; break;
		case SB_TOP: y = 0; break;
		case SB_BOTTOM: y = m_contentH; break;
		}
		ScrollTo(y);
	}

	BOOL CArtGrid::OnMouseWheel(UINT flags, short delta, CPoint) {
		if (flags & MK_CONTROL) {
			// Ctrl+wheel: album art size, one step per wheel notch
			m_wheel += delta;
			const int steps = m_wheel / WHEEL_DELTA;
			m_wheel -= steps * WHEEL_DELTA;
			if (steps) m_host->artZoom(steps);
			return TRUE;
		}
		m_wheel = 0;
		ScrollTo(m_scrollY - delta * m_itemH / WHEEL_DELTA / 2);
		return TRUE;
	}

	// ---------------------------------------------------------------- painting

	void CArtGrid::OnPaint(CDCHandle) {
		CPaintDC pdc(*this);
		CRect content = ContentRect();
		if (content.IsRectEmpty()) return;
		CMemoryDC dc(pdc, content);
		const COLORREF bg = Color(COLOR_WINDOW), fg = Color(COLOR_WINDOWTEXT);
		const COLORREF selBg = Color(COLOR_HIGHLIGHT), selFg = Color(COLOR_HIGHLIGHTTEXT);
		dc.FillSolidRect(content, bg);
		HFONT old = dc.SelectFont(m_font ? m_font : (HFONT)GetStockObject(DEFAULT_GUI_FONT));
		dc.SetBkMode(TRANSPARENT);
		auto mix = [](COLORREF a, COLORREF b, int pct) {
			return RGB((GetRValue(a) * (100 - pct) + GetRValue(b) * pct) / 100,
				(GetGValue(a) * (100 - pct) + GetGValue(b) * pct) / 100,
				(GetBValue(a) * (100 - pct) + GetBValue(b) * pct) / 100);
		};
		const COLORREF placeholder = mix(bg, fg, 12);
		const bool focused = GetFocus() == m_hWnd;

		const size_t n = m_host->artCount();
		for (size_t i = 0; i < n; ++i) {
			CRect item = ItemRect(i);
			item.OffsetRect(0, content.top - m_scrollY);
			if (item.bottom < content.top || item.top > content.bottom) continue;
			const bool sel = IsSelected(i);
			CRect box = item;   // selection frame: the cell minus Library Tree's 3 px border
			box.DeflateRect(m_side, m_side);
			if (sel) dc.FillSolidRect(box, selBg);
			CRect art = ArtRect(item);
			HBITMAP bmp = i < m_cache.size() ? m_cache[i] : NULL;
			if (bmp && bmp != kNoArt) {
				BITMAP bm = {}; GetObject(bmp, sizeof(bm), &bm);
				CDC mem; mem.CreateCompatibleDC(dc);
				HBITMAP ob = mem.SelectBitmap(bmp);
				// decoded for another size (the pane is being resized): draw it scaled to fit
				int w = bm.bmWidth, h = bm.bmHeight;
				if (w > art.Width() || h > art.Height() || (w < art.Width() && h < art.Height())) {
					const double f = std::min((double)art.Width() / std::max(1, w), (double)art.Height() / std::max(1, h));
					w = std::max(1, (int)(w * f + 0.5)); h = std::max(1, (int)(h * f + 0.5));
				}
				const int x = art.left + (art.Width() - w) / 2;
				const int y = art.top + (art.Height() - h) / 2;
				BLENDFUNCTION bf = { AC_SRC_OVER, 0, 255, AC_SRC_ALPHA };
				if (w != bm.bmWidth || h != bm.bmHeight) dc.SetStretchBltMode(HALFTONE);
				::AlphaBlend(dc, x, y, w, h, mem, 0, 0, bm.bmWidth, bm.bmHeight, bf);
				mem.SelectBitmap(ob);
			} else {
				dc.FillSolidRect(art, placeholder);
				dc.SetTextColor(mix(bg, fg, 45));
				CFont big; LOGFONT lf = {};
				GetObject(m_font ? m_font : (HFONT)GetStockObject(DEFAULT_GUI_FONT), sizeof(lf), &lf);
				lf.lfHeight = -art.Height() / 3;
				big.CreateFontIndirect(&lf);
				HFONT f0 = dc.SelectFont(big);
				dc.DrawText(m_host->artIsSummary(i) ? L"≡" : L"♪", -1, art, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
				dc.SelectFont(f0);
			}
			if (m_mode != Display::ArtNone) {
				auto lines = m_host->artLines(i);
				CRect text;
				UINT align;
				if (m_mode == Display::ArtRight) {
					text = CRect(art.right + m_gap, item.top + (item.Height() - (int)lines.size() * m_lineH) / 2, item.right - m_gap / 2, item.bottom);
					align = DT_LEFT;
				} else {
					// left-aligned under the cover and as wide as it, like Library Tree
					text = CRect(art.left, art.bottom + m_labelGap, art.right, item.bottom);
					align = DT_LEFT;
				}
				for (size_t l = 0; l < lines.size(); ++l) {
					CRect r(text.left, text.top + (int)l * m_lineH, text.right, text.top + (int)(l + 1) * m_lineH);
					dc.SetTextColor(sel ? selFg : (l == 0 ? fg : mix(bg, fg, 65)));
					dc.DrawText(pfc::stringcvt::string_wide_from_utf8(lines[l]), -1, r, align | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);
				}
			}
			if (focused && i == m_focus) {
				dc.SetTextColor(sel ? selFg : fg);
				dc.DrawFocusRect(box);
			}
		}
		dc.SelectFont(old);
		{
			// dark mode: same 1px line under the column titles as foobar2000's lists (#303030)
			auto cb = m_host->artCallback();
			if (m_headerH > 0 && cb.is_valid() && cb->is_dark_mode()) dc.FillSolidRect(CRect(content.left, content.top, content.right, content.top + 1), RGB(0x30, 0x30, 0x30));
		}
	}

	// ---------------------------------------------------------------- selection / input

	std::vector<size_t> CArtGrid::GetSelection() const {
		std::vector<size_t> out;
		for (size_t i = 0; i < m_sel.size(); ++i) if (m_sel[i]) out.push_back(i);
		return out;
	}

	void CArtGrid::SetSelection(const std::vector<size_t>& sel) {
		m_sel.assign(m_host->artCount(), 0);
		for (size_t i : sel) if (i < m_sel.size()) m_sel[i] = 1;
		if (!sel.empty()) m_focus = m_anchor = sel.front();
		Invalidate();
	}

	void CArtGrid::SelectSingle(size_t idx) {
		m_sel.assign(m_host->artCount(), 0);
		if (idx < m_sel.size()) m_sel[idx] = 1;
		m_focus = m_anchor = idx;
		EnsureVisible(idx);
		Invalidate();
		Notify();
	}

	void CArtGrid::OnLButtonDown(UINT flags, CPoint pt) {
		SetFocus();
		int hit = HitTest(pt);
		if (hit < 0) return;
		const size_t n = m_host->artCount();
		if (m_sel.size() != n) m_sel.assign(n, 0);
		if (flags & MK_SHIFT) {
			if (!(flags & MK_CONTROL)) m_sel.assign(n, 0);
			size_t a = std::min(m_anchor, (size_t)hit), b = std::max(m_anchor, (size_t)hit);
			for (size_t i = a; i <= b; ++i) m_sel[i] = 1;
			m_focus = hit;
		} else if (flags & MK_CONTROL) {
			m_sel[hit] = !m_sel[hit];
			m_focus = m_anchor = hit;
		} else {
			SelectSingle(hit);
			return;
		}
		Invalidate();
		Notify();
	}

	void CArtGrid::OnLButtonDblClk(UINT, CPoint pt) {
		int hit = HitTest(pt);
		if (hit >= 0) m_host->artAction(hit);
	}

	void CArtGrid::OnRButtonDown(UINT, CPoint pt) {
		SetFocus();
		int hit = HitTest(pt);
		if (hit >= 0 && !IsSelected(hit)) SelectSingle(hit);
	}

	void CArtGrid::OnContextMenu(CWindow wnd, CPoint pt) {
		bool onHeader = (wnd == m_header.m_hWnd);
		if (!onHeader && m_header.m_hWnd && !(pt.x == -1 && pt.y == -1)) {
			CRect hr; m_header.GetWindowRect(hr);
			onHeader = hr.PtInRect(pt) != FALSE;
		}
		if (pt.x == -1 && pt.y == -1) {
			CRect t = ItemRect(m_focus); t.OffsetRect(0, ContentRect().top - m_scrollY);
			pt = t.CenterPoint(); ClientToScreen(&pt);
		}
		m_host->artContextMenu(*this, pt, onHeader);
	}

	LRESULT CArtGrid::OnHeaderCustomDraw(LPNMHDR hdr) {
		// Same as the list panes' header: dark "ItemsView" theme only paints the
		// background, the label colour has to be supplied here.
		if (hdr->hwndFrom != m_header.m_hWnd) { SetMsgHandled(FALSE); return 0; }
		auto nmcd = reinterpret_cast<LPNMCUSTOMDRAW>(hdr);
		switch (nmcd->dwDrawStage) {
		case CDDS_PREPAINT: return CDRF_NOTIFYITEMDRAW | CDRF_NOTIFYPOSTPAINT;
		case CDDS_ITEMPREPAINT: ::SetTextColor(nmcd->hdc, Color(COLOR_WINDOWTEXT)); return CDRF_DODEFAULT;
		case CDDS_POSTPAINT: {
			auto cb = m_host->artCallback();
			if (cb.is_valid() && cb->is_dark_mode()) paintHeaderDividers(m_header, nmcd->hdc);
			return CDRF_DODEFAULT;
		}
		default: return CDRF_DODEFAULT;
		}
	}

	LRESULT CArtGrid::OnHeaderClick(LPNMHDR hdr) {
		auto info = reinterpret_cast<NMHEADER*>(hdr);
		m_host->artHeaderClick((size_t)info->iItem);
		return 0;
	}

	void CArtGrid::OnKeyDown(UINT ch, UINT, UINT) {
		if (m_host->artKeyDown(ch)) return;
		const size_t n = m_host->artCount();
		if (n == 0) return;
		CRect content = ContentRect();
		const int perPage = std::max(1, content.Height() / std::max(1, m_itemH)) * m_cols;
		long long f = (long long)m_focus;
		switch (ch) {
		case VK_LEFT: f -= 1; break;
		case VK_RIGHT: f += 1; break;
		case VK_UP: f -= m_cols; break;
		case VK_DOWN: f += m_cols; break;
		case VK_PRIOR: f -= perPage; break;
		case VK_NEXT: f += perPage; break;
		case VK_HOME: f = 0; break;
		case VK_END: f = (long long)n - 1; break;
		case VK_RETURN: m_host->artAction(m_focus); return;
		case 'A':
			if (GetKeyState(VK_CONTROL) < 0) {
				m_sel.assign(n, 1);
				for (size_t i = 0; i < n; ++i) if (m_host->artIsSummary(i)) m_sel[i] = 0;
				Invalidate(); Notify();
			}
			return;
		default: return;
		}
		f = std::max(0LL, std::min(f, (long long)n - 1));
		if (GetKeyState(VK_SHIFT) < 0) {
			m_sel.assign(n, 0);
			size_t a = std::min(m_anchor, (size_t)f), b = std::max(m_anchor, (size_t)f);
			for (size_t i = a; i <= b; ++i) m_sel[i] = 1;
			m_focus = (size_t)f;
			EnsureVisible(m_focus); Invalidate(); Notify();
		} else {
			SelectSingle((size_t)f);
		}
	}

	// ---------------------------------------------------------------- data + worker

	void CArtGrid::Reload() {
		{
			std::lock_guard<std::mutex> lock(m_mutex);
			m_queue.clear();
			++m_generation;
		}
		for (auto b : m_cache) if (b && b != kNoArt) DeleteObject(b);
		const size_t n = m_host->artCount();
		m_cache.assign(n, NULL);
		m_requested.assign(n, 0);
		if (m_sel.size() != n) m_sel.assign(n, 0);
		if (m_focus >= n) m_focus = 0;
		if (m_hWnd) {
			KillTimer(TIMER_RERENDER);
			{
				std::lock_guard<std::mutex> lock(m_mutex);
				m_workerArt = m_art;
			}
			Relayout();
		}
	}

	void CArtGrid::QueueVisible() {
		if (!m_hWnd) return;
		CRect content = ContentRect();
		const size_t n = m_host->artCount();
		std::vector<std::pair<size_t, metadb_handle_ptr>> add;
		const int top = m_scrollY - content.Height() / 2, bottom = m_scrollY + content.Height() * 2;
		for (size_t i = 0; i < n; ++i) {
			if (i < m_requested.size() && m_requested[i]) continue;
			CRect t = ItemRect(i);
			if (t.bottom < top || t.top > bottom) continue;
			auto h = m_host->artTrack(i);
			if (h.is_empty()) continue;
			m_requested[i] = 1;
			add.emplace_back(i, h);
		}
		if (add.empty()) return;
		{
			std::lock_guard<std::mutex> lock(m_mutex);
			for (auto& a : add) m_queue.push_back(a);
		}
		m_cv.notify_one();
	}

	LRESULT CArtGrid::OnArtLoaded(UINT, WPARAM, LPARAM lp) {
		std::unique_ptr<Result> r(reinterpret_cast<Result*>(lp));
		if (r->generation != m_generation || r->index >= m_cache.size()) {
			if (r->bmp && r->bmp != kNoArt) DeleteObject(r->bmp);
			return 0;
		}
		HBITMAP& slot = m_cache[r->index];
		if (slot && slot != kNoArt && slot != r->bmp) DeleteObject(slot);   // the scaled stand-in
		slot = r->bmp;
		CRect t = ItemRect(r->index); t.OffsetRect(0, ContentRect().top - m_scrollY);
		InvalidateRect(t);
		return 0;
	}

	void CArtGrid::StartWorker() {
		if (m_thread.joinable()) return;
		m_stop = false;
		m_thread = std::thread([this] { WorkerMain(); });
	}

	void CArtGrid::StopWorker() {
		if (!m_thread.joinable()) return;
		{
			std::lock_guard<std::mutex> lock(m_mutex);
			m_stop = true;
			m_queue.clear();
		}
		m_cv.notify_all();
		m_thread.join();
	}

	static HBITMAP decodeScaled(IWICImagingFactory* wic, const void* data, size_t size, int box) {
		CComPtr<IWICStream> stream;
		if (FAILED(wic->CreateStream(&stream))) return NULL;
		if (FAILED(stream->InitializeFromMemory((BYTE*)data, (DWORD)size))) return NULL;
		CComPtr<IWICBitmapDecoder> decoder;
		if (FAILED(wic->CreateDecoderFromStream(stream, nullptr, WICDecodeMetadataCacheOnDemand, &decoder))) return NULL;
		CComPtr<IWICBitmapFrameDecode> frame;
		if (FAILED(decoder->GetFrame(0, &frame))) return NULL;
		UINT w = 0, h = 0; frame->GetSize(&w, &h);
		if (!w || !h) return NULL;
		const double scale = std::min((double)box / w, (double)box / h);
		const UINT tw = std::max(1u, (UINT)(w * scale + 0.5)), th = std::max(1u, (UINT)(h * scale + 0.5));
		CComPtr<IWICBitmapScaler> scaler;
		if (FAILED(wic->CreateBitmapScaler(&scaler))) return NULL;
		if (FAILED(scaler->Initialize(frame, tw, th, WICBitmapInterpolationModeHighQualityCubic))) return NULL;
		CComPtr<IWICFormatConverter> conv;
		if (FAILED(wic->CreateFormatConverter(&conv))) return NULL;
		if (FAILED(conv->Initialize(scaler, GUID_WICPixelFormat32bppPBGRA, WICBitmapDitherTypeNone, nullptr, 0, WICBitmapPaletteTypeCustom))) return NULL;
		BITMAPINFO bi = {};
		bi.bmiHeader.biSize = sizeof(bi.bmiHeader);
		bi.bmiHeader.biWidth = tw; bi.bmiHeader.biHeight = -(LONG)th;
		bi.bmiHeader.biPlanes = 1; bi.bmiHeader.biBitCount = 32; bi.bmiHeader.biCompression = BI_RGB;
		void* bits = nullptr;
		HBITMAP bmp = CreateDIBSection(NULL, &bi, DIB_RGB_COLORS, &bits, NULL, 0);
		if (!bmp) return NULL;
		if (FAILED(conv->CopyPixels(nullptr, tw * 4, tw * 4 * th, (BYTE*)bits))) { DeleteObject(bmp); return NULL; }
		return bmp;
	}

	void CArtGrid::WorkerMain() {
		HRESULT hrCom = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
		{
			CComPtr<IWICImagingFactory> wic;
			wic.CoCreateInstance(CLSID_WICImagingFactory);
			for (;;) {
				std::pair<size_t, metadb_handle_ptr> job;
				uint32_t gen;
				int box;
				GUID id;
				{
					std::unique_lock<std::mutex> lock(m_mutex);
					m_cv.wait(lock, [this] { return m_stop || !m_queue.empty(); });
					if (m_stop) break;
					job = m_queue.front();
					m_queue.erase(m_queue.begin());
					gen = m_generation;
					box = m_workerArt;
					id = m_workerArtId;
				}
				HBITMAP bmp = kNoArt;
				try {
					abort_callback_dummy abort;
					auto inst = album_art_manager_v2::get()->open(pfc::list_single_ref_t<metadb_handle_ptr>(job.second), pfc::list_single_ref_t<GUID>(id), abort);
					auto data = inst->query(id, abort);
					if (data.is_valid() && wic) {
						HBITMAP b = decodeScaled(wic, data->get_ptr(), data->get_size(), box);
						if (b) bmp = b;
					}
				} catch (...) {
					// no art of this type for the item, or the file is unavailable
				}
				auto* r = new Result{ gen, job.first, bmp };
				if (!m_hWnd || !PostMessage(WM_ART_LOADED, 0, (LPARAM)r)) {
					if (bmp != kNoArt) DeleteObject(bmp);
					delete r;
				}
			}
		}
		if (SUCCEEDED(hrCom)) CoUninitialize();
	}
}
