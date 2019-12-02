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

#ifndef __SKETCH_H
#define __SKETCH_H

// Generally useful definitions.

typedef struct {
    double  x;
    double  y;
} DoublePoint;

typedef struct {
    int     x;
    int     y;
} IntPoint;


//----------------------------
// Definitions for the geometry of the sketch.

// The entity table is the source of the other (curve, point, param)
// tables. The entity ID is a number between 1 and 1022. It has the
// following structure:
//
//          bits 31:10  -- all zero
//          bits  9: 0  -- entity ID
//
typedef DWORD       hEntity;
#define REFERENCE_ENTITY 1023

// The point and parameter tables are derived from the entity table. A
// point ID associated with a given entity has the following structure:
//
//          bits 31:26  -- all zero
//          bits 25:16  -- associated entity ID
//          bits 15: 0  -- index (multiple pts associated with entity)
//
typedef DWORD       hPoint;

// A line ID associated with a given entity has the following structure:
//
//          bits 31:26  -- all zero
//          bits 25:16  -- associated entity ID
//          bits 15: 0  -- index (multiple lines associated with entity)
//
typedef DWORD       hLine;

// A parameter is always associated with an entity, but it might have
// additional structure:
//
//          bit  31     -- 1 if param represents A for a line, else 0
//          bit  30     -- 1 if param represents theta for a line, else 0
//          bit  29     -- 1 if param represents Y for a point, else 0
//          bit  28     -- 1 if param represents X for a point, else 0
//                                  (only one of these four bits is set)
//
//          bits 25:16  -- associated entity ID
//
//          bits 15: 0  -- if bit 31 or bit 30 is set: line index
//                      -- if bit 29 or bit 28 is set: point index
//                      -- otherwise: parameter index
//
typedef DWORD       hParam;

// To get a parameter ID from a point ID:
#define X_COORD_FOR_PT(hPt)         ((hParam)((hPt) | (1 << 28)))
#define Y_COORD_FOR_PT(hPt)         ((hParam)((hPt) | (1 << 29)))

// To get a parameter ID from a line ID. An infinite line is specified by
// two parameters. The first is the angle theta it makes with the x-axis,
// measured counter-clockwise. To define the second, we first define
// another line through the origin
//
//     p(s) = s*(-sin(theta), cos(theta))
//
// This is perpendicular to line we are trying to define, so they must
// intersect somewhere. The distance from the origin to their intersection
// is that second parameter A, so p(A) lies on both lines.
#define THETA_FOR_LINE(hLn)         ((hParam)(hLn) | (1 << 30))
#define A_FOR_LINE(hLn)             ((hParam)(hLn) | (1 << 31))

// To get a point ID from an entity ID:
#define POINT_FOR_ENTITY(hEnt, k)   ((hPoint)((k) | ((hEnt) << 16)))
// To get a line ID from an entity ID:
#define LINE_FOR_ENTITY(hEnt, k)    ((hLine)((k) | ((hEnt) << 16)))
// To get a parameter ID from an entity ID:
#define PARAM_FOR_ENTITY(hEnt, k)   ((hParam)((k) | ((hEnt) << 16)))

// Given a point, what entity is associated with it?
#define ENTITY_FROM_POINT(hPt)      ((hEntity)((hPt) >> 16))
// Given a point, what was the k value?
#define K_FROM_POINT(hPt)           ((int)((hPt) & 0xffff))
// Given a line, what entity is associated with it?
#define ENTITY_FROM_LINE(hLn)       ((hEntity)((hLn) >> 16))
// Given a parameter, what entity is associated with it?
#define ENTITY_FROM_PARAM(hp)       ((hEntity)(((hp) >> 16) & 1023))
// Given a paramter that is either the X or Y coordinate for a point, what
// is the ID for that point?
#define POINT_FROM_PARAM(hp)        ((hPoint)((hp) & 0x0fffffff))
// Given a parameter that is either the theta or A for a line, line ID?
#define LINE_FROM_PARAM(hp)         ((hPoint)((hp) & 0x0fffffff))

// Constraints also have unique IDs.
typedef DWORD       hConstraint;

// And likewise for layers.
typedef DWORD       hLayer;
#define MAX_LAYERS 32


#define ENTITY_DATUM_POINT              0
#define ENTITY_DATUM_LINE               1
#define ENTITY_LINE_SEGMENT             2
#define ENTITY_CIRCLE                   3
#define ENTITY_CIRCULAR_ARC             4
#define ENTITY_CUBIC_SPLINE             5
#define ENTITY_TTF_TEXT                 6
#define ENTITY_IMPORTED                 7
typedef struct {
    int             type;

    // This entity's ID. The handle IDs of our data (points, parameters)
    // are derived from our own ID.
    hEntity         id; 

    // How many points do we need? How many lines do we need? And not 
    // counting those parameters generated by the points or lines, how
    // many parameters do we need? Their IDs are derived from our own ID
    // according to a rule, so the count is all we need.
    int             points;
    int             lines;
    int             params;

    // Text ops need the string. That's a special case, since it's specified
    // directly, and the solver isn't allowed to modify it.
    char            text[MAX_STRING];
    // And the font file that we'll be using.
    char            file[MAX_STRING];
    // And the factor by which we will artificially increase or decrease the
    // horizontal spacing, since that seems useful.
    double          spacing;

    // The layer that we're on.
    hLayer          layer;
    // Whether we're a normal line (that generates CAM output) or a
    // construction line (that is there only for convenience in the
    // dimensioning).
    BOOL            construction;
} SketchEntity;

typedef struct {
    hParam          id;
    double          v;

    // These are used by the solver, as it figures out the correct order
    // in which to evaluate the equations (in simultaneous subsystems, with
    // back-substitution.)
    BOOL            known;
    int             mark;
    // These are used by the solver as it tries to forward-substitute the
    // easy equations.
    hParam          substd;

    // This is used in order to draw the automatic assumptions on-screen as
    // we solve, so that the user knows which points are draggable.
#define NOT_ASSUMED         0
#define ASSUMED_FIX         0x1d39b3ef
    int             assumed;
    int             assumedLastTime;
} SketchParam;


// The parametric equations of a curve are:
//      x(t) = Ax*(t^3) + Bx*(t^2) + Cx*(t) + Dx +
//                              (Rx + t*Rlx)*cos(omega*t + phix)
//      y(t) = Ay*(t^3) + By*(t^2) + Cy*(t) + Dy +
//                              (Ry + t*Rly)*cos(omega*t + phiy)
// as t goes from 0 to 1.
typedef struct {
    hEntity     id;
    hLayer      layer;

    struct {
        double  A;
        double  B;
        double  C;
        double  D;
        double  phi;
        double  R;
        double  Rl;
    }       x;

    struct {
        double  A;
        double  B;
        double  C;
        double  D;
        double  phi;
        double  R;
        double  Rl;
    }       y;

    double  omega;

    BOOL    construction;
} SketchCurve;

typedef struct {
    hEntity     id;
    hLayer      layer;
    BOOL        construction;
    int         tag;
    double      x0;
    double      y0;
    double      x1;
    double      y1;
} SketchPwl;

#define CONSTRAINT_PT_PT_DISTANCE               0
#define CONSTRAINT_POINTS_COINCIDENT            1
#define CONSTRAINT_PT_LINE_DISTANCE             2
#define CONSTRAINT_LINE_LINE_DISTANCE           3
#define CONSTRAINT_POINT_ON_LINE                4
#define CONSTRAINT_RADIUS                       5
#define CONSTRAINT_LINE_LINE_ANGLE              6
#define CONSTRAINT_AT_MIDPOINT                  7
#define CONSTRAINT_EQUAL_LENGTH                 8 
#define CONSTRAINT_EQUAL_RADIUS                 9
#define CONSTRAINT_ON_CIRCLE                    10
#define CONSTRAINT_PARALLEL                     11
#define CONSTRAINT_PERPENDICULAR                12
#define CONSTRAINT_SYMMETRIC                    13
#define CONSTRAINT_HORIZONTAL                   14
#define CONSTRAINT_VERTICAL                     15
#define CONSTRAINT_FORCE_PARAM                  16
#define CONSTRAINT_FORCE_ANGLE                  17
#define CONSTRAINT_SCALE_MM                     18
#define CONSTRAINT_SCALE_INCH                   19

typedef struct {
    hConstraint id;

    int         type;

    // If the constraint requires a numerical parameter (e.g. a dimension)
    // then this is it. Exact meaning depends on type of constraint.
    double      v;

    // These are the constrained parameters. The meaning depends on the type
    // of constraint.
    hPoint      ptA;
    hPoint      ptB;
    hParam      paramA;
    hParam      paramB;
    hParam      entityA;
    hParam      entityB;
    hParam      lineA;
    hParam      lineB;

    // If the constraint is displayed at a user-selected point on the sketch
    // (e.g. a linear or angular dimension), then this determines where it
    // goes, respect to some point that is calculated with respect to the
    // geometry (using a different rule for each type of constraint, of
    // course).
    DoublePoint offset;

    hLayer      layer;
} SketchConstraint;

#define MAX_ENTITIES_IN_SKETCH      128
#define MAX_PARAMETERS_IN_SKETCH    512
#define MAX_POINTS_IN_SKETCH        256
#define MAX_LINES_IN_SKETCH         128
#define MAX_CURVES_IN_SKETCH        1024
#define MAX_CONSTRAINTS_IN_SKETCH   512
#define MAX_PWLS_IN_SKETCH          65536

// This hash table is used to speed up certain lookups; its size must
// be a prime number, in order to avoid collisions.
#define PARAM_HASH 2129

typedef struct {
    SketchEntity        entity[MAX_ENTITIES_IN_SKETCH];
    int                 entities;

    SketchParam         param[MAX_PARAMETERS_IN_SKETCH];
    int                 params;
    int                 paramHash[PARAM_HASH];

    hLine               line[MAX_LINES_IN_SKETCH];
    int                 lines;

    hPoint              point[MAX_POINTS_IN_SKETCH];
    int                 points;

    SketchCurve         curve[MAX_CURVES_IN_SKETCH];
    int                 curves;

    SketchConstraint    constraint[MAX_CONSTRAINTS_IN_SKETCH];
    int                 constraints;

    SketchPwl           pwl[MAX_PWLS_IN_SKETCH];
    int                 pwls;

    struct {
        struct {
            hLayer              id;
            char                displayName[MAX_STRING];
            BOOL                shown;
        }                   list[MAX_LAYERS];
        int                 n;
    }                   layer;

    BOOL                eqnsDirty;
} Sketch;

#endif
