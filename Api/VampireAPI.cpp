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
#include "Kernel/Inference.hpp"
#include "Kernel/InferenceStore.hpp"
#include "Kernel/OperatorType.hpp"
#include "Kernel/Unit.hpp"

#include "Indexing/TermSharing.hpp"

#include "Shell/Options.hpp"
#include "Shell/Statistics.hpp"
#include "Shell/Preprocess.hpp"

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
    // Reinitialize the timer (needed for timeout support after reset)
    Lib::Timer::reinitialise();

    // Reset the global ordering
    Ordering::unsetGlobalOrdering();

    // Reset all static caches in the kernel
    Term::resetStaticCaches();
    AtomicSort::resetStaticCaches();
    PartialOrdering::resetStaticCaches();
    TermPartialOrdering::resetStaticCaches();
    TermOrderingDiagram::resetStaticCaches();

    // Reset shell static caches
    EqualityProxyMono::resetStaticCaches();

    // Reset the inference store
    InferenceStore::instance()->reset();

    // Delete and recreate the environment components
    // Note: Order matters here due to dependencies
    delete env.sharing;
    delete env.signature;
    delete env.statistics;

    env.signature = new Signature();
    env.signature->addEquality();  // Must add equality predicate (normally done in Environment constructor)
    env.sharing = new TermSharing();
    env.statistics = new Statistics();

    // Reset the main problem reference
    // Note: We don't delete the old problem as it may still be referenced
    // by the user. The user is responsible for managing problem lifetime.
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
    // Ensure problem is set
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

    // Preprocess if we have formulas (convert to clauses)
    Shell::Preprocess prepro(*env.options);
    prepro.preprocess(*prb);

    if (debugThisCall) {
        std::cerr << "[DEBUG] After preprocessing, units:" << std::endl;
        UnitList::Iterator it2(prb->units());
        while (it2.hasNext()) {
            Unit* u = it2.next();
            std::cerr << "  " << u->toString() << std::endl;
        }
        env.options->set("show_preprocessing", "off");
    }

    // Run the saturation algorithm
    ProvingHelper::runVampireSaturation(*prb, *env.options);

    // Convert termination reason to result
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
