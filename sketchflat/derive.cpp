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
// First, the user draws some curves. These would typically represent what the
// part is supposed to look like. Then, the user must transform the curves
// into something they can feed to the CNC machine. (For example, a mill
// might require cutter radius compensation.) They might also wish to merge
// multiple polgons using boolean union/difference operations, if that would
// make a part easier to draw. All of that happens here.
// Jonathan Westhues, May 2007
//-----------------------------------------------------------------------------
#include "sketchflat.h"

static DerivedList DLalloc;
extern DerivedList *DL = &DLalloc;

static hPoint HoveredPoint;
static hPoint SelectedPoints[MAX_POINTS_FOR_DERIVED];

// Keep our own private record of which layers are shown; don't necessarily
// want to always go back to what's shown in the sketch view.
static struct {
    hLayer      layer;
    BOOL        shown;
} LayerShown[MAX_LAYERS];

//=============================================================================
// Routines to build up our list of derived items: user interface to specify
// those, and to display the current list in a TreeView control. The requested
// operations are not performed immediately, but are stored in a list of
// requests, so that we can re-perform them every time the source sketches
// change.
//=============================================================================

static int CountWithType(int type)
{
    int i;
    int max = 0;
    for(i = 0; i < DL->reqs; i++) {
        if(DL->req[i].type != type) continue;

        char *s0 = DL->req[i].displayName;
        char *s = s0 + (strlen(s0) - 1);

        while(isdigit(*s) && (s - s0) > 0) {
            s--;
        }
        int v = atoi(s+1);
        if(v > max) {
            max = v;
        }
    }

    return max;
}

static void InsertDerivedItem(DerivedElement *de)
{
    if(DL->reqs >= MAX_DERIVED_ELEMENTS) return;

    UndoRemember();

    // Find a new unique ID to assign our thing.
    int i;
    hDerived maxId = 0;
    for(i = 0; i < DL->reqs; i++) {
        if(DL->req[i].id > maxId) {
            maxId = DL->req[i].id;
        }
    }
    maxId++;

    // Now add the new element to our table.
    i = DL->reqs;
    memcpy(&(DL->req[i]), de, sizeof(*de));
    // And assign it a unique ID.
    DL->req[i].id = maxId;

    DL->reqs = (i + 1);
}

static void DeleteDerivedItem(hDerived id)
{
    if(IS_DERIVED_FROM_LAYER(id)) {
        uiError("Can't delete sketch layer in derive mode.");
        return;
    }

    UndoRemember();

    int i;
    for(i = 0; i < DL->reqs; i++) {
        if(DL->req[i].id == id) break;
    }
    if(i >= DL->reqs) {
        oopsnf();
        return;
    }

    memmove(&(DL->req[i]), &(DL->req[i+1]), (DL->reqs - i)*sizeof(DL->req[0]));
    (DL->reqs)--;
}

static DPolygon *GetPolygonFrom(hDerived id)
{
    int i;
    for(i = 0; i < DL->polys; i++) {
        if(DL->poly[i].id == id) {
            return &(DL->poly[i].p);
        }
    }
    return NULL;
}

static BOOL EditDerivedItem(DerivedElement *de)
{
    switch(de->type) {
        case DERIVED_UNION: {
            hDerived polys[2] = { de->derivedA, de->derivedB };
            const char *labels[] = { "Polygon A:", "Polygon B:" };
            if(uiShowSimpleDialog("Union: Polygon A + Polygon B", 2,
                labels, 0, polys, NULL))
            {
                de->derivedA = polys[0];
                de->derivedB = polys[1];
                return TRUE;
            } else {
                return FALSE;
            }
        }

        case DERIVED_DIFFERENCE: {
            hDerived polys[2] = { de->derivedA, de->derivedB };
            const char *labels[] = { "Polygon A:", "- Polygon B:" };
            if(uiShowSimpleDialog("Difference: Polygon A - Polygon B", 2,
                labels, 0, polys, NULL))
            {
                de->derivedA = polys[0];
                de->derivedB = polys[1];
                return TRUE;
            } else {
                return FALSE;
            }
        }

        case DERIVED_SUPERIMPOSE: {
            hDerived polys[2] = { de->derivedA, de->derivedB };
            const char *labels[] = { "Polygon A:", "Polygon B:" };
            if(uiShowSimpleDialog("Superimpose: Polygon A and Polygon B", 2,
                labels, 0, polys, NULL))
            {
                de->derivedA = polys[0];
                de->derivedB = polys[1];
                return TRUE;
            } else {
                return FALSE;
            }
        }

        case DERIVED_OFFSET_CURVE: {
            hDerived polys[2] = { de->derivedA };
            char strRadius[MAX_STRING];
            strcpy(strRadius, ToDisplay(de->radius));

            char *strs[2] = { NULL, strRadius };
            const const char *labels[] = { "Polygon:", "Offset:" };
            if(uiShowSimpleDialog("Offset from Polygon Edge", 2,
                labels, (1 << 1), polys, strs))
            {
                de->derivedA = polys[0];
                de->radius = FromDisplay(strRadius);
                return TRUE;
            } else {
                return FALSE;
            }
        }

        case DERIVED_STEP_REPEAT_TRANSLATE: {
            hDerived polys[6] = { de->derivedA };
            char strIdx[MAX_STRING], strIdy[MAX_STRING];
            strcpy(strIdx, ToDisplay(de->v0));
            strcpy(strIdy, ToDisplay(de->v1));

            char strSdx[MAX_STRING], strSdy[MAX_STRING];
            strcpy(strSdx, ToDisplay(de->v2));
            strcpy(strSdy, ToDisplay(de->v3));

            char strN[MAX_STRING];
            sprintf(strN, "%d", de->n);

            char *strs[6] = { NULL, strIdx, strIdy, strSdx, strSdy, strN };
            const char *labels[] = {
                "Polygon:", "Initial dx:", "Initial dy:", "Step dx:",
                "Step dy:", "Copies:"
            };
            if(uiShowSimpleDialog("Step and Repeat Translate", 6,
                labels, 0x3e, polys, strs))
            {
                de->derivedA = polys[0];
                de->v0 = FromDisplay(strIdx);
                de->v1 = FromDisplay(strIdy);
                de->v2 = FromDisplay(strSdx);
                de->v3 = FromDisplay(strSdy);
                de->n = atoi(strN);
                return TRUE;
            } else {
                return FALSE;
            }
        }

        case DERIVED_STEP_REPEAT_ROTATE: {
            hDerived polys[5] = { de->derivedA };
            char strAboutX[MAX_STRING], strAboutY[MAX_STRING];
            strcpy(strAboutX, ToDisplay(de->v0));
            strcpy(strAboutY, ToDisplay(de->v1));

            char strInitialAngle[MAX_STRING];
            sprintf(strInitialAngle, "%.2f", de->v2);

            char strStepAngle[MAX_STRING];
            sprintf(strStepAngle, "%.2f", de->v3);

            char strN[MAX_STRING];
            sprintf(strN, "%d", de->n);

            char *strs[6] = { NULL, strAboutX, strAboutY,
                                    strInitialAngle, strStepAngle, strN };
            const char *labels[] = {
                "Polygon:", "About x:", "About y:",
                "Initial dtheta:", "Step dtheta:", "Copies:"
            };
            if(uiShowSimpleDialog("Step and Repeat Rotate", 6,
                labels, 0x3e, polys, strs))
            {
                de->derivedA = polys[0];
                de->v0 = FromDisplay(strAboutX);
                de->v1 = FromDisplay(strAboutY);
                de->v2 = atof(strInitialAngle);
                de->v3 = atof(strStepAngle);
                de->n = atoi(strN);
                return TRUE;
            } else {
                return FALSE;
            }
        }

        case DERIVED_SCALE: {
            hDerived polys[2] = { de->derivedA };
            char strScale[MAX_STRING];
            sprintf(strScale, "%.3f", de->v0);

            char *strs[2] = { NULL, strScale };
            const char *labels[] = { "Polygon:", "Scale:", };
            if(uiShowSimpleDialog("Scale", 2, labels, 0x2, polys, strs))
            {
                de->derivedA = polys[0];
                de->v0 = atof(strScale);
                return TRUE;
            } else {
                return FALSE;
            }
            break;
        }

        case DERIVED_MIRROR: {
            hDerived polys[5] = { de->derivedA };
            char strX0[MAX_STRING], strY0[MAX_STRING];
            strcpy(strX0, ToDisplay(de->v0));
            strcpy(strY0, ToDisplay(de->v1));
            char strX1[MAX_STRING], strY1[MAX_STRING];
            strcpy(strX1, ToDisplay(de->v2));
            strcpy(strY1, ToDisplay(de->v3));

            char *strs[5] = { NULL, strX0, strY0, strX1, strY1 };
            const char *labels[] = { "Polygon:", "Point A x:", "Point A y:",
                                           "Point B x:", "Point B y:" };
            if(uiShowSimpleDialog("Mirror About Line Through Points", 5,
                labels, 0x1e, polys, strs))
            {
                de->derivedA = polys[0];
                de->v0 = FromDisplay(strX0);
                de->v1 = FromDisplay(strY0);
                de->v2 = FromDisplay(strX1);
                de->v3 = FromDisplay(strY1);
                return TRUE;
            } else {
                return FALSE;
            }
            break;
        }

        case DERIVED_PERFORATE: {
            hDerived polys[3] = { de->derivedA };
            char strDashLength[MAX_STRING];
            strcpy(strDashLength, ToDisplay(de->v0));
            char strDutyCycle[MAX_STRING];
            sprintf(strDutyCycle, "%.2f", de->v1);

            char *strs[3] = { NULL, strDashLength, strDutyCycle };
            const char *labels[] = { "Polygon:", "Dash Length:", "Duty Cycle:" };
            if(uiShowSimpleDialog("Perforate Lines", 3,
                labels, 0x6, polys, strs))
            {
                de->derivedA = polys[0];
                de->v0 = FromDisplay(strDashLength);
                de->v1 = atof(strDutyCycle);
                return TRUE;
            } else {
                return FALSE;
            }
            break;
        }

        case DERIVED_ROUND: {
            hDerived polys[2] = { de->derivedA };
            char strRadius[MAX_STRING];
            strcpy(strRadius, ToDisplay(de->radius));

            // This should be applied to some points; if no points are selected
            // then the user did something wrong, so warn and stop.
            int j;
            int pts = 0;
            for(j = 0; j < MAX_POINTS_FOR_DERIVED; j++) {
                if(SelectedPoints[j]) pts++;
            }
            if(pts == 0) {
                uiError("No points selected; show points and select some "
                        "before proceeding.");
                // Abort, not meaningful e.g. to round zero points.
                return FALSE;
            }

            char *strs[2] = { NULL, strRadius };
            const char *labels[] = { "Polygon:", "Radius:", };
            if(uiShowSimpleDialog("Round Sharp Corners",
                                            2, labels, 0x2, polys, strs))
            {
                de->derivedA = polys[0];
                de->radius = FromDisplay(strRadius);

                // And we also copy the points.
                de->pts = 0;
                for(j = 0; j < MAX_POINTS_FOR_DERIVED; j++) {
                    if(SelectedPoints[j]) {
                        de->pt[(de->pts)] = SelectedPoints[j];
                        (de->pts)++;
                    }
                }
                return TRUE;
            } else {
                return FALSE;
            }
        }
    
        default:
            oopsnf();
            return FALSE;
    }
}

void MenuDerive(int id)
{
    const char *str = NULL;
    DerivedElement de;
    memset(&de, 0, sizeof(de));

    // Some reasonable choices for the default selection: the last polygons
    // in the list, presumably the most recently created and most relevant.
    int def1p = DL->polys - 1;
    int def2p = DL->polys - 2;
    if(def1p < 0) def1p = 0;
    if(def2p < 0) def2p = 0;
    hDerived def1 = DL->poly[def1p].id;
    hDerived def2 = DL->poly[def2p].id;

    switch(id) {
        case MNU_DERIVE_UNION:
            de.type = DERIVED_UNION;
            de.derivedA = def1;
            de.derivedB = def2;
            str = "Union";
            break;

        case MNU_DERIVE_DIFFERENCE:
            de.type = DERIVED_DIFFERENCE;
            de.derivedA = def1;
            de.derivedB = def2;
            str = "Difference";
            break;

        case MNU_DERIVE_SUPERIMPOSE:
            de.type = DERIVED_SUPERIMPOSE;
            de.derivedA = def1;
            de.derivedB = def2;
            str = "Superimpose";
            break;

        case MNU_DERIVE_OFFSET:
            de.type = DERIVED_OFFSET_CURVE;
            de.derivedA = def1;
            de.radius = 1000;
            str = "Offset_Curve";
            break;

        case MNU_DERIVE_STEP_TRANSLATE:
            de.type = DERIVED_STEP_REPEAT_TRANSLATE;
            de.derivedA = def1;
            de.v0 = 0;
            de.v1 = 0;
            de.v2 = 0;
            de.v3 = 0;
            de.n = 1;
            str = "Step_Translate";
            break;

        case MNU_DERIVE_STEP_ROTATE:
            de.type = DERIVED_STEP_REPEAT_ROTATE;
            de.derivedA = def1;
            de.v0 = 0;
            de.v1 = 0;
            de.v2 = 0;
            de.v3 = 0;
            de.n = 1;
            str = "Step_Rotate";
            break;

        case MNU_DERIVE_SCALE:
            de.type = DERIVED_SCALE;
            de.derivedA = def1;
            de.v0 = 1;
            str = "Scale";
            break;

        case MNU_DERIVE_MIRROR:
            de.type = DERIVED_MIRROR;
            de.derivedA = def1;
            de.v0 = FromDisplay("1");
            de.v1 = 0;
            de.v2 = FromDisplay("-1");
            de.v3 = 0;
            str = "Mirror";
            break;

        case MNU_DERIVE_PERFORATE:
            de.type = DERIVED_PERFORATE;
            de.derivedA = def1;
            de.v0 = 1000;
            de.v1 = 0.50;
            str = "Perforate";
            break;

        case MNU_DERIVE_ROUND:
            de.type = DERIVED_ROUND;
            de.derivedA = def1;
            de.radius = 1000;
            str = "Round";
            break;

        case MNU_EDIT_DELETE_DERIVED: {
            int i = uiGetSelectedDerivedItem();
            if(i < 0) break;
            hDerived hd = DL->poly[i].id;
            DeleteDerivedItem(hd);
            break;
        }
    }

    if(str) {
        // We're adding the element that was built up for us in de; assign
        // it an appropriate name,
        sprintf(de.displayName, "%s_%d", str, CountWithType(de.type) + 1);
        // make sure that it is shown
        de.shown = TRUE;

        // and then give the user the opportunity to either cancel or fill
        // in the rest.
        if(EditDerivedItem(&de)) {
            InsertDerivedItem(&de);
        }
    }

    GenerateDeriveds();
    uiRepaint();
}

//-----------------------------------------------------------------------------
// We keep our own list, in which we record the visibility of each layer,
// separately from its visibility in the sketch. This is often useful, because
// you often want to keep more layers shown in the sketch than in derived,
// because there's extra clutter in derived mode.
//-----------------------------------------------------------------------------
static void CopyShownStateFromPrivate(hLayer layer, BOOL *v)
{
    int j;
    for(j = 0; j < MAX_LAYERS; j++) {
        if(LayerShown[j].layer == layer) {
            *v = LayerShown[j].shown;
            return;
        }
    }
    // otherwise *v is left untouched.
}
static void SaveShownStateForPoly(int i)
{
    if(!IS_DERIVED_FROM_LAYER(DL->poly[i].id)) return;

    BOOL shown = DL->poly[i].shown;
    hLayer layer = LAYER_FROM_DERIVED(DL->poly[i].id);

    int j;
    // If an entry exists, then update it.
    for(j = 0; j < MAX_LAYERS; j++) {
        if(LayerShown[j].layer == layer) {
            LayerShown[j].shown = shown;
            return;
        }
    }
    // If no entry exists and there's space, then create one.
    for(j = 0; j < MAX_LAYERS; j++) {
        if(!LayerShown[j].layer) {
            LayerShown[j].layer = layer;
            LayerShown[j].shown = shown;
            return;
        }
    }
}
static void SaveShownStateToPrivate(void)
{
    int i;
    for(i = 0; i < DL->polys; i++) {
        if(DL->poly[i].type == DERIVED_COPY_FROM_LAYER) {
            SaveShownStateForPoly(i);
        }
    }
}

//-----------------------------------------------------------------------------
// For dealing with our treeview list of derived items: mostly this is just
// a matter of keeping track of which are visible, and repainting when
// that changes. A lot of callbacks from the GUI code here.
//-----------------------------------------------------------------------------
static void DerivedUpdateListBold(void)
{
    int i, j;
    for(i = 0; i < DL->polys; i++) {
        uiBoldDerivedItem(i, DL->poly[i].shown);

        // For something other than a layer, update the visibility of the
        // derived item in a table that doesn't get blown away every time
        // we refresh.
        if(!IS_DERIVED_FROM_LAYER(DL->poly[i].id)) {
            for(j = 0; j < DL->reqs; j++) {
                if(DL->req[j].id == DL->poly[i].id) {
                    DL->req[j].shown = DL->poly[i].shown;
                    break;
                }
            }
            if(j >= DL->reqs) {
                oopsnf();
                return;
            }
        }
    }
    SaveShownStateToPrivate();
    uiRepaint();
}
void DerivedItemsListToggleShown(int i)
{
    if(i < 0 || i >= DL->polys) {
        oopsnf();
        return;
    }

    DL->poly[i].shown = !(DL->poly[i].shown);

    DerivedUpdateListBold();
}
void DerivedItemsListEdit(int i)
{
    if(i < 0 || i >= DL->polys) {
        oopsnf();
        return;
    }

    hDerived hd = DL->poly[i].id;

    if(IS_DERIVED_FROM_LAYER(hd)) {
        uiError("Can't edit polygon that is copied from sketch layer.");
        return;
    }

    int j;
    for(j = 0; j < DL->reqs; j++) {
        if(DL->req[j].id == hd) {
            UndoRemember();
            EditDerivedItem(&(DL->req[j]));
            break;
        }
    }
    if(j >= DL->reqs) {
        oopsnf();
        return;
    }

    GenerateDeriveds();
}
void ButtonShowAllDerivedItems(void)
{
    int i;
    for(i = 0; i < DL->polys; i++) {
        DL->poly[i].shown = TRUE;
    }
    DerivedUpdateListBold();
}
void ButtonHideAllDerivedItems(void)
{
    int i;
    for(i = 0; i < DL->polys; i++) {
        DL->poly[i].shown = FALSE;
    }
    DerivedUpdateListBold();
}

//=============================================================================
// Routines to generate the derived polygons, based on the requested derived
// operations. The input is the source polygons, that the user sketched;
// so we get called to regenerate every time those change.
//=============================================================================

//-----------------------------------------------------------------------------
// Erase DL->poly[], freeing anything that was allocated dynamically and then
// zeroing it back out.
//-----------------------------------------------------------------------------
static void EraseAllPolys(void)
{
    int i, j;
    for(i = 0; i < DL->polys; i++) {
        for(j = 0; j < DL->poly[i].p.curves; j++) {
            DFree(DL->poly[i].p.curve[j].pt);
        }
    }

    memset(DL->poly, 0, sizeof(DL->poly));
    DL->polys = 0;
}

void GenerateDeriveds(void)
{
    int i, j;

    EraseAllPolys();

    // We are guaranteed by the definition of MAX_DERIVED_ELEMENTS and
    // MAX_LAYERS that we can't overrun any buffers now without already
    // having overrun a previous one.

    // To start, each layer in the sketch view generates a poly. These
    // are our source material.
    for(i = 0; i < SK->layer.n; i++) {
        hLayer lr = SK->layer.list[i].id;
       
        j = DL->polys;

        DL->poly[j].type = DERIVED_COPY_FROM_LAYER;
        sprintf(DL->poly[j].displayName, "Layer %s",
                                    SK->layer.list[i].displayName);

        BOOL leftovers;
        PolygonAssemble(&(DL->poly[j].p), SK->pwl, SK->pwls, lr, &leftovers);
        if(leftovers) {
            sprintf(DL->poly[j].infoA, "Not Closed Curve!");
        } else {
            sprintf(DL->poly[j].infoA, "Copied Layer, OK");
        }
        // By default, copy visibility of layer from sketch
        DL->poly[j].shown = LayerIsShown(lr);
        // but use our own table if we have an entry.
        CopyShownStateFromPrivate(lr, &(DL->poly[j].shown));

        DL->poly[j].id = DERIVED_FROM_LAYER(lr);

        DL->polys = (j + 1);
    }

    // Next, we perform each requested derived item. Each of those generates
    // a poly, too.
    for(i = 0; i < DL->reqs; i++) {
        DerivedElement *d = &(DL->req[i]);

        j = DL->polys;

        DL->poly[j].type = d->type;
        DL->poly[j].id = d->id;
        DL->poly[j].shown = d->shown;

        strcpy(DL->poly[j].displayName, d->displayName);
        char *infoA = DL->poly[j].infoA;
        char *infoB = DL->poly[j].infoB;
        char *infoC = DL->poly[j].infoC;

        strcpy(infoA, "");
        strcpy(infoB, "");
        strcpy(infoC, "");

        switch(d->type) {
            case DERIVED_UNION:
            case DERIVED_DIFFERENCE:
            case DERIVED_SUPERIMPOSE: {
                DPolygon *pa = GetPolygonFrom(d->derivedA);
                DPolygon *pb = GetPolygonFrom(d->derivedB);

                if(!pa || !pb) {
                    strcpy(infoA, "source poly missing!");
                } else {
                    if(d->type == DERIVED_UNION) {
                        PolygonUnion(&(DL->poly[j].p), pa, pb);
                    } else if(d->type == DERIVED_DIFFERENCE) {
                        PolygonDifference(&(DL->poly[j].p), pa, pb);
                    } else if(d->type == DERIVED_SUPERIMPOSE) {
                        PolygonSuperimpose(&(DL->poly[j].p), pa, pb);
                    }
                }
                break;
            }

            case DERIVED_SCALE:
            case DERIVED_STEP_REPEAT_TRANSLATE:
            case DERIVED_STEP_REPEAT_ROTATE:
            case DERIVED_MIRROR:
            case DERIVED_PERFORATE:
            case DERIVED_OFFSET_CURVE: {
                DPolygon *pa = GetPolygonFrom(d->derivedA);

                if(!pa) {
                    strcpy(infoA, "source poly missing!");
                } else {
                    if(d->type == DERIVED_SCALE) {
                        PolygonScale(&(DL->poly[j].p), pa, d->v0);
                        sprintf(infoA, "factor %.4f", d->v0);

                    } else if(d->type == DERIVED_OFFSET_CURVE) {
                        PolygonOffset(&(DL->poly[j].p), pa, d->radius);
                        sprintf(infoA, "radius %s", ToDisplay(d->radius));

                    } else if(d->type == DERIVED_STEP_REPEAT_TRANSLATE) {
                        PolygonStepTranslating(&(DL->poly[j].p), pa,
                            d->v0, d->v1, d->v2, d->v3, d->n);
                        sprintf(infoA, "initial (%s, %s)",
                            ToDisplay(d->v0), ToDisplay(d->v1));
                        sprintf(infoB, "step (%s, %s)",
                            ToDisplay(d->v2), ToDisplay(d->v3));
                        sprintf(infoC, "%d times", d->n);

                    } else if(d->type == DERIVED_STEP_REPEAT_ROTATE) {
                        PolygonStepRotating(&(DL->poly[j].p), pa,
                            d->v0, d->v1,
                            (d->v2*PI)/180, (d->v3*PI)/180, d->n);
                        sprintf(infoA, "about (%s, %s)",
                            ToDisplay(d->v0), ToDisplay(d->v1));
                        sprintf(infoB, "step %.1f°", d->v2);
                        sprintf(infoC, "%d times", d->n);
                    } else if(d->type == DERIVED_MIRROR) {
                        PolygonMirror(&(DL->poly[j].p), pa,
                            d->v0, d->v1, d->v2, d->v3);
                        sprintf(infoA, "about line through");
                        sprintf(infoB, "  (%s, %s)",
                            ToDisplay(d->v0), ToDisplay(d->v1));
                        sprintf(infoC, "  (%s, %s)",
                            ToDisplay(d->v2), ToDisplay(d->v3));
                    } else if(d->type = DERIVED_PERFORATE) {
                        PolygonPerforate(&(DL->poly[j].p), pa,
                            d->v0, d->v1);
                        sprintf(infoA, "dash len %s", ToDisplay(d->v0));
                        sprintf(infoB, "duty cycle %.3f", fabs(d->v1));
                        if(d->v1 < 0) {
                            strcpy(infoC, "no yes no (-tive duty)");
                        } else {
                            strcpy(infoC, "yes no yes (+tive duty)");
                        }
                    } else {
                        oopsnf();
                    }
                }
                break;
            }

            case DERIVED_ROUND: {
                DPolygon *pa = GetPolygonFrom(d->derivedA);

                if(!pa) {
                    strcpy(infoA, "source poly missing!");
                } else {
                    sprintf(infoA, "radius %s", ToDisplay(d->radius));
                    sprintf(infoB, "%d points", d->pts);
                    PolygonRoundCorners(&(DL->poly[j].p), pa, d->radius,
                        d->pt, d->pts);
                }
                break;
            }

            default:
                dbp("type is %d", d->type);
                oopsnf();
                break;
        }

        DL->poly[j].shown = d->shown;
        DL->polys = (j + 1);
    }

    // Display our list of copied and generated polygons in the treeview list.
    uiClearDerivedItemsList();
    for(i = 0; i < DL->polys; i++) {
        char *name = DL->poly[i].displayName;

        char *sA = DL->poly[i].infoA;
        char *sB = DL->poly[i].infoB;
        char *sC = DL->poly[i].infoC;

        uiAddToDerivedItemsList(i, name, sA, sB, sC);
    }
    DerivedUpdateListBold();
}

//=============================================================================
// Routines to draw the display, and handle mouse input.
//=============================================================================


void DerivedMouseMoved(int x, int y,
                            BOOL leftDown, BOOL rightDown, BOOL centerDown)
{
    hPoint closest;

    double xf = toMicronsX(x), yf = toMicronsY(y);

    int i;
    double dMin = VERY_POSITIVE;
    for(i = 0; i < SK->points; i++) {
        if(!PointExistsInSketch(SK->point[i])) continue;

        double xp, yp;
        EvalPoint(SK->point[i], &xp, &yp);
        double d = Distance(xf, yf, xp, yp);
        if(d < dMin) {
            dMin = d;
            closest = SK->point[i];
        }
    }
    if(dMin > toMicronsNotAffine(5)) {
        closest = 0;
    }

    if(closest != HoveredPoint) {
        HoveredPoint = closest;
        uiRepaint();
    }
}

void DerivedLeftButtonDown(int x, int y)
{
    if(!HoveredPoint) return;

    // If the point is already selected, then unselect it.
    int i;
    for(i = 0; i < MAX_POINTS_FOR_DERIVED; i++) {
        if(SelectedPoints[i] == HoveredPoint) {
            SelectedPoints[i] = 0;
            HoveredPoint = 0;
            uiRepaint();
            return;
        }
    }

    // If the point is not selected, then select it.
    for(i = 0; i < MAX_POINTS_FOR_DERIVED; i++) {
        if(!SelectedPoints[i]) {
            SelectedPoints[i] = HoveredPoint;
            uiRepaint();
            return;
        }
    }
    // And if we're out of space then give up.
}

void MenuDerivedUnselect(int id)
{
    switch(id) {
        case MNU_EDIT_UNSELECT_ALL_POINTS:
            int i;
            for(i = 0; i < MAX_POINTS_FOR_DERIVED; i++) {
                SelectedPoints[i] = 0;
            }
            uiRepaint();
            break;
    }
}
static void mark(double xf, double yf)
{
    int x = toPixelsX(xf);
    int y = toPixelsY(yf);

    int d = 4;
    PltRect(x - d, y - (d + 1), x + d + 1, y + d);
}
static void DrawPolygon(DPolygon *p)
{
    int i, j;
    for(i = 0; i < p->curves; i++) {
        DClosedCurve *c = &(p->curve[i]);

        PltMoveToMicrons(c->pt[0].x, c->pt[0].y);
//        mark(c->pt[0].x, c->pt[0].y);
        for(j = 1; j < p->curve[i].pts; j++) {
            PltLineToMicrons(c->pt[j].x, c->pt[j].y);
//            mark(c->pt[j-1].x, c->pt[j-1].y);
        }
    }
}
static void DrawPoint(hPoint pt)
{
    if(!PointExistsInSketch(pt)) return;

    double xf, yf;
    EvalPoint(pt, &xf, &yf);
    int x = toPixelsX(xf);
    int y = toPixelsY(yf);

    int d = 3;
    PltRect(x - d, y - (d + 1), x + d + 1, y + d);
}
void DrawDerived(void)
{
    int i;

    for(i = 0; i < DL->polys; i++) {
        if(DL->poly[i].shown) {
            PltSetColor((i % 5) + 1);
            DrawPolygon(&(DL->poly[i].p));
        }
    }

    if(uiPointsShownInDeriveMode()) {
        for(i = 0; i < SK->points; i++) {
            if(ENTITY_FROM_POINT(SK->point[i]) == REFERENCE_ENTITY) {
                PltSetColor(REFERENCES_COLOR);
            } else {
                PltSetColor(DATUM_COLOR);
            }
            DrawPoint(SK->point[i]);
        }
        if(HoveredPoint) {
            PltSetColor(HOVER_COLOR);
            DrawPoint(HoveredPoint);
        }
        for(i = 0; i < MAX_POINTS_FOR_DERIVED; i++) {
            if(!SelectedPoints[i]) continue;

            PltSetColor(SELECTED_COLOR);
            DrawPoint(SelectedPoints[i]);
        }
    }
}

void SwitchToDeriveMode(void)
{
    // Use a finer-than-normal chord tolerance, since these are the piecewise
    // linear segments that we are generating for export.
    double chordTolPixels = 0.25;
    double chordTol = 
        toMicronsNotAffine((int)(chordTolPixels*100))/100.0;
    if(chordTol > 20) chordTol = 20;

    GenerateParametersPointsLines();
    GenerateCurvesAndPwls(-1);
    HoveredPoint = 0;

    // Clear out the list of selected points too.
    int i;
    for(i = 0; i < MAX_POINTS_FOR_DERIVED; i++) {
        SelectedPoints[i] = 0;
    }

    GenerateDeriveds();
    uiRepaint();
}
