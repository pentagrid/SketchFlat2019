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
// A simple symbolic algebra package. This is a useful way to represent our
// constraint equations. It makes it easy to determine which unknowns affect
// the equation, and it also makes it quick to compute symbolic derivatives
// for the Jacobian.
// Jonathan Westhues, April 2007
//-----------------------------------------------------------------------------
#include "sketchflat.h"

#define MAX_EXPRS_ALLOCATED (1024*10)

static Expr *AllocExpr(void)
{
    return (Expr *)Alloc(sizeof(Expr));
}

void EWipeAllocated(void)
{

}

Expr *EParam(hParam p)
{
    Expr *e = AllocExpr();
    e->op = EXPR_PARAM;
    e->param = p;

    return e;
}

Expr *EConstant(double v)
{
    Expr *e = AllocExpr();
    e->op = EXPR_CONSTANT;
    e->v = v;

    return e;
}

Expr *EOfTwo(int op, Expr *e0, Expr *e1)
{
    Expr *e = AllocExpr();
    e->op = op;
    e->e0 = e0;
    e->e1 = e1;

    return e;
}

Expr *EOfOne(int op, Expr *e0)
{
    Expr *e = AllocExpr();
    e->op = op;
    e->e0 = e0;

    return e;
}

static char EPrintBuf[1024*40];
static void dbps(const char *str, ...)
{
    va_list f;

    va_start(f, str);
    _vsnprintf(EPrintBuf+strlen(EPrintBuf), 
        sizeof(EPrintBuf) - strlen(EPrintBuf), str, f);
}
static void EPrintWorker(Expr *e)
{
    switch(e->op) {
        case EXPR_PARAM:
            dbps("{param %08x (now %.2f)}", e->param, EvalParam(e->param));
            return;

        case EXPR_CONSTANT:
            dbps("{const %.2f}", e->v);
            return;

        case EXPR_DIV:
        case EXPR_TIMES:
        case EXPR_MINUS:
        case EXPR_PLUS:
            dbps("(");
            EPrintWorker(e->e0);
            switch(e->op) {
                case EXPR_DIV:   dbps(" / "); break;
                case EXPR_TIMES: dbps(" * "); break;
                case EXPR_MINUS: dbps(" - "); break;
                case EXPR_PLUS:  dbps(" + "); break;
            }
            EPrintWorker(e->e1);
            dbps(")");
            return;
            
        case EXPR_NEGATE:
            dbps("-("); EPrintWorker(e->e0); dbps(")");
            return;

        case EXPR_SQRT:
            dbps("sqrt("); EPrintWorker(e->e0); dbps(")");
            return;

        case EXPR_SQUARE:
            dbps("square("); EPrintWorker(e->e0); dbps(")");
            return;

        case EXPR_SIN:
            dbps("sin("); EPrintWorker(e->e0); dbps(")");
            return;

        case EXPR_COS:
            dbps("cos("); EPrintWorker(e->e0); dbps(")");
            return;

        default:
            oops();
    }
    oops();
}
void EPrint(const char *s, Expr *e)
{
    // XXX; this is slow, so it should be disabled completely unless it's
    // desired.
    return;

    strcpy(EPrintBuf, s);
    EPrintWorker(e);
    dbp2("expr: %s", EPrintBuf);
}

static double NumDiv(double a, double b)
{
    if(b == 0) {
        return VERY_POSITIVE;
    } else {
        return a / b;
    }
}

double EEval(Expr *e)
{
    switch(e->op) {
        case EXPR_PARAM:
            return EvalParam(e->param);

        case EXPR_CONSTANT:
            return e->v;

        case EXPR_PLUS:
            return EEval(e->e0) + EEval(e->e1);
            
        case EXPR_MINUS:
            return EEval(e->e0) - EEval(e->e1);

        case EXPR_TIMES:
            return EEval(e->e0) * EEval(e->e1);

        case EXPR_DIV:
            return NumDiv(EEval(e->e0), EEval(e->e1));

        case EXPR_NEGATE:
            return -EEval(e->e0);

        case EXPR_SQRT:
            return sqrt(EEval(e->e0));

        case EXPR_SQUARE: {
            double v = EEval(e->e0);
            return v*v;
        }
        case EXPR_SIN:
            return sin(EEval(e->e0));

        case EXPR_COS:
            return cos(EEval(e->e0));

        default:
            oops();
    }
    oops();
}

//-----------------------------------------------------------------------------
// Is an expression entirely independent of param? This is a useful
// optimisation, because it saves calculating and evaluating trivial
// partial derivatives.
//-----------------------------------------------------------------------------
BOOL EIndependentOf(Expr *e, hParam param)
{
    switch(e->op) {
        case EXPR_PARAM:
            return (e->param != param);

        case EXPR_CONSTANT:
            return TRUE;

        case EXPR_PLUS:
        case EXPR_MINUS:
        case EXPR_TIMES:
        case EXPR_DIV:
            return EIndependentOf(e->e0, param) &&
                   EIndependentOf(e->e1, param);

        case EXPR_SQRT:
        case EXPR_SQUARE:
        case EXPR_NEGATE:
        case EXPR_SIN:
        case EXPR_COS:
            return EIndependentOf(e->e0, param);

        default:
            oops();
    }
    oops();
}

//-----------------------------------------------------------------------------
// Return the symbolic partial derivative of the given expression, with
// respect to the parameter param.
//-----------------------------------------------------------------------------
Expr *EPartial(Expr *e, hParam param)
{
    Expr *d0, *d1;

    switch(e->op) {
        case EXPR_PARAM:
            if(e->param == param) {
                return EConstant(1);
            } else {
                return EConstant(0);
            }

        case EXPR_CONSTANT:
            return EConstant(0);

        case EXPR_PLUS:
            return EPlus(EPartial(e->e0, param), EPartial(e->e1, param));
            
        case EXPR_MINUS:
            return EMinus(EPartial(e->e0, param), EPartial(e->e1, param));

        case EXPR_TIMES:
            d0 = EPartial(e->e0, param);
            d1 = EPartial(e->e1, param);
            return EPlus(ETimes(e->e0, d1), ETimes(e->e1, d0));

        case EXPR_DIV:
            d0 = EPartial(e->e0, param);
            d1 = EPartial(e->e1, param);
            return EDiv(
                EMinus(ETimes(d0, e->e1), ETimes(e->e0, d1)),
                ESquare(e->e1));

        case EXPR_NEGATE:
            return ENegate(EPartial(e->e0, param));

        case EXPR_SQRT:
            return ETimes(
                EDiv(EConstant(0.5), ESqrt(e->e0)),
                EPartial(e->e0, param));

        case EXPR_SQUARE:
            return ETimes(
                ETimes(EConstant(2), e->e0),
                EPartial(e->e0, param));

        case EXPR_SIN:
            return ETimes(ECos(e->e0), EPartial(e->e0, param));

        case EXPR_COS:
            return ENegate(ETimes(ESin(e->e0), EPartial(e->e0, param)));

        default:
            oops();
    }
    oops();
}

//-----------------------------------------------------------------------------
// Return a simplified version of the passed expression, evaluated as far
// as we can with the paramters that have already been marked known. Allocates
// new memory, as not to trash the passed argument.
//-----------------------------------------------------------------------------
Expr *EEvalKnown(Expr *e)
{
    Expr *e0, *e1;

    switch(e->op) {
        case EXPR_PARAM:
            if(ParamById(e->param)->known) {
                return EConstant(EvalParam(e->param));
            } else {
                return EParam(e->param);
            }
            oops();

        case EXPR_CONSTANT:
            return EConstant(e->v);

        case EXPR_PLUS:
        case EXPR_MINUS:
        case EXPR_DIV:
        case EXPR_TIMES:
            e0 = EEvalKnown(e->e0);
            e1 = EEvalKnown(e->e1);

            if(e0->op == EXPR_CONSTANT && e1->op == EXPR_CONSTANT) {
                switch(e->op) {
                    case EXPR_PLUS:  return EConstant(e0->v + e1->v);
                    case EXPR_MINUS: return EConstant(e0->v - e1->v);
                    case EXPR_TIMES: return EConstant(e0->v * e1->v);
                    case EXPR_DIV:   return EConstant(NumDiv(e0->v, e1->v));
                }
            } else {
                switch(e->op) {
                    case EXPR_PLUS:  return EPlus(e0, e1);
                    case EXPR_MINUS: return EMinus(e0, e1);
                    case EXPR_DIV:   return EDiv(e0, e1);
                    case EXPR_TIMES:
                        if((e0->op == EXPR_CONSTANT && tol(e0->v, 0)) ||
                           (e1->op == EXPR_CONSTANT && tol(e1->v, 0)))
                        {
                            return EConstant(0);
                        } else {
                            return ETimes(e0, e1);
                        }
                }
            }
            oops();
                
        case EXPR_SQRT:
            e0 = EEvalKnown(e->e0);
            if(e0->op == EXPR_CONSTANT) {
                return EConstant(sqrt(e0->v));
            } else {
                return ESqrt(e0);
            }

        case EXPR_SQUARE:
            e0 = EEvalKnown(e->e0);
            if(e0->op == EXPR_CONSTANT) {
                return EConstant((e0->v)*(e0->v));
            } else {
                return ESquare(e0);
            }

        case EXPR_NEGATE:
            e0 = EEvalKnown(e->e0);
            if(e0->op == EXPR_CONSTANT) {
                return EConstant(-(e0->v));
            } else {
                return ENegate(e0);
            }

        case EXPR_SIN:
            e0 = EEvalKnown(e->e0);
            if(e0->op == EXPR_CONSTANT) {
                return EConstant(sin(e0->v));
            } else {
                return ESin(e0);
            }

        case EXPR_COS:
            e0 = EEvalKnown(e->e0);
            if(e0->op == EXPR_CONSTANT) {
                return EConstant(cos(e0->v));
            } else {
                return ECos(e0);
            }

        default:
            oops();
    }
}

//-----------------------------------------------------------------------------
// Adjust all of the SK->param[i].mark terms by the given 
//-----------------------------------------------------------------------------
void EMark(Expr *e, int delta)
{
    switch(e->op) {
        case EXPR_PARAM: {
            int i;
            for(i = 0; i < SK->params; i++) {
                if(SK->param[i].id == e->param) {
                    (SK->param[i].mark) += delta;
                    return;
                }
            }
            oops();
        }

        case EXPR_CONSTANT:
            return;

        case EXPR_PLUS:
        case EXPR_MINUS:
        case EXPR_TIMES:
        case EXPR_DIV:
            EMark(e->e0, delta);
            EMark(e->e1, delta);
            return;

        case EXPR_SQRT:
        case EXPR_SQUARE:
        case EXPR_NEGATE:
        case EXPR_SIN:
        case EXPR_COS:
            EMark(e->e0, delta);
            return;

        default:
            oops();
    }
    oops();
}

BOOL EExprMarksTwoParamsEqual(Expr *e, hParam *pA, hParam *pB)
{
    if(e->op != EXPR_MINUS) return FALSE;

    if(e->e0->op != EXPR_PARAM) return FALSE;
    if(e->e1->op != EXPR_PARAM) return FALSE;

    *pA = e->e0->param;
    *pB = e->e1->param;

    return TRUE;
}

void EReplaceParameter(Expr *e, hParam replacement, hParam toReplace)
{
    switch(e->op) {
        case EXPR_PARAM:
            if(e->param == toReplace) {
                e->param = replacement;
            }
            return;

        case EXPR_CONSTANT:
            return;

        case EXPR_PLUS:
        case EXPR_MINUS:
        case EXPR_TIMES:
        case EXPR_DIV:
            EReplaceParameter(e->e0, replacement, toReplace);
            EReplaceParameter(e->e1, replacement, toReplace);
            return;

        case EXPR_SQRT:
        case EXPR_SQUARE:
        case EXPR_NEGATE:
        case EXPR_SIN:
        case EXPR_COS:
            EReplaceParameter(e->e0, replacement, toReplace);
            return;

        default:
            oops();
    }
}
