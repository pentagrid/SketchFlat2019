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
// Routines to work with our parametric curves. When we get the curves, they
// already have constant numerical coefficients.
// Jonathan Westhues, April 2007
//-----------------------------------------------------------------------------
#include "sketchflat.h"

#define CHORD_TOLERANCE_IN_PIXELS 0.25

static DoublePoint TransX, TransY, TransOffset;

static DoublePoint ImportMin, ImportMax;

static void CurveEval(SketchCurve *c, double t, double *xp, double *yp);

static void AddPwl(hEntity id, hLayer layer, BOOL construction,
                                double x0, double y0, double x1, double y1);

static void Zero(SketchCurve *c)
{
    c->x.A = 0;
    c->x.B = 0;
    c->x.C = 0;
    c->x.D = 0;
    c->x.R = 0;
    c->x.Rl = 0;
    c->x.phi = 0;

    c->y.A = 0;
    c->y.B = 0;
    c->y.C = 0;
    c->y.D = 0;
    c->y.R = 0;
    c->y.Rl = 0;
    c->y.phi = 0;

    c->omega = 0;
}

static void AddCurve(SketchCurve *c)
{
    SketchEntity *e = EntityById(c->id);
    // The curve is on whatever layer the entity that generates it is on.
    c->layer = e->layer;
    // and has the same construction flag.
    c->construction = e->construction;

    int i = SK->curves;
    if(i < (MAX_CURVES_IN_SKETCH-1)) {
        memcpy(&(SK->curve[i]), c, sizeof(*c));
        SK->curves = i + 1;
    }
}

static void AddBezierCubic(hEntity he, double *x, double *y)
{
    // The cubic has the form 
    //   P[0]*(1-t)^3 + 3*P[1]*t*(1-t)^2 + 3*P[2]*t^2*(1-t) + P[3]*t^3
    // or, expanding,
    //   (-P[0]+3*P[1]-3*P[2]+P[3])     * t^3 + 
    //   (3*P[0]+3*P[2]-6*P[1])         * t^2 + 
    //   (-3*P[0]+3*P[1])               * t   + 
    //   P[0]

    SketchCurve c;
    Zero(&c);
    c.id = he;

    c.x.A =   -x[0] + 3*x[1] - 3*x[2] + x[3];
    c.y.A =   -y[0] + 3*y[1] - 3*y[2] + y[3];

    c.x.B =  3*x[0] - 6*x[1] + 3*x[2];
    c.y.B =  3*y[0] - 6*y[1] + 3*y[2];

    c.x.C = -3*x[0] + 3*x[1];
    c.y.C = -3*y[0] + 3*y[1];
    
    c.x.D =    x[0];
    c.y.D =    y[0];

    AddCurve(&c);
}

static void FromTtfTransform(int xi, int yi, double *xo, double *yo)
{
    double xf, yf;

    xf = (xi / 1024.0);
    yf = (yi / 1024.0);

    // output = xf*TransX + yf*TransY
    *xo = xf*(TransX.x) + yf*(TransY.x) + TransOffset.x;
    *yo = xf*(TransX.y) + yf*(TransY.y) + TransOffset.y;
}

static void FromImportedTransform(double *x, double *y)
{
    double xi = *x, yi = *y;

    xi -= ImportMin.x;
    yi -= ImportMin.y;

    double scale = (ImportMax.y - ImportMin.y);
    if(scale == 0) scale = 1;
    xi /= scale;
    yi /= scale;

    *x = (xi*TransX.x) + (yi*TransY.x) + TransOffset.x;
    *y = (xi*TransX.y) + (yi*TransY.y) + TransOffset.y;
}

void TtfLineSegment(DWORD ref, int x0, int y0, int x1, int y1)
{
    double x0f, y0f, x1f, y1f;

    FromTtfTransform(x0, y0, &x0f, &y0f);
    FromTtfTransform(x1, y1, &x1f, &y1f);

    SketchCurve c;
    Zero(&c);

    c.x.D = x0f;
    c.y.D = y0f;
    c.x.C = x1f - x0f;
    c.y.C = y1f - y0f;

    c.id = ref;

    AddCurve(&c);
}

void TtfBezier(DWORD ref, int x0, int y0, int x1, int y1, int x2, int y2)
{
    double x0f, y0f, x1f, y1f, x2f, y2f;

    FromTtfTransform(x0, y0, &x0f, &y0f);
    FromTtfTransform(x1, y1, &x1f, &y1f);
    FromTtfTransform(x2, y2, &x2f, &y2f);

    SketchCurve c;
    Zero(&c);
    
    // The Bezier curve has the form:
    //
    //     P(t) = P0*(1 - t)^2 + P1*2*t*(1 - t) + P2*t^2
    //          = P0 - P0*2*t + P0*t^2 + P1*2*t - P1*2*t^2 + P2*t^2
    //          = P0 + t*(-P0*2 + P1*2) + (t^2)*(P0 - P1*2 + P2)
    //
    // Of course it would be better to evaluate with de Casteljau's or
    // whatever, but this is more general.

    c.x.D = x0f;
    c.y.D = y0f;
    c.x.C = (-x0f*2 + x1f*2);
    c.y.C = (-y0f*2 + y1f*2);
    c.x.B = (x0f - x1f*2 + x2f);
    c.y.B = (y0f - y1f*2 + y2f);

    c.id = ref;

    AddCurve(&c);
}

//-----------------------------------------------------------------------------
// For the HPGL/DXF import: we want to scale the artwork as we import it,
// to put the two reference points at the corners of its bounding box. So
// we need to calculate the bounding box of the file, which we do on our
// first pass through.
//-----------------------------------------------------------------------------
static void RecordBounds(double x0, double y0, double x1, double y1)
{
    int i;
    for(i = 0; i < 2; i++) {
        double x, y;
        if(i == 0) {
            x = x0; y = y0;
        } else {
            x = x1; y = y1;
        }

        if(x > ImportMax.x) ImportMax.x = x;
        if(x < ImportMin.x) ImportMin.x = x;
        if(y > ImportMax.y) ImportMax.y = y;
        if(y < ImportMin.y) ImportMin.y = y;
    }
}

//-----------------------------------------------------------------------------
// Import an HPGL file. If justGetBound is TRUE, then we don't generate pwls,
// but call RecordBounds() to measure the bounding box.
//-----------------------------------------------------------------------------
static BOOL ImportFromHpgl(hEntity he, hLayer hl, char *file, BOOL justGetBound)
{
    FILE *f = fopen(file, "r");
    if(!f) return FALSE;

    double prevX = 0, prevY = 0;

#define GET_CHAR_INTO(c) (c) = fgetc(f); if((c) < 0) goto done
    for(;;) {
        int now, prev = -1;

        // First, look for a command.
        for(;;) {
            GET_CHAR_INTO(now);
            now = tolower(now);
            if(prev == 'p' && (now == 'd' || now == 'u')) break;
            prev = now;
        }
        // Is it followed by a number?
        char xbuf[100];
        int xbufp = 0;
        for(;;) {
            int c;
            GET_CHAR_INTO(c);
            if(isdigit(c) || c == '.' || c == '-') {
                if(xbufp > 10) break;
                xbuf[xbufp++] = c;
            } else {
                break;
            }
        }
        // Burn extra separators
        for(;;) {
            int c;
            GET_CHAR_INTO(c);
            if(c == ',' || c == ' ') {
                // do nothing
            } else {
                ungetc(c, f);
                break;
            }
        }
        // And then get the y
        char ybuf[100];
        int ybufp = 0;
        for(;;) {
            int c;
            GET_CHAR_INTO(c);
            if(isdigit(c) || c == '.' || c == '-') {
                if(ybufp > 10) break;
                ybuf[ybufp++] = c;
            } else {
                break;
            }
        }

        double x, y;
        xbuf[xbufp] = '\0';
        x = atof(xbuf);
        ybuf[ybufp] = '\0';
        y = atof(ybuf);

        if(justGetBound) {
            RecordBounds(prevX, prevY, x, y);
        } else {
            FromImportedTransform(&x, &y);
            if(now == 'd') {
                AddPwl(he, hl, FALSE, prevX, prevY, x, y);
            }
        } 
        prevX = x;
        prevY = y;
    }

done:
    fclose(f);
    return TRUE;
}

//-----------------------------------------------------------------------------
// Import a DXF file. If justGetBound is TRUE, then we don't generate pwls,
// but call RecordBounds() to measure the bounding box.
//-----------------------------------------------------------------------------
static BOOL ImportFromDxf(hEntity he, hLayer hl, char *file, BOOL justGetBound)
{
    FILE *f = fopen(file, "r");
    if(!f) return FALSE;

    char line[MAX_STRING];

#define GET_LINE_INTO(s) if(!fgets(s, sizeof(s), f)) goto done
    for(;;) {
        GET_LINE_INTO(line);
        
        char *s = line;
        while(isspace(*s)) s++;
        while(isspace(s[strlen(s)-1])) s[strlen(s)-1] = '\0';

        if(strcmp(s, "LINE")==0) {
            char x0[MAX_STRING] = "", y0[MAX_STRING] = "";
            char x1[MAX_STRING] = "", y1[MAX_STRING] = "";
            BYTE have = 0;
            
            for(;;) {
                GET_LINE_INTO(line);
                switch(atoi(line)) {
                    case 10:    GET_LINE_INTO(x0); have |= 1; break;
                    case 20:    GET_LINE_INTO(y0); have |= 2; break;
                    case 11:    GET_LINE_INTO(x1); have |= 4; break;
                    case 21:    GET_LINE_INTO(y1); have |= 8; break;
                    case 0:
                        goto break_loop;

                    default:    GET_LINE_INTO(line); break;
                }

                // Do we have all four paramter values?
                if(have == 0xf) break;
            }

            double x0f, y0f, x1f, y1f;
            x0f = atof(x0);
            y0f = atof(y0);
            x1f = atof(x1);
            y1f = atof(y1);

            if(justGetBound) {
                RecordBounds(x0f, y0f, x1f, y1f);
            } else {
                FromImportedTransform(&x0f, &y0f);
                FromImportedTransform(&x1f, &y1f);
                AddPwl(he, hl, FALSE, x0f, y0f, x1f, y1f);
            }
        }
break_loop:;
    }

done:
    fclose(f);
    return TRUE;
}

//-----------------------------------------------------------------------------
// Import some type of file, determining which according to its extension. If
// the file import fails, then draw an X, as an indication to the user that
// something broke. Also record the extent of the imported file, to display
// in the measurement view, since the user might want to use that to scale
// back to 1:1.
//-----------------------------------------------------------------------------
static BOOL ImportFromFile(hEntity he, hLayer hl, char *file)
{
    char *ext = file + strlen(file) - 4;

    ImportMax.x = VERY_NEGATIVE;
    ImportMax.y = VERY_NEGATIVE;
    ImportMin.x = VERY_POSITIVE;
    ImportMin.y = VERY_POSITIVE;

    int SKpwls0 = SK->pwls;

    // Guess the file type from the extension.
    if(stricmp(ext, ".plt")==0 || stricmp(ext, "hpgl")==0) {
        ImportFromHpgl(he, hl, file, TRUE);
        ImportFromHpgl(he, hl, file, FALSE);
    } else if(stricmp(ext, ".dxf")==0) {
        ImportFromDxf(he, hl, file, TRUE);
        ImportFromDxf(he, hl, file, FALSE);
    }

    // If we didn't generate any piecewise linear segments, then probably
    // something broke. Show an X in construction line segments, so that the
    // user still has something to grab and select.
    if(SKpwls0 == SK->pwls) {
        double x0, y0, x1, y1;
        ImportMax.x = 1; ImportMax.y = 1;
        ImportMin.x = 0; ImportMin.y = 0;
        x0 = 0; y0 = 0; x1 = 1; y1 = 1;
        FromImportedTransform(&x0, &y0);
        FromImportedTransform(&x1, &y1);
        AddPwl(he, hl, TRUE, x0, y0, x1, y1);
        x0 = 1; y0 = 0; x1 = 0; y1 = 1;
        FromImportedTransform(&x0, &y0);
        FromImportedTransform(&x1, &y1);
        AddPwl(he, hl, TRUE, x0, y0, x1, y1);
    }

    // Record the original bounding box of the imported file; the measure
    // stuff will display it from here.
    SketchEntity *e = EntityById(he);
    if(e) {
        sprintf(e->text, "    (%.3f, %.3f)\r\n"
                         "    (%.3f, %.3f)\r\n"
                         "  so dy = %.3f",
            ImportMin.x, ImportMin.y,
            ImportMax.x, ImportMax.y,
            ImportMax.y - ImportMin.y);
    }

    return TRUE;
}

static void GenerateCurvesFromEntity(SketchEntity *e)
{
    SketchCurve c;
    hPoint pt0, pt1;
    hParam prm0;

    Zero(&c);
    switch(e->type) {
        case ENTITY_DATUM_POINT:
            // No curves associated with this entity.
            break;

        case ENTITY_DATUM_LINE:
            // No curves associated with this entity (infinitely long lines
            // are a special case, distinct from line segments).
            break;

        case ENTITY_LINE_SEGMENT:
            pt0 = POINT_FOR_ENTITY(e->id, 0);
            pt1 = POINT_FOR_ENTITY(e->id, 1);

            Zero(&c);
            c.x.D = EvalParam(X_COORD_FOR_PT(pt0));
            c.y.D = EvalParam(Y_COORD_FOR_PT(pt0));
            c.x.C = EvalParam(X_COORD_FOR_PT(pt1)) - c.x.D;
            c.y.C = EvalParam(Y_COORD_FOR_PT(pt1)) - c.y.D;
            c.id = e->id;

            AddCurve(&c);
            break;

        case ENTITY_CIRCLE:
            pt0 = POINT_FOR_ENTITY(e->id, 0);
            prm0 = PARAM_FOR_ENTITY(e->id, 0);

            Zero(&c);
            EvalPoint(pt0, &(c.x.D), &(c.y.D));
            c.x.R = c.y.R = EvalParam(prm0);
            c.x.phi = 0;
            c.y.phi = PI/2;
            c.omega = 2*PI;
            c.id = e->id;

            AddCurve(&c);
            break;

        case ENTITY_CIRCULAR_ARC: {
            double x0, y0, x1, y1;
            EvalPoint(POINT_FOR_ENTITY(e->id, 0), &x0, &y0);
            EvalPoint(POINT_FOR_ENTITY(e->id, 1), &x1, &y1);
            double xc, yc;
            EvalPoint(POINT_FOR_ENTITY(e->id, 2), &xc, &yc);

            double r0 = Distance(xc, yc, x0, y0);
            double r1 = Distance(xc, yc, x1, y1);

            double phi0 = atan2(y0 - yc, x0 - xc);
            double phi1 = atan2(y1 - yc, x1 - xc);

            double dphi = phi0 - phi1;
            while(dphi < 0) dphi += 2*PI;
            while(dphi >= 2*PI) dphi -= 2*PI;
            
            Zero(&c);
            c.x.D = xc;
            c.y.D = yc;
            c.x.R = r0;
            c.x.Rl = r1 - r0;
            c.y.R = -r0;
            c.y.Rl = -(r1 - r0);
            c.x.phi = 0 + phi0;
            c.y.phi = PI/2 + phi0;
            c.omega = -dphi;
            c.id = e->id;

            AddCurve(&c);
            break;
        }

        case ENTITY_CUBIC_SPLINE: {
            double x[4], y[4];
            double xnp, ynp;    // previous oN curve point
            double xfp, yfp;    // previous oFf curve point

            int i;

            int pt = 0;
            int max = (e->points - 2) / 2;
            for(i = 0; i < max; i++) {
                if(i == 0) {
                    EvalPoint(POINT_FOR_ENTITY(e->id, pt), &(x[0]), &(y[0]));
                    pt++;
                    EvalPoint(POINT_FOR_ENTITY(e->id, pt), &(x[1]), &(y[1]));
                    pt++;
                } else {
                    x[0] = xnp;
                    y[0] = ynp;
                    x[1] = xfp;
                    y[1] = yfp;
                }

                if(i == (max - 1)) {
                    EvalPoint(POINT_FOR_ENTITY(e->id, pt), &(x[2]), &(y[2]));
                    pt++;
                    EvalPoint(POINT_FOR_ENTITY(e->id, pt), &(x[3]), &(y[3]));
                    pt++;
                } else {
                    EvalPoint(POINT_FOR_ENTITY(e->id, pt), &(x[2]), &(y[2]));
                    pt++;
                    EvalPoint(POINT_FOR_ENTITY(e->id, pt), &xfp, &yfp);
                    pt++;
                    xnp = x[3] = (xfp + x[2]) / 2;
                    ynp = y[3] = (yfp + y[2]) / 2;
                }

                AddBezierCubic(e->id, x, y);
            }

            break;
        }

        case ENTITY_TTF_TEXT:
        case ENTITY_IMPORTED: {
            double x0, y0, x1, y1;
            EvalPoint(POINT_FOR_ENTITY(e->id, 0), &x0, &y0);
            EvalPoint(POINT_FOR_ENTITY(e->id, 1), &x1, &y1);

            TransY.x = x1 - x0;
            TransY.y = y1 - y0;

            TransX.x =  TransY.y;
            TransX.y = -TransY.x;

            TransOffset.x = x0;
            TransOffset.y = y0;

            if(e->type == ENTITY_TTF_TEXT) {
                TtfSelectFont(e->file);
                TtfPlotString(e->id, e->text, e->spacing);
            } else {
                ImportFromFile(e->id, e->layer, e->file);
            }
            break;
        }

        default:
            oopsnf();
            break;
    }
}
static void GenerateCurves(void)
{
    SK->curves = 0;

    int i;
    for(i = 0; i < SK->entities; i++) {
        GenerateCurvesFromEntity(&(SK->entity[i]));
    }
}

static void CurveEval(SketchCurve *c, double t, double *xp, double *yp)
{
    double x, y;

    x  = c->x.A; x *= t;
    x += c->x.B; x *= t;
    x += c->x.C; x *= t;
    x += c->x.D;
    x += (c->x.R + (c->x.Rl)*t) * cos((c->omega)*t + c->x.phi);

    y  = c->y.A; y *= t;
    y += c->y.B; y *= t;
    y += c->y.C; y *= t;
    y += c->y.D;
    y += (c->y.R + (c->y.Rl)*t) * cos((c->omega)*t + c->y.phi);

    *xp = x;
    *yp = y;
}

static void AddPwl(hEntity id, hLayer layer, BOOL construction,
                                double x0, double y0, double x1, double y1)
{
    int i = SK->pwls;
    
    if(i >= (MAX_PWLS_IN_SKETCH-1)) return;

    SketchPwl *p = &(SK->pwl[i]);

    p->id = id;
    p->layer = layer;
    p->construction = construction;

    p->x0 = x0;
    p->y0 = y0;
    p->x1 = x1;
    p->y1 = y1;

    SK->pwls = i + 1;
}

//-----------------------------------------------------------------------------
// Break a curve down in to its piecewise linear representation. The number
// of points is determined by a "chord tolerance". We initially try to
// generate a single line for the entire curve, but halve the remaining
// interval each time we fail.
//-----------------------------------------------------------------------------
static void GeneratePwlsFromCurve(SketchCurve *c, double chordTol)
{
    int pts = 0;
    int iters = 0;

    double from = 0;
    double finalTo = 1;

    double tryTo = finalTo;

    int pwls0 = SK->pwls;

    while(from < (finalTo - 0.001)) {
        double xi, yi;      // Starting point of the line we are considering
        double xf, yf;      // Ending point of the line we are considering
        double xm, ym;      // A point on the curve midway between start, end
        double xml, yml;    // The midpoint of the line we are considering
        
        if(c->x.A != 0 || c->y.A != 0) {
            // A cubic might pass through the midpoint of the line connecting 
            // its endpoints, but deviate from that line elsewhere.
            if(tryTo - from > 0.1) {
                tryTo = min(finalTo, from + 0.1);
            }
        }

        CurveEval(c, from, &xi, &yi);
        CurveEval(c, tryTo, &xf, &yf);
        CurveEval(c, (from + tryTo)/2, &xm, &ym);
/*
        dbp("from (%.3f, %.3f) at %.3f to (%.3f, %.3f) at %.3f",
            xi, yi, from,
            xf, yf, tryTo); */

        xml = (xi + xf)/2;
        yml = (yi + yf)/2;

        if(Distance(xm, ym, xml, yml) < chordTol) {
            // Looks good
            AddPwl(c->id, c->layer, c->construction, xi, yi, xf, yf);
            from = tryTo;
            tryTo = finalTo;
            pts++;
        } else {
            tryTo = from + (tryTo - from)/2;
            // And try again
        }

        iters++;
        if(pts > 200 || iters > 1000) {
            // If we get too many points trying to plot the thing cleverly
            // and adaptively, then give up and just generate 200 evenly
            // spaced points.
            SK->pwls = pwls0;
            double t;
            CurveEval(c, 0, &xi, &yi);
            double steps = 200;
            double dt = 1.0/steps;
            for(t = dt; t < 1 + dt; t += dt) {
                CurveEval(c, t, &xf, &yf);
                AddPwl(c->id, c->layer, c->construction, xi, yi, xf, yf);
                xi = xf;
                yi = yf;
            }
            break;
        }
    }
}
void GenerateCurvesAndPwls(double chordTol)
{
    SK->pwls = 0;

    // First, create the various curves.
    GenerateCurves();

    // Then break them down to piecewise linear segments. The chord
    // tolerance with which we do this is caller-configurable.

    if(chordTol < 0) {
        // They want our default display tolerance.
        chordTol = 
            toMicronsNotAffine((int)(CHORD_TOLERANCE_IN_PIXELS*100))/100.0;
    }
   
    // And adaptive-pwl each curve.
    int i;
    for(i = 0; i < SK->curves; i++) {
        GeneratePwlsFromCurve(&(SK->curve[i]), chordTol);
    }
}
