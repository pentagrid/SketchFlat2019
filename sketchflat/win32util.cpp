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
// Miscellaneous Win32-specific stuff, not specifically related to our main
// window.
//
// Jonathan Westhues, May 2007
//-----------------------------------------------------------------------------
#define CREATE_MENU_TABLES
#include "sketchflat.h"

#define FREEZE_SUBKEY "SketchFlat"
#include "freeze.h"

HINSTANCE Instance;
HWND      MainWindow;
HMENU     SubMenus[20];

extern BOOL  EditingLayerListLabel;

// We'll keep these here because they get saved in the registry.
static BOOL ShowConstraints;
static BOOL ShowDatumItems;
static BOOL UseInches;

void CreateMainWindow(void);
void MakeMainWindowControls(void);
static BOOL ProcessKeyDown(int key);

static void UpdateMenusChecked(void);

//-----------------------------------------------------------------------------
// Entry point into the program.
//-----------------------------------------------------------------------------
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
    LPSTR lpCmdLine, INT nCmdShow)
{
    Instance = hInstance;

    // This sets up the heap on which we will allocate our dynamic memory.
    FreeAll();

    InitCommonControls();
    
    CreateMainWindow();

    ThawWindowPos(MainWindow);
    ShowConstraints = TRUE;
    ShowDatumItems = TRUE;
    UseInches = FALSE; ThawDWORD(UseInches);

    MakeMainWindowControls();
    ShowWindow(MainWindow, SW_SHOW);
    UpdateMenusChecked();

    // A file might be specified on the command line.
    char loadFile[MAX_STRING] = "";
    if(strlen(lpCmdLine) < MAX_STRING) {
        char *s = lpCmdLine;
        while(isspace(*s)) s++;
        if(*s == '"') s++;

        strcpy(loadFile, s);
        s = strrchr(loadFile, '"');
        if(s) *s = '\0';
    }
    Init(loadFile);
    
    MSG msg;
    DWORD ret;
    while(ret = GetMessage(&msg, NULL, 0, 0)) {
        // Don't let the tab control steal keyboard events.
        if(msg.message == WM_KEYDOWN) {
            if(ProcessKeyDown(msg.wParam)) continue;
        }
        // Even when the main window doesn't have the focus, let's give it
        // the mousewheel events, with which to zoom.
        if(msg.message == WM_MOUSEWHEEL) {
            msg.hwnd = MainWindow;
        }

        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    FreezeWindowPos(MainWindow);
    FreezeDWORD(UseInches);

    return 0;
}

void Exit(void)
{
    PostQuitMessage(0);
}

//-----------------------------------------------------------------------------
// printf-like debug function, to the Windows debug log.
//-----------------------------------------------------------------------------
void dbp(const char *str, ...)
{
    va_list f;
    char buf[1024*40];
    sprintf(buf,"%5u:  ", GetTickCount() % 100000);

    va_start(f, str);
    _vsnprintf(buf+7, sizeof(buf)-100, str, f);
    OutputDebugString(buf);
    OutputDebugString("\n");
}
void dbp2(const char *str, ...)
{
    return;

    va_list f;
    char buf[1024*40];
    sprintf(buf,"%5u:  ", GetTickCount() % 100000);

    va_start(f, str);
    _vsnprintf(buf+7, sizeof(buf)-100, str, f);
    OutputDebugString(buf);
    OutputDebugString("\n");
}

//-----------------------------------------------------------------------------
// Is the shift key pressed?
//-----------------------------------------------------------------------------
BOOL uiShiftKeyDown(void)
{
    return !!(GetAsyncKeyState(VK_SHIFT) & 0x8000);
}

//-----------------------------------------------------------------------------
// Common dialog routines, to open or save a file.
//-----------------------------------------------------------------------------
BOOL uiGetOpenFile(const char *file, const char *defExtension, const char *selPattern)
{
    OPENFILENAME ofn;

    char tempSaveFile[MAX_PATH] = "";

    memset(&ofn, 0, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hInstance = Instance;
    ofn.hwndOwner = MainWindow;
    ofn.lpstrFilter = selPattern;
    ofn.lpstrDefExt = defExtension;
    ofn.lpstrFile = (char*)file;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_HIDEREADONLY;

    EnableWindow(MainWindow, FALSE);
    BOOL r = GetOpenFileName(&ofn);
    EnableWindow(MainWindow, TRUE);
    SetForegroundWindow(MainWindow);
    return r;
}
BOOL uiGetSaveFile(const char *file, const char *defExtension, const char *selPattern)
{
    OPENFILENAME ofn;

    memset(&ofn, 0, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hInstance = Instance;
    ofn.hwndOwner = MainWindow;
    ofn.lpstrFilter = selPattern;
    ofn.lpstrDefExt = defExtension;
    ofn.lpstrFile = (char*)file;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT;

    EnableWindow(MainWindow, FALSE);
    BOOL r = GetSaveFileName(&ofn);
    EnableWindow(MainWindow, TRUE);
    SetForegroundWindow(MainWindow);
    return r;
}
int uiSaveFileYesNoCancel(void)
{
    return MessageBox(MainWindow, 
        "The program has changed since it was last saved.\r\n\r\n"
        "Do you want to save the changes?", "SketchFlat",
        MB_YESNOCANCEL | MB_ICONWARNING);
}

//-----------------------------------------------------------------------------
// For error messages to the user; printf-like, to a message box.
//-----------------------------------------------------------------------------
void uiError(const char *str, ...)
{
    va_list f;
    char buf[1024];
    va_start(f, str);
    vsprintf(buf, str, f);

    HWND h = GetForegroundWindow();
    MessageBox(h, buf, "SketchFlat Error", MB_OK | MB_ICONERROR);
}

//-----------------------------------------------------------------------------
// Create a window with a given client area.
//-----------------------------------------------------------------------------
HWND CreateWindowClient(DWORD exStyle, char *className, char *windowName,
    DWORD style, int x, int y, int width, int height, HWND parent,
    HMENU menu, HINSTANCE instance, void *param)
{
    HWND h = CreateWindowEx(exStyle, className, windowName, style, x, y,
        width, height, parent, menu, instance, param);

    RECT r;
    GetClientRect(h, &r);
    width = width - (r.right - width);
    height = height - (r.bottom - height);
    
    SetWindowPos(h, HWND_TOP, x, y, width, height, 0);

    return h;
}

//-----------------------------------------------------------------------------
// Memory allocation functions. We'll use a Windows heap, which makes it
// easy to free everything when we're asked to.
//-----------------------------------------------------------------------------
static HANDLE Heap;
void *Alloc(int bytes)
{
    void *v = HeapAlloc(Heap, HEAP_NO_SERIALIZE | HEAP_ZERO_MEMORY, bytes);
    if(!v) oops();

    return v;
}
void Free(void *p)
{
    HeapFree(Heap, HEAP_NO_SERIALIZE, p);
}
void FreeAll(void)
{
    if(Heap) {
        HeapDestroy(Heap);
    }

    Heap = HeapCreate(HEAP_NO_SERIALIZE, 1024*1024*20, 0);
}

//-----------------------------------------------------------------------------
// A separate allocator, that we use for polygons and such. This needs to
// be separate so the its stuff doesn't get freed when we free all the 
// expression stuff at once.
//-----------------------------------------------------------------------------
void DFree(void *p)
{
    free(p);
}
void *DAlloc(int bytes)
{
    return malloc(bytes);
}

//-----------------------------------------------------------------------------
// Routines to show and un-show the hourglass cursor. We use this when the
// solution routines are slow.
//-----------------------------------------------------------------------------
void uiSetCursorToHourglass(void)
{
    SetCursor(LoadCursor(0, IDC_WAIT));
}
void uiRestoreCursor(void)
{
    POINT p;
    GetCursorPos(&p);
    SetCursorPos(p.x, p.y);
}

//-----------------------------------------------------------------------------
// Create the menu bar associated with the main window. This changes,
// depending on whether we are are in the CAD or CAM view.
//-----------------------------------------------------------------------------
HMENU MakeMainWindowMenus(void)
{
    HMENU top = CreateMenu();
    HMENU m;

    int i;
    int subMenu = 0;
    
    for(i = 0; i < arraylen(Menus); i++) {
        if(Menus[i].level == 0) {
            m = CreateMenu();
            AppendMenu(top, MF_STRING | MF_POPUP, (UINT_PTR)m, Menus[i].label);

            if(subMenu >= arraylen(SubMenus)) oops();
            SubMenus[subMenu] = m;
            subMenu++;
        } else {
            if(Menus[i].label) {
                AppendMenu(m, MF_STRING, Menus[i].id, Menus[i].label);
            } else {
                AppendMenu(m, MF_SEPARATOR, Menus[i].id, "");
            }
        }
    }

    return top;
}

void uiCheckMenuById(int id, BOOL checked)
{
    int i;
    int subMenu = -1;

    for(i = 0; i < arraylen(Menus); i++) {
        if(Menus[i].level == 0) {
            subMenu++;
        }
        
        if(Menus[i].id == id) {
            if(subMenu < 0) oops();
            if(subMenu >= arraylen(SubMenus)) oops();

            CheckMenuItem(SubMenus[subMenu], id,
                        checked ? MF_CHECKED : MF_UNCHECKED);
            return;
        }
    }
    oopsnf();
}
void uiEnableMenuById(int id, BOOL enabled)
{
    int i;
    int subMenu = -1;

    for(i = 0; i < arraylen(Menus); i++) {
        if(Menus[i].level == 0) {
            subMenu++;
        }
        
        if(Menus[i].id == id) {
            if(subMenu < 0) oops();
            if(subMenu >= arraylen(SubMenus)) oops();

            EnableMenuItem(SubMenus[subMenu], id,
                        enabled ? MF_ENABLED : MF_GRAYED);
            return;
        }
    }
    oopsnf();
}


//-----------------------------------------------------------------------------
// Process a menu selection. We have a table, so just look through there
// and call the appropriate function.
//-----------------------------------------------------------------------------
void ProcessMenu(int id)
{
    if(id == 0) return;

    int i;
    for(i = 0; i < arraylen(Menus); i++) {
        if(Menus[i].id == id) {
            // If e.g. we're dragging something in the sketch mode GUI, then
            // stop that.
            CancelSketchModeOperation();
            (*(Menus[i].fn))(id);
            return;
        }
    }
    oopsnf();
}

//-----------------------------------------------------------------------------
// Process a keypress. If the keypress came to the main window, then it's
// probably an accelerator for a menu item. Not an error if it's not
// meaningful, though.
//-----------------------------------------------------------------------------
static BOOL IsForNavigation(int key)
{
    return   (key == VK_LEFT    ||
              key == VK_RIGHT   ||
              key == VK_HOME    ||
              key == VK_END     ||
              key == VK_DELETE  ||
              key == VK_BACK);
}
static BOOL ProcessKeyDown(int key)
{
    int i;

    // There's a textbox that we place on the drawing, to enter the values
    // of dimensions. When it's visible, it should get keys.
    if(uiTextEntryBoxIsVisible()) {
        if((key >= '0' && key <= '9') || key == VK_OEM_MINUS ||
            (key >= VK_NUMPAD0 && key <= VK_NUMPAD9) ||
            key == VK_SUBTRACT ||
            key == VK_DECIMAL ||
            key == VK_OEM_PERIOD || IsForNavigation(key))
        {
            return FALSE;
        }
    }

    // The labels in the list of sketches/layers are editable. When that
    // textbox is shown, it should get keys.
    if(EditingLayerListLabel) {
        if((key >= 'A' && key <= 'Z') || (key >= '0' && key <= '9') ||
            key == VK_OEM_MINUS ||
            IsForNavigation(key) || key == VK_RETURN || key == VK_ESCAPE)
        {
            return FALSE;
        } else {
            // Swallow additional keys; they probably don't want to use
            // menu accelerators while editing the label.
            return TRUE;
        }
    }

    // These ones are useful to navigate the "consistency lists" in the
    // right-side toolbar.
    if(key == VK_LEFT || key == VK_RIGHT || key == VK_UP || key == VK_DOWN) {
        return FALSE;
    }


    if(GetAsyncKeyState(VK_CONTROL) & 0x8000) {
        if(key >= 'A' && key <= 'Z') {
            // Map the control characters appropriately.
            key -= 'A';
            key += 1;
        }
    }

    int subMenu = -1;
    for(i = 0; i < arraylen(Menus); i++) {
        if(Menus[i].level == 0) {
            subMenu++;
        }

        if(Menus[i].accelerator == key) {
            // We don't want the keyboard shortcut to work when the menu is
            // disabled.
            if(EnableMenuItem(SubMenus[subMenu], Menus[i].id, MF_ENABLED) ==
                    MF_ENABLED)
            {
                // If e.g. we're dragging something in the sketch mode GUI, 
                // then stop that.
                CancelSketchModeOperation();
                (*(Menus[i].fn))(Menus[i].id);
                return TRUE;
            } else {
                // We just accidentally enabled a disabled menu item, so
                // fix that. I'm not sure how to query the state without
                // changing it.
                EnableMenuItem(SubMenus[subMenu], Menus[i].id, MF_GRAYED);
            }
        }
    }

    // Otherwise no harm, though; pass it along to the general-purpose
    // keypress handler.
    KeyPressed(key);
    return TRUE;
}

//-----------------------------------------------------------------------------
// Selections that get saved in the registry, and are therefore easiest
// to store with the Win32 stuff.
//-----------------------------------------------------------------------------
static void UpdateMenusChecked(void)
{
    uiCheckMenuById(MNU_VIEW_SHOW_CONSTRAINTS, ShowConstraints);
    uiCheckMenuById(MNU_VIEW_SHOW_DATUM_ITEMS, ShowDatumItems);
    uiCheckMenuById(MNU_VIEW_IN_INCHES, UseInches);
    uiCheckMenuById(MNU_VIEW_IN_MM, !UseInches);
}
void MenuSettings(int id)
{
    switch(id) {
        case MNU_VIEW_SHOW_CONSTRAINTS:
            ShowConstraints = !ShowConstraints;
            break;

        case MNU_VIEW_SHOW_DATUM_ITEMS:
            ShowDatumItems = !ShowDatumItems;
            break;

        case MNU_VIEW_IN_INCHES:
            UseInches = TRUE;
            UpdateAfterUnitChange();
            break;

        case MNU_VIEW_IN_MM:
            UseInches = FALSE;
            UpdateAfterUnitChange();
            break;
    }

    UpdateMenusChecked();
    uiRepaint();
}
BOOL uiShowConstraints(void) { return ShowConstraints; }
BOOL uiShowDatumItems(void) { return ShowDatumItems; }
BOOL uiUseInches(void) { return UseInches; }

