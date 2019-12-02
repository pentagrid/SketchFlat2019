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
// Win32-specific routines to pop up a dialog, which we can use to select
// a font (by selecting a TrueType font file) and the text to be entered.
//
// Jonathan Westhues, May 2007
//-----------------------------------------------------------------------------
#include "sketchflat.h"

extern void NiceFont(HWND x);
extern HWND CreateWindowClient(DWORD exStyle, char *className, char *windowName,
    DWORD style, int x, int y, int width, int height, HWND parent,
    HMENU menu, HINSTANCE instance, void *param);
extern HINSTANCE Instance;
extern HWND MainWindow;

static HWND TextWindow;

static HWND TextBox;
static HWND SpacingBox;
static HWND FontList;
static HWND OkButton;
static HWND CancelButton;

static BOOL DialogDone, DialogCancel;

// A fixed limit is useful here, to avoid wasting huge amounts of time.
#define MAX_FONTS_IN_LIST   1000
static struct {
    char    file[MAX_STRING];
    char    name[MAX_STRING];
}  FontToChoose[MAX_FONTS_IN_LIST];
int FontsToChoose;


static void GetFontDirectory(char *str)
{
    GetWindowsDirectory(str, MAX_STRING);
    if(strlen(str) > MAX_STRING - 100) return;
    strcat(str, "\\fonts");
}

void txtuiGetDefaultFont(char *str)
{
    GetFontDirectory(str);
    strcat(str, "\\arial.ttf");
}

static LRESULT CALLBACK 
    TextWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg) {
        case WM_COMMAND: {
            HWND h = (HWND)lParam;
            if(h == OkButton && wParam == BN_CLICKED) {
                DialogDone = TRUE;
            } else if(h == CancelButton && wParam == BN_CLICKED) {
                DialogDone = TRUE;
                DialogCancel = TRUE;
            }
            break;
        }

        case WM_CLOSE:
        case WM_DESTROY:
            DialogDone = TRUE;
            DialogCancel = TRUE;
            break;

        default:
            return DefWindowProc(hwnd, msg, wParam, lParam);
    }

    return TRUE;
}

static void MakeWindowClass(void)
{
    WNDCLASSEX wc;
    memset(&wc, 0, sizeof(wc));
    wc.cbSize = sizeof(wc);

    wc.style            = CS_BYTEALIGNCLIENT | CS_BYTEALIGNWINDOW | CS_OWNDC |
                            CS_DBLCLKS;
    wc.lpfnWndProc      = (WNDPROC)TextWndProc;
    wc.hInstance        = Instance;
    wc.hbrBackground    = (HBRUSH)COLOR_BTNSHADOW;
    wc.lpszClassName    = "SketchFlatText";
    wc.lpszMenuName     = NULL;
    wc.hCursor          = LoadCursor(NULL, IDC_ARROW);
    wc.hIcon            = (HICON)LoadImage(Instance, MAKEINTRESOURCE(4000),
                            IMAGE_ICON, 32, 32, 0);
    wc.hIconSm          = (HICON)LoadImage(Instance, MAKEINTRESOURCE(4000),
                            IMAGE_ICON, 16, 16, 0);

    RegisterClassEx(&wc);
}

static void MakeControls(void)
{
    HWND label0, label1, label2;

    // Label for text box (for text to be sketched in TrueType font), and
    // that text box.
    label0 = CreateWindowEx(0, WC_STATIC, "Text:",
        WS_CHILD | WS_CLIPSIBLINGS | WS_VISIBLE | SS_RIGHT,
        5, 8, 55, 21,
        TextWindow, NULL, Instance, NULL);
    NiceFont(label0);
    TextBox = CreateWindowEx(WS_EX_CLIENTEDGE, WC_EDIT, "",
        WS_CHILD | ES_AUTOHSCROLL | WS_TABSTOP | WS_CLIPSIBLINGS | WS_VISIBLE,
        65, 8, 185, 21,
        TextWindow, NULL, Instance, NULL);
    NiceFont(TextBox);

    // Label for spacing box, and that box.
    label1 = CreateWindowEx(0, WC_STATIC, "Spacing:",
        WS_CHILD | WS_CLIPSIBLINGS | WS_VISIBLE | SS_RIGHT,
        5, 38, 55, 21,
        TextWindow, NULL, Instance, NULL);
    NiceFont(label1);
    SpacingBox = CreateWindowEx(WS_EX_CLIENTEDGE, WC_EDIT, "",
        WS_CHILD | ES_AUTOHSCROLL | WS_TABSTOP | WS_CLIPSIBLINGS | WS_VISIBLE,
        65, 38, 70, 21,
        TextWindow, NULL, Instance, NULL);
    NiceFont(SpacingBox);

    // Label for list of fonts, and those fonts.
    label2 = CreateWindowEx(0, WC_STATIC, "Font:",
        WS_CHILD | WS_CLIPSIBLINGS | WS_VISIBLE | SS_RIGHT,
        5, 68, 55, 21,
        TextWindow, NULL, Instance, NULL);
    NiceFont(label2);
    FontList = CreateWindowEx(WS_EX_CLIENTEDGE, WC_LISTBOX, "",
        WS_CHILD | WS_TABSTOP | WS_CLIPSIBLINGS | WS_VISIBLE | WS_VSCROLL |
        LBS_NOTIFY | LBS_SORT,
        65, 68, 185, 100, TextWindow, NULL, Instance, NULL);
    NiceFont(FontList);

    // OK and Cancel buttons
    OkButton = CreateWindowEx(0, WC_BUTTON, "OK",
        WS_CHILD | WS_TABSTOP | WS_CLIPSIBLINGS | WS_VISIBLE | BS_DEFPUSHBUTTON,
        266, 7, 70, 23, TextWindow, NULL, Instance, NULL); 
    NiceFont(OkButton);
    CancelButton = CreateWindowEx(0, WC_BUTTON, "Cancel",
        WS_CHILD | WS_TABSTOP | WS_CLIPSIBLINGS | WS_VISIBLE,
        266, 37, 70, 23, TextWindow, NULL, Instance, NULL); 
    NiceFont(CancelButton);

    // And now let's fill the listbox with available fonts, from our
    // Windows font directory.
    WIN32_FIND_DATA wfd;
    char dir[MAX_STRING];
    GetFontDirectory(dir);
    strcat(dir, "\\*.ttf");

    HANDLE h = FindFirstFile(dir, &wfd);

    FontsToChoose = 0;

    while(h != INVALID_HANDLE_VALUE) {
       if(FontsToChoose >= MAX_FONTS_IN_LIST) break; 

        wfd.cFileName[80] = '\0';

        char fullPath[MAX_STRING];
        GetFontDirectory(fullPath);
        strcat(fullPath, "\\");
        strcat(fullPath, wfd.cFileName);

        char str[MAX_STRING];
        TtfGetDisplayName(fullPath, str);
        str[80] = '\0';

        SendMessage(FontList, LB_ADDSTRING, 0, (LPARAM)str);

        strcpy(FontToChoose[FontsToChoose].name, str);
        strcpy(FontToChoose[FontsToChoose].file, fullPath);
        FontsToChoose++;
        
        if(!FindNextFile(h, &wfd)) break;
    }
}

void txtuiGetTextForDrawing(char *text, char *font, double *spacing)
{   
    MakeWindowClass();

    TextWindow = CreateWindowClient(0, (char*)"SketchFlatText", (char*)"Choose Text and Font",
        WS_OVERLAPPED | WS_SYSMENU,
        100, 100, 350, 180, NULL, NULL, Instance, NULL);
    
    MakeControls();

    // Set the textbox to whatever the text was before.
    SendMessage(TextBox, WM_SETTEXT, 0, (LPARAM)text);
    SendMessage(TextBox, EM_SETSEL, 0, -1);

    // And same for the box with our horizontal spacing.
    char buf[MAX_STRING];
    sprintf(buf, "%d", (int)*spacing);
    SendMessage(SpacingBox, WM_SETTEXT, 0, (LPARAM)buf);

    // Do we have an entry in the listbox that corresponds to the currently-
    // selected font?
    int i;
    char targetName[MAX_STRING];
    for(i = 0; i < MAX_FONTS_IN_LIST; i++) {
        if(strcmp(font, FontToChoose[i].file)==0) {
            strcpy(targetName, FontToChoose[i].name);
            break;
        }
    }
    if(i < MAX_FONTS_IN_LIST) {
        // We do, so let's select that one to start with.
        int n = SendMessage(FontList, LB_GETCOUNT, 0, 0);
        for(i = 0; i < n; i++) {
            char name[MAX_STRING] = "";
            SendMessage(FontList, LB_GETTEXT, i, (LPARAM)name);
            if(strcmp(targetName, name)==0) {
                SendMessage(FontList, LB_SETCURSEL, i, 0);
                break;
            }
        }
    }

    // Show our window and go.
    EnableWindow(MainWindow, FALSE);
    ShowWindow(TextWindow, TRUE);
    SetFocus(TextBox);

    MSG msg;
    DWORD ret;
    DialogDone = FALSE;
    DialogCancel = FALSE;
    while((ret = GetMessage(&msg, NULL, 0, 0)) && !DialogDone) {
        if(msg.message == WM_KEYDOWN) {
            if(msg.wParam == VK_RETURN) {
                DialogDone = TRUE;
                break;
            } else if(msg.wParam == VK_ESCAPE) {
                DialogDone = TRUE;
                DialogCancel = TRUE;
                break;
            }
        }

        if(IsDialogMessage(TextWindow, &msg)) continue;
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    if(!DialogCancel) {
        int pos = SendMessage(FontList, LB_GETCURSEL, 0, 0);

        if(pos >= 0) {
            char name[MAX_STRING];
            SendMessage(FontList, LB_GETTEXT, pos, (LPARAM)name);

            for(i = 0; i < MAX_FONTS_IN_LIST; i++) {
                if(strcmp(FontToChoose[i].name, name)==0) {
                    strcpy(font, FontToChoose[i].file);
                    break;
                }
            }

            SendMessage(TextBox, WM_GETTEXT, MAX_STRING, (LPARAM)text);

            char buf[MAX_STRING];
            SendMessage(SpacingBox, WM_GETTEXT, MAX_STRING, (LPARAM)buf);
            *spacing = atoi(buf);
        }
    }

    EnableWindow(MainWindow, TRUE);
    DestroyWindow(TextWindow);
}
