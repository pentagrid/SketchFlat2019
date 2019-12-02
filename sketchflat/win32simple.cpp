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
// A dialog box with a combination of textboxes, labels for those textboxes,
// and listboxes that can be used to choose an existing derived. We use this
// when we're creating all sorts of different derived things.
//
// This is partially swiped from the simpldialog stuff in LDmicro.
//
// Jonathan Westhues, May 2007
//-----------------------------------------------------------------------------
#include "sketchflat.h"

void NiceFont(HWND x);
HWND CreateWindowClient(DWORD exStyle, char *className, char *windowName,
    DWORD style, int x, int y, int width, int height, HWND parent,
    HMENU menu, HINSTANCE instance, void *param);
extern HINSTANCE Instance;
extern HWND MainWindow;

// Our dialog, and controls in our dialog.
static HWND SimpleDialog;
static HWND OkButton;
static HWND CancelButton;

#define MAX_BOXES 10
static HWND Textboxes[MAX_BOXES];
static HWND Lists[MAX_BOXES];
static HWND Labels[MAX_BOXES];

static LONG_PTR PrevNumOnlyProc[MAX_BOXES];

static BOOL DialogDone;
static BOOL DialogCancel;

//-----------------------------------------------------------------------------
// Don't allow any characters other than -0-9. in the box.
//-----------------------------------------------------------------------------
static LRESULT CALLBACK MyNumOnlyProc(HWND hwnd, UINT msg, WPARAM wParam,
    LPARAM lParam)
{
    if(msg == WM_CHAR) {
        if(!(isdigit(wParam) || wParam == '.' || wParam == '\b' 
            || wParam == '-'))
        {
            return 0;
        }
    }

    int i;
    for(i = 0; i < MAX_BOXES; i++) {
        if(hwnd == Textboxes[i]) {
            return CallWindowProc((WNDPROC)PrevNumOnlyProc[i], hwnd, msg, 
                wParam, lParam);
        }
    }
    oops();
}

static void MakeControls(int boxes, const char **labels, DWORD numMask)
{
    int i, j;
    for(i = 0; i < boxes; i++) {
        Labels[i] = CreateWindowEx(0, WC_STATIC, labels[i],
            WS_CHILD | WS_CLIPSIBLINGS | WS_VISIBLE | SS_RIGHT,
            5, 13 + i*30, 80, 21,
            SimpleDialog, NULL, Instance, NULL);
        NiceFont(Labels[i]);

        if(numMask & (1 << i)) {
            Textboxes[i] = CreateWindowEx(WS_EX_CLIENTEDGE, WC_EDIT, "",
                WS_CHILD | ES_AUTOHSCROLL | WS_TABSTOP | WS_CLIPSIBLINGS |
                WS_VISIBLE,
                90, 12 + 30*i, 170, 21,
                SimpleDialog, NULL, Instance, NULL);
        } else {
            Lists[i] = CreateWindowEx(WS_EX_CLIENTEDGE, WC_COMBOBOX, "",
                WS_CHILD | WS_TABSTOP | WS_CLIPSIBLINGS | WS_VISIBLE |
                CBS_DROPDOWNLIST | CBS_HASSTRINGS | WS_VSCROLL,
                90, 10 + 30*i, 170, 200,
                SimpleDialog, NULL, Instance, NULL);
            NiceFont(Lists[i]);

            for(j = 0; j < DL->polys; j++) {
                SendMessage(Lists[i], CB_ADDSTRING, 0, 
                                (LPARAM)DL->poly[j].displayName);
            }
        }

        NiceFont(Textboxes[i]);
    }

    OkButton = CreateWindowEx(0, WC_BUTTON, "OK",
        WS_CHILD | WS_TABSTOP | WS_CLIPSIBLINGS | WS_VISIBLE | BS_DEFPUSHBUTTON,
        268, 11, 70, 23, SimpleDialog, NULL, Instance, NULL); 
    NiceFont(OkButton);

    CancelButton = CreateWindowEx(0, WC_BUTTON, "Cancel",
        WS_CHILD | WS_TABSTOP | WS_CLIPSIBLINGS | WS_VISIBLE,
        268, 41, 70, 23, SimpleDialog, NULL, Instance, NULL); 
    NiceFont(CancelButton);
}

//-----------------------------------------------------------------------------
// Window proc for the dialog boxes. Handles Ok/Cancel stuff.
//-----------------------------------------------------------------------------
static LRESULT CALLBACK SimpleProc(HWND hwnd, UINT msg, WPARAM wParam,
    LPARAM lParam)
{
    switch (msg) {
        case WM_NOTIFY:
            break;

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

    return 1;
}

static void MakeSimpleWindowClass(void)
{
    WNDCLASSEX wc;
    memset(&wc, 0, sizeof(wc));
    wc.cbSize = sizeof(wc);

    wc.style            = CS_BYTEALIGNCLIENT | CS_BYTEALIGNWINDOW | CS_OWNDC |
                          CS_DBLCLKS;
    wc.lpfnWndProc      = (WNDPROC)SimpleProc;
    wc.hInstance        = Instance;
    wc.hbrBackground    = (HBRUSH)COLOR_BTNSHADOW;
    wc.lpszClassName    = "SketchFlatDialog";
    wc.lpszMenuName     = NULL;
    wc.hCursor          = LoadCursor(NULL, IDC_ARROW);
    wc.hIcon            = (HICON)LoadImage(Instance, MAKEINTRESOURCE(4000),
                            IMAGE_ICON, 32, 32, 0);
    wc.hIconSm          = (HICON)LoadImage(Instance, MAKEINTRESOURCE(4000),
                            IMAGE_ICON, 16, 16, 0);

    RegisterClassEx(&wc);
}

BOOL uiShowSimpleDialog(const char *title, int boxes, const char **labels,
    DWORD numMask, hDerived *destH, char **destS)
{
    BOOL didCancel;

    if(boxes > MAX_BOXES) oops();

    MakeSimpleWindowClass();
    SimpleDialog = CreateWindowClient(0, (char*)"SketchFlatDialog", (char*)title, 
        WS_OVERLAPPED | WS_SYSMENU,
        100, 100, 354, 15 + 30*(boxes < 2 ? 2 : boxes), NULL, NULL,
        Instance, NULL);

    MakeControls(boxes, labels, numMask);
  
    int i;
    for(i = 0; i < boxes; i++) {
        if(numMask & (1 << i)) {
            // Initially, the text box shows whatever was in the string.
            SendMessage(Textboxes[i], WM_SETTEXT, 0, (LPARAM)destS[i]);

            PrevNumOnlyProc[i] = SetWindowLongPtr(Textboxes[i], GWLP_WNDPROC, 
                (LONG_PTR)MyNumOnlyProc);
        } else {
            // And the lists are set to whichever polygon we were using 
            // previously.
            int j;
            for(j = 0; j < DL->polys; j++) {
                if(DL->poly[j].id == destH[i]) {
                    SendMessage(Lists[i], CB_SETCURSEL, j, 0);
                    break;
                }
            }
        }
    }

    EnableWindow(MainWindow, FALSE);
    ShowWindow(SimpleDialog, TRUE);

    if(numMask & 1) {
        SetFocus(Textboxes[0]);
        SendMessage(Textboxes[0], EM_SETSEL, 0, -1);
    } else {
        SetFocus(Lists[0]);
    }

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

        if(IsDialogMessage(SimpleDialog, &msg)) continue;
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    didCancel = DialogCancel;

    if(!didCancel) {
        for(i = 0; i < boxes; i++) {
            if(numMask & (1 << i)) {
                SendMessage(Textboxes[i], WM_GETTEXT, 15, (LPARAM)destS[i]);
            } else {
                int p = SendMessage(Lists[i], CB_GETCURSEL, 0, 0);
                if(p < 0 || p >= DL->polys) {
                    // That's like cancelling; shouldn't be possible anyways.
                    return FALSE;
                }
                destH[i] = DL->poly[p].id;
            }
        }
    }

    EnableWindow(MainWindow, TRUE);
    DestroyWindow(SimpleDialog);

    return !didCancel;
}
