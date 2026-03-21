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
 * @file VampireAPI.hpp
 * Public API for using Vampire as a library.
 *
 * This header provides functions for:
 * - Registering function and predicate symbols
 * - Constructing terms, literals, and clauses programmatically
 * - Running the prover and retrieving results
 */

#ifndef __VampireAPI__
#define __VampireAPI__

#include <string>
#include <initializer_list>
#include <ostream>
#include <vector>

#include "Forwards.hpp"
#include "Kernel/Term.hpp"
#include "Kernel/Clause.hpp"
#include "Kernel/Formula.hpp"
#include "Kernel/FormulaUnit.hpp"
#include "Kernel/Problem.hpp"
#include "Kernel/Signature.hpp"
#include "Kernel/Inference.hpp"
#include "Shell/Options.hpp"
#include "Shell/Statistics.hpp"

namespace Api {

using namespace Kernel;
using namespace Shell;

/**
 * Result of a proving attempt.
 */
enum class ProofResult {
    PROOF,            // Proof found (conjecture is a theorem)
    SATISFIABLE,      // Counter-model exists
    TIMEOUT,          // Time limit exceeded
    MEMORY_LIMIT,     // Memory limit exceeded
    UNKNOWN,          // Could not determine
    INCOMPLETE        // Incomplete search
};

/**
 * Prepare for running another proof (light reset).
 */
void prepareForNextProof();

/**
 * Fully reset the Vampire state for a fresh start.
 */
void reset();

/**
 * Access the options object for configuration.
 */
Options& options();

/**
 * Access the signature for direct symbol manipulation.
 */
Signature& signature();

/**
 * Access statistics after proving.
 */
Statistics& statistics();

// ===========================================
// Symbol Registration
// ===========================================

unsigned addFunction(const std::string& name, unsigned arity);
unsigned addPredicate(const std::string& name, unsigned arity);
unsigned addInterpretedFunction(const std::string& name, Interpretation interp);
unsigned addInterpretedPredicate(const std::string& name, Interpretation interp);
unsigned addIntegerConstant(const std::string& value);
unsigned addRationalConstant(const std::string& value);
unsigned addRealConstant(const std::string& value);

// ===========================================
// Sort Registration (TFF)
// ===========================================

unsigned addSort(const std::string& name);
unsigned defaultSort();
unsigned sortBool();
unsigned sortInt();
unsigned sortReal();
unsigned sortRational();

// ===========================================
// Typed Symbol Registration (TFF)
// ===========================================

unsigned addTypedFunction(const std::string& name,
                          const std::vector<unsigned>& argSortIndices,
                          unsigned returnSortIndex);

unsigned addTypedPredicate(const std::string& name,
                           const std::vector<unsigned>& argSortIndices);

// ===========================================
// Typed Formula Construction (TFF)
// ===========================================

Formula* forallTyped(unsigned varIndex, unsigned sortIndex, Formula* f);
Formula* existsTyped(unsigned varIndex, unsigned sortIndex, Formula* f);
Literal* typedEq(bool positive, TermList lhs, TermList rhs, unsigned sortIndex);

// ===========================================
// Term Construction
// ===========================================

TermList var(unsigned index);
TermList constant(unsigned functor);
TermList term(unsigned functor, std::initializer_list<TermList> args);
TermList term(unsigned functor, const std::vector<TermList>& args);

// ===========================================
// Literal Construction
// ===========================================

Literal* eq(bool positive, TermList lhs, TermList rhs);
Literal* lit(unsigned pred, bool positive, std::initializer_list<TermList> args);
Literal* lit(unsigned pred, bool positive, const std::vector<TermList>& args);
Literal* neg(Literal* l);

// ===========================================
// Formula Construction (First-Order Logic)
// ===========================================

Formula* atom(Literal* l);
Formula* notF(Formula* f);
Formula* andF(std::initializer_list<Formula*> fs);
Formula* andF(const std::vector<Formula*>& fs);
Formula* orF(std::initializer_list<Formula*> fs);
Formula* orF(const std::vector<Formula*>& fs);
Formula* impF(Formula* lhs, Formula* rhs);
Formula* iffF(Formula* lhs, Formula* rhs);
Formula* forallF(unsigned varIndex, Formula* f);
Formula* existsF(unsigned varIndex, Formula* f);
Formula* trueF();
Formula* falseF();
Unit* axiomF(Formula* f);
Unit* conjectureF(Formula* f);

// ===========================================
// Clause Construction
// ===========================================

Clause* axiom(std::initializer_list<Literal*> literals);
Clause* axiom(const std::vector<Literal*>& literals);
Clause* conjecture(std::initializer_list<Literal*> literals);
Clause* conjecture(const std::vector<Literal*>& literals);
Clause* clause(std::initializer_list<Literal*> literals, UnitInputType inputType);
Clause* clause(const std::vector<Literal*>& literals, UnitInputType inputType);

// ===========================================
// Problem and Proving
// ===========================================

Problem* problem(std::initializer_list<Clause*> clauses);
Problem* problem(const std::vector<Clause*>& clauses);
Problem* problem(std::initializer_list<Unit*> units);
Problem* problem(const std::vector<Unit*>& units);

ProofResult prove(Problem* prb);
void clausify(Problem* prb);
std::vector<std::string> getCNF(Problem* prb);
void destroyUnit(Unit* u);
Unit* getRefutation();
void printProof(std::ostream& out, Unit* refutation);

// ===========================================
// Structured Proof Access
// ===========================================

struct ProofStep {
    unsigned id;
    InferenceRule rule;
    UnitInputType inputType;
    std::vector<unsigned> premiseIds;
    Unit* unit;

    Clause* clause() const;
    bool isEmpty() const;
    bool isInput() const { return premiseIds.empty(); }
    std::string ruleName() const;
    std::string inputTypeName() const;
};

std::vector<ProofStep> extractProof(Unit* refutation);
std::vector<Literal*> getLiterals(Clause* c);

std::string termToString(TermList t);
std::string literalToString(Literal* l);
std::string clauseToString(Clause* c);
std::string formulaToString(Formula* f);

} // namespace Api

#endif // __VampireAPI__
