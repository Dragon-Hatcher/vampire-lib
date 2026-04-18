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
#include <stdint.h>

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

/** Interpreted theory symbols (subset) */
typedef enum {
    VAMPIRE_INTERP_EQUAL,

    VAMPIRE_INTERP_INT_GREATER,
    VAMPIRE_INTERP_INT_GREATER_EQUAL,
    VAMPIRE_INTERP_INT_LESS,
    VAMPIRE_INTERP_INT_LESS_EQUAL,
    VAMPIRE_INTERP_INT_DIVIDES,

    VAMPIRE_INTERP_INT_SUCCESSOR,
    VAMPIRE_INTERP_INT_UNARY_MINUS,
    VAMPIRE_INTERP_INT_PLUS,
    VAMPIRE_INTERP_INT_MINUS,
    VAMPIRE_INTERP_INT_MULTIPLY,
    VAMPIRE_INTERP_INT_QUOTIENT_E,
    VAMPIRE_INTERP_INT_QUOTIENT_T,
    VAMPIRE_INTERP_INT_QUOTIENT_F,
    VAMPIRE_INTERP_INT_REMAINDER_E,
    VAMPIRE_INTERP_INT_REMAINDER_T,
    VAMPIRE_INTERP_INT_REMAINDER_F,
    VAMPIRE_INTERP_INT_ABS,

    VAMPIRE_INTERP_RAT_GREATER,
    VAMPIRE_INTERP_RAT_GREATER_EQUAL,
    VAMPIRE_INTERP_RAT_LESS,
    VAMPIRE_INTERP_RAT_LESS_EQUAL,
    VAMPIRE_INTERP_RAT_PLUS,
    VAMPIRE_INTERP_RAT_MINUS,
    VAMPIRE_INTERP_RAT_MULTIPLY,
    VAMPIRE_INTERP_RAT_QUOTIENT,

    VAMPIRE_INTERP_REAL_GREATER,
    VAMPIRE_INTERP_REAL_GREATER_EQUAL,
    VAMPIRE_INTERP_REAL_LESS,
    VAMPIRE_INTERP_REAL_LESS_EQUAL,
    VAMPIRE_INTERP_REAL_PLUS,
    VAMPIRE_INTERP_REAL_MINUS,
    VAMPIRE_INTERP_REAL_MULTIPLY,
    VAMPIRE_INTERP_REAL_QUOTIENT
} vampire_interpretation_t;

/** Input type for units */
typedef enum {
    VAMPIRE_AXIOM = 0,
    VAMPIRE_NEGATED_CONJECTURE = 1,
    VAMPIRE_CONJECTURE = 2
} vampire_input_type_t;

/** Inference rules (subset of commonly used rules) */
typedef enum {
    INPUT,
    GENERIC_FORMULA_CLAUSE_TRANSFORMATION,
    NEGATED_CONJECTURE,
    ANSWER_LITERAL_INJECTION,
    ANSWER_LITERAL_INPUT_SKOLEMISATION,
    CLAIM_DEFINITION,
    RECTIFY,
    CLOSURE,
    FLATTEN,
    ENNF,
    NNF,
    REDUCE_FALSE_TRUE,
    DEFINITION_FOLDING,
    THEORY_NORMALIZATION,
    ALASCA_INTEGER_TRANSFORMATION,
    SKOLEMIZE,
    SKOLEM_SYMBOL_INTRODUCTION,
    CLAUSIFY,
    REORIENT_EQUATIONS,
    GENERIC_FORMULA_CLAUSE_TRANSFORMATION_LAST,
    GENERIC_SIMPLIFYING_INFERENCE,
    REORDER_LITERALS,
    REMOVE_DUPLICATE_LITERALS,
    TRIVIAL_INEQUALITY_REMOVAL,
    EQUALITY_RESOLUTION_WITH_DELETION,
    FORWARD_SUBSUMPTION_RESOLUTION,
    BACKWARD_SUBSUMPTION_RESOLUTION,
    FORWARD_DEMODULATION,
    BACKWARD_DEMODULATION,
    ALASCA_FWD_DEMODULATION,
    ALASCA_BWD_DEMODULATION,
    FORWARD_SUBSUMPTION_DEMODULATION,
    BACKWARD_SUBSUMPTION_DEMODULATION,
    FORWARD_LITERAL_REWRITING,
    INNER_REWRITING,
    CONDENSATION,
    EVALUATION,
    ALASCA_NORMALIZATION,
    ALASCA_ABSTRACTION,
    ALASCA_FLOOR_ELIMINATION,
    CANCELLATION,
    INTERPRETED_SIMPLIFICATION,
    THEORY_FLATTENING,
    TERM_ALGEBRA_DISTINCTNESS,
    TERM_ALGEBRA_POSITIVE_INJECTIVITY_SIMPLIFYING,
    TERM_ALGEBRA_NEGATIVE_INJECTIVITY_SIMPLIFYING,
    GLOBAL_SUBSUMPTION,
    DISTINCT_EQUALITY_REMOVAL,
    GAUSSIAN_VARIABLE_ELIMINIATION,
    ARITHMETIC_SUBTERM_GENERALIZATION,
    ANSWER_LITERAL_REMOVAL,
    ANSWER_LITERAL_JOIN_WITH_CONSTRAINTS,
    ANSWER_LITERAL_JOIN_AS_ITE,
    AVATAR_ASSERTION_REINTRODUCTION,
    CASES_SIMP,
    ALASCA_VIRAS_QE,
    BOOL_SIMP,
    FUNCTION_DEFINITION_DEMODULATION,
    GENERIC_SIMPLIFYING_INFERENCE_LAST,
    GENERIC_GENERATING_INFERENCE,
    RESOLUTION,
    CONSTRAINED_RESOLUTION,
    FACTORING,
    CONSTRAINED_FACTORING,
    SUPERPOSITION,
    FUNCTION_DEFINITION_REWRITING,
    CONSTRAINED_SUPERPOSITION,
    EQUALITY_FACTORING,
    EQUALITY_RESOLUTION,
    EXTENSIONALITY_RESOLUTION,
    TERM_ALGEBRA_INJECTIVITY_GENERATING,
    TERM_ALGEBRA_ACYCLICITY,
    FOOL_PARAMODULATION,
    UNIT_RESULTING_RESOLUTION,
    INDUCTION_HYPERRESOLUTION,
    INSTANTIATION,
    ALASCA_FOURIER_MOTZKIN,
    ALASCA_INTEGER_FOURIER_MOTZKIN,
    ALASCA_TERM_FACTORING,
    ALASCA_FLOOR_BOUNDS,
    ALASCA_EQ_FACTORING,
    ALASCA_LITERAL_FACTORING,
    ALASCA_SUPERPOSITION,
    ALASCA_COHERENCE,
    ALASCA_COHERENCE_NORMALIZATION,
    ALASCA_VARIABLE_ELIMINATION,
    ARG_CONG,
    INJECTIVITY,
    PRIMITIVE_INSTANTIATION,
    LEIBNIZ_ELIMINATION,
    HILBERTS_CHOICE_INSTANCE,
    NEGATIVE_EXT,
    EQ_TO_DISEQ,
    HOL_NOT_ELIMINATION,
    BINARY_CONN_ELIMINATION,
    VSIGMA_ELIMINATION,
    VPI_ELIMINATION,
    HOL_EQUALITY_ELIMINATION,
    GENERIC_GENERATING_INFERENCE_LAST,
    EQUALITY_PROXY_REPLACEMENT,
    EQUALITY_PROXY_AXIOM1,
    EQUALITY_PROXY_AXIOM2,
    DEFINITION_UNFOLDING,
    FUNCTION_DEFINITION,
    PREDICATE_DEFINITION,
    PREDICATE_DEFINITION_UNFOLDING,
    PREDICATE_DEFINITION_MERGING,
    POLARITY_FLIPPING,
    UNUSED_PREDICATE_DEFINITION_REMOVAL,
    PURE_PREDICATE_REMOVAL,
    INEQUALITY_SPLITTING,
    INEQUALITY_SPLITTING_NAME_INTRODUCTION,
    DISTINCTNESS_AXIOM,
    BOOLEAN_TERM_ENCODING,
    FOOL_ELIMINATION,
    FOOL_ITE_DEFINITION,
    FOOL_LET_DEFINITION,
    FOOL_FORMULA_DEFINITION,
    FOOL_MATCH_DEFINITION,
    GENERAL_SPLITTING,
    GENERAL_SPLITTING_COMPONENT,
    COLOR_UNBLOCKING,
    SAT_COLOR_ELIMINATION,
    FORMULIFY,
    FMB_FLATTENING,
    FMB_FUNC_DEF,
    FMB_DEF_INTRO,
    MODEL_NOT_FOUND,
    ADD_SORT_PREDICATES,
    ADD_SORT_FUNCTIONS,
    ANSWER_LITERAL_RESOLVER,
    THEORY_TAUTOLOGY_SAT_CONFLICT,
    GENERIC_AVATAR_INFERENCE,
    AVATAR_DEFINITION,
    AVATAR_COMPONENT,
    AVATAR_REFUTATION,
    AVATAR_REFUTATION_SMT,
    AVATAR_SPLIT_CLAUSE,
    AVATAR_CONTRADICTION_CLAUSE,
    GENERIC_AVATAR_INFERENCE_LAST,
    GENERIC_THEORY_AXIOM,
    THA_COMMUTATIVITY,
    THA_ASSOCIATIVITY,
    THA_RIGHT_IDENTITY,
    THA_LEFT_IDENTITY,
    THA_INVERSE_OP_OP_INVERSES,
    THA_INVERSE_OP_UNIT,
    THA_INVERSE_ASSOC,
    THA_NONREFLEX,
    THA_TRANSITIVITY,
    THA_ORDER_TOTALITY,
    THA_ORDER_MONOTONICITY,
    THA_ALASCA,
    THA_PLUS_ONE_GREATER,
    THA_ORDER_PLUS_ONE_DICHOTOMY,
    THA_MINUS_MINUS_X,
    THA_TIMES_ZERO,
    THA_DISTRIBUTIVITY,
    THA_DIVISIBILITY,
    THA_MODULO_MULTIPLY,
    THA_MODULO_POSITIVE,
    THA_MODULO_SMALL,
    THA_DIVIDES_MULTIPLY,
    THA_NONDIVIDES_SKOLEM,
    THA_ABS_EQUALS,
    THA_ABS_MINUS_EQUALS,
    THA_QUOTIENT_NON_ZERO,
    THA_QUOTIENT_MULTIPLY,
    THA_EXTRA_INTEGER_ORDERING,
    THA_FLOOR_SMALL,
    THA_FLOOR_BIG,
    THA_CEILING_BIG,
    THA_CEILING_SMALL,
    THA_TRUNC1,
    THA_TRUNC2,
    THA_TRUNC3,
    THA_TRUNC4,
    THA_ARRAY_EXTENSIONALITY,
    THA_BOOLEAN_ARRAY_EXTENSIONALITY, 
    THA_BOOLEAN_ARRAY_WRITE1,
    THA_BOOLEAN_ARRAY_WRITE2,
    THA_ARRAY_WRITE1,
    THA_ARRAY_WRITE2,
    TERM_ALGEBRA_ACYCLICITY_AXIOM,
    TERM_ALGEBRA_DIRECT_SUBTERMS_AXIOM,
    TERM_ALGEBRA_SUBTERMS_TRANSITIVE_AXIOM,
    TERM_ALGEBRA_DISCRIMINATION_AXIOM,
    TERM_ALGEBRA_DISTINCTNESS_AXIOM,
    TERM_ALGEBRA_EXHAUSTIVENESS_AXIOM,
    TERM_ALGEBRA_INJECTIVITY_AXIOM,
    FOOL_AXIOM_TRUE_NEQ_FALSE,
    FOOL_AXIOM_ALL_IS_TRUE_OR_FALSE,
    STRUCT_INDUCTION_AXIOM_ONE,
    STRUCT_INDUCTION_AXIOM_TWO,
    STRUCT_INDUCTION_AXIOM_THREE,
    STRUCT_INDUCTION_AXIOM_RECURSION,
    INT_INF_UP_INDUCTION_AXIOM,
    INT_INF_DOWN_INDUCTION_AXIOM,
    INT_FIN_UP_INDUCTION_AXIOM,
    INT_FIN_DOWN_INDUCTION_AXIOM,
    INT_DB_UP_INDUCTION_AXIOM,
    INT_DB_DOWN_INDUCTION_AXIOM,
}   vampire_inference_rule_t;
  
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
 * Set a time limit in milliseconds (1000 = 1 second, 0 = no limit).
 */
void vampire_set_time_limit_milliseconds(int milliseconds);

/**
 * Enable or disable proof output.
 */
void vampire_set_show_proof(bool show);

/**
 * Set saturation algorithm.
 * @param algorithm Name of algorithm (e.g., "lrs", "discount", "otter")
 */
void vampire_set_saturation_algorithm(const char* algorithm);

/**
 * Set a generic Vampire option by name and value string.
 * This maps directly to env.options->set(name, value).
 * Example: vampire_set_option("mode", "casc") enables CASC portfolio mode.
 * Returns 0 on success, -1 if the option name or value is invalid.
 */
int vampire_set_option(const char* name, const char* value);

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

/**
 * Register an interpreted function symbol.
 * @param name Symbol name (e.g. "$sum", "$product")
 * @param interp The theory interpretation
 * @return functor index
 */
unsigned int vampire_add_interpreted_function(const char* name, vampire_interpretation_t interp);

/**
 * Register an interpreted predicate symbol.
 * @param name Symbol name (e.g. "$less", "$greater")
 * @param interp The theory interpretation
 * @return predicate index
 */
unsigned int vampire_add_interpreted_predicate(const char* name, vampire_interpretation_t interp);

/**
 * Register an interpreted integer constant.
 * @param value The integer value (as a string to support large numbers)
 * @return functor index
 */
unsigned int vampire_add_integer_constant(const char* value);

/**
 * Register an interpreted rational constant.
 * @param value The rational value (as a string, e.g. "1/2")
 * @return functor index
 */
unsigned int vampire_add_rational_constant(const char* value);

/**
 * Register an interpreted real constant.
 * @param value The real value (as a string, e.g. "3.14")
 * @return functor index
 */
unsigned int vampire_add_real_constant(const char* value);

/* ===========================================
 * Sort Registration (TFF)
 * =========================================== */

/**
 * Register a user-defined sort by name.
 * Idempotent: calling with the same name returns the same sort index.
 * @param name Sort name (null-terminated string, e.g. "person")
 * @return sort index for use in typed symbol registration and quantifiers
 */
unsigned int vampire_add_sort(const char* name);

/** Return the sort index for the default individual sort ($i). */
unsigned int vampire_sort_default(void);

/** Return the sort index for the integer sort ($int). */
unsigned int vampire_sort_int(void);

/** Return the sort index for the real sort ($real). */
unsigned int vampire_sort_real(void);

/** Return the sort index for the rational sort ($rat). */
unsigned int vampire_sort_rational(void);

/* ===========================================
 * Typed Symbol Registration (TFF)
 * =========================================== */

/**
 * Register a typed function symbol.
 * Only sets the sort signature when the symbol is newly created.
 * @param name Symbol name (null-terminated string)
 * @param arg_sort_indices Array of sort indices for each argument
 * @param arity Number of arguments (length of arg_sort_indices)
 * @param return_sort_index Sort index for the return type
 * @return functor index for use in term construction
 */
unsigned int vampire_add_typed_function(const char* name,
                                        unsigned int* arg_sort_indices,
                                        size_t arity,
                                        unsigned int return_sort_index);

/**
 * Register a typed predicate symbol.
 * Only sets the sort signature when the symbol is newly created.
 * @param name Symbol name (null-terminated string)
 * @param arg_sort_indices Array of sort indices for each argument
 * @param arity Number of arguments (length of arg_sort_indices)
 * @return predicate index for use in literal construction
 */
unsigned int vampire_add_typed_predicate(const char* name,
                                         unsigned int* arg_sort_indices,
                                         size_t arity);

/* ===========================================
 * Typed Formula Construction (TFF)
 * =========================================== */

/**
 * Create a universally quantified formula with a sort annotation on the bound variable.
 * @param var_index The variable index to bind
 * @param sort_index The sort index for the bound variable
 * @param f The body formula
 * @return Formula handle
 */
vampire_formula_t* vampire_forall_typed(unsigned int var_index,
                                        unsigned int sort_index,
                                        vampire_formula_t* f);

/**
 * Create an existentially quantified formula with a sort annotation on the bound variable.
 * @param var_index The variable index to bind
 * @param sort_index The sort index for the bound variable
 * @param f The body formula
 * @return Formula handle
 */
vampire_formula_t* vampire_exists_typed(unsigned int var_index,
                                        unsigned int sort_index,
                                        vampire_formula_t* f);

/**
 * Create a typed equality literal (s = t or s != t) with an explicit sort.
 * @param positive true for equality, false for disequality
 * @param lhs Left-hand side term
 * @param rhs Right-hand side term
 * @param sort_index The sort index of the terms
 * @return Literal handle
 */
vampire_literal_t* vampire_typed_eq(bool positive,
                                    vampire_term_t* lhs,
                                    vampire_term_t* rhs,
                                    unsigned int sort_index);

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
 * Create a true (tautology) formula.
 */
vampire_formula_t* vampire_true(void);

/**
 * Create a false (contradiction) formula.
 */
vampire_formula_t* vampire_false(void);

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

/**
 * Release a unit handle (axiom or conjecture formula).
 * @param unit The unit to free
 */
void vampire_free_unit(vampire_unit_t* unit);

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
 * Preprocess a problem, converting all formulas to CNF clauses.
 * @param problem The problem to clausify
 * @return Number of clauses after clausification
 */
size_t vampire_clausify(vampire_problem_t* problem);

/**
 * Get the CNF representation of the problem as strings.
 * @param problem The clausified problem
 * @param out_clauses Output array of strings (caller must free with vampire_free_string_array)
 * @param out_count Output number of clauses
 * @return 0 on success, negative on error
 */
int vampire_get_cnf(vampire_problem_t* problem, char*** out_clauses, size_t* out_count);

/**
 * Free an array of strings returned by vampire_get_cnf.
 * @param strings The array
 * @param count The number of strings
 */
void vampire_free_string_array(char** strings, size_t count);

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
 * Get the formula from a unit (if the unit is a formula).
 * @param unit The unit
 * @return Formula handle, or NULL if unit is not a formula
 */
vampire_formula_t* vampire_unit_as_formula(vampire_unit_t* unit);

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

/* ===========================================
 * Structural Equality and Hashing
 * =========================================== */

/**
 * Structural equality for terms.
 * Returns true if the two terms have identical structure (same functor/variable and same arguments).
 * Terms are hash-consed internally so this is an O(1) comparison.
 */
bool vampire_term_equal(vampire_term_t* a, vampire_term_t* b);

/**
 * Structural hash for terms.
 * Consistent with vampire_term_equal: equal terms have the same hash.
 */
uint64_t vampire_term_hash(vampire_term_t* a);

/**
 * Structural equality for formulas.
 * Returns true if the two formulas have identical structure.
 */
bool vampire_formula_equal(vampire_formula_t* a, vampire_formula_t* b);

/**
 * Structural hash for formulas.
 * Consistent with vampire_formula_equal: equal formulas have the same hash.
 */
uint64_t vampire_formula_hash(vampire_formula_t* a);

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

/* ===========================================
 * Structured Literal / Term Readers
 *
 * These complement vampire_get_literals() / vampire_clause_*() by giving
 * the caller access to the internal structure of a Literal or Term
 * without going through stringification.  Used by consumers that need
 * to reason about clause structure (e.g. canonical-hash-based clause
 * deduplication).  All pointers returned here are borrowed views into
 * Vampire's internal hash-consed term storage unless otherwise noted.
 * =========================================== */

/** Return the predicate functor index of `lit`. */
unsigned int vampire_literal_predicate(vampire_literal_t* lit);

/** Return true if `lit` is a positive literal. */
bool vampire_literal_is_positive(vampire_literal_t* lit);

/** Return true if `lit` is an equality literal (functor 0). */
bool vampire_literal_is_equality(vampire_literal_t* lit);

/** Return the arity of `lit` (argument count). */
size_t vampire_literal_arity(vampire_literal_t* lit);

/**
 * Return the i-th argument of `lit` as a freshly heap-allocated
 * vampire_term_t (wrapping a TermList value copy).  The caller owns
 * the wrapper and does not need to free it today (matches the
 * convention of the other vampire_term_t producers).
 */
vampire_term_t* vampire_literal_arg(vampire_literal_t* lit, size_t i);

/** Return true if `term` is a variable (ordinary or special). */
bool vampire_term_is_var(vampire_term_t* term);

/** Return the variable index of `term`; undefined if not a variable. */
unsigned int vampire_term_var_index(vampire_term_t* term);

/**
 * Return the function functor index of `term`; undefined if `term` is
 * a variable.  Callers should check vampire_term_is_var() first.
 */
unsigned int vampire_term_functor(vampire_term_t* term);

/** Return the arity (argument count) of `term`; 0 for variables and constants. */
size_t vampire_term_arity(vampire_term_t* term);

/**
 * Return the i-th argument of `term` as a freshly heap-allocated
 * vampire_term_t wrapper.  Undefined if `term` is a variable.
 */
vampire_term_t* vampire_term_arg(vampire_term_t* term, size_t i);

/**
 * Return a pointer to the (borrowed, static-lifetime) name of the
 * function symbol with index `functor_idx`.  The pointer is valid for
 * the lifetime of the Vampire environment.  Do not free.
 */
const char* vampire_functor_name(unsigned int functor_idx);

/**
 * Return a pointer to the (borrowed, static-lifetime) name of the
 * predicate symbol with index `predicate_idx`.  Do not free.
 */
const char* vampire_predicate_name(unsigned int predicate_idx);

#ifdef __cplusplus
}
#endif

#endif /* __VAMPIRE_C_API__ */
