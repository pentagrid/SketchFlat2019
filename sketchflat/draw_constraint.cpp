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
// Constraints are drawn on-screen, to show the user what the sketch
// represents and to provide some means to edit them. Here is where we
// draw those, and where we manipulate those graphical representations.
// Jonathan Westhues, April 2007
//-----------------------------------------------------------------------------
#include "sketchflat.h"

static double NormTheta(double theta)
{
    while(theta < PI) theta += 2*PI;
    while(theta > PI) theta -= 2*PI;

    return theta;
}

static void IntersectionAndReasonablePoint(SketchConstraint *c,
            double *x0, double *y0, double *dx, double *dy,
                    double *xi, double *yi, double *xc, double *yc)
{
    LineOrLineSegment(c->lineA, c->entityA,
                        &(x0[0]), &(y0[0]), &(dx[0]), &(dy[0]));
    LineOrLineSegment(c->lineB, c->entityB,
                        &(x0[1]), &(y0[1]), &(dx[1]), &(dy[1]));

    if(IntersectionOfLines(x0[0], y0[0], dx[0], dy[0],
                           x0[1], y0[1], dx[1], dy[1], xi, yi))
    {
        // The lines intersect, so draw the symbol near their
        // intersection.
        if(c->entityA || c->entityB) {
            double d = VERY_POSITIVE;
            double x, y;

            int i, j;
            for(i = 0; i < 2; i++) {
                hEntity he = (i == 0) ? c->entityA : c->entityB;
                if(!he) continue;

                for(j = 0; j < 2; j++) {
                    EvalPoint(POINT_FOR_ENTITY(he, j), &x, &y);
                    double dthis = Distance(x, y, *xi, *yi);
                    if(dthis < d) {
                        d = dthis;
                        *xc = x;
                        *yc = y;
                    }
                }
            }
        } else {
            *xc = *xi;
            *yc = *yi;
        }
    } else {
        // The lines are parallel, so no intersection. Choose a point on
        // one of the lines, at least, an endpoint if one's a line segment.
        if(c->entityA) {
            *xc = x0[0];
            *yc = y0[0];
        } else {
            *xc = x0[1];
            *yc = y0[1];
        }
        // And give a bogus intersection point for good measure, since we
        // still must do something.
        *xi = *xc;
        *yi = *yc;
    }
}

BOOL ConstraintHasLabelAssociated(SketchConstraint *c)
{
    switch(c->type) {
        case CONSTRAINT_PT_PT_DISTANCE:
        case CONSTRAINT_PT_LINE_DISTANCE:
        case CONSTRAINT_LINE_LINE_DISTANCE:
        case CONSTRAINT_RADIUS:
        case CONSTRAINT_LINE_LINE_ANGLE:
        case CONSTRAINT_SCALE_MM:
        case CONSTRAINT_SCALE_INCH:
            return TRUE;

        default:
            return FALSE;
    }
}

//-----------------------------------------------------------------------------
// A routine that is often necessary when we draw a piece of text that goes
// with a circle; elliptical interpolation.
//-----------------------------------------------------------------------------
double EllipticalInterpolation(double rx, double ry, double theta)
{
    double ex = rx*cos(theta);
    double ey = ry*sin(theta);
    double v = sqrt(ex*ex + ey*ey);

    return v;
}

//-----------------------------------------------------------------------------
// Get the bounding box for a given piece of text.
//-----------------------------------------------------------------------------
static void TextExtent(char *str, double *w, double *h)
{
    *w = toMicronsNotAffine(strlen(str)*8);
    *h = toMicronsNotAffine(10);
}

//-----------------------------------------------------------------------------
// Debugging function, to mark a point on screen.
//-----------------------------------------------------------------------------
static void mark(double xf, double yf)
{
    int x = toPixelsX(xf);
    int y = toPixelsY(yf);
    int a = 5;
    PltSetColor(DATUM_COLOR);
    PltMoveTo(x - a, y);
    PltLineTo(x + a, y);
    PltMoveTo(x, y - a);
    PltLineTo(x, y + a);
    PltSetColor(CONSTRAINTS_COLOR);
}

//-----------------------------------------------------------------------------
// This is where the graphical representations of constraints are handled.
// A single function, because it makes sense to group the routines to draw
// a particular constraint with the routines to determine the distance from
// a point to the graphical representation of that constraint.
//-----------------------------------------------------------------------------
double ForDrawnConstraint(int op, SketchConstraint *c, double *x, double *y)
{
    int pts;
    switch(c->type) {
        case CONSTRAINT_HORIZONTAL:
        case CONSTRAINT_VERTICAL: { 
            int n;
            double xA, yA, xB, yB;

            // Horizontal/vertical line segments are drawn differently from
            // two otherwise unrelated points that are constrained to lie
            // on a horizontal/vertical line. For the line segment, draw
            // an H or a V at the middle of the line. For the points, draw
            // geometric marks.

            n = 2;
            if(ENTITY_FROM_POINT(c->ptA) == ENTITY_FROM_POINT(c->ptB)) {
                hEntity he = ENTITY_FROM_POINT(c->ptA);
                SketchEntity *e = EntityById(he);

                if(e->type == ENTITY_LINE_SEGMENT) {
                    n = 1;
                }
            }
            EvalPoint(c->ptA, &xA, &yA);
            EvalPoint(c->ptB, &xB, &yB);
            if(n == 1) {
                xA = (xA + xB) / 2;
                yA = (yA + yB) / 2;
                if(c->type == CONSTRAINT_HORIZONTAL) {
                    yA += toMicronsNotAffine(20);
                } else {
                    xA += toMicronsNotAffine(7);
                }
            } else {
                // Sort the points, either left to right (horizontal) or
                // right to left (vertical).
                if(c->type == CONSTRAINT_HORIZONTAL) {
                    if(xA > xB) {
                        double t;
                        t = xA; xA = xB; xB = t;
                        t = yA; yA = yB; yB = t;
                    }
                } else {
                    if(yA > yB) {
                        double t;
                        t = xA; xA = xB; xB = t;
                        t = yA; yA = yB; yB = t;
                    }
                }
            }

            if(op == DRAW_CONSTRAINT) {
                if(n == 1) {
                    PltText(toPixelsX(xA), toPixelsY(yA), TRUE,
                        (c->type == CONSTRAINT_HORIZONTAL) ? "H" : "V");
                    *x = xA;
                    *y = yA;
                } else {
                    int i, j;
                    for(i = 0; i < 2; i++) {
                        double xp, yp, ds, dl;
                        ds = toMicronsNotAffine(10);
                        dl = toMicronsNotAffine(16);
                        if(i == 0) {
                            xp = xA; yp = yA;
                        } else {
                            xp = xB; yp = yB;
                            ds = -ds; dl = -dl;
                        }

                        if(c->type == CONSTRAINT_HORIZONTAL) {
                            PltMoveToMicrons(xp, yp);
                            PltLineToMicrons(xp + ds, yp);
                            for(j = 0; j < 5; j++) {
                                double o = toMicronsNotAffine(j-2);
                                PltMoveToMicrons(xp + ds, yp + o);
                                PltLineToMicrons(xp + dl, yp + o);
                            }
                            *x = xp + ds;
                            *y = yp;
                        } else {
                            PltMoveToMicrons(xp, yp);
                            PltLineToMicrons(xp, yp + ds);
                            for(j = 0; j < 5; j++) {
                                double o = toMicronsNotAffine(j-2);
                                PltMoveToMicrons(xp + o, yp + ds);
                                PltLineToMicrons(xp + o, yp + dl);
                            }
                            *x = xp;
                            *y = yp + ds;
                        }
                    }
                }
                return 0;

            } else if(op == GET_DISTANCE_TO_CONSTRAINT) {
                if(n == 1) {
                    // Aim for the center of the letter, not the top left.
                    yA -= toMicronsNotAffine(8);
                    xA += toMicronsNotAffine(3);
                    return Distance(*x, *y, xA, yA) - 10;
                } else {
                    // Aim for the centers of the two markers that I drew.
                    if(c->type == CONSTRAINT_HORIZONTAL) {
                        xA += toMicronsNotAffine(13);
                        xB -= toMicronsNotAffine(13);
                    } else {
                        yA += toMicronsNotAffine(13);
                        yB -= toMicronsNotAffine(13);
                    }
                    return min(Distance(*x, *y, xA, yA),
                               Distance(*x, *y, xB, yB)) - 10;
                }
            }
            oopsnf();
            break;
        }
            
        case CONSTRAINT_AT_MIDPOINT: 
        case CONSTRAINT_POINT_ON_LINE:
        case CONSTRAINT_ON_CIRCLE:
            pts = 1; goto pointOnSomething;

        case CONSTRAINT_POINTS_COINCIDENT:
            pts = 2; goto pointOnSomething;

pointOnSomething:
        {
            // A `point on something' constraint is drawn as a big circle
            // around the relevant point. There's either one or two points
            // to encircle, depending on the type of constraint.
            int ri = 7;
            double r = toMicronsNotAffine(ri);

            if(op == DRAW_CONSTRAINT) {
                double xp, yp;
                int i;
                for(i = 0; i < pts; i++) {
                    EvalPoint((i == 0) ? c->ptA : c->ptB, &xp, &yp);
                    PltCircle(toPixelsX(xp), toPixelsY(yp)-1, ri);
                    *x = xp;
                    *y = yp;
                }
                return 0;
            } else if(op == GET_DISTANCE_TO_CONSTRAINT) {
                double xA, yA, xB, yB;
                double dA = VERY_POSITIVE, dB = VERY_POSITIVE;

                EvalPoint(c->ptA, &xA, &yA);
                dA = Distance(xA, yA, *x, *y);
                dA = fabs(dA - r);
        
                if(pts > 1) {
                    EvalPoint(c->ptB, &xB, &yB);
                    dB = Distance(xB, yB, *x, *y);
                    dB = fabs(dB - r);
                }

                return (dA < dB) ? dA : dB;
            }

            oopsnf();
            break;
        }

        case CONSTRAINT_FORCE_PARAM: {
            hParam hp = c->paramA;
            hPoint pt;
            double a = toMicronsNotAffine(12);
            double b = toMicronsNotAffine(5);
            double dx, dy;
            double xp, yp;
            double xo, yo;

            pt = POINT_FROM_PARAM(hp);
            if(hp & X_COORD_FOR_PT(0)) {
                dx = a; dy = 0;
                xo = 0; yo = b;
            } else {
                dx = 0; dy = a;
                xo = b; yo = 0;
            }
            EvalPoint(pt, &xp, &yp);

            if(op == DRAW_CONSTRAINT) {
                PltMoveToMicrons(xp + xo - dx, yp + yo - dy);
                PltLineToMicrons(xp + xo + dx, yp + yo + dy);
                PltMoveToMicrons(xp - xo - dx, yp - yo - dy);
                PltLineToMicrons(xp - xo + dx, yp - yo + dy);

                *x = xp;
                *y = yp;
                return 0;

            } else if(op == GET_DISTANCE_TO_CONSTRAINT) {
                double dA = DistanceFromPointToLine(*x, *y,
                    xp + xo -dx, yp + yo - dy, dx*2, dy*2, TRUE);
                double dB = DistanceFromPointToLine(*x, *y,
                    xp - xo -dx, yp - yo - dy, dx*2, dy*2, TRUE);
                return min(dA, dB);
            }

            oopsnf();
            break;
        }

        case CONSTRAINT_FORCE_ANGLE: {
            hPoint center = c->ptA;
            hPoint pt = c->ptB;

            double a = toMicronsNotAffine(12);
            double b = toMicronsNotAffine(5);
            double xp, yp;
            double xc, yc;
            double dx, dy;
            double xo, yo;

            EvalPoint(pt, &xp, &yp);
            EvalPoint(center, &xc, &yc);
            double theta = atan2(yc - yp, xc - xp);

            dx = a*sin(-theta);
            dy = a*cos(-theta);
            xo = b*cos(theta);
            yo = b*sin(theta);

            if(op == DRAW_CONSTRAINT) {
                PltMoveToMicrons(xp + xo - dx, yp + yo - dy);
                PltLineToMicrons(xp + xo + dx, yp + yo + dy);
                PltMoveToMicrons(xp - xo - dx, yp - yo - dy);
                PltLineToMicrons(xp - xo + dx, yp - yo + dy);

                *x = xp;
                *y = yp;
                return 0;

            } else if(op == GET_DISTANCE_TO_CONSTRAINT) {
                double dA = DistanceFromPointToLine(*x, *y,
                    xp + xo -dx, yp + yo - dy, dx*2, dy*2, TRUE);
                double dB = DistanceFromPointToLine(*x, *y,
                    xp - xo -dx, yp - yo - dy, dx*2, dy*2, TRUE);
                return min(dA, dB);
            }

            oopsnf();
            break;
        }

        case CONSTRAINT_SYMMETRIC: {
            // A symmetry constraint, drawn as arrows from the points toward
            // each other.
            double da = toMicronsNotAffine(22);
            double dh = toMicronsNotAffine(11);
            double xA, yA, xB, yB;
            EvalPoint(c->ptA, &xA, &yA);
            EvalPoint(c->ptB, &xB, &yB);

            double dxs = (xA - xB);
            double dys = (yA - yB);
            UnitVector(&dxs, &dys);

            if(op == DRAW_CONSTRAINT) {
                int i;
                for(i = 0; i < 2; i++) {
                    double xl, yl, dx, dy;
                    if(i == 0) {
                        xl = xA; yl = yA; dx = -dxs; dy = -dys;
                    } else {
                        xl = xB; yl = yB; dx = dxs; dy = dys;
                    }

                    // The position of the tip of the arrow.
                    double xt = xl + dx*da;
                    double yt = yl + dy*da;
                    // So we can plot the line from the tail to the tip.
                    PltMoveToMicrons(xl, yl);
                    PltLineToMicrons(xt, yt);
                    
                    int j;
                    for(j = 0; j < 2; j++) {
                        double thetaabs = DEGREES(25);
                        double theta = (j == 0) ? thetaabs : -thetaabs;
                        double dxh, dyh;

                        dxh =  cos(theta)*dx + sin(theta)*dy;
                        dyh = -sin(theta)*dx + cos(theta)*dy;

                        PltMoveToMicrons(xt, yt);
                        PltLineToMicrons(xt - dxh*dh, yt - dyh*dh);

                    }

                    *x = xt;
                    *y = yt;
                }
                return 0;

            } else if(op == GET_DISTANCE_TO_CONSTRAINT) {
                double dA, dB;

                dA = Distance(xA - dxs*da, yA - dys*da, *x, *y) - 9;
                dB = Distance(xB + dxs*da, yB + dys*da, *x, *y) - 9;
                return (dA < dB) ? dA : dB;
            }

            oopsnf();
            break;
        }

        case CONSTRAINT_PERPENDICULAR: {
            double x0[2], y0[2];
            double dx[2], dy[2];

            double xi, yi, xc, yc;

            IntersectionAndReasonablePoint(c,
                    x0, y0, dx, dy, &xi, &yi, &xc, &yc);

            xc += toMicronsNotAffine(12);
            yc += toMicronsNotAffine(12);
            double l = toMicronsNotAffine(15);

            if(op == DRAW_CONSTRAINT) {

                PltMoveToMicrons(xc, yc);
                PltLineToMicrons(xc+l, yc);
                PltMoveToMicrons(xc+(l/2), yc);
                PltLineToMicrons(xc+(l/2), yc+l);

                *x = xc;
                *y = yc;
                return 0;

            } else if(op == GET_DISTANCE_TO_CONSTRAINT) {
                return Distance(*x, *y, xc, yc) - l;
            }

            oopsnf();
            break;
        }

        case CONSTRAINT_PARALLEL: {
            int i;
            double xc[2], yc[2];
            double dx[2], dy[2];
            BOOL tangency[2];

            for(i = 0; i < 2; i++) {
                hEntity he = (i == 0) ? c->entityA : c->entityB;
                hLine hl = (i == 0) ? c->lineA : c->lineB;
                hPoint hp = (i == 0) ? c->ptA : c->ptB;

                double xA, yA, xB, yB;
                if(he && !hl && !hp) {
                    EvalPoint(POINT_FOR_ENTITY(he, 0), &xA, &yA);
                    EvalPoint(POINT_FOR_ENTITY(he, 1), &xB, &yB);
                    tangency[i] = FALSE;
                } else if(hl && !he && !hp) {
                    LineToPointsOnEdgeOfViewport(hl, &xA, &yA, &xB, &yB);
                    tangency[i] = FALSE;
                } else if(hp && !hl && !he) {
                    tangency[i] = TRUE;
                    EvalPoint(hp, &(xc[i]), &(yc[i]));
                    xc[i] += toMicronsNotAffine(10);
                    yc[i] += toMicronsNotAffine(20);
                } else oops();

                if(!tangency[i]) {
                    xc[i] = (2*xA + xB) / 3;
                    yc[i] = (2*yA + yB) / 3;

                    dx[i] = xA - xB;
                    dy[i] = yA - yB;

                    double m = Magnitude(dx[i], dy[i]);
                    if(!tol(m, 0)) {
                        dx[i] /= m;
                        dy[i] /= m;
                    }
                }
            }

            if(op == DRAW_CONSTRAINT) {
                double l = 6;
                double o = 3;
                for(i = 0; i < 2; i++) {
                    if(tangency[i]) {
                        // Parallel tangent, so at some particular point
                        // on a curve. Draw a big T there.
                        int j;
                        for(j = 0; j < 2; j++) {
                            PltText(toPixelsX(xc[i]), toPixelsY(yc[i]),
                                TRUE, "T");
                        }
                    } else {
                        // Parallel line or line segment, so draw some
                        // parallel markings at the middle of the line.
                        PltMoveTo(toint(toPixelsX(xc[i]) + o*dy[i] - l*dx[i]),
                                  toint(toPixelsY(yc[i]) - o*dx[i] - l*dy[i]));
                        PltLineTo(toint(toPixelsX(xc[i]) + o*dy[i] + l*dx[i]),
                                  toint(toPixelsY(yc[i]) - o*dx[i] + l*dy[i]));

                        PltMoveTo(toint(toPixelsX(xc[i]) - o*dy[i] - l*dx[i]),
                                  toint(toPixelsY(yc[i]) + o*dx[i] - l*dy[i]));
                        PltLineTo(toint(toPixelsX(xc[i]) - o*dy[i] + l*dx[i]),
                                  toint(toPixelsY(yc[i]) + o*dx[i] + l*dy[i]));
                    }
                }

                *x = xc[0];
                *y = yc[0];
                return 0;

            } else if(op == GET_DISTANCE_TO_CONSTRAINT) {
                double d[2];
                for(i = 0; i < 2; i++) {
                    d[i] = Distance(xc[i], yc[i], *x, *y) - 
                                                    toMicronsNotAffine(10);
                    if(d[i] < 0) d[i] = 0;
                }

                return min(d[0], d[1]);
            }

            oopsnf();
            break;
        }

        case CONSTRAINT_LINE_LINE_DISTANCE:
        case CONSTRAINT_PT_LINE_DISTANCE: {
            double x0, y0, dx, dy;
            double xp, yp;

            if(c->type == CONSTRAINT_PT_LINE_DISTANCE) {
                LineOrLineSegment(c->lineB, c->entityB, &x0, &y0, &dx, &dy);
                EvalPoint(c->ptA, &xp, &yp);
            } else {
                if(c->entityA) {
                    EvalPoint(POINT_FOR_ENTITY(c->entityA, 0), &xp, &yp);
                    LineOrLineSegment(c->lineB, c->entityB, &x0, &y0, &dx, &dy);
                } else if(c->entityB) {
                    EvalPoint(POINT_FOR_ENTITY(c->entityB, 0), &xp, &yp);
                    LineOrLineSegment(c->lineA, c->entityA, &x0, &y0, &dx, &dy);
                } else {
                    double fakeDx, fakeDy;
                    LineToParametric(c->lineA, &xp, &yp, &fakeDx, &fakeDy);
                    LineOrLineSegment(c->lineB, 0, &x0, &y0, &dx, &dy);
                }
            }

            double xc, yc;
            ClosestPointOnLineToPoint(&xc, &yc, xp, yp, x0, y0, dx, dy);

            // Calculate the position of the label to draw. This is measured
            // from the midpoint of the shortest line segment connecting
            // our point to our line.
            double xt, yt;
            xt = (xp + xc) / 2;
            yt = (yp + yc) / 2;
            xt += c->offset.x;
            yt += c->offset.y;

            if(op == DRAW_CONSTRAINT) {
                PltMoveToMicrons(xp, yp);
                PltLineToMicrons(xc, yc);
                PltText(toPixelsX(xt), toPixelsY(yt), FALSE, "%s",
                                                ToDisplay(fabs(c->v)));
                *x = xt;
                *y = yt;
                return 0;

            } else if(op == GET_LABEL_LOCATION) {
                *x = xt;
                *y = yt;
                return 0;

            } else if(op == GET_DISTANCE_TO_CONSTRAINT) {
                double dt = Distance(*x, *y, xt + toMicronsNotAffine(20), yt);
                // Make the label an effectively bigger target.
                dt -= toMicronsNotAffine(10);
                if(dt < 0) dt = 0;
                return dt;
            }

            oopsnf();
            break;
        }

        case CONSTRAINT_LINE_LINE_ANGLE: {
            double x0[2], y0[2], dx[2], dy[2];
            double xi, yi, xc, yc;

            IntersectionAndReasonablePoint(c,
                    x0, y0, dx, dy, &xi, &yi, &xc, &yc);

            // We would prefer to use the intersection point as our label
            // origin, if we can.
            xc = xi;
            yc = yi;

            xc += c->offset.x;
            yc += c->offset.y;

            if(op == DRAW_CONSTRAINT) {
                char buf[MAX_STRING];
                sprintf(buf, "%.1f°", fabs(c->v));
                PltText(toPixelsX(xc), toPixelsY(yc), FALSE, "%s", buf);

                double textWidth, textHeight;
                TextExtent(buf, &textWidth, &textHeight);

                xc += textWidth/2;
                yc -= textHeight/2;
                // And now (xc, yc) is at the center of the text.

                double r = Distance(xi, yi, xc, yc);
                double thetar = atan2(yc - yi, xc - xi);

                // Determine the correct offset distance, so that our arc
                // does not overwrite the label. This offset depends on
                // the angle; at +/- 90, it's textHeight/2, and at 0 or
                // 180, it's textWidth/2. In between, let's interpolate with
                // an ellipse.
                double a;
                a = EllipticalInterpolation((textWidth/2), (textHeight/2),
                        thetar);
                r -= a;
                // And a little more for good cosmetics.
                r -= toMicronsNotAffine(5);

                double theta[2];
                int i;
                for(i = 0; i < 2; i++) {
                    double xA, yA, xB, yB;
                    xA = x0[i]; yA = y0[i];
                    xB = x0[i] - dx[i]; yB = y0[i] - dy[i];

                    if(tol(xB, xi) && tol(yB, yi)) {
                        theta[i] = atan2(dy[i], dx[i]);
                    } else {
                        theta[i] = atan2(-dy[i], -dx[i]);
                    }
                }
                double dtheta = NormTheta(theta[1] - theta[0]);
              
                // We can draw one of two angles: the acute angle, or the
                // obtuse angle. These are actually equivalent, because the
                // constraint is only for angle modulo 180 degrees, but
                // we should draw the one it looks like.
                double thisSpan = fabs(dtheta*180/PI);
                double otherSpan = fabs(180 - thisSpan);
                double reqSpan = fabs(c->v);
                if(fabs(otherSpan - reqSpan) < fabs(thisSpan - reqSpan)) {
                    theta[0] += PI;
                    dtheta = NormTheta(theta[1] - theta[0]);
                }
       
                // Draw the arc on the more meaningful side (when two lines
                // intersect, there's four angles, and we've knocked out
                // two, so two left), determined by label placement.
                double distance = NormTheta((theta[0] + dtheta/2) - thetar);
                if(fabs(distance) > PI/2) {
                    theta[0] += PI;
                    theta[1] += PI;
                }

                PltMoveToMicrons(xi + r*cos(theta[0]), yi + r*sin(theta[0]));
                int n = 30;
                for(i = 0; i < n; i++) {
                    double phi = theta[0] + (dtheta*i)/(n-1);
                    PltLineToMicrons(xi + r*cos(phi), yi + r*sin(phi));
                }

                *x = xc;
                *y = yc;
                return 0;

            } else if(op == GET_LABEL_LOCATION) {
                *x = xc;
                *y = yc;
                return 0;

            } else if(op == GET_DISTANCE_TO_CONSTRAINT) {
                double dt = Distance(*x, *y, xc + toMicronsNotAffine(20), yc);
                // Make the label an effectively bigger target.
                dt -= toMicronsNotAffine(10);
                if(dt < 0) dt = 0;
                return dt;
            }

            oopsnf();
            break;
        }

        case CONSTRAINT_PT_PT_DISTANCE: {
            double xA, yA, xB, yB;
            EvalPoint(c->ptA, &xA, &yA);
            EvalPoint(c->ptB, &xB, &yB);

            double xt, yt;
            xt = (xA + xB) / 2;
            yt = (yA + yB) / 2;
            xt += c->offset.x;
            yt += c->offset.y;

            if(op == DRAW_CONSTRAINT) {
                double dx = (xA - xB);
                double dy = (yA - yB);

                double xpA, ypA;
                ClosestPointOnLineToPoint(&xpA, &ypA, xA, yA, xt, yt, dx, dy);
                PltMoveToMicrons(xpA, ypA);
                PltLineToMicrons(xA, yA);

                double xpB, ypB;
                ClosestPointOnLineToPoint(&xpB, &ypB, xB, yB, xt, yt, dx, dy);
                PltMoveToMicrons(xpB, ypB);
                PltLineToMicrons(xB, yB);

                PltText(toPixelsX(xt), toPixelsY(yt), FALSE, "%s",
                                                ToDisplay(c->v));
                *x = xt;
                *y = yt;
                return 0;

            } else if(op == GET_LABEL_LOCATION) {
                *x = xt;
                *y = yt;
                return 0;

            } else if(op == GET_DISTANCE_TO_CONSTRAINT) {
                double dt = Distance(*x, *y, xt + toMicronsNotAffine(20), yt);
                // Make the label an effectively bigger target.
                dt -= toMicronsNotAffine(10);
                if(dt < 0) dt = 0;
                return dt;
            }

            oopsnf();
            break;
        }

        case CONSTRAINT_RADIUS: {
            double xt, yt;
            double xc, yc;
            double r;

            if(EntityById(c->entityA)->type == ENTITY_CIRCLE) {
                hPoint center = POINT_FOR_ENTITY(c->entityA, 0);
                EvalPoint(center, &xc, &yc);

                r = EvalParam(PARAM_FOR_ENTITY(c->entityA, 0));
            } else {
                hPoint center = POINT_FOR_ENTITY(c->entityA, 2);
                EvalPoint(center, &xc, &yc);

                double xo, yo;
                EvalPoint(POINT_FOR_ENTITY(c->entityA, 0), &xo, &yo);
                r = Distance(xo, yo, xc, yc);
            }

            xt = xc + c->offset.x;
            yt = yc + c->offset.y;

            if(op == DRAW_CONSTRAINT) {
                // Draw the label where they asked for it.
                char buf[MAX_STRING];
                strcpy(buf, ToDisplay(c->v));
                PltText(toPixelsX(xt), toPixelsY(yt), FALSE, "%s", buf);
                // Draw a line from the label to the closest point on the
                // circle. First, determine the endpoint of the line near
                // the label.
                double w, h;
                TextExtent(buf, &w, &h);
                xt += w/2;
                yt -= h/2;

                // From the center of our label to the center of our circle.
                double dx = xc - xt;
                double dy = yc - yt;
                double theta = atan2(dy, dx);

                double a = EllipticalInterpolation(w/2, h/2, theta);
                a += toMicronsNotAffine(3);

                // Higher harmonic correction, otherwise it's too far out
                // at the corners.
                double cc = cos(2*(theta - PI/4));
                a -= toMicronsNotAffine(10)*cc*cc;

                double fromCenter = Magnitude(dx, dy);
                dx /= fromCenter; dy /= fromCenter;

                // Move to the closest point on the perimeter of the circle.
                PltMoveToMicrons(xc - dx*r, yc - dy*r);
                // And draw to our label. Separate cases for inside vs.
                // outside the circle.
                if(fromCenter > r) {
                    PltLineToMicrons(xt + dx*a, yt + dy*a);
                } else {
                    PltLineToMicrons(xt - dx*a, yt - dy*a);
                }

                *x = xt;
                *y = yt;
                return 0;

            } else if(op == GET_LABEL_LOCATION) {
                *x = xt;
                *y = yt;
                return 0;

            } else if(op == GET_DISTANCE_TO_CONSTRAINT) {
                double dt = Distance(*x, *y, xt + toMicronsNotAffine(20), yt);
                // Make the label an effectively bigger target.
                dt -= toMicronsNotAffine(10);
                if(dt < 0) dt = 0;
                return dt;
            }
        }

        case CONSTRAINT_EQUAL_RADIUS: {
            int i;
            double x0[2], y0[2];
            double dx[2], dy[2];

            for(i = 0; i < 2; i++) {
                hEntity he = (i == 0) ? c->entityA : c->entityB;
                SketchEntity *e = EntityById(he);

                double th;

                if(e->type == ENTITY_CIRCLE) {
                    double xc, yc;
                    EvalPoint(POINT_FOR_ENTITY(he, 0), &xc, &yc);
                    double r = EvalParam(PARAM_FOR_ENTITY(he, 0));
                    x0[i] = xc;
                    y0[i] = yc + r;
                    th = 0;
                } else {
                    double xc, yc;
                    EvalPoint(POINT_FOR_ENTITY(he, 2), &xc, &yc);
                    double xn, yn;
                    double th0, th1;
                    double r0,  r1;
                    EvalPoint(POINT_FOR_ENTITY(he, 0), &xn, &yn);
                    th0 = atan2(yn - yc, xn - xc);
                    r0 = Distance(xc, yc, xn, yn);
                    EvalPoint(POINT_FOR_ENTITY(he, 1), &xn, &yn);
                    th1 = atan2(yn - yc, xn - xc);
                    r1 = Distance(xc, yc, xn, yn);

                    double dth = th0 - th1;
                    while(dth < 0) dth += 2*PI;
                    while(dth >= 2*PI) dth -= 2*PI;

                    th = th0 - dth/2;
                    double rm = (r0 + r1)/2;
                    x0[i] = xc + rm*cos(th);
                    y0[i] = yc + rm*sin(th);

                    th = (PI/2) -th;
                }

                th += DEGREES(60);
                double d = toMicronsNotAffine(10);
                dx[i] = d*cos(-th);
                dy[i] = d*sin(-th);
            }

            if(op == DRAW_CONSTRAINT) {
                for(i = 0; i < 2; i++) {
                    PltMoveToMicrons(x0[i]-dx[i], y0[i]-dy[i]);
                    PltLineToMicrons(x0[i]+dx[i], y0[i]+dy[i]);
                }

                *x = x0[0];
                *y = y0[0];
                return 0;

            } else if(op == GET_DISTANCE_TO_CONSTRAINT) {
                double dMin = VERY_POSITIVE;
                for(i = 0; i < 2; i++) {
                    double d = DistanceFromPointToLine(*x, *y,
                                x0[i]-dx[i], y0[i]-dy[i],
                                2*dx[i],     2*dy[i],     TRUE);
                    d -= toMicronsNotAffine(2);
                    if(d < dMin) dMin = d;
                }
                return dMin;
            }

            oopsnf();
            break;
        }

        case CONSTRAINT_EQUAL_LENGTH: {
            int i;
            double x0[2], y0[2];
            double x1[2], y1[2];

            for(i = 0; i < 2; i++) {
                hEntity he = (i == 0) ? c->entityA : c->entityB;
                hPoint ptA = POINT_FOR_ENTITY(he, 0);
                hPoint ptB = POINT_FOR_ENTITY(he, 1);

                double xA, yA, xB, yB;
                EvalPoint(ptA, &xA, &yA);
                EvalPoint(ptB, &xB, &yB);

                // Place it a third of the way along, not at the middle, so
                // that it doesn't end up on top of parallel marks.
                double xc = (xA + 2*xB)/3;
                double yc = (yA + 2*yB)/3;
                double dx = xA - xB;
                double dy = yA - yB;

                // Standardize the direction that parallel markers point.
                if(dx > 0) {
                    dx = -dx;
                    dy = -dy;
                }
                double m = Magnitude(dx, dy);
                dx /= m; dy /= m;

                double d = toMicronsNotAffine(10);
                double th = DEGREES(45);

                double dxl = d*( cos( th)*dx + sin( th)*dy);
                double dyl = d*(-sin( th)*dx + cos( th)*dy);

                x0[i] = xc - dxl; y0[i] = yc - dyl;
                x1[i] = xc + dxl; y1[i] = yc + dyl;
            }

            if(op == DRAW_CONSTRAINT) {
                for(i = 0; i < 2; i++) {
                    PltMoveToMicrons(x0[i], y0[i]);
                    PltLineToMicrons(x1[i], y1[i]);
                }
                *x = x0[0];
                *y = y0[0];
                return 0;

            } else if(op == GET_DISTANCE_TO_CONSTRAINT) {
                double dMin = VERY_POSITIVE;
                for(i = 0; i < 2; i++) {
                    double d = DistanceFromPointToLine(*x, *y,
                        x0[i], y0[i], (x1[i] - x0[i]), (y1[i] - y0[i]), TRUE);
                    if(d < dMin) dMin = d;
                }
                return dMin;
            }

            oopsnf();
            break;
        }

        case CONSTRAINT_SCALE_MM:
        case CONSTRAINT_SCALE_INCH: {
            double xa, ya;
            hPoint hpa = POINT_FOR_ENTITY(c->entityA, 0);
            EvalPoint(hpa, &xa, &ya);
            double xb, yb;
            hPoint hpb = POINT_FOR_ENTITY(c->entityA, 1);
            EvalPoint(hpb, &xb, &yb);

            double xt = xa + c->offset.x;
            double yt = ya + c->offset.y;

            if(op == DRAW_CONSTRAINT) {
                double dx = xb - xa;
                double dy = yb - ya;
                double l = Magnitude(dx, dy);
                dx /= l; dy /= l;
                double d;
                // This is the signed distance from the point (xt, yt) to
                // the line defined by (xa, ya) + s*(dx, dy) for any real s
                d = dx*(yt - ya) - dy*(xt - xa);
                dx *= d; dy *= d;

                PltMoveToMicrons(xa, ya);
                PltLineToMicrons(xa-dy, ya+dx);
                PltMoveToMicrons(xb, yb);
                PltLineToMicrons(xb-dy, yb+dx);

                PltText(toPixelsX(xt), toPixelsY(yt), FALSE, "%.4g%s:1",
                    c->v,
                    (c->type == CONSTRAINT_SCALE_MM) ? " mm" : "\"");
                *x = xt;
                *y = yt;
                return 0;

            } else if(op == GET_LABEL_LOCATION) {
                *x = xt;
                *y = yt;
                return 0;

            } else if(op == GET_DISTANCE_TO_CONSTRAINT) {
                double dt = Distance(*x, *y, xt + toMicronsNotAffine(40), yt);
                // Make the label an effectively bigger target.
                dt -= toMicronsNotAffine(20);
                if(dt < 0) dt = 0;
                return dt;
            }

            oopsnf();
            break;
        }

        default:
            oopsnf();
            break;
    }
}
