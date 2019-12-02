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

#ifndef __SKETCHFLAT_H
#define __SKETCHFLAT_H

#include <windows.h>
#include <commctrl.h>
#include <commdlg.h>

#include <setjmp.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <math.h>

// Generally useful definitions
#define PI (3.14159265358979323846)
#define VERY_POSITIVE ( 1e40)
#define VERY_NEGATIVE (-1e40)
typedef signed short SWORD;
#define arraylen(x) (sizeof((x))/sizeof((x)[0]))
#define MAX_STRING      1024
#define DEGREES(x) ((x)*PI/180)


#include "sketch.h"
#include "expr.h"
#include "derived.h"


//--------------------------------------------
// in sketchflat.cpp
void Init(char *cmdLine);
void MenuFile(int id);
void MenuManual(int id);
void MenuAbout(int id);

//--------------------------------------------
// in draw.cpp
void UpdateStatusBar(void);
void ZoomToFit(void);

// Conversion functions, different ways to represent coordinates.
double toMicronsX(int x);
double toMicronsY(int y);
double toMicronsNotAffine(int r);
int toPixelsX(double x);
int toPixelsY(double y);
int toPixelsNotAffine(double r);
char *ToDisplay(double v);
double FromDisplay(const char *v);

// These following functions are called by the GUI code.
void MenuZoom(int id);
void KeyPressed(int key);
void MouseMoved(int x, int y, BOOL leftDown, BOOL rightDown, BOOL centerDown);
void LeftButtonDown(int x, int y);
void LeftButtonUp(int x, int y);
void LeftButtonDblclk(int x, int y);
void CenterButtonDown(int x, int y);
void ScrollWheelMoved(int x, int y, int delta);
void PaintWindow(void);
void UpdateAfterUnitChange(void);

extern int MouseLeftDownX, MouseLeftDownY;

//--------------------------------------------
// in draw_sketch.cpp
void SketchGetStatusBarDescription(char *s, char *solving, BOOL *red);
void SketchMouseMoved(int x, int y,
                            BOOL leftDown, BOOL rightDown, BOOL centerDown);
void SketchLeftButtonDblclk(int x, int y);
void SketchLeftButtonUp(int x, int y);
void SketchLeftButtonDown(int x, int y);
void SketchKeyPressed(int c);
void DrawSketch(void);
void SolvePerMode(BOOL dragging);
void NowUnsolved(void);
void StopSolving(void);
// These following functions are called by the GUI code.
void MenuEdit(int id);
void MenuDraw(int id);
void MenuHowToSolve(int id);
void SwitchToSketchMode(void);

// For the selection.
#define SEL_NONE        0
#define SEL_POINT       1
#define SEL_ENTITY      2
#define SEL_LINE        3
#define SEL_CONSTRAINT  4
typedef struct {
    int         which;
    hPoint      point;
    hEntity     entity;
    hLine       line;
    hConstraint constraint;
} SelState;
extern SelState Hover;
#define MAX_SELECTED_ITEMS  8
extern SelState Selected[MAX_SELECTED_ITEMS];
extern BOOL EmphasizeSelected;
void ClearHoverAndSelected(void);
void CancelSketchModeOperation(void);

#define SOLVING_AUTOMATICALLY           0
#define NOT_SOLVING_AFTER_PROBLEM       1
#define NOT_SOLVING                     2
extern int SolvingState;

//--------------------------------------------
// in draw_constraint.cpp
#define GET_DISTANCE_TO_CONSTRAINT  0
#define DRAW_CONSTRAINT             1
#define GET_LABEL_LOCATION          2
double ForDrawnConstraint(int op, SketchConstraint *c, double *x, double *y);
BOOL ConstraintHasLabelAssociated(SketchConstraint *c);
void ChangeConstraintValue(SketchConstraint *c, char *newVal);

//--------------------------------------------
// in layer.cpp
#define REFERENCE_LAYER 0x00ffffff
void UpdateLayerList(void);
hLayer GetCurrentLayer(void);
hLayer LayerForLine(hLine ln);
hLayer LayerForPoint(hPoint pt);
hLayer LayerForEntity(hEntity he);
BOOL LayerIsShown(hLayer layer);

// Callbacks from the GUI code.
void ButtonAddLayer(BOOL before);
void ButtonDeleteLayer(void);
void LayerDisplayNameUpdated(int p, char *str);
void LayerDisplayCheckboxChanged(int p, BOOL checked);
void LayerDisplayChanged(void);

//--------------------------------------------
// in constraint.cpp
typedef DWORD hEquation;
#define EQUATION_FOR_CONSTRAINT(eq, k)      (((eq) << 4) | (k))
#define CONSTRAINT_FOR_EQUATION(eq)         ((eq) >> 4)
#define CONSTRAINT_FOR_ENTITY(he)           ((hConstraint)((he) | 0x800000))
#define MAX_EQUATIONS (MAX_CONSTRAINTS_IN_SKETCH*2)
typedef struct {
    int eqns;
    struct {
        hEquation       he;
        Expr            *e;

        int             subSys;
    }   eqn[MAX_EQUATIONS];
} Equations;
extern Equations *EQ;

void MenuConstrain(int id);
void ConstrainCoincident(hPoint a, hPoint b);
void DeleteConstraint(hConstraint hc);
SketchConstraint *ConstraintById(hConstraint hc);

void MakeConstraintEquations(SketchConstraint *c);
void MakeEntityEquations(SketchEntity *e);

//--------------------------------------------
// in measure.cpp
void UpdateMeasurements(void);

//--------------------------------------------
// in sketch.cpp
double EvalParam(hParam p);
void EvalPoint(hPoint pt, double *x, double *y);
BOOL PointExistsInSketch(hPoint pt);
void ForcePoint(hPoint pt, double x, double y);
void ForceParam(hParam p, double v);
void ForceReferences(void);
void RestoreParamsToLastGood(void);
void SaveGoodParams(void);
void RestoreParamsToRemembered(void);
void GenerateParametersPointsLines(void);
SketchParam *ParamById(hParam p);
SketchEntity *EntityById(hEntity he);
void SketchDeleteEntity(hEntity he);
hEntity SketchAddEntity(int type);
void SketchAddPointToCubicSpline(hEntity he);
extern Sketch *SK;

//--------------------------------------------
// in curves.cpp
void GenerateCurvesAndPwls(double chordTol);
// These are callbacks from the TTF routines, to tell us where to put
// curves from the font.
void TtfLineSegment(DWORD ref, int x0, int y0, int x1, int y1);
void TtfBezier(DWORD ref, int x0, int y0, int x1, int y1, int x2, int y2);

//--------------------------------------------
// in ttf.cpp
void TtfGetDisplayName(char *file, char *str);
void TtfSelectFont(char *file);
void TtfPlotString(DWORD ref, char *str, double spacing);

//--------------------------------------------
// in polygon.cpp
void PolygonAssemble(DPolygon *p, SketchPwl *src, int n,
                                    hLayer lr, BOOL *leftovers);
BOOL PolygonUnion(DPolygon *dest, DPolygon *pa, DPolygon *pb);
BOOL PolygonDifference(DPolygon *dest, DPolygon *a, DPolygon *b);
BOOL PolygonSuperimpose(DPolygon *dest, DPolygon *a, DPolygon *b);
BOOL PolygonScale(DPolygon *dest, DPolygon *pa, double scale);
BOOL PolygonStepRotating(DPolygon *dest, DPolygon *src, 
                            double xa, double ya,
                            double theta0, double thetas, int n);
BOOL PolygonStepTranslating(DPolygon *dest, DPolygon *src, 
                            double dx0, double dy0, double dxs, double dys,
                            int n);
BOOL PolygonMirror(DPolygon *dest, DPolygon *src, 
                            double x0, double y0, double x1, double y1);
BOOL PolygonOffset(DPolygon *dest, DPolygon *src, double radius);
BOOL PolygonRoundCorners(DPolygon *dest, DPolygon *src, double radius, 
                                                        hPoint *pt, int pts);
BOOL PolygonPerforate(DPolygon *dest, DPolygon *src, double len, double duty);

//--------------------------------------------
// in derive.cpp
void GenerateDeriveds(void);

// Called by GUI code.
void SwitchToDeriveMode(void);
void DerivedItemsListToggleShown(int i);
void DerivedItemsListEdit(int i);
void ButtonShowAllDerivedItems(void);
void ButtonHideAllDerivedItems(void);
void MenuDerivedUnselect(int id);
void MenuDerive(int id);
// indirectly, through draw.cpp
void DrawDerived(void);
void DerivedLeftButtonDown(int x, int y);
void DerivedMouseMoved(int x, int y,
                            BOOL leftDown, BOOL rightDown, BOOL centerDown);

//--------------------------------------------
// in newton.cpp
#define MAX_NUMERICAL_UNKNOWNS 40
#define MAX_UNKNOWNS_AT_ONCE   128
BOOL SolveNewton(int subSys);

//--------------------------------------------
// in assume.cpp
BOOL Assume(int *assumed);
// Callbacks for the lists of inconsistent constraints and assumed parameters;
// from the GUI code.
void HighlightAssumption(char *str);
void HighlightConstraint(char *str);

//--------------------------------------------
// in solve.cpp
void SatisfyCoincidenceConstraints(hPoint pt);
void MarkUnknowns(void);
void GenerateEquationsToSolve(void);
void Solve(void);

// State that tells us how to partiion the equations in order to solve
// them. We want to remember this, because it's expensive to generate
// by brute force. Ideally, we will build this up slowly as the user
// draws their sketch, and never have to generate it from scratch.
#define MAX_REMEMBERED_SUBSYSTEMS 256
typedef struct {
    struct {
        // We should either try assuming a parameter
        hParam      p;
        // or solving a particular subsystem
        hEquation   eq[MAX_NUMERICAL_UNKNOWNS];
        int         eqs;
        // and this set has not been discarded as useless.
        BOOL        use;
    } set[MAX_REMEMBERED_SUBSYSTEMS];
    int     sets;
} RememberedSubsystems;
// These have to be extern because they are saved to the .skf file, since
// they're so expensive to regenerate.
extern RememberedSubsystems *RSt;
extern RememberedSubsystems *RSp;

//--------------------------------------------
// in loadsave.cpp
BOOL SaveToFile(char *name);
BOOL LoadFromFile(char *name);
void NewEmptyProgram(void);

//--------------------------------------------
// in undoredo.cpp
extern BOOL ProgramChangedSinceSave;
void UndoRemember(void);
void UndoFlush(void);
void MenuUndo(int id);

//--------------------------------------------
// in util.cpp
void dbp(const char *str, ...);
void dbp2(const char *str, ...);
// For a fatal error; print a message and die.
#define oops() do { \
        uiError((char*)"at file " __FILE__ " line %d", __LINE__); \
        dbp((char*)"at file " __FILE__ " line %d", __LINE__); \
        if(0) *((char *)0) = 1; \
        exit(-1); \
    } while(0)
// For a non-fatal error; print a message the first few times it happens,
// then shut up about it.
#define oopsnf() do { \
        static int ErrorCount; \
        if(ErrorCount < 3) { \
            uiError((char*)"Internal error at file " __FILE__ " line %d", __LINE__); \
            dbp((char*)"at file " __FILE__ " line %d", __LINE__); \
            ErrorCount++; \
        } \
    } while(0)
#define DEFAULT_TOL (0.001)
BOOL tol(double a, double b);
int toint(double v);
BOOL told(double a, double b);
BOOL tola(double a, double b);
BOOL SolveLinearSystem(double X[], double A[][MAX_UNKNOWNS_AT_ONCE], 
                                                        double B[], int n);

void LineOrLineSegment(hLine ln, hEntity e,
                            double *x0, double *y0, double *dx, double *dy);
BOOL IntersectionOfLines(double x0A, double y0A, double dxA, double dyA,
                         double x0B, double y0B, double dxB, double dyB,
                         double *xi, double *yi);
BOOL IntersectionOfLinesGetT(double x0A, double y0A, double dxA, double dyA,
                             double x0B, double y0B, double dxB, double dyB,
                             double *tA, double *tB);
double DistanceFromPointToLine(double xp, double yp,
                               double x0, double y0,
                               double dx, double dy, BOOL segment);
void ClosestPointOnLineToPoint(double *xc, double *yc,
                               double xp, double yp,
                               double x0, double y0, double dx, double dy);
double Distance(double x0, double y0, double x1, double y1);
double Magnitude(double x, double y);
void UnitVector(double *x, double *y);
void RepAsPointOnLineAndDirection(double theta, double a,
                double *x0, double *y0, double *dx, double *dy);
void RepAsAngleAndDistance(double x0, double y0, double dx, double dy,
                                double *theta, double *a);
void LineToParametric(hLine ln, double *x0, double *y0, double *dx, double *dy);
void LineToPointsOnEdgeOfViewport(hLine ln, double *x1, double *y1,
                                            double *x2, double *y2);

typedef struct {
    hPoint      point[MAX_SELECTED_ITEMS];
    hEntity     entity[MAX_SELECTED_ITEMS];
    hLine       line[MAX_SELECTED_ITEMS];

    int         n;

    int         points;
    int         entities;
    int         lines;

    int         anyLines;
    int         nonLineEntities;
    int         circlesOrArcs;
    int         anyDirections;
} GroupedSelection;
void GroupSelection(GroupedSelection *g);

//--------------------------------------------
// in export.cpp
void MenuExport(int id);

//--------------------------------------------
// in win32util.cpp
BOOL uiGetSaveFile(const char *file, const char *defExtension, const char *selPattern);
BOOL uiGetOpenFile(const char *file, const char *defExtension, const char *selPattern);
int uiSaveFileYesNoCancel(void);

void MenuSettings(int id);
BOOL uiShowConstraints(void);
BOOL uiShowDatumItems(void);
BOOL uiUseInches(void);

void uiCheckMenuById(int id, BOOL checked);
void uiEnableMenuById(int id, BOOL enabled);

void uiSetCursorToHourglass(void);
void uiRestoreCursor(void);
BOOL uiShiftKeyDown();

void uiError(const char *str, ...);

void Free(void *p);
void *Alloc(int bytes);
void FreeAll(void);
void Exit(void);

void DFree(void *p);
void *DAlloc(int bytes);

//--------------------------------------------
// in win32main_window.cpp
void uiSetStatusBarText(char *solving, BOOL red, char *x, char *y, char *msg);

BOOL uiInSketchMode(void);
void uiForceSketchMode(void);

void uiSetMeasurementAreaText(const char *str);
#define BK_GREY     0
#define BK_YELLOW   1
#define BK_GREEN    2
#define BK_VIOLET   3
void uiSetConsistencyStatusText(const char *str, int bk);
void uiSetMainWindowTitle(char *str);

void uiClearAssumptionsList(void);
void uiAddToAssumptionsList(char *str);

void uiClearConstraintsList(void);
void uiAddToConstraintsList(char *str);

int uiGetLayerListSelection(void);
void uiClearLayerList(void);
void uiAddToLayerList(BOOL shown, char *str);
void uiSelectInLayerList(int p);

void uiAddToDerivedItemsList(int i, char *str,
                                        char *subA, char *subB, char *subC);
void uiClearDerivedItemsList(void);
void uiBoldDerivedItem(int i, BOOL bold);
int uiGetSelectedDerivedItem(void);
BOOL uiPointsShownInDeriveMode(void);

void uiShowTextEntryBoxAt(char *initial, int x, int y);
void uiHideTextEntryBox(void);
void uiGetTextEntryBoxText(char *dest);
BOOL uiTextEntryBoxIsVisible(void);

void uiRepaint(void);

void PltGetRegion(int *xMin, int *yMin, int *xMax, int *yMax);
void PltMoveTo(int x, int y);
void PltLineTo(int x, int y);
void PltCircle(int x, int y, int r);
void PltRect(int x0, int y0, int x1, int y1);
void PltText(int x, int y, BOOL boldFont, const char *s, ...);
#define MAX_COLORS 25
#define LAYER_COLOR(x)          (x)
#define HOVER_COLOR             16
#define SELECTED_COLOR          17
#define DATUM_COLOR             18
#define REFERENCES_COLOR        19
#define CONSTRAINTS_COLOR       20
#define CONSTRUCTION_COLOR      21
#define UNSELECTED_LAYER_COLOR  22
#define ASSUMPTIONS_COLOR       23
void PltSetColor(int c);
void PltSetDashed(BOOL dashed);

#define PltMoveToMicrons(x, y) PltMoveTo(toPixelsX(x), toPixelsY(y))
#define PltLineToMicrons(x, y) PltLineTo(toPixelsX(x), toPixelsY(y))
#define PltRectMicrons(x0, y0, x1, y1) \
    PltRect(toPixelsX(x0), toPixelsY(y0), toPixelsX(x1), toPixelsY(y1))
#define PltCircleMicrons(x, y, r) PltCircle(toPixelsX(x), toPixelsY(y) \
                                                toPixelsNotAffine(r))

//--------------------------------------------
// in win32text.cpp
void txtuiGetTextForDrawing(char *text, char *font, double *spacing);
void txtuiGetDefaultFont(char *str);

//--------------------------------------------
// in win32simple.cpp
BOOL uiShowSimpleDialog(const char *title, int boxes, const char **labels,
    DWORD numMask, hDerived *destH,  char **destS);


#include "ui.h"


#endif
