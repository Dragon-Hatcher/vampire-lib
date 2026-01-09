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

#include "Forwards.hpp"
#include "Kernel/Term.hpp"
#include "Kernel/Clause.hpp"
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
 * Prepare for running another proof.
 * Call this between independent proving attempts to reset
 * the global ordering and other per-proof state.
 *
 * Note: This does NOT reset the signature - symbols accumulate
 * between proofs. This is harmless as long as symbol names don't
 * conflict. A full reset is not possible due to static caches in
 * the AtomicSort implementation.
 */
void prepareForNextProof();

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

} // namespace Api

#endif // __VampireAPI__
