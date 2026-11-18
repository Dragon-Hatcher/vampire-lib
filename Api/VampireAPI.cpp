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

#include "Kernel/Term.hpp"
#include "Kernel/Clause.hpp"
#include "Kernel/Formula.hpp"
#include "Kernel/FormulaUnit.hpp"
#include "Kernel/Problem.hpp"
#include "Kernel/Signature.hpp"
#include "Kernel/HOL/HOL.hpp"
#include "Kernel/Inference.hpp"
#include "Kernel/InferenceStore.hpp"
#include "Kernel/OperatorType.hpp"
#include "Kernel/Theory.hpp"
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
    // Initialize the timer thread on first call (needed for timeout support)
    static bool timer_initialized = false;
    if (!timer_initialized) {
        Lib::Timer::reinitialise();
        timer_initialized = true;
    }

    // Reset elapsed time so the timer thread measures from now
    Lib::Timer::resetStartTime();

    // Clear any termination reason from a previous proof so the timer thread
    // and saturation loop don't immediately trigger
    env.statistics->terminationReason = Shell::TerminationReason::UNKNOWN;

    // Reset statistics fields that affect saturation behavior.
    // - activations: used to detect LRS start time and in reachable-count
    //   estimates; if inherited from P1 the LRS start time is never recorded
    //   for P2 (_lrsStartTime stays 0), corrupting LRS estimates.
    // - discardedNonRedundantClauses / inferencesSkippedDueToColors: used in
    //   isComplete(); if non-zero from P1, P2 returns REFUTATION_NOT_FOUND
    //   instead of SATISFIABLE when passive queue empties.
    env.statistics->activations = 0;
    env.statistics->discardedNonRedundantClauses = 0;
    env.statistics->inferencesSkippedDueToColors = 0;

    // Reset the preprocessing-end marker so that clauses created during the
    // next proof's preprocessing are correctly identified as preprocessing
    // clauses (isFromPreprocessing() returns true).  Without this reset the
    // stale value from the previous proof causes newly-created clauses to be
    // misclassified as saturation clauses and silently destroyed when their
    // reference count drops to zero during preprocessing.
    Unit::resetPreprocessingEnd();

    // Reset the global ordering so the next proof can set its own
    Ordering::unsetGlobalOrdering();

    // Reset static ordering caches that store results keyed to the previous
    // ordering object.  Without this, P2 can hit stale cached comparisons
    // from P1's ordering, causing the superposition algorithm to miss
    // inferences and return SATISFIABLE for a problem that is actually
    // unsatisfiable.
    Term::resetStaticCaches();
    AtomicSort::resetStaticCaches();
    PartialOrdering::resetStaticCaches();
    TermPartialOrdering::resetStaticCaches();
    TermOrderingDiagram::resetStaticCaches();
    EqualityProxyMono::resetStaticCaches();

    // Reset symbol usage counts.  The default symbol precedence (FREQUENCY)
    // sorts symbols by usage count.  After a proof, usage counts reflect
    // how often each symbol was used in that proof.  If not reset, the next
    // proof builds a KBO ordering with a different precedence, which can
    // block key inferences and cause the saturation to report SATISFIABLE
    // for a problem that is actually unsatisfiable.
    for (unsigned i = 0; i < env.signature->functions(); i++) {
        env.signature->getFunction(i)->resetUsageCnt();
    }
    for (unsigned i = 0; i < env.signature->predicates(); i++) {
        env.signature->getPredicate(i)->resetUsageCnt();
    }

    // Invalidate all KBO weight caches stored on shared terms.  The KBO
    // ordering object is recreated for each proof, so weights cached during
    // P1 are wrong for P2.  In release builds there is no per-term pointer
    // check (that only exists in VDEBUG), so without an epoch bump P2 would
    // silently reuse P1's stale weights and produce wrong ordering decisions.
    Term::invalidateKboWeightCache();

    // Reset cached equality argument orders on all shared literals.  The order
    // of equality arguments (which side is larger) is cached on each literal and
    // is only valid for the ordering that was active when it was set.  Without
    // this reset, P2's ordering silently reuses P1's orientations, which can
    // direct superposition inferences the wrong way and prevent finding a proof.
    env.sharing->resetEqualityArgumentOrders();

    // Reset EXIT_LOCK to allow proofs on different threads.
    // disableLimitEnforcement() locks EXIT_LOCK without unlocking,
    // which prevents subsequent proofs on different threads from completing
    Lib::Timer::resetLimitEnforcement();
}

void reset() {
    // Timeout support needs the timer reinitialised after a reset.
    Lib::Timer::reinitialise();

    Ordering::unsetGlobalOrdering();

    Term::resetStaticCaches();
    AtomicSort::resetStaticCaches();
    PartialOrdering::resetStaticCaches();
    TermPartialOrdering::resetStaticCaches();
    TermOrderingDiagram::resetStaticCaches();
    EqualityProxyMono::resetStaticCaches();

    InferenceStore::instance()->reset();

    delete env.sharing;
    delete env.signature;
    delete env.statistics;

    env.signature = new Signature();
    env.signature->addEquality();  // Must add equality predicate (normally done in Environment constructor)
    env.sharing = new TermSharing();
    env.statistics = new Statistics();

    // The old problem is left alone (not deleted) since the caller may still hold a reference to it.
}

Options& options() {
    return *env.options;
}

Signature& signature() {
    return *env.signature;
}

Statistics& statistics() {
    return *env.statistics;
}

// ===========================================
// Symbol Registration
// ===========================================

unsigned addFunction(const std::string& name, unsigned arity) {
    unsigned functor = env.signature->addFunction(name, arity);

    // Set a default type (all arguments and result are default sort)
    TermList defSort = AtomicSort::defaultSort();
    Stack<TermList> argSorts;
    for (unsigned i = 0; i < arity; i++) {
        argSorts.push(defSort);
    }

    env.signature->getFunction(functor)->setType(
        OperatorType::getFunctionType(arity, argSorts.begin(), defSort));

    return functor;
}

unsigned addPredicate(const std::string& name, unsigned arity) {
    unsigned pred = env.signature->addPredicate(name, arity);

    // Set a default type (all arguments are default sort)
    TermList defSort = AtomicSort::defaultSort();
    Stack<TermList> argSorts;
    for (unsigned i = 0; i < arity; i++) {
        argSorts.push(defSort);
    }

    env.signature->getPredicate(pred)->setType(
        OperatorType::getPredicateType(arity, argSorts.begin()));

    return pred;
}

unsigned addInterpretedFunction(const std::string& name, Interpretation interp) {
    return env.signature->addInterpretedFunction(interp, name);
}

unsigned addInterpretedPredicate(const std::string& name, Interpretation interp) {
    return env.signature->addInterpretedPredicate(interp, name);
}

unsigned addIntegerConstant(const std::string& value) {
    auto n = IntegerConstantType::parse(value);
    if (n.isNone()) return 0;
    return env.signature->addNumeralConstant(*n);
}

unsigned addRationalConstant(const std::string& value) {
    auto n = RationalConstantType::parse(value);
    if (n.isNone()) return 0;
    return env.signature->addNumeralConstant(*n);
}

unsigned addRealConstant(const std::string& value) {
    auto n = RealConstantType::parse(value);
    if (n.isNone()) return 0;
    return env.signature->addNumeralConstant(*n);
}

// ===========================================
// Sort Registration (TFF)
// ===========================================

// Compound (arrow) sorts are Kernel terms, not signature type-cons, so they
// cannot ride the unsigned type-con id space directly: registered here and
// flagged with the high bit, resolved transparently in reconstructSort so
// EVERY typed entry point (typed functions, quantifiers, equality) accepts
// them unchanged.
static std::vector<TermList> s_compoundSorts;
static const unsigned COMPOUND_SORT_FLAG = 0x80000000u;

unsigned sortArrow(unsigned from, unsigned to);

static TermList reconstructSort(unsigned id) {
    if (id & COMPOUND_SORT_FLAG) return s_compoundSorts[id & ~COMPOUND_SORT_FLAG];
    if (id == env.signature->getDefaultSort()) return AtomicSort::defaultSort();
    if (id == env.signature->getBoolSort()) return AtomicSort::boolSort();
    if (id == env.signature->getIntSort()) return AtomicSort::intSort();
    if (id == env.signature->getRealSort()) return AtomicSort::realSort();
    if (id == env.signature->getRatSort()) return AtomicSort::rationalSort();
    return TermList(AtomicSort::createConstant(id));
}

unsigned addSort(const std::string& name) {
    bool added;
    return env.signature->addTypeCon(name, 0, added);
}

unsigned defaultSort() { return env.signature->getDefaultSort(); }

unsigned sortArrow(unsigned from, unsigned to) {
    TermList arrow = AtomicSort::arrowSort(reconstructSort(from), reconstructSort(to));
    s_compoundSorts.push_back(arrow);
    return COMPOUND_SORT_FLAG | (unsigned)(s_compoundSorts.size() - 1);
}

// HOL application (curried `f @ x`); sorts inferred from the (proper-term)
// head via SortHelper.
TermList holApp(TermList head, TermList arg) {
    return HOL::create::app(head, arg);
}

// HOL application with an explicit head sort — REQUIRED when the head is a
// variable (`P @ X` under a `! [P: $i > $o]` binder): sort inference needs a
// proper term, so the caller supplies the binder's arrow sort.
TermList holAppSorted(TermList head, unsigned headSortIndex, TermList arg) {
    return HOL::create::app(reconstructSort(headSortIndex), head, arg);
}

// A Boolean-sorted term in formula position (`X: $o` used as a formula).
Formula* boolTermFormula(TermList t) {
    return new BoolTermFormula(t);
}

// A formula in term position (FOOL: an `$o` argument that is a compound
// formula, e.g. `knows @ a @ (p & q)`).
TermList formulaTerm(Formula* f) {
    return TermList(Term::createFormula(f));
}
unsigned sortBool() { return env.signature->getBoolSort(); }
unsigned sortInt() { return env.signature->getIntSort(); }
unsigned sortReal() { return env.signature->getRealSort(); }
unsigned sortRational() { return env.signature->getRatSort(); }

unsigned addTypedFunction(const std::string& name, const std::vector<unsigned>& argSortIndices, unsigned returnSortIndex) {
    unsigned functor = env.signature->addFunction(name, argSortIndices.size());
    Stack<TermList> args;
    for (unsigned s : argSortIndices) args.push(reconstructSort(s));
    env.signature->getFunction(functor)->setType(
        OperatorType::getFunctionType(argSortIndices.size(), args.begin(), reconstructSort(returnSortIndex)));
    return functor;
}

unsigned addTypedPredicate(const std::string& name, const std::vector<unsigned>& argSortIndices) {
    unsigned pred = env.signature->addPredicate(name, argSortIndices.size());
    Stack<TermList> args;
    for (unsigned s : argSortIndices) args.push(reconstructSort(s));
    env.signature->getPredicate(pred)->setType(
        OperatorType::getPredicateType(argSortIndices.size(), args.begin()));
    return pred;
}

// ===========================================
// Term Construction
// ===========================================

TermList var(unsigned index) {
    return TermList(index, false);  // false = ordinary variable
}

TermList constant(unsigned functor) {
    return TermList(Term::createConstant(functor));
}

TermList term(unsigned functor, std::initializer_list<TermList> args) {
    return TermList(Term::create(functor, args));
}

TermList term(unsigned functor, const std::vector<TermList>& args) {
    // Term::create expects (functor, arity, const TermList* args)
    return TermList(Term::create(functor, args.size(), args.data()));
}

// ===========================================
// Literal Construction
// ===========================================

Literal* eq(bool positive, TermList lhs, TermList rhs) {
    return Literal::createEquality(positive, lhs, rhs, AtomicSort::defaultSort());
}

Literal* lit(unsigned pred, bool positive, std::initializer_list<TermList> args) {
    return Literal::create(pred, positive, args);
}

Literal* lit(unsigned pred, bool positive, const std::vector<TermList>& args) {
    // Literal::create expects (predicate, arity, polarity, TermList* args)
    // Note: const_cast is safe here as Literal::create doesn't modify args
    return Literal::create(pred, args.size(), positive, const_cast<TermList*>(args.data()));
}

Literal* neg(Literal* l) {
    return Literal::complementaryLiteral(l);
}

// ===========================================
// Formula Construction
// ===========================================

Formula* atom(Literal* l) {
    return new AtomicFormula(l);
}

Formula* notF(Formula* f) {
    return new NegatedFormula(f);
}

Formula* andF(std::initializer_list<Formula*> fs) {
    FormulaList* args = nullptr;
    for (Formula* f : fs) {
        FormulaList::push(f, args);
    }
    return new JunctionFormula(AND, args);
}

Formula* andF(const std::vector<Formula*>& fs) {
    FormulaList* args = nullptr;
    for (Formula* f : fs) {
        FormulaList::push(f, args);
    }
    return new JunctionFormula(AND, args);
}

Formula* orF(std::initializer_list<Formula*> fs) {
    FormulaList* args = nullptr;
    for (Formula* f : fs) {
        FormulaList::push(f, args);
    }
    return new JunctionFormula(OR, args);
}

Formula* orF(const std::vector<Formula*>& fs) {
    FormulaList* args = nullptr;
    for (Formula* f : fs) {
        FormulaList::push(f, args);
    }
    return new JunctionFormula(OR, args);
}

Formula* impF(Formula* lhs, Formula* rhs) {
    return new BinaryFormula(IMP, lhs, rhs);
}

Formula* iffF(Formula* lhs, Formula* rhs) {
    return new BinaryFormula(IFF, lhs, rhs);
}

Formula* forallF(unsigned varIndex, Formula* f) {
    VList* vars = nullptr;
    VList::push(varIndex, vars);
    return new QuantifiedFormula(FORALL, vars, nullptr, f);
}

Formula* existsF(unsigned varIndex, Formula* f) {
    VList* vars = nullptr;
    VList::push(varIndex, vars);
    return new QuantifiedFormula(EXISTS, vars, nullptr, f);
}

Formula* forallTyped(unsigned varIndex, unsigned sortIndex, Formula* f) {
    VList* vars = nullptr;
    VList::push(varIndex, vars);
    SList* sorts = nullptr;
    SList::push(reconstructSort(sortIndex), sorts);
    return new QuantifiedFormula(FORALL, vars, sorts, f);
}

Formula* existsTyped(unsigned varIndex, unsigned sortIndex, Formula* f) {
    VList* vars = nullptr;
    VList::push(varIndex, vars);
    SList* sorts = nullptr;
    SList::push(reconstructSort(sortIndex), sorts);
    return new QuantifiedFormula(EXISTS, vars, sorts, f);
}

Literal* typedEq(bool positive, TermList lhs, TermList rhs, unsigned sortIndex) {
    return Literal::createEquality(positive, lhs, rhs, reconstructSort(sortIndex));
}

Formula* trueF() {
    return Formula::trueFormula();
}

Formula* falseF() {
    return Formula::falseFormula();
}

Unit* axiomF(Formula* f) {
    return new FormulaUnit(f, FromInput(UnitInputType::AXIOM));
}

Unit* conjectureF(Formula* f) {
    // For refutation-based proving, negate the conjecture
    Formula* negated = new NegatedFormula(f);
    return new FormulaUnit(negated, FromInput(UnitInputType::NEGATED_CONJECTURE));
}

// ===========================================
// Clause Construction
// ===========================================

Clause* axiom(std::initializer_list<Literal*> literals) {
    return clause(literals, UnitInputType::AXIOM);
}

Clause* axiom(const std::vector<Literal*>& literals) {
    return clause(literals, UnitInputType::AXIOM);
}

Clause* conjecture(std::initializer_list<Literal*> literals) {
    return clause(literals, UnitInputType::NEGATED_CONJECTURE);
}

Clause* conjecture(const std::vector<Literal*>& literals) {
    return clause(literals, UnitInputType::NEGATED_CONJECTURE);
}

Clause* clause(std::initializer_list<Literal*> literals, UnitInputType inputType) {
    return Clause::fromLiterals(literals,
        NonspecificInference0(inputType, InferenceRule::INPUT));
}

Clause* clause(const std::vector<Literal*>& literals, UnitInputType inputType) {
    // Use fromArray instead of fromLiterals for vector input
    return Clause::fromArray(literals.data(), literals.size(),
        NonspecificInference0(inputType, InferenceRule::INPUT));
}

// ===========================================
// Problem and Proving
// ===========================================

Problem* problem(std::initializer_list<Clause*> clauses) {
    UnitList* units = nullptr;
    for (Clause* c : clauses) {
        UnitList::push(c, units);
    }
    Problem* prb = new Problem(units);
    env.setMainProblem(prb);
    return prb;
}

Problem* problem(const std::vector<Clause*>& clauses) {
    UnitList* units = nullptr;
    for (Clause* c : clauses) {
        UnitList::push(c, units);
    }
    Problem* prb = new Problem(units);
    env.setMainProblem(prb);
    return prb;
}

Problem* problem(std::initializer_list<Unit*> units) {
    UnitList* unitList = nullptr;
    for (Unit* u : units) {
        UnitList::push(u, unitList);
    }
    Problem* prb = new Problem(unitList);
    env.setMainProblem(prb);
    return prb;
}

Problem* problem(const std::vector<Unit*>& units) {
    UnitList* unitList = nullptr;
    for (Unit* u : units) {
        UnitList::push(u, unitList);
    }
    Problem* prb = new Problem(unitList);
    env.setMainProblem(prb);
    return prb;
}

static unsigned _proveCallCount = 0;

ProofResult prove(Problem* prb) {
    env.setMainProblem(prb);

    _proveCallCount++;
    bool debugThisCall = false;
    if (debugThisCall) {
        env.options->set("show_preprocessing", "on");
        std::cerr << "[DEBUG] Proving call #" << _proveCallCount << ", input units:" << std::endl;
        UnitList::Iterator it(prb->units());
        while (it.hasNext()) {
            Unit* u = it.next();
            std::cerr << "  " << u->toString() << std::endl;
        }
    }

    try {
        Shell::Preprocess prepro(*env.options);
        prepro.preprocess(*prb);
    } catch (Lib::UserErrorException& e) {
        std::cerr << "[Vampire API] Preprocessing UserError: " << e.msg() << std::endl;
        return ProofResult::UNKNOWN;
    } catch (Lib::Exception& e) {
        std::cerr << "[Vampire API] Preprocessing error: " << e.msg() << std::endl;
        return ProofResult::UNKNOWN;
    }

    if (debugThisCall) {
        std::cerr << "[DEBUG] After preprocessing, units:" << std::endl;
        UnitList::Iterator it2(prb->units());
        while (it2.hasNext()) {
            Unit* u = it2.next();
            std::cerr << "  " << u->toString() << std::endl;
        }
        env.options->set("show_preprocessing", "off");
    }

    ProvingHelper::runVampireSaturation(*prb, *env.options);

    switch (env.statistics->terminationReason) {
        case TerminationReason::REFUTATION:
            return ProofResult::PROOF;
        case TerminationReason::SATISFIABLE:
            return ProofResult::SATISFIABLE;
        case TerminationReason::TIME_LIMIT:
        case TerminationReason::INSTRUCTION_LIMIT:
            return ProofResult::TIMEOUT;
        case TerminationReason::MEMORY_LIMIT:
            return ProofResult::MEMORY_LIMIT;
        case TerminationReason::REFUTATION_NOT_FOUND:
            return ProofResult::INCOMPLETE;
        default:
            return ProofResult::UNKNOWN;
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
        if (u->isClause()) {
            results.push(u);
        } else {
            FormulaUnit* fu = static_cast<FormulaUnit*>(u);
            Stack<Clause*> clauses;
            newCnf.clausify(fu, clauses);
            if (clauses.isEmpty()) {
                Formula* f = fu->formula();
                if (f->connective() == LITERAL) {
                    Literal* lp = f->literal();
                    results.push(Clause::fromArray(&lp, 1, fu->inference()));
                } else if (f->connective() == OR) {
                    Stack<Literal*> lits;
                    FormulaList::Iterator fit(f->args());
                    bool all = true;
                    while (fit.hasNext()) {
                        Formula* arg = fit.next();
                        if (arg->connective() == LITERAL) lits.push(arg->literal());
                        else { all = false; break; }
                    }
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
    while (it.hasNext()) {
        Unit* u = it.next();
        if (u->isClause()) res.push_back(clauseToString(static_cast<Clause*>(u)));
    }
    return res;
}

void destroyUnit(Unit* u) {
    if (!u) return;
    if (u->isClause()) static_cast<Clause*>(u)->decRefCnt();
    else static_cast<FormulaUnit*>(u)->destroy();
}

Unit* getRefutation() {
    return env.statistics->refutation;
}

void printProof(std::ostream& out, Unit* refutation) {
    if (refutation) {
        InferenceStore::instance()->outputProof(out, refutation);
    }
}

// ===========================================
// Structured Proof Access
// ===========================================

Clause* ProofStep::clause() const {
    if (unit && unit->isClause()) {
        return static_cast<Clause*>(unit);
    }
    return nullptr;
}

bool ProofStep::isEmpty() const {
    Clause* c = clause();
    return c && c->isEmpty();
}

std::string ProofStep::ruleName() const {
    return Kernel::ruleName(rule);
}

std::string ProofStep::inputTypeName() const {
    return Kernel::inputTypeName(inputType);
}

std::string termToString(TermList t) {
    std::ostringstream oss;
    oss << t;
    return oss.str();
}

std::string literalToString(Literal* l) {
    std::ostringstream oss;
    oss << *l;
    return oss.str();
}

std::string clauseToString(Clause* c) {
    std::ostringstream oss;
    if (c->isEmpty()) {
        oss << "$false";
    } else {
        for (unsigned i = 0; i < c->length(); i++) {
            if (i > 0) oss << " | ";
            oss << *(*c)[i];
        }
    }
    return oss.str();
}

std::string formulaToString(Formula* f) {
    std::ostringstream oss;
    oss << *f;
    return oss.str();
}

std::vector<Literal*> getLiterals(Clause* c) {
    std::vector<Literal*> result;
    if (c) {
        for (unsigned i = 0; i < c->length(); i++) {
            result.push_back((*c)[i]);
        }
    }
    return result;
}

std::vector<ProofStep> extractProof(Unit* refutation) {
    std::vector<ProofStep> steps;
    if (!refutation) {
        return steps;
    }

    // Collect all units in the proof DAG using DFS
    DHSet<unsigned> visited;
    Stack<Unit*> toProcess;
    std::vector<Unit*> unitsInOrder;

    toProcess.push(refutation);

    while (!toProcess.isEmpty()) {
        Unit* current = toProcess.pop();

        if (!visited.insert(current->number())) {
            continue;  // Already visited
        }

        unitsInOrder.push_back(current);

        // Add parents (premises) to the stack
        Inference& inf = current->inference();
        Inference::Iterator it = inf.iterator();
        while (inf.hasNext(it)) {
            Unit* parent = inf.next(it);
            toProcess.push(parent);
        }
    }

    // Reverse to get topological order (premises before conclusions)
    std::reverse(unitsInOrder.begin(), unitsInOrder.end());

    // Build ProofStep for each unit
    for (Unit* u : unitsInOrder) {
        ProofStep step;
        step.id = u->number();
        step.unit = u;

        Inference& inf = u->inference();
        step.rule = inf.rule();
        step.inputType = inf.inputType();

        // Collect premise IDs
        Inference::Iterator it = inf.iterator();
        while (inf.hasNext(it)) {
            Unit* parent = inf.next(it);
            step.premiseIds.push_back(parent->number());
        }

        steps.push_back(step);
    }

    return steps;
}

} // namespace Api
