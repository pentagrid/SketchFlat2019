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
// Entry point into the program. This is where I handle the top-level user
// interface.
// Jonathan Westhues, April 2007
//-----------------------------------------------------------------------------
#include "sketchflat.h"

char CurrentFileName[MAX_PATH];
#define SKF_PATTERN "SketchFlat files (*.skf)\0*.skf\0All files\0*\0\0"

static void UpdateMainWindowTitle(void)
{
    char str[MAX_STRING];
    sprintf(str, "SketchFlat - %s",
        CurrentFileName[0] == '\0' ? "(not yet saved)" : CurrentFileName);

    uiSetMainWindowTitle(str);
}

void Init(char *cmdLine)
{
    NewEmptyProgram();

    if(strlen(cmdLine) > 0) {
        // Make the provided filename absolute, to avoid mystery problems
        // if e.g. the file dialog boxes later change our current directory.
        GetFullPathName(cmdLine,
                            sizeof(CurrentFileName), CurrentFileName, NULL);
        if(!LoadFromFile(CurrentFileName)) {
            uiError("Couldn't open '%s'", CurrentFileName);
        }
    }

    UpdateMainWindowTitle();
}

//-----------------------------------------------------------------------------
// Save the program. If saveAs is TRUE, then ask for a new filename, whether
// or not we have one already. Otherwise show a dialog box only if we already
// know the filename.
//-----------------------------------------------------------------------------
static BOOL SaveProgram(BOOL saveAs)
{
    if(saveAs || CurrentFileName[0] == '\0') {
        if(!uiGetSaveFile(CurrentFileName, "skf", SKF_PATTERN)) {
            return FALSE;
        }
    }

    if(SaveToFile(CurrentFileName)) {
        ProgramChangedSinceSave = FALSE;
        return TRUE;
    } else {
        uiError("Couldn't write to file '%s'", CurrentFileName);
        return FALSE;
    }
}

//-----------------------------------------------------------------------------
// If the program has been modified then give the user the option to save it
// or to cancel the operation they are performing. Return TRUE if they want
// to cancel.
//-----------------------------------------------------------------------------
static BOOL CheckSaveUserCancels(void)
{
    if(!ProgramChangedSinceSave) {
        // no problem
        return FALSE;
    }

    switch(uiSaveFileYesNoCancel()) {
        case IDYES:
            if(SaveProgram(FALSE))
                return FALSE;
            else
                return TRUE;

        case IDNO:
            return FALSE;

        case IDCANCEL:
            return TRUE;

        default:
            oops();
    }
}

void MenuFile(int id)
{
    switch(id) {
        case MNU_NEW:
            if(CheckSaveUserCancels()) break;
            CurrentFileName[0] = '\0';
            NewEmptyProgram();
            ProgramChangedSinceSave = FALSE;
            UpdateMainWindowTitle();
            break;

        case MNU_OPEN:
            if(CheckSaveUserCancels()) break;

            if(uiGetOpenFile(CurrentFileName, "skf", SKF_PATTERN)) {
                if(!LoadFromFile(CurrentFileName)) {
                    uiError("Couldn't open '%s'", CurrentFileName);
                    NewEmptyProgram();
                }
            }
            ProgramChangedSinceSave = FALSE;
            UpdateMainWindowTitle();
            break;

        case MNU_SAVE:
            SaveProgram(FALSE);
            UpdateMainWindowTitle();
            break;

        case MNU_SAVE_AS:
            SaveProgram(TRUE);
            UpdateMainWindowTitle();
            break;

        case MNU_EXIT:
            if(CheckSaveUserCancels()) break;

            Exit();
            break;
    }
}

void MenuManual(int id)
{
}

void MenuAbout(int id)
{   
    MessageBox(NULL,
"SketchFlat is a two-dimensional technical drawing program. It is "
"designed primarily to generate CAM output data, for manufacturing on "
"a laser cutter, waterjet machine, vinyl cutter, 3-axis mill, or other "
"machine tool."
"\r\n\r\n"
"See http://cq.cx/sketchflat.pl"
"\r\n\r\n"
"Release 0.3, built " __DATE__ " " __TIME__ 
". Copyright 2008, Jonathan Westhues."
"\r\n\r\n"
"SketchFlat is free software: you can redistribute it and/or modify "
"it under the terms of the GNU General Public License as published by "
"the Free Software Foundation, either version 3 of the License, or "
"(at your option) any later version."
"\r\n\r\n"
"SketchFlat is distributed in the hope that it will be useful, "
"but WITHOUT ANY WARRANTY; without even the implied warranty of "
"MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the "
"GNU General Public License for more details."
"\r\n\r\n"
"You should have received a copy of the GNU General Public License "
"along with SketchFlat.  If not, see <http://www.gnu.org/licenses/>.",
        "About SketchFlat", MB_OK | MB_ICONINFORMATION);
}
