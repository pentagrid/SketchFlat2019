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

#ifndef __DERIVED_H
#define __DERIVED_H

// Our basic unit is a closed curve; we assemble the line and curve
// segments into sketch into those.
typedef struct {
    DoublePoint *pt;
    int          pts;
} DClosedCurve;

// A polygon contains one or more closed curves. (It's not necessarily
// simple, so not just one.)
#define MAX_CLOSED_CURVES_IN_POLYGON 2048
typedef struct {
    DClosedCurve     curve[MAX_CLOSED_CURVES_IN_POLYGON];
    int              curves;
} DPolygon;

// Types of derived elements.
#define DERIVED_COPY_FROM_LAYER          0
#define DERIVED_OFFSET_CURVE             1
#define DERIVED_UNION                    2
#define DERIVED_DIFFERENCE               3
#define DERIVED_SUPERIMPOSE              4
#define DERIVED_ROUND                    5
#define DERIVED_ZIGZAG_POCKET            6
#define DERIVED_STEP_REPEAT_TRANSLATE    7
#define DERIVED_STEP_REPEAT_ROTATE       8
#define DERIVED_SCALE                    9
#define DERIVED_MIRROR                  10
#define DERIVED_PERFORATE               11

typedef DWORD hDerived;
#define DERIVED_FROM_LAYER(x)       (0x80000000 | (x))
#define IS_DERIVED_FROM_LAYER(x)    (!!(x & (0x80000000)))
#define LAYER_FROM_DERIVED(x)       ((x) & 0x7fffffff)

#define MAX_POINTS_FOR_DERIVED          128

// A derived thing.
typedef struct {
    int         type;
    char        displayName[MAX_STRING];
    hDerived    id;
    // Keep a shown state here as well, because the other list gets
    // blown away each time we regenerate.
    BOOL        shown;
    // Source information
    hDerived    derivedA;
    hDerived    derivedB;
    double      radius;
    double      v0;
    double      v1;
    double      v2;
    double      v3;
    int         n;

    hPoint      pt[MAX_POINTS_FOR_DERIVED];
    int         pts;
} DerivedElement;

// Our last of derived items.
#define MAX_DERIVED_ELEMENTS 64
typedef struct {
    DerivedElement  req[MAX_DERIVED_ELEMENTS];
    int             reqs;

    struct {
        int             type;
        char            displayName[MAX_STRING];
        hDerived        id;
        char            infoA[MAX_STRING];
        char            infoB[MAX_STRING];
        char            infoC[MAX_STRING];
        BOOL            shown;

        DPolygon        p;
    }               poly[MAX_DERIVED_ELEMENTS+MAX_LAYERS];
    int             polys;
} DerivedList;

extern DerivedList *DL;

#endif

