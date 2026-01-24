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

static bool s_initialized = false;

void init() {
    if (s_initialized) return;
    // The global env is auto-constructed before main().
    // Just mark as initialized.
    s_initialized = true;
}

void prepareForNextProof() {
    // Reset the global ordering so the next proof can set its own
    Ordering::unsetGlobalOrdering();
}

void reset() {
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

    s_initialized = true;
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
    return Literal::create(pred, positive, args.size(), args.data());
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
    return Clause::fromLiterals(literals.size(), literals.data(),
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

ProofResult prove(Problem* prb) {
    // Ensure problem is set
    env.setMainProblem(prb);

    // Preprocess if we have formulas (convert to clauses)
    Shell::Preprocess prepro(*env.options);
    prepro.preprocess(*prb);

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
