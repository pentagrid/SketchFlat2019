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
// The portion of the graphical user interface that deals directly with the
// sketch: displaying it on-screen, and editing it with the mouse, menus,
// and keyboard.
// Jonathan Westhues, April 2007
//-----------------------------------------------------------------------------
#include "sketchflat.h"

// The mapping from the curve's coordinate system to our own coordinate
// system.
static struct {
    double      scale;      // pixels per micron
    struct {
        double      x;
        double      y;
    }           offset;     // from (0, 0) to center of displayed
} View;

static int LastMouseX, LastMouseY;
static int CenterClickX, CenterClickY;
static double OffsetAtCenterClickX, OffsetAtCenterClickY;
int MouseLeftDownX, MouseLeftDownY;

//-----------------------------------------------------------------------------
// Convert from screen coordinates (in pixels) to drawing coordinates (in
// microns).
//-----------------------------------------------------------------------------
double toMicronsX(int x)
{
    return (x / View.scale) + View.offset.x;
}
double toMicronsY(int y)
{
    return (y / View.scale) + View.offset.y;
}
double toMicronsNotAffine(int r)
{
    return (r / View.scale);
}
// And the other way too
int toPixelsX(double x)
{
    return toint((x - View.offset.x) * View.scale);
}
int toPixelsY(double y)
{
    return toint((y - View.offset.y) * View.scale);
}
int toPixelsNotAffine(double r)
{
    return toint(r * View.scale);
}

//-----------------------------------------------------------------------------
// Convert from microns to the user-displayed units (inch, mm, whatever).
// These work with strings, because the number formatting (e.g. how many
// places after the decimal point) will change depending on the units.
//-----------------------------------------------------------------------------
char *ToDisplay(double v)
{
    static int WhichBuf;
    static char Bufs[8][128];

    WhichBuf++;
    if(WhichBuf >= 8 || WhichBuf < 0) WhichBuf = 0;

    char *s = Bufs[WhichBuf];

    if(uiUseInches()) {
        sprintf(s, "%.3f", v/(25.4*1000));
    } else {
        sprintf(s, "%.2f", v/1000);
    }
    return s;
}
double FromDisplay(const char *v)
{
    if(uiUseInches()) {
        return atof(v)*(25.4*1000);
    } else {
        return atof(v)*1000;
    }
}

//-----------------------------------------------------------------------------
// If the units change, then we have to refresh a few things.
//-----------------------------------------------------------------------------
void UpdateAfterUnitChange(void)
{
    if(uiInSketchMode()) {
        // A selected item is described with various measurements, that
        // have units.
        UpdateMeasurements();
    } else {
        // Force a regenerate of the derived list, since that contains
        // values with units.
        SwitchToDeriveMode();
    }
}

//-----------------------------------------------------------------------------
// Update the text in the status bar; show the current mouse position, and
// the current operation in progress in sketch mode.
//-----------------------------------------------------------------------------
void UpdateStatusBar(void)
{
    char s[MAX_STRING];
    char solving[MAX_STRING];
    BOOL red;

    SketchGetStatusBarDescription(s, solving, &red);

    uiSetStatusBarText(solving, red,
        ToDisplay(toMicronsX(LastMouseX)), ToDisplay(toMicronsY(LastMouseY)), 
        s);
}

void KeyPressed(int key)
{
    if(uiInSketchMode()) {
        SketchKeyPressed(key);
    }
}

void ScrollWheelMoved(int x, int y, int delta)
{
    // If the user is modifying a dimension, then they don't get to do
    // anything else until that's done.
    if(uiTextEntryBoxIsVisible()) return;

    double oldMouseX = toMicronsX(x);
    double oldMouseY = toMicronsY(y);

    if(delta < 0) {
        View.scale *= 1.3;
    } else {
        View.scale /= 1.3;
    }

    double newMouseX = toMicronsX(x);
    double newMouseY = toMicronsY(y);

    // Fix how the viewport is panned, so that mouse is still over whatever
    // it was over before.
    View.offset.x -= (newMouseX - oldMouseX);
    View.offset.y -= (newMouseY - oldMouseY);

    uiRepaint();
}

void LeftButtonDown(int x, int y)
{
    MouseLeftDownX = x;
    MouseLeftDownY = y;

    if(uiInSketchMode()) {
        SketchLeftButtonDown(x, y);
    } else {
        DerivedLeftButtonDown(x, y);
    }
}

void LeftButtonUp(int x, int y)
{
    if(uiInSketchMode()) {
        SketchLeftButtonUp(x, y);
    }
}

void LeftButtonDblclk(int x, int y)
{
    if(uiInSketchMode()) {
        SketchLeftButtonDblclk(x, y);
    }
}

void CenterButtonDown(int x, int y)
{
    if(uiTextEntryBoxIsVisible()) return;

    CenterClickX = x;
    CenterClickY = y;

    OffsetAtCenterClickX = View.offset.x;
    OffsetAtCenterClickY = View.offset.y;
}

void MouseMoved(int x, int y, BOOL leftDown, BOOL rightDown, BOOL centerDown)
{
    if(uiTextEntryBoxIsVisible()) return;

    // Center-dragging pans.
    if(centerDown) {
        View.offset.x = OffsetAtCenterClickX + (CenterClickX - x) / View.scale;
        View.offset.y = OffsetAtCenterClickY + (CenterClickY - y) / View.scale;
        uiRepaint();
        return;
    }

    if(uiInSketchMode()) {
        SketchMouseMoved(x, y, leftDown, rightDown, centerDown);
    } else {
        DerivedMouseMoved(x, y, leftDown, rightDown, centerDown);
    }

    LastMouseX = x;
    LastMouseY = y;
    UpdateStatusBar();
}

void ZoomToFit(void)
{
    GenerateParametersPointsLines();
    GenerateCurvesAndPwls(-1);

    double xMax = VERY_NEGATIVE;
    double xMin = VERY_POSITIVE;
    double yMax = VERY_NEGATIVE;
    double yMin = VERY_POSITIVE;

    int i;

    if(uiInSketchMode()) {
        for(i = 0; i < SK->pwls; i++) {
            SketchPwl *p = &(SK->pwl[i]);
            int j;
            for(j = 0; j < 2; j++) {
                double x = (j == 0) ? p->x0 : p->x1;
                double y = (j == 0) ? p->y0 : p->y1;
                
                if(x > xMax) xMax = x;
                if(x < xMin) xMin = x;
                if(y > yMax) yMax = y;
                if(y < yMin) yMin = y;
            }
        }

        for(i = 0; i < SK->points; i++) {
            double x, y;
            EvalPoint(SK->point[i], &x, &y);

            if(x > xMax) xMax = x;
            if(x < xMin) xMin = x;
            if(y > yMax) yMax = y;
            if(y < yMin) yMin = y;
        }

        for(i = 0; i < SK->constraints; i++) {
            SketchConstraint *c = &(SK->constraint[i]);

            if(ConstraintHasLabelAssociated(c)) {
                double x, y;
                ForDrawnConstraint(GET_LABEL_LOCATION, c, &x, &y);

                if(x > xMax) xMax = x;
                if(x < xMin) xMin = x;
                if(y > yMax) yMax = y;
                if(y < yMin) yMin = y;
            }
        }
    } else {
        for(i = 0; i < DL->polys; i++) {
            DPolygon *p = &(DL->poly[i].p);
            int j;
            for(j = 0; j < p->curves; j++) {
                DClosedCurve *c = &(p->curve[j]);
                int k;
                for(k = 0; k < c->pts; k++) {
                    double x = c->pt[k].x;
                    double y = c->pt[k].y;

                    if(x > xMax) xMax = x;
                    if(x < xMin) xMin = x;
                    if(y > yMax) yMax = y;
                    if(y < yMin) yMin = y;
                }
            }
        }
    }

    if((xMax - xMin < 1000) || (yMax - yMin < 1000)) {
        // These are unlikely to occur unless the sketch is empty; in that
        // case, choose a reasonable default scale.
        View.scale = 72.0/(25.4*1000);  // Start at 72 dpi, approximately 1:1.
        View.offset.x = 0;
        View.offset.y = 0;
    } else {
        int xViewMax;
        int yViewMax;
        int xViewMin;
        int yViewMin;
        PltGetRegion(&xViewMin, &yViewMin, &xViewMax, &yViewMax);

        double scaleX = (xViewMax - xViewMin) / (xMax - xMin);
        double scaleY = (yViewMax - yViewMin) / (yMax - yMin);
        // Whichever of the two is smaller is the correct scale
        View.scale = (scaleX < scaleY) ? scaleX : scaleY;
        View.scale *= 0.85;

        View.offset.x = (xMax + xMin) / 2;
        View.offset.y = (yMax + yMin) / 2;
    }

    uiRepaint();
}

void MenuZoom(int id)
{
    if(uiTextEntryBoxIsVisible()) uiHideTextEntryBox();

    switch(id) {
        case MNU_VIEW_ZOOM_IN:
            View.scale *= 1.2;
            uiRepaint();
            break;

        case MNU_VIEW_ZOOM_OUT:
            View.scale /= 1.2;
            uiRepaint();
            break;

        case MNU_VIEW_ZOOM_TO_FIT:
            ZoomToFit();
            break;

        default:
            oopsnf();
            break;
    }
}

//-----------------------------------------------------------------------------
// Draw whatever graphical stuff should appear on-screen.
//-----------------------------------------------------------------------------
void PaintWindow(void)
{
    if(uiInSketchMode()) {
        DrawSketch();
    } else {
        DrawDerived();
    }
}

