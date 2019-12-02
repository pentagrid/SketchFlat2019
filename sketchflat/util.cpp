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
// General utility functions, mostly numerical.
//
// Jonathan Westhues, Jun 2007
//-----------------------------------------------------------------------------
#include "sketchflat.h"

BOOL told(double a, double b)
{
    double d = a - b;
    if(d < 0) d = -d;

    // Distances to within 1 um.
    return (d < 1);
}

BOOL tola(double a, double b)
{
    double d = a - b;
    if(d < 0) d = -d;

    // Angles to within 0.1 degrees, roughly
    return (d < 1.7e-3);
}

BOOL tol(double a, double b)
{
    double d = a - b;
    if(d < 0) d = -d;

    return (d < DEFAULT_TOL);
}

int toint(double v)
{
    return (int)floor(v + 0.5);
}

double Distance(double x0, double y0, double x1, double y1)
{
    return sqrt((x0 - x1)*(x0 - x1) + (y0 - y1)*(y0 - y1));
}

double Magnitude(double x, double y)
{
    return sqrt(x*x + y*y);
}
void UnitVector(double *x, double *y)
{
    double m = Magnitude(*x, *y);
    if(m != 0) {
        *x /= m;
        *y /= m;
    }
}

//-----------------------------------------------------------------------------
// Solve a linear system, by Gaussian elimination to a triangular matrix
// then back-substitution. Partial pivoting, returns TRUE for success, FALSE
// for a singular matrix.
//-----------------------------------------------------------------------------
BOOL SolveLinearSystem(double X[], double A[][MAX_UNKNOWNS_AT_ONCE], 
                                                        double B[], int N)
{
    // Gaussian elimination, with partial pivoting. It's an error if the
    // matrix is singular, because that means two constraints are
    // equivalent.
    int i, j, ip, jp, imax;
    double max, temp;

    for(i = 0; i < N; i++) {
        // We are trying eliminate the term in column i, for rows i+1 and
        // greater. First, find a pivot (between rows i and N-1).
        max = 0;
        for(ip = i; ip < N; ip++) {
            if(fabs(A[ip][i]) > max) {
                imax = ip;
                max = fabs(A[ip][i]);
            }
        }
        if(fabs(max) < 1e-12) return FALSE;

        // Swap row imax with row i
        for(jp = 0; jp < N; jp++) {
            temp = A[i][jp];
            A[i][jp] = A[imax][jp];
            A[imax][jp] = temp;
        }
        temp = B[i];
        B[i] = B[imax];
        B[imax] = temp;

        // For rows i+1 and greater, eliminate the term in column i.
        for(ip = i+1; ip < N; ip++) {
            temp = A[ip][i]/A[i][i];

            for(jp = 0; jp < N; jp++) {
                A[ip][jp] -= temp*(A[i][jp]);
            }
            B[ip] -= temp*B[i];
        }
    }

    // We've put the matrix in upper triangular form, so at this point we
    // can solve by back-substitution.
    for(i = N - 1; i >= 0; i--) {
        if(fabs(A[i][i]) < 1e-10) return FALSE;

        temp = B[i];
        for(j = N - 1; j > i; j--) {
            temp -= X[j]*A[i][j];
        }
        X[i] = temp / A[i][i];
    }

    return TRUE;
}

//-----------------------------------------------------------------------------
// Given either a line or a line segment (but not both), give a point on
// that line (or extension of the line segment) and a vector in its direction.
//-----------------------------------------------------------------------------
void LineOrLineSegment(hLine ln, hEntity e,
                            double *x0, double *y0, double *dx, double *dy)
{
    if(ln && e) oops();

    if(e) {
        hPoint ptA = POINT_FOR_ENTITY(e, 0);
        hPoint ptB = POINT_FOR_ENTITY(e, 1);

        *x0 = EvalParam(X_COORD_FOR_PT(ptA));
        *y0 = EvalParam(Y_COORD_FOR_PT(ptA));

        *dx = *x0 - EvalParam(X_COORD_FOR_PT(ptB));
        *dy = *y0 - EvalParam(Y_COORD_FOR_PT(ptB));
    } else {
        double theta = EvalParam(THETA_FOR_LINE(ln));
        double a = EvalParam(A_FOR_LINE(ln));

        RepAsPointOnLineAndDirection(theta, a, x0, y0, dx, dy);
    }
}

//-----------------------------------------------------------------------------
// Calculate the intersection point of two lines. Returns TRUE for success,
// FALSE if they're parallel.
//-----------------------------------------------------------------------------
BOOL IntersectionOfLines(double x0A, double y0A, double dxA, double dyA,
                         double x0B, double y0B, double dxB, double dyB,
                         double *xi, double *yi)
{
    double A[2][2];
    double b[2];

    // The line is given to us in the form:
    //    (x(t), y(t)) = (x0, y0) + t*(dx, dy)
    // so first rewrite it as
    //    (x - x0, y - y0) dot (dy, -dx) = 0
    //    x*dy - x0*dy - y*dx + y0*dx = 0
    //    x*dy - y*dx = (x0*dy - y0*dx)

    // So write the matrix, pre-pivoted.
    if(fabs(dyA) > fabs(dyB)) {
        A[0][0] = dyA;  A[0][1] = -dxA;  b[0] = x0A*dyA - y0A*dxA;
        A[1][0] = dyB;  A[1][1] = -dxB;  b[1] = x0B*dyB - y0B*dxB;
    } else {
        A[1][0] = dyA;  A[1][1] = -dxA;  b[1] = x0A*dyA - y0A*dxA;
        A[0][0] = dyB;  A[0][1] = -dxB;  b[0] = x0B*dyB - y0B*dxB;
    }

    // Check the determinant; if it's zero then no solution.
    if(tol(A[0][0]*A[1][1] - A[0][1]*A[1][0], 0)) {
        return FALSE;
    }
    
    // Solve
    double v = A[1][0] / A[0][0];
    A[1][0] -= A[0][0]*v;
    A[1][1] -= A[0][1]*v;
    b[1] -= b[0]*v;

    // Back-substitute.
    *yi = b[1] / A[1][1];
    *xi = (b[0] - A[0][1]*(*yi)) / A[0][0];

    return TRUE;
}

//-----------------------------------------------------------------------------
// Same thing, except instead of an intersection, returns the values of t
// for which the intersection occurs.
//-----------------------------------------------------------------------------
BOOL IntersectionOfLinesGetT(double x0A, double y0A, double dxA, double dyA,
                             double x0B, double y0B, double dxB, double dyB,
                             double *tA, double *tB)
{
    double xi, yi;

    if(!IntersectionOfLines(x0A, y0A, dxA, dyA,
                            x0B, y0B, dxB, dyB, &xi, &yi))
    {
        // Parallel, no solution.
        return FALSE;
    }

    // xi = x0 + t*dx
    // t = (xi - x0)/dx
    if(fabs(dxA) > fabs(dyA)) {
        *tA = (xi - x0A)/dxA;
    } else {
        *tA = (yi - y0A)/dyA;
    }
    if(fabs(dxB) > fabs(dyB)) {
        *tB = (xi - x0B)/dxB;
    } else {
        *tB = (yi - y0B)/dyB;
    }

    return TRUE;
}

//-----------------------------------------------------------------------------
// The distance from a point (xp, yp) to the line segment given in parametric
// form by (x(t), y(t)) = (x0, y0) + t*(dx, dy). If segment is true, then
// return distance to line segment as t goes from 0 to 1, else return 
// distance to the infinitely long line.
//-----------------------------------------------------------------------------
double DistanceFromPointToLine(double xp, double yp,
                               double x0, double y0,
                               double dx, double dy, BOOL segment)
{
    if((dx*dx + dy*dy) < 0.1) {
        // Our direction is defective, and will cause a numerical issue later.
        // (Typically (dx, dy) is either a vector in the drawing, in which
        // case this corresponds to something shorter than a micron, not
        // meaningful. It might also be a unit vector, but in that case a
        // magnitude less than sqrt(0.1) is certainly bad.)
        return VERY_POSITIVE;
    }
    
    // A point on the line is given by
    //
    //     (x(t), y(t)) =  (x0, y0) + t*(dx, y)
    //
    // The shortest distance from the point to the line is the perpendicular
    // distance, so a vector from (xp, yp) to (x(t), y(t)) is perpendicular
    // to (dx, dy), or
    //
    //     (x0 + t*dx - xp, y0 + t*dy - yp) dot (dx, dy) = 0 
    //     dx*(x0 + t*dx - xp) + dy*(y0 + t*dy - yp) = 0
    //     t*(dx^2 + dy^2) = dx*(xp - x0) + dy*(yp - y0)
    
    double t = (dx*(xp - x0) + dy*(yp - y0))/(dx*dx + dy*dy);

    if((t < 0 || t > 1) && segment) {
        // The closest point is one of the endpoints; determine which.
        double d0 = Distance(xp, yp, x0, y0);
        double d1 = Distance(xp, yp, x0 + dx, y0 + dy);

        if(d1 < d0) {
            return d1;
        } else {
            return d0;
        }
    } else {
        double xl = x0 + t*dx;
        double yl = y0 + t*dy;

        return Distance(xp, yp, xl, yl);
    }
}
void ClosestPointOnLineToPoint(double *xc, double *yc,
                               double xp, double yp,
                               double x0, double y0, double dx, double dy)
{
    if((dx*dx + dy*dy) < 0.1) {
        // Bogus data that's likely to be off-screen; this happens only when
        // something numerically bad happened first.
        *xc = x0;
        *yc = y0;
    }

    // See above for derivation.
    double t = (dx*(xp - x0) + dy*(yp - y0))/(dx*dx + dy*dy);

    *xc = x0 + t*dx;
    *yc = y0 + t*dy;
}

//-----------------------------------------------------------------------------
// An infinite line is defined by two parameters, a and theta. A parametric
// equation of the line has the form
//
//     r(t) = (x0, y0) + t*(dx, dy)
//
// where (dx, dy) = (cos theta, sin theta). To determine (x0, y0), define
// a second line
//
//     p(s) = s*(-dy, dx)
//
// This intersects r(t) when s = a. (Or equivalently, since (dx, dy) is a
// unit vector, the distance from the origin to the intersection is a.)
//
// These functions interchange between an (x0, y0), (dx, dy) representation
// and the [unique] (a, theta) representation.
//-----------------------------------------------------------------------------
void RepAsPointOnLineAndDirection(double theta, double a,
                double *x0, double *y0, double *dx, double *dy)
{
    *dx = cos(theta);
    *dy = sin(theta);

    // I would rather keep everything in [0, pi), but the solver doesn't
    // know that. This means that we end up with two possible representations
    // for any line, which is somewhat painful.

    *x0 = -(a * (*dy));
    *y0 =  (a * (*dx));
}
void RepAsAngleAndDistance(double x0, double y0, double dx, double dy,
                                double *theta, double *a)
{
    if(dy < 0) {
        dx = -dx;
        dy = -dy;
    }
    if(dx == 0 && dy == 0) {
        dx = 0;
        dy = 1;
    }
    *theta = atan2(dy, dx);

    // Let (dx, dy) be normalized to a unit vector. Our line is given by
    //     r(t) = (x0, y0) + t*(dx, dy)
    // and it intersects
    //     p(s) = s*(-dy, dx)
    // at s = a; so a is the component of (x0, y0) is the direction of
    // (-dy, dx), or
    //     a = (x0, y0) dot (-dy, dx) = (y0*dx - x0*dy)

    // Normalize to a unit vector.
    double r = sqrt(dx*dx + dy*dy);
    dx /= r;
    dy /= r;

    // Dot product.
    *a = (y0*dx - x0*dy);
}

//-----------------------------------------------------------------------------
// For a given (infinite, datum) line in the sketch, return some
// representation of the form (x(t), y(t)) = (x0, y0) + t*(dx, dy)
//-----------------------------------------------------------------------------
void LineToParametric(hLine ln, double *x0, double *y0, double *dx, double *dy)
{
    hParam thetap = THETA_FOR_LINE(ln);
    hParam ap = A_FOR_LINE(ln);

    double theta = EvalParam(thetap);
    double a = EvalParam(ap);

    RepAsPointOnLineAndDirection(theta, a, x0, y0, dx, dy);
}

//-----------------------------------------------------------------------------
// We are given an infinitely long line, that we wish to display on-screen.
// The viewport on screen is rectangular; in general, this means that the
// line intersects it twice or not at all. Either find those two intersections,
// if they exist, or return two coincident off-screen points.
//-----------------------------------------------------------------------------
void LineToPointsOnEdgeOfViewport(hLine ln, double *x1, double *y1,
                                            double *x2, double *y2)
{
    // Get the extent of the drawing area.
    int xMin, yMin, xMax, yMax;
    double xMinf, yMinf, xMaxf, yMaxf;
    PltGetRegion(&xMin, &yMin, &xMax, &yMax);
    // And convert from pixels to microns.
    xMinf = toMicronsX(xMin);
    xMaxf = toMicronsX(xMax);
    yMinf = toMicronsY(yMin);
    yMaxf = toMicronsY(yMax);

//    dbp("");
//    dbp("extent: (%.0f, %.0f) to (%.0f, %.0f)", xMinf, yMinf,
//        xMaxf, yMaxf);

    double x0, y0, dx, dy;
    LineToParametric(ln, &x0, &y0, &dx, &dy);

    // So the line has the form
    //      (x(t), y(t)) = (x0, y0) + t*(dx, dy)
    // and we need to determine over what range of t we should plot. We
    // do this by intersecting the line with the horizontal and vertical
    // lines that bound the screen. 

    double t, xt, yt;
    DoublePoint pts[4];
    int n = 0;

#define ADD_POINT_IF_IN_VIEWPORT_CHECKING_Y() \
        xt = x0 + dx*t; \
        yt = y0 + dy*t; \
        if(yt > yMinf && yt < yMaxf) { \
            pts[n].x = xt; \
            pts[n].y = yt; \
            n++; \
        }

#define ADD_POINT_IF_IN_VIEWPORT_CHECKING_X() \
        xt = x0 + dx*t; \
        yt = y0 + dy*t; \
        if(xt > xMinf && xt < xMaxf) { \
            pts[n].x = xt; \
            pts[n].y = yt; \
            n++; \
        }

    // There's four edges that it might intersect. In general it hits either
    // none of them or two of them, ignoring cases where it's right on the
    // edge.

    if(dx != 0) {
        // x = xMinf
        t = (xMinf - x0) / dx;
        ADD_POINT_IF_IN_VIEWPORT_CHECKING_Y();
        // x = xMaxf
        t = (xMaxf - x0) / dx;
        ADD_POINT_IF_IN_VIEWPORT_CHECKING_Y();
    }

    if(dy != 0) {
        // y = yMinf
        t = (yMinf - y0) / dy;
        ADD_POINT_IF_IN_VIEWPORT_CHECKING_X();
        // y = yMaxf
        t = (yMaxf - y0)  /dy;
        ADD_POINT_IF_IN_VIEWPORT_CHECKING_X();
    }

    if(n == 2) {
        if(pts[0].y > pts[1].y) {
            *x1 = pts[0].x;
            *y1 = pts[0].y;
            *x2 = pts[1].x;
            *y2 = pts[1].y;
        } else {
            *x1 = pts[1].x;
            *y1 = pts[1].y;
            *x2 = pts[0].x;
            *y2 = pts[0].y;
        }
    } else {
        // It lies outside the viewport, so return a bogus off-screen point.
        *x1 = xMinf - 20000;
        *y1 = yMinf - 20000;
        *x2 = xMinf - 20000;
        *y2 = yMinf - 20000;
    }

//    dbp("line: (%.0f, %.0f) to (%.0f, %.0f)", *x1, *y1, *x2, *y2);
}

//-----------------------------------------------------------------------------
// The selection is a list of entities, of different types. When we are
// processing that, we often wish to see it grouped by type, so that all
// the points are together, and all the lines are together, and so on.
//-----------------------------------------------------------------------------
void GroupSelection(GroupedSelection *gs)
{
    memset(gs, 0, sizeof(*gs));

    int i;
    for(i = 0; i < MAX_SELECTED_ITEMS; i++) {
        if(Selected[i].which == SEL_POINT) {
            gs->point[gs->points] = Selected[i].point;
            (gs->points)++;

            hEntity he = ENTITY_FROM_POINT(Selected[i].point);
            if(he != REFERENCE_ENTITY) {
                SketchEntity *e = EntityById(he);
                if(e->type == ENTITY_CIRCULAR_ARC) {
                    // For tangency to a circular arc at its endpoints.
                    (gs->anyDirections)++;
                } else if(e->type == ENTITY_CUBIC_SPLINE) {
                    int k = K_FROM_POINT(Selected[i].point);
                    if(k == 0 || k == (e->points - 1)) {
                        // For tangency to a spline at its endpoints.
                        (gs->anyDirections)++;
                    }
                }
            }

            (gs->n)++;
        }
        if(Selected[i].which == SEL_LINE) {
            gs->line[gs->lines] = Selected[i].line;
            (gs->lines)++;
            (gs->anyLines)++;
            (gs->anyDirections)++;

            (gs->n)++;
        }
        if(Selected[i].which == SEL_ENTITY) {
            SketchEntity *e = EntityById(Selected[i].entity);
            if(e->type == ENTITY_LINE_SEGMENT) {
                (gs->anyLines)++;
                (gs->anyDirections)++;
            } else {
                (gs->nonLineEntities)++;
            }
            if(e->type == ENTITY_CIRCLE || e->type == ENTITY_CIRCULAR_ARC) {
                (gs->circlesOrArcs)++;
            }

            gs->entity[gs->entities] = Selected[i].entity;
            (gs->entities)++;

            (gs->n)++;
        }
    }
}
