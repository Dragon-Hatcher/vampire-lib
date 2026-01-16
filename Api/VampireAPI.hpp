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
 * Initialize the Vampire library.
 * The global environment is auto-constructed, but this ensures
 * proper initialization state.
 */
void init();

/**
 * Prepare for running another proof (light reset).
 * Call this between independent proving attempts to reset
 * the global ordering and other per-proof state.
 *
 * Note: This does NOT reset the signature - symbols accumulate
 * between proofs. Use reset() for a full reset.
 */
void prepareForNextProof();

/**
 * Fully reset the Vampire state for a fresh start.
 * This resets all static caches, clears the signature, and
 * reinitializes the environment. After calling this, the
 * state is as if Vampire was just started.
 *
 * Call this between proofs if you want to reuse symbol names
 * without conflicts, or to prevent memory growth from accumulated
 * symbols and caches.
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

/**
 * Register a function symbol with the given name and arity.
 * For constants, use arity 0.
 * @param name Symbol name
 * @param arity Number of arguments
 * @return functor index for use in term construction
 */
unsigned addFunction(const std::string& name, unsigned arity);

/**
 * Register a predicate symbol with the given name and arity.
 * @param name Symbol name
 * @param arity Number of arguments
 * @return predicate index for use in literal construction
 */
unsigned addPredicate(const std::string& name, unsigned arity);

// ===========================================
// Term Construction
// ===========================================

/**
 * Create a variable term.
 * @param index Variable index (0, 1, 2, ...)
 * @return TermList representing the variable
 */
TermList var(unsigned index);

/**
 * Create a constant term (0-arity function application).
 * @param functor Function symbol index from addFunction
 * @return TermList representing the constant
 */
TermList constant(unsigned functor);

/**
 * Create a function application term.
 * @param functor Function symbol index from addFunction
 * @param args Argument terms
 * @return TermList representing the term
 */
TermList term(unsigned functor, std::initializer_list<TermList> args);

// ===========================================
// Literal Construction
// ===========================================

/**
 * Create an equality literal (s = t or s != t).
 * @param positive true for equality, false for disequality
 * @param lhs Left-hand side term
 * @param rhs Right-hand side term
 * @return Pointer to the literal
 */
Literal* eq(bool positive, TermList lhs, TermList rhs);

/**
 * Create a predicate literal.
 * @param pred Predicate symbol index from addPredicate
 * @param positive true for positive literal, false for negated
 * @param args Argument terms
 * @return Pointer to the literal
 */
Literal* lit(unsigned pred, bool positive, std::initializer_list<TermList> args);

/**
 * Get the complementary (negated) literal.
 * @param l The literal to negate
 * @return Pointer to the complementary literal
 */
Literal* neg(Literal* l);

// ===========================================
// Formula Construction (First-Order Logic)
// ===========================================

/**
 * Create an atomic formula from a literal.
 * @param l The literal
 * @return Pointer to the formula
 */
Formula* atom(Literal* l);

/**
 * Create a negated formula (NOT f).
 * @param f The formula to negate
 * @return Pointer to the negated formula
 */
Formula* notF(Formula* f);

/**
 * Create a conjunction (f1 AND f2 AND ...).
 * @param fs The formulas to conjoin
 * @return Pointer to the conjunction
 */
Formula* andF(std::initializer_list<Formula*> fs);

/**
 * Create a disjunction (f1 OR f2 OR ...).
 * @param fs The formulas to disjoin
 * @return Pointer to the disjunction
 */
Formula* orF(std::initializer_list<Formula*> fs);

/**
 * Create an implication (f1 => f2).
 * @param lhs The antecedent
 * @param rhs The consequent
 * @return Pointer to the implication
 */
Formula* impF(Formula* lhs, Formula* rhs);

/**
 * Create an equivalence (f1 <=> f2).
 * @param lhs Left-hand side
 * @param rhs Right-hand side
 * @return Pointer to the equivalence
 */
Formula* iffF(Formula* lhs, Formula* rhs);

/**
 * Create a universally quantified formula (forall x. f).
 * @param varIndex The variable index to bind
 * @param f The body formula
 * @return Pointer to the quantified formula
 */
Formula* forallF(unsigned varIndex, Formula* f);

/**
 * Create an existentially quantified formula (exists x. f).
 * @param varIndex The variable index to bind
 * @param f The body formula
 * @return Pointer to the quantified formula
 */
Formula* existsF(unsigned varIndex, Formula* f);

/**
 * Create an axiom formula unit.
 * @param f The formula
 * @return Pointer to the formula unit (as Unit*)
 */
Unit* axiomF(Formula* f);

/**
 * Create a conjecture formula unit (to be proven).
 * The formula is automatically negated for refutation-based proving.
 * @param f The formula to prove
 * @return Pointer to the formula unit (as Unit*)
 */
Unit* conjectureF(Formula* f);

// ===========================================
// Clause Construction
// ===========================================

/**
 * Create an axiom clause (disjunction of literals).
 * @param literals The literals in the clause
 * @return Pointer to the clause
 */
Clause* axiom(std::initializer_list<Literal*> literals);

/**
 * Create a conjecture clause (to be refuted).
 * @param literals The literals in the clause
 * @return Pointer to the clause
 */
Clause* conjecture(std::initializer_list<Literal*> literals);

/**
 * Create a clause with specified input type.
 * @param literals The literals in the clause
 * @param inputType The type of input (AXIOM, CONJECTURE, etc.)
 * @return Pointer to the clause
 */
Clause* clause(std::initializer_list<Literal*> literals, UnitInputType inputType);

// ===========================================
// Problem and Proving
// ===========================================

/**
 * Create a problem from a list of clauses.
 * @param clauses The clauses comprising the problem
 * @return Pointer to the problem
 */
Problem* problem(std::initializer_list<Clause*> clauses);

/**
 * Create a problem from a list of units (clauses or formulas).
 * Formulas will be clausified during preprocessing.
 * @param units The units comprising the problem
 * @return Pointer to the problem
 */
Problem* problem(std::initializer_list<Unit*> units);

/**
 * Run the prover on a problem.
 * Results are stored in statistics().
 * @param prb The problem to solve
 * @return The proof result
 */
ProofResult prove(Problem* prb);

/**
 * Get the refutation (proof) after a successful prove().
 * @return The empty clause with inference chain, or nullptr if no proof
 */
Unit* getRefutation();

/**
 * Print the proof to a stream.
 * @param out Output stream
 * @param refutation The refutation from getRefutation()
 */
void printProof(std::ostream& out, Unit* refutation);

// ===========================================
// Structured Proof Access
// ===========================================

/**
 * A single step in a proof.
 */
struct ProofStep {
    unsigned id;                        // Unique identifier for this unit
    InferenceRule rule;                 // Inference rule (enum)
    UnitInputType inputType;            // Input type (enum)
    std::vector<unsigned> premiseIds;   // IDs of premise units
    Unit* unit;                         // The underlying Unit (Clause or Formula)

    // Access the clause if this step is a clause (most steps are)
    Clause* clause() const;

    // Check if this is the empty clause (refutation)
    bool isEmpty() const;

    // Check if this is an input clause (no premises)
    bool isInput() const { return premiseIds.empty(); }

    // Get string representation of the rule
    std::string ruleName() const;

    // Get string representation of the input type
    std::string inputTypeName() const;
};

/**
 * Extract the proof as a sequence of steps.
 * Steps are returned in topological order (premises before conclusions).
 * The last step is the empty clause (refutation).
 *
 * @param refutation The refutation from getRefutation()
 * @return Vector of proof steps, or empty if refutation is null
 */
std::vector<ProofStep> extractProof(Unit* refutation);

/**
 * Get the literals of a clause as a vector.
 * @param c The clause
 * @return Vector of literals in the clause
 */
std::vector<Literal*> getLiterals(Clause* c);

/**
 * Convert a term to a string representation.
 */
std::string termToString(TermList t);

/**
 * Convert a literal to a string representation.
 */
std::string literalToString(Literal* l);

/**
 * Convert a clause to a string representation.
 */
std::string clauseToString(Clause* c);

} // namespace Api

#endif // __VampireAPI__
