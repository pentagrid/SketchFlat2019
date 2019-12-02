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
// As the user selects points, entities, and lines, we will display meaningful
// measurements on the selection automatically. For example, if the user
// selects two points, then we will display the distance between them. This
// file contains routines that look at the selection, and generate an
// English language description of the measurements.
//
// Jonathan Westhues, May 2007
//-----------------------------------------------------------------------------
#include "sketchflat.h"

static void DescribeConstraint(char *desc, SketchConstraint *c)
{
    switch(c->type) {
        case CONSTRAINT_PT_PT_DISTANCE:
            sprintf(desc, "point-point distance:\r\n"
                          "  d = %s\r\n",
                ToDisplay(c->v));
            break;

        case CONSTRAINT_POINTS_COINCIDENT:
            sprintf(desc, "points concident:\r\n");
            break;

        case CONSTRAINT_PT_LINE_DISTANCE:
            sprintf(desc, "point-line distance:\r\n"
                          "  d = %s\r\n",
                ToDisplay(c->v));
            break;

        case CONSTRAINT_LINE_LINE_DISTANCE:
            sprintf(desc, "line-line distance:\r\n"
                          "  d = %s\r\n",
                ToDisplay(c->v));
            break;

        case CONSTRAINT_POINT_ON_LINE:
            sprintf(desc, "point on line\r\n"
                          "  (or extension of line\r\n"
                          "   segment)\r\n");
            break;

        case CONSTRAINT_PARALLEL:
            sprintf(desc, "parallel\r\n  (or parallel tangent)\r\n");
            break;

        case CONSTRAINT_RADIUS:
            sprintf(desc, "diameter\r\n");
            break;

        case CONSTRAINT_LINE_LINE_ANGLE:
            sprintf(desc, "angle\r\n");
            break;

        case CONSTRAINT_AT_MIDPOINT:
            sprintf(desc, "at midpoint of line\r\n"
                           "  segment\r\n");
            break;

        case CONSTRAINT_SYMMETRIC:
            sprintf(desc, "symmetric\r\n");
            break;

        case CONSTRAINT_EQUAL_LENGTH:
            sprintf(desc, "equal length\r\n");
            break;

        case CONSTRAINT_EQUAL_RADIUS:
            sprintf(desc, "equal radius\r\n");
            break;

        case CONSTRAINT_ON_CIRCLE:
            sprintf(desc, "point on circle\r\n");
            break;

        case CONSTRAINT_PERPENDICULAR:
            sprintf(desc, "perpendicular\r\n");
            break;

        case CONSTRAINT_HORIZONTAL:
            sprintf(desc, "horizontal\r\n");
            break;

        case CONSTRAINT_VERTICAL:
            sprintf(desc, "vertical\r\n");
            break;

        case CONSTRAINT_FORCE_PARAM:
            sprintf(desc, "draggable horiz/vert\r\n");
            break;

        case CONSTRAINT_FORCE_ANGLE:
            sprintf(desc, "draggable about point\r\n");
            break;

        case CONSTRAINT_SCALE_MM:
        case CONSTRAINT_SCALE_INCH: {
            const char *unit = (c->type == CONSTRAINT_SCALE_MM) ? " mm" : "\"";
            sprintf(desc, "scale imported file\r\n"
                          "  by factor\r\n"
                          "    %.4g%s : 1 unit\r\n"
                          "  is equivalent to\r\n"
                          "    1%s : %.4g unit\r\n"
                          "  ('unit' is what appears\r\n"
                          "   in import source file)\r\n",
                c->v, unit,
                unit, 1/(c->v));
            break;
        }

        default:
            strcpy(desc, "?? constraint");
            break;
    }
}

static void DescribeTwoPoints(char *desc, 
                        double x0, double y0, double x1, double y1, const char *w)
{
    double dx = x1 - x0;
    double dy = y1 - y0;

    sprintf(desc, 
                  "   (%s, %s) and\r\n"
                  "   (%s, %s)\r\n"
                  "  %s = %s\r\n"
                  "  direction\r\n"
                  "     (%s, %s)\r\n"
                  "  angle = %.2f°\r\n",
        ToDisplay(x0), ToDisplay(y0),
        ToDisplay(x1), ToDisplay(y1),
        w, ToDisplay(Distance(x0, y0, x1, y1)),
        ToDisplay(dx), ToDisplay(dy),
        atan2(dy, dx)*180/PI);
}

static void DescribeEntity(char *desc, SketchEntity *e)
{
    switch(e->type) {
        case ENTITY_LINE_SEGMENT:  {
            strcpy(desc, "line segment:\r\n  endpoints\r\n");
            double x0, y0, x1, y1;
            EvalPoint(POINT_FOR_ENTITY(e->id, 0), &x0, &y0);
            EvalPoint(POINT_FOR_ENTITY(e->id, 1), &x1, &y1);

            DescribeTwoPoints(desc+strlen(desc), x0, y0, x1, y1, "length");
            break;
        }
        case ENTITY_CIRCLE: {
            double xc, yc;
            double r;
            EvalPoint(POINT_FOR_ENTITY(e->id, 0), &xc, &yc);
            r = EvalParam(PARAM_FOR_ENTITY(e->id, 0));
            sprintf(desc, "circle:\r\n"
                          "  center is\r\n"
                          "     (%s, %s)\r\n"
                          "  diameter = %s\r\n"
                          "  radius = %s\r\n",
                ToDisplay(xc), ToDisplay(yc),
                ToDisplay(r*2), ToDisplay(r));
            break;
        }
        case ENTITY_CIRCULAR_ARC: {
            double x0, y0, x1, y1;
            EvalPoint(POINT_FOR_ENTITY(e->id, 0), &x0, &y0);
            EvalPoint(POINT_FOR_ENTITY(e->id, 1), &x1, &y1);
            double xc, yc;
            EvalPoint(POINT_FOR_ENTITY(e->id, 2), &xc, &yc);

            double theta0 = atan2(y0 - yc, x0 - xc);
            double theta1 = atan2(y1 - yc, x1 - xc);
            double dtheta = theta0 - theta1;
            while(dtheta < -PI) dtheta += 2*PI;
            while(dtheta >  PI) dtheta -= 2*PI;

            sprintf(desc, "arc of a circle:\r\n"
                          "  center is\r\n"
                          "    (%s, %s)\r\n"
                          "  points\r\n"
                          "    (%s, %s)\r\n"
                          "    (%s, %s)\r\n"
                          "  radius = %s or %s\r\n"
                          "  angle = %.2f°\r\n",
                ToDisplay(xc), ToDisplay(yc),
                ToDisplay(x0), ToDisplay(y0),
                ToDisplay(x1), ToDisplay(y1),
                ToDisplay(Distance(xc, yc, x0, y0)),
                ToDisplay(Distance(xc, yc, x1, y1)),
                dtheta*180/PI);
            break;
        }
        case ENTITY_TTF_TEXT: {
            double x0, y0, x1, y1;
            EvalPoint(POINT_FOR_ENTITY(e->id, 0), &x0, &y0);
            EvalPoint(POINT_FOR_ENTITY(e->id, 1), &x1, &y1);

            char buf[MAX_STRING];
            strcpy(buf, e->file);
            char *path = buf;
            char *file = strrchr(buf, '\\');
            if(file) {
                *file = '\0';
                file++;
            } else {
                file = (char*)"";
            }

            sprintf(desc, "TTF text:\r\n"
                          "  use font file\r\n"
                          "%s\\\r\n"
                          "%s\r\n\r\n"
                          "  points\r\n"
                          "    (%s, %s)\r\n"
                          "    (%s, %s)\r\n",
                path, file,
                ToDisplay(x0), ToDisplay(y0),
                ToDisplay(x1), ToDisplay(y1));
            break;
        }
        case ENTITY_IMPORTED:
            sprintf(desc, "imported:\r\n"
                          "  original extent\r\n"
                          "%s\r\n"
                          "  file\r\n"
                          "%s",
                e->text, strlen(e->file) ? e->file : "(no file)");
            break;

        case ENTITY_CUBIC_SPLINE:
            sprintf(desc, "Cubic Spline:\r\n"
                          "  %d segment%s\r\n",
                (e->points - 2)/2, e->points == 4 ? "" : "s");
            break;

        default:
            sprintf(desc, "entity");
            break;
    }
}

void UpdateMeasurements(void)
{
    char desc[MAX_STRING] = "";

    // Constraints handled separately, if they appear in the selection list.
    int i;
    for(i = 0; i < arraylen(Selected); i++) {
        if(Selected[i].which == SEL_CONSTRAINT) {
            DescribeConstraint(desc, ConstraintById(Selected[i].constraint));
            uiSetMeasurementAreaText(desc);
            return;
        }
    }

    // Lines, entities, and points handled here.
    GroupedSelection gs;
    GroupSelection(&gs);

    if(gs.lines == 0 && gs.points == 0 && gs.entities == 0) {
        strcpy(desc, "nothing selected");
    } else if(gs.lines == 1 && gs.points == 0 && gs.entities == 0) {
        double dx, dy;
        double theta;
        double x0, y0;

        LineToParametric(gs.line[0], &x0, &y0, &dx, &dy);
        theta = atan2(dy, dx) * 180 / PI;

        sprintf(desc, "datum line: %s\r\n"
                      "  direction is\r\n"
                      "     (%.3f, %.3f)\r\n"
                      "  angle with x-axis\r\n"
                      "     %.2f°\r\n"
                      "  through pt\r\n"
                      "     (%s, %s)\r\n",
            ENTITY_FROM_LINE(gs.line[0]) == REFERENCE_ENTITY ? "(ref)" : "",
            dx, dy, theta, ToDisplay(x0), ToDisplay(y0));
    } else if(gs.lines == 0 && gs.points == 1 && gs.entities == 0) {
        double x, y;
        EvalPoint(gs.point[0], &x, &y);

        sprintf(desc, "point: %s\r\n"
                      "  at (%s, %s)\r\n"
                      "  id is %08x\r\n",
            ENTITY_FROM_POINT(gs.point[0]) == REFERENCE_ENTITY ? "(ref)" : "",
            ToDisplay(x), ToDisplay(y), gs.point[0]);

        hEntity he = ENTITY_FROM_POINT(gs.point[0]);
        if(he == REFERENCE_ENTITY) {
            sprintf(desc+strlen(desc), "  the origin");
        } else {
            SketchEntity *e = EntityById(he);
            if(e->type == ENTITY_CUBIC_SPLINE) {
                int segs = (e->points - 2) / 2;
                sprintf(desc+strlen(desc), "  for cubic spline\r\n"
                                           "    with %d segment%s\r\n",
                                    segs,
                                    segs == 1 ? "" : "s");
                int k = K_FROM_POINT(gs.point[0]);
                if(k == 0) {
                    strcpy(desc+strlen(desc), "  first point on curve\r\n");
                } else if(k == e->points - 1) {
                    strcpy(desc+strlen(desc), "  last point on curve\r\n");
                } else {
                    sprintf(desc+strlen(desc), "  control point for seg %d\r\n",
                        (k - 1) / 2);
                }
            }
        }
    } else if(gs.lines == 0 && gs.points == 2 && gs.entities == 0) {
        double x0, y0;
        double x1, y1;
        EvalPoint(gs.point[0], &x0, &y0);
        EvalPoint(gs.point[1], &x1, &y1);
        strcpy(desc, "two points:\r\n");
        DescribeTwoPoints(desc+strlen(desc), x0, y0, x1, y1, "distance");
    } else if(gs.lines == 0 && gs.points == 0 && gs.entities == 1) {
        DescribeEntity(desc, EntityById(gs.entity[0]));
    } else if(gs.anyLines == 1 && gs.points == 1 && gs.nonLineEntities == 0) {
        double x0, y0, dx, dy;
        if(gs.lines == 1) {
            LineOrLineSegment(gs.line[0], 0, &x0, &y0, &dx, &dy);
        } else {
            LineOrLineSegment(0, gs.entity[0], &x0, &y0, &dx, &dy);
        }

        double xp, yp;
        EvalPoint(gs.point[0], &xp, &yp);

        double d = DistanceFromPointToLine(xp, yp, x0, y0, dx, dy, FALSE);

        sprintf(desc, "point and line\r\n"
                      "  point at\r\n"
                      "     (%s, %s)\r\n"
                      "  line through\r\n"
                      "     (%s, %s)\r\n"
                      "  with direction\r\n"
                      "     (%s, %s)\r\n"
                      "  min distance = %s\r\n",
            ToDisplay(xp), ToDisplay(yp),
            ToDisplay(x0), ToDisplay(y0),
            ToDisplay(dx), ToDisplay(dy), ToDisplay(d));
    } else if(gs.n == 2 && gs.anyLines == 2) {
        double x0A, y0A, dxA, dyA;
        double x0B, y0B, dxB, dyB;

        if(gs.lines == 2) {
            LineOrLineSegment(gs.line[0], 0, &x0A, &y0A, &dxA, &dyA);
            LineOrLineSegment(gs.line[1], 0, &x0B, &y0B, &dxB, &dyB);
        } else if(gs.lines == 1) {
            LineOrLineSegment(gs.line[0], 0, &x0A, &y0A, &dxA, &dyA);
            LineOrLineSegment(0, gs.entity[0], &x0B, &y0B, &dxB, &dyB);
        } else {
            LineOrLineSegment(0, gs.entity[0], &x0A, &y0A, &dxA, &dyA);
            LineOrLineSegment(0, gs.entity[1], &x0B, &y0B, &dxB, &dyB);
        }
        double thetaA = atan2(dyA, dxA);
        double thetaB = atan2(dyB, dxB);
        double dtheta = (thetaA - thetaB)*180/PI;
        while(dtheta > 180) dtheta -= 360;
        while(dtheta < -180) dtheta += 360;

        double dthetap = dtheta;
        while(dthetap > 90) dthetap -= 180;
        while(dthetap < -90) dthetap += 180;

        double xi, yi;
        BOOL parallel = !IntersectionOfLines(x0A, y0A, dxA, dyA,
                                             x0B, y0B, dxB, dyB,
                                             &xi, &yi);

        sprintf(desc, "two lines\r\n"
                      "  angle mod 360° is\r\n"
                      "     %.2f°\r\n"
                      "  angle mod 180° is\r\n"
                      "     %.2f°\r\n",
            dtheta, dthetap);
        if(parallel) {
            sprintf(desc+strlen(desc), "  parallel\r\n");
        } else {
            sprintf(desc+strlen(desc), "  intersection at\r\n"
                                       "    (%s, %s)\r\n",
                                ToDisplay(xi), ToDisplay(yi));
        }
    } else {
        strcpy(desc, "items are selected");
    }

    uiSetMeasurementAreaText(desc);
}
