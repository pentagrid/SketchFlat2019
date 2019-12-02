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
// Drawing stuff specific to the sketch (vs. derive) mode of display.
// Jonathan Westhues, May 2007
//-----------------------------------------------------------------------------
#include "sketchflat.h"

#define OPERATION_NONE                  0
#define OPERATION_DRAGGING_PT           0x10
#define OPERATION_DRAGGING_PT_ON_ARC    0x11
#define OPERATION_DRAGGING_PT_ON_SPLINE 0x12
#define OPERATION_IS_DRAGGING_PT(x)     (((x) & 0xfffffff0) == 0x10)
#define OPERATION_DRAGGING_LINE         0x20
#define OPERATION_DRAGGING_RADIUS       0x30
#define OPERATION_DRAGGING_OFFSET       0x40
// Or any of the MNU_ constants mean that we will start that operation with
// the next mouse click.
static int CurrentOperation;

// There's two cases where we ended up dragging something: right after
// creating it, in which case you have to click to drop the point, and
// when you're modifying an existing entity, in which case you're dragging
// with the mouse down, and you release the mouse to drop it. Those must
// be distinguished.
static BOOL DropDraggedOnMouseUp = FALSE;

static BOOL CancelledOperationWasNewLineSegment = FALSE;

// If the user has clicked on something and is dragging it behind them
// (e.g. a point that they are moving, or a new line that they are
// drawing), then this is where we keep that state.
static struct {
    hPoint      point;
    hParam      param;
    hLine       line;
    hEntity     entity; 
    DoublePoint *offset;
    DoublePoint ref;
    int         i;
} Dragging;

// If the mouse moves, then don't solve immediately; if we did then we might
// end up solving many times but not displaying our solutions, so that the
// display would look sluggish. Instead, schedule a solve to be performed
// before we repaint the next time. This guarantees one paint per solve.
static BOOL SolveBeforeNextPaint = FALSE;

// How the constraints are being handled: ignored, solved, or ignore while
// dragging but solved after each change to the constraints/entities.
int SolvingState;

// Which item, if any, is the mouse over? This one should be drawn
// highlighted.
static SelState Hover;
// And which items has the user clicked on? Here we keep a list of
// multiple items, so that e.g. the user can select two points, and
// then constrain them coincident.
SelState Selected[MAX_SELECTED_ITEMS];
// If the user changes the selection e.g. from the list of assumptions, then
// let's highlight that somewhat more, because it might otherwise be hard
// to find.
BOOL EmphasizeSelected;
// And the point that we will draw the line to.
static DoublePoint Emphasized;

// Determine what to highlight; things that are highlighted might later
// get selected. Depending on the current mode, a different set of things
// might be selectable.
static void CheckHover(int xPixels, int yPixels, DWORD mask);
#define HOVER_POINTS           1
#define HOVER_CONSTRAINTS      2
#define HOVER_PWLS             4
#define HOVER_LINES            8
#define HOVER_ALL              0xffffffff

//-----------------------------------------------------------------------------
// Solve whatever is appropriate given our mode.
//-----------------------------------------------------------------------------
void SolvePerMode(BOOL dragging)
{
    if(SolvingState == NOT_SOLVING_AFTER_PROBLEM && !dragging) {
        SolvingState = SOLVING_AUTOMATICALLY;
        UpdateStatusBar();
    }
    if(SolvingState == SOLVING_AUTOMATICALLY) {
        Solve();
    }
    uiRepaint();
}

//-----------------------------------------------------------------------------
// The sketch is modified but not yet re-solved; so we should clear out the
// lists of constraints and assumptions, since some might not exist, and
// notify the user.
//-----------------------------------------------------------------------------
void NowUnsolved(void)
{
    uiClearAssumptionsList();
    uiClearConstraintsList();
    uiSetConsistencyStatusText(" Not yet solved.", BK_GREY);

    int i;
    for(i = 0; i < SK->params; i++) {
        SK->param[i].assumed = NOT_ASSUMED;
    }
}

//-----------------------------------------------------------------------------
// Enter a mode where we don't automatically solve.
//-----------------------------------------------------------------------------
void StopSolving(void)
{
    if(SolvingState == SOLVING_AUTOMATICALLY) {
        SolvingState = NOT_SOLVING_AFTER_PROBLEM;
    }
    UpdateStatusBar();
}

void SketchGetStatusBarDescription(char *str, char *solving, BOOL *red)
{
    const char *s;
    switch(CurrentOperation) {
        case MNU_DRAW_DATUM_POINT:
            s = "Click to define datum point location.";
            break;

        case MNU_DRAW_DATUM_LINE:
            s = "Click to define a point on a datum line.";
            break;

        case MNU_DRAW_LINE_SEGMENT:
            s = "Click to define starting point of line segment.";
            break;

        case MNU_DRAW_CIRCLE:
            s = "Click to define center point of circle.";
            break;

        case MNU_DRAW_ARC:
            s = "Click to define one point on circular arc.";
            break;
        
        case MNU_DRAW_CUBIC_SPLINE:
            s = "Click to define point on cubic spline.";
            break;

        case MNU_DRAW_TEXT:
            s = "Click to define reference point for text.";
            break;

        case MNU_DRAW_FROM_IMPORTED:
            s = "Click to define reference pointed for imported file.";
            break;

        case OPERATION_DRAGGING_PT:
        case OPERATION_DRAGGING_PT_ON_ARC:
        case OPERATION_DRAGGING_PT_ON_SPLINE:
        case OPERATION_DRAGGING_LINE:
        case OPERATION_DRAGGING_RADIUS:
        case OPERATION_DRAGGING_OFFSET:
            if(DropDraggedOnMouseUp) {
                s = "Release mouse button to place object.";
            } else {
                s = "Click to place object.";
            }
            break;

        case OPERATION_NONE:
            s = "Use menus to choose an operation, or drag an object to "
                "move it, or click an object to select it.";
            break;

        default:
            s = "???";
            CurrentOperation = OPERATION_NONE;
            break;
    }

    strcpy(str, s);

    switch(SolvingState) {
        case SOLVING_AUTOMATICALLY:
            strcpy(solving, " solving automatically"); *red = FALSE;
            break;
        
        case NOT_SOLVING_AFTER_PROBLEM:
            strcpy(solving, " can't solve, stopped"); *red = TRUE;
            break;

        case NOT_SOLVING:
            strcpy(solving, " solver disabled"); *red = TRUE;
            break;
    }

    uiCheckMenuById(MNU_CONSTR_SOLVE_AUTO,
                                    (SolvingState==SOLVING_AUTOMATICALLY));
    uiCheckMenuById(MNU_CONSTR_DONT_SOLVE,
                                    (SolvingState==NOT_SOLVING));
}

//-----------------------------------------------------------------------------
// Clear the list of selected items, plus the highlighted state.
//-----------------------------------------------------------------------------
void ClearHoverAndSelected(void)
{
    int i;
    for(i = 0; i < arraylen(Selected); i++) {
        Selected[i].which = SEL_NONE;
    }
    Hover.which = SEL_NONE;
    uiRepaint();
    UpdateMeasurements();
}

//-----------------------------------------------------------------------------
// Cancel whatever the current operation is; many menu items are still
// accessible via keyboard shortcuts while dragging, and those will do bad
// things.
//-----------------------------------------------------------------------------
void CancelSketchModeOperation(void)
{
    // Need to remember this, since Escape key works a bit differently.
    if(!DropDraggedOnMouseUp && CurrentOperation == OPERATION_DRAGGING_PT) {
        CancelledOperationWasNewLineSegment = TRUE;
    } else {
        CancelledOperationWasNewLineSegment = FALSE;
    }

    CurrentOperation = OPERATION_NONE;
    UpdateStatusBar();
    DropDraggedOnMouseUp = FALSE;
}

//-----------------------------------------------------------------------------
// Toggle the selection state of the hovered item: if it's selected, then
// unselect it, and if it's not, then select it.
//-----------------------------------------------------------------------------
static void ToggleSelectForHoveredItem(void)
{
    if(Hover.which == SEL_NONE) return;

    int i;
    // If an item is highlighted (because we're hovering over it),
    // then we should toggle its selection state.
    // First, see if this item already appears in the selected
    // list. In that case, the user wishes to remove it.
    for(i = 0; i < arraylen(Selected); i++) {
        if(Selected[i].which != Hover.which) continue;
        if(Hover.which == SEL_POINT &&
            Selected[i].point != Hover.point) continue;
        if(Hover.which == SEL_ENTITY &&
            Selected[i].entity != Hover.entity) continue;
        if(Hover.which == SEL_LINE &&
            Selected[i].line != Hover.line) continue;
        if(Hover.which == SEL_CONSTRAINT &&
            Selected[i].constraint != Hover.constraint) continue;

        // Looks like it's us, so clear it out.
        Selected[i].which = SEL_NONE;
        break;
    }
    if(i >= arraylen(Selected)) {
        // Add the currently hovered item to the selected list, if
        // there's space for it somewhere.
        for(i = 0; i < arraylen(Selected); i++) {
            if(Selected[i].which == SEL_NONE) {
                memcpy(&(Selected[i]), &Hover, sizeof(Hover));
                break;
            }
        }
    }
    uiRepaint();
    UpdateMeasurements();
}

//-----------------------------------------------------------------------------
// See if we're close enough to anything (a pwl segment or a point) that
// we should highlight it, to indicate that it will be selected when the
// user clicks.
//-----------------------------------------------------------------------------
static void CheckHover(int xPixels, int yPixels, DWORD mask)
{
    // If something's not shown, then it should not be selectable.
    if(!uiShowDatumItems()) {
        mask &= ~HOVER_POINTS;
        mask &= ~HOVER_LINES;
    }
    if(!uiShowConstraints()) {
        mask &= ~HOVER_CONSTRAINTS;
    }


    SelState old;
    memcpy(&old, &Hover, sizeof(old));

    Hover.which = SEL_NONE;

    double x = toMicronsX(xPixels);
    double y = toMicronsY(yPixels);

    double tol = toMicronsNotAffine(5);

    int i;

    double closestPointDistance;
    double closestConstraintDistance;
    double closestPwlDistance;
    double closestLineDistance;

    BOOL closestIsOnSelectedLayer = FALSE;
    hLayer cl = GetCurrentLayer();

    if(mask & HOVER_POINTS) {
        closestPointDistance = VERY_POSITIVE;
        for(i = 0; i < SK->points; i++) {
            hLayer layer = LayerForPoint(SK->point[i]);
            if(!LayerIsShown(layer)) continue;

            // Don't select the point being dragged! That would make it
            // hard for the automatic coincidence constraints.
            if(SK->point[i] == Dragging.point && 
                       OPERATION_IS_DRAGGING_PT(CurrentOperation)) continue;

            double xc, yc;
            EvalPoint(SK->point[i], &xc, &yc);

            // We'll do the points first. It's easy to select a curve, because
            // you can select it anywhere along its extent (and you can zoom
            // in if things are crowded). It's harder to select a line, so this
            // will make it easier.
            double d = Distance(x, y, xc, yc);
            BOOL closest;
            if(Hover.point == POINT_FOR_ENTITY(REFERENCE_ENTITY, 0)) {
                // Make it easy to select another point if the one we're
                // currently considering is the origin. The origin cannot
                // be deleted, so behaviour might otherwise be confusing.
                closest = (d < tol) && (d < (closestPointDistance + 10));
            } else {
                // Favor the points that we see earlier in the list; if two
                // points are on top of each other, then we should try to drag
                // then one that isn't going to get substituted into oblivion
                // when we solve.
                closest = (d < tol) && (d < (closestPointDistance - 10));
            }
            if(closest) {
                // We will preferentially select the points that are on our
                // selected layer. This improves our odds of getting one that
                // we can drag.
                if((layer == cl) || !closestIsOnSelectedLayer) {
                    Hover.which = SEL_POINT;
                    Hover.point = SK->point[i];
                    closestPointDistance = d;
                    closestIsOnSelectedLayer = (layer == cl);
                }
            }
        }
        if(closestPointDistance < VERY_POSITIVE) goto done;
    }

    if(mask & HOVER_CONSTRAINTS) {
        // Then the constraints; these are a mess of special cases, handled
        // elsewhere.
        closestConstraintDistance = VERY_POSITIVE;
        for(i = 0; i < SK->constraints; i++) {
            SketchConstraint *c = &(SK->constraint[i]);

            if(!LayerIsShown(c->layer)) continue;
            // Constraints are only drawn on the currently selected layer,
            // and therefore should only be selectable there.
            if(c->layer != cl) continue;

            double d =
                ForDrawnConstraint(GET_DISTANCE_TO_CONSTRAINT, c, &x, &y);

            if(d < tol && d < closestConstraintDistance) {
                Hover.which = SEL_CONSTRAINT;
                Hover.constraint = c->id;
                closestConstraintDistance = d;
            }
        }
        if(closestConstraintDistance < VERY_POSITIVE) goto done;
    }

    if(mask & HOVER_PWLS) {
        // Now do the piecewise linear segments; proximity to those selects
        // the entity that generated the curve that generated the pwl.
        closestPwlDistance = VERY_POSITIVE;
        for(i = 0; i < SK->pwls; i++) {
            SketchPwl *p = &(SK->pwl[i]);

            if(!LayerIsShown(p->layer)) continue;

            // Let's optimize a tiny bit; do a bounding box check before
            // proceeding with the actual measurement.
            double toll = tol*4;
            if(p->x0 < p->x1 && x < (p->x0 - toll)) continue;
            if(p->x0 < p->x1 && x > (p->x1 + toll)) continue;
            if(p->x0 > p->x1 && x < (p->x1 - toll)) continue;
            if(p->x0 > p->x1 && x > (p->x0 + toll)) continue;
            if(p->y0 < p->y1 && y < (p->y0 - toll)) continue;
            if(p->y0 < p->y1 && y > (p->y1 + toll)) continue;
            if(p->y0 > p->y1 && y < (p->y1 - toll)) continue;
            if(p->y0 > p->y1 && y > (p->y0 + toll)) continue;
           
            double d = DistanceFromPointToLine(
                    x, y,
                    p->x0, p->y0,
                    (p->x1 - p->x0), (p->y1 - p->y0), TRUE);

            if(d < tol && d < closestPwlDistance) {
                Hover.which = SEL_ENTITY;
                Hover.entity = p->id;
                closestPwlDistance = d;
            }
        }
        if(closestPwlDistance < VERY_POSITIVE) goto done;
    }

    if(mask & HOVER_LINES) {
        // And finally do the infinitely long lines.
        closestLineDistance = VERY_POSITIVE;
        for(i = 0; i < SK->lines; i++) {
            if(!LayerIsShown(LayerForLine(SK->line[i]))) continue;

            double x0, y0, dx, dy;
            LineToParametric(SK->line[i], &x0, &y0, &dx, &dy);
            
            double d = DistanceFromPointToLine(
                    x, y,
                    x0, y0, dx, dy, FALSE);

            if(d < tol && d < closestPwlDistance) {
                Hover.which = SEL_LINE;
                Hover.line = SK->line[i];
                closestLineDistance = d;
            }
        }
        if(closestLineDistance < VERY_POSITIVE) goto done;
    }

done:
    if(Hover.which != old.which || Hover.point != old.point ||
       Hover.entity != old.entity || Hover.line != old.line || 
       Hover.constraint != old.constraint)
    {
        uiRepaint();
    }
}

//-----------------------------------------------------------------------------
// Check if a given {whatever} lies on the selected list.
//-----------------------------------------------------------------------------
static BOOL PointIsSelected(hPoint hpt)
{
int i;
for(i = 0; i < arraylen(Selected); i++) {
    if(Selected[i].which == SEL_POINT && Selected[i].point == hpt) {
        return TRUE;
    }
}
return FALSE;
}
static BOOL EntityIsSelected(hEntity he)
{
int i;
for(i = 0; i < arraylen(Selected); i++) {
    if(Selected[i].which == SEL_ENTITY&& Selected[i].entity == he) {
        return TRUE;
    }
}
return FALSE;
}
static BOOL LineIsSelected(hLine hl)
{
int i;
for(i = 0; i < arraylen(Selected); i++) {
    if(Selected[i].which == SEL_LINE && Selected[i].line == hl) {
        return TRUE;
    }
}
return FALSE;
}
static BOOL ConstraintIsSelected(hConstraint hc)
{
    int i;
    for(i = 0; i < arraylen(Selected); i++) {
        if(Selected[i].which == SEL_CONSTRAINT &&
                                Selected[i].constraint == hc)
        {
            return TRUE;
        }
    }
    return FALSE;
}

void SwitchToSketchMode(void)
{
    uiRepaint();
}

//=============================================================================
// Helpers to draw each type of object that appears on the sketch.
//=============================================================================
#define DRAW_OTHERS     0
#define DRAW_HOVERED    1
#define DRAW_SELECTED   2
static hLayer CurrentLayer;

static void DrawSketchPwls(int which, BOOL thisLayerOnly)
{
    int i;

    if(which == DRAW_HOVERED && Hover.which != SEL_ENTITY) return;

    // The curves have been knocked down to piecewise linears (by the
    // same routines that we use to generate CAM data, though maybe
    // with a different chord tolerance), so trivial to plot.
    for(i = 0; i < SK->pwls; i++) {
        SketchPwl *p = &(SK->pwl[i]);

        if(which == DRAW_HOVERED && Hover.entity != p->id) continue;
        if(which == DRAW_SELECTED && !EntityIsSelected(p->id)) continue;

        if(!LayerIsShown(p->layer)) continue;
        if(thisLayerOnly && p->layer != CurrentLayer) continue;

        if(which == DRAW_OTHERS) {
            if(p->layer == CurrentLayer) {
                if(p->construction) {
                    PltSetColor(CONSTRUCTION_COLOR);
                } else {
                    PltSetColor(0);
                }
            } else {
                PltSetColor(UNSELECTED_LAYER_COLOR);
            }
        }
    
        PltMoveTo(toPixelsX(p->x0), toPixelsY(p->y0));
        PltLineTo(toPixelsX(p->x1), toPixelsY(p->y1));
    }
}
static void DrawSketchLines(int which, BOOL thisLayerOnly)
{
    int i;

    if(which == DRAW_HOVERED && Hover.which != SEL_LINE) return;

    // The datum lines are a pain, because they're infinite; we just want
    // to show whatever portion happens to correspond to our viewport.
    for(i = 0; i < SK->lines; i++) {
        hLine ln = SK->line[i];

        if(which == DRAW_HOVERED && Hover.line != ln) continue;
        if(which == DRAW_SELECTED && !LineIsSelected(ln)) continue;

        hLayer layer = LayerForLine(ln);
        if(!LayerIsShown(layer)) continue;
        if(thisLayerOnly && layer != CurrentLayer) continue;

        double x1, y1, x2, y2;
        LineToPointsOnEdgeOfViewport(ln, &x1, &y1, &x2, &y2);

        if(which == DRAW_SELECTED && EmphasizeSelected) {
            Emphasized.x = (x1 + x2)/2;
            Emphasized.y = (y1 + y2)/2;
        }

        if(which == DRAW_OTHERS) {
            if(ENTITY_FROM_LINE(ln) == REFERENCE_ENTITY) {
                PltSetColor(REFERENCES_COLOR);
            } else {
                if(CurrentLayer == layer) {
                    PltSetColor(DATUM_COLOR);
                } else {
                    PltSetColor(UNSELECTED_LAYER_COLOR);
                }
            }
        }
        PltSetDashed(TRUE);
        PltMoveTo(toPixelsX(x1), toPixelsY(y1));
        PltLineTo(toPixelsX(x2), toPixelsY(y2));
    }
}
static void DrawSketchConstraints(int which)
{
    int i;

    if(which == DRAW_HOVERED && Hover.which != SEL_CONSTRAINT) return;

    // The constraints all get drawn graphically as well.
    for(i = 0; i < SK->constraints; i++) {
        SketchConstraint *c = &(SK->constraint[i]);

        // Constraints are drawn only for the selected layer, even if
        // other layers are visible. And the current layer is always
        // visible, so no need to check for that.
        if(c->layer != CurrentLayer) continue;

        if(which == DRAW_HOVERED && Hover.constraint != c->id) continue;
        if(which == DRAW_SELECTED && !ConstraintIsSelected(c->id)) continue;

        double xe = VERY_POSITIVE, ye = VERY_POSITIVE;
        ForDrawnConstraint(DRAW_CONSTRAINT, &(SK->constraint[i]), &xe, &ye);
        if(xe < VERY_POSITIVE) {
            Emphasized.x = xe;
            Emphasized.y = ye;
        }
    }
}
static void DrawSketchPoints(int which, BOOL thisLayerOnly)
{
    int i;

    if(which == DRAW_HOVERED && Hover.which != SEL_POINT && 
                                Hover.which != SEL_ENTITY)
    {
        return;
    }

    // The points get highlighted with their entity, so that's a special case.
    for(i = 0; i < SK->points; i++) {
        hPoint p = SK->point[i];

        if(which == DRAW_HOVERED && !(
           (Hover.which == SEL_POINT && Hover.point == p) ||
           (Hover.which == SEL_ENTITY && Hover.entity == ENTITY_FROM_POINT(p))))
        {
            continue;
        }
        if(which == DRAW_SELECTED && !(PointIsSelected(p) ||
                            EntityIsSelected(ENTITY_FROM_POINT(p)))) continue;

        hLayer layer = LayerForPoint(p);
        if(!LayerIsShown(layer)) continue;
        if(thisLayerOnly && layer != CurrentLayer) continue;

        double xf, yf;
        EvalPoint(SK->point[i], &xf, &yf);

        int x = toPixelsX(xf);
        int y = toPixelsY(yf);

        // Record the location of this point, in case we are emphasizing it.
        // This also triggers when an entity is selected (e.g., a circle, for
        // its radius; so if a circle's radius is assumed then we emphasize
        // the circle's center.)
        if(which == DRAW_SELECTED) {
            Emphasized.x = xf;
            Emphasized.y = yf;
        }

        if(which == DRAW_OTHERS) {
            if(ENTITY_FROM_POINT(SK->point[i]) == REFERENCE_ENTITY) {
                PltSetColor(REFERENCES_COLOR);
            } else {
                if(CurrentLayer == LayerForPoint(p)) {
                    PltSetColor(DATUM_COLOR);
                } else {
                    PltSetColor(UNSELECTED_LAYER_COLOR);
                }
            }
        }

        PltSetDashed(FALSE);

        int d = 3;
        PltRect(x - d, y - (d + 1), x + d + 1, y + d);
    }
}
static void DrawSketchAssumptions(int which, BOOL thisLayerOnly)
{
    if(which != DRAW_OTHERS) return;

    int i;
    for(i = 0; i < SK->params; i++) {
        if(SK->param[i].assumed == NOT_ASSUMED) continue;

        hParam p = SK->param[i].id;
        if(!((p & X_COORD_FOR_PT(0)) || (p & Y_COORD_FOR_PT(0)))) {
            continue;
        }
        hPoint pt = POINT_FROM_PARAM(p);

        hLayer layer = LayerForPoint(pt);
        if(!LayerIsShown(layer)) continue;
        if(thisLayerOnly && layer != CurrentLayer) continue;

        // Let's draw a marker to indicate what we're assuming.
        double xp, yp;
        EvalPoint(pt, &xp, &yp);

        double a = toMicronsNotAffine(12);
        double b = toMicronsNotAffine(5);
        double xo, yo, dx, dy;

        if(p & X_COORD_FOR_PT(0)) {
            dx = a; dy = 0;
            xo = 0; yo = b;
        } else {
            dx = 0; dy = a;
            xo = b; yo = 0;
        }
        PltMoveToMicrons(xp + xo - dx, yp + yo - dy);
        PltLineToMicrons(xp + xo + dx, yp + yo + dy);
        PltMoveToMicrons(xp - xo - dx, yp - yo - dy);
        PltLineToMicrons(xp - xo + dx, yp - yo + dy);
    }
}
static void DrawSketchSetColors(int which)
{
    if(which == DRAW_OTHERS) {
        // Leave it wherever it was
    } else if(which == DRAW_SELECTED) {
        PltSetColor(SELECTED_COLOR);
    } else if(which == DRAW_HOVERED) {
        PltSetColor(HOVER_COLOR);
    }
    PltSetDashed(FALSE);
}

//-----------------------------------------------------------------------------
// Our callback from the GUI stuff. This is where the sketch is actually
// drawn, by calling the appropriate PltXXX() functions.
//-----------------------------------------------------------------------------
void DrawSketch(void)
{
    int i, j;

    if(SolveBeforeNextPaint) {
        SolvePerMode(TRUE);
        SolveBeforeNextPaint = FALSE;
    }

    Emphasized.x = VERY_POSITIVE;
    Emphasized.y = VERY_POSITIVE;

    // We'll cache the current (selected) layer, since we'll be using that
    // quite a lot.
    CurrentLayer = GetCurrentLayer();

    GenerateCurvesAndPwls(-1);

    // The order in which we draw the constraints determines what overlaps
    // what. In this case, let's plot everything, then plot the hovered
    // element, and then plot the selected. This means that the hovered
    // elements comes to the front, and when you click that element so
    // it's also selected, it changes color.
    for(i = DRAW_OTHERS; i <= DRAW_SELECTED; i++) {
        for(j = 0; j < 2; j++) {    
            // We also want to make sure that stuff on the active layer
            // gets drawn in front of everything else; so first plot
            // everything, then do the current layer, at the end.
            BOOL thisLayerOnly = (j == 1);

            if(uiShowDatumItems()) {
                PltSetColor(DATUM_COLOR);
                DrawSketchSetColors(i);
                DrawSketchLines(i, thisLayerOnly);
            }

            PltSetColor(0);
            DrawSketchSetColors(i);
            DrawSketchPwls(i, thisLayerOnly);

            if(uiShowConstraints()) {
                PltSetColor(ASSUMPTIONS_COLOR);
                DrawSketchSetColors(i);
                DrawSketchAssumptions(i, thisLayerOnly);

                PltSetColor(CONSTRAINTS_COLOR);
                DrawSketchSetColors(i);
                DrawSketchConstraints(i);
            }

            if(uiShowDatumItems()) {
                PltSetColor(DATUM_COLOR);
                DrawSketchSetColors(i);
                DrawSketchPoints(i, thisLayerOnly);
            }
        }
    }

    if(EmphasizeSelected) {
        if(Emphasized.x < VERY_POSITIVE) {
            int xMin, yMin, xMax, yMax;
            PltGetRegion(&xMin, &yMin, &xMax, &yMax);
            
            PltSetColor(1);
            PltMoveTo(xMin, yMax);
            PltLineToMicrons(Emphasized.x, Emphasized.y);
        }
        EmphasizeSelected = FALSE;
    }
}

//-----------------------------------------------------------------------------
// The user just tried to place the point pt at mouse location (x, y). If
// that happens to be on top of an existing point, then that point will be
// hovered, so we should place pt exactly on top of that existing point,
// and constrain them coincident. Otherwise we just set the current numerical
// position of the point.
//-----------------------------------------------------------------------------
static BOOL PlacePoint(hPoint pt, int x, int y)
{
    if(Hover.which == SEL_POINT && pt != Hover.point) {
        // Snap it to the highlighted point, and constrain those coincident
        // while we're at it.
        double xp, yp;
        EvalPoint(Hover.point, &xp, &yp);
        ForcePoint(pt, xp, yp);

        ConstrainCoincident(pt, Hover.point);
        ClearHoverAndSelected();
        return TRUE;
    } else {
        // Don't snap to a point; just place it where the mouse is.
        ForcePoint(pt, toMicronsX(x), toMicronsY(y));
        return FALSE;
    }
}

//=============================================================================
// Callbacks from the GUI, for menus (and keyboard accelerators).
//=============================================================================
void MenuHowToSolve(int id)
{
    switch(id) {
        case MNU_CONSTR_SOLVE_AUTO:
            SolvingState = SOLVING_AUTOMATICALLY;
            break;

        case MNU_CONSTR_DONT_SOLVE:
            SolvingState = NOT_SOLVING;
            break;

        case MNU_CONSTR_SOLVE_NOW:
            SK->eqnsDirty = TRUE;
            if(SolvingState == NOT_SOLVING) {
                UndoRemember();
                Solve();
                uiRepaint();
            } else {
                SolvePerMode(FALSE);
            }
            break;

    }
    UpdateStatusBar();
}

void MenuEdit(int id)
{
    if(uiTextEntryBoxIsVisible()) uiHideTextEntryBox();

    switch(id) {
        case MNU_EDIT_DELETE_FROM_SKETCH:
        case MNU_EDIT_BRING_TO_LAYER:
            // Both of these operations iterate over the selection, working
            // on the associated entities for datum points or lines.
            if(CurrentOperation == OPERATION_NONE) {
                UndoRemember();
                int i;
                for(i = 0; i < arraylen(Selected); i++) {
                    hEntity he;
                    if(Selected[i].which == SEL_NONE) continue;

                    if(Selected[i].which == SEL_CONSTRAINT) {
                        hConstraint hc = Selected[i].constraint;
                        if(id == MNU_EDIT_DELETE_FROM_SKETCH) {
                            DeleteConstraint(hc);
                        } else {
                            SketchConstraint *c = ConstraintById(hc);
                            if(c) c->layer = GetCurrentLayer();
                        }
                        continue;
                    }

                    if(Selected[i].which == SEL_POINT) {
                        he = ENTITY_FROM_POINT(Selected[i].point);
                        if(he == REFERENCE_ENTITY) continue;

                        // In general, it's not meaningful to delete a point,
                        // because the point is part of something bigger. The
                        // except is a datum point.
                        SketchEntity *e = EntityById(he);
                        if(e->type != ENTITY_DATUM_POINT) {
                            continue;
                        }
                    }

                    if(Selected[i].which == SEL_LINE) {
                        he = ENTITY_FROM_LINE(Selected[i].line);
                    }
                    if(Selected[i].which == SEL_ENTITY) {
                        he = Selected[i].entity;
                    }

                    // It's not meaningful to delete the references.
                    if(he == REFERENCE_ENTITY) continue;

                    if(id == MNU_EDIT_DELETE_FROM_SKETCH) {
                        SketchDeleteEntity(he);   
                    } else {
                        SketchEntity *e = EntityById(he);
                        e->layer = GetCurrentLayer();
                    }
                }
                ClearHoverAndSelected();
                NowUnsolved();
                SolvePerMode(FALSE);
                uiRepaint();
            }
            break;

        case MNU_EDIT_UNSELECT_ALL:
            if(CancelledOperationWasNewLineSegment) {
                hEntity he = ENTITY_FROM_POINT(Dragging.point);
                SketchEntity *e = EntityById(he);
                if(e->type == ENTITY_LINE_SEGMENT) {
                    SketchDeleteEntity(he);
                }
                CancelledOperationWasNewLineSegment = FALSE;
            }

            ClearHoverAndSelected();
            CurrentOperation = OPERATION_NONE;
            UpdateStatusBar();
            SolvePerMode(TRUE);
            uiRepaint();
            break;
    }
}

void MenuDraw(int id)
{
    if(uiTextEntryBoxIsVisible()) uiHideTextEntryBox();

    if(id == MNU_TOGGLE_CONSTRUCTION) {
        GroupedSelection gs;
        GroupSelection(&gs);
        if(gs.entities != 0 && gs.lines == 0 && gs.points == 0) {
            UndoRemember();
            int i;
            for(i = 0; i < gs.entities; i++) {
                SketchEntity *e = EntityById(gs.entity[i]);
                e->construction = !e->construction;
            }
            ClearHoverAndSelected();
        } else {
            uiError("Bad selection; you can toggle the construction status "
                  "of any number of entities (line segments, circles, etc.).");
        }
        return;
    }

    // All of these are multi-step operations, where nothing happens till
    // the user selects at least one thing. So just reset the current
    // operation, so that we know what the next mouse click means.
    CurrentOperation = id;

    UpdateStatusBar();
}

static void ChooseFileForImported(SketchEntity *e)
{
    if(!e) return;

    (void)uiGetOpenFile(e->file, NULL, 
            "HPGL file (*.plt; *.hpgl)\0*.plt;*.hpgl\0"
            "DXF file (*.dxf)\0*.dxf\0"
            "All files (*)\0*\0\0"
    );
}

void SketchKeyPressed(int key)
{
    if(uiTextEntryBoxIsVisible()) {
        if(key == VK_RETURN) {
            if(Hover.which == SEL_CONSTRAINT) {
                UndoRemember();

                SketchConstraint *c = ConstraintById(Hover.constraint);
                if(ConstraintHasLabelAssociated(c)) {
                    char buf[128];
                    uiGetTextEntryBoxText(buf);

                    ChangeConstraintValue(c, buf);
                }
            }
            uiHideTextEntryBox();
            ClearHoverAndSelected();

            SolvePerMode(FALSE);
        }
        return;
    }

    switch(key) {
        case VK_TAB:
            if(SolvingState == SOLVING_AUTOMATICALLY) {
                SolvingState = NOT_SOLVING;
            } else if(SolvingState == NOT_SOLVING) {
                SolvingState = SOLVING_AUTOMATICALLY;
            } else if(SolvingState == NOT_SOLVING_AFTER_PROBLEM) {
                SolvingState = NOT_SOLVING;
            } else {
                SolvingState = NOT_SOLVING;
            }
            UpdateStatusBar();
            break;
    }
}

//=============================================================================
// Mouse events. We get these only when the sketch tab is selected.
//=============================================================================
static void ForcePointWhereFree(hPoint pt, double x, double y)
{
    if(SolvingState == SOLVING_AUTOMATICALLY && !uiShiftKeyDown()) {
        int i;
        for(i = 0; i < 2; i++) {
            hParam hp = (i == 0) ? X_COORD_FOR_PT(pt) : Y_COORD_FOR_PT(pt);

            BOOL draggable = FALSE;

            // Is it draggable because we automatically assumed it?
            SketchParam *p = ParamById(hp);
            if(p->assumed == ASSUMED_FIX) {
                draggable = TRUE;
            }
            
            if(!draggable) {
                // Or draggable because it's explicitly constrained draggable?
                int j;
                for(j = 0; j < SK->constraints; j++) {
                    SketchConstraint *c = &(SK->constraint[j]);

                    // Horizontally/vertically
                    if(c->type == CONSTRAINT_FORCE_PARAM && c->paramA == hp) {
                        draggable = TRUE;
                        break;
                    }
                    // or about a point
                    if(c->type == CONSTRAINT_FORCE_ANGLE && c->ptB == pt) {
                        draggable = TRUE;
                        break;
                    }
                }
            }
            if(draggable) {
                ForceParam(hp, (i == 0) ? x : y);
            }
        }
    } else {
        ForcePoint(pt, x, y);
    }
}
void SketchMouseMoved(int x, int y,
                            BOOL leftDown, BOOL rightDown, BOOL centerDown)
{
    // To move an object, drag it (i.e., click, then move the mouse). Enforce
    // a bit of "static friction," so that the mouse has to move a bit before
    // we pick up.
    if(leftDown && CurrentOperation == OPERATION_NONE && 
        Distance(MouseLeftDownX, MouseLeftDownY, x, y) > 3)
    {
        if(Hover.which == SEL_POINT) {
            if(ENTITY_FROM_POINT(Hover.point) == REFERENCE_ENTITY) {
                // Let's not drag the origin around
            } else if(LayerForPoint(Hover.point) != GetCurrentLayer()) {
                // Let's not drag a point that's on another layer; that
                // will probably do something bad, since that layer is
                // not getting re-solved.
            } else {
                UndoRemember();
                ClearHoverAndSelected();
                Dragging.point = Hover.point;
                CurrentOperation = OPERATION_DRAGGING_PT;
            }
        } else if(Hover.which == SEL_CONSTRAINT) {
            SketchConstraint *c = ConstraintById(Hover.constraint);

            if(ConstraintHasLabelAssociated(c)) {
                UndoRemember();
                ClearHoverAndSelected();
                Dragging.offset = &(c->offset);
                Dragging.ref.x = toMicronsX(MouseLeftDownX) - c->offset.x;
                Dragging.ref.y = toMicronsY(MouseLeftDownY) - c->offset.y;
                CurrentOperation = OPERATION_DRAGGING_OFFSET;
            }
        } else if(Hover.which == SEL_ENTITY &&
                        EntityById(Hover.entity)->type == ENTITY_CIRCLE)
        {
            // Dragging a circle's perimeter drags its radius.
            hPoint center = POINT_FOR_ENTITY(Hover.entity, 0);
            if(LayerForPoint(center) == GetCurrentLayer()) {
                UndoRemember();
                ClearHoverAndSelected();
                EvalPoint(center, &(Dragging.ref.x), &(Dragging.ref.y));
                Dragging.param = PARAM_FOR_ENTITY(Hover.entity, 0);
                CurrentOperation = OPERATION_DRAGGING_RADIUS;
            }
        }
        DropDraggedOnMouseUp = TRUE;
    }
    
    switch(CurrentOperation) {
        case OPERATION_DRAGGING_PT_ON_ARC:
        case OPERATION_DRAGGING_PT_ON_SPLINE:
        case OPERATION_DRAGGING_PT:
            if(DropDraggedOnMouseUp) {
                ForcePointWhereFree(Dragging.point, 
                                            toMicronsX(x), toMicronsY(y));
            } else {
                // New points are easy, no constraints on them.
                ForcePoint(Dragging.point, toMicronsX(x), toMicronsY(y));
            }

            // When creating an arc for the first time, we will draw a
            // semicircle, which means that we'll drag the center such that
            // it lies at the midpoint of a line through the two on-curve
            // points.
            if(CurrentOperation == OPERATION_DRAGGING_PT_ON_ARC) {
                double x0, y0, x1, y1;
                EvalPoint(POINT_FOR_ENTITY(Dragging.entity, 0), &x0, &y0);
                EvalPoint(POINT_FOR_ENTITY(Dragging.entity, 1), &x1, &y1);
                if(tol(x0, x1) && tol(y0, y1)) {
                    // We're trying to place all the points on top of each
                    // other, which guarantees us a numerical blowup when
                    // we try to solve for the entity-generated constraint.
                    // Fake the position to avoid this.
                    ForcePoint(Dragging.point, toMicronsX(x+2),
                                               toMicronsY(y+2));
                    EvalPoint(POINT_FOR_ENTITY(Dragging.entity, 1), &x1, &y1);
                }
                ForcePoint(POINT_FOR_ENTITY(Dragging.entity, 2),
                    (x0 + x1) / 2, (y0 + y1) / 2);
            } else if(CurrentOperation == OPERATION_DRAGGING_PT_ON_SPLINE) {
                double x0, y0, x1, y1;
                int i = Dragging.i;
                if(i == 0) {
                    EvalPoint(POINT_FOR_ENTITY(Dragging.entity, 0), &x0, &y0);
                    EvalPoint(POINT_FOR_ENTITY(Dragging.entity, 3), &x1, &y1);

                    ForcePoint(POINT_FOR_ENTITY(Dragging.entity, 1),
                        (2*x0 + x1) / 3, (2*y0 + y1) / 3);
                    ForcePoint(POINT_FOR_ENTITY(Dragging.entity, 2),
                        (x0 + 2*x1) / 3, (y0 + 2*y1) / 3);
                } else {
                    EvalPoint(POINT_FOR_ENTITY(Dragging.entity, i+1), &x0, &y0);
                    EvalPoint(POINT_FOR_ENTITY(Dragging.entity, i+3), &x1, &y1);
                    ForcePoint(POINT_FOR_ENTITY(Dragging.entity, i+2),
                        (x0 + x1) / 2, (y0 + y1) / 2);
                }
            }

            if(!DropDraggedOnMouseUp) {
                // When placing a point for the first time, we will
                // automatically add coincidence constraints, and we need
                // the hover for that.
                CheckHover(x, y, HOVER_POINTS);
            }

            // If the display is dirty but has not been repainted, then that's
            // more important. Otherwise we will end up solving many times in
            // between refreshes, which makes the display unresponsive.
            SatisfyCoincidenceConstraints(Dragging.point);
            SolveBeforeNextPaint = TRUE;
            uiRepaint();
            break;

        case OPERATION_DRAGGING_RADIUS: {
            double d = Distance(
                toMicronsX(x), toMicronsY(y),
                Dragging.ref.x, Dragging.ref.y);
            ForceParam(Dragging.param, d);
            SolveBeforeNextPaint = TRUE;
            uiRepaint();
            break;
        }
        case OPERATION_DRAGGING_LINE: {
            // Let the Dragging.ref point and the current mouse location both 
            // lie on the line.
            double theta, a;
            RepAsAngleAndDistance(Dragging.ref.x, Dragging.ref.y,
                toMicronsX(x)-Dragging.ref.x, toMicronsY(y)-Dragging.ref.y,
                &theta, &a);
            hParam ap = A_FOR_LINE(Dragging.line);
            hParam thetap = THETA_FOR_LINE(Dragging.line);
            ForceParam(ap, a);
            ForceParam(thetap, theta);
            uiRepaint();
            break;
        }
        case OPERATION_DRAGGING_OFFSET:
            Dragging.offset->x = toMicronsX(x) - Dragging.ref.x;
            Dragging.offset->y = toMicronsY(y) - Dragging.ref.y;
            uiRepaint();
            break;

        case MNU_DRAW_LINE_SEGMENT:
        case MNU_DRAW_CIRCLE:
        case MNU_DRAW_ARC:
        case MNU_DRAW_DATUM_POINT:
        case MNU_DRAW_CUBIC_SPLINE:
            if(Hover.which != SEL_POINT && Hover.which != SEL_NONE) {
                Hover.which = SEL_NONE;
                uiRepaint();
            }
            CheckHover(x, y, HOVER_POINTS);
            break;

        case MNU_DRAW_DATUM_LINE:
            if(Hover.which != SEL_NONE) {
                Hover.which = SEL_NONE;
                uiRepaint();
            }
            break;

        case OPERATION_NONE:
        default:
            // Mustn't redo CheckHover if user is maybe about to move something
            // by left-dragging with mouse; then we might move off the hovered
            // object, and they'll be confused.
            if(!leftDown) {
                CheckHover(x, y, HOVER_ALL);
            }
            break;
    }
}

void SketchLeftButtonDblclk(int x, int y)
{
    if(uiTextEntryBoxIsVisible()) return;

    if(Hover.which == SEL_CONSTRAINT) {
        hConstraint hc = Hover.constraint;
        SketchConstraint *c = ConstraintById(hc);

        if(ConstraintHasLabelAssociated(c)) {
            double x, y;
            ForDrawnConstraint(GET_LABEL_LOCATION, c, &x, &y);
            char buf[128];

            if(c->type == CONSTRAINT_PT_LINE_DISTANCE ||
               c->type == CONSTRAINT_LINE_LINE_DISTANCE)
            {
                sprintf(buf, "%s", ToDisplay(fabs(c->v)));
            } else if(c->type == CONSTRAINT_PT_PT_DISTANCE ||
                      c->type == CONSTRAINT_RADIUS)
            {
                sprintf(buf, "%s", ToDisplay(c->v));
            } else if(c->type == CONSTRAINT_LINE_LINE_ANGLE) {
                sprintf(buf, "%.1f", fabs(c->v));
            } else if(c->type == CONSTRAINT_SCALE_MM ||
                      c->type == CONSTRAINT_SCALE_INCH)
            {
                sprintf(buf, "%.9g", c->v);
            } else {
                oopsnf();
                strcpy(buf, "");
            }

            uiShowTextEntryBoxAt(buf, toPixelsX(x), toPixelsY(y) + 4);

            ClearHoverAndSelected();
            Hover.which = SEL_CONSTRAINT;
            Hover.constraint = hc;
            uiRepaint();
        }
    } else if(Hover.which == SEL_ENTITY || Hover.which == SEL_POINT) {
        hEntity he;
        if(Hover.which == SEL_ENTITY) {
            he = Hover.entity;
        } else {
            he = ENTITY_FROM_POINT(Hover.point);
        }
        SketchEntity *e = EntityById(he);

        if(e->type == ENTITY_TTF_TEXT) {
            UndoRemember();
            txtuiGetTextForDrawing(e->text, e->file, &(e->spacing));
        } else if(e->type == ENTITY_IMPORTED) {
            UndoRemember();
            ChooseFileForImported(e);
        }
        ClearHoverAndSelected();
        Hover.which = SEL_ENTITY;
        Hover.entity = he;
        uiRepaint();
    }
}

void SketchLeftButtonUp(int x, int y)
{
    if(uiTextEntryBoxIsVisible()) return;

    if(DropDraggedOnMouseUp) {
        CurrentOperation = OPERATION_NONE;
        UpdateStatusBar();
        DropDraggedOnMouseUp = FALSE;
    }
}

void SketchLeftButtonDown(int x, int y)
{
    if(uiTextEntryBoxIsVisible()) return;

    hEntity he;

    switch(CurrentOperation) {
        case OPERATION_NONE:
            // If an item is hovered, then a left-click means to select
            // that item.
            ToggleSelectForHoveredItem();
            break;

        case OPERATION_DRAGGING_PT_ON_SPLINE:
            SketchAddPointToCubicSpline(Dragging.entity);
            Dragging.i += 2;
            Dragging.point = POINT_FOR_ENTITY(Dragging.entity, Dragging.i+3);
            // And we're still dragging.
            break;

        case OPERATION_DRAGGING_PT:
        case OPERATION_DRAGGING_PT_ON_ARC: {
            // We've been updating our position all along, but this is when
            // we might automatically add a coincidence constraint, if
            // we're right on top of some other point at this moment.
            BOOL snapped = PlacePoint(Dragging.point, x, y);
        
            hEntity prevEntity = ENTITY_FROM_POINT(Dragging.point);
            if(!snapped && 
                        EntityById(prevEntity)->type == ENTITY_LINE_SEGMENT)
            {
                // Let's make it easy to draw polylines.
                he = SketchAddEntity(ENTITY_LINE_SEGMENT);
                ForcePoint(POINT_FOR_ENTITY(he, 0),
                                            toMicronsX(x), toMicronsY(y));
                ConstrainCoincident(POINT_FOR_ENTITY(he, 0), Dragging.point);
                CurrentOperation = OPERATION_DRAGGING_PT;
                Dragging.point = POINT_FOR_ENTITY(he, 1);
            } else {
                // Whatever we're dragging, we should drop it.
                CurrentOperation = OPERATION_NONE;
            }
            break;
        }
        case OPERATION_DRAGGING_LINE:
        case OPERATION_DRAGGING_RADIUS:
        case OPERATION_DRAGGING_OFFSET:
            // Whatever we're dragging, we should drop it.
            CurrentOperation = OPERATION_NONE;
            break;

        case MNU_DRAW_DATUM_POINT:
            he = SketchAddEntity(ENTITY_DATUM_POINT);
            PlacePoint(POINT_FOR_ENTITY(he, 0), x, y);
            CurrentOperation = OPERATION_NONE;
            break;

        case MNU_DRAW_DATUM_LINE:
            he = SketchAddEntity(ENTITY_DATUM_LINE);
            CurrentOperation = OPERATION_DRAGGING_LINE;
            Dragging.ref.x = toMicronsX(x);
            Dragging.ref.y = toMicronsY(y);
            Dragging.line = LINE_FOR_ENTITY(he, 0);
            break;

        case MNU_DRAW_LINE_SEGMENT:
            he = SketchAddEntity(ENTITY_LINE_SEGMENT);
            PlacePoint(POINT_FOR_ENTITY(he, 0), x, y);
            CurrentOperation = OPERATION_DRAGGING_PT;
            Dragging.point = POINT_FOR_ENTITY(he, 1);
            break;

        case MNU_DRAW_CIRCLE:
            he = SketchAddEntity(ENTITY_CIRCLE);
            PlacePoint(POINT_FOR_ENTITY(he, 0), x, y);
            CurrentOperation = OPERATION_DRAGGING_RADIUS;
            Dragging.param = PARAM_FOR_ENTITY(he, 0);
            Dragging.ref.x = toMicronsX(x);
            Dragging.ref.y = toMicronsY(y);
            break;

        case MNU_DRAW_ARC:
            he = SketchAddEntity(ENTITY_CIRCULAR_ARC);
            PlacePoint(POINT_FOR_ENTITY(he, 0), x, y);
            CurrentOperation = OPERATION_DRAGGING_PT_ON_ARC;
            Dragging.point = POINT_FOR_ENTITY(he, 1);
            Dragging.entity = he;
            break;

        case MNU_DRAW_CUBIC_SPLINE:
            he = SketchAddEntity(ENTITY_CUBIC_SPLINE);
            PlacePoint(POINT_FOR_ENTITY(he, 0), x, y);
            CurrentOperation = OPERATION_DRAGGING_PT_ON_SPLINE;
            Dragging.point = POINT_FOR_ENTITY(he, 3);
            Dragging.entity = he;
            Dragging.i = 0;
            break;

        case MNU_DRAW_TEXT:
            he = SketchAddEntity(ENTITY_TTF_TEXT);
            PlacePoint(POINT_FOR_ENTITY(he, 0), x, y);
            CurrentOperation = OPERATION_DRAGGING_PT;
            Dragging.point = POINT_FOR_ENTITY(he, 1);
            break;

        case MNU_DRAW_FROM_IMPORTED:
            he = SketchAddEntity(ENTITY_IMPORTED);
            PlacePoint(POINT_FOR_ENTITY(he, 0), x, y);
            // Now let the user choose the file to import.
            ChooseFileForImported(EntityById(he));
            // And then we're dragging the other point
            CurrentOperation = OPERATION_DRAGGING_PT;
            Dragging.point = POINT_FOR_ENTITY(he, 1);
            break;
    }

    GenerateParametersPointsLines();
    uiRepaint();
    SketchMouseMoved(x, y, FALSE, FALSE, FALSE);
}
