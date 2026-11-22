#pragma once
#include "config.h"
#include "globals.h"
#include "model.h"
#include "artgrid.h"

namespace fb {

	// List view of a pane: the SDK list control (foobar2000 colours + dark mode) with the
	// folder marker and the dimmed count of the summary item drawn on top.
	class CPaneList : public CListControlFb2kColorsUIElem<CListControlOwnerData> {
	public:
		CPaneList(ui_element_instance_callback::ptr cb, IListControlOwnerDataSource* src)
			: CListControlFb2kColorsUIElem<CListControlOwnerData>(cb, src) {}
		std::function<bool(size_t)> hasChildren;   // draw "›" on row
		std::function<bool(size_t)> isSummary;     // row is the "All (...)" item
		std::function<bool(CPoint)> isSplitterZone;   // screen point is on/next to a pane divider
		std::function<void(int)> columnResized;      // the user dragged or double-clicked a column divider
		std::function<pfc::string8(size_t, size_t)> markupText;   // cell text with dim/highlight markers, or "" to draw normally
		typedef CListControlFb2kColorsUIElem<CListControlOwnerData> TBase;
		BEGIN_MSG_MAP_EX(CPaneList)
			MSG_WM_NCHITTEST(OnNcHitTest)
			NOTIFY_CODE_HANDLER_EX(HDN_ENDTRACKW, OnHeaderResized)
			NOTIFY_CODE_HANDLER_EX(HDN_DIVIDERDBLCLICKW, OnHeaderResized)
			NOTIFY_CODE_HANDLER_EX(NM_CUSTOMDRAW, OnHeaderDraw)
			CHAIN_MSG_MAP(TBase)
		END_MSG_MAP()
	protected:
		void RenderSubItemText(t_size item, t_size subItem, const CRect& rc, const CRect& update, CDCHandle dc, bool allowColors) override;
		// Let the parent handle the mouse right next to a divider, so the thin line is easy to grab.
		LRESULT OnNcHitTest(CPoint pt) { if (isSplitterZone && isSplitterZone(pt)) return HTTRANSPARENT; SetMsgHandled(FALSE); return 0; }
		LRESULT OnHeaderDraw(LPNMHDR hdr);
		LRESULT OnHeaderResized(LPNMHDR hdr) {
			if (columnResized) columnResized(reinterpret_cast<LPNMHEADER>(hdr)->iItem);
			SetMsgHandled(FALSE);   // libPPUI still applies the change
			return 0;
		}
		void RenderMarkup(const pfc::string8& text, size_t subItem, const CRect& rc, CDCHandle dc);
	};

	// Repaints the dividers between column titles in foobar2000's dark frame colour (#646464).
	void paintHeaderDividers(HWND header, HDC dc);

	class CFacetBrowser :
		public CWindowImpl<CFacetBrowser>,
		public ui_element_instance,
		private IListControlOwnerDataSource,
		private library_callback_dynamic,
		private playlist_callback_single_impl_base,
		private IGlobalsListener
	{
	public:
		DECLARE_WND_CLASS_EX(L"FacetBrowser", 0, -1);

		CFacetBrowser(ui_element_config::ptr cfg, ui_element_instance_callback::ptr callback);
		~CFacetBrowser();

		static GUID g_get_guid();
		static GUID g_get_subclass() { return ui_element_subclass_media_library_viewers; }
		static void g_get_name(pfc::string_base& out) { out = "Facet Browser"; }
		static const char* g_get_description() { return "Gives quick access to Media Library content with customizable list views."; }
		static ui_element_config::ptr g_get_default_configuration();
		static ui_element_config::ptr makeConfig(const BrowserConfig& cfg);
		static BrowserConfig parseConfig(ui_element_config::ptr cfg);

		void initialize_window(HWND parent);
		HWND get_wnd() override { return m_hWnd; }
		void set_configuration(ui_element_config::ptr cfg) override;
		ui_element_config::ptr get_configuration() override { return makeConfig(m_cfg); }
		void notify(const GUID& what, t_size p1, const void* p2, t_size p2size) override;

		BEGIN_MSG_MAP_EX(CFacetBrowser)
			MSG_WM_CREATE(OnCreate)
			MSG_WM_DESTROY(OnDestroy)
			MSG_WM_SIZE(OnSize)
			MSG_WM_ERASEBKGND(OnEraseBkgnd)
			MSG_WM_PAINT(OnPaint)
			MSG_WM_TIMER(OnTimer)
			MSG_WM_CONTEXTMENU(OnContextMenu)
			MSG_WM_SETFOCUS(OnSetFocus)
			MSG_WM_LBUTTONDOWN(OnLButtonDown)
			MSG_WM_LBUTTONUP(OnLButtonUp)
			MSG_WM_MOUSEMOVE(OnMouseMove)
			MSG_WM_SETCURSOR(OnSetCursor)
			MSG_WM_CTLCOLOREDIT(OnCtlColorEdit)
			MSG_WM_CTLCOLORLISTBOX(OnCtlColorListBox)
			MESSAGE_HANDLER_EX(WM_FB_PANE_COMMAND, OnPaneCommand)
			MESSAGE_HANDLER_EX(WM_FB_COLUMN_RESIZED, OnColumnResized)
			COMMAND_HANDLER_EX(IDC_SOURCE, BN_CLICKED, OnSourceButton)
			COMMAND_HANDLER_EX(IDC_FILTER, BN_CLICKED, OnFilterButton)
			COMMAND_HANDLER_EX(IDC_ACTION, BN_CLICKED, OnActionButton)
			COMMAND_HANDLER_EX(IDC_HELPBTN, BN_CLICKED, OnHelpButton)
			COMMAND_HANDLER_EX(IDC_SEARCH, CBN_EDITCHANGE, OnSearchChange)
			COMMAND_HANDLER_EX(IDC_SEARCH, CBN_SELENDOK, OnSearchPick)
			COMMAND_HANDLER_EX(IDC_SEARCH, CBN_DROPDOWN, OnSearchDropDown)
			NOTIFY_HANDLER_EX(IDC_ACTION, BCN_DROPDOWN, OnActionDropDown)
			NOTIFY_CODE_HANDLER_EX(NM_CUSTOMDRAW, OnButtonCustomDraw)
		ALT_MSG_MAP(2) // search combo: repaint its frame in the border colour
			MESSAGE_HANDLER_EX(WM_PAINT, OnSearchComboPaint)
		ALT_MSG_MAP(1) // search edit
			MSG_WM_KEYDOWN(OnSearchKeyDown)
			MSG_WM_CHAR(OnSearchChar)
			MSG_WM_GETDLGCODE(OnSearchGetDlgCode)
		END_MSG_MAP()

	private:
		friend class FacetBrowserElement;
		enum { IDC_SOURCE = 1001, IDC_FILTER, IDC_SEARCH, IDC_ACTION, IDC_HELPBTN, IDC_PANE0 = 1100 };
		enum { TIMER_REBUILD = 1, TIMER_SEARCH = 2, TIMER_SELECT_ACTION = 3 };
		// Pane shortcuts (Ctrl+X/C/V) run from a posted message, after the pane's own WM_KEYDOWN has returned.
		enum { WM_FB_PANE_COMMAND = WM_APP + 42 };
		// A column divider was dragged (wParam: column, lParam: the pane's list); read the width once libPPUI has applied it.
		enum { WM_FB_COLUMN_RESIZED = WM_APP + 43 };
		LRESULT OnColumnResized(UINT, WPARAM col, LPARAM list);
		static pfc::string8 WidthKey(const PaneConfig& pc, const PaneSpec& spec, size_t col);
		int SavedWidth(size_t pane, size_t col) const;   // pixels at the list's DPI, 0 = not set
		bool PaneKeyDown(size_t pane, UINT ch);
		LRESULT OnPaneCommand(UINT, WPARAM cmd, LPARAM pane);

		struct ArtAdapter : IArtGridHost {
			CFacetBrowser* owner = nullptr; size_t pane = 0;
			bool dead = false;   // set when its pane was removed; the grid may still be unwinding a message
			size_t artCount() override { return dead ? 0 : owner->RowCount(pane); }
			std::vector<pfc::string8> artLines(size_t i) override { if (dead) return {}; return owner->ArtLines(pane, i); }
			metadb_handle_ptr artTrack(size_t i) override { if (dead) return nullptr; return owner->ArtTrack(pane, i); }
			bool artIsSummary(size_t i) override { return !dead && owner->IsSummaryRow(pane, i); }
			void artSelectionChanged() override { if (!dead) owner->OnPaneSelectionChanged(pane); }
			void artAction(size_t) override { if (!dead) owner->RunToolbarAction(pane); }
			void artContextMenu(HWND w, CPoint pt, bool onHeader) override { if (!dead) owner->ShowPaneMenu(pane, w, pt, onHeader); }
			void artHeaderClick(size_t col) override { if (!dead) owner->OnHeaderClick(pane, col); }
			ui_element_instance_callback::ptr artCallback() override { return owner->m_callback; }
			bool artKeyDown(UINT ch) override { return !dead && owner->PaneKeyDown(pane, ch); }
		};

		struct Pane {
			std::unique_ptr<CPaneList> list;
			std::unique_ptr<ArtAdapter> art;
			std::unique_ptr<CArtGrid> grid;
			PaneSpec spec;
			std::vector<FacetItem> items;
			FacetItem total;
			std::vector<uint32_t> input, output;
			std::vector<pfc::string8> selected;   // value keys; empty = everything
			CRect rect;
			CWindow wnd() const { return list ? CWindow(*list) : CWindow(grid ? grid->m_hWnd : NULL); }
		};
		// Panes removed while one of their own messages (menu, click) is still on the
		// call stack. Their windows are destroyed at once, the C++ objects are freed
		// later from WM_FB_FREE_PANES once that message has returned.
		std::vector<Pane> m_graveyard;
		// ATL keeps m_hWnd set until the window's outermost message has returned, so an
		// object is only safe to delete once its m_hWnd is NULL. Checked on a timer, so a
		// modal loop (message box, menu) inside that message can't free it early.
		static constexpr UINT_PTR kFreePanesTimer = 0xFB01;
		void FreeDeadPanes();
		LRESULT OnButtonCustomDraw(LPNMHDR hdr);
		COLORREF LineColor() const;

		// window
		int OnCreate(LPCREATESTRUCT);
		void OnDestroy();
		void OnSize(UINT, CSize) { Layout(); }
		BOOL OnEraseBkgnd(CDCHandle dc);
		void OnPaint(CDCHandle);
		void OnTimer(UINT_PTR id);
		void OnContextMenu(CWindow wnd, CPoint pt);
		void OnSetFocus(CWindow);
		void OnLButtonDown(UINT, CPoint pt);
		void OnLButtonUp(UINT, CPoint);
		void OnMouseMove(UINT, CPoint pt);
		BOOL OnSetCursor(CWindow wnd, UINT hit, UINT msg);
		HBRUSH OnCtlColorEdit(CDCHandle dc, CEdit);
		HBRUSH OnCtlColorListBox(CDCHandle dc, CListBox);
		int SearchDropHeight(int editH);
		void OnSearchDropDown(UINT, int, CWindow);

		// toolbar
		void OnSourceButton(UINT, int, CWindow);
		void OnFilterButton(UINT, int, CWindow);
		void OnActionButton(UINT, int, CWindow) { RunToolbarAction(m_lastPane); }
		LRESULT OnActionDropDown(LPNMHDR);
		void OnHelpButton(UINT, int, CWindow);
		void OnSearchChange(UINT, int, CWindow) { SetTimer(TIMER_SEARCH, 300); }
		void OnSearchPick(UINT, int, CWindow);
		void OnSearchKeyDown(UINT ch, UINT rep, UINT flags);
		void OnSearchChar(UINT ch, UINT rep, UINT flags);
		// Claim Enter and Esc, or the host's dialog navigation swallows them before the edit sees them.
		UINT OnSearchGetDlgCode(LPMSG msg) {
			UINT code = (UINT)m_searchEdit.DefWindowProc();
			if (msg && msg->message == WM_KEYDOWN && (msg->wParam == VK_RETURN || msg->wParam == VK_ESCAPE)) code |= DLGC_WANTMESSAGE;
			return code;
		}
		void UpdateToolbarText();
		void FillSearchHistory();
		pfc::string8 SearchText() const;
		void ShowToolbarMenu(CPoint pt);

		// building
		void CreatePanes();
		void DestroyPanes();
		void ApplyConfig();
		void ApplyFonts();
		void ApplyColors();
		void Layout();
		int SplitterAt(CPoint pt) const;
		std::vector<PaneSpec> ResolveSpecs() const;
		Source EffectiveSource() const;
		void RebuildSource();
		void Cascade(size_t fromPane, bool regroupFirst = true);
		void RefreshPaneUI(size_t i);
		void UpdateColumns(size_t i);
		void FitKeyColumns(size_t i);
		void ReadSelectionFromUI(size_t i);
		void OnPaneSelectionChanged(size_t i);
		void OnHeaderClick(size_t pane, size_t col);
		metadb_handle_list PaneTracks(size_t i) const;
		size_t PaneIndex(const void* ctx) const;
		size_t PaneIndexFromWnd(HWND wnd) const;

		// rows: the summary item (if shown) comes first
		size_t RowCount(size_t pane) const;
		bool IsSummaryRow(size_t pane, size_t row) const;
		const FacetItem* RowItem(size_t pane, size_t row) const;
		pfc::string8 CellText(size_t pane, size_t row, size_t col) const;
		pfc::string8 SummaryLabel(size_t pane) const;
		std::vector<pfc::string8> ArtLines(size_t pane, size_t row);
		metadb_handle_ptr ArtTrack(size_t pane, size_t row);

		// menus
		void ShowPaneMenu(size_t pane, CWindow wnd, CPoint pt, bool onHeader);
		void BuildHeaderMenu(size_t pane, CMenu& menu, CMenuDescriptionHybrid& desc);
		bool RunHeaderCommand(size_t pane, int cmd);

		// panes
		void InsertPane(size_t at, const PaneConfig& p);
		void RemovePane(size_t i);
		void CopyPane(size_t i);
		bool PastePane(size_t at);

		// actions
		void RunToolbarAction(size_t pane);
		void RunAction(Action a, metadb_handle_list tracks, size_t pane);
		void SortTracks(metadb_handle_list& tracks);
		pfc::string8 SelectionLabel(size_t pane) const;
		pfc::string8 SelectionQuery(size_t upToPane) const;

		// IListControlOwnerDataSource
		size_t listGetItemCount(ctx_t) override;
		pfc::string8 listGetSubItemText(ctx_t, size_t item, size_t sub) override;
		void listSelChanged(ctx_t) override;
		void listItemAction(ctx_t, size_t item) override;
		void listColumnHeaderClick(ctx_t, size_t sub) override;
		bool listAllowTypeFindHere(ctx_t, size_t, size_t sub) override { return sub == 0; }
		uint32_t listQueryDragDropTypes(ctx_t) const override { return 2; /* dragDrop_external */ }
		pfc::com_ptr_t<IDataObject> listMakeDataObject(ctx_t ctx) override;
		DWORD listDragDropSourceEffects(ctx_t) override { return DROPEFFECT_COPY; }
		bool listKeyDown(ctx_t ctx, UINT ch, UINT, UINT) override { return PaneKeyDown(PaneIndex(ctx), ch); }

		// notifications
		void on_items_added(metadb_handle_list_cref) override { if (EffectiveSource() == Source::Library) ScheduleRebuild(); }
		void on_items_removed(metadb_handle_list_cref) override { if (EffectiveSource() == Source::Library) ScheduleRebuild(); }
		void on_items_modified(metadb_handle_list_cref) override { if (EffectiveSource() == Source::Library) ScheduleRebuild(); }
		void on_items_added(t_size, metadb_handle_list_cref, const bit_array&) override { PlaylistChanged(); }
		void on_items_removed(const bit_array&, t_size, t_size) override { PlaylistChanged(); }
		void on_items_modified(const bit_array&) override { PlaylistChanged(); }
		void on_items_replaced(const bit_array&, const pfc::list_base_const_t<playlist_callback::t_on_items_replaced_entry>&) override { PlaylistChanged(); }
		void on_playlist_switch() override { if (m_cfg.source != Source::Library && !m_sending) ScheduleRebuild(); }
		void PlaylistChanged() { if (EffectiveSource() == Source::Playlist && !m_sending) ScheduleRebuild(); }
		void ScheduleRebuild() { if (m_hWnd) SetTimer(TIMER_REBUILD, 400); }
		void globalsChanged(const GlobalsChange& change) override;
		void CreatePanesKeepSelection();
		bool m_modelHasCreated = false;   // the model was built with file creation times
		size_t m_selectActionPane = 0;

		ui_element_instance_callback::ptr m_callback;
		BrowserConfig m_cfg;
		FacetModel m_model;
		std::vector<Pane> m_panes;
		CButton m_btnSource, m_btnFilter, m_btnAction, m_btnHelp;
		CComboBox m_search;
		CContainedWindowT<CEdit> m_searchEdit;
		CContainedWindow m_searchCombo;
		LRESULT OnSearchComboPaint(UINT msg, WPARAM wp, LPARAM lp);
		bool InSplitterZone(CPoint screenPt) const;
		int m_grab = 3;
		fb2k::CDarkModeHooks m_dark;
		CBrush m_editBrush;
		pfc::string8 m_error;
		size_t m_lastPane = 0;
		bool m_updating = false;
		bool m_sending = false;
		int m_drag = -1;
		CPoint m_dragStart;
		std::vector<uint32_t> m_dragWeights;
		int m_toolbarH = 0, m_gap = 4, m_border = 1;
		CRect m_frameRect;
		CRect m_toolbarRect;
	};
}
