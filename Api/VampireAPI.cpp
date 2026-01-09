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

#include "Kernel/Term.hpp"
#include "Kernel/Clause.hpp"
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

// ===========================================
// Literal Construction
// ===========================================

Literal* eq(bool positive, TermList lhs, TermList rhs) {
    return Literal::createEquality(positive, lhs, rhs, AtomicSort::defaultSort());
}

Literal* lit(unsigned pred, bool positive, std::initializer_list<TermList> args) {
    return Literal::create(pred, positive, args);
}

Literal* neg(Literal* l) {
    return Literal::complementaryLiteral(l);
}

// ===========================================
// Clause Construction
// ===========================================

Clause* axiom(std::initializer_list<Literal*> literals) {
    return clause(literals, UnitInputType::AXIOM);
}

Clause* conjecture(std::initializer_list<Literal*> literals) {
    return clause(literals, UnitInputType::NEGATED_CONJECTURE);
}

Clause* clause(std::initializer_list<Literal*> literals, UnitInputType inputType) {
    return Clause::fromLiterals(literals,
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

} // namespace Api
