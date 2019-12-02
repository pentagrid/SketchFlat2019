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

#ifndef __UI_H
#define __UI_H

#define MNU_NEW                         0x1000
#define MNU_OPEN                        0x1001
#define MNU_SAVE                        0x1002
#define MNU_SAVE_AS                     0x1003
#define MNU_EXIT                        0x1004
#define MNU_EXPORT_FIRST              0x1010
#define MNU_EXPORT_DXF                  0x1010
#define MNU_EXPORT_HPGL                 0x1011
#define MNU_EXPORT_G_CODE               0x1012
#define MNU_EXPORT_LAST               0x1012

#define MNU_EDIT_DELETE_FROM_SKETCH     0x2000
#define MNU_EDIT_DELETE_DERIVED         0x2001
#define MNU_EDIT_UNSELECT_ALL           0x2002
#define MNU_EDIT_UNSELECT_ALL_POINTS    0x2003
#define MNU_EDIT_UNDO                   0x2004
#define MNU_EDIT_REDO                   0x2005
#define MNU_EDIT_BRING_TO_LAYER         0x2006

#define MNU_VIEW_ZOOM_IN                0x3000
#define MNU_VIEW_ZOOM_OUT               0x3001
#define MNU_VIEW_ZOOM_TO_FIT            0x3002
#define MNU_VIEW_IN_INCHES              0x3010
#define MNU_VIEW_IN_MM                  0x3011
#define MNU_VIEW_SHOW_CONSTRAINTS       0x3020
#define MNU_VIEW_SHOW_DATUM_ITEMS       0x3021

#define MNU_DRAW_FIRST                0x4000
#define MNU_DRAW_DATUM_POINT            0x4000
#define MNU_DRAW_DATUM_LINE             0x4001
#define MNU_DRAW_LINE_SEGMENT           0x4002
#define MNU_DRAW_CIRCLE                 0x4003
#define MNU_DRAW_ARC                    0x4004
#define MNU_DRAW_CUBIC_SPLINE           0x4005
#define MNU_DRAW_TEXT                   0x4006
#define MNU_DRAW_FROM_IMPORTED          0x4007
#define MNU_TOGGLE_CONSTRUCTION         0x4008
#define MNU_DRAW_LAST                 0x4008

#define MNU_CONSTR_FIRST              0x6000
#define MNU_CONSTR_DISTANCE             0x6000
#define MNU_CONSTR_ANGLE                0x6001
#define MNU_CONSTR_SUPPLEMENTARY        0x6002
#define MNU_CONSTR_COINCIDENT           0x6003
#define MNU_CONSTR_EQUAL                0x6004
#define MNU_CONSTR_MIDPOINT             0x6005
#define MNU_CONSTR_SYMMETRIC            0x6006
#define MNU_CONSTR_PARALLEL             0x6007
#define MNU_CONSTR_PERPENDICULAR        0x6008
#define MNU_CONSTR_HORIZONTAL           0x6009
#define MNU_CONSTR_VERTICAL             0x600a
#define MNU_CONSTR_DRAG_HORIZ           0x600b
#define MNU_CONSTR_DRAG_VERT            0x600c
#define MNU_CONSTR_DRAG_ANGLE           0x600d
#define MNU_CONSTR_SCALE_MM             0x600e
#define MNU_CONSTR_SCALE_INCH           0x600f
#define MNU_CONSTR_SOLVE_AUTO           0x6010
#define MNU_CONSTR_DONT_SOLVE           0x6011
#define MNU_CONSTR_SOLVE_NOW            0x6012
#define MNU_CONSTR_LAST               0x6012

#define MNU_DERIVE_FIRST              0x7000
#define MNU_DERIVE_OFFSET               0x7000
#define MNU_DERIVE_UNION                0x7001
#define MNU_DERIVE_DIFFERENCE           0x7002
#define MNU_DERIVE_SUPERIMPOSE          0x7003
#define MNU_DERIVE_ROUND                0x7004
#define MNU_DERIVE_STEP_TRANSLATE       0x7005
#define MNU_DERIVE_STEP_ROTATE          0x7006
#define MNU_DERIVE_SCALE                0x7007
#define MNU_DERIVE_MIRROR               0x7008
#define MNU_DERIVE_PERFORATE            0x7009
#define MNU_DERIVE_LAST               0x7009

#define MNU_MANUAL                      0x8000
#define MNU_ABOUT                       0x8001

#ifdef CREATE_MENU_TABLES
#define CTRL(x) ((x) - 'A' + 1)
typedef void MenuHandler(int id);
static struct {
    int         level;          // 0 == on menu bar, 1 == one level down, ...
    char       *label;          // or NULL for a separator
    int         accelerator;    // keyboard accelerator to do same
    int         id;             // unique ID
    MenuHandler *fn;
} Menus[] = {
    { 0, (char*)"&File",                           0,          0,                          NULL },
    { 1, (char*)"&New\tCtrl+N",                    CTRL('N'),  MNU_NEW,                    MenuFile },
    { 1, (char*)"&Open...\tCtrl+O",                CTRL('O'),  MNU_OPEN,                   MenuFile },
    { 1, (char*)"&Save\tCtrl+S",                   CTRL('S'),  MNU_SAVE,                   MenuFile },
    { 1, (char*)"Save &As...",                     0,          MNU_SAVE_AS,                MenuFile },
    { 1, NULL,                              0,          0,                          NULL },
    { 1, (char*)"Export &DXF...\tCtrl+D",          CTRL('D'),  MNU_EXPORT_DXF,             MenuExport },
    { 1, (char*)"Export &HPGL...\tCtrl+H",         CTRL('H'),  MNU_EXPORT_HPGL,            MenuExport },
    { 1, (char*)"Export &G Code...\tCtrl+G",       CTRL('G'),  MNU_EXPORT_G_CODE,          MenuExport },
    { 1, NULL,                              0,          0,                          NULL },
    { 1, (char*)"E&xit",                           0,          MNU_EXIT,                   MenuFile },

    { 0, (char*)"&Edit",                           0,          0,                          NULL },
    { 1, (char*)"&Undo\tCtrl+Z",                   CTRL('Z'),  MNU_EDIT_UNDO,              MenuUndo },
    { 1, (char*)"&Redo\tCtrl+Y",                   CTRL('Y'),  MNU_EDIT_REDO,              MenuUndo },
    { 1, NULL,                              0,          0,                          NULL },
    { 1, (char*)"Delete From &Sketch\tDel",        VK_DELETE,  MNU_EDIT_DELETE_FROM_SKETCH,MenuEdit },
    { 1, (char*)"Delete &Derived Item\tDel",       VK_DELETE,  MNU_EDIT_DELETE_DERIVED,    MenuDerive },
    { 1, NULL,                              0,          0,                          NULL },
    { 1, (char*)"Bring To This &Layer\t6",         '6',        MNU_EDIT_BRING_TO_LAYER,    MenuEdit },
    { 1, NULL,                              0,          0,                          NULL },
    { 1, (char*)"&Unselect All\tEsc",              VK_ESCAPE,  MNU_EDIT_UNSELECT_ALL,      MenuEdit },
    { 1, (char*)"&Unselect All Points\tEsc",       VK_ESCAPE,  MNU_EDIT_UNSELECT_ALL_POINTS,MenuDerivedUnselect },

    { 0, (char*)"&View",                           0,          0,                          NULL },
    { 1, (char*)"Zoom &In\t+",                     VK_OEM_PLUS,MNU_VIEW_ZOOM_IN,           MenuZoom },
    { 1, (char*)"Zoom &Out\t-",                    VK_OEM_MINUS,MNU_VIEW_ZOOM_OUT,         MenuZoom },
    { 1, (char*)"Zoom To &Fit\tF",                 'F',        MNU_VIEW_ZOOM_TO_FIT,       MenuZoom },
    { 1, NULL,                              0,          0,                          NULL },
    { 1, (char*)"Show &Constraints\t1",            '1',        MNU_VIEW_SHOW_CONSTRAINTS,  MenuSettings },
    { 1, (char*)"Show &Datum Items\t2",            '2',        MNU_VIEW_SHOW_DATUM_ITEMS,  MenuSettings },
    { 1, NULL,                              0,          0,                          NULL },
    { 1, (char*)"Dimensions in &Inches",           0,          MNU_VIEW_IN_INCHES,         MenuSettings },
    { 1, (char*)"Dimensions in &Millimeters",      0,          MNU_VIEW_IN_MM,             MenuSettings },

    { 0, (char*)"&Sketch",                         0,          0,                          NULL },
    { 1, (char*)"Datum &Point\tP",                 'P',        MNU_DRAW_DATUM_POINT,       MenuDraw },
    { 1, (char*)"Datum &Line\tL",                  'L',        MNU_DRAW_DATUM_LINE,        MenuDraw },
    { 1, NULL,                              0,          0,                          NULL },
    { 1, (char*)"Line &Segment\tS",                'S',        MNU_DRAW_LINE_SEGMENT,      MenuDraw },
    { 1, (char*)"&Circle\tC",                      'C',        MNU_DRAW_CIRCLE,            MenuDraw },
    { 1, (char*)"&Arc of a Circle\tA",             'A',        MNU_DRAW_ARC,               MenuDraw },
    { 1, (char*)"Cubic &Spline\t3",                '3',        MNU_DRAW_CUBIC_SPLINE,      MenuDraw },
    { 1, NULL,                              0,          0,                          NULL },
    { 1, (char*)"&Text\tT",                        'T',        MNU_DRAW_TEXT,              MenuDraw },
    { 1, (char*)"&Imported From File\tI",          'I',        MNU_DRAW_FROM_IMPORTED,     MenuDraw },
    { 1, NULL,                              0,          0,                          NULL },
    { 1, (char*)"To&ggle Construction\tG",         'G',        MNU_TOGGLE_CONSTRUCTION,    MenuDraw },

    { 0, (char*)"&Constrain",                      0,          0,                          NULL },
    { 1, (char*)"&Distance / Diameter\tD",         'D',        MNU_CONSTR_DISTANCE,        MenuConstrain },
    { 1, (char*)"A&ngle\tN",                       'N',        MNU_CONSTR_ANGLE,           MenuConstrain },
    { 1, (char*)"Other S&upplementary Angle\tU",   'U',        MNU_CONSTR_SUPPLEMENTARY,   MenuConstrain },
    { 1, NULL,                              0,          0,                          NULL },
    { 1, (char*)"&Horizontal\tH",                  'H',        MNU_CONSTR_HORIZONTAL,      MenuConstrain },
    { 1, (char*)"&Vertical\tV",                    'V',        MNU_CONSTR_VERTICAL,        MenuConstrain },
    { 1, NULL,                              0,          0,                          NULL },
    { 1, (char*)"Coincident / &On Curve\tO",       'O',        MNU_CONSTR_COINCIDENT,      MenuConstrain },
    { 1, (char*)"E&qual Length / Radius\tQ",       'Q',        MNU_CONSTR_EQUAL,           MenuConstrain },
    { 1, (char*)"At &Midpoint\tM",                 'M',        MNU_CONSTR_MIDPOINT,        MenuConstrain },
    { 1, (char*)"S&ymmetric\tY",                   'Y',        MNU_CONSTR_SYMMETRIC,       MenuConstrain },
    { 1, (char*)"Parall&el\tE",                    'E',        MNU_CONSTR_PARALLEL,        MenuConstrain },
    { 1, (char*)"Perpendicula&r\tR",               'R',        MNU_CONSTR_PERPENDICULAR,   MenuConstrain },
    { 1, NULL,                              0,          0,                          NULL },
    { 1, (char*)"Import Scale in &Millimeters",    0,          MNU_CONSTR_SCALE_MM,        MenuConstrain },
    { 1, (char*)"Import Scale in &Inches",         0,          MNU_CONSTR_SCALE_INCH,      MenuConstrain },
    { 1, NULL,                              0,          0,                          NULL },
    { 1, (char*)"Draggable &Horizontally\t[",      VK_OEM_4,   MNU_CONSTR_DRAG_HORIZ,      MenuConstrain },
    { 1, (char*)"Draggable &Vertically\t]",        VK_OEM_6,   MNU_CONSTR_DRAG_VERT,       MenuConstrain },
    { 1, (char*)"Draggable &About Point\t\\",      VK_OEM_5,   MNU_CONSTR_DRAG_ANGLE,      MenuConstrain },
    { 1, NULL,                              0,          0,                          NULL },
    { 1, (char*)"Solve Automatically\tTab",        0,          MNU_CONSTR_SOLVE_AUTO,      MenuHowToSolve },
    { 1, (char*)"Don't Sol&ve\tTab",               0,          MNU_CONSTR_DONT_SOLVE,      MenuHowToSolve },
    { 1, NULL,                              0,          0,                          NULL },
    { 1, (char*)"Solve Once &Now\tSpace",          VK_SPACE,   MNU_CONSTR_SOLVE_NOW,       MenuHowToSolve },

    { 0, (char*)"&Derive",                         0,          0,                          NULL },
    { 1, (char*)"&Union\tU",                       'U',        MNU_DERIVE_UNION,           MenuDerive },
    { 1, (char*)"&Difference\tD",                  'D',        MNU_DERIVE_DIFFERENCE,      MenuDerive },
    { 1, (char*)"&Superimpose\tS",                 'S',        MNU_DERIVE_SUPERIMPOSE,     MenuDerive },
    { 1, NULL,                              0,          0,                          NULL },
    { 1, (char*)"Step and Repeat &Translate\tT",   'T',        MNU_DERIVE_STEP_TRANSLATE,  MenuDerive },
    { 1, (char*)"Step and Repeat &Rotate\tR",      'R',        MNU_DERIVE_STEP_ROTATE,     MenuDerive },
    { 1, (char*)"S&cale\tC",                       'C',        MNU_DERIVE_SCALE,           MenuDerive },
    { 1, (char*)"&Mirror\tM",                      'M',        MNU_DERIVE_MIRROR,          MenuDerive },
    { 1, NULL,                              0,          0,                          NULL },
    { 1, (char*)"&Perforate\tP",                   'P',        MNU_DERIVE_PERFORATE,       MenuDerive },
    { 1, NULL,                              0,          0,                          NULL },
    { 1, (char*)"&Offset Closed Curve\tO",         'O',        MNU_DERIVE_OFFSET,          MenuDerive },
    { 1, (char*)"Round Sharp Cor&ner\tC",          'N',        MNU_DERIVE_ROUND,           MenuDerive },

    { 0, (char*)"&Help",                           0,          0,                          NULL },
//    { 1, "&Manual\tF1",                     VK_F1,      MNU_MANUAL,                 MenuManual },
    { 1, (char*)"&About\t",                        0,          MNU_ABOUT,                  MenuAbout },
};
#endif

#ifdef CREATE_COLOR_TABLES
static struct {
    int r;
    int g;
    int b;
} Colors[MAX_COLORS] = {
    { 240, 240, 240 },      // Layer  0 drawing
    {   0, 255, 255 },      // Layer  1 drawing
    { 255,   0, 255 },      // Layer  2 drawing
    { 100, 100, 255 },      // Layer  3 drawing
    { 150, 150,   0 },      // Layer  4 drawing
    { 110, 115, 110 },      // Layer  5 drawing
    { 255,   0, 255 },      // Layer  6 drawing
    { 100, 100, 100 },      // Layer  7 drawing
    { 255, 255, 255 },      // Layer  8 drawin6
    { 255, 100, 100 },      // Layer  9 drawing
    { 100, 255, 100 },      // Layer 10 drawing
    { 100, 100, 255 },      // Layer 11 drawing
    { 255, 255,   0 },      // Layer 12 drawing
    {   0, 255, 255 },      // Layer 13 drawing
    { 255,   0, 255 },      // Layer 14 drawing
    { 100, 100, 100 },      // Layer 15 drawing

    { 255, 255,   0 },      // Hover color
    { 255,  50,  50 },      // Selected color
    {   0, 140,   0 },      // Datum (excluding references) color
    {   0,  30, 150 },      // References color
    { 200,   0, 200 },      // Constraints color
    {   0, 100,   0 },      // Construction lines color
    {   0,  70,  70 },      // Unselected layer color
    {  70,  90,   0 },      // Unselected layer color
};
#endif

#endif
