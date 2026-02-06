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
 * @file vampire_c_api.h
 * Plain C API for using Vampire as a library via FFI.
 *
 * This header provides a C interface suitable for Foreign Function Interface (FFI)
 * bindings from languages like Python, Rust, Go, etc.
 *
 * All Vampire internal types are represented as opaque pointers.
 * Memory is managed by Vampire - do not free returned pointers.
 */

#ifndef __VAMPIRE_C_API__
#define __VAMPIRE_C_API__

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdbool.h>

/* ===========================================
 * Opaque Types
 * =========================================== */

/** Opaque handle to a term (TermList) */
typedef struct vampire_term_t vampire_term_t;

/** Opaque handle to a literal */
typedef struct vampire_literal_t vampire_literal_t;

/** Opaque handle to a formula */
typedef struct vampire_formula_t vampire_formula_t;

/** Opaque handle to a unit (clause or formula unit) */
typedef struct vampire_unit_t vampire_unit_t;

/** Opaque handle to a clause */
typedef struct vampire_clause_t vampire_clause_t;

/** Opaque handle to a problem */
typedef struct vampire_problem_t vampire_problem_t;

/* ===========================================
 * Enumerations
 * =========================================== */

/** Result of a proving attempt */
typedef enum {
    VAMPIRE_PROOF = 0,           /* Proof found (conjecture is a theorem) */
    VAMPIRE_SATISFIABLE = 1,     /* Counter-model exists */
    VAMPIRE_TIMEOUT = 2,         /* Time limit exceeded */
    VAMPIRE_MEMORY_LIMIT = 3,    /* Memory limit exceeded */
    VAMPIRE_UNKNOWN = 4,         /* Could not determine */
    VAMPIRE_INCOMPLETE = 5       /* Incomplete search */
} vampire_proof_result_t;

/** Input type for units */
typedef enum {
    VAMPIRE_AXIOM = 0,
    VAMPIRE_NEGATED_CONJECTURE = 1,
    VAMPIRE_CONJECTURE = 2
} vampire_input_type_t;

/** Inference rules (subset of commonly used rules) */
typedef enum {
    VAMPIRE_RULE_INPUT = 0,
    VAMPIRE_RULE_RESOLUTION = 1,
    VAMPIRE_RULE_FACTORING = 2,
    VAMPIRE_RULE_SUPERPOSITION = 3,
    VAMPIRE_RULE_EQUALITY_RESOLUTION = 4,
    VAMPIRE_RULE_EQUALITY_FACTORING = 5,
    VAMPIRE_RULE_CLAUSIFY = 6,
    VAMPIRE_RULE_OTHER = 99
} vampire_inference_rule_t;

/* ===========================================
 * Structures
 * =========================================== */

/** A single step in a proof */
typedef struct {
    unsigned int id;                    /* Unique identifier for this unit */
    vampire_inference_rule_t rule;      /* Inference rule */
    vampire_input_type_t input_type;    /* Input type */
    unsigned int* premise_ids;          /* Array of premise unit IDs */
    size_t premise_count;               /* Number of premises */
    vampire_unit_t* unit;               /* The underlying unit */
} vampire_proof_step_t;

/* ===========================================
 * Library Initialization and Reset
 * =========================================== */

/**
 * Prepare for running another proof (light reset).
 * Call this between independent proving attempts to reset
 * the global ordering and other per-proof state.
 *
 * Note: This does NOT reset the signature - symbols accumulate
 * between proofs. Use vampire_reset() for a full reset.
 */
void vampire_prepare_for_next_proof(void);

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
void vampire_reset(void);

/* ===========================================
 * Options Configuration
 * =========================================== */

/**
 * Set a time limit in seconds (0 = no limit).
 */
void vampire_set_time_limit(int seconds);

/**
 * Set a time limit in deciseconds (10 = 1 second, 0 = no limit).
 */
void vampire_set_time_limit_deciseconds(int deciseconds);

/**
 * Enable or disable proof output.
 */
void vampire_set_show_proof(bool show);

/**
 * Set saturation algorithm.
 * @param algorithm Name of algorithm (e.g., "lrs", "discount", "otter")
 */
void vampire_set_saturation_algorithm(const char* algorithm);

/* ===========================================
 * Symbol Registration
 * =========================================== */

/**
 * Register a function symbol with the given name and arity.
 * For constants, use arity 0.
 * @param name Symbol name (null-terminated string)
 * @param arity Number of arguments
 * @return functor index for use in term construction
 */
unsigned int vampire_add_function(const char* name, unsigned int arity);

/**
 * Register a predicate symbol with the given name and arity.
 * @param name Symbol name (null-terminated string)
 * @param arity Number of arguments
 * @return predicate index for use in literal construction
 */
unsigned int vampire_add_predicate(const char* name, unsigned int arity);

/* ===========================================
 * Term Construction
 * =========================================== */

/**
 * Create a variable term.
 * @param index Variable index (0, 1, 2, ...)
 * @return Term handle
 */
vampire_term_t* vampire_var(unsigned int index);

/**
 * Create a constant term (0-arity function application).
 * @param functor Function symbol index from vampire_add_function
 * @return Term handle
 */
vampire_term_t* vampire_constant(unsigned int functor);

/**
 * Create a function application term.
 * @param functor Function symbol index from vampire_add_function
 * @param args Array of argument terms
 * @param arg_count Number of arguments
 * @return Term handle
 */
vampire_term_t* vampire_term(unsigned int functor, vampire_term_t** args, size_t arg_count);

/* ===========================================
 * Literal Construction
 * =========================================== */

/**
 * Create an equality literal (s = t or s != t).
 * @param positive true for equality, false for disequality
 * @param lhs Left-hand side term
 * @param rhs Right-hand side term
 * @return Literal handle
 */
vampire_literal_t* vampire_eq(bool positive, vampire_term_t* lhs, vampire_term_t* rhs);

/**
 * Create a predicate literal.
 * @param pred Predicate symbol index from vampire_add_predicate
 * @param positive true for positive literal, false for negated
 * @param args Array of argument terms
 * @param arg_count Number of arguments
 * @return Literal handle
 */
vampire_literal_t* vampire_lit(unsigned int pred, bool positive,
                                vampire_term_t** args, size_t arg_count);

/**
 * Get the complementary (negated) literal.
 * @param l The literal to negate
 * @return Literal handle
 */
vampire_literal_t* vampire_neg(vampire_literal_t* l);

/* ===========================================
 * Formula Construction
 * =========================================== */

/**
 * Create an atomic formula from a literal.
 * @param l The literal
 * @return Formula handle
 */
vampire_formula_t* vampire_atom(vampire_literal_t* l);

/**
 * Create a negated formula (NOT f).
 * @param f The formula to negate
 * @return Formula handle
 */
vampire_formula_t* vampire_not(vampire_formula_t* f);

/**
 * Create a conjunction (f1 AND f2 AND ...).
 * @param formulas Array of formulas
 * @param count Number of formulas
 * @return Formula handle
 */
vampire_formula_t* vampire_and(vampire_formula_t** formulas, size_t count);

/**
 * Create a disjunction (f1 OR f2 OR ...).
 * @param formulas Array of formulas
 * @param count Number of formulas
 * @return Formula handle
 */
vampire_formula_t* vampire_or(vampire_formula_t** formulas, size_t count);

/**
 * Create an implication (f1 => f2).
 * @param lhs The antecedent
 * @param rhs The consequent
 * @return Formula handle
 */
vampire_formula_t* vampire_imp(vampire_formula_t* lhs, vampire_formula_t* rhs);

/**
 * Create an equivalence (f1 <=> f2).
 * @param lhs Left-hand side
 * @param rhs Right-hand side
 * @return Formula handle
 */
vampire_formula_t* vampire_iff(vampire_formula_t* lhs, vampire_formula_t* rhs);

/**
 * Create a universally quantified formula (forall x. f).
 * @param var_index The variable index to bind
 * @param f The body formula
 * @return Formula handle
 */
vampire_formula_t* vampire_forall(unsigned int var_index, vampire_formula_t* f);

/**
 * Create an existentially quantified formula (exists x. f).
 * @param var_index The variable index to bind
 * @param f The body formula
 * @return Formula handle
 */
vampire_formula_t* vampire_exists(unsigned int var_index, vampire_formula_t* f);

/**
 * Create an axiom formula unit.
 * @param f The formula
 * @return Unit handle
 */
vampire_unit_t* vampire_axiom_formula(vampire_formula_t* f);

/**
 * Create a conjecture formula unit (to be proven).
 * The formula is automatically negated for refutation-based proving.
 * @param f The formula to prove
 * @return Unit handle
 */
vampire_unit_t* vampire_conjecture_formula(vampire_formula_t* f);

/* ===========================================
 * Clause Construction
 * =========================================== */

/**
 * Create an axiom clause (disjunction of literals).
 * @param literals Array of literals
 * @param count Number of literals
 * @return Clause handle
 */
vampire_clause_t* vampire_axiom_clause(vampire_literal_t** literals, size_t count);

/**
 * Create a conjecture clause (to be refuted).
 * @param literals Array of literals
 * @param count Number of literals
 * @return Clause handle
 */
vampire_clause_t* vampire_conjecture_clause(vampire_literal_t** literals, size_t count);

/**
 * Create a clause with specified input type.
 * @param literals Array of literals
 * @param count Number of literals
 * @param input_type The type of input
 * @return Clause handle
 */
vampire_clause_t* vampire_clause(vampire_literal_t** literals, size_t count,
                                  vampire_input_type_t input_type);

/* ===========================================
 * Problem and Proving
 * =========================================== */

/**
 * Create a problem from an array of clauses.
 * @param clauses Array of clause handles
 * @param count Number of clauses
 * @return Problem handle
 */
vampire_problem_t* vampire_problem_from_clauses(vampire_clause_t** clauses, size_t count);

/**
 * Create a problem from an array of units (clauses or formulas).
 * Formulas will be clausified during preprocessing.
 * @param units Array of unit handles
 * @param count Number of units
 * @return Problem handle
 */
vampire_problem_t* vampire_problem_from_units(vampire_unit_t** units, size_t count);

/**
 * Run the prover on a problem.
 * @param problem The problem to solve
 * @return The proof result
 */
vampire_proof_result_t vampire_prove(vampire_problem_t* problem);

/**
 * Get the refutation (proof) after a successful vampire_prove() call.
 * @return The empty clause with inference chain, or NULL if no proof
 */
vampire_unit_t* vampire_get_refutation(void);

/**
 * Print the proof to stdout.
 * @param refutation The refutation from vampire_get_refutation()
 */
void vampire_print_proof(vampire_unit_t* refutation);

/**
 * Print the proof to a file.
 * @param filename Path to output file (null-terminated string)
 * @param refutation The refutation from vampire_get_refutation()
 * @return 0 on success, -1 on error
 */
int vampire_print_proof_to_file(const char* filename, vampire_unit_t* refutation);

/* ===========================================
 * Structured Proof Access
 * =========================================== */

/**
 * Extract the proof as a sequence of steps.
 * Steps are returned in topological order (premises before conclusions).
 * The last step is the empty clause (refutation).
 *
 * @param refutation The refutation from vampire_get_refutation()
 * @param out_steps Pointer to receive the array of proof steps
 * @param out_count Pointer to receive the number of steps
 * @return 0 on success, -1 on error
 *
 * Note: The caller must free the returned array and each step's premise_ids array.
 */
int vampire_extract_proof(vampire_unit_t* refutation,
                          vampire_proof_step_t** out_steps,
                          size_t* out_count);

/**
 * Free the proof steps array returned by vampire_extract_proof().
 * @param steps The array to free
 * @param count Number of steps
 */
void vampire_free_proof_steps(vampire_proof_step_t* steps, size_t count);

/**
 * Get the literals of a clause as an array.
 * @param clause The clause
 * @param out_literals Pointer to receive the array of literals
 * @param out_count Pointer to receive the number of literals
 * @return 0 on success, -1 on error
 *
 * Note: The caller must free the returned array.
 */
int vampire_get_literals(vampire_clause_t* clause,
                         vampire_literal_t*** out_literals,
                         size_t* out_count);

/**
 * Free the literals array returned by vampire_get_literals().
 * @param literals The array to free
 */
void vampire_free_literals(vampire_literal_t** literals);

/**
 * Get the clause from a unit (if the unit is a clause).
 * @param unit The unit
 * @return Clause handle, or NULL if unit is not a clause
 */
vampire_clause_t* vampire_unit_as_clause(vampire_unit_t* unit);

/**
 * Check if a clause is empty (represents false).
 * @param clause The clause
 * @return true if empty, false otherwise
 */
bool vampire_clause_is_empty(vampire_clause_t* clause);

/* ===========================================
 * String Conversions
 * =========================================== */

/**
 * Convert a term to a string representation.
 * @param term The term
 * @return Allocated string (must be freed with vampire_free_string), or NULL on error
 */
char* vampire_term_to_string(vampire_term_t* term);

/**
 * Convert a literal to a string representation.
 * @param literal The literal
 * @return Allocated string (must be freed with vampire_free_string), or NULL on error
 */
char* vampire_literal_to_string(vampire_literal_t* literal);

/**
 * Convert a clause to a string representation.
 * @param clause The clause
 * @return Allocated string (must be freed with vampire_free_string), or NULL on error
 */
char* vampire_clause_to_string(vampire_clause_t* clause);

/**
 * Convert a formula to a string representation.
 * @param formula The formula
 * @return Allocated string (must be freed with vampire_free_string), or NULL on error
 */
char* vampire_formula_to_string(vampire_formula_t* formula);

/**
 * Free a string allocated by vampire_*_to_string functions.
 * @param str The string to free
 */
void vampire_free_string(char* str);

/**
 * Get the name of an inference rule.
 * @param rule The inference rule
 * @return String name (static, do not free)
 */
const char* vampire_rule_name(vampire_inference_rule_t rule);

/**
 * Get the name of an input type.
 * @param input_type The input type
 * @return String name (static, do not free)
 */
const char* vampire_input_type_name(vampire_input_type_t input_type);

#ifdef __cplusplus
}
#endif

#endif /* __VAMPIRE_C_API__ */
