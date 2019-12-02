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
// Math corresponding to each constraint: given each constraint, return one
// or more metrics that determine how far we are from satisfying that
// constraint. The solver calls these repeatedly, in an effort to drive them
// all to zero.
// Jonathan Westhues, April 2007
//-----------------------------------------------------------------------------
#include "sketchflat.h"

static void ModifyConstraintToReflectSketch(SketchConstraint *c);

Equations EQalloc;
Equations *EQ = (Equations *)&EQalloc;

static void AddConstraint(SketchConstraint *c)
{
    SK->eqnsDirty = TRUE;
    UndoRemember();

    if(SK->constraints >= MAX_CONSTRAINTS_IN_SKETCH) {
        uiError("Too many constraints!");
        return;
    }

    hConstraint max = 0;
    int i;
    for(i = 0; i < SK->constraints; i++) {
        if(SK->constraint[i].id > max) {
            max = SK->constraint[i].id;
        }
    }

    memcpy(&(SK->constraint[SK->constraints]), c, sizeof(*c));
    SK->constraint[SK->constraints].id = (max + 1);
    SK->constraint[SK->constraints].layer = GetCurrentLayer();

    (SK->constraints)++;

    SolvePerMode(FALSE);
}

void DeleteConstraint(hConstraint hc)
{
    SK->eqnsDirty = TRUE;

    int i;

    // So that we can't accidentally get deleted later, remove ourselves
    // from the selection. This would otherwise cause a problem if our
    // selection included an entity, and a constraint on that entity, in
    // that order; we'd delete the entity, and then delete the constraint
    // because it is on the entity, and then try to delete the constraint
    // again.
    for(i = 0; i < MAX_SELECTED_ITEMS; i++) {
        if(Selected[i].which == SEL_CONSTRAINT && Selected[i].constraint == hc)
        {
            Selected[i].which = SEL_NONE;
            Selected[i].constraint = 0;
        }
    }

    for(i = 0; i < SK->constraints; i++) {
        if(SK->constraint[i].id == hc) {
            (SK->constraints)--;
            memmove(&(SK->constraint[i]), &(SK->constraint[i+1]),
                (SK->constraints - i)*sizeof(SK->constraint[0]));

            return;
        }
    }

    oopsnf();
}

SketchConstraint *ConstraintById(hConstraint hc)
{
    int i;
    for(i = 0; i < SK->constraints; i++) {
        if(SK->constraint[i].id == hc) {
            return &(SK->constraint[i]);
        }
    }
    oops();
}

//-----------------------------------------------------------------------------
// Change the numerical value associated with certain types of constraints
// (e.g. distances and lengths). If we're solving, then we will do this in
// multiple steps, to decrease the odds of numerical disaster.
//-----------------------------------------------------------------------------
void ChangeConstraintValue(SketchConstraint *c, char *str)
{
    double nv;

    if(c->type == CONSTRAINT_PT_LINE_DISTANCE ||
       c->type == CONSTRAINT_LINE_LINE_DISTANCE)
    {
        // These distances are signed (correspond to above or below a
        // horizontal line). The sign is never displayed to the user, since
        // that's confusing, but they can flip the sign by entering a
        // negative value.
        BOOL neg = (c->v < 0);
        if(neg) {
            nv = -FromDisplay(str);
        } else {
            nv = FromDisplay(str);
        }
    } else if(c->type == CONSTRAINT_PT_PT_DISTANCE ||
              c->type == CONSTRAINT_RADIUS)
    {
        nv = FromDisplay(str);
    } else if(c->type == CONSTRAINT_LINE_LINE_ANGLE) {
        // As for point-line distances: the sign is not displayed to the
        // user, but may be flipped by entering a negative number.
        BOOL neg = (c->v  < 0);
        if(neg) {
            nv = -atof(str);
        } else {
            nv = atof(str);
        }
        // And let's force that to lie between -180 and 180.
        while(nv >  180) nv -= 360;
        while(nv < -180) nv += 360;
    } else if(c->type == CONSTRAINT_SCALE_MM ||
              c->type == CONSTRAINT_SCALE_INCH)
    {
        nv = fabs(atof(str));
    } else {
        oopsnf();
        return;
    }

    double cv0 = c->v;

    int i;
    int n = toint(fabs((nv - cv0)) / 5000);
    if(n > 15) n = 15;
    if(n < 5) n = 5;

    // Step the dimension to the requested value.
    for(i = n; i >= 0; i--) {
        c->v = nv + (i*(cv0 - nv))/n;
        SolvePerMode(FALSE);
    }
}

static void InitConstraint(SketchConstraint *c)
{
    c->v = 0;
    c->ptA = 0;
    c->ptB = 0;
    c->paramA = 0;
    c->paramB = 0;
    c->entityA = 0;
    c->entityB = 0;
    c->lineA = 0;
    c->lineB = 0;
    c->offset.x = toMicronsNotAffine(50);
    c->offset.y = 0;
}

static void HandleMenuSelection(int id)
{
    // As a convenience, let's count how many of each are selected, because
    // that determines which (if any) constraint a given menu item will
    // generate.
    GroupedSelection gs;
    GroupSelection(&gs);

    // Let's make sure that the constraint will be applied to at least
    // one item on the currently selected layer. If not, then it is surely
    // inconsistent, since the sketches are solved one layer at a time.
    hLayer cl = GetCurrentLayer();

    int i;
    for(i = 0; i < gs.points; i++) {
        if(LayerForPoint(gs.point[i]) == cl) {
            goto items_on_current_layer_selected;
        }
    }
    for(i = 0; i < gs.entities; i++) {
        if(gs.entity[i] == REFERENCE_ENTITY) continue;
        SketchEntity *e = EntityById(gs.entity[i]);
        if(e->layer == cl) {
            goto items_on_current_layer_selected;
        }
    }
    for(i = 0; i < gs.lines; i++) {
        if(LayerForLine(gs.line[i]) == cl) {
            goto items_on_current_layer_selected;
        }
    }
    uiError("Selection for constraint must contain at least one item on "
            "current layer.");
    return;

items_on_current_layer_selected:

    // Set up a blank constraint, to possibly fill in later and add.
    SketchConstraint c;
    InitConstraint(&c);

    // And now create the appropriate constraint.
    switch(id) {
        case MNU_CONSTR_DISTANCE:
            if(gs.points == 2 && gs.lines == 0 && gs.entities == 0) {
                // Plain distance, from point to point (2 points)
                c.type = CONSTRAINT_PT_PT_DISTANCE;
                c.ptA = gs.point[0];
                c.ptB = gs.point[1];

                ModifyConstraintToReflectSketch(&c);
                AddConstraint(&c);
                break;
            }
            if(gs.entities == 1 && gs.anyLines == 1 && gs.points == 0 && 
                    gs.lines == 0)
            {
                // Plain distance, from point to point. The two endpoints of
                // a line segment, in this case.
                c.type = CONSTRAINT_PT_PT_DISTANCE;
                c.ptA = POINT_FOR_ENTITY(gs.entity[0], 0);
                c.ptB = POINT_FOR_ENTITY(gs.entity[0], 1);

                ModifyConstraintToReflectSketch(&c);
                AddConstraint(&c);
                break;
            }
            if(gs.points == 1 && gs.anyLines == 1 && gs.nonLineEntities == 0) {
                // Distance from a point to a line; line can be either an
                // infinite datum line, or the infinite extension of a 
                // line segment.
                c.type = CONSTRAINT_PT_LINE_DISTANCE;
                c.ptA = gs.point[0];
                if(gs.lines == 1) {
                    c.lineB = gs.line[0];
                } else {
                    c.entityB = gs.entity[0];
                }

                ModifyConstraintToReflectSketch(&c);
                AddConstraint(&c);
                break;
            }
            if(gs.points == 0 && gs.anyLines == 2 && gs.nonLineEntities == 0) {
                // Minimum distance from a line to a line (2 lines, 2 line
                // segments, or a line and a line segment, and they have
                // to be parallel or it doesn't work)
                c.type = CONSTRAINT_LINE_LINE_DISTANCE;
                if(gs.lines == 2) {
                    c.lineA = gs.line[0];
                    c.lineB = gs.line[1];
                } else if(gs.lines == 1) {
                    c.lineA = gs.line[0];
                    c.entityB = gs.entity[0];
                } else if(gs.entities == 2) {
                    c.entityA = gs.entity[0];
                    c.entityB = gs.entity[1];
                }

                ModifyConstraintToReflectSketch(&c);
                AddConstraint(&c);
                break;
            }
            if(gs.points == 0 && gs.entities == 1 && gs.circlesOrArcs == 1 
                && gs.lines == 0)
            {
                c.type = CONSTRAINT_RADIUS;
                c.entityA = gs.entity[0];

                ModifyConstraintToReflectSketch(&c);
                AddConstraint(&c);
                break;
            }

            uiError("Invalid selection for distance / diameter constraint. A "
             "distance / diameter constraint may be applied to:\r\n\r\n"
"       - two line segments/datum lines (perpendicular distance)\r\n"
"       - a point and a line segment/datum line (perpendicular distance)\r\n"
"       - two points\r\n"
"       - a circle or an arc\r\n");
            break;

        case MNU_CONSTR_ANGLE:
            // Line to line (2 lines, 2 line segments, or a line and a line
            // segment)
            if(gs.anyLines == 2 && gs.n == 2) {
                c.type = CONSTRAINT_LINE_LINE_ANGLE;
                if(gs.lines == 2) {
                    c.lineA = gs.line[0];
                    c.lineB = gs.line[1];
                } else if(gs.entities == 2) {
                    c.entityA = gs.entity[0];
                    c.entityB = gs.entity[1];
                } else {
                    c.entityA = gs.entity[0];
                    c.lineB = gs.line[0];
                }

                ModifyConstraintToReflectSketch(&c);
                AddConstraint(&c);
                break;
            }

            uiError("Invalid selection for angle constraint. An angle "
                  "constraint may be applied to two items "
                  "from the set of:\r\n\r\n"
                  "       - datum lines\r\n"
                  "       - line segments\r\n");
            break;

        case MNU_CONSTR_COINCIDENT:
            if(gs.points == 2 && gs.lines == 0 && gs.entities == 0) {
                // Point on point
                c.type = CONSTRAINT_POINTS_COINCIDENT;
                c.ptA = gs.point[0];
                c.ptB = gs.point[1];
                AddConstraint(&c);
                break;
            }
            if(gs.points == 1 && gs.anyLines == 1 && gs.nonLineEntities == 0) {
                // Point on line or line segment; this corresponds to a
                // point-to-line distance constraint, with a distance
                // of zero.
                c.type = CONSTRAINT_POINT_ON_LINE;

                c.ptA = gs.point[0];
                if(gs.lines == 1) {
                    c.lineB = gs.line[0];
                } else {
                    c.entityB = gs.entity[0];
                }
                // Zero distance
                c.v = 0;

                AddConstraint(&c);
                break;
            }
            if(gs.points == 1 && gs.circlesOrArcs == 1 && gs.entities == 1
                && gs.lines == 0)
            {
                // Point on circle. This is actually just a point-to-point
                // distance constraint, from the center of the circle.
                c.type = CONSTRAINT_ON_CIRCLE;

                c.ptA = gs.point[0];
                c.entityA = gs.entity[0];

                AddConstraint(&c);
                break;
            }

            uiError(
                  "Invalid selection for coincidence constraint. A coincidence "
                  "constraint may be applied to:\r\n\r\n"
                  "       - two points (points become coincident)\r\n"
                  "       - a point and a curve (point lies on curve)\r\n"
                  "       - a point and a datum line (point lies on line)\r\n");
            break;
        
        case MNU_CONSTR_PERPENDICULAR:
        case MNU_CONSTR_PARALLEL:
            if((((id == MNU_CONSTR_PARALLEL) && gs.anyDirections == 2) ||
                    gs.anyLines == 2) && gs.n == 2)
            {
                int have = 0;
                
                // Parallelism can be defined between line segments, lines,
                // circular arc tangents, and cubic spline tangents.
                while(gs.lines > 0) {
                    if(have == 0) {
                        c.lineA = gs.line[gs.lines-1];
                    } else {
                        c.lineB = gs.line[gs.lines-1];
                    }
                    have++;
                    gs.lines--;
                }
                while(gs.points > 0) {
                    if(have == 0) {
                        c.ptA = gs.point[gs.points-1];
                    } else {
                        c.ptB = gs.point[gs.points-1];
                    }
                    have++;
                    gs.points--;
                }
                while(gs.entities > 0) {
                    if(have == 0) {
                        c.entityA = gs.entity[gs.entities-1];
                    } else {
                        c.entityB = gs.entity[gs.entities-1];
                    }
                    have++;
                    gs.entities--;
                }
                if(have != 2) {
                    oopsnf();
                    return;
                }

                if(id == MNU_CONSTR_PARALLEL) {
                    // They make a zero degree angle with each other.
                    c.type = CONSTRAINT_PARALLEL;
                    c.v = 0; // degrees
                } else {
                    // They make a ninety degree angle with each other.
                    c.type = CONSTRAINT_PERPENDICULAR;
                    c.v = 90; // degrees
                }
                AddConstraint(&c);
                break;
            }
            if(id == MNU_CONSTR_PARALLEL) {
                uiError("Invalid selection for parallel constraint. A parallel "
                      "constraint may be applied to two items from the set "
                      "of:\r\n\r\n"
                      "       - datum lines\r\n"
                      "       - line segments\r\n"
                      "       - endpoints of circular arcs (for tangency)\r\n"
                      "       - endpoints of cubic splines (for tangency)\r\n");
            } else {
                uiError("Invalid selection for perpendicular constraint. A "
                      "perpendicular constraint may be applied to two items "
                      "from the set of:\r\n\r\n"
                      "       - datum lines\r\n"
                      "       - line segments\r\n");
            }
            break;

        case MNU_CONSTR_EQUAL:
            // Equal length (2 line segments)
            if(gs.points == 0 && gs.lines == 0 && gs.anyLines == 2 &&
                gs.entities == 2)
            {
                c.type = CONSTRAINT_EQUAL_LENGTH;
                c.entityA = gs.entity[0];
                c.entityB = gs.entity[1];

                AddConstraint(&c);
                break;
            }
            // Equal radius (1 each circle or arc)
            if(gs.circlesOrArcs == 2 && gs.n == 2) {
                c.type = CONSTRAINT_EQUAL_RADIUS;
                c.entityA = gs.entity[0];
                c.entityB = gs.entity[1];

                AddConstraint(&c);
                break;
            }

            uiError("Invalid selection for equal constraint. An equal "
                  "constraint may be applied to:\r\n\r\n"
                  "       - two line segments\r\n"
                  "       - two circles, or arcs of circles\r\n");
            break;

        case MNU_CONSTR_MIDPOINT:
            // Point on line segment (point and line segment)
            if(gs.points == 1 && gs.lines == 0 && gs.anyLines == 1 &&
                gs.entities == 1)
            {
                c.type = CONSTRAINT_AT_MIDPOINT;
                c.entityA = gs.entity[0];
                c.ptA = gs.point[0];

                AddConstraint(&c);
                break;
            }

            uiError("Invalid selection for midpoint constraint. An midpoint "
                  "constraint may be applied to:\r\n\r\n"
                  "       - a point and a line segment\r\n");
            break;

        case MNU_CONSTR_SYMMETRIC:
            if(gs.points == 2 && gs.lines == 1 && gs.entities == 0) {
                c.type = CONSTRAINT_SYMMETRIC;
                c.lineA = gs.line[0];
                c.ptA = gs.point[0];
                c.ptB = gs.point[1];

                AddConstraint(&c);
                break;
            } else if(gs.anyLines == 2 && gs.entities == 1 && gs.points == 0 &&
                      gs.lines == 1 && gs.n == 2)
            {
                c.type = CONSTRAINT_SYMMETRIC;
                c.lineA = gs.line[0];
                c.ptA = POINT_FOR_ENTITY(gs.entity[0], 0);
                c.ptB = POINT_FOR_ENTITY(gs.entity[0], 1);

                AddConstraint(&c);
                break;
            }

            uiError("Invalid selection for symmetry constraint. A symmetry "
                    "constraint may be applied to:\r\n\r\n"
                    "       - two points and a datum line\r\n"
                    "       - a line segment and a datum line\r\n");
            break;

        case MNU_CONSTR_HORIZONTAL:
        case MNU_CONSTR_VERTICAL:
            if((gs.points == 2 && gs.n == 2) || 
               (gs.entities == 1 && gs.anyLines == 1 && gs.n == 1))
            {
                c.type = (id == MNU_CONSTR_HORIZONTAL) ?
                    CONSTRAINT_HORIZONTAL : CONSTRAINT_VERTICAL;

                if(gs.points == 2) {
                    // Two points that lie on a whicheveral line.
                    c.ptA = gs.point[0];
                    c.ptB = gs.point[1];
                } else {
                    // A line segment whose endpoints lie on a whicheveral
                    // line (i.e., that whicheveral line segment).
                    c.ptA = POINT_FOR_ENTITY(gs.entity[0], 0);
                    c.ptB = POINT_FOR_ENTITY(gs.entity[0], 1);
                }

                AddConstraint(&c);
                break;
            }

            uiError("Invalid selection for horizontal / vertical constraint. "
                    "A horizontal / vertical constraint may be applied to:"
                    "\r\n\r\n"
                    "       - two points\r\n"
                    "       - a line segment\r\n");
            break;

        case MNU_CONSTR_DRAG_HORIZ:
        case MNU_CONSTR_DRAG_VERT:
            if(gs.n == 1 && gs.points == 1) {
                c.type = CONSTRAINT_FORCE_PARAM;

                if(id == MNU_CONSTR_DRAG_HORIZ) {
                    c.paramA = X_COORD_FOR_PT(gs.point[0]);
                } else {
                    c.paramA = Y_COORD_FOR_PT(gs.point[0]);
                }

                AddConstraint(&c);
                break;
            }

            uiError("Invalid selection for draggable constraint. A draggable "
                    "constraint may be applied to:"
                    "\r\n\r\n"
                    "       - a point\r\n");
            break;

        case MNU_CONSTR_DRAG_ANGLE:
            if(gs.n == 2 && gs.points == 2) {
                c.type = CONSTRAINT_FORCE_ANGLE;
                c.ptA = gs.point[0];
                c.ptB = gs.point[1];

                AddConstraint(&c);
                break;
            }

            uiError("Invalid selection for draggable about point constraint. A "
                    "draggable about point constraint may be applied to:"
                    "\r\n\r\n"
                    "       - two points (the center of rotation, and the "
                    "point to constrain)\r\n");
            break;

        case MNU_CONSTR_SCALE_MM:
        case MNU_CONSTR_SCALE_INCH:
            if(gs.n == 1 && gs.entities == 1) {
                SketchEntity *e = EntityById(gs.entity[0]);
                if(e && e->type == ENTITY_IMPORTED) {
                    c.type = (id == MNU_CONSTR_SCALE_MM) ? 
                                CONSTRAINT_SCALE_MM : CONSTRAINT_SCALE_INCH;
                    c.entityA = gs.entity[0];
                    c.v = 1;
                    
                    ModifyConstraintToReflectSketch(&c);
                    AddConstraint(&c);
                    break;
                }
            }

            uiError("Invalid selection for scale (in inches or mm) constraint. "
                    "A scale constraint may be applied to:"
                    "\r\n\r\n"
                    "       - an imported file\r\n");
            break;
    }
}

void ConstrainCoincident(hPoint a, hPoint b)
{
    if(a == b) {
        oopsnf();
        return;
    }

    SketchConstraint c;
    InitConstraint(&c);

    // Point on point
    c.type = CONSTRAINT_POINTS_COINCIDENT;
    c.ptA = a;
    c.ptB = b;
    AddConstraint(&c);
}

void MenuConstrain(int id)
{
    if(id == MNU_CONSTR_SUPPLEMENTARY) {
        SketchConstraint *c;

        // Selection should include a single item, an angle constraint
        if(Selected[0].which != SEL_CONSTRAINT) goto badsel;
        int i;
        for(i = 1; i < MAX_SELECTED_ITEMS; i++) {
            if(Selected[i].which != SEL_NONE) goto badsel;
        }
        c = ConstraintById(Selected[0].constraint);
        if(c->type != CONSTRAINT_LINE_LINE_ANGLE) goto badsel;

        // An angle that is equal modulo 180 degrees will generate the
        // equivalent constraint, but display differently on-screen.
        c->v = c->v + 180;
        while(c->v > 180) c->v -= 360;

        // And redraw, and resolve, even though resolving should do nothing.
        SolvePerMode(FALSE);
        ClearHoverAndSelected();
        uiRepaint();
        return;

badsel:
        uiError("Must select an angle constraint.");
        return;
    }

    HandleMenuSelection(id);

    ClearHoverAndSelected();
    uiRepaint();
}

static Expr *EDistance(hPoint ptA, hPoint ptB)
{
    hParam xA = X_COORD_FOR_PT(ptA);
    hParam yA = Y_COORD_FOR_PT(ptA);

    hParam xB = X_COORD_FOR_PT(ptB);
    hParam yB = Y_COORD_FOR_PT(ptB);
    
    return ESqrt(EPlus(
                    ESquare(EMinus(EParam(xA), EParam(xB))),
                    ESquare(EMinus(EParam(yA), EParam(yB)))));
}

static void EGetPointAndDirectionForLine(hLine ln, hEntity e,
                                Expr **x0, Expr **y0, Expr **dx, Expr **dy)
{
    if(e && ln) oops();

    if(e) {
        hPoint ptA = POINT_FOR_ENTITY(e, 0);
        hPoint ptB = POINT_FOR_ENTITY(e, 1);

        hParam xA = X_COORD_FOR_PT(ptA);
        hParam yA = Y_COORD_FOR_PT(ptA);

        hParam xB = X_COORD_FOR_PT(ptB);
        hParam yB = Y_COORD_FOR_PT(ptB);

        if(dx) *dx = EMinus(EParam(xA), EParam(xB));
        if(dy) *dy = EMinus(EParam(yA), EParam(yB));

        if(x0) *x0 = EParam(xA);
        if(y0) *y0 = EParam(yA);
    } else {
        hParam theta = THETA_FOR_LINE(ln);
        hParam a = A_FOR_LINE(ln);

        if(dx) *dx = ECos(EParam(theta));
        if(dy) *dy = ESin(EParam(theta));

        if(x0) *x0 = ENegate(ETimes(EParam(a), ESin(EParam(theta))));
        if(y0) *y0 = ETimes(EParam(a), ECos(EParam(theta)));
    }
}

static void EGetDirectionOrTangent(hLine ln, hEntity he, hPoint p,
                                                Expr **dx, Expr **dy)
{   
    if(ln || he) {
        if(p) oops();
        EGetPointAndDirectionForLine(ln, he, NULL, NULL, dx, dy);
        return;
    }
    if(!p) oops();

    hParam px = X_COORD_FOR_PT(p);
    hParam py = Y_COORD_FOR_PT(p);

    hEntity hep = ENTITY_FROM_POINT(p);
    SketchEntity *e = EntityById(hep);
    if(e->type == ENTITY_CIRCULAR_ARC) {
        hPoint c = POINT_FOR_ENTITY(e->id, 2);
        // This is the center of the circle that defines the arc.
        hParam cx = X_COORD_FOR_PT(c);
        hParam cy = Y_COORD_FOR_PT(c);
        // The tangent is perpendicular to the radius.
        *dx = EMinus(EParam(cy), EParam(py));
        *dy = ENegate(EMinus(EParam(cx), EParam(px)));
    } else if(e->type == ENTITY_CUBIC_SPLINE) {
        int k = K_FROM_POINT(p);
        // This is the nearest Bezier control point.
        hParam bx, by;
        if(k == 0) {
            bx = X_COORD_FOR_PT(POINT_FOR_ENTITY(e->id, k+1));
            by = Y_COORD_FOR_PT(POINT_FOR_ENTITY(e->id, k+1));
        } else if(k == (e->points -1)) {
            bx = X_COORD_FOR_PT(POINT_FOR_ENTITY(e->id, k-1));
            by = Y_COORD_FOR_PT(POINT_FOR_ENTITY(e->id, k-1));
        } else {
            oops();
        }

        *dx = EMinus(EParam(bx), EParam(px));
        *dy = EMinus(EParam(by), EParam(py));
    } else {
        oops();
    }
}

static Expr *EDistanceFromExprPointToLine(Expr *xp, Expr *yp,
                                                        hLine ln, hEntity e)
{
    if(e && ln) oops();

    Expr *dx, *dy;
    Expr *x0, *y0;

    EGetPointAndDirectionForLine(ln, e, &x0, &y0, &dx, &dy);

    Expr *d = EDiv( EMinus(  ETimes(dx, EMinus(y0, yp)), 
                             ETimes(dy, EMinus(x0, xp))),
                    ESqrt(EPlus(ESquare(dx), ESquare(dy))));
    return d;
}
static Expr *EDistanceFromPointToLine(hPoint pt, hLine ln, hEntity e)
{
    Expr *xp = EParam(X_COORD_FOR_PT(pt));
    Expr *yp = EParam(Y_COORD_FOR_PT(pt));

    return EDistanceFromExprPointToLine(xp, yp, ln, e);
}

static void AddEquation(hConstraint hc, int k, Expr *e)
{
    if(EQ->eqns >= arraylen(EQ->eqn)) oops();

    EQ->eqn[EQ->eqns].e  = e;
    EQ->eqn[EQ->eqns].he = EQUATION_FOR_CONSTRAINT(hc, k);

    (EQ->eqns)++;
}

static void Make_PtPtDistance(SketchConstraint *c)
{
    if(told(c->v, 0) || c->type == CONSTRAINT_POINTS_COINCIDENT) {
        // Two points, zero distance apart; so we are trying to contrain
        // the points coincident, for which we should write two equations,
        // since that restricts two degrees of freedom.

        hParam pa, pb;

        // First equation: xA - xB = 0
        pa = X_COORD_FOR_PT(c->ptA);
        pb = X_COORD_FOR_PT(c->ptB);
        
        AddEquation(c->id, 0, EMinus(EParam(pa), EParam(pb)));

        // Second equation: yA - yB = 0
        pa = Y_COORD_FOR_PT(c->ptA);
        pb = Y_COORD_FOR_PT(c->ptB);
        
        AddEquation(c->id, 1, EMinus(EParam(pa), EParam(pb)));
    } else {
        Expr *d = EDistance(c->ptA, c->ptB);

        AddEquation(c->id, 0, EMinus(d, EConstant(c->v)));
    }
}

static void Make_PtLineDistance(SketchConstraint *c)
{
    Expr *d = EDistanceFromPointToLine(c->ptA, c->lineB, c->entityB);

    AddEquation(c->id, 0, EMinus(d, EConstant(c->v)));
}

static void Make_LineLineDistance(SketchConstraint *c)
{
    // We will choose an arbitrary point on one line, and constrain the
    // distance to the other.
    Expr *d;
    if(c->entityA) {
        // A is a line segment, so use one of its endpoints.
        d = EDistanceFromPointToLine(POINT_FOR_ENTITY(c->entityA, 0),
                                                    c->lineB, c->entityB);
    } else if(c->entityB) {
        // B is a line segment, so use one of its endpoints.
        d = EDistanceFromPointToLine(POINT_FOR_ENTITY(c->entityB, 0),
                                                    c->lineA, c->entityA);
    } else {
        // Two datum lines. Choose an arbtirary point on lineA, and measure
        // to the other line.
        Expr *x0, *y0;
        EGetPointAndDirectionForLine(c->lineA, 0, &x0, &y0, NULL, NULL);
        d = EDistanceFromExprPointToLine(x0, y0, c->lineB, 0);
    }

    AddEquation(c->id, 0, EMinus(d, EConstant(c->v)));
}

static void Make_Angle(SketchConstraint *c)
{
    Expr *dxA, *dyA;
    Expr *dxB, *dyB;

    EGetDirectionOrTangent(c->lineA, c->entityA, c->ptA, &dxA, &dyA);
    EGetDirectionOrTangent(c->lineB, c->entityB, c->ptB, &dxB, &dyB);


    // Rotate one of the vectors by the desired angle
    double theta = (c->v)*PI/180;
    Expr *dxr, *dyr;
    dxr = EPlus(ETimes(EConstant( cos(theta)), dxA),
                ETimes(EConstant( sin(theta)), dyA));
    dyr = EPlus(ETimes(EConstant(-sin(theta)), dxA),
                ETimes(EConstant( cos(theta)), dyA));

    // And we are trying to make them parallel, i.e. to cause
    //     [  dxr   dyr  ]
    // det [  dxB   dyB  ] = 0

    // It's important to normalize by the lengths of our direction vectors.
    // If we don't, then the solver can satisfy our constraint by driving
    // the length of the line to zero, at which point it is parallel to
    // everything.
    //
    // The constant is to get it on to the right order, so that the tolerances
    // for singularity and stuff later are reasonable.
    AddEquation(c->id, 0,
        EDiv(EMinus(ETimes(dxr, dyB), ETimes(dyr, dxB)),
             ETimes(ESqrt(EPlus(ESquare(dxB), ESquare(dyB))),
                    ESqrt(EPlus(ESquare(dxA), ESquare(dyA))))));
}

static void Make_EqualLength(SketchConstraint *c)
{
    hPoint ptA0 = POINT_FOR_ENTITY(c->entityA, 0);
    hPoint ptA1 = POINT_FOR_ENTITY(c->entityA, 1);

    hPoint ptB0 = POINT_FOR_ENTITY(c->entityB, 0);
    hPoint ptB1 = POINT_FOR_ENTITY(c->entityB, 1);

    AddEquation(c->id, 0, EMinus(
        EDistance(ptA0, ptA1),
        EDistance(ptB0, ptB1)));
}

static void Make_AtMidpoint(SketchConstraint *c)
{
    hPoint ptA = POINT_FOR_ENTITY(c->entityA, 0);
    hPoint ptB = POINT_FOR_ENTITY(c->entityA, 1);

    hParam xA = X_COORD_FOR_PT(ptA);
    hParam yA = Y_COORD_FOR_PT(ptA);
    hParam xB = X_COORD_FOR_PT(ptB);
    hParam yB = Y_COORD_FOR_PT(ptB);

    hParam xp = X_COORD_FOR_PT(c->ptA);
    hParam yp = Y_COORD_FOR_PT(c->ptA);

    AddEquation(c->id, 0, EMinus(
        EDiv(EPlus(EParam(xA), EParam(xB)), EConstant(2)),
        EParam(xp)));

    AddEquation(c->id, 1, EMinus(
        EDiv(EPlus(EParam(yA), EParam(yB)), EConstant(2)),
        EParam(yp)));
}

static Expr *RadiusForEntity(hEntity he)
{
    int type = EntityById(he)->type;

    if(type == ENTITY_CIRCLE) {
        // Straightforward, radius is the parameter of the circle.
        hParam r = PARAM_FOR_ENTITY(he, 0);
        return EParam(r);
    } else if(type == ENTITY_CIRCULAR_ARC) {
        // Radius is the distance from either on-curve point to the center
        // point.
        return EDistance(POINT_FOR_ENTITY(he, 0), POINT_FOR_ENTITY(he, 2));
    } else {
        oops();
    }
}

static void Make_Radius(SketchConstraint *c)
{
    hParam he = c->entityA;

    Expr *r = RadiusForEntity(he);

    // We work with the radius internally, but the user enters the diameter;
    // so the radii that we get from the sketch get multiplied by two before
    // we compare them.
    AddEquation(c->id, 0, EMinus(ETimes(EConstant(2), r), EConstant(c->v)));
}

static void Make_EqualRadius(SketchConstraint *c)
{
    Expr *rA = RadiusForEntity(c->entityA);
    Expr *rB = RadiusForEntity(c->entityB);

    // This should be written simply, so that equal radius for circles will
    // get solved by forward-substitution.
    AddEquation(c->id, 0, EMinus(rA, rB));
}

static void Make_OnCircle(SketchConstraint *c)
{
    SketchEntity *e = EntityById(c->entityA);

    if(e->type == ENTITY_CIRCLE) {
        hParam r = PARAM_FOR_ENTITY(c->entityA, 0);
        hPoint cntr = POINT_FOR_ENTITY(c->entityA, 0);

        Expr *d = EDistance(cntr, c->ptA);

        AddEquation(c->id, 0, EMinus(d, EParam(r)));
    } else if(e->type == ENTITY_CIRCULAR_ARC) {
        hPoint cntr = POINT_FOR_ENTITY(c->entityA, 2);
        hPoint onArc = POINT_FOR_ENTITY(c->entityA, 0); // or 1, either way

        Expr *r = EDistance(cntr, onArc);
        Expr *d = EDistance(cntr, c->ptA);

        AddEquation(c->id, 0, EMinus(d, r));
    } else {
        oops();
    }
}

static void Make_Symmetric(SketchConstraint *c)
{
    Expr *dA = EDistanceFromPointToLine(c->ptA, c->lineA, 0);
    Expr *dB = EDistanceFromPointToLine(c->ptB, c->lineA, 0);

    // These are signed distances, and the symmetric points should be on
    // opposite sides of the line, so they should be equal and magnitude,
    // and opposite in sign.
    AddEquation(c->id, 0, EPlus(dA, dB));

    Expr *xA = EParam(X_COORD_FOR_PT(c->ptA));
    Expr *yA = EParam(Y_COORD_FOR_PT(c->ptA));
    Expr *xB = EParam(X_COORD_FOR_PT(c->ptB));
    Expr *yB = EParam(Y_COORD_FOR_PT(c->ptB));

    hParam theta = THETA_FOR_LINE(c->lineA);
    Expr *dx = ECos(EParam(theta));
    Expr *dy = ESin(EParam(theta));

    // We want the line between the two points to be perpendicular to the
    // datum, so (xA - xB, yA - yB) dot (cos theta, sin theta) = 0.
    AddEquation(c->id, 1, EPlus(ETimes(EMinus(xA, xB), dx),
                      ETimes(EMinus(yA, yB), dy)));
}

static void Make_HorizontalVertical(SketchConstraint *c)
{
    hParam pA, pB;

    if(c->type == CONSTRAINT_HORIZONTAL) {
        // Horizontal lines have equal y coordinates for their endpoints.
        pA = Y_COORD_FOR_PT(c->ptA);
        pB = Y_COORD_FOR_PT(c->ptB);
    } else {
        pA = X_COORD_FOR_PT(c->ptA);
        pB = X_COORD_FOR_PT(c->ptB);
    }

    // A simple equation of this form will be solved by substitution, and
    // will therefore solve very quickly.
    AddEquation(c->id, 0, EMinus(EParam(pA), EParam(pB)));
}

static void Make_ForceParam(SketchConstraint *c)
{
    hParam hp = c->paramA;

    // This is a weird one. It's basically doing the same job as the
    // solver does when it makes assumptions, fixing an unknown wherever
    // it has been dragged. A bit silly to write an equation, perhaps,
    // but this will solve very easily.
    AddEquation(c->id, 0,
        EMinus(EParam(hp), EConstant(EvalParam(hp))));
}

static void Make_ForceAngle(SketchConstraint *c)
{
    hParam xA = X_COORD_FOR_PT(c->ptA);
    hParam yA = Y_COORD_FOR_PT(c->ptA);

    hParam xB = X_COORD_FOR_PT(c->ptB);
    hParam yB = Y_COORD_FOR_PT(c->ptB);

    // We want to fix the angle of the line connecting points A and B, to
    // be whatever it is right now in the initial numerical guess. Write
    // this in the usual dot/cross product form,
    //     (xA - xB, yA - yB) dot (-(yA0 - yB0), xA0 - xB0) = 0

    AddEquation(c->id, 0,
        EPlus(
            ETimes(EMinus(EParam(xA), EParam(xB)),
                   EConstant(-(EvalParam(yA) - EvalParam(yB)))),
            ETimes(EMinus(EParam(yA), EParam(yB)),
                   EConstant(EvalParam(xA) - EvalParam(xB))))); 
}

static void Make_Scale(SketchConstraint *c)
{
    SketchEntity *e = EntityById(c->entityA);
    if(!e) {
        oopsnf();
        return;
    }
    const char *sought = "so dy = ";
    char *s = strstr(e->text, sought);
    if(!s) return;
    s += strlen(sought);

    double dy = atof(s);
    if(c->type == CONSTRAINT_SCALE_MM) {
        dy *= 1000;
    } else if(c->type == CONSTRAINT_SCALE_INCH) {
        dy *= 25400;
    } else {
        oopsnf();
        return;
    }
    dy *= c->v;

    hPoint pA = POINT_FOR_ENTITY(c->entityA, 0);
    hPoint pB = POINT_FOR_ENTITY(c->entityA, 1);

    AddEquation(c->id, 0, EMinus(EDistance(pA, pB), EConstant(dy)));
}

void MakeConstraintEquations(SketchConstraint *c)
{
    switch(c->type) {
        case CONSTRAINT_POINTS_COINCIDENT:
        case CONSTRAINT_PT_PT_DISTANCE:
            Make_PtPtDistance(c);
            break;

        case CONSTRAINT_POINT_ON_LINE:
        case CONSTRAINT_PT_LINE_DISTANCE:
            Make_PtLineDistance(c);
            break;

        case CONSTRAINT_LINE_LINE_DISTANCE:
            Make_LineLineDistance(c);
            break;

        case CONSTRAINT_ON_CIRCLE:
            Make_OnCircle(c);
            break;

        case CONSTRAINT_RADIUS:
            Make_Radius(c);
            break;

        case CONSTRAINT_PARALLEL:
        case CONSTRAINT_PERPENDICULAR:
        case CONSTRAINT_LINE_LINE_ANGLE:
            Make_Angle(c);
            break;

        case CONSTRAINT_EQUAL_LENGTH:
            Make_EqualLength(c);
            break;

        case CONSTRAINT_EQUAL_RADIUS:
            Make_EqualRadius(c);
            break;

        case CONSTRAINT_AT_MIDPOINT:
            Make_AtMidpoint(c);
            break;

        case CONSTRAINT_SYMMETRIC:
            Make_Symmetric(c);
            break;

        case CONSTRAINT_HORIZONTAL:
        case CONSTRAINT_VERTICAL:
            Make_HorizontalVertical(c);
            break;

        case CONSTRAINT_FORCE_PARAM:
            Make_ForceParam(c);
            break;

        case CONSTRAINT_FORCE_ANGLE:
            Make_ForceAngle(c);
            break;

        case CONSTRAINT_SCALE_MM:
        case CONSTRAINT_SCALE_INCH:
            Make_Scale(c);
            break;

        default:
            oops();
    }
}

void MakeEntityEquations(SketchEntity *e)
{
    switch(e->type) {
        case ENTITY_CIRCULAR_ARC: {
            Expr *d0 = EDistance(POINT_FOR_ENTITY(e->id, 0),
                                 POINT_FOR_ENTITY(e->id, 2));
            Expr *d1 = EDistance(POINT_FOR_ENTITY(e->id, 1),
                                 POINT_FOR_ENTITY(e->id, 2));
            // Make sure it gets an hEquation that won't conflict with
            // some constraint's equation.
            AddEquation(CONSTRAINT_FOR_ENTITY(e->id), 0, EMinus(d0, d1));
            break;
        }

        default:
            // Most entities don't generate any equations. Ideally, none
            // would.
            break;
    }
}

static void ModifyConstraintToReflectSketch(SketchConstraint *c)
{
    switch(c->type) {
        case CONSTRAINT_PT_PT_DISTANCE: {
            // Stupid special case. The point-to-point distance constraint
            // becomes a point-on-point constraint when that distance is
            // zero, and at that point it restricts two DOF, not one, so
            // I have to write two equations. That means that it breaks from
            // the usual form for "dimension"-type constraints.
            double xA, yA, xB, yB;
            EvalPoint(c->ptA, &xA, &yA);
            EvalPoint(c->ptB, &xB, &yB);
            c->v = Distance(xA, yA, xB, yB);
            break;
        }
        case CONSTRAINT_LINE_LINE_ANGLE: {
            // Another special case; this equation has the form of a dot
            // product, not so easy to derive present angle from that.
            int A = 0, B = 1;
            double x0[2], y0[2];
            double dx[2], dy[2];
            LineOrLineSegment(c->lineA, c->entityA,
                                &(x0[A]), &(y0[A]), &(dx[A]), &(dy[A]));
            LineOrLineSegment(c->lineB, c->entityB,
                                &(x0[B]), &(y0[B]), &(dx[B]), &(dy[B]));
            
            // A special case for the special case. If these are two line
            // segments that share an endpoint, then we clearly should be
            // dimensioning the angle between the lines segments, not the
            // angle that's hanging in thin air.
            if(c->entityA && c->entityB) {

                double x0A, y0A, x1A, y1A;
                double x0B, y0B, x1B, y1B;
                EvalPoint(POINT_FOR_ENTITY(c->entityA, 0), &x0A, &y0A);
                EvalPoint(POINT_FOR_ENTITY(c->entityA, 1), &x1A, &y1A);
                EvalPoint(POINT_FOR_ENTITY(c->entityB, 0), &x0B, &y0B);
                EvalPoint(POINT_FOR_ENTITY(c->entityB, 1), &x1B, &y1B);
    
                // Four possible cases for which points are coincident
                if(tol(x0A, x0B) && tol(y0A, y0B)) {
                    dx[A] = x1A - x0A;
                    dy[A] = y1A - y0A;
                    dx[B] = x1B - x0B;
                    dy[B] = y1B - y0B;
                } else if(tol(x0A, x1B) && tol(y0A, y1B)) {
                    dx[A] = x1A - x0A;
                    dy[A] = y1A - y0A;
                    dx[B] = x0B - x1B;
                    dy[B] = y0B - y1B;
                } else if(tol(x1A, x0B) && tol(y1A, y0B)) {
                    dx[A] = x0A - x1A;
                    dy[A] = y0A - y1A;
                    dx[B] = x1B - x0B;
                    dy[B] = y1B - y0B;
                } else if(tol(x1A, x1B) && tol(y1A, y1B)) {
                    dx[A] = x0A - x1A;
                    dy[A] = y0A - y1A;
                    dx[B] = x0B - x1B;
                    dy[B] = y0B - y1B;
                }
            }

            double thetaA = atan2(dy[A], dx[A]);
            double thetaB = atan2(dy[B], dx[B]);
            double dtheta = thetaA - thetaB;
            while(dtheta < PI) dtheta += 2*PI;
            while(dtheta > PI) dtheta -= 2*PI;
            c->v = dtheta*180/PI;
            break;
        }
        case CONSTRAINT_PT_LINE_DISTANCE:
        case CONSTRAINT_LINE_LINE_DISTANCE:
        case CONSTRAINT_RADIUS: {
            // These constraints all have a number associated with them;
            // they are trying to make some measurement equal to that
            // number. The constraint equation f = 0 is written in the
            // form
            //
            //        actual - desired = 0
            //
            // and the desired value is stored in c->v.
            EQ->eqns = 0;
            MakeConstraintEquations(c);
            if(EQ->eqns != 1) oops();

            double actualMinusDesired = EEval(EQ->eqn[0].e);

            c->v += actualMinusDesired;
            break;
        }
        case CONSTRAINT_SCALE_MM:
        case CONSTRAINT_SCALE_INCH: {
            // These constraints are in the form
            //
            //      actual - desired = 0
            //
            //  and the desired has the form (c->v)*k, for some constant k.
            EQ->eqns = 0;
            c->v = 0;
            MakeConstraintEquations(c);
            if(EQ->eqns != 1) {
                // The dimensions of the imported file might not yet be
                // calculated, in which case we can't do the constraint yet,
                // but it's not an error.
                c->v = 1;
                break;
            }
            double actual = EEval(EQ->eqn[0].e);
            EQ->eqns = 0;
            c->v = 1;
            MakeConstraintEquations(c);
            if(EQ->eqns != 1) oops();
            double actualMinusK = EEval(EQ->eqn[0].e);
            double k = actual - actualMinusK;

            c->v = actual/k;
            break;
        }

        default:
            // These constraints have no parameter. Either they hold, or
            // they don't, no way to adjust the constraint to make it go.
            break;
    }
}
