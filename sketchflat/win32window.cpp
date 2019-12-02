//-----------------------------------------------------------------------------
// Copyright 2008 Jonathan Westhues
//
// This file is part of SketchFlat.
// 
// SketchFlat is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
// 
// SketchFlat is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
// 
// You should have received a copy of the GNU General Public License
// along with SketchFlat.  If not, see <http://www.gnu.org/licenses/>.
//------
//
// Win32-specific user interface stuff. In general I will try to keep this
// separate from the rest of program, so that a port in the future would not
// be too horrible. I greatly prefer a native Win32 look, so I don't want
// to use a portability toolkit, even though that would make life much easier.
//
// This also contains WinMain, since that is Win32-specific.
//
// Jonathan Westhues, April 2007
//-----------------------------------------------------------------------------
#define CREATE_COLOR_TABLES
#include "sketchflat.h"

void uiEnableMenuById(int id, BOOL enabled);

extern HINSTANCE   Instance;
extern HWND        MainWindow;

static HFONT    DefaultLightFont;
static HFONT    HugeFixedFont;
static HFONT    LargeFixedFont;
static HFONT    MediumFixedFont;
static HFONT    SmallFixedFont;
static HBRUSH   YellowBrush;
static HBRUSH   PinkBrush;
static HBRUSH   GreenBrush;
static HBRUSH   VioletBrush;
static HBRUSH   BlackBrush;

static HPEN     SolidPens[MAX_COLORS];
static HPEN     DashedPens[MAX_COLORS];
static HBRUSH   SolidBrushes[MAX_COLORS];

// Controls in the main window, that are always shown;
#define RIGHT_PALETTE_WIDTH 210
static HWND  MainTab;
static HWND  MainTabForceColor;
#define STATUS_BAR_HEIGHT 30
static HWND  MainStatusBar;
static char  MainStatusBarText[MAX_STRING];
// For sketch mode only:
static HWND  MeasurementArea;
BOOL  EditingLayerListLabel;
static HWND  LayerList;
static HWND  LayerInsertBefore;
static HWND  LayerInsertAfter;
static HWND  LayerDelete;
static HWND  AssumedParameters;
static HWND  InconsistentConstraints;
static HWND  ConsistencyStatus;
static int   ConsistencyStatusColor;
static HWND  SketchModeLabel[3];
// For derive mode only:
static HWND  DerivedItemsList;
static HWND  HideAllDerivedItems;
static HWND  ShowAllDerivedItems;
static HWND  ShowPointsInDerivedMode;
// And some record-keeping for the items we've placed in that treeview.
static HTREEITEM ParentItems[MAX_DERIVED_ELEMENTS];
static int DerivedItemCount;
// We use the text box to edit a dimension on the drawing, by moving it
// to the appropriate point on the actual drawing.
static HWND  TextEntryBox;

// Keep a record of the most recent mouse activity.
static struct {
    int     x;
    int     y;
} LastMousePos;

// This is a back-buffer that's compatible with our paint DC; we draw
// the sketch into BackDc, then blit it over in a single operation,
// no flicker.
static HDC BackDc;
static HBITMAP BackBitmap;

HMENU MakeMainWindowMenus(void);
void ProcessMenu(int id);

//-----------------------------------------------------------------------------
// Set a control to use the pretty font that I like.
//-----------------------------------------------------------------------------
void NiceFont(HWND x)
{
    SendMessage(x, WM_SETFONT, (WPARAM)DefaultLightFont, TRUE);
}

//-----------------------------------------------------------------------------
// Create a more attractive font than the Windows default, for NiceFont()
// to use, plus various different brushes and pens we will need.
//-----------------------------------------------------------------------------
static HFONT Lucida(int size)
{
    return CreateFont(size, 0, 0, 0, FW_REGULAR, FALSE, FALSE, FALSE,
        ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY,
        FF_DONTCARE, "Lucida Console");
}
static void CreateFontsBrushesPens(void)
{
    // A proportional-width font, for general UI use.
    DefaultLightFont = CreateFont(16, 0, 0, 0, FW_REGULAR, FALSE, FALSE, FALSE,
        ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY,
        FF_DONTCARE, "Tahoma");

    HugeFixedFont = CreateFont(18, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
        ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY,
        FF_DONTCARE, "Lucida Console");

    // The font that we use on the drawing, and for the dimension entry
    // text box.
    LargeFixedFont = Lucida(14);

    // For the assumed parameter and inconsistent constraint lists.
    MediumFixedFont = Lucida(13);

    // The font that we use for the measurements.
    SmallFixedFont = Lucida(11);

    LOGBRUSH lb;
    lb.lbStyle = BS_SOLID;
    lb.lbColor = RGB(255, 150, 150);
    PinkBrush = CreateBrushIndirect(&lb);
    lb.lbColor = RGB(255, 255, 120);
    YellowBrush = CreateBrushIndirect(&lb);
    lb.lbColor = RGB(150, 255, 150);
    GreenBrush = CreateBrushIndirect(&lb);
    lb.lbColor = RGB(255, 150, 255);
    VioletBrush = CreateBrushIndirect(&lb);
    lb.lbColor = RGB(0, 0, 0);
    BlackBrush = CreateBrushIndirect(&lb);

    int i;
    for(i = 0; i < MAX_COLORS; i++) {
        COLORREF c = RGB(Colors[i].r, Colors[i].g, Colors[i].b);
        SolidPens[i]    = CreatePen(PS_SOLID, 1, c);
        DashedPens[i]   = CreatePen(PS_DASH, 1, c);
        SolidBrushes[i] = CreateSolidBrush(c);
    }
}

//-----------------------------------------------------------------------------
// Map the X and Y coordinates (which we get relative to the main window's
// top left corner) so that they are relative to the center of the available
// drawing area. Also flip the sign of Y, for standard xy (vs. ij) look.
//-----------------------------------------------------------------------------
static int MapX(int x)
{
    RECT r;
    GetClientRect(MainWindow, &r);
    return x - (r.right - r.left - RIGHT_PALETTE_WIDTH)/2;
}
static int MapY(int y)
{
    RECT r;
    GetClientRect(MainWindow, &r);
    return (r.bottom - r.top - STATUS_BAR_HEIGHT)/2 - y;
}
// And their inverses, for when we go the other way
static int UnmapX(int x)
{
    RECT r;
    GetClientRect(MainWindow, &r);
    return x + (r.right - r.left - RIGHT_PALETTE_WIDTH)/2;
}
static int UnmapY(int y)
{
    RECT r;
    GetClientRect(MainWindow, &r);
    return (r.bottom - r.top - STATUS_BAR_HEIGHT)/2 - y;
}

//-----------------------------------------------------------------------------
// Resize the controls in the main window, to fit the main window that has
// just been resized.
//-----------------------------------------------------------------------------
static void MainWindowResized(void)
{
    RECT r;
    GetClientRect(MainWindow, &r);

    // This one knows how to size itself
    SendMessage(MainStatusBar, WM_SIZE, 0, 0);
    SetWindowPos(MainStatusBar, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE);

    HDWP hDwp = BeginDeferWindowPos(20);

    int p = r.right - RIGHT_PALETTE_WIDTH;

    HWND after = MainStatusBar;

    int y = r.top;

    // In the sketch tab:

    y += 40;
    hDwp = DeferWindowPos(hDwp, LayerList, after,
        p + 10, y, RIGHT_PALETTE_WIDTH - 20, 100,
        SWP_NOACTIVATE);

    y += 105;
    hDwp = DeferWindowPos(hDwp, LayerInsertBefore, after,
        p + 10, r.top + y, 60, 20,
        SWP_NOACTIVATE);
    hDwp = DeferWindowPos(hDwp, LayerInsertAfter, after,
        p + 75, r.top + y, 60, 20,
        SWP_NOACTIVATE);
    hDwp = DeferWindowPos(hDwp, LayerDelete, after,
        p + 140, r.top + y, 60, 20,
        SWP_NOACTIVATE);

    y += 30;
    hDwp = DeferWindowPos(hDwp, SketchModeLabel[0], after,
        p + 10, r.top + y, RIGHT_PALETTE_WIDTH - 20, 20,
        SWP_NOACTIVATE);

    y += 20;
    hDwp = DeferWindowPos(hDwp, MeasurementArea, after,
        p + 10, y, RIGHT_PALETTE_WIDTH - 20, 100,
        SWP_NOACTIVATE);

    y += 120;
    hDwp = DeferWindowPos(hDwp, ConsistencyStatus, after,
        p +  9, y, RIGHT_PALETTE_WIDTH - 20, 22,
        SWP_NOACTIVATE);

    y += 35;
    hDwp = DeferWindowPos(hDwp, SketchModeLabel[1], after,
        p + 10, y, RIGHT_PALETTE_WIDTH - 20, 20,
        SWP_NOACTIVATE);

    y += 20;
    hDwp = DeferWindowPos(hDwp, AssumedParameters, after,
        p + 10, y, RIGHT_PALETTE_WIDTH - 20, 95,
        SWP_NOACTIVATE);

    y += 100;
    hDwp = DeferWindowPos(hDwp, SketchModeLabel[2], after,
        p + 10, y, RIGHT_PALETTE_WIDTH - 20, 20,
        SWP_NOACTIVATE);

    y += 20;
    hDwp = DeferWindowPos(hDwp, InconsistentConstraints, after,
        p + 10, y, RIGHT_PALETTE_WIDTH - 20, 69,
        SWP_NOACTIVATE);

    // In the derive tab:
    y = r.top;

    y += 40;
    hDwp = DeferWindowPos(hDwp, DerivedItemsList, after,
        p + 10, y, RIGHT_PALETTE_WIDTH - 20, 300,
        SWP_NOACTIVATE);

    y += 305;
    hDwp = DeferWindowPos(hDwp, ShowAllDerivedItems, HWND_BOTTOM,
        p + 10, y, 90, 20,
        SWP_NOACTIVATE);
    hDwp = DeferWindowPos(hDwp, HideAllDerivedItems, HWND_BOTTOM,
        p + 110, y, 90, 20,
        SWP_NOACTIVATE);

    y += 30;
    hDwp = DeferWindowPos(hDwp, ShowPointsInDerivedMode, HWND_BOTTOM,
        p + 15, y, 200, 20,
        SWP_NOACTIVATE);

    // Tab's background image
    hDwp = DeferWindowPos(hDwp, MainTabForceColor, HWND_BOTTOM,
        p, r.top + 25, r.right, r.bottom,
        SWP_NOACTIVATE);
    // The tab itself, which we want to place at the bottom of the Z order.
    hDwp = DeferWindowPos(hDwp, MainTab, HWND_BOTTOM,
        p, r.top, r.right, r.bottom,
        SWP_NOACTIVATE);

    EndDeferWindowPos(hDwp);
}

//-----------------------------------------------------------------------------
// The software has two major modes: sketch mode, where you draw things using
// the constraint solver, and derive mode, where you derive other things
// (e.g. offset curves) from the things that you drew in sketch mode. This
// tells us which one we're in.
//-----------------------------------------------------------------------------
BOOL uiInSketchMode(void)
{
    return (TabCtrl_GetCurSel(MainTab) == 0);
}
static void UpdateAfterTabChange(void)
{
    int i;

    int showDerive, showSketch;
    BOOL enableDerive, enableSketch;

    if(uiInSketchMode()) {
        showSketch = SW_SHOW;
        showDerive = SW_HIDE;
        enableSketch = TRUE;
        enableDerive = FALSE;
    } else {
        showSketch = SW_HIDE;
        showDerive = SW_SHOW;
        enableSketch = FALSE;
        enableDerive = TRUE;
    }

    // Controls in sketch mode
    ShowWindow(MeasurementArea, showSketch);
    ShowWindow(LayerList, showSketch);
    ShowWindow(LayerInsertBefore, showSketch);
    ShowWindow(LayerInsertAfter, showSketch);
    ShowWindow(LayerDelete, showSketch);
    ShowWindow(ConsistencyStatus, showSketch);
    ShowWindow(AssumedParameters, showSketch);
    ShowWindow(InconsistentConstraints, showSketch);
    for(i = 0; i < 3; i++) {
        ShowWindow(SketchModeLabel[i], showSketch);
    }

    // Controls in derive mode
    ShowWindow(DerivedItemsList, showDerive);
    ShowWindow(ShowAllDerivedItems, showDerive);
    ShowWindow(HideAllDerivedItems, showDerive);
    ShowWindow(ShowPointsInDerivedMode, showDerive);

    // Menus in sketch mode
    for(i = MNU_DRAW_FIRST; i <= MNU_DRAW_LAST; i++) {
        uiEnableMenuById(i, enableSketch);
    }
    for(i = MNU_CONSTR_FIRST; i <= MNU_CONSTR_LAST; i++) {
        uiEnableMenuById(i, enableSketch);
    }
    uiEnableMenuById(MNU_EDIT_DELETE_FROM_SKETCH, enableSketch);
    uiEnableMenuById(MNU_EDIT_UNSELECT_ALL, enableSketch);
    uiEnableMenuById(MNU_EDIT_BRING_TO_LAYER, enableSketch);
    uiEnableMenuById(MNU_VIEW_SHOW_CONSTRAINTS, enableSketch);
    uiEnableMenuById(MNU_VIEW_SHOW_DATUM_ITEMS, enableSketch);

    // Menus in derive mode
    for(i = MNU_DERIVE_FIRST; i <= MNU_DERIVE_LAST; i++) {
        uiEnableMenuById(i, enableDerive);
    }
    for(i = MNU_EXPORT_FIRST; i <= MNU_EXPORT_LAST; i++) {
        uiEnableMenuById(i, enableDerive);
    }
    uiEnableMenuById(MNU_EDIT_DELETE_DERIVED, enableDerive);
    uiEnableMenuById(MNU_EDIT_UNSELECT_ALL_POINTS, enableDerive);

    if(uiInSketchMode()) {
        SwitchToSketchMode();
    } else {
        SwitchToDeriveMode();
    }
}
//-----------------------------------------------------------------------------
// And this forces us back to sketch mode.
//-----------------------------------------------------------------------------
void uiForceSketchMode(void)
{
    TabCtrl_SetCurSel(MainTab, 0);
    UpdateAfterTabChange();
}

//-----------------------------------------------------------------------------
// Create the controls that go in the main window. We have a status bar at
// the bottom, a tab at the side, for a sort of palette, and the textbox
// that is usually hidden but sometimes used to enter dimensions.
//-----------------------------------------------------------------------------
void MakeMainWindowControls(void)
{
    int i;
    // The tab control that lets people view the current drawing as a
    // list of entities/constraints/whatever, instead of graphically.

    MainTab = CreateWindowEx(0, WC_TABCONTROL, "Main Tabs",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_CLIPSIBLINGS,
        0, 0, 0, 0, MainWindow, NULL, Instance, NULL);
    NiceFont(MainTab);

    // Under XP, the fancy gradient background causes trouble, so get rid
    // of that.
    MainTabForceColor = CreateWindowEx(0, WC_STATIC, "",
        WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS,
        0, 0, 0, 0, MainWindow, NULL, Instance, NULL);

    TCITEM t;
    t.dwState = 0;
    t.dwStateMask = 0;
    t.mask = TCIF_TEXT | TCIF_PARAM;

    t.pszText = (char*)"Sketch"; t.lParam = 0;
    if(TabCtrl_InsertItem(MainTab, 0, &t) < 0)
        oops();
    t.pszText = (char*)"Derive and Export"; t.lParam = 1;
    if(TabCtrl_InsertItem(MainTab, 1, &t) < 0)
        oops();

    // Status bar at the bottom, main function is to tell the user what
    // the mouse activity means now.
    MainStatusBar = CreateWindowEx(0, STATUSCLASSNAME, NULL,
        SBARS_SIZEGRIP | WS_CHILD | WS_VISIBLE,
            0, 0, 0, 0, MainWindow, NULL, Instance, NULL);
    NiceFont(MainStatusBar);

    // Text entry box that is usually hidden, used to edit dimensions
    // on-screen.
    TextEntryBox = CreateWindowEx(WS_EX_CLIENTEDGE, WC_EDIT, "",
        WS_CHILD | ES_AUTOHSCROLL | WS_TABSTOP | WS_CLIPSIBLINGS,
        50, 50, 100, 21, MainWindow, NULL, Instance, NULL);
    SendMessage(TextEntryBox, WM_SETFONT, (WPARAM)LargeFixedFont, TRUE);

    // A list of layers (practically, independently editable sketches).
    // This is a listview, with checkboxes for visibility.
    LayerList = CreateWindowEx(WS_EX_CLIENTEDGE, WC_LISTVIEW, "",
        WS_CHILD | LVS_SMALLICON | LVS_NOSORTHEADER | LVS_EDITLABELS |
            LVS_SINGLESEL | LVS_SHOWSELALWAYS | WS_TABSTOP | WS_CLIPSIBLINGS, 
        0, 0, 0, 0, MainWindow, NULL, Instance, NULL);
    ListView_SetExtendedListViewStyle(LayerList, 
        LVS_EX_CHECKBOXES | LVS_EX_FULLROWSELECT);
    NiceFont(LayerList);

    ListView_SetColumnWidth(LayerList, 0, RIGHT_PALETTE_WIDTH - 20);

    // And buttons for the layers/sketches, to insert and delete them.
    LayerInsertBefore = CreateWindowEx(0, WC_BUTTON, "Before",
        WS_CHILD | WS_TABSTOP | WS_CLIPSIBLINGS | WS_VISIBLE,
        0, 0, 0, 0, MainWindow, NULL, Instance, NULL); 
    NiceFont(LayerInsertBefore);
    LayerInsertAfter = CreateWindowEx(0, WC_BUTTON, "After",
        WS_CHILD | WS_TABSTOP | WS_CLIPSIBLINGS | WS_VISIBLE,
        0, 0, 0, 0, MainWindow, NULL, Instance, NULL); 
    NiceFont(LayerInsertAfter);
    LayerDelete = CreateWindowEx(0, WC_BUTTON, "Delete",
        WS_CHILD | WS_TABSTOP | WS_CLIPSIBLINGS | WS_VISIBLE,
        0, 0, 0, 0, MainWindow, NULL, Instance, NULL); 
    NiceFont(LayerDelete);

    // A read-only edit control in which we display measurements on the
    // selected items (e.g., if two points are selected then the distance
    // between them), in CAD mode only.
    MeasurementArea = CreateWindowEx(WS_EX_CLIENTEDGE, WC_EDIT, "",
        WS_CHILD | ES_AUTOHSCROLL | WS_TABSTOP | WS_CLIPSIBLINGS |
        ES_READONLY | ES_MULTILINE,
        0, 0, 0, 0, MainWindow, NULL, Instance, NULL);
    SendMessage(MeasurementArea, WM_SETFONT, (WPARAM)SmallFixedFont, TRUE);

    // A list of parameters that we have assumed during the solution
    // process, either because the problem is underconstrained or
    // because it is too complicated (too many simultaneous eqns need
    // to be solved) for us.
    AssumedParameters = CreateWindowEx(WS_EX_CLIENTEDGE, WC_LISTBOX, "",
        WS_CHILD | WS_TABSTOP | WS_CLIPSIBLINGS | WS_VISIBLE | WS_VSCROLL |
        LBS_NOTIFY,
        0, 0, 0, 0, MainWindow, NULL, Instance, NULL);
    SendMessage(AssumedParameters, WM_SETFONT, (WPARAM)MediumFixedFont, TRUE);

    // A list of constraints that, if removed, would bring the system back
    // to consistency.
    InconsistentConstraints = CreateWindowEx(WS_EX_CLIENTEDGE, WC_LISTBOX, "",
        WS_CHILD | WS_TABSTOP | WS_CLIPSIBLINGS | WS_VISIBLE | WS_VSCROLL |
        LBS_NOTIFY,
        0, 0, 0, 0, MainWindow, NULL, Instance, NULL);
    SendMessage(InconsistentConstraints, WM_SETFONT, (WPARAM)MediumFixedFont,
        TRUE);

    // Displays whether we're exactly determined, overdetermined,
    // underdetermined, etc.
    ConsistencyStatus = CreateWindowEx(WS_EX_CLIENTEDGE, WC_STATIC,
        " Not yet solved.",
        WS_CHILD | WS_CLIPSIBLINGS | WS_VISIBLE,
        0, 0, 0, 0, MainWindow, NULL, Instance, NULL);
    ConsistencyStatusColor = BK_GREY;
    NiceFont(ConsistencyStatus);

    for(i = 0; i < 3; i++) {
        const char *s;
        switch(i) {
            case 0: s = "Selected Items:";              break;
            case 1: s = "Assumed Parameters:";          break;
            case 2: s = "Inconsistent Constraints:";    break;
        }
        SketchModeLabel[i] = CreateWindowEx(0, WC_STATIC, s,
            WS_CHILD | WS_CLIPSIBLINGS | WS_VISIBLE,
            0, 0, 0, 0, MainWindow, NULL, Instance, NULL);
        NiceFont(SketchModeLabel[i]);
    }

    // Derive mode: the list of derived items, which we will show as a tree
    // view.
    DerivedItemsList = CreateWindowEx(WS_EX_CLIENTEDGE, WC_TREEVIEW, "",
        WS_CHILD | WS_CLIPSIBLINGS |
        TVS_DISABLEDRAGDROP | TVS_HASBUTTONS | TVS_LINESATROOT | TVS_HASLINES,
        0, 0, 0, 0, MainWindow, NULL, Instance, NULL);
    NiceFont(DerivedItemsList);

    // Two buttons to show/hide all derived items.
    ShowAllDerivedItems = CreateWindowEx(0, WC_BUTTON, "Show All",
        WS_CHILD | WS_TABSTOP | WS_CLIPSIBLINGS | WS_VISIBLE,
        0, 0, 0, 0, MainWindow, NULL, Instance, NULL); 
    NiceFont(ShowAllDerivedItems);
    HideAllDerivedItems = CreateWindowEx(0, WC_BUTTON, "Hide All",
        WS_CHILD | WS_TABSTOP | WS_CLIPSIBLINGS | WS_VISIBLE,
        0, 0, 0, 0, MainWindow, NULL, Instance, NULL); 
    NiceFont(HideAllDerivedItems);

    // A checkbox to determine whether the points are shown.
    ShowPointsInDerivedMode = CreateWindowEx(0, WC_BUTTON,
        "Show Points From Sketch",
        WS_CHILD | BS_AUTOCHECKBOX | WS_TABSTOP | WS_VISIBLE,
        0, 0, 0, 0, MainWindow, NULL, Instance, NULL);
    NiceFont(ShowPointsInDerivedMode);


    UpdateAfterTabChange();
    MainWindowResized();
}

//-----------------------------------------------------------------------------
// To set the textbox where we display measurement-type information.
//-----------------------------------------------------------------------------
void uiSetMeasurementAreaText(const char *str)
{
    SendMessage(MeasurementArea, WM_SETTEXT, 0, (LPARAM)str);
}

//-----------------------------------------------------------------------------
// To set the static control that displays how well-constrained the sketch
// is right now.
//-----------------------------------------------------------------------------
void uiSetConsistencyStatusText(const char *str, int bk)
{
    ConsistencyStatusColor = bk;
    SendMessage(ConsistencyStatus, WM_SETTEXT, 0, (LPARAM)str);
}

//-----------------------------------------------------------------------------
// To set the list of assumptions that we had to make to solve.
//-----------------------------------------------------------------------------
void uiClearAssumptionsList(void)
{
    SendMessage(AssumedParameters, LB_RESETCONTENT, 0, 0);
}
void uiAddToAssumptionsList(char *str)
{
    SendMessage(AssumedParameters, LB_ADDSTRING, 0, (LPARAM)str);
}

//-----------------------------------------------------------------------------
// To set the list of constraints that if we removed them would make our
// inconsistent sketch consistent.
//-----------------------------------------------------------------------------
void uiClearConstraintsList(void)
{
    SendMessage(InconsistentConstraints, LB_RESETCONTENT, 0, 0);
}
void uiAddToConstraintsList(char *str)
{
    SendMessage(InconsistentConstraints, LB_ADDSTRING, 0, (LPARAM)str);
}


//-----------------------------------------------------------------------------
// For the list of layers, to edit this and get information on what the user
// did with it.
//-----------------------------------------------------------------------------
int uiGetLayerListSelection(void)
{
    int n = ListView_GetItemCount(LayerList);
    int i;
    for(i = 0; i < n; i++) {
        if(ListView_GetItemState(LayerList, i, LVIS_SELECTED)) {
            return i;
        }
    }

    return -1;
}
void uiAddToLayerList(BOOL shown, char *str)
{
    int p = ListView_GetItemCount(LayerList);

    LVITEM lvi;
    lvi.mask        = LVIF_TEXT | LVIF_PARAM | LVIF_STATE;
    lvi.state = INDEXTOSTATEIMAGEMASK(1);
    lvi.stateMask = LVIS_STATEIMAGEMASK;
    lvi.iItem       = p;
    lvi.iSubItem    = 0;
    lvi.pszText     = str;
    lvi.lParam      = 0;
    ListView_InsertItem(LayerList, &lvi);

    ListView_SetCheckState(LayerList, p, shown);
}
void uiClearLayerList(void)
{
    ListView_DeleteAllItems(LayerList);
}
void uiSelectInLayerList(int p)
{
    ListView_SetItemState(LayerList, p, LVIS_SELECTED, LVIS_SELECTED);
    ListView_EnsureVisible(LayerList, p, FALSE);
}

//-----------------------------------------------------------------------------
// For the list (in treeview) of derived objects. Each derived gets one
// top-level item to identify it, plus zero to three subitems to describe
// it in the list.
//-----------------------------------------------------------------------------
void uiAddToDerivedItemsList(int i, char *str,
                                        char *subA, char *subB, char *subC)
{
    if(DerivedItemCount >= MAX_DERIVED_ELEMENTS) oops();

    // Count the children, since the parent needs to know that.
    int children = 0;
    if(subA && subA[0] != '\0') children++;
    if(subB && subB[0] != '\0') children++;
    if(subC && subC[0] != '\0') children++;

    TVINSERTSTRUCT tvis;
    memset(&tvis, 0, sizeof(tvis));

    tvis.hParent = TVI_ROOT;
    tvis.hInsertAfter = TVI_LAST;

    TVITEMEX *tvix = &(tvis.itemex);
    tvix->mask = TVIF_TEXT | TVIF_PARAM | TVIF_STATE;
    tvix->pszText = str;
    tvix->cChildren = children;
    tvix->iIntegral = 1;
    tvix->state = 0;
    tvix->stateMask = TVIS_SELECTED;
    tvix->lParam = DerivedItemCount;

    HTREEITEM p = TreeView_InsertItem(DerivedItemsList, &tvis);
    if(!p) oops();
    ParentItems[DerivedItemCount] = p;

    int j;
    for(j = 0; j < 3; j++) {
        char *s;
        if(j == 0) s = subA;
        else if(j == 1) s = subB;
        else if(j == 2) s = subC;
        
        // They don't have to provide all subitems.
        if(!s || s[0] == '\0') continue; 

        tvis.hParent = p;
        tvis.hInsertAfter = TVI_LAST;

        tvix = &(tvis.itemex);
        tvix->mask = TVIF_TEXT | TVIF_STATE | TVIF_STATE;
        tvix->pszText = s;
        tvix->cChildren = 0;
        tvix->iIntegral = 1;
        tvix->state = 0;
        tvix->stateMask = TVIS_SELECTED;
        tvix->lParam = DerivedItemCount;

        HTREEITEM c = TreeView_InsertItem(DerivedItemsList, &tvis);
    }

    DerivedItemCount++;
}
void uiClearDerivedItemsList(void)
{
    TreeView_DeleteAllItems(DerivedItemsList);
    DerivedItemCount = 0;
}
void uiBoldDerivedItem(int i, BOOL bold)
{
    TVITEMEX tvx;
    
    tvx.mask = TVIF_HANDLE | TVIF_STATE;
    tvx.hItem = ParentItems[i];
    tvx.stateMask = TVIS_BOLD;
    tvx.state = bold ? TVIS_BOLD : 0;

    TreeView_SetItem(DerivedItemsList, &tvx);
}
int uiGetSelectedDerivedItem(void)
{
    int i;
    for(i = 0; i < DerivedItemCount; i++) {
        if(TreeView_GetItemState(DerivedItemsList,
            ParentItems[i], TVIS_SELECTED) & TVIS_SELECTED)
        {
            return i;
        }
    }
    return -1;
}

//-----------------------------------------------------------------------------
// For the checkbox that decides whether we show the points or not in
// derive mode.
//-----------------------------------------------------------------------------
BOOL uiPointsShownInDeriveMode(void)
{
    return !!(
        SendMessage(ShowPointsInDerivedMode, BM_GETSTATE, 0, 0) &
            BST_CHECKED);
}

//-----------------------------------------------------------------------------
// For the text entry box that is shown on the drawing to enter dimensions.
//-----------------------------------------------------------------------------
void uiShowTextEntryBoxAt(char *initial, int x, int y)
{
    int mx = UnmapX(x);
    int my = UnmapY(y);

    MoveWindow(TextEntryBox, mx, my, 100, 21, TRUE);
    ShowWindow(TextEntryBox, SW_SHOW);

    SendMessage(TextEntryBox, WM_SETTEXT, 0, (LPARAM)initial);
    SendMessage(TextEntryBox, EM_SETSEL, 0, strlen(initial));

    SetFocus(TextEntryBox);
}
void uiHideTextEntryBox(void)
{
    ShowWindow(TextEntryBox, SW_HIDE);
}
void uiGetTextEntryBoxText(char *dest)
{
    SendMessage(TextEntryBox, WM_GETTEXT, 10, (LPARAM)dest);
    dest[10] = '\0';
}
BOOL uiTextEntryBoxIsVisible(void)
{
    return IsWindowVisible(TextEntryBox);
}

//-----------------------------------------------------------------------------
// Change the status bar. This indicates the current mouse operation, the
// cursor position (x, y), and, in CAD mode, whether the 
//-----------------------------------------------------------------------------
void uiSetStatusBarText(char *solving, BOOL red, char *x, char *y, char *msgIn)
{
    char str[32];
    sprintf(str, " (%s, %s)", x, y);

    char msg[128];
    memcpy(msg+1, msgIn, 120); msg[120] = '\0';
    msg[0] = ' ';

    if(solving) {
        int edges[3] = { 150, 290, -1 };
        SendMessage(MainStatusBar, SB_SETPARTS, 3, (LPARAM)edges);

        if(red) {
            // Our WM_DRAWITEM will draw it in red.
            strcpy(MainStatusBarText, solving);
            SendMessage(MainStatusBar, SB_SETTEXT, 0 | SBT_OWNERDRAW, 
                (LPARAM)NULL);
        } else {
            SendMessage(MainStatusBar, SB_SETTEXT, 0, (LPARAM)solving);
        }

        SendMessage(MainStatusBar, SB_SETTEXT, 1, (LPARAM)str);
        SendMessage(MainStatusBar, SB_SETTEXT, 2, (LPARAM)msg);
    } else {
        int edges[2] = { 140, -1 };
        SendMessage(MainStatusBar, SB_SETPARTS, 2, (LPARAM)edges);

        SendMessage(MainStatusBar, SB_SETTEXT, 0, (LPARAM)str);
        SendMessage(MainStatusBar, SB_SETTEXT, 1, (LPARAM)msg);
    }
    uiRepaint();
}

//-----------------------------------------------------------------------------
// Set our main window's title.
//-----------------------------------------------------------------------------
void uiSetMainWindowTitle(char *str)
{
    SendMessage(MainWindow, WM_SETTEXT, 0, (LPARAM)str);
}

//-----------------------------------------------------------------------------
// Drawing routines. These must only be called from within PaintWindow().
// These draw into the back-buffer that gets brought forward at the end
// of our paint handler.
//-----------------------------------------------------------------------------
static int  CurrentColor;
static BOOL CurrentlyDashed;
static HBRUSH CurrentBrush;
static void SetPen(void)
{
    if(CurrentColor >= MAX_COLORS || CurrentColor < 0) {
        CurrentColor = 0;
    }
    if(CurrentlyDashed) {
        SelectObject(BackDc, DashedPens[CurrentColor]);
    } else {
        SelectObject(BackDc, SolidPens[CurrentColor]);
    }
    CurrentBrush = SolidBrushes[CurrentColor];
    SetTextColor(BackDc, RGB(
                            Colors[CurrentColor].r,
                            Colors[CurrentColor].g,
                            Colors[CurrentColor].b));
    SetBkMode(BackDc, TRANSPARENT);
}
void PltMoveTo(int x, int y)
{
    x = UnmapX(x);
    y = UnmapY(y);
    
    MoveToEx(BackDc, x, y, NULL);
}
void PltLineTo(int x, int y)
{
    x = UnmapX(x);
    y = UnmapY(y);
   
    LineTo(BackDc, x, y);
}
void PltRect(int x0, int y0, int x1, int y1)
{
    x0 = UnmapX(x0);
    y0 = UnmapY(y0);
    x1 = UnmapX(x1);
    y1 = UnmapY(y1);

    RECT r;
    r.left = x0; r.top = y0;
    r.right = x1; r.bottom = y1;
    FillRect(BackDc, &r, CurrentBrush);
}
void PltCircle(int x, int y, int r)
{
    x = UnmapX(x);
    y = UnmapY(y);

    SelectObject(BackDc, GetStockObject(HOLLOW_BRUSH));
    Ellipse(BackDc, x-r+1, y-r, x+r, y+r-1);
}
void PltText(int x, int y, BOOL boldFont, const char *s, ...)
{
    x = UnmapX(x);
    y = UnmapY(y);

    va_list f;
    char buf[1024];
    va_start(f, s);
    vsprintf(buf, s, f);

    if(boldFont) {
        SelectObject(BackDc, HugeFixedFont);
    } else {
        SelectObject(BackDc, LargeFixedFont);
    }
        
    TextOut(BackDc, x, y, buf, strlen(buf));
}
void PltSetColor(int c)
{
    CurrentColor = c;
    SetPen();
}
void PltSetDashed(BOOL dashed)
{
    CurrentlyDashed = dashed;
    SetPen();
}
void PltGetRegion(int *xMin, int *yMin, int *xMax, int *yMax)
{
    RECT r;
    GetClientRect(MainWindow, &r);
    *xMin = MapX(r.left);
    *xMax = MapX(r.right) - RIGHT_PALETTE_WIDTH;
    *yMin = MapY(r.bottom - STATUS_BAR_HEIGHT);
    *yMax = MapY(r.top);
}

//-----------------------------------------------------------------------------
// Force a repaint of the sketch.
//-----------------------------------------------------------------------------
void uiRepaint(void)
{
    InvalidateRect(MainWindow, NULL, FALSE);
    InvalidateRect(MainStatusBar, NULL, FALSE);
}

static BOOL InDrawingArea(int x, int y)
{
    RECT r;
    GetClientRect(MainWindow, &r);

    if(x < 0) return FALSE;
    if(x > r.right - RIGHT_PALETTE_WIDTH) return FALSE;

    if(y < 0) return FALSE;
    if(y > r.bottom - STATUS_BAR_HEIGHT) return FALSE;

    return TRUE;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
static void ShowContextMenuForDerivedItems(int i)
{
    HMENU m = CreatePopupMenu();
    AppendMenu(m, MF_STRING, 1, DL->poly[i].shown ? "&Hide" : "&Show");
    AppendMenu(m, MF_STRING, 2, "&Edit");

    POINT p;
    GetCursorPos(&p);
    int r = TrackPopupMenu(m, TPM_RIGHTBUTTON | TPM_RETURNCMD | TPM_TOPALIGN,
        p.x, p.y, 0, MainWindow, NULL);

    switch(r) {
        case 1: 
            // Toggle visibility state of wherever they clicked
            DerivedItemsListToggleShown(i);
            break;

        case 2:
            // Edit the contents of that list.
            DerivedItemsListEdit(i);
            break;

        default:
            break;
    }
}

//-----------------------------------------------------------------------------
// Callback for the main window. For the most part, this stuff gets dispatched
// to routines in other files.
//-----------------------------------------------------------------------------
static LRESULT CALLBACK 
    MainWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg) {
        case WM_CLOSE:
        case WM_DESTROY:
            MenuFile(MNU_EXIT);
            break;

        case WM_NOTIFY: {
            NMHDR *h = (NMHDR *)lParam;
            if(h->hwndFrom == MainTab && h->code == TCN_SELCHANGE) {
                UpdateAfterTabChange();
            } else if(h->hwndFrom == LayerList) {
                NMLVDISPINFO *lvdi = (NMLVDISPINFO *)h;

                if(h->code == LVN_BEGINLABELEDIT) {
                    // Permit the user to edit the label.
                    EditingLayerListLabel = TRUE;
                    return FALSE;
                } else if(h->code == LVN_ENDLABELEDIT) {
                    // Accept whatever they typed in, and notify the
                    // rest of the code that a layer name changed.
                    EditingLayerListLabel = FALSE;

                    if(lvdi->item.pszText) {
                        LayerDisplayNameUpdated(lvdi->item.iItem,
                                                lvdi->item.pszText);
                    } // else the user cancels, so do nothing
                    return TRUE;
                } else if(h->code == LVN_ITEMCHANGED) {
                    NMLISTVIEW *nmlv = (NMLISTVIEW *)lParam;
                    // It must not be possible to hide the selected layer.
                    if(!ListView_GetCheckState(LayerList,
                        uiGetLayerListSelection()))
                    {   
                        ListView_SetCheckState(LayerList,
                            uiGetLayerListSelection(), TRUE);
                    }
                    // If the selection might have just changed, then
                    // notify the rest of the code.
                    if(nmlv->iItem >= 0) {
                        LayerDisplayCheckboxChanged(nmlv->iItem,
                            ListView_GetCheckState(LayerList, nmlv->iItem));
                    }
                    // And notify the rest of the code regardless.
                    LayerDisplayChanged();
                }
            } else if(h->hwndFrom == DerivedItemsList) {
                if(h->code == NM_RCLICK) {
                    POINT pt;
                    GetCursorPos(&pt);
                    // Mouse position came in screen coordinates, not client.
                    MapWindowPoints(NULL, DerivedItemsList, &pt, 1);
                    // And figure out which item is under the cursor, if any.
                    TVHITTESTINFO thi;
                    thi.pt = pt;
                    thi.flags = TVHT_ONITEM;
                    thi.hItem = NULL;
                    HTREEITEM hti = TreeView_HitTest(DerivedItemsList, &thi);
                    if(!hti) return TRUE;  // not over an item

                    int i;
                    for(i = 0; i < DerivedItemCount; i++) {
                        if(ParentItems[i] == hti) {
                            // And deselect any item that might be selected
                            // now.
                            HTREEITEM nowsel =
                                TreeView_GetSelection(DerivedItemsList);
                            if(nowsel != hti) {
                                TreeView_SetItemState(DerivedItemsList, nowsel,
                                    0, TVIS_SELECTED);
                            }
                            // And select it.
                            TreeView_SetItemState(DerivedItemsList, hti,
                                TVIS_SELECTED, TVIS_SELECTED);
                            TreeView_SelectItem(DerivedItemsList, hti);
                            // Show the right-click menu.
                            ShowContextMenuForDerivedItems(i);
                            break;
                        }
                    }
                    // We would prefer to manage the selection ourselves,
                    // thank you; suppress their attempts at it.
                    return TRUE;
                }
            }
            return DefWindowProc(hwnd, msg, wParam, lParam);
        }
        case WM_SIZE:
            if(BackDc) {
                DeleteDC(BackDc);
                BackDc = NULL;
            }
            if(BackBitmap) {
                DeleteObject(BackBitmap);
                BackBitmap = NULL;
            }
            MainWindowResized();
            uiRepaint();
            break;

        case WM_COMMAND: {
            HWND ch = (HWND)lParam;
            if(ch == AssumedParameters) {
                if(HIWORD(wParam) == LBN_SELCHANGE) {
                    char buf[MAX_STRING];
                    int i = SendMessage(AssumedParameters, LB_GETCURSEL, 0, 0);
                    if(i >= 0 && i != LB_ERR) {
                        SendMessage(AssumedParameters, LB_GETTEXT, i,
                                                            (LPARAM)buf);
                        HighlightAssumption(buf);
                    }
                }
            } else if(ch == InconsistentConstraints) {
                if(HIWORD(wParam) == LBN_SELCHANGE) {
                    char buf[MAX_STRING];
                    int i = SendMessage(InconsistentConstraints, 
                                                    LB_GETCURSEL, 0, 0);
                    if(i >= 0 && i != LB_ERR) {
                        SendMessage(InconsistentConstraints, LB_GETTEXT, i,
                                                                (LPARAM)buf);
                        HighlightConstraint(buf);
                    }
                }
            } else if(ch == LayerInsertAfter && wParam == BN_CLICKED) {
                ButtonAddLayer(FALSE);
            } else if(ch == LayerInsertBefore && wParam == BN_CLICKED) {
                ButtonAddLayer(TRUE);
            } else if(ch == LayerDelete && wParam == BN_CLICKED) {
                ButtonDeleteLayer();
            } else if(ch == ShowAllDerivedItems && wParam == BN_CLICKED) {
                ButtonShowAllDerivedItems();
            } else if(ch == HideAllDerivedItems && wParam == BN_CLICKED) {
                ButtonHideAllDerivedItems();
            } else if(ch == ShowPointsInDerivedMode && wParam == BN_CLICKED) {
                // The code will notice that it should redraw those correctly
                // when we repaint.
                uiRepaint();
            } else if (!lParam) {
                // Menu messages to the main window.
                ProcessMenu(LOWORD(wParam));
            }
            break;
        }
        case WM_MOUSEWHEEL: {
            // The mouse position that they provide in lParam is in screen
            // coordinates, not client coordinates, and easier to just use
            // the old MOUSEMOVED values than to convert.
            int x = LastMousePos.x;
            int y = LastMousePos.y;

            if(!InDrawingArea(x, y)) break;

            int mx = MapX(x);
            int my = MapY(y);

            int delta = GET_WHEEL_DELTA_WPARAM(wParam);
            ScrollWheelMoved(mx, my, -delta);
            break;
        }
        case WM_LBUTTONDBLCLK:
        case WM_LBUTTONUP: 
        case WM_LBUTTONDOWN:
        case WM_MBUTTONDOWN:
        case WM_MOUSEMOVE: {
            int x = LOWORD(lParam);
            int y = HIWORD(lParam);
            LastMousePos.x = x;
            LastMousePos.y = y;

            if(!InDrawingArea(x, y)) break;

            if(msg != WM_MOUSEMOVE) {
                SendMessage(AssumedParameters, LB_SETCURSEL, -1, 0);
                if(uiTextEntryBoxIsVisible()) {
                    SetFocus(TextEntryBox);
                } else {
                    SetFocus(MainWindow);
                }
            }

            int mx = MapX(x);
            int my = MapY(y);

            if(msg == WM_LBUTTONDBLCLK) {
                LeftButtonDblclk(x, y);
            } else if(msg == WM_LBUTTONUP) {
                LeftButtonUp(mx, my);
            } else if(msg == WM_LBUTTONDOWN) {
                LeftButtonDown(mx, my);
            } else if(msg == WM_MBUTTONDOWN) {
                CenterButtonDown(mx, my);
            } else if(msg == WM_MOUSEMOVE) {
                MouseMoved(mx, my,
                    !!(wParam & MK_LBUTTON),
                    !!(wParam & MK_RBUTTON),
                    !!(wParam & MK_MBUTTON));
            } else {
                oopsnf();
            }
            break;
        }

        case WM_ERASEBKGND:
            return NULL;

        case WM_PAINT: {
            HDC paintDc;

            PAINTSTRUCT ps;
            paintDc = BeginPaint(hwnd, &ps);
            
            RECT r;
            GetClientRect(MainWindow, &r);
            int w = (r.right - r.left) - RIGHT_PALETTE_WIDTH;
            int h = r.bottom - r.top;

            if(!BackDc) {

                BackBitmap = CreateCompatibleBitmap(paintDc, w, h);

                BackDc = CreateCompatibleDC(paintDc);
                SelectObject(BackDc, BackBitmap);
            }

            FillRect(BackDc, &r, BlackBrush);
            SetBkColor(BackDc, RGB(0, 0, 0));

            PaintWindow();

            BitBlt(paintDc, 0, 0, w, h, BackDc, 0, 0, SRCCOPY);

            EndPaint(hwnd, &ps);

            break;
        }

        case WM_CTLCOLORSTATIC: {
            HWND h = (HWND)lParam;
            HDC dc = (HDC)wParam;
            
            if(h == ConsistencyStatus) {
                SetBkMode(dc, TRANSPARENT);
                char desc[100];
                SendMessage(h, WM_GETTEXT, sizeof(desc), (LPARAM)desc);

                switch(ConsistencyStatusColor) {
                    case BK_GREEN:
                        return (LRESULT)GreenBrush;
                    case BK_YELLOW:
                        return (LRESULT)YellowBrush;
                    case BK_VIOLET:
                        return (LRESULT)VioletBrush;
                    
                    case BK_GREY:
                    default:
                        // Let the DefWindowProc handle things.
                        break;
                }
            }
            return DefWindowProc(hwnd, msg, wParam, lParam);
        }

        case WM_DRAWITEM: {
            DRAWITEMSTRUCT *dis = (DRAWITEMSTRUCT *)lParam;
            // We'll set the status bar to owner-drawn when we want
            // to highlight a section in pink, to emphasize that we
            // have relaxed the constraints and are no longer trying
            // to solve.
            if(dis->hwndItem == MainStatusBar) {
                FillRect(dis->hDC, &(dis->rcItem), PinkBrush);
                SetBkMode(dis->hDC, TRANSPARENT);
                TextOut(dis->hDC, dis->rcItem.left+3, dis->rcItem.top+1,
                    MainStatusBarText, strlen(MainStatusBarText));
            }
            break;
        }

        default:
            return DefWindowProc(hwnd, msg, wParam, lParam);
    }

    return TRUE;
}

//-----------------------------------------------------------------------------
// Window class for top-level window, then top-level window and its menus.
//-----------------------------------------------------------------------------
void CreateMainWindow(void)
{
    WNDCLASSEX wc;
    memset(&wc, 0, sizeof(wc));
    wc.cbSize = sizeof(wc);
    wc.style            = CS_BYTEALIGNCLIENT | CS_BYTEALIGNWINDOW | CS_OWNDC |
                            CS_DBLCLKS;
    wc.lpfnWndProc      = (WNDPROC)MainWndProc;
    wc.hInstance        = Instance;
    wc.hbrBackground    = NULL;
    wc.lpszClassName    = "SketchFlat";
    wc.lpszMenuName     = NULL;
    wc.hCursor          = LoadCursor(NULL, IDC_ARROW);
    wc.hIcon            = (HICON)LoadImage(Instance, MAKEINTRESOURCE(4000),
                            IMAGE_ICON, 32, 32, 0);
    wc.hIconSm          = (HICON)LoadImage(Instance, MAKEINTRESOURCE(4000),
                            IMAGE_ICON, 16, 16, 0);

    if(!RegisterClassEx(&wc)) oops();

    CreateFontsBrushesPens();
    HMENU top = MakeMainWindowMenus();

    MainWindow = CreateWindowEx(0, "SketchFlat", "",
        WS_OVERLAPPED | WS_THICKFRAME | WS_CLIPCHILDREN | WS_MAXIMIZEBOX |
        WS_MINIMIZEBOX | WS_SYSMENU | WS_SIZEBOX,
        10, 10, 800, 600, NULL, top, Instance, NULL);
}
