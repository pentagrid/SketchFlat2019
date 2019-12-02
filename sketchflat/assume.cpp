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
// If we are given an underdetermined system of equations, then we can still
// solve it, but only under certain assumptions. This is where we make those
// assumptions.
//
// Jonathan Westhues, June 2007
//-----------------------------------------------------------------------------
#include "sketchflat.h"

// The Jacobian for our entire system. This is never actually solved--not
// efficient, and asking for numerical surprises--but is used to determine
// how we should make assumptions and partition the system.
static struct {
    int      eq[MAX_UNKNOWNS_AT_ONCE];
    hParam   param[MAX_UNKNOWNS_AT_ONCE];

    Expr    *sym[MAX_UNKNOWNS_AT_ONCE][MAX_UNKNOWNS_AT_ONCE];
    double   num[MAX_UNKNOWNS_AT_ONCE][MAX_UNKNOWNS_AT_ONCE];
    int      M;
    int      N;

    BOOL    solvedFor[MAX_UNKNOWNS_AT_ONCE];
    BOOL    assumed[MAX_UNKNOWNS_AT_ONCE];
} J;

// These are used in the least-squares type assumption heuristic.
struct {
    double  A[MAX_UNKNOWNS_AT_ONCE][MAX_UNKNOWNS_AT_ONCE];
    double  AAt[MAX_UNKNOWNS_AT_ONCE][MAX_UNKNOWNS_AT_ONCE];

    double  b[MAX_UNKNOWNS_AT_ONCE];
    double  z[MAX_UNKNOWNS_AT_ONCE];
    double  x[MAX_UNKNOWNS_AT_ONCE];

    int     rows;
    int     cols;
} AH;

//-----------------------------------------------------------------------------
// Return a string that describes some parameter. This is needed when we
// add the parameter to the list of those we've assumed.
//-----------------------------------------------------------------------------
static char *StringForParam(hParam p)
{
    static char Ret[MAX_STRING];

    hEntity he = ENTITY_FROM_PARAM(p);

    if(p & X_COORD_FOR_PT(0)) {
        sprintf(Ret, "x     : p%08x", p);
    } else if(p & Y_COORD_FOR_PT(0)) {
        sprintf(Ret, "y     : p%08x", p);
    } else if(p & THETA_FOR_LINE(0)) {
        sprintf(Ret, "theta : p%08x", p);
    } else if(p & A_FOR_LINE(0)) {
        sprintf(Ret, "offset: p%08x", p);
    } else {
        SketchEntity *e = EntityById(he);
        if(e->type == ENTITY_CIRCLE) {
            sprintf(Ret, "radius: p%08x", p);
        } else {
            sprintf(Ret, "???   : p%08x", p);
        }
    }

    return Ret;
}

//-----------------------------------------------------------------------------
// If we are solving for the first time after a modification to the sketch
// entities or constraints, then add the given parameter to the list of
// underconstrained and therefore assumed unknowns. We mustn't do this all
// the time, or we will flicker the display and be needlessly slow.
//-----------------------------------------------------------------------------
static void NotifyUserThatWeAssumed(hParam p)
{
    if(SK->eqnsDirty) {
        uiAddToAssumptionsList(StringForParam(p));
    }
}

//-----------------------------------------------------------------------------
// Given a string in the form returned by StringForParam(), highlighted
// whatever it is on the sketch that that parameter describes. This will
// help the user to see what they have that's underconstrained.
//-----------------------------------------------------------------------------
void HighlightAssumption(char *str)
{
    if(strlen(str) < 12) return;

    str += 9;
    DWORD v;
    if(sscanf(str, "%x", &v) != 1) return;
    
    hParam p = (hParam)v;

    ClearHoverAndSelected();
    if((p & X_COORD_FOR_PT(0)) || (p & Y_COORD_FOR_PT(0))) {
        hPoint pt = POINT_FROM_PARAM(p);
        Selected[0].which = SEL_POINT;
        Selected[0].point = pt;
        EmphasizeSelected = TRUE;
    } else if((p & A_FOR_LINE(0)) || (p & THETA_FOR_LINE(0))) {
        hLine ln = LINE_FROM_PARAM(p);
        Selected[0].which = SEL_LINE;
        Selected[0].line = ln;
        EmphasizeSelected = TRUE;
    } else {
        Selected[0].which = SEL_ENTITY;
        Selected[0].entity = ENTITY_FROM_PARAM(p);
        EmphasizeSelected = TRUE;
    }
}

//-----------------------------------------------------------------------------
// Test for equality, with a tolerance. Let's use our own private tolerance
// here in case it needs tweaking.
//-----------------------------------------------------------------------------
static BOOL ntol(double a, double b)
{
    double d = a - b;
    if(d < 0) d = -d;

    return (d < 1e-9);
}

//-----------------------------------------------------------------------------
// Debugging functions to print the systems being solved.
//-----------------------------------------------------------------------------
static void pm(double mat[][MAX_UNKNOWNS_AT_ONCE], int m, int n)
{
    int i, j;
    char buf[1024];
    for(i = 0; i < m; i++) {
        OutputDebugString(" ");
        for(j = 0; j < n; j++) {
            double v = mat[i][j];
            sprintf(buf, "%s%g ", v < 0 ? "" : " ", v);
            OutputDebugString(buf);
        }
        sprintf(buf, ";\n");
        OutputDebugString(buf);
    }
}
static void pmJ(void)
{
    pm(J.num, J.M, J.N);

    int j;
    for(j = 0; j < J.N; j++) {
        char buf[1024];
        sprintf(buf, "col %d: p%08x\n", j, J.param[j]);
        OutputDebugString(buf);
    }
    OutputDebugString("\n");
}
static void pmAH(void)
{
    char buf[1024];

    pm(AH.A, AH.rows, AH.cols);

    int r, c;
    for(r = 0; r < AH.rows; r++) {
        sprintf(buf, "   %3.8f ;\n", AH.b[r]);
        OutputDebugString(buf);
    }
    pm(AH.AAt, AH.rows, AH.rows);
    OutputDebugString("%%\n");
    for(r = 0; r < AH.rows; r++) {
        sprintf(buf, "   %3.8f ;\n", AH.z[r]);
        OutputDebugString(buf);
    }
    OutputDebugString("%%\n");
    for(c = 0; c < AH.cols; c++) {
        sprintf(buf, "   %3.8f ;\n", AH.x[c]);
        OutputDebugString(buf);
    }
    OutputDebugString("\n");
}

//-----------------------------------------------------------------------------
// It might be possible to assume either coordinate of a point, but better
// to assume one than the other. Consider a point, whose distance to the
// origin is constrained. We should drag the point horizontally when it's
// near the y-axis, and verticlaly when it's near the x-axis.
//
// This achieves that, by looking at sensitivities of each parameter in the
// Jacobian. For points referenced by only one constraint, this will always
// do the right thing. Otherwise I don't always make good assumptions, because
// I look at only entries in a single column, and ignore the rest of the
// matrix. I have a better method that's too slow; see
// LeastSquaresXSensitivity().
//-----------------------------------------------------------------------------
static double SensitivityTo(hParam hp, int *jpos)
{
    int j;
    for(j = 0; j < J.N; j++) {
        if(J.param[j] == hp) {
            double v = 0;
            int i;
            for(i = 0; i < J.M; i++) {
                // The choice of a square here, in addition to looking
                // theoretically sound, makes us nearly ignore angle
                // constraints (dimensionless) when distance constraints
                // (microns) are present. This is good; the distance
                // constraints are most likely to end up unsatisfiable,
                // so they should win.
                double u = J.num[i][j];
                v += u*u;
            }
            *jpos = j;
            return v;
        }
    }
    return VERY_NEGATIVE;
}
static void MostSensitiveCoordinateFirst(void)
{
    int a;
    for(a = 0; a < SK->points; a++) {
        hPoint pt = SK->point[a];
        hParam hpx = X_COORD_FOR_PT(pt);
        hParam hpy = Y_COORD_FOR_PT(pt);

        int jx, jy;
        double sx = SensitivityTo(hpx, &jx);
        double sy = SensitivityTo(hpy, &jy);
        if(sx < 0 || sy < 0) continue;

        BOOL swap;
        // A bit of hysteresis, to stop us from flitting back and forth
        // between the two assumptions when e.g. we're right at 45 degrees.
        double rat = 1.4;
        if(sx/sy < rat && sy/sx < rat) {
            swap = FALSE;
            // Almost the same; do whatever we did last time.
            BOOL xas = (ParamById(hpx))->assumedLastTime;
            BOOL yas = (ParamById(hpy))->assumedLastTime;
            if(xas && jx < jy) swap = TRUE;
            if(yas && jy < jx) swap = TRUE;
        } else {
            swap = (sx > sy) && (jx > jy);
        }

        if(swap) {
            // Swap the two columns for this point's x and y, so that the
            // more sensitive one comes first.
            hParam thp = J.param[jx];
            J.param[jx] = J.param[jy];
            J.param[jy] = thp;

            int i;
            for(i = 0; i < J.M; i++) {
                double tn = J.num[i][jx];
                J.num[i][jx] = J.num[i][jy];
                J.num[i][jy] = tn;

                Expr *ts = J.sym[i][jx];
                J.sym[i][jx] = J.sym[i][jy];
                J.sym[i][jy] = ts;
            }
        }
    }
}

//-----------------------------------------------------------------------------
// Use Gauss-Jordan elimination to put our Jacobian in row-reduced echelon
// form. This is used to determine which variables are bound vs. free.
//-----------------------------------------------------------------------------
static void GaussJordan(void)
{
    int i, j;

//    dbp("before:");
//    pmJ();

    // Initially, no variables are bound.
    for(j = 0; j < J.N; j++) {
        J.solvedFor[j] = FALSE;
        J.assumed[j] = FALSE;
    }

    // If there's anything we're very insensitive to, then treat that as
    // completely insensitive. We will make these decisions in relative
    // terms, with respect to the magnitude of the row in which an entry
    // appears; otherwise, the threshold would change if the constraint
    // equation were multiplied by a constant.
    double angleFudge = 10000;
    for(i = 0; i < J.M; i++) {
        double mag = 0;
        for(j = 0; j < J.N; j++) {
            double v = J.num[i][j];
            // Lengths are typically around 1 cm, or 10 000 um. Angles are
            // typically between 0 and 3.14. The angle parameter will cause
            // trouble because it's of a different order of magnitude, and
            // its sensitivities should therefore be fudged.
            if(J.param[j] & THETA_FOR_LINE(0)) v /= angleFudge;

            mag += v*v;
        }
        mag = sqrt(mag);
        // If a given unknown contributes no more than 1/200th of the
        // total sensitivity of this constraint, then we will treat it
        // as completely insensitive.
        double threshold = mag/200;
        for(j = 0; j < J.N; j++) {
            double v = J.num[i][j];
            if(J.param[j] & THETA_FOR_LINE(0)) v /= angleFudge;

            if(fabs(v) < threshold) {
                J.num[i][j] = 0;
            }
        }
    }

    // Now eliminate.
    i = 0;
    for(j = 0; j < J.N; j++) {
        // First, seek a pivot in our column.
        int ip, imax;
        double max = 0;
        for(ip = i; ip < J.M; ip++) {
            double v = fabs(J.num[ip][j]);
            if(v > max) {
                imax = ip;
                max = v;
            }
        }
        if(!ntol(max, 0)) {
            // There's a usable pivot in this column. Swap it in:
            int js;
            for(js = j; js < J.N; js++) {
                double temp;
                temp = J.num[imax][js];
                J.num[imax][js] = J.num[i][js];
                J.num[i][js] = temp;
            }

            // Get a 1 as the leading entry in the row we're working on.
            double v = J.num[i][j];
            for(js = 0; js < J.N; js++) {
                J.num[i][js] /= v;
            }

            // Eliminate this column from rows except this one.
            int is;
            for(is = 0; is < J.M; is++) {
                if(is == i) continue;

                // We're trying to drive J.num[is][j] to zero. We know
                // that J.num[i][j] is 1, so we want to subtract off
                // J.num[is][j] times our present row.
                double v = J.num[is][j];
                for(js = 0; js < J.N; js++) {
                    J.num[is][js] -= v*J.num[i][js];
                }
                J.num[is][j] = 0;
            }

            // And mark this as a bound variable.
            J.solvedFor[j] = TRUE;

            // Move on to the next row, since we just used this one to
            // eliminate from column j.
            i++;
            if(i >= J.M) break;
        }
    }

//    dbp("all done:");
//    pmJ();
}

//-----------------------------------------------------------------------------
// Write the Jacobian matrix, and then put it in rref. We use those parameters
// that are marked as unknown, and those equations that are not yet assigned
// to a subsystem.
//-----------------------------------------------------------------------------
static void WriteJacobian(BOOL skipOne, hConstraint toSkip)
{

    int i, j;

    // Write our list of equations
    J.M = 0;
    for(i = 0; i < EQ->eqns; i++) {
        if(EQ->eqn[i].subSys >= 0) continue;
        if(skipOne) {
            if(CONSTRAINT_FOR_EQUATION(EQ->eqn[i].he) == toSkip) continue;
        }

        if(J.M >= MAX_UNKNOWNS_AT_ONCE) return;
        J.eq[J.M] = i;
        (J.M)++;
    }

    // And then our list of unknowns
    J.N = 0;
    for(i = SK->params - 1; i >= 0; i--) {
        if(SK->param[i].known) continue;

        if(J.N >= MAX_UNKNOWNS_AT_ONCE) return;
        J.param[J.N] = SK->param[i].id;
        (J.N)++;
    }

    // Write the Jacobian, first symbolically then numerically (about the 
    // current guessed solution).
    for(i = 0; i < J.M; i++) {
        for(j = 0; j < J.N; j++) {
            int eq = J.eq[i];
            hParam p = J.param[j];

            if(EIndependentOf(EQ->eqn[eq].e, p)) {
                J.sym[i][j] = EConstant(0);
                J.num[i][j] = 0;
            } else {
                J.sym[i][j] = EPartial(EQ->eqn[eq].e, p);
                J.num[i][j] = EEval(J.sym[i][j]);
            }
        }
    }

    MostSensitiveCoordinateFirst();

    GaussJordan();
}

//-----------------------------------------------------------------------------
// Is there a row of all zeros in the Jacobian? This means that some set of
// constraint equations was linearly dependent, which means that those
// constraints are either redundant or inconsistent. In either case, this
// is an error that we wish to flag.
//-----------------------------------------------------------------------------
static BOOL RowOfAllZeros(void)
{
    int i, j;
    for(i = 0; i < J.M; i++) {
        for(j = 0; j < J.N; j++) {
            if(!ntol(J.num[i][j], 0)) break;
        }
        if(j >= J.N) return TRUE;
    }

    return FALSE;
}

//-----------------------------------------------------------------------------
// If the Jacobian row-reduces to contain a row of zeros, then the constraints
// are inconsistent or redundant. We can bring it back to a consistent
// (though probably still underdetermined) system by removing constraints;
// figure out which ones, and list them.
//-----------------------------------------------------------------------------
void HighlightConstraint(char *str)
{
    char *s = strchr(str, ':');
    if(!s) return;
    s = strchr(s, 'c');
    if(!s) return;
    s++;

    DWORD v;
    if(sscanf(s, "%x", &v) != 1) return;

    hConstraint hc = (hConstraint)v;

    ClearHoverAndSelected();
    Selected[0].which = SEL_CONSTRAINT;
    Selected[0].constraint = hc;
    EmphasizeSelected = TRUE;
}
static void DescribeConstraint(hConstraint hc)
{
    SketchConstraint *c = ConstraintById(hc);

    char desc[MAX_STRING];
    sprintf(desc, "         : c%08x", hc);

    const char *s;
    switch(c->type) {
        case CONSTRAINT_PT_PT_DISTANCE:             s = "distance"; break;
        case CONSTRAINT_POINTS_COINCIDENT:          s = "coincide"; break;
        case CONSTRAINT_PT_LINE_DISTANCE:           s = "distance"; break;
        case CONSTRAINT_LINE_LINE_DISTANCE:         s = "distance"; break;
        case CONSTRAINT_POINT_ON_LINE:              s = "on line"; break;
        case CONSTRAINT_RADIUS:                     s = "radius"; break;
        case CONSTRAINT_LINE_LINE_ANGLE:            s = "angle"; break;
        case CONSTRAINT_AT_MIDPOINT:                s = "at midpt"; break;
        case CONSTRAINT_EQUAL_LENGTH:               s = "eq length"; break;
        case CONSTRAINT_EQUAL_RADIUS:               s = "eq radius"; break;
        case CONSTRAINT_ON_CIRCLE:                  s = "on circle"; break;
        case CONSTRAINT_PARALLEL:                   s = "parallel"; break;
        case CONSTRAINT_PERPENDICULAR:              s = "perpendic"; break;
        case CONSTRAINT_SYMMETRIC:                  s = "symmetric"; break;
        case CONSTRAINT_HORIZONTAL:                 s = "horizont"; break;
        case CONSTRAINT_VERTICAL:                   s = "vertical"; break;
        case CONSTRAINT_FORCE_PARAM:                s = "drag h/v"; break;
        case CONSTRAINT_FORCE_ANGLE:                s = "drag rot"; break;
        case CONSTRAINT_SCALE_MM:                   s = "scale mm"; break;
        case CONSTRAINT_SCALE_INCH:                 s = "scale in"; break;
        default:                                    s = "???"; break;
    }

    memcpy(desc, s, strlen(s));
    uiAddToConstraintsList(desc);
}
static void FindConstraintsToRemoveForConsistency(void)
{
    uiClearConstraintsList();

    // We must regenerate the equations, because we got them with the
    // forward-substitutions already made. We should ignore those, or
    // else we will fail to identify any removable constraint that is
    // solved by substitution.
    GenerateEquationsToSolve();
    MarkUnknowns();

    int i;
    for(i = 0; i < SK->constraints; i++) {
        hConstraint hc = SK->constraint[i].id;

        WriteJacobian(TRUE, hc);
        if(!RowOfAllZeros()) {
            // This one fixes the problem.
            DescribeConstraint(hc);
        }
    }
}

//-----------------------------------------------------------------------------
// A different approach for determining what to assume, that I am not using
// right now. Imagine that the constraints are fully satisfied. Then, a small
// change occurs in the unknown corresponding to column jsens. What is the
// magnitude of the change in the other variables required to bring us back
// to satisfying the constraints? This metric tells us whether dragging some
// point's x or y will move the other points a little or a lot.
//
// But this is too slow, so don't use it.
//-----------------------------------------------------------------------------
static double LeastSquaresXSensitivity(int jsens)
{
    int i, j, r, c;
    // Get the Jacobian again, in un-row-reduced numerical form.
    for(i = 0; i < J.M; i++) {
        for(j = 0; j < J.N; j++) {
            J.num[i][j] = EEval(J.sym[i][j]);
        }
    }

    // Imagine that a small change occurs in the parameter corresponding to
    // column j of the Jacobian. We would like to determine the magnitude
    // of the minimum (in a least-squares sense) change that must be made
    // in the other parameters to compensate. This is just a least-squares
    // matrix solve, so write that problem:
    
    AH.rows = J.M;
    for(r = 0; r < J.M; r++) {
        AH.b[r] = -J.num[r][jsens];
    }

    for(r = 0; r < J.M; r++) {
        c = 0;
        for(j = 0; j < J.N; j++) {
            if(J.assumed[j]) continue;
            if(j == jsens) continue;

            AH.A[r][c] = J.num[r][j];
            c++;
        }
    }
    AH.cols = c;

    // Seek x = A'*inv(A*A')*b, replacing the inverse with a linear system
    // solve. This gives us x such that A*x = b, with the smallest possible
    // norm(x).

    // Write A*A'
    for(r = 0; r < AH.rows; r++) {
        for(c = 0; c < AH.rows; c++) {  // yes, AAt is square
            double sum = 0;
            for(i = 0; i < AH.cols; i++) {
                sum += AH.A[r][i]*AH.A[c][i];
            }
            AH.AAt[r][c] = sum;
        }
    }

    // Calculate inv(A*A')*b
    if(!SolveLinearSystem(AH.z, AH.AAt, AH.b, AH.rows)) {
        return VERY_POSITIVE;
    }

    // And multiply that by A' to get our solution.
    double mag = 0;
    for(c = 0; c < AH.cols; c++) {
        double sum = 0;
        for(i = 0; i < AH.rows; i++) {
            sum += AH.A[i][c]*AH.z[i];
        }
        AH.x[c] = sum;
        mag += sum*sum;
    }

    return mag;
}

//-----------------------------------------------------------------------------
// A special case for unknowns that are not mentioned in any of the equations.
// We can assume those immediately, and not waste our time later.
//-----------------------------------------------------------------------------
static void AssumeForCompletelyUnconstrained(int *assumed)
{
    int i;
    
    for(i = 0; i < SK->params; i++) {
        SK->param[i].mark = 0;
    }

    for(i = 0; i < EQ->eqns; i++) {
        // If an equation has already been solved by substitution, then we
        // shouldn't consider that.
        if(EQ->eqn[i].subSys < 0) {
            EMark(EQ->eqn[i].e, 1);
        }
    }
    for(i = 0; i < SK->params; i++) {
        SketchParam *p = &(SK->param[i]);
        if(p->mark == 0 && !(p->known)) {
            // We've never seen it, so it appears in none of the equations,
            // so we should assume something for it and mark it as known.
            p->known = TRUE;
            p->assumed = ASSUMED_FIX;
            NotifyUserThatWeAssumed(p->id);
            (*assumed)++;
            dbp2((char*)"fix because unmentioned: %08x", p->id);
        }
        p->mark = 0;
    }
}

BOOL Assume(int *assumed)
{
    AssumeForCompletelyUnconstrained(assumed);

    WriteJacobian(FALSE, 0);

    if(J.M > MAX_UNKNOWNS_AT_ONCE || J.N > MAX_UNKNOWNS_AT_ONCE) return FALSE;

    if(RowOfAllZeros()) {
        dbp((char*)"jacobian does not have full rank (%d eqs by %d params)", J.M,
            J.N);
        FindConstraintsToRemoveForConsistency();
        StopSolving();
        return FALSE;
    }

    // Shouldn't happen, since that should always produce a row of zeros.
    if(J.M > J.N) {
        return FALSE;
    }

    // For the still-free variables, just fix them wherever they were drawn.
    int j;
    for(j = 0; j < J.N; j++) {
        if(J.solvedFor[j]) continue;

        SketchParam *p = ParamById(J.param[j]);
        if(p->known) {
            oopsnf();
            continue;
        }

        // Trivial assumption, just fix the parameter wherever it is now.
        NotifyUserThatWeAssumed(p->id);
        p->known = TRUE;
        p->assumed = ASSUMED_FIX;
        (*assumed)++;

        J.assumed[j] = TRUE;
    }

    return TRUE;
}
