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
// Undo and redo changes to the sketch. We'll keep back-copies of the entity,
// constraint, and parameter tables.
//
// Jonathan Westhues, June 2007
//-----------------------------------------------------------------------------
#include "sketchflat.h"

BOOL ProgramChangedSinceSave;

#define MAX_LEVELS_OF_UNDO      16
static struct {
    Sketch                  SK[MAX_LEVELS_OF_UNDO];
    DerivedList             DL[MAX_LEVELS_OF_UNDO];
    RememberedSubsystems    RSp[MAX_LEVELS_OF_UNDO];

    int          write;         // Next position to write
    int          undoCount;     // Can move back by this many, still valid
    int          redoCount;     // Can move forward by this many, still valid
} Saved;

static Sketch SKtemp;
static DerivedList DLtemp;
static RememberedSubsystems RSptemp;

static int Wrap(int k)
{
    while(k < 0) k += MAX_LEVELS_OF_UNDO;
    while(k >= MAX_LEVELS_OF_UNDO) k -= MAX_LEVELS_OF_UNDO;
    return k;
}

static void CopySketch(Sketch *d, Sketch *s)
{
    memcpy(d->param, s->param, (s->params)*(sizeof(s->param[0])));
    d->params = s->params;

    memcpy(d->entity, s->entity, (s->entities)*(sizeof(s->entity[0])));
    d->entities = s->entities;

    memcpy(d->constraint, s->constraint,
                        (s->constraints)*(sizeof(s->constraint[0])));
    d->constraints = s->constraints;

    memcpy(&(d->layer), &(s->layer), sizeof(s->layer));
    // And everything else is generated. (The params are too, but we need
    // their initial numerical values.)
}
static void CopyDerivedList(DerivedList *d, DerivedList *s)
{
    memcpy(d->req, s->req, (s->reqs)*sizeof(s->req[0]));
    d->reqs = s->reqs;
}
static void CopyRememberedSubsystems(RememberedSubsystems *d,
                                            RememberedSubsystems *s)
{
    memcpy(d, s, sizeof(*d));
}
static void SwapCurrentWith(int i)
{
    i = Wrap(i);
    // Store current in a temporary buffer
    CopySketch(&SKtemp, SK);
    CopyDerivedList(&DLtemp, DL);
    CopyRememberedSubsystems(&RSptemp, RSp);
    // Replace current with entry from list
    CopySketch(SK, &Saved.SK[i]);
    CopyDerivedList(DL, &Saved.DL[i]);
    CopyRememberedSubsystems(RSp, &Saved.RSp[i]);
    // Replace list entry with temporary buffer
    CopySketch(&(Saved.SK[i]), &SKtemp);
    CopyDerivedList(&(Saved.DL[i]), &DLtemp);
    CopyRememberedSubsystems(&(Saved.RSp[i]), &RSptemp);
}

static void UpdateMenus(void)
{
    uiEnableMenuById(MNU_EDIT_UNDO, (Saved.undoCount > 0));
    uiEnableMenuById(MNU_EDIT_REDO, (Saved.redoCount > 0));
}

void UndoRemember(void)
{
    ProgramChangedSinceSave = TRUE;

    CopySketch(&(Saved.SK[Saved.write]), SK);
    CopyDerivedList(&(Saved.DL[Saved.write]), DL);
    CopyRememberedSubsystems(&(Saved.RSp[Saved.write]), RSp);

    Saved.redoCount = 0;
    if(Saved.undoCount == MAX_LEVELS_OF_UNDO) {
        // Do nothing, just letting it wrap
    } else if(Saved.undoCount > MAX_LEVELS_OF_UNDO) {
        oopsnf();
        return;
    } else {
        (Saved.undoCount)++;
    }
    Saved.write = Wrap(Saved.write + 1);

    UpdateMenus();
}

static void Regenerate(void)
{
    ClearHoverAndSelected();
    SK->eqnsDirty = TRUE;
    GenerateParametersPointsLines();
    GenerateCurvesAndPwls(-1);
    uiRepaint();

    // Regenerate the layer list, without losing the selection.
    int layerSelected = uiGetLayerListSelection();
    if(layerSelected < 0) layerSelected = 0;
    UpdateLayerList();
    uiSelectInLayerList(layerSelected);

    if(!uiInSketchMode()) {
        GenerateDeriveds();
    }

    NowUnsolved();
    SolvePerMode(FALSE);
}

static void Undo(void)
{
    if(Saved.undoCount > 0) {
        Saved.write = Wrap(Saved.write - 1);
      
        SwapCurrentWith(Saved.write);

        (Saved.undoCount)--;
        (Saved.redoCount)++;

        Regenerate();
    }
    UpdateMenus();
}

static void Redo(void)
{
    if(Saved.redoCount > 0) {
        SwapCurrentWith(Saved.write);
        
        Saved.write = Wrap(Saved.write + 1);
        (Saved.redoCount)--;
        (Saved.undoCount)++;

        Regenerate();
    }
    UpdateMenus();
}

void UndoFlush(void)
{
    Saved.undoCount = 0;
    Saved.redoCount = 0;
    Saved.write = 0;
    UpdateMenus();
}

void MenuUndo(int id)
{
    switch(id) {
        case MNU_EDIT_UNDO:
            Undo();
            break;

        case MNU_EDIT_REDO:
            Redo();
            break;
    }
}

