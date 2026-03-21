/*
 * This file is part of the source code of the software program
 * Vampire. It is protected by applicable
 * copyright laws.
 *
 * This source code is distributed under the licence found here
 * https://vprover.github.io/license.html
 * and in the source directory
 */
/**
 * @file VampireAPI.cpp
 * Implementation of the public Vampire API.
 */

#include "VampireAPI.hpp"

#include "Lib/Environment.hpp"
#include "Lib/List.hpp"
#include "Lib/Stack.hpp"
#include "Lib/DHSet.hpp"

#include <sstream>
#include <algorithm>
#include <cstring>

#include "Kernel/Term.hpp"
#include "Kernel/Clause.hpp"
#include "Kernel/Formula.hpp"
#include "Kernel/FormulaUnit.hpp"
#include "Kernel/Problem.hpp"
#include "Kernel/Signature.hpp"
#include "Kernel/Inference.hpp"
#include "Kernel/InferenceStore.hpp"
#include "Kernel/OperatorType.hpp"
#include "Kernel/Unit.hpp"

#include "Indexing/TermSharing.hpp"

#include "Shell/Options.hpp"
#include "Shell/Statistics.hpp"
#include "Shell/Preprocess.hpp"
#include "Shell/NNF.hpp"
#include "Shell/CNF.hpp"
#include "Shell/NewCNF.hpp"

#include "Saturation/ProvingHelper.hpp"

#include "Lib/Timer.hpp"

#include "Kernel/MainLoop.hpp"
#include "Kernel/Ordering.hpp"
#include "Kernel/PartialOrdering.hpp"
#include "Kernel/TermPartialOrdering.hpp"
#include "Kernel/TermOrderingDiagram.hpp"

#include "Shell/EqualityProxyMono.hpp"

namespace Api {

using namespace Lib;
using namespace Kernel;
using namespace Shell;
using namespace Indexing;
using namespace Saturation;

void prepareForNextProof() {
    static bool timer_initialized = false;
    if (!timer_initialized) {
        Lib::Timer::reinitialise();
        timer_initialized = true;
    }
    Lib::Timer::resetStartTime();
    env.statistics->terminationReason = Shell::TerminationReason::UNKNOWN;
    env.statistics->activations = 0;
    env.statistics->discardedNonRedundantClauses = 0;
    env.statistics->inferencesSkippedDueToColors = 0;
    env.statistics->refutation = nullptr;
    Unit::resetPreprocessingEnd();
    Ordering::unsetGlobalOrdering();
    Lib::Timer::resetLimitEnforcement();
}

void reset() {
    Lib::Timer::reinitialise();
    Ordering::unsetGlobalOrdering();
    Term::resetStaticCaches();
    AtomicSort::resetStaticCaches();
    PartialOrdering::resetStaticCaches();
    TermPartialOrdering::resetStaticCaches();
    TermOrderingDiagram::resetStaticCaches();
    EqualityProxyMono::resetStaticCaches();
    
    delete env.statistics;
    delete env.sharing;
    delete env.signature;

    env.statistics = new Statistics();
    env.signature = new Signature();
    env.sharing = new Indexing::TermSharing();

    env.signature->addEquality();
    AtomicSort::defaultSort();
    AtomicSort::boolSort();
    AtomicSort::intSort();
    AtomicSort::realSort();
    AtomicSort::rationalSort();

    InferenceStore::instance()->reset();
    env.setMainProblem(nullptr);
}

Options& options() { return *env.options; }
Signature& signature() { return *env.signature; }
Statistics& statistics() { return *env.statistics; }

unsigned addFunction(const std::string& name, unsigned arity) { return env.signature->addFunction(name, arity); }
unsigned addPredicate(const std::string& name, unsigned arity) {
    unsigned pred = env.signature->addPredicate(name, arity);
    TermList defSort = AtomicSort::defaultSort();
    Stack<TermList> argSorts;
    for (unsigned i = 0; i < arity; i++) argSorts.push(defSort);
    env.signature->getPredicate(pred)->setType(OperatorType::getPredicateType(arity, argSorts.begin()));
    return pred;
}

unsigned addInterpretedFunction(const std::string& name, Interpretation interp) { return env.signature->addInterpretedFunction(interp, name); }
unsigned addInterpretedPredicate(const std::string& name, Interpretation interp) { return env.signature->addInterpretedPredicate(interp, name); }
unsigned addIntegerConstant(const std::string& value) { auto n = IntegerConstantType::parse(value); return n.isNone() ? 0 : env.signature->addNumeralConstant(*n); }
unsigned addRationalConstant(const std::string& value) { auto n = RationalConstantType::parse(value); return n.isNone() ? 0 : env.signature->addNumeralConstant(*n); }
unsigned addRealConstant(const std::string& value) { auto n = RealConstantType::parse(value); return n.isNone() ? 0 : env.signature->addNumeralConstant(*n); }

static TermList reconstructSort(unsigned typeConIndex) { return TermList(AtomicSort::createConstant(typeConIndex)); }
unsigned addSort(const std::string& name) { bool added; return env.signature->addTypeCon(name, 0, added); }
unsigned defaultSort() { return env.signature->getDefaultSort(); }
unsigned sortBool() { return env.signature->getBoolSort(); }
unsigned sortInt() { return env.signature->getIntSort(); }
unsigned sortReal() { return env.signature->getRealSort(); }
unsigned sortRational() { return env.signature->getRatSort(); }

unsigned addTypedFunction(const std::string& name, const std::vector<unsigned>& argSortIndices, unsigned returnSortIndex) {
    unsigned functor = env.signature->addFunction(name, argSortIndices.size());
    Stack<TermList> args;
    for (unsigned s : argSortIndices) args.push(reconstructSort(s));
    env.signature->getFunction(functor)->setType(OperatorType::getFunctionType(argSortIndices.size(), args.begin(), reconstructSort(returnSortIndex)));
    return functor;
}
unsigned addTypedPredicate(const std::string& name, const std::vector<unsigned>& argSortIndices) {
    unsigned pred = env.signature->addPredicate(name, argSortIndices.size());
    Stack<TermList> args;
    for (unsigned s : argSortIndices) args.push(reconstructSort(s));
    env.signature->getPredicate(pred)->setType(OperatorType::getPredicateType(argSortIndices.size(), args.begin()));
    return pred;
}

TermList constant(unsigned functor) { return TermList(Term::create(functor, 0, nullptr)); }
TermList var(unsigned index) { return TermList::var(index); }
TermList term(unsigned functor, const std::vector<TermList>& args) { return TermList(Term::create(functor, args.size(), const_cast<TermList*>(args.data()))); }

Literal* eq(bool positive, TermList lhs, TermList rhs) { return Literal::createEquality(positive, lhs, rhs, AtomicSort::defaultSort()); }
Literal* lit(unsigned pred, bool positive, const std::vector<TermList>& args) { return Literal::create(pred, args.size(), positive, const_cast<TermList*>(args.data())); }
Literal* neg(Literal* l) { return Literal::complementaryLiteral(l); }

Formula* atom(Literal* l) { return new AtomicFormula(l); }
Formula* notF(Formula* f) { return new NegatedFormula(f); }
Formula* andF(const std::vector<Formula*>& fs) { FormulaList* list = nullptr; for (auto it = fs.rbegin(); it != fs.rend(); ++it) FormulaList::push(*it, list); return new JunctionFormula(AND, list); }
Formula* orF(const std::vector<Formula*>& fs) { FormulaList* list = nullptr; for (auto it = fs.rbegin(); it != fs.rend(); ++it) FormulaList::push(*it, list); return new JunctionFormula(OR, list); }
Formula* impF(Formula* lhs, Formula* rhs) { return new BinaryFormula(IMP, lhs, rhs); }
Formula* iffF(Formula* lhs, Formula* rhs) { return new BinaryFormula(IFF, lhs, rhs); }
Formula* forallF(unsigned varIndex, Formula* f) { VList* vars = nullptr; VList::push(varIndex, vars); return new QuantifiedFormula(FORALL, vars, nullptr, f); }
Formula* existsF(unsigned varIndex, Formula* f) { VList* vars = nullptr; VList::push(varIndex, vars); return new QuantifiedFormula(EXISTS, vars, nullptr, f); }

Formula* forallTyped(unsigned varIndex, unsigned sortIndex, Formula* f) { VList* vars = nullptr; VList::push(varIndex, vars); SList* sorts = nullptr; SList::push(reconstructSort(sortIndex), sorts); return new QuantifiedFormula(FORALL, vars, sorts, f); }
Formula* existsTyped(unsigned varIndex, unsigned sortIndex, Formula* f) { VList* vars = nullptr; VList::push(varIndex, vars); SList* sorts = nullptr; SList::push(reconstructSort(sortIndex), sorts); return new QuantifiedFormula(EXISTS, vars, sorts, f); }
Literal* typedEq(bool positive, TermList lhs, TermList rhs, unsigned sortIndex) { return Literal::createEquality(positive, lhs, rhs, reconstructSort(sortIndex)); }

Formula* trueF() { return Formula::trueFormula(); }
Formula* falseF() { return Formula::falseFormula(); }

Unit* axiomF(Formula* f) { return new FormulaUnit(f, Inference(FromInput(UnitInputType::AXIOM))); }
Unit* conjectureF(Formula* f) { Formula* negated = new NegatedFormula(f); return new FormulaUnit(negated, Inference(FromInput(UnitInputType::NEGATED_CONJECTURE))); }

Clause* axiom(const std::vector<Literal*>& literals) { return Clause::fromArray(literals.data(), literals.size(), NonspecificInference0(UnitInputType::AXIOM, InferenceRule::INPUT)); }
Clause* conjecture(const std::vector<Literal*>& literals) { return Clause::fromArray(literals.data(), literals.size(), NonspecificInference0(UnitInputType::NEGATED_CONJECTURE, InferenceRule::INPUT)); }
Clause* clause(const std::vector<Literal*>& literals, UnitInputType inputType) { return Clause::fromArray(literals.data(), literals.size(), NonspecificInference0(inputType, InferenceRule::INPUT)); }

Problem* problem(const std::vector<Unit*>& units) {
    UnitList* unitList = nullptr;
    for (int i = (int)units.size() - 1; i >= 0; i--) UnitList::push(units[i], unitList);
    Problem* prb = new Problem(unitList);
    prb->invalidateEverything();
    return prb;
}

ProofResult prove(Problem* prb) {
    env.setMainProblem(prb);
    ProvingHelper::runVampireSaturation(*prb, *env.options);
    switch (env.statistics->terminationReason) {
        case TerminationReason::REFUTATION: return ProofResult::PROOF;
        case TerminationReason::SATISFIABLE: return ProofResult::SATISFIABLE;
        default: return ProofResult::UNKNOWN;
    }
}

void clausify(Problem* prb) {
    if (!prb || !prb->units()) return;
    env.setMainProblem(prb);
    Stack<Unit*> results;
    UnitList::Iterator it(prb->units());
    NewCNF newCnf(env.options->naming());
    while (it.hasNext()) {
        Unit* u = it.next();
        if (u->isClause()) results.push(u);
        else {
            FormulaUnit* fu = static_cast<FormulaUnit*>(u);
            Stack<Clause*> clauses;
            newCnf.clausify(fu, clauses);
            if (clauses.isEmpty()) {
                Formula* f = fu->formula();
                if (f->connective() == LITERAL) { Literal* lp = f->literal(); results.push(Clause::fromArray(&lp, 1, fu->inference())); }
                else if (f->connective() == OR) {
                    Stack<Literal*> lits; FormulaList::Iterator fit(f->args()); bool all = true;
                    while(fit.hasNext()) { Formula* arg = fit.next(); if (arg->connective() == LITERAL) lits.push(arg->literal()); else { all = false; break; } }
                    if (all) results.push(Clause::fromArray(lits.begin(), lits.size(), fu->inference()));
                }
            } else {
                Stack<Unit*> temp;
                while (clauses.isNonEmpty()) temp.push(clauses.pop());
                while (temp.isNonEmpty()) results.push(temp.pop());
            }
        }
    }
    prb->units() = nullptr;
    prb->invalidateEverything();
    UnitList* newList = nullptr;
    while (results.isNonEmpty()) UnitList::push(results.pop(), newList);
    prb->addUnits(newList);
}

std::vector<std::string> getCNF(Problem* prb) {
    std::vector<std::string> res;
    if (!prb || !prb->units()) return res;
    UnitList::Iterator it(prb->units());
    while (it.hasNext()) { Unit* u = it.next(); if (u->isClause()) res.push_back(clauseToString(static_cast<Clause*>(u))); }
    return res;
}

void destroyUnit(Unit* u) { if (!u) return; if (u->isClause()) static_cast<Clause*>(u)->decRefCnt(); else static_cast<FormulaUnit*>(u)->destroy(); }
Unit* getRefutation() { return env.statistics->refutation; }
void printProof(std::ostream& out, Unit* refutation) { if (refutation) InferenceStore::instance()->outputProof(out, refutation); }

std::vector<ProofStep> extractProof(Unit* refutation) {
    std::vector<ProofStep> steps; if (!refutation) return steps;
    Stack<Unit*> todo; todo.push(refutation); DHSet<Unit*> seen; std::vector<Unit*> proofUnits;
    while (todo.isNonEmpty()) {
        Unit* u = todo.pop();
        if (seen.insert(u)) {
            proofUnits.push_back(u); Inference& inf = u->inference(); auto it = inf.iterator();
            while (inf.hasNext(it)) todo.push(inf.next(it));
        }
    }
    std::reverse(proofUnits.begin(), proofUnits.end());
    for (Unit* u : proofUnits) {
        ProofStep step; step.id = u->number(); step.rule = u->inference().rule();
        step.inputType = u->inputType(); step.unit = u;
        Inference& inf = u->inference(); auto it = inf.iterator();
        while (inf.hasNext(it)) step.premiseIds.push_back(inf.next(it)->number());
        steps.push_back(step);
    }
    return steps;
}

Clause* ProofStep::clause() const { return (unit && unit->isClause()) ? static_cast<Clause*>(unit) : nullptr; }
bool ProofStep::isEmpty() const { Clause* cl = clause(); return cl && cl->isEmpty(); }
std::string ProofStep::ruleName() const { return Kernel::ruleName(rule); }
std::string ProofStep::inputTypeName() const { return Kernel::inputTypeName(inputType); }

std::string termToString(TermList t) { return t.toString(); }
std::string literalToString(Literal* l) { return l->toString(); }
std::string clauseToString(Clause* c) {
    if (!c) return "null";
    if (c->isEmpty()) return "$false";
    std::ostringstream oss;
    for (unsigned i = 0; i < c->length(); i++) { if (i > 0) oss << " | "; oss << (*c)[i]->toString(); }
    return oss.str();
}
std::string formulaToString(Formula* f) { return f->toString(); }

} // namespace Api
