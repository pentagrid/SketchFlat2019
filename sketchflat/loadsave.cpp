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
// Saved file stuff: write the current sketch out as a text file, and then
// read it in. We must write a description of the entities, plus the current
// parameter state. Also configuration options (layer settings etc.).
//
// Jonathan Westhues, April 2007
//-----------------------------------------------------------------------------
#include "sketchflat.h"

typedef struct {
    const char    *str;
    int      type;
} TypeMap;

TypeMap EntityTypes[] = {
    { "DATUM_POINT",    ENTITY_DATUM_POINT },
    { "DATUM_LINE",     ENTITY_DATUM_LINE },
    { "LINE_SEGMENT",   ENTITY_LINE_SEGMENT },
    { "CIRCLE",         ENTITY_CIRCLE },
    { "CIRCULAR_ARC",   ENTITY_CIRCULAR_ARC },
    { "CUBIC_SPLINE",   ENTITY_CUBIC_SPLINE },
    { "TTF_TEXT",       ENTITY_TTF_TEXT },
    { "IMPORTED",       ENTITY_IMPORTED },
};

TypeMap ConstraintTypes[] = {
    { "PT_PT_DISTANCE",         CONSTRAINT_PT_PT_DISTANCE },
    { "POINTS_COINCIDENT",      CONSTRAINT_POINTS_COINCIDENT },
    { "PT_LINE_DISTANCE",       CONSTRAINT_PT_LINE_DISTANCE },
    { "LINE_LINE_DISTANCE",     CONSTRAINT_LINE_LINE_DISTANCE },
    { "POINT_ON_LINE",          CONSTRAINT_POINT_ON_LINE },
    { "RADIUS",                 CONSTRAINT_RADIUS },
    { "LINE_LINE_ANGLE",        CONSTRAINT_LINE_LINE_ANGLE },
    { "AT_MIDPOINT",            CONSTRAINT_AT_MIDPOINT },
    { "EQUAL_LENGTH",           CONSTRAINT_EQUAL_LENGTH },
    { "EQUAL_RADIUS",           CONSTRAINT_EQUAL_RADIUS },
    { "ON_CIRCLE",              CONSTRAINT_ON_CIRCLE },
    { "PARALLEL",               CONSTRAINT_PARALLEL },
    { "PERPENDICULAR",          CONSTRAINT_PERPENDICULAR },
    { "SYMMETRIC",              CONSTRAINT_SYMMETRIC },
    { "HORIZONTAL",             CONSTRAINT_HORIZONTAL },
    { "VERTICAL",               CONSTRAINT_VERTICAL },
    { "FORCE_PARAM",            CONSTRAINT_FORCE_PARAM },
    { "FORCE_ANGLE",            CONSTRAINT_FORCE_ANGLE },
    { "SCALE_MM",               CONSTRAINT_SCALE_MM },
    { "SCALE_INCH",             CONSTRAINT_SCALE_INCH },
};

TypeMap DerivedTypes[] = {
    { "OFFSET_CURVE",           DERIVED_OFFSET_CURVE },
    { "UNION",                  DERIVED_UNION },
    { "DIFFERENCE",             DERIVED_DIFFERENCE },
    { "SUPERIMPOSE",            DERIVED_SUPERIMPOSE },
    { "ROUND",                  DERIVED_ROUND },
    { "ZIGZAG_POCKET",          DERIVED_ZIGZAG_POCKET },
    { "STEP_REPEAT_TRANSLATE",  DERIVED_STEP_REPEAT_TRANSLATE },
    { "STEP_REPEAT_ROTATE",     DERIVED_STEP_REPEAT_ROTATE },
    { "SCALE",                  DERIVED_SCALE },
    { "MIRROR",                 DERIVED_MIRROR },
    { "PERFORATE",              DERIVED_PERFORATE },
};

static const char *LookUpByType(int type, TypeMap *map, int n)
{
    int i;
    for(i = 0; i < n; i++) {
        if(map[i].type == type) {
            return map[i].str;
        }
    }
    oops();
}

static int LookUpByString(char *str, TypeMap *map, int n)
{
    int i;
    for(i = 0; i < n; i++) {
        if(strcmp(map[i].str, str)==0) {
            return map[i].type;
        }
    }
    oops();
}

#define EntityTypeToString(type) \
            LookUpByType(type, EntityTypes, arraylen(EntityTypes))
#define StringToEntityType(str) \
            LookUpByString(str, EntityTypes, arraylen(EntityTypes))

#define ConstraintTypeToString(type) \
            LookUpByType(type, ConstraintTypes, arraylen(ConstraintTypes))
#define StringToConstraintType(str) \
            LookUpByString(str, ConstraintTypes, arraylen(ConstraintTypes))

#define DerivedTypeToString(type) \
            LookUpByType(type, DerivedTypes, arraylen(DerivedTypes))
#define StringToDerivedType(str) \
            LookUpByString(str, DerivedTypes, arraylen(DerivedTypes))


static void WriteLiteralString(FILE *f, char *s)
{
    fprintf(f, "    ");
    while(*s) {
        if(*s == '\r' || *s == '\n') {
            fprintf(f, " ");
        } else {
            fprintf(f, "%c", *s);
        }
        s++;
    }
    fprintf(f, "\n");
}

static BOOL ReadLiteralString(FILE *f, char *s, int n)
{
    if(!fgets(s, n, f)) return FALSE;
    if(strlen(s) < 4) return FALSE;

    memmove(s, s+4, strlen(s)-3);
    s[strlen(s) - 1] = '\0';

    return TRUE;
}

BOOL SaveToFile(char *name)
{
    FILE *f = fopen(name, "w");
    if(!f) {
        return FALSE;
    }

    int i, j;
    // First, the parameters
    for(i = 0; i < SK->params; i++) {
        fprintf(f, "PARAM %08x %.9f\n", SK->param[i].id, SK->param[i].v);
    }

    fprintf(f, "\n");

    // Then, the entities
    for(i = 0; i < SK->entities; i++) {
        fprintf(f, "ENTITY %s %08x %d %08x %d %d %d\n",
            EntityTypeToString(SK->entity[i].type),
            SK->entity[i].layer,
            SK->entity[i].construction,
            SK->entity[i].id,
            SK->entity[i].points,
            SK->entity[i].lines,
            SK->entity[i].params);

        if(SK->entity[i].type == ENTITY_TTF_TEXT ||
           SK->entity[i].type == ENTITY_IMPORTED)
        {
            // The text and imported file entries come with some extra info,
            // for filenames and strings and such.
            WriteLiteralString(f, SK->entity[i].text);
            WriteLiteralString(f, SK->entity[i].file);
            fprintf(f, "    %.3f\n", SK->entity[i].spacing);
            fprintf(f, "\n");
        }
    }

    fprintf(f, "\n");

    // Then, the constraints
    for(i = 0; i < SK->constraints; i++) {
        SketchConstraint *c = &(SK->constraint[i]);

        fprintf(f, "CONSTRAINT %s %08x %08x %.9f %08x %08x %08x %08x "
                           "%08x %08x %08x %08x "
                           "%.9f %.9f\n",
                    ConstraintTypeToString(c->type),
                    c->layer,
                    c->id,
                    c->v,
                    c->ptA, c->ptB,
                    c->paramA, c->paramB,
                    c->entityA, c->entityB,
                    c->lineA, c->lineB,
                    c->offset.x, c->offset.y);
    }

    fprintf(f, "\n");

    // The list of layers
    for(i = 0; i < SK->layer.n; i++) {
        fprintf(f, "LAYER %08x %s %d\n",
            SK->layer.list[i].id,
            SK->layer.list[i].displayName,
            SK->layer.list[i].shown);
    }

    fprintf(f, "\n");

    // Our cheat sheet on how to partition the system to solve it.
    for(i = 0; i < RSp->sets; i++) {
        fprintf(f, "REMEMBERED_SUBSET\n");
        if(RSp->set[i].eqs > 0) {
            for(j = 0; j < RSp->set[i].eqs; j++) {
                fprintf(f, "    eq %08x\n", RSp->set[i].eq[j]);
            }
        } else {
            fprintf(f, "    param %08x\n", RSp->set[i].p);
        }
        fprintf(f, "\n");
    }

    // Once we have the sketch, with a polygon on each layer, we might
    // derive some other polygons from those polygons.
    for(i = 0; i < DL->reqs; i++) {
        DerivedElement *d = &(DL->req[i]);
        fprintf(f,
          "DERIVED %s %s %08x %d %08x %08x %.9f %.9f %.9f %.9f %.9f %d %d\n",
            DerivedTypeToString(d->type),
            d->displayName,
            d->id, d->shown,
            d->derivedA, d->derivedB,
            d->radius,
            d->v0,
            d->v1,
            d->v2,
            d->v3,
            d->n,
            d->pts);
        int j;
        for(j = 0; j < d->pts; j++) {
            fprintf(f, "    point %08x\n", d->pt[j]);
        }
    }

    fclose(f);
    return TRUE;
}

static void RefreshAllGenerated(void)
{
    UndoFlush();
    ProgramChangedSinceSave = FALSE;

    SK->eqnsDirty = TRUE;
    GenerateParametersPointsLines();
    UpdateLayerList();
    // This forces us to create at least one layer, if none exist already,
    // and selects the first layer if not is yet selected.
    (void)GetCurrentLayer();

    ClearHoverAndSelected();
    uiForceSketchMode();
    ZoomToFit();

    NowUnsolved();
}

void NewEmptyProgram(void)
{
    memset(SK, 0, sizeof(*SK));
    memset(RSp, 0, sizeof(*RSp));
    memset(DL, 0, sizeof(*DL));

    RefreshAllGenerated();
}

BOOL LoadFromFile(char *name)
{
    FILE *f = fopen(name, "r");
    if(!f) {
        return FALSE;
    }

    memset(SK, 0, sizeof(*SK));
    memset(RSp, 0, sizeof(*RSp));
    memset(DL, 0, sizeof(*DL));

    char line[MAX_STRING];

    while(fgets(line, sizeof(line), f)) {
        if(line[0] == '\0') return FALSE;

        if(line[strlen(line)-1] != '\n') {
            // odd, excessively long line
            return FALSE;
        }

        line[strlen(line)-1] = '\0';

        if(line[0] == '\0' || line[0] == ' ' || line[0] == '#') {
            // Comment or blank line
            continue;
        }

        int i;
        double v, v0, v1, v2, v3;
        int n, pts;
        hParam hp;
        hEntity he;
        hEquation heq;
        hLayer layer;
        BOOL construction, shown;
        int points, lines, params;

        hDerived hd, derivedA, derivedB;

        SketchConstraint c;

        char entity[MAX_STRING];
        char constraint[MAX_STRING];
        char derived[MAX_STRING];
        char displayName[MAX_STRING];

        if(sscanf(line, "PARAM %x %lg", &hp, &v)==2) {
            i = SK->params;
            if(i >= arraylen(SK->param)) oops();
            SK->param[i].id = hp;
            SK->param[i].v = v;
            SK->params = i + 1;
        } else if(sscanf(line, "ENTITY %[A-Z_] %x %d %x %d %d %d", entity,
            &layer, &construction,
            &he, &points, &lines, &params)==7)
        {
            i = SK->entities;
            if(i >= arraylen(SK->entity)) oops();
            SK->entity[i].layer = layer;
            SK->entity[i].construction = construction;
            SK->entity[i].id = he;
            SK->entity[i].type = StringToEntityType(entity);
            SK->entity[i].points = points;
            SK->entity[i].lines = lines;
            SK->entity[i].params = params;
            SK->entities = i + 1;

            if(SK->entity[i].type == ENTITY_TTF_TEXT ||
               SK->entity[i].type == ENTITY_IMPORTED)
            {
                // In addition to the usual parameters, a font entity requires
                // the text to be displayed, and the font in which to display
                // that string (filename of the .ttf).
                if(!ReadLiteralString(f, line, sizeof(line))) return FALSE;
                strcpy(SK->entity[i].text, line);

                if(!ReadLiteralString(f, line, sizeof(line))) return FALSE;
                strcpy(SK->entity[i].file, line);

                if(!fgets(line, sizeof(line), f)) return FALSE;
                SK->entity[i].spacing = atof(line);
            }
        } else if(sscanf(line, "CONSTRAINT %[A-Z_] %x %x %lg %x %x %x %x "
                                    "%x %x %x %x "
                                    "%lg %lg",
                                constraint,
                                &(c.layer),
                                &(c.id),
                                &(c.v),
                                &(c.ptA), &(c.ptB),
                                &(c.paramA), &(c.paramB),
                                &(c.entityA), &(c.entityB),
                                &(c.lineA), &(c.lineB),
                                &(c.offset.x), &(c.offset.y)) == 14)
        {
            c.type = StringToConstraintType(constraint);
            i = SK->constraints;
            memcpy(&(SK->constraint[i]), &c, sizeof(c));
            SK->constraints = i + 1;
        } else if(strcmp(line, "REMEMBERED_SUBSET")==0) {
            int k = RSp->sets;
            if(k >= MAX_REMEMBERED_SUBSYSTEMS) return FALSE;

            fgets(line, sizeof(line), f);
            if(sscanf(line, "    param %08x", &hp)==1) {
                // This step represents an assumption.
                RSp->set[k].eqs = 0;
                RSp->set[k].p = hp;
            } else {
                // This step represents a list of equations to solve.
                while(sscanf(line, "    eq %08x", &heq)==1) {
                    int a = RSp->set[k].eqs;
                    if(a >= MAX_NUMERICAL_UNKNOWNS) return FALSE;

                    RSp->set[k].eq[a] = heq;
                    RSp->set[k].eqs = (a + 1);

                    fgets(line, sizeof(line), f);
                }
            }

            if(RSp->set[k].eqs == 0 && RSp->set[k].p == 0) return FALSE;
            RSp->sets = (k + 1);
        } else if(sscanf(line, "LAYER %08x %[A-Za-z_0-9] %d", &layer,
            displayName, &shown)==3)
        {
            int k = SK->layer.n;
            if(k >= MAX_LAYERS) return FALSE;
            
            SK->layer.list[k].id = layer;
            SK->layer.list[k].shown = shown;
            strcpy(SK->layer.list[k].displayName, displayName);

            SK->layer.n = (k + 1);
        } else if(sscanf(line,
            "DERIVED %[A-Z_] %[A-Za-z_0-9] %08x %d %08x %08x %lg "
                                            "%lg %lg %lg %lg %d %d",
            derived, displayName, &hd, &shown, &derivedA, &derivedB, &v,
            &v0, &v1, &v2, &v3, &n, &pts)==13)
        {
            int k = DL->reqs;
            if(k >= MAX_DERIVED_ELEMENTS) return FALSE;

            DL->req[k].type = StringToDerivedType(derived);
            strcpy(DL->req[k].displayName, displayName);
            DL->req[k].id = hd;
            DL->req[k].shown = shown;
            DL->req[k].derivedA = derivedA;
            DL->req[k].derivedB = derivedB;
            DL->req[k].radius = v;
            DL->req[k].v0 = v0;
            DL->req[k].v1 = v1;
            DL->req[k].v2 = v2;
            DL->req[k].v3 = v3;
            DL->req[k].n = n;
            DL->req[k].pts = pts;
            if(pts >= MAX_POINTS_FOR_DERIVED) return FALSE;

            int a;
            for(a = 0; a < pts; a++) {
                fgets(line, sizeof(line), f);
                hPoint pt;
                if(sscanf(line, "    point %08x", &pt) != 1) return FALSE;
                DL->req[k].pt[a] = pt;
            }

            DL->reqs = (k + 1);
        }
    }
    
    fclose(f);
    RefreshAllGenerated();

    return TRUE;
}
