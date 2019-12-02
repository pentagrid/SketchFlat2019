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
// The routines to `solve' the sketch. The routines in constraint.cpp have
// generate a system of symbolic equations. We will determine if that system
// is consistent. If it is, then we will solve it, using a structured
// numerical solver. If it isn't, then we will try to give guidance to the
// user as to where it should be fixed.
// Jonathan Westhues, April 2007
//-----------------------------------------------------------------------------
#include "sketchflat.h"

static int SolutionStartTime;
// In milliseconds; we should give up if we're taking too long.
#define MAX_SOLUTION_TIME 3000
// And the time before we'll show an hourglass
#define MAX_SOLUTION_TIME_BEFORE_HOURGLASS 200
static BOOL CursorIsHourglass;

#define MAX_PARTITIONED_UNKNOWNS 5

static RememberedSubsystems RSallocA;
static RememberedSubsystems RSallocB;

// The list of remembered subsystems that we are building up during this
// call to the solve routines (i.e., an output from the solver).
RememberedSubsystems *RSt = &RSallocA;
// The list of remembered subsystems that we built up during the most recent
// previous successful call to the solver (i.e., an input to the solver).
RememberedSubsystems *RSp = &RSallocB;

//-----------------------------------------------------------------------------
// This trivial-solver exists mostly to make dragging points work like
// you'd expect. Consider two points that are constrained as coincident; if
// you grab one of them, then you would like them both to move, if the
// sketch is dimensioned in a way that would permit that. In general, there's
// no way to predict whether the solver will substitute the moving point with
// the stationary one, or the reverse. This means that our point might end
// up stuck. To avoid this, do a special numerical pre-solve, to propagate
// the moving point.
//-----------------------------------------------------------------------------
void SatisfyCoincidenceConstraints(hPoint pt)
{
    int i;

    hLayer cl = GetCurrentLayer();

    // Mark 
    for(i = 0; i < SK->params; i++) {
        SketchParam *p = &(SK->param[i]);
        hEntity he = ENTITY_FROM_PARAM(p->id);

        if(p->id == X_COORD_FOR_PT(pt) || p->id == Y_COORD_FOR_PT(pt) ||
            he == REFERENCE_ENTITY || LayerForEntity(he) != cl)
        {
            p->known = TRUE;
        } else {
            p->known = FALSE;
        }
    }
   
    // So now we substitute repeatedly, propagating out from our initially
    // known point (the moving one, typically).
    BOOL didSomething;
    do {
       didSomething = FALSE; 

        for(i = 0; i < SK->constraints; i++) {
            SketchConstraint *c = &(SK->constraint[i]);

            if(c->type == CONSTRAINT_POINTS_COINCIDENT) {
                SketchParam *xA = ParamById(X_COORD_FOR_PT(c->ptA));
                SketchParam *xB = ParamById(X_COORD_FOR_PT(c->ptB));
                SketchParam *yA = ParamById(Y_COORD_FOR_PT(c->ptA));
                SketchParam *yB = ParamById(Y_COORD_FOR_PT(c->ptB));

                if(xA->known && !xB->known) {
                    xB->v = xA->v;
                    yB->v = yA->v;
                    xB->known = TRUE;
                    yB->known = TRUE;

                    didSomething = TRUE;
                } else if(xB->known && !xA->known) {
                    xA->v = xB->v;
                    yA->v = yB->v;
                    xA->known = TRUE;
                    yA->known = TRUE;

                    didSomething = TRUE;
                }
            }
        }
    } while(didSomething);

    // If they were dragging the references around, then stick those back
    // where they were supposed to go.
    ForceReferences();
}


//-----------------------------------------------------------------------------
// Given the sketched entities and constraints, write a system of equations
// that the parameters ought to satisfy. These equations are kept in
// symbolic form, in EQ->eqn[].
//-----------------------------------------------------------------------------
void GenerateEquationsToSolve(void)
{
    int i;

    // Recreate the list of parameters and points. Each entity will
    // generate some number of these, depending on how many degrees
    // of freedom (that the solver can play with) it requires.
    //
    // This also saves the parameter state. We won't forget any initial
    // guesses that we might have, either from previous solution runs or
    // from the positions where the user drew things.
    GenerateParametersPointsLines();

    // For each constraint, we write some number of equations. These are
    // kept in symbolic form. So loop through the constraints, and do that.
    EQ->eqns = 0;
    for(i = 0; i < SK->constraints; i++) {
        SketchConstraint *c = &(SK->constraint[i]);
        MakeConstraintEquations(c);
    }
    // An entity might also generate equations, irrespective of how it's
    // constrained (e.g. our 3-point arc, which requires a constraint to
    // make the two radii equal).
    for(i = 0; i < SK->entities; i++) {
        SketchEntity *e = &(SK->entity[i]);
        MakeEntityEquations(e);
    }

    // To begin with, all equations are unassigned.
    for(i = 0; i < EQ->eqns; i++) {
        EQ->eqn[i].subSys = -1;
    }


    dbp2("have %d equations", EQ->eqns);
    for(i = 0; i < EQ->eqns; i++) {
        EPrint("eqn: ", EQ->eqn[i].e);
    }
    dbp2("");
}

//-----------------------------------------------------------------------------
// As we solve subsystems, parameters in the sketch will move from unknown
// to known. But to start, everything's unknown, except for the references.
//-----------------------------------------------------------------------------
void MarkUnknowns(void)
{
    // Even in an empty sketch, there are six parameters, for the two datum
    // lines (coordinate axes) and the datum point (origin). Those can be
    // marked as known immediately, because they're our starting point. All
    // others are unknown at this point.
    int i;

    int unknowns = 0;

    for(i = 0; i < SK->params; i++) {
        SketchParam *p = &(SK->param[i]);

        hParam hp = p->id;
        if(ENTITY_FROM_PARAM(hp) == REFERENCE_ENTITY) {
            p->known = TRUE;
        } else {
            p->known = FALSE;
            unknowns++;
            dbp2("unknown: %08x", p->id);
        }

        p->assumedLastTime = p->assumed;
        p->assumed = NOT_ASSUMED;
        p->substd = 0;
    }
}

//-----------------------------------------------------------------------------
// The first stage of the solution is to forward-substitute certain
// equations. If we have param0 - param1 = 0, then it doesn't make sense
// to keep that equation and both parameters around; we'll replace param1
// with param0 in the equations, wherever it might appear, and mark param1
// as known. At the end, we can go back and set param1 := param0.
//-----------------------------------------------------------------------------
static void SolveByForwardSubstitution(void)
{
#define SUBSYS_SOLVED_BY_SUBSTITUTION       65535
    
    int i, j;
    
    for(i = 0; i < EQ->eqns; i++) {
        hParam toReplace, replacement;
        if(EExprMarksTwoParamsEqual(EQ->eqn[i].e, &toReplace, &replacement)) {

            dbp2("equation just marks two paramters equal:");
            EPrint("this: ", EQ->eqn[i].e);
            dbp2("we think %08x and %08x", toReplace, replacement);

            if(toReplace == replacement) {
                // This might happen if e.g. we marked a line as horizontal
                // twice, or if we constrained two horizontal points to lie
                // on top of two other horizontal points. It's an error, but
                // we should ignore it and go blindly ahead, and let the
                // assumption code find it.
                continue;
            }

            if(ParamById(toReplace)->known) {
                // This must be one of the references; it's okay to constrain
                // against that, but we want the references to stay where
                // they are, so they should become the replacment.
                hParam t;
                t = toReplace;
                toReplace = replacement;
                replacement = t;
            }

            // Sanity check, that our replacement hasn't been substituted
            // already.
            if(ParamById(replacement)->substd) {
                // This shouldn't have happened! If we made it here
                // then pA appeared in an equation, but if pA is
                // already substituted, then pA should have already
                // been replaced with its substitute.
                oopsnf();
                continue;
            }

            // So now let's make the substitute. First, fix up the parameter
            // record to reflect the substitution.
            BOOL found = FALSE;
            for(j = 0; j < SK->params; j++) {
                // Substitute the parameter that this equation involves.
                if(SK->param[j].id == toReplace) {
                    if(SK->param[j].substd) oops();

                    SK->param[j].substd = replacement;
                    SK->param[j].known = TRUE;
                    found = TRUE;
                }

                // But toReplace might previously have been used as a
                // replacement itself. So fix those up too.
                if(SK->param[j].substd == toReplace) {
                    SK->param[j].substd = replacement;
                }
            }
            if(!found) {
                oopsnf();
                continue;
            }

            // Then, reach in and fix all the equations (including this
            // one, so that it becomes meaningless).
            for(j = 0; j < EQ->eqns; j++) {
                EReplaceParameter(EQ->eqn[j].e, replacement, toReplace);
            }

            // And mark this equation as already used. We've eliminated
            // one equation and one unknown.
            EQ->eqn[i].subSys = SUBSYS_SOLVED_BY_SUBSTITUTION;
        }
    }

    dbp2("");
    dbp2("");
    dbp2("now, having eliminated substituted:");
    j = 0;
    for(i = 0; i < EQ->eqns; i++) {
        if(EQ->eqn[i].subSys < 0) {
            EPrint("eqn: ", EQ->eqn[i].e);
            j++;
        }
    }
    dbp2("%d eqns", j);
    dbp2("");
    for(i = 0; i < SK->params; i++) {
        if(!SK->param[i].known) {
            dbp2("unknown: %08x", SK->param[i].id);
        }
    }
}

//-----------------------------------------------------------------------------
// Some helper functions to get information on the subsystem currently under
// construction: the number of equations, and the number of unknowns.
//-----------------------------------------------------------------------------
static int EqnsInSubsys(int subSys)
{
    int c = 0;

    int i;
    for(i = 0; i < EQ->eqns; i++) { 
        if(EQ->eqn[i].subSys == subSys) {
            c++;
        }
    }
    
    return c;
}
static int ParamsMarked(void)
{
    int c = 0;

    int i;
    for(i = 0; i < SK->params; i++) {
        if((!SK->param[i].known) && SK->param[i].mark > 0) {
            c++;
        }
    }

    return c;
}

//-----------------------------------------------------------------------------
// Try to partition off a subsystem of equations, of at most depth equations
// (but fewer is better, if we can do it), using only those equations
// appearing at position startAt or later in EQ->eqn[]. If we succeed, then
// we will mark those equations with the provided subSys, and return EXACT.
// If we reach maximum depth with no luck, then we will return UNDER. If
// we find a single equation where all the parameters are known, then
// return OVER.
//-----------------------------------------------------------------------------
#define UNDER 0
#define EXACT 1
#define OVER  2
static int SeekExactlyConstrained(int subSys, int depth, int startAt)
{
    int i;
    for(i = startAt; i < EQ->eqns; i++) {
        // If an equation has already been used (either by a previously
        // solved subsystem, or by the subsystem that we are working on),
        // then we're not interested.
        if(EQ->eqn[i].subSys >= 0) continue;

        // So let's investigate what happens if add this equation to our
        // subsystem under construction.
        EQ->eqn[i].subSys = subSys;

        // Unknowns must be counted in the context of those parameters
        // already known; the equation p1*p2 + p3 = 4 is independent of
        // p2 if p1 = 0.
        Expr *e = EQ->eqn[i].e;
        Expr *pruned = EEvalKnown(e);
    
        EMark(pruned, 1);
    
        int eqs = EqnsInSubsys(subSys);
        int unknowns = ParamsMarked();

        if(eqs == unknowns) {
            // What we want; we'll be solving this one. So return.
            return EXACT;
        } else if(eqs > unknowns) {
            // When you add an equation to the system, you add one equation,
            // and zero or more unknowns. You should not be able to move from
            // underconstrained to overconstrained without passing through
            // exactly constrained, so this is can't happen.
            //
            // The exception is the degenerate case: an equation that is
            // inconsistent by itself. That might happen if all of that
            // equation's parameters have been solved for already.
            //
            // In that case, the system is clearly inconsistent.
            return OVER;
        } else if(eqs < unknowns) {
            // We're underconstrained, but perhaps we can fix that by
            // adding another equation.

            if(depth > 1) {
                // Of course, it's hopeless if we have more free variables
                // than we have equations left to fix them, so check that.
                if(unknowns - eqs <= depth) {
                    switch(SeekExactlyConstrained(subSys, depth - 1, i + 1)) {
                        case EXACT:
                            return EXACT;

                        case OVER:
                            return OVER;

                        case UNDER:
                            // Keep looking
                            break;
                    }
                }
            }
            // And if it didn't return, then we keep going.
        }

        // Didn't go, put this one back and try another.
        EMark(pruned, -1);
        EQ->eqn[i].subSys = -1;
    }

    return UNDER;
}

//-----------------------------------------------------------------------------
// Try to pick off a subsytem of equations that is possibly consistent (i.e.,
// n equations in n unknowns). Then, try to solve that subsystem. If we
// succeed, then check if we still have unknowns. If no, then we're done,
// so return TRUE. If yes, then call SolveSubSystemsStartingFrom(subSys + 1)
// recursively.
//
// If we can't find a possibly consistent subsystem, then perhaps we are
// underdetermined, and should make assumptions. In that case, make an
// assumption, and call SolveSubSystemsStartingFrom(subSys). If that
// succeeds, then we're done and can return TRUE. If it fails, then we try
// all available assumptions, until we run out and must return FALSE.
//
// The major observation to make is that this function returns FALSE only
// when the subsystem it tried to solve is inconsistent. This might be
// because the given set of constraints really is inconsistent, or it
// might be because we made a lousy assumption while solving an
// underconstrained system.
//-----------------------------------------------------------------------------
BOOL SolveSubSystemsStartingFrom(int subSys)
{   
    int i, j;

    // An equation is worth considering (because it might cause our
    // system to become solvable immediately, or lead to a solution
    // that does) if it contains at least one unknown parameter in
    // common with the equations in our subsystem so far. That's a
    // TODO, though, for now we look at all the combinations.

    // If we're taking way too long then give up.
    int now = GetTickCount();
    if(now - SolutionStartTime > MAX_SOLUTION_TIME) {
        return FALSE;
    }
    // If we're taking a little bit too long then show an hourglass.
    if(now - SolutionStartTime > MAX_SOLUTION_TIME_BEFORE_HOURGLASS &&
        !CursorIsHourglass)
    {
        uiSetCursorToHourglass();
        CursorIsHourglass = TRUE;
    }

    // First, let's count how many equations we have, and how many unknowns.
    int unknowns = 0;
    for(i = 0; i < SK->params; i++) {
        SketchParam *p = &(SK->param[i]);
        if(!p->known) {
            unknowns++;
        }
    }
    dbp2("unknowns: %d", unknowns);
    int eqs = 0;
    for(i = 0; i < EQ->eqns; i++) {
        if(EQ->eqn[i].subSys < 0) {
            eqs++;
        }
    }
    dbp2("equations to be solved: %d", eqs);

    // More equations than unknowns is an overdetermined system, certainly
    // inconsistent or redundant.
    if(eqs > unknowns) return FALSE;

    // Zero unknowns (and zero equations, since the prevous check passed)
    // is an empty system, which means that we solved successfully.
    if(unknowns == 0) return TRUE;

    // Before we start searching by brute force, let's see if we can
    // reuse a partition from last time.
    for(i = (RSp->sets - 1); i >= 0; i--) {
        // They have a subsystem of equations that perhaps we should
        // try. Start from an empty subset
        for(j = 0; j < SK->params; j++) {
            SK->param[j].mark = 0;
        }
        // Mark the unknowns in each equation of our remembered subset.
        for(j = 0; j < RSp->set[i].eqs; j++) {
            hEquation he = RSp->set[i].eq[j];
            int k;
            for(k = 0; k < EQ->eqns; k++) {
                if(EQ->eqn[k].he == he) {
                    // Don't try to grab the equation if it's already used.
                    if(EQ->eqn[k].subSys < 0) {
                        EQ->eqn[k].subSys = subSys;
                        Expr *pruned = EEvalKnown(EQ->eqn[k].e);
                        EMark(pruned, 1);
                    }
                }
            }
        }
        int eqn = EqnsInSubsys(subSys);
        int unkns = ParamsMarked();
        if(eqn == unkns && eqn > 0) {
            // This subsystem is ready to solve.
            goto got_exact;
        }
        // This subystem is not soluble, so those equations are free
        // to be partitioned later.
        for(j = 0; j < EQ->eqns; j++) {
            if(EQ->eqn[j].subSys == subSys) {
                EQ->eqn[j].subSys = -1;
            }
        }
        // Subsystems are less dangerous than assumptions (i.e., the
        // search paths they start terminate quicker) so we can keep
        // it around to try later, even if it doesn't work now.
    }

    // Right now we have a subsystem of zero equations, so that's in zero
    // unknowns.
    for(i = 0; i < SK->params; i++) {
        SK->param[i].mark = 0;
    }

    // Now try to find a possibly-consistent susbsystem, in the smallest
    // available number of unknowns.
    int depth;
    for(depth = 1; depth <= MAX_PARTITIONED_UNKNOWNS; depth++) {
        switch(SeekExactlyConstrained(subSys, depth, 0)) {
            case UNDER:
                // Couldn't find a soluble subsystem at the current depth,
                // so we have to look deeper.
                break;

            case OVER:
                // Single equation left where all the equations already
                // solved for, bad. We must 
                goto system_inconsistent;

            case EXACT:
                // What we're looking for.
                goto got_exact;
        }
    }
    // We give up; can't find a small subsystem to partition off and solve.
    // Instead let's just solve the whole mess at once. The assumer was
    // responsible for making the system exactly constrained, so as long
    // as we don't have too many eqs to solve, we should be fine.
    for(i = 0; i < SK->params; i++) {
        SK->param[i].mark = 0;
    }
    eqs = 0;
    for(i = 0; i < EQ->eqns; i++) {
        if(EQ->eqn[i].subSys < 0) {
            eqs++;
            EQ->eqn[i].subSys = subSys;
            EMark(EQ->eqn[i].e, 1);
        }
    }
    // Didn't want to waste time pruning our equations, so have to unmark
    // those we already solved for.
    for(i = 0; i < SK->params; i++) {
        if(SK->param[i].known) {
            SK->param[i].mark = 0;
        }
    }
    unknowns = ParamsMarked();
    if(eqs != unknowns) goto system_inconsistent; // Shouldn't happen anyways
    if(eqs > MAX_NUMERICAL_UNKNOWNS) goto system_inconsistent;

    // And now we solve, the same as if we had picked these off deliberately.
    // This subsystem will get remembered, which might be good or bad;
    // on the one hand that will save us from exhausting the search next
    // time, but on the other, it will stop us from discovering a better
    // partition if a change to the system permits that. Such a change
    // seems unlikely, at least without breaking the remembered subsystem,
    // so I'll leave it like this.

got_exact:
    // We picked off a possibly-consistent subsystem, which we can
    // now solve numerically. Let us do so.
    if(!SolveNewton(subSys)) {
        // What does this mean? It means that our subsystem might have been
        // consistent (n equations in n unknowns), but either it wasn't
        // (some eqns linearly dependent, linearized about current guess).
        // So give up, because this branch is now hopeless.
        goto system_inconsistent;
    }

    // Our solution succeed; so mark the parameters that we were solving
    // for as known. We must remember which parameters we marked as known;
    // if the system turns out to be inconsistent, then we must replace
    // them as unknown so that other solutions can be investigated.
    for(i = 0; i < SK->params; i++) {
        SketchParam *p = &(SK->param[i]);

        if(p->known) {
            // okay, nothing to do
        } else if(p->mark != 0) {
            // This is one of the unknowns that we just solved for.
            p->known = TRUE;
        }
    }

    // And let's try to solve the next subsystem.
    if(SolveSubSystemsStartingFrom(subSys + 1)) {
        // Everything worked; we've solved the full system. Let's make a
        // note of the subsystem we chose to use.
        //
        // Note that this list will get built in reverse order, because we
        // can't be sure that we chose a good subsystem until we've
        // confirmed that that leads to a consistent solution.
        int k = RSt->sets;
        if(k >= MAX_REMEMBERED_SUBSYSTEMS) oops();
        RSt->set[k].p = 0;
        RSt->set[k].eqs = 0;
        for(i = 0; i < EQ->eqns; i++) {
            if(EQ->eqn[i].subSys == subSys) {
                j = RSt->set[k].eqs;
                if(j >= MAX_NUMERICAL_UNKNOWNS) oops();
                RSt->set[k].eq[j] = EQ->eqn[i].he;
                RSt->set[k].eqs = (j + 1);
            }
        }
        RSt->sets = (k + 1);
        return TRUE;
    } else {
        goto system_inconsistent;
    }
    
system_inconsistent:
    // Either this subsystem was inconsistent itself, or we later on became
    // unable to make a consistent subsystem and yet still had unknowns
    // that we hadn't solved for. It might not be hopeless, though; if we
    // were called after an assumption was made, then maybe a different
    // assumption will work better.
    dbp2("so inconsistent");

    return FALSE;
}

//-----------------------------------------------------------------------------
// Change the parameters of the sketch in such a way as to make them satisfy
// the provided constraints.
//-----------------------------------------------------------------------------
void Solve(void)
{
    int i;

    CursorIsHourglass = FALSE;
    SolutionStartTime = GetTickCount();
   
    if(SK->eqnsDirty) {
        uiClearAssumptionsList();
        uiClearConstraintsList();
    }

    GenerateEquationsToSolve();

    // Our goal is to find the smallest solvable subsystem, and solve
    // it. This means that some previously unknown parameters have become
    // known; with luck, that has created another solvable subsystem,
    // and we can keep going.
    // 
    // The process will go quicker if we solve in smallest chunks possible,
    // and a practical sketch will tend to break down this way quite
    // readily.  Each subsystem will be solved by a Newton's method.

    // Mark all parameters except the references (x-axis, y-axis, origin)
    // as unknown to start.
    MarkUnknowns();

    // It's stupid to solve for an equation of the form p1 - p2 = 0 using
    // the Newton's method, and such equations arise routinely, from
    // things like coincidence constraints. Solve those by forward-
    // substitution now.
    SolveByForwardSubstitution();

    // This is where we decide if any assumptions are needed, and make them
    // if yes. If the system is provably inconsistent, then we give up now.
    int assumedParameters = 0;
    if(!Assume(&assumedParameters)) {
        uiSetConsistencyStatusText(" Inconsistent constraints.", BK_VIOLET);
        goto failed;
    }

    // Now let's count the unknowns, just to be sure.
    int unknowns;
    unknowns = 0;
    for(i = 0; i < SK->params; i++) {
        if(!SK->param[i].known) {
            unknowns++;
        }
    }
    // If everything's known at this point, then we're done.
    if(unknowns == 0) goto trivial;

    
    // If possible, instead of searching for the correct partition by
    // brute force, we will use the previous partition. In order to do
    // this, we must keep a record of the partition that we use as we
    // solve, so let's do that.
    RSt->sets = 0;
    // And all of our ideas from last time are still valid at this point.
    for(i = 0; i < RSp->sets; i++) {
        RSp->set[i].use = TRUE;
    }

    // Now start trying to make subsystems and solve them. This routine is
    // also responsible for identifying underconstrained situations, and
    // making appropriate assumptions.
    if(SolveSubSystemsStartingFrom(0)) {
        // It worked; we've found a solution.
    } else {
        // Not so good; we weren't able to pick off a set of subsystems
        // that all converged numerically.
        uiSetConsistencyStatusText(" Can't solve; no convergence.", BK_VIOLET);
        goto failed;
    }

trivial:
    if(assumedParameters > 0) {
        uiSetConsistencyStatusText(" Under-constrained system.", BK_YELLOW);
    } else {
        uiSetConsistencyStatusText(" Exactly constrained system.", BK_GREEN);
    }

    // Those unknowns that were solved by forward substitution can be
    // evaluated numerically now.
    for(i = 0; i < SK->params; i++) {
        if(SK->param[i].substd) {
            dbp2("asign: src=%08x, dest=%08x", SK->param[i].substd,
                SK->param[i].id);
            SK->param[i].v = EvalParam(SK->param[i].substd);
        }
    }

    // For debugging only.
    dbp2("%d subsystems", RSt->sets);
    for(i = 0; i < RSt->sets; i++) {
        dbp2("   subsystem %d in %d equations", i, RSt->set[i].eqs);
    }

    // We succeeded, so whatever partition we chose is worth remembering.
    RememberedSubsystems *rstemp;
    rstemp = RSp;
    RSp = RSt;
    RSt = rstemp;

    FreeAll();
    SK->eqnsDirty = FALSE;

    if(CursorIsHourglass) uiRestoreCursor();
    int out;
    out = GetTickCount();
    dbp2("time=%d", out - SolutionStartTime);

    SaveGoodParams();

    return;

failed:
    // It didn't work, probably due to a numerical problem. In that case
    // we should restore the previous parameter values, since the current
    // ones are probably screwed up.
    RestoreParamsToRemembered();
    if(SolvingState == SOLVING_AUTOMATICALLY) {
        RestoreParamsToLastGood();
    }

    FreeAll();
    SK->eqnsDirty = FALSE;

    out = GetTickCount();
    if(out - SolutionStartTime > 200) {
        // If we just spent a noticeable time solving to an inconsistent
        // system, then we probably don't want to keep doing this
        // interactively.
        StopSolving();
    }

    if(CursorIsHourglass) uiRestoreCursor();
}
