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
// Routines to solve a system of nonlinear equations numerically, using
// a Newton's method. This is not called on the entire set of constraint
// equations, but on a subset, where the partitions are chosen to make
// the solution process quick for a typical sketch.
// Jonathan Westhues, April 2007
//-----------------------------------------------------------------------------
#include "sketchflat.h"

static struct {
    double      num[MAX_UNKNOWNS_AT_ONCE];
    Expr       *sym[MAX_UNKNOWNS_AT_ONCE];
} Function;

static struct {
    double      num[MAX_UNKNOWNS_AT_ONCE][MAX_UNKNOWNS_AT_ONCE];
    Expr       *sym[MAX_UNKNOWNS_AT_ONCE][MAX_UNKNOWNS_AT_ONCE];
} Jacobian;

static hParam unkwn[MAX_UNKNOWNS_AT_ONCE];
static double InitialGuess[MAX_UNKNOWNS_AT_ONCE];

static double X[MAX_UNKNOWNS_AT_ONCE];
static int N;

static void pm(void)
{
    int i, j;
    
    char buf[1024];
    for(i = 0; i < N; i++) {
        OutputDebugString("[ ");
        for(j = 0; j < N; j++) {
            sprintf(buf, "%3.3f ", Jacobian.num[i][j]);
            OutputDebugString(buf);
        }
        sprintf(buf, "| %3.3f ]   [ %3.3f ]\n", Function.num[i], X[i]);
        OutputDebugString(buf);
    }
    OutputDebugString("\n");
}

static BOOL SolveJacobian(void)
{
    return SolveLinearSystem(X, Jacobian.num, Function.num, N);
}

BOOL SolveNewton(int subSys)
{
    int i, j;

    // First, count the number of equations to solve simultaneously.
    N = 0;
    for(i = 0; i < EQ->eqns; i++) {
        if(EQ->eqn[i].subSys == subSys) {
            if(N >= MAX_NUMERICAL_UNKNOWNS) oops();

            Function.sym[N] = EEvalKnown(EQ->eqn[i].e);

            N++;
        }
    }

    // And make a list of unknowns that we're solving for.
    int np = 0;
    for(i = 0; i < SK->params; i++) {
        if(SK->param[i].mark != 0) {
            if(np >= MAX_NUMERICAL_UNKNOWNS) oops();

            unkwn[np] = SK->param[i].id;
            InitialGuess[np] = SK->param[i].v;

            np++;
        }
    }
    if(np != N) {
        dbp("eqs=%d unknowns=%d", N, np);
        oops();
    }

    // Now write the symbolic Jacobian, using the symbolic differentiation
    // routines.
    for(i = 0; i < N; i++) {
        for(j = 0; j < N; j++) {
            Expr *p;
            if(EIndependentOf(Function.sym[i], unkwn[j])) {
                // A bit of optimisation.
                p = EConstant(0);
            } else {
                p = EPartial(Function.sym[i], unkwn[j]);
                p = EEvalKnown(p);
            }
            Jacobian.sym[i][j] = p;
            EPrint("diff: ", Jacobian.sym[i][j]);
        }
    }

    dbp2("");
    dbp2("solving for %d equations", N);
    for(i = 0; i < N; i++) {
        EPrint("eq: ", Function.sym[i]);
    }

    // And iterate.
    BOOL converged;
    int iter = 0;
    for(;;) {
        // First, evaluate the functions given the current parameters.
        for(i = 0; i < N; i++) {
            Function.num[i] = EEval(Function.sym[i]);
            dbp2("eqn[%d] is %.3f", i, Function.num[i]);
        }

        // Now evaluate the Jacobian.
        for(i = 0; i < N; i++) {
            for(j = 0; j < N; j++) {
                Jacobian.num[i][j] = EEval(Jacobian.sym[i][j]);
                dbp2("jacobian[%d][%d] is %.3f", i, j,
                    Jacobian.num[i][j]);
            }
        }

        if(SolveJacobian())  {
            // The Newton step looks like
            //      J(x_n) (x_{n+1} - x_n) = 0 - F(x_n)
            for(i = 0; i < N; i++) {
                hParam p = unkwn[i];
                double v = EvalParam(p);
                v -= 0.98*X[i];
                ForceParam(p, v);
            }
        } else {
            dbp2("singular Jacobian");
            goto failed;
        }

        // Now check if we've converged, and break if we have. We deliberately
        // don't check for convergence until we've run at least one Newton
        // iteration. This is because two linearly dependent constraints
        // that happen to be satisfied right now are still bad; if we just
        // checked for convergence, without checking for an invertible
        // Jacobian, then the rest of the code would incorrectly treat those
        // two constraints as restraining two degrees of freedom.
        converged = TRUE;
        for(i = 0; i < N; i++) {
            if(!tol(Function.num[i], 0)) {
                converged = FALSE;
            }
        }
        if(converged) break;
        if(iter > 50) break;
        iter++;
    }
    dbp2("");
    dbp2("");

    if(!converged) {
        dbp2("no convergence");
        goto failed;
    }
    return TRUE;

failed:
    // If we didn't converge, then we probably made our solution worse
    // rather than better. We should therefore put the parameters back
    // where they were.
    np = 0;
    for(i = 0; i < SK->params; i++) {
        if(SK->param[i].mark != 0) {
            if(np >= MAX_NUMERICAL_UNKNOWNS) oops();

            SK->param[i].v = InitialGuess[np];

            np++;
        }
    }
    if(np != N) oops();

    return FALSE;
}
