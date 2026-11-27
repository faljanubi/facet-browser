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

	void CArtGrid::OnDestroy() {
		StopWorker();
		if (m_tip.m_hWnd) m_tip.DestroyWindow();
	}

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
		if (!m_columns.empty()) { auto cols = m_columns; SetColumns(cols, m_sortColumn, m_sortDescending); }   // titles measured in this font
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
		m_holdResize = true;   // decode the covers that are on screen once the scroll position is final
		Relayout();
		if (anchor >= 0) ScrollTo(ItemRect((size_t)anchor).top);
		m_holdResize = false;
		if (m_hWnd && m_workerArt != m_art) ResizeNow();
	}

	void CArtGrid::ScrollToItem(size_t idx) {
		if (idx >= m_host->artCount()) return;
		const CRect t = ItemRect(idx);
		ScrollTo(idx < (size_t)std::max(1, m_cols) ? 0 : t.top);   // first row: the very top, margin included
	}

	CArtGrid::Covers CArtGrid::DetachCovers() {
		Covers c;
		c.artSize = m_workerArt;
		c.artType = artGuid(m_artType);
		for (size_t i = 0; i < m_cache.size(); ++i) {
			if (!m_cache[i]) continue;
			c.items.push_back({ i, m_cache[i] });
			m_cache[i] = NULL;   // owned by the caller now
		}
		return c;
	}

	void CArtGrid::FreeCovers(const Covers& c) {
		for (auto& e : c.items) if (e.second && e.second != kNoArt) DeleteObject(e.second);
	}

	void CArtGrid::AdoptCovers(const Covers& c) {
		const bool sameType = c.artType == artGuid(m_artType);
		std::vector<char> adopted(m_cache.size(), 0);
		for (auto& e : c.items) {
			const size_t i = e.first;
			if (sameType && i < m_cache.size() && !m_cache[i]) {
				m_cache[i] = e.second;
				m_requested[i] = 1;
				adopted[i] = 1;
			} else if (e.second && e.second != kNoArt) DeleteObject(e.second);
		}
		{
			// covers just handed over don't need the decode Reload() may have queued for them
			std::lock_guard<std::mutex> lock(m_mutex);
			m_queue.erase(std::remove_if(m_queue.begin(), m_queue.end(), [&](const Job& q) { return q.first < adopted.size() && adopted[q.first]; }), m_queue.end());
		}
		// decoded for another art size: decode the ones on screen again at this size
		if (sameType && c.artSize != m_workerArt) ResizeNow();
		Invalidate();
	}

	int CArtGrid::FirstVisibleItem() const {
		const size_t n = m_host->artCount();
		if (n == 0 || m_itemH <= 0) return -1;
		const int row = std::max(0, (m_scrollY - m_top) / m_itemH);
		return (int)std::min(n - 1, (size_t)row * (size_t)std::max(1, m_cols));
	}

	void CArtGrid::SetColumns(const std::vector<std::pair<pfc::string8, bool>>& cols, int sortColumn, bool descending) {
		m_columns = cols; m_sortColumn = sortColumn; m_sortDescending = descending;
		if (!m_header.m_hWnd) return;
		while (m_header.GetItemCount() > 0) m_header.DeleteItem(0);
		const int dpi = QueryScreenDPI(*this);
		// Statistic titles only label the sort (their values are in the captions), so each gets just
		// the room its title needs; a fixed width cut off the last title in narrow panes.
		CClientDC dc(m_header);
		HFONT hf = m_header.GetFont();
		HFONT oldFont = dc.SelectFont(hf ? hf : (HFONT)GetStockObject(DEFAULT_GUI_FONT));
		const int n = (int)cols.size();
		for (int i = 0; i < n; ++i) {
			pfc::stringcvt::string_wide_from_utf8 title(cols[i].first);
			HDITEM item = {};
			item.mask = HDI_TEXT | HDI_FORMAT | HDI_WIDTH;
			item.pszText = const_cast<wchar_t*>(title.get_ptr());
			item.fmt = (cols[i].second ? HDF_RIGHT : HDF_LEFT) | HDF_STRING;
			if (i == sortColumn) item.fmt |= descending ? HDF_SORTDOWN : HDF_SORTUP;
			CSize sz; dc.GetTextExtent(title, (int)wcslen(title), &sz);
			item.cxy = cols[i].second ? std::max<int>(sz.cx + MulDiv(14, dpi, 96), MulDiv(40, dpi, 96)) : 60;
			m_header.InsertItem(i, &item);
		}
		dc.SelectFont(oldFont);
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
		if (m_workerArt != m_art && !m_holdResize) ResizeNow();   // art size changed
		QueueVisible();
		Invalidate();
	}

	static HBITMAP decodeScaled(IWICImagingFactory* wic, const void* data, size_t size, int box);

	void CArtGrid::ResizeNow() {
		GUID artId;
		{
			std::lock_guard<std::mutex> lock(m_mutex);
			m_queue.clear();
			++m_generation;      // results still on their way are for the old size
			m_workerArt = m_art;
			artId = m_workerArtId;
		}
		const size_t n = std::min(m_cache.size(), m_host->artCount());
		const CRect content = ContentRect();
		const int viewTop = m_scrollY, viewBottom = m_scrollY + content.Height();
		// the covers on screen: read again and decoded at the new size now, several at a time
		std::vector<size_t> now;
		std::vector<metadb_handle_ptr> tracks;
		for (size_t i = 0; i < n; ++i) {
			if (!m_cache[i] || m_cache[i] == kNoArt) continue;
			const CRect t = ItemRect(i);
			if (t.bottom < viewTop || t.top > viewBottom) continue;
			auto h = m_host->artTrack(i);
			if (h.is_empty()) continue;
			now.push_back(i);
			tracks.push_back(h);
		}
		std::vector<HBITMAP> fresh(now.size(), NULL);
		if (!now.empty()) {
			const int box = m_art;
			const size_t threads = std::min<size_t>(now.size(), std::max(1u, std::min(8u, std::thread::hardware_concurrency())));
			std::atomic<size_t> next{ 0 };
			auto run = [&] {
				const HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
				{
					CComPtr<IWICImagingFactory> wic;
					wic.CoCreateInstance(CLSID_WICImagingFactory);
					for (size_t k; wic && (k = next++) < now.size();) {
						try {
							abort_callback_dummy abort;
							auto inst = album_art_manager_v2::get()->open(pfc::list_single_ref_t<metadb_handle_ptr>(tracks[k]), pfc::list_single_ref_t<GUID>(artId), abort);
							auto data = inst->query(artId, abort);
							if (data.is_valid()) fresh[k] = decodeScaled(wic, data->get_ptr(), data->get_size(), box);
						} catch (...) {}
					}
				}
				if (SUCCEEDED(hr)) CoUninitialize();
			};
			std::vector<std::thread> pool;
			for (size_t t = 0; t < threads; ++t) pool.emplace_back(run);
			for (auto& t : pool) t.join();
		}
		m_requested.assign(m_cache.size(), 0);
		for (size_t k = 0; k < now.size(); ++k) {
			if (!fresh[k]) continue;
			DeleteObject(m_cache[now[k]]);
			m_cache[now[k]] = fresh[k];
			m_requested[now[k]] = 1;
		}
		// the rest keep their bitmap as a stand-in until the background decode replaces it
		for (size_t i = 0; i < m_cache.size(); ++i) if (m_cache[i] == kNoArt) m_requested[i] = 1;   // still no art
		QueueVisible();
		Invalidate();
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
		TipRemove();
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
		TipRemove();
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
				UINT align;
				const CRect text = LabelRect(item, art, lines.size(), align);
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

	CRect CArtGrid::LabelRect(const CRect& item, const CRect& art, size_t lines, UINT& align) const {
		if (m_mode == Display::ArtRight) {
			align = DT_LEFT;
			return CRect(art.right + m_gap, item.top + (item.Height() - (int)lines * m_lineH) / 2, item.right - m_gap / 2, item.bottom);
		}
		// centred under the cover and as wide as it, like Facets
		align = DT_CENTER;
		return CRect(art.left, art.bottom + m_labelGap, art.right, item.bottom);
	}

	// ---------------------------------------------------------------- tooltips

	void CArtGrid::OnMouseMove(UINT, CPoint pt) {
		if (!m_tipRect.IsRectEmpty()) {
			CPoint s = pt; ClientToScreen(&s);
			if (!m_tipRect.PtInRect(s)) TipRemove();
		}
		TRACKMOUSEEVENT ev = { sizeof(ev) };
		ev.dwFlags = TME_HOVER | TME_LEAVE;
		ev.hwndTrack = *this;
		ev.dwHoverTime = HOVER_DEFAULT;
		TrackMouseEvent(&ev);
	}

	void CArtGrid::OnMouseHover(WPARAM flags, CPoint pt) {
		if (!m_tipRect.IsRectEmpty() || (flags & (MK_LBUTTON | MK_RBUTTON | MK_MBUTTON))) return;
		if (m_mode == Display::ArtNone) return;
		const int hit = HitTest(pt);
		if (hit < 0) return;
		CRect content = ContentRect();
		CRect item = ItemRect((size_t)hit);
		item.OffsetRect(0, content.top - m_scrollY);
		auto lines = m_host->artLines((size_t)hit);
		UINT align;
		const CRect text = LabelRect(item, ArtRect(item), lines.size(), align);
		for (size_t l = 0; l < lines.size(); ++l) {
			CRect r(text.left, text.top + (int)l * m_lineH, text.right, text.top + (int)(l + 1) * m_lineH);
			if (!r.PtInRect(pt)) continue;
			pfc::stringcvt::string_wide_from_utf8 w(lines[l]);
			CClientDC dc(*this);
			HFONT old = dc.SelectFont(m_font ? m_font : (HFONT)GetStockObject(DEFAULT_GUI_FONT));
			CSize sz; dc.GetTextExtent(w, (int)wcslen(w), &sz);
			dc.SelectFont(old);
			if (sz.cx <= r.Width() || wcslen(w) >= 4096) return;   // not cut off
			if (!m_tip.m_hWnd) {
				// TTS_NOPREFIX: an "&" in a name is shown as is, not as a mnemonic underline
				if (!m_tip.Create(NULL, NULL, NULL, WS_POPUP | TTS_NOPREFIX, WS_EX_TRANSPARENT)) return;
				m_tipInfo.cbSize = sizeof(m_tipInfo);
				m_tipInfo.uFlags = TTF_TRACK | TTF_IDISHWND | TTF_ABSOLUTE | TTF_TRANSPARENT;
				m_tipInfo.hwnd = *this;
				m_tipInfo.uId = 0;
				m_tipInfo.lpszText = LPSTR_TEXTCALLBACK;
				m_tip.AddTool(&m_tipInfo);
				auto cb = m_host->artCallback();
				DarkMode::ApplyDarkThemeCtrl(m_tip, cb.is_valid() && cb->is_dark_mode());
			}
			if (m_font) m_tip.SetFont(m_font);
			m_tipText = w.get_ptr();
			// a cut-off line fills its whole width, so the full text starts where it does
			CRect full = r;
			ClientToScreen(full);
			m_tipRect = full;
			m_tip.TrackPosition(full.left, full.top);
			m_tip.TrackActivate(&m_tipInfo, TRUE);
			return;
		}
	}

	LRESULT CArtGrid::OnTipGetDispInfo(LPNMHDR hdr) {
		if (hdr->hwndFrom != m_tip.m_hWnd) { SetMsgHandled(FALSE); return 0; }
		auto info = reinterpret_cast<LPNMTTDISPINFO>(hdr);
		info->lpszText = const_cast<wchar_t*>(m_tipText.c_str());
		info->hinst = 0;
		info->uFlags = 0;
		return 0;
	}

	void CArtGrid::ColorsChanged() {
		Invalidate();
		if (m_header.m_hWnd) m_header.Invalidate();
		if (m_tip.m_hWnd) {   // follow a dark/light mode switch
			auto cb = m_host->artCallback();
			DarkMode::ApplyDarkThemeCtrl(m_tip, cb.is_valid() && cb->is_dark_mode());
		}
	}

	void CArtGrid::TipRemove() {
		if (m_tipRect.IsRectEmpty()) return;
		m_tipRect.SetRectEmpty();
		if (m_tip.m_hWnd) m_tip.TrackActivate(&m_tipInfo, FALSE);
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
		TipRemove();
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
		TipRemove();
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
		TipRemove();
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
		const size_t n = std::min(m_host->artCount(), m_requested.size());
		// Covers on screen load first, then the ones just above and below (prefetch). Covers that have
		// scrolled out of that range leave the queue, so after a long scroll the visible ones don't wait
		// for every row passed on the way, as in Facets. Covers already loaded stay cached.
		const int viewTop = m_scrollY, viewBottom = m_scrollY + content.Height();
		const int top = m_scrollY - content.Height() / 2, bottom = m_scrollY + content.Height() * 2;
		auto inRange = [&](size_t i, int lo, int hi) { CRect t = ItemRect(i); return t.bottom >= lo && t.top <= hi; };
		std::vector<Job> visible, nearby;
		for (size_t i = 0; i < n; ++i) {
			if (m_requested[i] || !inRange(i, top, bottom)) continue;
			auto h = m_host->artTrack(i);
			if (h.is_empty()) continue;
			m_requested[i] = 1;
			(inRange(i, viewTop, viewBottom) ? visible : nearby).push_back({ i, h });
		}
		bool changed = !visible.empty() || !nearby.empty();
		{
			std::lock_guard<std::mutex> lock(m_mutex);
			std::vector<Job> queue;
			queue.reserve(m_queue.size() + visible.size() + nearby.size());
			// order: visible (already queued, then new), then prefetch (already queued, then new);
			// queued covers now out of range are dropped and can be asked for again later
			std::vector<Job> queuedNearby;
			for (auto& q : m_queue) {
				if (q.first < n && inRange(q.first, viewTop, viewBottom)) queue.push_back(q);
				else if (q.first < n && inRange(q.first, top, bottom)) queuedNearby.push_back(q);
				else { if (q.first < m_requested.size()) m_requested[q.first] = 0; changed = true; }
			}
			for (auto& a : visible) queue.push_back(a);
			for (auto& q : queuedNearby) queue.push_back(q);
			for (auto& a : nearby) queue.push_back(a);
			m_queue.swap(queue);
		}
		if (changed) m_cv.notify_one();
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
				Job job;
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
