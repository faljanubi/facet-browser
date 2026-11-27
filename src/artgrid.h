#pragma once
#include "config.h"

// Album art display of a facet pane: a header (same columns as the list view) above a
// scrollable grid of covers. Labels can be below the covers, to their right, or hidden.
// Covers are loaded on a worker thread (album_art_manager_v2 + WIC) and cached per item.

namespace fb {

	class IArtGridHost {
	public:
		virtual size_t artCount() = 0;                          // items, including the summary item
		virtual std::vector<pfc::string8> artLines(size_t idx) = 0;   // label lines
		virtual metadb_handle_ptr artTrack(size_t idx) = 0;     // representative track, null for none
		virtual bool artIsSummary(size_t idx) = 0;
		virtual void artSelectionChanged() = 0;
		virtual void artAction(size_t idx) = 0;
		virtual void artContextMenu(HWND wnd, CPoint pt, bool onHeader) = 0;
		virtual void artHeaderClick(size_t column) = 0;
		virtual ui_element_instance_callback::ptr artCallback() = 0;
		virtual bool artKeyDown(UINT) { return false; }       // true: key handled by the host (pane shortcuts)
		virtual void artFocusChanged(bool) {}                 // the grid got (true) or lost (false) the keyboard focus
		virtual void artZoom(int) {}                          // Ctrl+mouse wheel: +1 larger, -1 smaller
	};

	class CArtGrid : public CWindowImpl<CArtGrid> {
	public:
		DECLARE_WND_CLASS_EX(L"FacetBrowserArtGrid", CS_DBLCLKS, -1);

		explicit CArtGrid(IArtGridHost* host) : m_host(host) {}
		~CArtGrid();

		BEGIN_MSG_MAP_EX(CArtGrid)
			MSG_WM_CREATE(OnCreate)
			MSG_WM_NCHITTEST(OnNcHitTest)
			MSG_WM_DESTROY(OnDestroy)
			MSG_WM_PAINT(OnPaint)
			MSG_WM_ERASEBKGND(OnEraseBkgnd)
			MSG_WM_SIZE(OnSize)
			MSG_WM_VSCROLL(OnVScroll)
			MSG_WM_MOUSEWHEEL(OnMouseWheel)
			MSG_WM_LBUTTONDOWN(OnLButtonDown)
			MSG_WM_LBUTTONDBLCLK(OnLButtonDblClk)
			MSG_WM_RBUTTONDOWN(OnRButtonDown)
			MSG_WM_CONTEXTMENU(OnContextMenu)
			MSG_WM_KEYDOWN(OnKeyDown)
			MSG_WM_GETDLGCODE(OnGetDlgCode)
			MSG_WM_SETFOCUS(OnSetFocus)
			MSG_WM_KILLFOCUS(OnKillFocus)
			MSG_WM_TIMER(OnTimer)
			MSG_WM_MOUSEMOVE(OnMouseMove)
			MSG_WM_MOUSEHOVER(OnMouseHover)
			MSG_WM_MOUSELEAVE(OnMouseLeave)
			NOTIFY_CODE_HANDLER_EX(TTN_GETDISPINFO, OnTipGetDispInfo)
			NOTIFY_CODE_HANDLER_EX(HDN_ITEMCLICK, OnHeaderClick)
			NOTIFY_CODE_HANDLER_EX(NM_CUSTOMDRAW, OnHeaderCustomDraw)
			MESSAGE_HANDLER_EX(WM_ART_LOADED, OnArtLoaded)
		END_MSG_MAP()

		void Reload();                        // items changed
		void SetFont(HFONT f);
		void SetHeaderFont(HFONT f);
		void SetMode(Display d) { m_mode = d; Relayout(); }
		void SetArtType(ArtType t);
		void SetArtScale(uint32_t percent);   // album art size, percent of the default
		void ColorsChanged();
		void SetColumns(const std::vector<std::pair<pfc::string8, bool>>& cols, int sortColumn, bool descending);   // title, right-aligned
		CHeaderCtrl GetHeader() const { return m_header; }
		std::function<bool(CPoint)> isSplitterZone;   // screen point is on/next to a pane divider

		std::vector<size_t> GetSelection() const;
		void SetSelection(const std::vector<size_t>& sel);
		bool IsSelected(size_t idx) const { return idx < m_sel.size() && m_sel[idx]; }
		void EnsureVisible(size_t idx);

		// When the panes are rebuilt (another pane's columns changed), the new grid takes over the
		// covers already loaded and the scroll position, instead of starting at the top and loading
		// everything again. Items are identified by the host; indices here are this grid's items.
		struct CoverItem { size_t first; HBITMAP second; };   // index, bitmap
		struct Covers { std::vector<CoverItem> items; int artSize = 0; GUID artType = {}; };
		Covers DetachCovers();                     // takes the loaded covers out (this grid no longer owns them)
		void AdoptCovers(const Covers& c);         // indices already mapped to this grid's items; frees the rest
		static void FreeCovers(const Covers& c);   // when no grid takes them
		int FirstVisibleItem() const;              // -1 when empty
		void ScrollToItem(size_t idx);             // its row at the top

	private:
		void FitHeader();
		std::vector<std::pair<pfc::string8, bool>> m_columns;   // for re-measuring the header when its font changes
		int m_sortColumn = -1; bool m_sortDescending = false;
		enum { WM_ART_LOADED = WM_APP + 41 };
		void OnTimer(UINT_PTR) { SetMsgHandled(FALSE); }
		// Art size changed: the covers on screen are read again and decoded at the new size at once
		// (several in parallel) before the next repaint, so they go from sharp to sharp; the others
		// follow in the background. Nothing extra is kept in memory for this.
		void ResizeNow();
		bool m_holdResize = false;   // SetArtScale: resize after restoring the scroll position
		struct Job { size_t first; metadb_handle_ptr second; };
		struct Result { uint32_t generation; size_t index; HBITMAP bmp; };

		int OnCreate(LPCREATESTRUCT);
		LRESULT OnNcHitTest(CPoint pt) { if (isSplitterZone && isSplitterZone(pt)) return HTTRANSPARENT; SetMsgHandled(FALSE); return 0; }
		void OnDestroy();
		void OnPaint(CDCHandle);
		BOOL OnEraseBkgnd(CDCHandle) { return TRUE; }
		void OnSize(UINT, CSize) { Relayout(); }
		void OnVScroll(UINT code, UINT, CScrollBar);
		BOOL OnMouseWheel(UINT, short delta, CPoint);
		void OnLButtonDown(UINT flags, CPoint pt);
		void OnLButtonDblClk(UINT flags, CPoint pt);
		void OnRButtonDown(UINT flags, CPoint pt);
		void OnContextMenu(CWindow wnd, CPoint pt);
		void OnKeyDown(UINT ch, UINT, UINT);
		UINT OnGetDlgCode(LPMSG) { return DLGC_WANTARROWS | DLGC_WANTCHARS; }
		void OnSetFocus(CWindow) { Invalidate(); m_host->artFocusChanged(true); }
		void OnKillFocus(CWindow) { Invalidate(); m_host->artFocusChanged(false); }
		LRESULT OnHeaderClick(LPNMHDR hdr);
		LRESULT OnHeaderCustomDraw(LPNMHDR hdr);
		LRESULT OnArtLoaded(UINT, WPARAM, LPARAM);

		// Tooltip with the full text of a label line cut off with "...", shown in place over the line
		// (like the list panes' tooltips from libPPUI).
		void OnMouseMove(UINT flags, CPoint pt);
		void OnMouseHover(WPARAM flags, CPoint pt);
		void OnMouseLeave() { TipRemove(); }
		LRESULT OnTipGetDispInfo(LPNMHDR hdr);
		void TipRemove();
		CRect LabelRect(const CRect& item, const CRect& art, size_t lines, UINT& align) const;
		CToolTipCtrl m_tip;
		TOOLINFO m_tipInfo = {};
		CRect m_tipRect;                      // screen coordinates of the line it covers; empty = hidden
		std::wstring m_tipText;

		void Relayout();
		CRect ContentRect() const;            // client area below the header
		void ScrollTo(int y);
		int HitTest(CPoint pt) const;
		CRect ItemRect(size_t idx) const;     // content coordinates
		CRect ArtRect(const CRect& item) const;
		void SelectSingle(size_t idx);
		void Notify() { m_host->artSelectionChanged(); }
		COLORREF Color(int sysIdx) const;
		int Columns() const { return m_cols; }

		void StartWorker();
		void StopWorker();
		void WorkerMain();
		void QueueVisible();

		IArtGridHost* m_host;
		CHeaderCtrl m_header;
		HFONT m_font = NULL;
		Display m_mode = Display::ArtBottom;
		ArtType m_artType = ArtType::Front;
		std::vector<char> m_sel;
		size_t m_focus = 0, m_anchor = 0;
		int m_art = 120, m_lineH = 16, m_lines = 1, m_gap = 8, m_cols = 1, m_rows = 0, m_scrollY = 0, m_contentH = 0;
		int m_itemW = 0, m_itemH = 0, m_headerH = 0;
		uint32_t m_scale = 100;
		int m_wheel = 0;   // Ctrl+wheel movement not yet turned into a size step
		// grid geometry (bottom labels / no labels follow Library Tree's album art view)
		int m_left = 0, m_top = 0, m_inset = 0, m_side = 0, m_labelGap = 0;

		std::vector<HBITMAP> m_cache;     // per item; NULL = not loaded, (HBITMAP)1 = no art
		std::vector<char> m_requested;

		std::thread m_thread;
		std::mutex m_mutex;
		std::condition_variable m_cv;
		std::vector<Job> m_queue;
		std::atomic<uint32_t> m_generation{ 0 };
		bool m_stop = false;
		int m_workerArt = 120;
		GUID m_workerArtId = {};
	};
}
