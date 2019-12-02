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

#ifndef __EXPR_H
#define __EXPR_H

#define EXPR_PARAM          0x000
#define EXPR_CONSTANT       0x001

#define EXPR_PLUS           0x010
#define EXPR_MINUS          0x011
#define EXPR_TIMES          0x012
#define EXPR_DIV            0x013

#define EXPR_NEGATE         0x014
#define EXPR_SQRT           0x015
#define EXPR_SQUARE         0x016

#define EXPR_SIN            0x020
#define EXPR_COS            0x021

typedef struct ExprTag Expr;
struct ExprTag {
    int             op;
    Expr            *e0;
    Expr            *e1;
    hParam          param;
    double          v;
};

Expr *EParam(hParam p);
Expr *EConstant(double v);

Expr *EOfTwo(int op, Expr *e0, Expr *e1);
#define EPlus(e0, e1)   EOfTwo(EXPR_PLUS, e0, e1)
#define EMinus(e0, e1)  EOfTwo(EXPR_MINUS, e0, e1)
#define ETimes(e0, e1)  EOfTwo(EXPR_TIMES, e0, e1)
#define EDiv(e0, e1)    EOfTwo(EXPR_DIV, e0, e1)

Expr *EOfOne(int op, Expr *e0);
#define ENegate(e0)     EOfOne(EXPR_NEGATE, e0)
#define ESqrt(e0)       EOfOne(EXPR_SQRT, e0)
#define ESquare(e0)     EOfOne(EXPR_SQUARE, e0)
#define ESin(e0)        EOfOne(EXPR_SIN, e0)
#define ECos(e0)        EOfOne(EXPR_COS, e0)

double EEval(Expr *e);
Expr *EEvalKnown(Expr *e);
Expr *EPartial(Expr *e, hParam param);
BOOL EIndependentOf(Expr *e, hParam param);
void EMark(Expr *e, int delta);

BOOL EExprMarksTwoParamsEqual(Expr *e, hParam *pA, hParam *pB);
void EReplaceParameter(Expr *e, hParam replacement, hParam toReplace);

void EPrint(const char *s, Expr *e);

#endif

