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
// Polygon stuff, assembling them from loose pwls, testing whether a point
// lies inside, boolean operations, etc. This an afterthought, and of much
// worse quality than the rest of the program.
// Jonathan Westhues, May 2007
//-----------------------------------------------------------------------------
#include "sketchflat.h"

static DoublePoint PtBuf[MAX_PWLS_IN_SKETCH];

static SketchPwl AllBuf[MAX_PWLS_IN_SKETCH];
static SketchPwl BrokenBuf[MAX_PWLS_IN_SKETCH];
static int AllCnt, BrokenCnt;

//-----------------------------------------------------------------------------
// Does a given closed curve run clockwise or counterclockwise?
//-----------------------------------------------------------------------------
static BOOL ClosedCurveIsClockwise(DoublePoint *pt, int pts)
{
    // Use the signed area trick: sum up the signed areas of the trapezoids
    // from each edge to the x-axis. The magnitude of the sum is equal to
    // the area of the polygon, and the sign gives its direction.
    double sum = 0;
    int i;
    for(i = 1; i < pts; i++) {
        sum += ((pt[i-1].y + pt[i].y)/2)*(pt[i].x - pt[i-1].x);
    }

    return (sum > 0);
}

//-----------------------------------------------------------------------------
// Take the pts points in PtBuf, allocate memory for them, copy them in to
// that memory, and return that. Also standardizes the direction of the
// curve.
//-----------------------------------------------------------------------------
static DoublePoint *AllocPointsForClosedCurve(int pts)
{
    DoublePoint *ret = (DoublePoint *)DAlloc(pts * sizeof(DoublePoint));

    if(ClosedCurveIsClockwise(PtBuf, pts)) {
        // It's in the right order to start with, just copy it over.
        memcpy(ret, PtBuf, pts * sizeof(DoublePoint));
    } else {
        int i, j;
        for(i = 0, j = pts - 1; i < pts; i++, j--) {
            ret[i].x = PtBuf[j].x;
            ret[i].y = PtBuf[j].y;
        }
    }

    return ret;
}

void PolygonAssemble(DPolygon *p, SketchPwl *src, int n,
                                    hLayer lr, BOOL *leftovers)
{
    *leftovers = FALSE;

    int i;
    // Mark all the pwls on this layer as unused.
    for(i = 0; i < n; i++) {
        if(src[i].layer == lr && !src[i].construction) {
            src[i].tag = 1;
        } else {
            src[i].tag = 0;
        }
    }

    for(;;) {
        // Find an edge that we haven't used yet.
        for(i = 0; i < n; i++) {
            if(src[i].tag) {
                break;
            }
        }
        if(i >= n) {
            // We're done, no more edges on this layer, success.
            break;
        }

        src[i].tag = 0;

        int pts = 0;
        PtBuf[pts].x = src[i].x0;
        PtBuf[pts].y = src[i].y0;
        pts++;
        PtBuf[pts].x = src[i].x1;
        PtBuf[pts].y = src[i].y1;
        pts++;
        
        do {
            // Find an edge that connects.
            for(i = 0; i < n; i++) {
                SketchPwl *pwl = &(src[i]);
                if(!(pwl->tag)) continue;

                if(tol(pwl->x0, PtBuf[pts-1].x) && 
                   tol(pwl->y0, PtBuf[pts-1].y))
                {
                    pwl->tag = 0;
                    PtBuf[pts].x = pwl->x1;
                    PtBuf[pts].y = pwl->y1;
                    pts++;
                    break;
                }
                if(tol(pwl->x1, PtBuf[pts-1].x) && 
                   tol(pwl->y1, PtBuf[pts-1].y))
                {
                    pwl->tag = 0;
                    PtBuf[pts].x = pwl->x0;
                    PtBuf[pts].y = pwl->y0;
                    pts++;
                    break;
                }
            }
            // If we couldn't find a connecting edge, then the layer contains
            // an open curve.
            if(i >= n) {
                *leftovers = TRUE;
                // but might as well generate the (open) curve anyways and
                // keep going; more useful than just giving up
                break;
            }
        } while(!(tol(PtBuf[pts-1].x, PtBuf[0].x) &&
                  tol(PtBuf[pts-1].y, PtBuf[0].y)));

        // If we think it's an approximately closed curve, from the numerical
        // tolerance, then force the endpoints so that it's really a closed
        // curve. Otherwise, we will run into numerical trouble later, for
        // stuff like the point-in-closed-curve routine.
        if(tol(PtBuf[pts-1].x, PtBuf[0].x) && tol(PtBuf[pts-1].y, PtBuf[0].y)) {
            PtBuf[pts-1].x = PtBuf[0].x;
            PtBuf[pts-1].y = PtBuf[0].y;
        }
    
        // So we have a closed curve.
        i = p->curves;
        if(i >= MAX_CLOSED_CURVES_IN_POLYGON) return;

        p->curve[i].pts = pts;
        p->curve[i].pt = AllocPointsForClosedCurve(pts);
        p->curves = (i + 1);
    }
}

static BOOL ClosedCurveContainsPoint(DClosedCurve *c, double xp, double yp)
{
    int i;
    // Cast a horizontal ray from (x, y), in the increasing x direction, and
    // change from inside to outside each time we cross an edge.
    BOOL inside = FALSE;

    for(i = 1; i < c->pts; i++) {
        DoublePoint *pA = &(c->pt[i]);
        DoublePoint *pB = &(c->pt[i-1]);

        // Write the parametric equation for the line, standardized so that
        // t=0 has smaller y than t=1
        double x0, y0, dx, dy;
        if(pA->y < pB->y) {
            x0 = pA->x; y0 = pA->y;
            dx = (pB->x - pA->x); dy = (pB->y - pA->y);
        } else {
            x0 = pB->x; y0 = pB->y;
            dx = (pA->x - pB->x); dy = (pA->y - pB->y);
        }

        if(dy == 0) {
            continue;
        }

        double t = (yp - y0)/dy;
        double xi = x0 + t*dx;

        if(xi > xp && t >= 0 && t < 1) inside = !inside;
    }

    return inside;
}

static int TimesEnclosed(double xp, double yp, DPolygon *p, int ignore)
{
    int enclosures = 0;
    int i;
    for(i = 0; i < p->curves; i++) {
        if(i == ignore) continue;

        if(ClosedCurveContainsPoint(&(p->curve[i]), xp, yp)) {
            enclosures++;
        }
    }
    return enclosures;
}

//-----------------------------------------------------------------------------
// Break a polygon down into many piecwise linear segments, abandoning any
// concept of what connects where. This works on all the closed curves of
// the polygon, and adds to AllBuf.
//-----------------------------------------------------------------------------
static void PwlsFromPolygon(DPolygon *p, int tag)
{
    int i;
    for(i = 0; i < p->curves; i++) {
        int j;
        DClosedCurve *c = &(p->curve[i]);
        for(j = 1; j < c->pts; j++) {
            if(AllCnt >= arraylen(AllBuf)) break;

            AllBuf[AllCnt].x0 = c->pt[j-1].x;
            AllBuf[AllCnt].y0 = c->pt[j-1].y;
            AllBuf[AllCnt].x1 = c->pt[j].x;
            AllBuf[AllCnt].y1 = c->pt[j].y;
            AllBuf[AllCnt].construction = FALSE;
            AllBuf[AllCnt].layer = 0;
            AllBuf[AllCnt].tag = tag;
            AllCnt++;
        }
    }
}

//-----------------------------------------------------------------------------
// Helpers for PolygonBoolean. The data structure represents intersections
// between the "i" edge, currently being considered, and some other "j"
// edges, which will be sorted by their length "ti" along the "i" edge.
// (ByTi is a comparison function for the sort.)
//-----------------------------------------------------------------------------
typedef struct {
    int     j;
    double  ti;
} Intersection;
static int ByTi(const void *av, const void *bv)
{
    const Intersection *a = (const Intersection *)av;
    const Intersection *b = (const Intersection *)bv;

    double v = a->ti - b->ti;
    if(v < 0) {
        return -1;
    } else if(v > 0) {
        return 1;
    } else {
        return 0;
    }
}
//-----------------------------------------------------------------------------
// A Boolean op on a polygon: union or difference. This is a non-optimal
// implementation, but I like this algorithm because it is easy to understand.
// The polygons are first flattened to a set of line segments. A line
// segment that intersects another line segment is split into two collinear
// segments, until the line segments meet each other only at their endpoints.
// Certain line segments are culled, depending on the desired operation, and
// those left standing are re-assembled into the output polygon.
//-----------------------------------------------------------------------------
static BOOL PolygonBoolean(DPolygon *dest, DPolygon *pa, DPolygon *pb,
                                                            BOOL difference)
{
    // First, flatten all the curves from both polygons into one big list
    // of line segments.
    AllCnt = 0;
    PwlsFromPolygon(pa, 0);
    PwlsFromPolygon(pb, 1);

    // For those line segments that intersect other line segments at some
    // point along their length, split them into multiple collinear line
    // segments with intermediate points at the intersections.
    BrokenCnt = 0;
    int i;
    for(i = 0; i < AllCnt; i++) {
        Intersection intersect[100];
        int intersects = 0;

        double x0 = AllBuf[i].x0, y0 = AllBuf[i].y0;
        double x1 = AllBuf[i].x1, y1 = AllBuf[i].y1;
        double dx = x1 - x0, dy = y1 - y0;
        int j;
        for(j = 0; j < AllCnt; j++) {
            if(i == j) continue;
            SketchPwl *pwl = &(AllBuf[j]);
            double ti, tj;
            if(!IntersectionOfLinesGetT(
                    x0, y0, dx, dy,
                    pwl->x0, pwl->y0, (pwl->x1 - pwl->x0), (pwl->y1 - pwl->y0),
                    &ti, &tj))
            {
                // parallel, no intersection
                continue;
            }
            // Need to go a bit beyond the edge, so that nothing breaks if
            // an edge of polygon A passes through a vertex of polygon B.
            if(tj < -0.0001 || tj > 1.0001) continue; // doesn't hit j segement
            if(ti < -0.0001 || ti > 1.0001) continue; // doesn't hit i segment

            if(intersects < arraylen(intersect)) {
                intersect[intersects].j = j;
                intersect[intersects].ti = ti;
                intersects++;
            }
        }
        // In addition to the intersection points, we always want to generate
        // the original endpoints.
        intersect[intersects].j = -1;
        intersect[intersects].ti = 1;
        intersects++;
        intersect[intersects].j = -1;
        intersect[intersects].ti = 0;
        intersects++;
        // Order the intersections, from first encountered to last.
        qsort(intersect, intersects, sizeof(intersect[0]), ByTi);
        // And generate the broken-apart line segment.
        int k;
        for(k = 1; k < intersects; k++) {
            if(BrokenCnt >= arraylen(BrokenBuf)) break;

            double t0 = intersect[k-1].ti;
            double t1 = intersect[k].ti;
            BrokenBuf[BrokenCnt].x0 = x0 + dx*t0;
            BrokenBuf[BrokenCnt].y0 = y0 + dy*t0;
            BrokenBuf[BrokenCnt].x1 = x0 + dx*t1;
            BrokenBuf[BrokenCnt].y1 = y0 + dy*t1;
            BrokenBuf[BrokenCnt].construction = FALSE;
            BrokenBuf[BrokenCnt].layer = 0;
            BrokenBuf[BrokenCnt].tag = AllBuf[i].tag;
            BrokenCnt++;
        }
    }

    // Now, cull those broken line segments that should not lie on the
    // resulting curve.
    for(i = 0; i < BrokenCnt; i++) {
        SketchPwl *pwl = &(BrokenBuf[i]);
        // This odd choice of midpoint helps to paper over later numerical
        // problems. (Closed polygons that actually aren't quite closed are
        // most likely to hit at the actual (0.5) midpoint of something.)
        // I don't think it's "necessary" right now, but doesn't hurt.
        double xm = (pwl->x0*49 + pwl->x1*48)/97;
        double ym = (pwl->y0*49 + pwl->y1*48)/97;

        BOOL onA = (pwl->tag == 0);
        BOOL inA = TimesEnclosed(xm, ym, pa, -1) & 1;
        BOOL onB = (pwl->tag == 1);
        BOOL inB = TimesEnclosed(xm, ym, pb, -1) & 1;
        if(!(onA || onB)) oops();

        if(difference) {
            if((onB && !inA) || (onA && inB)) {
                pwl->construction = TRUE;
            }
        } else {
            if((onA && inB) || (onB && inA)) {
                pwl->construction = TRUE;
            }
        }

        if(tol(pwl->x0, pwl->x1) && tol(pwl->y0, pwl->y1)) {
            pwl->construction = TRUE;
        }
    }

    // Finally, assembled the un-culled line segments into our output.
    BOOL leftovers;
    PolygonAssemble(dest, BrokenBuf, BrokenCnt, 0, &leftovers);

    return TRUE;
}

BOOL PolygonUnion(DPolygon *dest, DPolygon *pa, DPolygon *pb)
{
    return PolygonBoolean(dest, pa, pb, FALSE);
}

BOOL PolygonDifference(DPolygon *dest, DPolygon *pa, DPolygon *pb)
{
    return PolygonBoolean(dest, pa, pb, TRUE);
}

//-----------------------------------------------------------------------------
// Append the closed curves in the source polygon to the destination
// polygon. The points are put through the map [ xn yn ]' = [ a b ; cd ]
// [ xo yo ]' + [ dx dy ]' as they are copied.
//-----------------------------------------------------------------------------
static void AppendToPolygon(DPolygon *dest, DPolygon *src, 
                double a, double b, double c, double d,
                double dx, double dy)
{
    if(dest->curves + src->curves > MAX_CLOSED_CURVES_IN_POLYGON) return;

    int n = dest->curves;

    int i;
    for(i = 0; i < src->curves; i++) {
        int pts = src->curve[i].pts;
        int size = pts*sizeof(DoublePoint);

        dest->curve[n].pt = (DoublePoint *)DAlloc(size);
        int j;
        for(j = 0; j < pts; j++) {
            DoublePoint *dp = &(dest->curve[n].pt[j]);
            DoublePoint *sp = &(src->curve[i].pt[j]);

            dp->x = a*(sp->x) + b*(sp->y) + dx;
            dp->y = c*(sp->x) + d*(sp->y) + dy;
        }
        dest->curve[n].pts = pts;
        n++;
    }

    dest->curves = n;
}

BOOL PolygonSuperimpose(DPolygon *dest, DPolygon *pa, DPolygon *pb)
{
    if(pa->curves + pb->curves > MAX_CLOSED_CURVES_IN_POLYGON) return FALSE;

    dest->curves = 0;
    AppendToPolygon(dest, pa, 1, 0, 0, 1, 0, 0);
    AppendToPolygon(dest, pb, 1, 0, 0, 1, 0, 0);

    return TRUE;
}

BOOL PolygonScale(DPolygon *dest, DPolygon *pa, double scale)
{
    dest->curves = 0;
    AppendToPolygon(dest, pa, scale, 0, 0, scale, 0, 0);

    return TRUE;
}

BOOL PolygonStepTranslating(DPolygon *dest, DPolygon *src, 
                            double dx0, double dy0, double dxs, double dys,
                            int n)
{
    dest->curves = 0;
    int i;
    for(i = 0; i < n; i++) {
        double dx = dx0 + i*dxs;
        double dy = dy0 + i*dys;
        AppendToPolygon(dest, src, 1, 0, 0, 1, dx, dy);
    }

    return TRUE;
}

BOOL PolygonStepRotating(DPolygon *dest, DPolygon *src, 
                            double xa, double ya,
                            double theta0, double thetas, int n)
{
    dest->curves = 0;

    int i;
    for(i = 0; i < n; i++) {
        // We want to rotate our point (x, y) about (xa, ya). This is
        // equivalent to rotating (x - xa, y - ya) about the origin, and
        // then adding back (xa, ya). So we have
        //      [ xr ] = [  c  s ][ x - xa ] + [ xa ]
        //      [ yr ]   [ -s  c ][ y - ya ]   [ ya ]
        // or
        //        xr = c*(x - xa) + s*(y - ya) + xa
        //           = c*x + s*y + (xa - c*xa - s*ya)
        //        yr = -s*(x - xa) + c*(y - ya) + ya
        //           = -s*x + c*y + (ya + s*xa - c*ya)

        double theta = theta0 + i*thetas;
        double c = cos(theta);
        double s = sin(theta);

        AppendToPolygon(dest, src,
             c,  s,
            -s,  c,
                                            xa - c*xa - s*ya,
                                            ya + s*xa - c*ya);
    }

    return TRUE;
}

BOOL PolygonMirror(DPolygon *dest, DPolygon *src, 
                            double x0, double y0, double x1, double y1)
{
    double dx = x1 - x0, dy = y1 - y0;
    double theta = atan2(dy, dx);

    // Similar situation to the reflections; translated csys gives
    //      [ xm ] = [ c  s ][ x - x0 ] + [ x0 ]
    //      [ ym ]   [ s -c ][ y - y0 ]   [ y0 ]
    // or
    //        xm = c*(x - x0) + s*(y - y0) + x0
    //           = c*x + s*y + (x0 - c*x0 - s*y0)
    //        ym = s*(x - x0) - c*(y - y0) + y0
    //           = s*x - c*y + (y0 - s*x0 + c*y0)
    double s = sin(2*theta);
    double c = cos(2*theta);

    dest->curves = 0;
    AppendToPolygon(dest, src,
        c,  s,
        s, -c,
                                            x0 - c*x0 - s*y0,
                                            y0 - s*x0 + c*y0);
    return TRUE;
}

//-----------------------------------------------------------------------------
// Tack a point on to our static buffer in which we represent closed curves
// as we build them up.
//-----------------------------------------------------------------------------
static void WritePt(int *pts, double x, double y)
{
    if(*pts < MAX_PWLS_IN_SKETCH) {
        PtBuf[*pts].x = x;
        PtBuf[*pts].y = y;
        (*pts)++;
    }
}

//-----------------------------------------------------------------------------
// Return a value equal to i (mod n), between 0 and (n - 1).
//-----------------------------------------------------------------------------
static int Wrap(int i, int n)
{
    while(i < 0) {
        i += n;
    }
    while(i >= n) {
        i -= n;
    }
    return i;
}
static void OffsetClosedCurve(DClosedCurve *dest, DClosedCurve *src, 
                                            double radius, BOOL inwards)
{
    int pts = 0;

    int i, nri;
    int nriInitial, nriFinal, nriIncrement;

    if(inwards) {
        nriInitial = 0;
        nriFinal = src->pts;
        nriIncrement = 1;
    } else {
        nriInitial = src->pts - 1;
        nriFinal = -1;
        nriIncrement = -1;
    }
    for(nri = nriInitial; nri != nriFinal; nri += nriIncrement) {
        DoublePoint a, b, c;
        double dxp, dyp, dxn, dyn;
        double thetan, thetap;

        i = Wrap(nri, (src->pts-1));

        if(inwards) {
            a = src->pt[Wrap(i-1, (src->pts-1))];
            b = src->pt[Wrap(i,   (src->pts-1))];
            c = src->pt[Wrap(i+1, (src->pts-1))];
        } else {
            c = src->pt[Wrap(i-1, (src->pts-1))];
            b = src->pt[Wrap(i,   (src->pts-1))];
            a = src->pt[Wrap(i+1, (src->pts-1))];
        }

        dxp = a.x - b.x;
        dyp = a.y - b.y;
        thetap = atan2(dyp, dxp);

        dxn = b.x - c.x;
        dyn = b.y - c.y;
        thetan = atan2(dyn, dxn);

        // A short line segment in a badly-generated polygon might look
        // okay but screw up our sense of direction.
        if(Magnitude(dxp, dyp) < 10 || Magnitude(dxn, dyn) < 10) {
            continue;
        }

        if(thetan > thetap && (thetan - thetap) > PI) {
            thetap += 2*PI;
        }
        if(thetan < thetap && (thetap - thetan) > PI) {
            thetan += 2*PI;
        }
    
        if(fabs(thetan - thetap) < (1*PI)/180) {
            WritePt(&pts, b.x - radius*sin(thetap), b.y + radius*cos(thetap));
        } else if(thetan < thetap) {
            // This is an inside corner. We have two edges, Ep and En. Move
            // out from their intersection by radius, normal to En, and
            // then draw a line parallel to En. Move out from their
            // intersection by radius, normal to Ep, and then draw a second
            // line parallel to Ep. The point that we want to generate is
            // the intersection of these two lines--it removes as much
            // material as we can without removing any that we shouldn't.
            double px0, py0, pdx, pdy;
            double nx0, ny0, ndx, ndy;
            double x, y;

            px0 = b.x - radius*sin(thetap);
            py0 = b.y + radius*cos(thetap);
            pdx = cos(thetap);
            pdy = sin(thetap);

            nx0 = b.x - radius*sin(thetan);
            ny0 = b.y + radius*cos(thetan);
            ndx = cos(thetan);
            ndy = sin(thetan);

            IntersectionOfLines(px0, py0, pdx, pdy, 
                                nx0, ny0, ndx, ndy,
                                &x, &y);
            
            WritePt(&pts, x, y);
        } else {
            if(fabs(thetap - thetan) < (6*PI)/180) {
                WritePt(&pts, b.x - radius*sin(thetap),
                              b.y + radius*cos(thetap));
                WritePt(&pts, b.x - radius*sin(thetan),
                              b.y + radius*cos(thetan));
            } else {
                double theta;
                for(theta = thetap; theta <= thetan; theta += (6*PI)/180) {
                    WritePt(&pts, b.x - radius*sin(theta),
                                  b.y + radius*cos(theta));
                }
            }
        }
    }

    dest->pts = pts;
    dest->pt = AllocPointsForClosedCurve(pts);
}
BOOL PolygonOffset(DPolygon *dest, DPolygon *src, double radius)
{
    int i;
    for(i = 0; i < src->curves; i++) {
        BOOL inwards;
        int v;
        v = TimesEnclosed(src->curve[i].pt[0].x, src->curve[i].pt[0].y, src, i);
        if(v & 1) {
            inwards = TRUE;
        } else {
            inwards = FALSE;
        }

        OffsetClosedCurve(&(dest->curve[i]), &(src->curve[i]), radius, inwards);
    }
    dest->curves = src->curves;
    return TRUE;
}

void RoundCornersForClosedCurve(DClosedCurve *dest, DClosedCurve *src,
                                    double radius, hPoint *rpt, int rpts)
{
    // This is where we are coming from.
    DoublePoint from = src->pt[src->pts - 2];
    int pts = 0;

    int i;
    for(i = 0; i < src->pts; i++) {
        DoublePoint now = src->pt[i];
        DoublePoint to = src->pt[i == (src->pts - 1) ? 1 : i+1];

        int j;
        for(j = 0; j < rpts; j++) {
            double xr, yr;
            // Don't blow up if the user deleted whatever used to source
            // our points.
            if(PointExistsInSketch(rpt[j])) {
                EvalPoint(rpt[j], &xr, &yr);
                if(tol(now.x, xr) && tol(now.y, yr)) {
                    break;
                }
            }
        }
        if(j >= rpts) {
            // This vertex is not getting rounded; just generate it and go.
            from = now;
            WritePt(&pts, now.x, now.y);
            continue;
        }

        // We are supposed to round this point. First, calculate the start
        // and finish angles of our arc.
        double thetaFrom = atan2(from.y - now.y, from.x - now.x);
        double thetaTo = atan2(now.y - to.y, now.x - to.x);
        if(thetaTo > thetaFrom && (thetaTo - thetaFrom) > PI) {
            thetaFrom += 2*PI;
        }
        if(thetaTo < thetaFrom && (thetaFrom - thetaTo) > PI) {
            thetaTo += 2*PI;
        }

        if(fabs(thetaTo - thetaFrom)*180/PI < 3) {
            // Nearly parallel; no one will want this, so don't try.
            from = now;
            WritePt(&pts, now.x, now.y);
            continue;
        }

        if(thetaFrom > thetaTo) {
            // Outside corner, rounding off decreases the area enclosed.
            double theta = thetaTo - thetaFrom;
            theta = PI - theta;
            // Length along each edge to the point where the arc starts.
            double le = radius/tan(theta/2);

            // First point on the arc
            double dx, dy, m;
            dx = (from.x - now.x); dy = (from.y - now.y);
            m = Magnitude(dx, dy);
            dx /= m; dy /= m;
            double xA = now.x - le*dx;
            double yA = now.y - le*dy;

            // Center of the arc
            double xc = (now.x - le*dx) - radius*dy; 
            double yc = (now.y - le*dy) + radius*dx; 

            // Second point on the arc
            dx = (to.x - now.x); dy = (to.y - now.y);
            m = Magnitude(dx, dy);
            dx /= m; dy /= m;
            double xB = now.x - le*dx;
            double yB = now.y - le*dy;

            if(i == (src->pts - 1)) {
                WritePt(&pts, xA, yA);
            } else {
                double thetaA = atan2(yA - yc, xA - xc);
                double thetaB = atan2(yB - yc, xB - xc);
                while(thetaB > thetaA) thetaB -= 2*PI;
                int steps = toint((thetaA - thetaB)*(180/PI)/5);
                int j;
                for(j = 0; j <= steps; j++) {
                    double t = thetaA + ((thetaB - thetaA)*j)/steps;
                    WritePt(&pts, xc + radius*cos(t), yc + radius*sin(t));
                }
            }
        } else {
            // Inside corner, rounding off increases the area enclosed.
            double theta = thetaTo - thetaFrom;
            theta = PI - theta;
            // Length along each edge to the point where the arc starts.
            double le = radius/tan(theta/2);

            // First point on the arc
            double dx, dy, m;
            dx = (from.x - now.x); dy = (from.y - now.y);
            m = Magnitude(dx, dy);
            dx /= m; dy /= m;
            double xA = now.x + le*dx;
            double yA = now.y + le*dy;

            // Center of the arc
            double xc = (now.x + le*dx) + radius*dy; 
            double yc = (now.y + le*dy) - radius*dx; 

            // Second point on the arc
            dx = (to.x - now.x); dy = (to.y - now.y);
            m = Magnitude(dx, dy);
            dx /= m; dy /= m;
            double xB = now.x + le*dx;
            double yB = now.y + le*dy;
            
            if(i == (src->pts - 1)) {
                WritePt(&pts, xA, yA);
            } else {
                double thetaA = atan2(yA - yc, xA - xc);
                double thetaB = atan2(yB - yc, xB - xc);
                while(thetaB < thetaA) thetaB += 2*PI;
                int steps = toint((thetaB - thetaA)*(180/PI)/5);
                int j;
                for(j = 0; j <= steps; j++) {
                    double t = thetaA + ((thetaB - thetaA)*j)/steps;
                    WritePt(&pts, xc + radius*cos(t), yc + radius*sin(t));
                }
            }
        }

        from = now;
    }

    dest->pt = AllocPointsForClosedCurve(pts);
    dest->pts = pts;
}

BOOL PolygonRoundCorners(DPolygon *dest, DPolygon *src, double radius, 
                                                        hPoint *pt, int pts)
{
    int i;
    for(i = 0; i < src->curves; i++) {
        RoundCornersForClosedCurve(&(dest->curve[i]), &(src->curve[i]),
            radius, pt, pts);
    }
    dest->curves = src->curves;
    return TRUE;
}

//-----------------------------------------------------------------------------
// Break a polygon down into piecewise linear segments, and then break those
// lines apart so that they appear to be dashed. This is useful for perforated
// lines, e.g. on a cardboard part that is meant to be folded.
//-----------------------------------------------------------------------------
BOOL PolygonPerforate(DPolygon *dest, DPolygon *src, double dash, double duty)
{
    // Knock the polygon down into piecewise linear segments.
    AllCnt = 0;
    PwlsFromPolygon(src, 0);

    int i, pts, curves;
    curves = 0;
    for(i = 0; i < AllCnt; i++) {
        SketchPwl *pwl = &(AllBuf[i]);
        double len = Distance(pwl->x0, pwl->y0, pwl->x1, pwl->y1);
        int n = (int)ceil(len / dash);
        // We want the endpoints of the line to both have the same state,
        // so we must choose an odd number of segments.
        if(n % 2 == 0) n++;
        n /= 2;

        double dx = pwl->x1 - pwl->x0;
        double dy = pwl->y1 - pwl->y0;

        // So the line consists of n + 1 "on" segments, and n "off"
        // segments. For some basic length l, the "on" segments have
        // length D*l, and the "off" segments (1 - D)*l. So the total
        // length is
        //   (n + 1)*D*l + n*(1 - D)*l
        // = (n*D + D + n - n*D)*l
        // = (D + n)*l (makes sense, n pairs, plus one extra)

        int j;
        // We're rounding up, so <= n; that gives the extra "on" at the end.
        for(j = 0; j <= n; j++) {
            double ti;
            double tf;
    
            double ad = fabs(duty);
            if(duty > 0) {
                // yes no yes; this hits the original endpoints
                ti = ((double)j) / (ad + n);
                tf = ((double)j + ad) / (ad + n);
            } else {
                // no yes no; this doesn't

                // There's one fewer "off" than on
                if(j == n) break;
                
                // Probably more intuitive to flip the sense of the duty
                // cycle, since we're generating lines for the "off".
                ad = 1 - ad;
                ti = ((double)j + ad) / (ad + n);
                tf = ((double)j + 1) / (ad + n);
            }

            pts = 0;
            WritePt(&pts, pwl->x0 + ti*dx, pwl->y0 + ti*dy);
            WritePt(&pts, pwl->x0 + tf*dx, pwl->y0 + tf*dy);
            if(curves < MAX_CLOSED_CURVES_IN_POLYGON) {
                dest->curve[curves].pts = pts;
                dest->curve[curves].pt = AllocPointsForClosedCurve(pts);
                curves++;
            }
        }
    }

    dest->curves = curves;
    return TRUE;
}
