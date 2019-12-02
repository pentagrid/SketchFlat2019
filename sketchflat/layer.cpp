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
// Routines that allow us to handle multiple sketches on multiple layers.
//
// Jonathan Westhues, May 2007
//-----------------------------------------------------------------------------
#include "sketchflat.h"

//-----------------------------------------------------------------------------
// Update the layer list control to contain whatever information is in our
// sketch's layer table.
//-----------------------------------------------------------------------------
void UpdateLayerList(void)
{
    int i;

    uiClearLayerList();
    for(i = 0; i < SK->layer.n; i++) {
        char str[MAX_STRING];
        sprintf(str, " %s", SK->layer.list[i].displayName);
        uiAddToLayerList(SK->layer.list[i].shown, str);
    }
}

hLayer GetCurrentLayer(void)
{
    // If we don't have at least one layer, create one, and we'll be choosing
    // that one.
    if(SK->layer.n <= 0) {
        SK->layer.list[0].id = 1;
        SK->layer.list[0].shown = TRUE;
        strcpy(SK->layer.list[0].displayName, "Layer00000001");
        SK->layer.n = 1;
        UpdateLayerList();
    }

    if(uiGetLayerListSelection() >= SK->layer.n) {
        // Shouldn't happen; listview was out of sync with our last for
        // some reason.
        UpdateLayerList();
    }

    if(uiGetLayerListSelection() < 0) {
        // Default to first layer if nothing selected.
        uiSelectInLayerList(0);
    }

    int p = uiGetLayerListSelection();

    return SK->layer.list[p].id;
}

//-----------------------------------------------------------------------------
// Given an hPoint or an hLine, find what layer it's on. That will require
// us to look up the associated entity, find that entity in the table, and
// return that entity's ->layer.
//-----------------------------------------------------------------------------
hLayer LayerForEntity(hEntity he)
{
    if(he == REFERENCE_ENTITY) return REFERENCE_LAYER;

    SketchEntity *e = EntityById(he);
    return e->layer;
}
hLayer LayerForPoint(hPoint pt)
{
    hEntity he = ENTITY_FROM_POINT(pt);

    return LayerForEntity(he);
}
hLayer LayerForLine(hLine ln)
{
    hEntity he = ENTITY_FROM_LINE(ln);

    return LayerForEntity(he);
}

BOOL LayerIsShown(hLayer layer)
{
    if(layer == REFERENCE_LAYER) return TRUE;

    int i;
    for(i = 0; i < SK->layer.n; i++) {
        if(layer == SK->layer.list[i].id) {
            return SK->layer.list[i].shown;
        }
    }

    return TRUE;
}

void ButtonAddLayer(BOOL before)
{
    if(SK->layer.n >= MAX_LAYERS) {
        uiError("Too many layers.");
        return;
    }

    UndoRemember();

    int currentPos = uiGetLayerListSelection();

    int ip;
    if(SK->layer.n == 0) {
        ip = 0;
    } else {
        if(currentPos < 0) {
            uiError("Must select layer before inserting new layer before or "
                    "after.");
            return;
        }
        if(before) {
            ip = currentPos;
        } else {
            ip = currentPos + 1;
        }
    }

    // Generate a new layer ID, higher than any previous one.
    hLayer max = 0;
    int i;
    for(i = 0; i < SK->layer.n; i++) {
        if(SK->layer.list[i].id > max) {
            max = SK->layer.list[i].id;
        }
    }
    max++;

    // Insert the new layer at the appropriate position
    memmove(&(SK->layer.list[ip+1]), &(SK->layer.list[ip]),
        (SK->layer.n - ip)*sizeof(SK->layer.list[0]));
    (SK->layer.n)++;

    SK->layer.list[ip].id = max;
    sprintf(SK->layer.list[ip].displayName, "Layer%08x", max);
    SK->layer.list[ip].shown = TRUE;

    UpdateLayerList();
    uiSelectInLayerList(ip);
}

void ButtonDeleteLayer(void)
{
    if(SK->layer.n <= 1) {
        uiError("Can't delete this layer; only one left in sketch.");
        return;
    }

    UndoRemember();

    int dp = uiGetLayerListSelection();

    if(dp < 0) {
        uiError("Must select a layer to delete.");
        return;
    }

    hLayer hl = SK->layer.list[dp].id;

    // All entities (and in the process, constraints) on that layer must
    // be deleted before we can proceed.
    static hEntity ToDelete[MAX_ENTITIES_IN_SKETCH];
    int tdc = 0;

    int i;
    for(i = 0; i < SK->entities; i++) {
        SketchEntity *e = &(SK->entity[i]);

        if(e->layer == hl) {
            ToDelete[tdc++] = e->id;
        }
    }
    for(i = 0; i < tdc; i++) {
        SketchDeleteEntity(ToDelete[i]);
    }

    // Any leftover constraints were malformed, because they constrained
    // only entities on different layers. They must still be deleted,
    // because there is otherwise no way to manipulate them anymore.
    static hConstraint ConstraintToDelete[MAX_CONSTRAINTS_IN_SKETCH];
    tdc = 0;
    for(i = 0; i < SK->constraints; i++) {
        SketchConstraint *c = &(SK->constraint[i]);
        if(c->layer == hl) {
            ConstraintToDelete[tdc++] = c->id;
        }
    }
    for(i = 0; i < tdc; i++) {
        DeleteConstraint(ConstraintToDelete[i]);
    }

    // Regenerate to get rid of anything generated on the dying layer.
    GenerateParametersPointsLines();
    GenerateCurvesAndPwls(-1);

    // And now we can delete the layer.
    (SK->layer.n)--;
    memmove(&(SK->layer.list[dp]), &(SK->layer.list[dp+1]),
        (SK->layer.n - dp)*sizeof(SK->layer.list[0]));
    UpdateLayerList();

    if(dp == 0) {
        uiSelectInLayerList(0);
    } else {
        uiSelectInLayerList(dp-1);
    }
}

void LayerDisplayNameUpdated(int p, char *str)
{
    if(p < 0 || p >= SK->layer.n) return;

    char *d = SK->layer.list[p].displayName;
    if(*str == ' ') {
        strcpy(d, str+1);
    } else {
        strcpy(d, str);
    }

    // Remove any characters other than [-A-Za-z0-9_]
    char *dp;
    for(dp = d; *dp; dp++) {
        if(*dp >= 'A' && *dp <= 'Z') continue;
        if(*dp >= 'a' && *dp <= 'z') continue;
        if(*dp >= '0' && *dp <= '9') continue;
        if(*dp == '_') continue;

        *dp = '_';
    }

    sprintf(str, " %s", d);
}

void LayerDisplayChanged(void)
{
    uiRepaint();

    // Don't clear the selection, because then there's no way to bring
    // a constraint onto a different layer (since constraints are
    // hidden, except when their layer is active).
}

void LayerDisplayCheckboxChanged(int p, BOOL checked)
{
    if(p < 0 || p >= SK->layer.n) return;

    SK->layer.list[p].shown = checked;
}

