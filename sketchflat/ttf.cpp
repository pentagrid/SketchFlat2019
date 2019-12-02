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
// Routines to read a TrueType font file and spit out the vectors. We are
// given a string, and a baseline and height along which to plot it. Our
// job is to generate lines and quadratic cubes from this, that can be
// added to the sketch.
// Jonathan Westhues, April 2007
//-----------------------------------------------------------------------------
#include "sketchflat.h"

// Definitions for the TrueType fonts that we store in memory.

#define MAX_TTF_FONTS_IN_MEMORY 128
#define MAX_POINTS_IN_GLYPH 512
#define MAX_GLYPHS 1024

// A glyph within a font
typedef struct {
    struct {
        BYTE        onCurve;
        BYTE        lastInContour;
        SWORD       x;
        SWORD       y;
    }           pts[MAX_POINTS_IN_GLYPH];
    int         n;
    int         xMax;
    int         xMin;
    int         leftSideBearing;
    int         advanceWidth;
} TtfGlyph;

// A font, which is a collection of glyphs.
typedef struct {
    WORD        useGlyph[256];
    TtfGlyph    glyphs[MAX_GLYPHS];
    int         scale;
} Ttf;

// The list of fonts that we have loaded in to memory.
typedef struct {
    char    file[MAX_STRING];
    Ttf     font;
} TtfStored;

static TtfStored *TtfsStored[MAX_TTF_FONTS_IN_MEMORY];

static FILE *OpenTtf;
static Ttf *FN;
static jmp_buf TtfErrorBuf;

//-----------------------------------------------------------------------------
// Report any kind of TTF error. This may be called from any function that
// was called from TtfLoadFromFile(), and will cause TtfLoadFromFile() to
// return FALSE.
//-----------------------------------------------------------------------------
static void TtfOops(void)
{
    longjmp(TtfErrorBuf, 1);
}

//-----------------------------------------------------------------------------
// Get a single character from the open .ttf file; EOF is an error, since
// we can always see that coming.
//-----------------------------------------------------------------------------
static int Getc(void)
{
    int c = fgetc(OpenTtf);
    if(c < 0) {
        dbp("out of characters");
        TtfOops();
    }
    return c;
}

//-----------------------------------------------------------------------------
// Helpers to get 1, 2, or 4 bytes from the .ttf file. Big endian.
//-----------------------------------------------------------------------------
#define GetBYTE Getc
static int GetWORD(void)
{
    BYTE b0, b1;
    b1 = Getc();
    b0 = Getc();

    return (b1 << 8) | b0;
}
static int GetDWORD(void)
{
    BYTE b0, b1, b2, b3;
    b3 = Getc();
    b2 = Getc();
    b1 = Getc();
    b0 = Getc();

    return (b3 << 24) | (b2 << 16) | (b1 << 8) | b0;
}

//-----------------------------------------------------------------------------
// Load a glyph from the .ttf file into memory. Assumes that the .ttf file
// is already seeked to the correct location, and writes the result to
// FN->glyphs[index]
//-----------------------------------------------------------------------------
static void LoadGlyph(int index)
{
    int i;

    WORD  contours          = GetWORD();
    SWORD xMin              = GetWORD();
    SWORD yMin              = GetWORD();
    SWORD xMax              = GetWORD();
    SWORD yMax              = GetWORD();

    if(FN->useGlyph['A'] == index) {
        FN->scale = (1024*1024) / yMax;
    }

//    printf("bounding: (%d, %d)-(%d, %d)\n", xMin, yMin, xMax, yMax);

//    printf("glyph %d: contours = %d\n", index, contours);

    if(contours > 0) {
        WORD endPointsOfContours[256];
        if(contours > arraylen(endPointsOfContours)) {
            return;
        }

        for(i = 0; i < contours; i++) {
            endPointsOfContours[i] = GetWORD();
//            printf("    contour %d ends at %d\n",
//                i, endPointsOfContours[i]);
        }
        WORD totalPts = endPointsOfContours[i-1] + 1;

        WORD instructionLength = GetWORD();
        for(i = 0; i < instructionLength; i++) {
            // We can ignore the instructions, since we're doing vector
            // output.
            (void)GetBYTE();
        }

        BYTE  flags[MAX_POINTS_IN_GLYPH];
        SWORD x    [MAX_POINTS_IN_GLYPH];
        SWORD y    [MAX_POINTS_IN_GLYPH];

        if(totalPts > MAX_POINTS_IN_GLYPH) {
            dbp("too many points on glyph: %d", totalPts);
            TtfOops();
        }

        // Flags, that indicate format of the coordinates
#define FLAG_ON_CURVE           (1 << 0)
#define FLAG_DX_IS_BYTE         (1 << 1)
#define FLAG_DY_IS_BYTE         (1 << 2)
#define FLAG_REPEAT             (1 << 3)
#define FLAG_X_IS_SAME          (1 << 4)
#define FLAG_X_IS_POSITIVE      (1 << 4)
#define FLAG_Y_IS_SAME          (1 << 5)
#define FLAG_Y_IS_POSITIVE      (1 << 5)
        for(i = 0; i < totalPts; i++) {
            flags[i] = GetBYTE();
            if(flags[i] & FLAG_REPEAT) {
//                printf("flag repeat\n");
                int n = GetBYTE();
                BYTE f = flags[i];
                int j;
                for(j = 0; j < n; j++) {
                    i++;
                    flags[i] = f;
                }
            }
        }

        // x coordinates
        SWORD xa = 0;
        for(i = 0; i < totalPts; i++) {
            if(flags[i] & FLAG_DX_IS_BYTE) {
                BYTE v = GetBYTE();
                if(flags[i] & FLAG_X_IS_POSITIVE) {
                    xa += v;
                } else {
                    xa -= v;
                }
            } else {
                if(flags[i] & FLAG_X_IS_SAME) {
                    // no change
                } else {
                    SWORD d = GetWORD();
                    xa += d;
                }
            }
            x[i] = xa;
        }

        // y coordinates
        SWORD ya = 0;
        for(i = 0; i < totalPts; i++) {
            if(flags[i] & FLAG_DY_IS_BYTE) {
                BYTE v = GetBYTE();
                if(flags[i] & FLAG_Y_IS_POSITIVE) {
                    ya += v;
                } else {
                    ya -= v;
                }
            } else {
                if(flags[i] & FLAG_Y_IS_SAME) {
                    // no change
                } else {
                    SWORD d = GetWORD();
                    ya += d;
                }
            }
            y[i] = ya;
        }
   
        TtfGlyph *g = &(FN->glyphs[index]);
        int contour = 0;
        for(i = 0; i < totalPts; i++) {
            g->pts[i].x = x[i];
            g->pts[i].y = y[i];
            g->pts[i].onCurve = (BYTE)(flags[i] & FLAG_ON_CURVE);

            if(i == endPointsOfContours[contour]) {
                g->pts[i].lastInContour = TRUE;
                contour++;
            } else {
                g->pts[i].lastInContour = FALSE;
            }
        }
        g->n = totalPts;
        g->xMax = xMax;
        g->xMin = xMin;

    } else {
        // This is a composite glyph, TODO.
    }
}

//-----------------------------------------------------------------------------
// Load a TrueType font into memory. We care about the curves that define
// the letter shapes, and about the mappings that determine which glyph goes
// with which character.
//-----------------------------------------------------------------------------
static BOOL LoadFontFromFile(char *file, char *displayName)
{
    int i;
    
    OpenTtf = fopen(file, "rb");
    if(!OpenTtf) {
        return FALSE;
    }

    if(setjmp(TtfErrorBuf) != 0) {
        fclose(OpenTtf);
        return FALSE;
    }

    // First, load the Offset Table
    DWORD   version         = GetDWORD();
    WORD    numTables       = GetWORD();
    WORD    searchRange     = GetWORD();
    WORD    entrySelector   = GetWORD();
    WORD    rangeShift      = GetWORD();

    // Now load the Table Directory; our goal in doing this will be to
    // find the addresses of the tables that we will need.
    DWORD   glyfAddr = -1, glyfLen;
    DWORD   cmapAddr = -1, cmapLen;
    DWORD   headAddr = -1, headLen;
    DWORD   locaAddr = -1, locaLen;
    DWORD   maxpAddr = -1, maxpLen;
    DWORD   nameAddr = -1, nameLen;
    DWORD   hmtxAddr = -1, hmtxLen;
    DWORD   hheaAddr = -1, hheaLen;

    for(i = 0; i < numTables; i++) {
        char tag[5] = "xxxx";
        tag[0]              = GetBYTE();
        tag[1]              = GetBYTE();
        tag[2]              = GetBYTE();
        tag[3]              = GetBYTE();
        DWORD   checksum    = GetDWORD();
        DWORD   offset      = GetDWORD();
        DWORD   length      = GetDWORD();

        if(strcmp(tag, "glyf")==0) {
            glyfAddr = offset;
            glyfLen = length;
        } else if(strcmp(tag, "cmap")==0) {
            cmapAddr = offset;
            cmapLen = length;
        } else if(strcmp(tag, "head")==0) {
            headAddr = offset;
            headLen = length;
        } else if(strcmp(tag, "loca")==0) {
            locaAddr = offset;
            locaLen = length;
        } else if(strcmp(tag, "maxp")==0) {
            maxpAddr = offset;
            maxpLen = length;
        } else if(strcmp(tag, "name")==0) {
            nameAddr = offset;
            nameLen = length;
        } else if(strcmp(tag, "hhea")==0) {
            hheaAddr = offset;
            hheaLen = length;
        } else if(strcmp(tag, "hmtx")==0) {
            hmtxAddr = offset;
            hmtxLen = length;
        }
    }

    if(glyfAddr == -1 || cmapAddr == -1 || headAddr == -1 || locaAddr == -1 ||
       maxpAddr == -1 || hmtxAddr == -1 || nameAddr == -1 || hheaAddr == -1)
    {
        fclose(OpenTtf);
        return FALSE;
    }

    // Load the name table. This gives us display names for the font, which
    // we need when we're giving the user a list to choose from.
    fseek(OpenTtf, nameAddr, SEEK_SET);

    WORD  nameFormat            = GetWORD();
    WORD  nameCount             = GetWORD();
    WORD  nameStringOffset      = GetWORD();
    // And now we're at the name records. Go through those till we find
    // one that we want.
    int displayNameOffset, displayNameLength;
    for(i = 0; i < nameCount; i++) {
        WORD    platformID      = GetWORD();
        WORD    encodingID      = GetWORD();
        WORD    languageID      = GetWORD();
        WORD    nameId          = GetWORD();
        WORD    length          = GetWORD();
        WORD    offset          = GetWORD();

        if(nameId == 4) {
            displayNameOffset = offset;
            displayNameLength = length;
            break;
        }
    }
    if(displayName && i >= nameCount) {
        fclose(OpenTtf);
        return FALSE;
    }
    displayNameLength = min(displayNameLength, 100);

    if(displayName) {
        // Find the display name, and store it in the provided buffer.
        fseek(OpenTtf, nameAddr+nameStringOffset+displayNameOffset, SEEK_SET);
        for(i = 0; i < displayNameLength; i++) {
            BYTE b = GetBYTE();
            if(b) {
                *displayName = b;
                displayName++;
            }
        }
        *displayName = '\0';
     
        fclose(OpenTtf);
        return TRUE;
    }


    // Load the head table; we need this to determine the format of the
    // loca table, 16- or 32-bit entries
    fseek(OpenTtf, headAddr, SEEK_SET);

    DWORD headVersion           = GetDWORD();
    DWORD headFontRevision      = GetDWORD();
    DWORD headCheckSumAdj       = GetDWORD();
    DWORD headMagicNumber       = GetDWORD();
    WORD  headFlags             = GetWORD();
    WORD  headUnitsPerEm        = GetWORD();
    (void)GetDWORD(); // created time
    (void)GetDWORD();
    (void)GetDWORD(); // modified time
    (void)GetDWORD();
    WORD  headXmin              = GetWORD();
    WORD  headYmin              = GetWORD();
    WORD  headXmax              = GetWORD();
    WORD  headYmax              = GetWORD();
    WORD  headMacStyle          = GetWORD();
    WORD  headLowestRecPPEM     = GetWORD();
    WORD  headFontDirectionHint = GetWORD();
    WORD  headIndexToLocFormat  = GetWORD();
    WORD  headGlyphDataFormat   = GetWORD();
    
    if(headMagicNumber != 0x5F0F3CF5) {
        dbp("bad magic number: %08x", headMagicNumber);
        TtfOops();
    }

    // Load the hhea table, which contains the number of entries in the
    // horizontal metrics (hmtx) table.
    fseek(OpenTtf, hheaAddr, SEEK_SET);
    DWORD hheaVersion           = GetDWORD();
    WORD  hheaAscender          = GetWORD();
    WORD  hheaDescender         = GetWORD();
    WORD  hheaLineGap           = GetWORD();
    WORD  hheaAdvanceWidthMax   = GetWORD();
    WORD  hheaMinLsb            = GetWORD();
    WORD  hheaMinRsb            = GetWORD();
    WORD  hheaXMaxExtent        = GetWORD();
    WORD  hheaCaretSlopeRise    = GetWORD();
    WORD  hheaCaretSlopeRun     = GetWORD();
    WORD  hheaCaretOffset       = GetWORD();
    (void)GetWORD();
    (void)GetWORD();
    (void)GetWORD();
    (void)GetWORD();
    WORD  hheaMetricDataFormat  = GetWORD();
    WORD  hheaNumberOfMetrics   = GetWORD();

    // Load the maxp table, which determines (among other things) the number
    // of glyphs in the font
    fseek(OpenTtf, maxpAddr, SEEK_SET);

    DWORD maxpVersion               = GetDWORD();
    WORD  maxpNumGlyphs             = GetWORD();
    WORD  maxpMaxPoints             = GetWORD();
    WORD  maxpMaxContours           = GetWORD();
    WORD  maxpMaxComponentPoints    = GetWORD();
    WORD  maxpMaxComponentContours  = GetWORD();
    WORD  maxpMaxZones              = GetWORD();
    WORD  maxpMaxTwilightPoints     = GetWORD();
    WORD  maxpMaxStorage            = GetWORD();
    WORD  maxpMaxFunctionDefs       = GetWORD();
    WORD  maxpMaxInstructionDefs    = GetWORD();
    WORD  maxpMaxStackElements      = GetWORD();
    WORD  maxpMaxSizeOfInstructions = GetWORD();
    WORD  maxpMaxComponentElements  = GetWORD();
    WORD  maxpMaxComponentDepth     = GetWORD();

    // Load the hmtx table, which gives the horizontal metrics (spacing
    // and advance width) of the font.
    fseek(OpenTtf, hmtxAddr, SEEK_SET);

    WORD  hmtxAdvanceWidth;
    SWORD hmtxLsb;
    for(i = 0; i < min(MAX_GLYPHS, hheaNumberOfMetrics); i++) {
        hmtxAdvanceWidth = GetWORD();
        hmtxLsb          = (SWORD)GetWORD();

        FN->glyphs[i].leftSideBearing = hmtxLsb;
        FN->glyphs[i].advanceWidth = hmtxAdvanceWidth;
    }
    // The last entry in the table applies to all subsequent glyphs also.
    for(; i < MAX_GLYPHS; i++) {
        FN->glyphs[i].leftSideBearing = hmtxLsb;
        FN->glyphs[i].advanceWidth = hmtxAdvanceWidth;
    }

    // Load the cmap table, which determines the mapping of characters to
    // glyphs.
    fseek(OpenTtf, cmapAddr, SEEK_SET);

    DWORD usedTableAddr = -1;

    WORD  cmapVersion        = GetWORD();
    WORD  cmapTableCount     = GetWORD();
    for(i = 0; i < cmapTableCount; i++) {
        WORD  platformId = GetWORD();
        WORD  encodingId = GetWORD();
        DWORD offset     = GetDWORD();

        if(platformId == 3 && encodingId == 1) {
            usedTableAddr = cmapAddr + offset;
        }
    }

    if(usedTableAddr == -1) {
        fclose(OpenTtf);
        return FALSE;
    }

    // So we can load the desired subtable; in this case, Windows Unicode,
    // which is us.
    fseek(OpenTtf, usedTableAddr, SEEK_SET);

    WORD  mapFormat             = GetWORD();
    WORD  mapLength             = GetWORD();
    WORD  mapVersion            = GetWORD();
    WORD  mapSegCountX2         = GetWORD();
    WORD  mapSearchRange        = GetWORD();
    WORD  mapEntrySelector      = GetWORD();
    WORD  mapRangeShift         = GetWORD();
    
    if(mapFormat != 4) {
        // Required to use format 4 per spec
        fclose(OpenTtf);
        return FALSE;
    }

    WORD endChar[1024], startChar[1024], idDelta[1024], idRangeOffset[1024];
    int segCount = mapSegCountX2 / 2;
    if(segCount > arraylen(endChar) || segCount > arraylen(startChar) ||
       segCount > arraylen(idDelta) || segCount > arraylen(idRangeOffset))
    {
        fclose(OpenTtf);
        return FALSE;
    }

    for(i = 0; i < segCount; i++) {
        endChar[i] = GetWORD();
    }
    WORD  mapReservedPad        = GetWORD();
    for(i = 0; i < segCount; i++) {
        startChar[i] = GetWORD();
    }
    for(i = 0; i < segCount; i++) {
        idDelta[i] = GetWORD();
    }
    for(i = 0; i < segCount; i++) {
        idRangeOffset[i] = GetWORD();
    }

    // So first, null out the glyph table in our in-memory representation
    // of the font; any character for which cmap does not provide a glyph
    // corresponds to -1
    for(i = 0; i < arraylen(FN->useGlyph); i++) {
        FN->useGlyph[i] = 0;
    }

    for(i = 0; i < segCount; i++) {
        WORD v = idDelta[i];
        if(idRangeOffset[i] == 0) {
            int j;
            for(j = startChar[i]; j <= endChar[i]; j++) {
                if(j > 0 && j < arraylen(FN->useGlyph)) {
                    // Don't create a reference to a glyph that we won't
                    // store because it's bigger than the table.
                    if((WORD)(j + v) < MAX_GLYPHS) {
                        FN->useGlyph[j] = (WORD)(j + v);
                    }
                }
            }
        } else {
            // TODO, there's a different table
        }
    }

    // Load the loca table. This contains the offsets of each glyph,
    // relative to the beginning of the glyf table.
    fseek(OpenTtf, locaAddr, SEEK_SET);

    DWORD glyphOffsets[MAX_GLYPHS];
    for(i = 0; i < maxpNumGlyphs && i < MAX_GLYPHS; i++) {
        if(headIndexToLocFormat == 1) {
            // long offsets, 32 bits
            glyphOffsets[i] = GetDWORD();
        } else if(headIndexToLocFormat == 0) {
            // short offsets, 16 bits but divided by 2
            glyphOffsets[i] = GetWORD()*2;
        } else {
            dbp("bad headIndexToLocFormat: %d", headIndexToLocFormat);
            TtfOops();
        }
    }

    FN->scale = 1024;
    // Load the glyf table. This contains the actual representations of the
    // letter forms, as piecewise linear or quadratic outlines.
    for(i = 0; i < maxpNumGlyphs && i < MAX_GLYPHS;  i++) {
        fseek(OpenTtf, glyfAddr + glyphOffsets[i], SEEK_SET);
        LoadGlyph(i);
    }

    fclose(OpenTtf);
    return TRUE;
}

//-----------------------------------------------------------------------------
// Select a TTF font, for use with subsequent calls to TtfPlotString(). If
// the font is already in memory, then set FN to that font. Otherwise try
// to read the font from its file, and store it in memory.
//-----------------------------------------------------------------------------
void TtfSelectFont(char *file)
{
    FN = NULL;

    if(*file == '\0') return;

    // Is it already loaded?
    int i;
    for(i = 0; i < MAX_TTF_FONTS_IN_MEMORY; i++) {
        if(TtfsStored[i] && strcmp(file, TtfsStored[i]->file)==0) {
            // It's already in memory, so there's nothing to do.
            FN = &(TtfsStored[i]->font);
            return;
        }
    }

    for(i = 0; i < MAX_TTF_FONTS_IN_MEMORY; i++) {
        if(!TtfsStored[i]) {
            TtfsStored[i] = (TtfStored *)DAlloc(sizeof(*(TtfsStored[0])));
            strcpy(TtfsStored[i]->file, file);
            FN = &(TtfsStored[i]->font);
            break;
        }
    }
    if(!FN) return;

    // Can we load it now?
    if(LoadFontFromFile(file, NULL)) return;

    // Didn't work; null it out, so that an error pattern gets displayed
    // instead of the glyphs.
    DFree(TtfsStored[i]);
    TtfsStored[i] = NULL;
    FN = NULL;
}

//-----------------------------------------------------------------------------
// Get the display name from a font filename. This requires that we open
// up the font file, and search through the name table.
//-----------------------------------------------------------------------------
void TtfGetDisplayName(char *file, char *str)
{
    // If we're unable to find the display name, return an empty string.
    *str = '\0';
    LoadFontFromFile(file, str);
}

static int LastWas;
#define NOTHING     0
#define ON_CURVE    1
#define OFF_CURVE   2
static IntPoint LastOnCurve;
static IntPoint LastOffCurve;

static void Flush(void)
{
    LastWas = NOTHING;
}

static void ln(DWORD ref, int x0, int y0, int x1, int y1)
{
    x0 = (x0*FN->scale + 512) >> 10;
    y0 = (y0*FN->scale + 512) >> 10;
    x1 = (x1*FN->scale + 512) >> 10;
    y1 = (y1*FN->scale + 512) >> 10;

    TtfLineSegment(ref, x0, y0, x1, y1);
}

static void Handle(DWORD ref, int *dx, int x, int y, BOOL onCurve)
{
    x = ((x + *dx)*FN->scale + 512) >> 10;
    y = (y*FN->scale + 512) >> 10;

    if(LastWas == ON_CURVE && onCurve) {
        // This is a line segment.
        TtfLineSegment(ref, LastOnCurve.x, LastOnCurve.y, x, y);


    } else if(LastWas == ON_CURVE && !onCurve) {
        // We can't do the Bezier until we get the next on-curve point,
        // but we must store the off-curve point.


    } else if(LastWas == OFF_CURVE && onCurve) {
        // We are ready to do a Bezier.
        TtfBezier(ref, LastOnCurve.x, LastOnCurve.y, 
                       LastOffCurve.x, LastOffCurve.y,
                        x, y);

    } else if(LastWas == OFF_CURVE && !onCurve) {
        // Two consecutive off-curve points implicitly have an on-point
        // curve between them, and that should trigger us to generate a
        // Bezier.
        IntPoint fake;
        fake.x = (x + LastOffCurve.x) / 2;
        fake.y = (y + LastOffCurve.y) / 2;
        TtfBezier(ref, LastOnCurve.x, LastOnCurve.y, 
                       LastOffCurve.x, LastOffCurve.y,
                       fake.x, fake.y);

        LastOnCurve.x = fake.x;
        LastOnCurve.y = fake.y;
    }

    if(onCurve) {
        LastOnCurve.x = x;
        LastOnCurve.y = y;
        LastWas = ON_CURVE;
    } else {
        LastOffCurve.x = x;
        LastOffCurve.y = y;
        LastWas = OFF_CURVE;
    }
}

static void PlotCharacter(DWORD ref, int *dx, int c, double spacing)
{
    int glyph = FN->useGlyph[c];

    if(glyph < 0 || glyph > MAX_GLYPHS) return;
    TtfGlyph *g = &(FN->glyphs[glyph]);

    if(c == ' ') {
        *dx += g->advanceWidth;
        return;
    }

/*
    dbp("character '%c': left side %d, right side %d", c,
        g->leftSideBearing, g->advanceWidth - (g->xMax - g->xMin));
    dbp("             : advance width %d, width %d",
        g->advanceWidth, g->xMax - g->xMin); */

/*
    int w = g->xMax - g->xMin;
    w = g->advanceWidth;
    int refp = ref + c;
    ln(refp, *dx, -50, *dx + w, -50);
    ln(refp, *dx, 2000, *dx + w, 2000);
    ln(refp, *dx, 2000, *dx, -50);
    ln(refp, *dx + w, 2000, *dx + w, -50); */

    int dx0 = *dx;

    // A point that has x = xMin should be plotted at (dx0 + lsb); fix up
    // our x-position so that the curve-generating code will put stuff
    // at the right place.
    *dx = dx0 - g->xMin;
    *dx += g->leftSideBearing;

    int i;
    int firstInContour = 0;
    for(i = 0; i < g->n; i++) {
/*        printf("(%4d, %4d)  %s  %s\n", g->pts[i].x, g->pts[i].y,
            g->pts[i].onCurve ? "ON " : "off", g->pts[i].lastInContour ?
                "LAST" : "");  */

        Handle(ref, dx, g->pts[i].x, g->pts[i].y, g->pts[i].onCurve);
        
        if(g->pts[i].lastInContour) {
            int f = firstInContour;
            Handle(ref, dx, g->pts[f].x, g->pts[f].y, g->pts[f].onCurve);
            firstInContour = i + 1;
            Flush();
        }
    }

    // And we're done, so advance our position by the requested advance
    // width, plus the user-requested extra advance.
    *dx = dx0 + g->advanceWidth + toint(spacing);
}

void TtfPlotString(DWORD ref, char *str, double spacing)
{
    if(!FN || !str || *str == '\0') {
        TtfLineSegment(ref, 0, 0, 1024, 0);
        TtfLineSegment(ref, 1024, 0, 1024, 1024);
        TtfLineSegment(ref, 1024, 1024, 0, 1024);
        TtfLineSegment(ref, 0, 1024, 0, 0);
        return;
    }

    int dx = 0;

    while(*str) {
        PlotCharacter(ref, &dx, *str, spacing);
        str++;
    }
}

#if 0
void TtfLineSegment(DWORD ref, int x0, int y0, int x1, int y1)
{
    printf("PU %d, %d\n", x0, y0);
    printf("PD %d, %d\n", x1, y1);
}

void TtfBezier(DWORD ref, int x0, int y0, int x1, int y1, int x2, int y2)
{
}

int main(void)
{
    TtfLoadFromFile("arial.ttf");

    PlotCharacter('E');
    return 0;
}
#endif
