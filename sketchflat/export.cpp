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
// Given a polygon, output that polygon in some useful file format.
//
// Jonathan Westhues, June 2007
//-----------------------------------------------------------------------------
#include "sketchflat.h"

// Return codes for the export functions.
#define RESULT_OKAY         0
#define RESULT_CANCELLED    1
#define RESULT_FAILED       2

static double GetScale(void)
{
    return uiUseInches() ? (25.4*1000) : 1000;
}

static int ExportAsDxf(char *file)
{
    FILE *f = fopen(file, "w");
    if(!f) return RESULT_FAILED;

    // Some software, like Adobe Illustrator, insists on a header.
    fprintf(f,
"  999\n"
"file created by SketchFlat\n"
"  0\n"
"SECTION\n"
"  2\n"
"HEADER\n"
"  9\n"
"$ACADVER\n"
"  1\n"
"AC1006\n"
"  9\n"
"$INSBASE\n"
"  10\n"
"0.0\n"
"  20\n"
"0.0\n"
"  30\n"
"0.0\n"
"  9\n"
"$EXTMIN\n"
"  10\n"
"0.0\n"
"  20\n"
"0.0\n"
"  9\n"
"$EXTMAX\n"
"  10\n"
"10000.0\n"
"  20\n"
"10000.0\n"
"  0\n"
"ENDSEC\n");

    // Now begin the entities, which are just line segments reproduced from
    // our piecewise linear curves.
    fprintf(f,
"  0\n"
"SECTION\n"
"  2\n"
"ENTITIES\n");

    double scale = GetScale();

    int a, i, j;
    for(a = 0; a < DL->polys; a++) {
        if(!DL->poly[a].shown) continue;
        DPolygon *p = &(DL->poly[a].p);

        for(i = 0; i < p->curves; i++) {
            DClosedCurve *c = &(p->curve[i]);

            for(j = 1; j < c->pts; j++) {
                fprintf(f,
"  0\n"
"LINE\n"
"  8\n"     // Layer code
"%d\n"
"  10\n"    // xA
"%.6f\n"
"  20\n"    // yA
"%.6f\n"
"  30\n"    // zA
"%.6f\n"
"  11\n"    // xB
"%.6f\n"
"  21\n"    // yB
"%.6f\n"
"  31\n"    // zB
"%.6f\n",
                    0,
                    c->pt[j-1].x / scale, c->pt[j-1].y / scale, 0.0,
                    c->pt[j  ].x / scale, c->pt[j  ].y / scale, 0.0);
            }
        }
    }

    fprintf(f,
"  0\n"
"ENDSEC\n"
"  0\n"
"EOF\n" );

    fclose(f);
    return RESULT_OKAY;
}

static int HpglUnits(double v)
{
    // The standard HPGL units are 25 um, or 40 units per mm.
    return toint(v / 25);
}
static int ExportAsHpgl(char *file)
{
    FILE *f = fopen(file, "w");
    if(!f) return RESULT_FAILED;

    fprintf(f, "IN;\n");

    int a, i, j;
    for(a = 0; a < DL->polys; a++) {
        if(!DL->poly[a].shown) continue;
        DPolygon *p = &(DL->poly[a].p);
        
        fprintf(f, "LT;");

        for(i = 0; i < p->curves; i++) {
            DClosedCurve *c = &(p->curve[i]);

            fprintf(f, "SP1;");
            fprintf(f, "PU%d,%d;\n",
                        HpglUnits(c->pt[0].x), HpglUnits(c->pt[0].y));
            for(j = 1; j < c->pts; j++) {
                fprintf(f, "PD%d,%d;\n",
                        HpglUnits(c->pt[j].x), HpglUnits(c->pt[j].y));
            }
        }
    }

    fclose(f);
    return RESULT_OKAY;
}

static int ExportAsGCode(char *file)
{
    FILE *f = fopen(file, "w");
    if(!f) return RESULT_FAILED;

    char depthStr[MAX_STRING];
    strcpy(depthStr, ToDisplay(1000));

    char passesStr[MAX_STRING] = "1";
    char feed[MAX_STRING] = "10";
    char plunge[MAX_STRING] = "10";

    char *strs[] = { depthStr, passesStr, feed, plunge };
    const char *labels[] = { "Total depth:", "Passes:", "Feed:", "Plunge feed:" };
    if(!uiShowSimpleDialog("Export as G Code", 4, labels, 0xf, NULL, strs)) {
        fclose(f);
        return RESULT_CANCELLED;
    }

    int passes = atoi(passesStr);
    if(passes > 1000 || passes < 1) return RESULT_FAILED;

    double totalDepth = FromDisplay(depthStr);
    char clearance[MAX_STRING];
    sprintf(clearance, "%s%s", 
        (totalDepth < 0) ? "" : "-", 
        ToDisplay(uiUseInches() ? 25.4*1e3/8 : 5e3));


    int pass, a, i, j;
    for(pass = 0; pass < passes; pass++) {
        double depth = (totalDepth/passes)*(pass+1);

        for(a = 0; a < DL->polys; a++) {
            if(!DL->poly[a].shown) continue;
            DPolygon *p = &(DL->poly[a].p);
            
            for(i = 0; i < p->curves; i++) {
                DClosedCurve *c = &(p->curve[i]);

                fprintf(f, "G00 X%s Y%s\n",
                        ToDisplay(c->pt[0].x), ToDisplay(c->pt[0].y));
                fprintf(f, "G01 Z%s F%s\n", ToDisplay(depth), plunge);
                for(j = 1; j < c->pts; j++) {
                    fprintf(f, "G01 X%s Y%s F%s\n",
                            ToDisplay(c->pt[j].x), ToDisplay(c->pt[j].y),
                            feed);
                    fflush(f);
                }
                fprintf(f, "G00 Z%s\n", clearance);
                fflush(f);
            }
        }
    }

    fclose(f);
    return RESULT_OKAY;
}

void MenuExport(int id)
{
    const char *filter;
    const char *defExt;
#define FILTER_ENDING "\0All Files\0*\0\0"
    switch(id) {
        case MNU_EXPORT_DXF:
            filter = "DXF Files (*.dxf)\0*.dxf" FILTER_ENDING;
            defExt = "dxf";
            break;
        case MNU_EXPORT_HPGL:
            filter = "HPGL Files (*.plt,*.hpgl)\0*.plt,*.hpgl" FILTER_ENDING;
            defExt = "plt";
            break;
        case MNU_EXPORT_G_CODE:
            filter = "G Code Files (*.txt)\0*.txt" FILTER_ENDING;
            defExt = "txt";
            break;
        default:
            oopsnf();
            return;
    }
    
    char dest[MAX_PATH] = "";
    if(!uiGetSaveFile(dest, defExt, filter)) return;

    int res = RESULT_FAILED;
    switch(id) {
        case MNU_EXPORT_DXF:
            res = ExportAsDxf(dest);
            break;

        case MNU_EXPORT_HPGL:
            res = ExportAsHpgl(dest);
            break;

        case MNU_EXPORT_G_CODE:
            res = ExportAsGCode(dest);
            break;
    }
    if(res == RESULT_FAILED) {
        uiError("Export failed.");
    }
}

