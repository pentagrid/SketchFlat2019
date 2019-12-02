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
// Add entities to the sketch, and do all the parameters bookkeeping. This
// does not consider constraints.
// Jonathan Westhues, April 2007
//-----------------------------------------------------------------------------
#include "sketchflat.h"

static Sketch SKalloc;
Sketch *SK = &SKalloc;

typedef struct {
    SketchParam param[MAX_PARAMETERS_IN_SKETCH];
    int         params;
} SavedParams;

static SavedParams Remembered;
static SavedParams Good;

static double FindRemembered(hParam p)
{
    int i;
    for(i = 0; i < Remembered.params; i++) {
        if(Remembered.param[i].id == p) {
            return Remembered.param[i].v;
        }
    }

    return 0;
}

void RestoreParamsToRemembered(void)
{
    int i;
    for(i = 0; i < Remembered.params; i++) {
        ForceParam(Remembered.param[i].id, Remembered.param[i].v);
    }
}

void SaveGoodParams(void)
{
    memcpy(Good.param, SK->param, (SK->params)*sizeof(SketchParam));
    Good.params = SK->params;
}

void RestoreParamsToLastGood(void)
{
    int i;
    for(i = 0; i < Good.params; i++) {
        ForceParam(Good.param[i].id, Good.param[i].v);
    }
}

double EvalParam(hParam p)
{
    unsigned int i;

    i = SK->paramHash[p % PARAM_HASH];
    if(i < (unsigned)SK->params && SK->param[i].id == p) {
        return SK->param[i].v;
    }

    for(i = 0; i < (unsigned)SK->params; i++) {
        if(SK->param[i].id == p) {
            SK->paramHash[p % PARAM_HASH] = i;
            return SK->param[i].v;
        }
    }
    dbp("param=%08x", p);
    oops();
}

SketchParam *ParamById(hParam p)
{
    int i;
    for(i = 0; i < SK->params; i++) {
        if(SK->param[i].id == p) {
            return &(SK->param[i]);
        }
    }
    return NULL;
}

void EvalPoint(hPoint pt, double *x, double *y)
{
    hParam xp = X_COORD_FOR_PT(pt);
    hParam yp = Y_COORD_FOR_PT(pt);

    *x = EvalParam(xp);
    *y = EvalParam(yp);
}

BOOL PointExistsInSketch(hPoint pt)
{
    int i;
    for(i = 0; i < SK->points; i++) {
        if(SK->point[i] == pt) return TRUE;
    }
    return FALSE;
}

//-----------------------------------------------------------------------------
// Force the value of a parameter in the sketch's parameter table. It's an
// error if that parameter does not already exist.
//-----------------------------------------------------------------------------
void ForceParam(hParam p, double v)
{
    unsigned int i;

    i = SK->paramHash[p % PARAM_HASH];
    if(i < (unsigned)SK->params && SK->param[i].id == p) {
        SK->param[i].v = v;
        return;
    }

    for(i = 0; i < (unsigned)SK->params; i++) {
        if(SK->param[i].id == p) {
            SK->param[i].v = v;
            return;
        }
    }
    // A number of things can make us force a non-existent parameter, for
    // example if we recover to the last good remembered set of parameters,
    // and some sketch items have been deleted since the last time we
    // successfully solved. So ignore it if we do.
}

//-----------------------------------------------------------------------------
// Force the value of a point in the sketch's parameter table.
//-----------------------------------------------------------------------------
void ForcePoint(hPoint pt, double x, double y)
{
    ForceParam(X_COORD_FOR_PT(pt), x);
    ForceParam(Y_COORD_FOR_PT(pt), y);
}

static void AddParam(hParam p)
{
    int i;
    // It should not exist already.
    for(i = 0; i < SK->params; i++) {
        if(SK->param[i].id == p) {
            oopsnf();
            return;
        }
    }

    i = SK->params;
    if(i >= arraylen(SK->param)) oops();
    SK->param[i].id = p;
    SK->param[i].v = FindRemembered(p);

    SK->params = i + 1;
}
static void AddPoint(hPoint pt)
{
    int i;
    // It should not exist already.
    for(i = 0; i < SK->points; i++) {
        if(SK->point[i] == pt) {
            oopsnf();
            return;
        }
    }

    i = SK->points;
    if(i >= arraylen(SK->point)) oops();
    SK->point[i] = pt;
    SK->points = i + 1;
}
static void AddLine(hLine ln)
{
    int i;
    // It should not exist already.
    for(i = 0; i < SK->lines; i++) {
        if(SK->line[i] == ln) {
            oopsnf();
            return;
        }
    }

    i = SK->lines;
    if(i >= arraylen(SK->line)) oops();
    SK->line[i] = ln;
    SK->lines = i + 1;
}

//-----------------------------------------------------------------------------
// Force the references back to their correct positions, if something else
// has been trying to drag them around.
//-----------------------------------------------------------------------------
void ForceReferences(void)
{
    // Origin:
    hPoint pt = POINT_FOR_ENTITY(REFERENCE_ENTITY, 0);
    ForcePoint(pt, 0, 0);

    hLine ln;
    // Horizontal axis:
    ln = LINE_FOR_ENTITY(REFERENCE_ENTITY, 0);
    ForceParam(THETA_FOR_LINE(ln), 0);
    ForceParam(A_FOR_LINE(ln), 0);
    // Vertical axis:
    ln = LINE_FOR_ENTITY(REFERENCE_ENTITY, 1);
    ForceParam(THETA_FOR_LINE(ln), PI/2);
    ForceParam(A_FOR_LINE(ln), 0); 
}

//-----------------------------------------------------------------------------
// First, loop through the entities; for each entity, we will generate one
// or more points.
//-----------------------------------------------------------------------------
void GenerateParametersPointsLines(void)
{
    int i;

    // We'll be recreating the list, but don't forget numerical values,
    // since those are probably almost right (and for an underconstrained
    // sketch, our only indication of what this should look like).
    memcpy(Remembered.param, SK->param, SK->params*sizeof(SketchParam));
    Remembered.params = SK->params;

    SK->params = 0;
    SK->points = 0;
    SK->lines = 0;

    // First, generate the references. Our sketch must always contain
    // a datum point at the origin, and two datum lines, parallel to the
    // two coordinate axes.
    //
    // Origin:
    hPoint pt = POINT_FOR_ENTITY(REFERENCE_ENTITY, 0);
    AddPoint(pt);
    AddParam(X_COORD_FOR_PT(pt));
    AddParam(Y_COORD_FOR_PT(pt));
    hLine ln;
    // Horizontal axis:
    ln = LINE_FOR_ENTITY(REFERENCE_ENTITY, 0);
    AddLine(ln);
    AddParam(THETA_FOR_LINE(ln));
    AddParam(A_FOR_LINE(ln));
    // Vertical axis:
    ln = LINE_FOR_ENTITY(REFERENCE_ENTITY, 1);
    AddLine(ln);
    AddParam(THETA_FOR_LINE(ln));
    AddParam(A_FOR_LINE(ln));
    // so place the references at the desired positions
    ForceReferences();

    // Now generate based on the entities that have been drawn.
    for(i = 0; i < SK->entities; i++) {
        SketchEntity *e = &(SK->entity[i]);
        int j;
        for(j = 0; j < e->points; j++) {
            hPoint pt = POINT_FOR_ENTITY(e->id, j);
            AddPoint(pt);
            // And add the parameters for the point's X and Y.
            AddParam(X_COORD_FOR_PT(pt));
            AddParam(Y_COORD_FOR_PT(pt));
        }
        for(j = 0; j < e->params; j++) {
            AddParam(PARAM_FOR_ENTITY(e->id, j));
        }
        for(j = 0; j < e->lines; j++) {
            hLine ln = LINE_FOR_ENTITY(e->id, j);
            AddLine(ln);
            // And add the parameters for the line's theta and a.
            AddParam(THETA_FOR_LINE(ln));
            AddParam(A_FOR_LINE(ln));
        }
    }
}

//-----------------------------------------------------------------------------
// Delete an entity from the sketch. This is relatively straightforward;
// once we regenerate the points, lines, and curves, its children will just
// disappear.
//-----------------------------------------------------------------------------
void SketchDeleteEntity(hEntity he)
{
    SK->eqnsDirty = TRUE;

    int i;
    static hConstraint ToDelete[MAX_CONSTRAINTS_IN_SKETCH];
    int toDelete;

    // So that we can't accidentally get deleted later, remove ourselves
    // from the selection.
    for(i = 0; i < MAX_SELECTED_ITEMS; i++) {
        if(Selected[i].which == SEL_ENTITY && Selected[i].entity == he) {
            Selected[i].which = SEL_NONE;
            Selected[i].entity = 0;
        }
    }

    if(he == REFERENCE_ENTITY) {
        // We can't delete the references, special case.
        oopsnf();
        return;
    }

    // Before we delete the entity, we must delete any constraints that
    // reference it. Otherwise things will break when that constraint
    // tries to make its equations.
    toDelete = 0;
    for(i = 0; i < SK->constraints; i++) {
        BOOL del = FALSE;
        SketchConstraint *c = &(SK->constraint[i]);

        if(c->entityA == he ||
           c->entityB == he)                            del = TRUE;
        if(ENTITY_FROM_LINE(c->lineA) == he ||
           ENTITY_FROM_LINE(c->lineB) == he)            del = TRUE;
        if(ENTITY_FROM_POINT(c->ptA) == he ||
           ENTITY_FROM_POINT(c->ptB) == he)             del = TRUE;
        if(ENTITY_FROM_PARAM(c->paramA) == he ||
           ENTITY_FROM_PARAM(c->paramB) == he)          del = TRUE;

        if(del) {
            ToDelete[toDelete] = c->id;
            toDelete++;
        }
    }
    for(i = 0; i < toDelete; i++) {
        DeleteConstraint(ToDelete[i]);
    }

    for(i = 0; i < SK->entities; i++) {
        if(SK->entity[i].id == he) {
            (SK->entities)--;
            memmove(&(SK->entity[i]), &(SK->entity[i+1]),
                (SK->entities - i)*sizeof(SK->entity[0]));
    
            GenerateParametersPointsLines();
            uiRepaint();
            return;
        }
    }

    // Odd, we didn't find it.
    oopsnf();
}

//-----------------------------------------------------------------------------
// Given an entity's ID, return its descriptor. This requires a search through
// the tables.
//-----------------------------------------------------------------------------
SketchEntity *EntityById(hEntity he)
{
    int i;
    if(he == REFERENCE_ENTITY) oops();

    for(i = 0; i < SK->entities; i++) {
        if(SK->entity[i].id == he) {
            return &(SK->entity[i]);
        }
    }
    oops();
}

//-----------------------------------------------------------------------------
// Add an entity to the sketch; returns the ID of the newly inserted entity.
//-----------------------------------------------------------------------------
static hEntity SketchAddEntityWorker(SketchEntity *e)
{
    SK->eqnsDirty = TRUE;
    UndoRemember();

    int i;
    hEntity greatestId = 0;

    for(i = 0; i < SK->entities; i++) {
        if(SK->entity[i].id > greatestId) {
            greatestId = SK->entity[i].id;
        }
    }

    i = SK->entities;
    if(i >= MAX_ENTITIES_IN_SKETCH) oops();

    memcpy(&(SK->entity[i]), e, sizeof(*e));
    SK->entity[i].id = greatestId + 1;
    SK->entity[i].layer = GetCurrentLayer();
    SK->entities = i + 1;

    return SK->entity[i].id;
}

hEntity SketchAddEntity(int type)
{
    static struct {
        int type;
        int points;
        int lines;
        int params;
    } EntityRequirements[] = {
        // entity type            points    lines   params
        { ENTITY_DATUM_POINT,     1,        0,      0       },
        { ENTITY_DATUM_LINE,      0,        1,      0       },
        { ENTITY_LINE_SEGMENT,    2,        0,      0       },
        { ENTITY_TTF_TEXT,        2,        0,      0       },
        { ENTITY_IMPORTED,        2,        0,      0       },
        { ENTITY_CIRCLE,          1,        0,      1       },
        { ENTITY_CIRCULAR_ARC,    3,        0,      0       },
        { ENTITY_CUBIC_SPLINE,    4,        0,      0       },
    };

    SketchEntity e;
    memset(&e, 0, sizeof(e));
    e.spacing = 0;

    int i;
    for(i = 0; i < arraylen(EntityRequirements); i++) {
        if(type == EntityRequirements[i].type) break;
    }
    if(i >= arraylen(EntityRequirements)) oops();

    e.type   = type;
    e.points = EntityRequirements[i].points;
    e.lines  = EntityRequirements[i].lines;
    e.params = EntityRequirements[i].params;

    // A TrueType text element needs a bit of additional setup.
    if(type == ENTITY_TTF_TEXT) {
        txtuiGetDefaultFont(e.file);
        strcpy(e.text, "Abc");
    }

    hEntity he = SketchAddEntityWorker(&e);
    GenerateParametersPointsLines();
    return he;
}

void SketchAddPointToCubicSpline(hEntity he)
{
    SketchEntity *e = EntityById(he);

    // Two extra points per piecewise cubic segment.
    (e->points) += 2;
}
