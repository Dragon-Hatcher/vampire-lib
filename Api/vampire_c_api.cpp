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
 * @file vampire_c_api.cpp
 * Implementation of the plain C API for Vampire.
 *
 * This file wraps the C++ API in C-compatible functions suitable for FFI.
 */

#include "vampire_c_api.h"
#include "VampireAPI.hpp"

#include <cstring>
#include <fstream>
#include <sstream>
#include <vector>

// Internal type conversions - cast opaque pointers to internal types
#define TO_TERM(t) (*reinterpret_cast<Kernel::TermList*>(t))
#define TO_LITERAL(l) (reinterpret_cast<Kernel::Literal*>(l))
#define TO_FORMULA(f) (reinterpret_cast<Kernel::Formula*>(f))
#define TO_UNIT(u) (reinterpret_cast<Kernel::Unit*>(u))
#define TO_CLAUSE(c) (reinterpret_cast<Kernel::Clause*>(c))
#define TO_PROBLEM(p) (reinterpret_cast<Kernel::Problem*>(p))

#define FROM_TERM(t) (reinterpret_cast<vampire_term_t*>(new Kernel::TermList(t)))
#define FROM_LITERAL(l) (reinterpret_cast<vampire_literal_t*>(l))
#define FROM_FORMULA(f) (reinterpret_cast<vampire_formula_t*>(f))
#define FROM_UNIT(u) (reinterpret_cast<vampire_unit_t*>(u))
#define FROM_CLAUSE(c) (reinterpret_cast<vampire_clause_t*>(c))
#define FROM_PROBLEM(p) (reinterpret_cast<vampire_problem_t*>(p))

extern "C" {

/* ===========================================
 * Library Initialization and Reset
 * =========================================== */

void vampire_prepare_for_next_proof(void) {
    Api::prepareForNextProof();
}

void vampire_reset(void) {
    Api::reset();
}

/* ===========================================
 * Options Configuration
 * =========================================== */

void vampire_set_time_limit(int seconds) {
    Api::options().setTimeLimitInSeconds(seconds);
}

void vampire_set_time_limit_deciseconds(int deciseconds) {
    Api::options().setTimeLimitInDeciseconds(deciseconds);
}

void vampire_set_show_proof(bool show) {
    if (show) {
        Api::options().setProof(Shell::Options::Proof::ON);
    } else {
        Api::options().setProof(Shell::Options::Proof::OFF);
    }
}

void vampire_set_saturation_algorithm(const char* algorithm) {
    Api::options().set("saturation_algorithm", algorithm);
}

/* ===========================================
 * Symbol Registration
 * =========================================== */

unsigned int vampire_add_function(const char* name, unsigned int arity) {
    return Api::addFunction(std::string(name), arity);
}

unsigned int vampire_add_predicate(const char* name, unsigned int arity) {
    return Api::addPredicate(std::string(name), arity);
}

/* ===========================================
 * Term Construction
 * =========================================== */

vampire_term_t* vampire_var(unsigned int index) {
    return FROM_TERM(Api::var(index));
}

vampire_term_t* vampire_constant(unsigned int functor) {
    return FROM_TERM(Api::constant(functor));
}

vampire_term_t* vampire_term(unsigned int functor, vampire_term_t** args, size_t arg_count) {
    std::vector<Kernel::TermList> cpp_args;
    cpp_args.reserve(arg_count);
    for (size_t i = 0; i < arg_count; i++) {
        cpp_args.push_back(TO_TERM(args[i]));
    }
    return FROM_TERM(Api::term(functor, cpp_args));
}

/* ===========================================
 * Literal Construction
 * =========================================== */

vampire_literal_t* vampire_eq(bool positive, vampire_term_t* lhs, vampire_term_t* rhs) {
    return FROM_LITERAL(Api::eq(positive, TO_TERM(lhs), TO_TERM(rhs)));
}

vampire_literal_t* vampire_lit(unsigned int pred, bool positive,
                                vampire_term_t** args, size_t arg_count) {
    std::vector<Kernel::TermList> cpp_args;
    cpp_args.reserve(arg_count);
    for (size_t i = 0; i < arg_count; i++) {
        cpp_args.push_back(TO_TERM(args[i]));
    }
    return FROM_LITERAL(Api::lit(pred, positive, cpp_args));
}

vampire_literal_t* vampire_neg(vampire_literal_t* l) {
    return FROM_LITERAL(Api::neg(TO_LITERAL(l)));
}

/* ===========================================
 * Formula Construction
 * =========================================== */

vampire_formula_t* vampire_atom(vampire_literal_t* l) {
    return FROM_FORMULA(Api::atom(TO_LITERAL(l)));
}

vampire_formula_t* vampire_not(vampire_formula_t* f) {
    return FROM_FORMULA(Api::notF(TO_FORMULA(f)));
}

vampire_formula_t* vampire_and(vampire_formula_t** formulas, size_t count) {
    std::vector<Kernel::Formula*> cpp_formulas;
    cpp_formulas.reserve(count);
    for (size_t i = 0; i < count; i++) {
        cpp_formulas.push_back(TO_FORMULA(formulas[i]));
    }
    return FROM_FORMULA(Api::andF(cpp_formulas));
}

vampire_formula_t* vampire_or(vampire_formula_t** formulas, size_t count) {
    std::vector<Kernel::Formula*> cpp_formulas;
    cpp_formulas.reserve(count);
    for (size_t i = 0; i < count; i++) {
        cpp_formulas.push_back(TO_FORMULA(formulas[i]));
    }
    return FROM_FORMULA(Api::orF(cpp_formulas));
}

vampire_formula_t* vampire_imp(vampire_formula_t* lhs, vampire_formula_t* rhs) {
    return FROM_FORMULA(Api::impF(TO_FORMULA(lhs), TO_FORMULA(rhs)));
}

vampire_formula_t* vampire_iff(vampire_formula_t* lhs, vampire_formula_t* rhs) {
    return FROM_FORMULA(Api::iffF(TO_FORMULA(lhs), TO_FORMULA(rhs)));
}

vampire_formula_t* vampire_forall(unsigned int var_index, vampire_formula_t* f) {
    return FROM_FORMULA(Api::forallF(var_index, TO_FORMULA(f)));
}

vampire_formula_t* vampire_exists(unsigned int var_index, vampire_formula_t* f) {
    return FROM_FORMULA(Api::existsF(var_index, TO_FORMULA(f)));
}

vampire_unit_t* vampire_axiom_formula(vampire_formula_t* f) {
    return FROM_UNIT(Api::axiomF(TO_FORMULA(f)));
}

vampire_unit_t* vampire_conjecture_formula(vampire_formula_t* f) {
    return FROM_UNIT(Api::conjectureF(TO_FORMULA(f)));
}

/* ===========================================
 * Clause Construction
 * =========================================== */

vampire_clause_t* vampire_axiom_clause(vampire_literal_t** literals, size_t count) {
    std::vector<Kernel::Literal*> cpp_literals;
    cpp_literals.reserve(count);
    for (size_t i = 0; i < count; i++) {
        cpp_literals.push_back(TO_LITERAL(literals[i]));
    }
    return FROM_CLAUSE(Api::axiom(cpp_literals));
}

vampire_clause_t* vampire_conjecture_clause(vampire_literal_t** literals, size_t count) {
    std::vector<Kernel::Literal*> cpp_literals;
    cpp_literals.reserve(count);
    for (size_t i = 0; i < count; i++) {
        cpp_literals.push_back(TO_LITERAL(literals[i]));
    }
    return FROM_CLAUSE(Api::conjecture(cpp_literals));
}

vampire_clause_t* vampire_clause(vampire_literal_t** literals, size_t count,
                                  vampire_input_type_t input_type) {
    std::vector<Kernel::Literal*> cpp_literals;
    cpp_literals.reserve(count);
    for (size_t i = 0; i < count; i++) {
        cpp_literals.push_back(TO_LITERAL(literals[i]));
    }

    Kernel::UnitInputType cpp_input_type;
    switch (input_type) {
        case VAMPIRE_AXIOM:
            cpp_input_type = Kernel::UnitInputType::AXIOM;
            break;
        case VAMPIRE_NEGATED_CONJECTURE:
            cpp_input_type = Kernel::UnitInputType::NEGATED_CONJECTURE;
            break;
        case VAMPIRE_CONJECTURE:
            cpp_input_type = Kernel::UnitInputType::CONJECTURE;
            break;
        default:
            cpp_input_type = Kernel::UnitInputType::AXIOM;
            break;
    }

    return FROM_CLAUSE(Api::clause(cpp_literals, cpp_input_type));
}

/* ===========================================
 * Problem and Proving
 * =========================================== */

vampire_problem_t* vampire_problem_from_clauses(vampire_clause_t** clauses, size_t count) {
    std::vector<Kernel::Clause*> cpp_clauses;
    cpp_clauses.reserve(count);
    for (size_t i = 0; i < count; i++) {
        cpp_clauses.push_back(TO_CLAUSE(clauses[i]));
    }
    return FROM_PROBLEM(Api::problem(cpp_clauses));
}

vampire_problem_t* vampire_problem_from_units(vampire_unit_t** units, size_t count) {
    std::vector<Kernel::Unit*> cpp_units;
    cpp_units.reserve(count);
    for (size_t i = 0; i < count; i++) {
        cpp_units.push_back(TO_UNIT(units[i]));
    }
    return FROM_PROBLEM(Api::problem(cpp_units));
}

vampire_proof_result_t vampire_prove(vampire_problem_t* problem) {
    Api::ProofResult result = Api::prove(TO_PROBLEM(problem));

    switch (result) {
        case Api::ProofResult::PROOF:
            return VAMPIRE_PROOF;
        case Api::ProofResult::SATISFIABLE:
            return VAMPIRE_SATISFIABLE;
        case Api::ProofResult::TIMEOUT:
            return VAMPIRE_TIMEOUT;
        case Api::ProofResult::MEMORY_LIMIT:
            return VAMPIRE_MEMORY_LIMIT;
        case Api::ProofResult::INCOMPLETE:
            return VAMPIRE_INCOMPLETE;
        default:
            return VAMPIRE_UNKNOWN;
    }
}

vampire_unit_t* vampire_get_refutation(void) {
    return FROM_UNIT(Api::getRefutation());
}

void vampire_print_proof(vampire_unit_t* refutation) {
    Api::printProof(std::cout, TO_UNIT(refutation));
}

int vampire_print_proof_to_file(const char* filename, vampire_unit_t* refutation) {
    std::ofstream out(filename);
    if (!out) {
        return -1;
    }
    Api::printProof(out, TO_UNIT(refutation));
    out.close();
    return 0;
}

/* ===========================================
 * Structured Proof Access
 * =========================================== */

static vampire_inference_rule_t convert_inference_rule(Kernel::InferenceRule rule) {
    switch (rule) {
        case Kernel::InferenceRule::INPUT:
            return VAMPIRE_RULE_INPUT;
        case Kernel::InferenceRule::RESOLUTION:
            return VAMPIRE_RULE_RESOLUTION;
        case Kernel::InferenceRule::FACTORING:
            return VAMPIRE_RULE_FACTORING;
        case Kernel::InferenceRule::SUPERPOSITION:
            return VAMPIRE_RULE_SUPERPOSITION;
        case Kernel::InferenceRule::EQUALITY_RESOLUTION:
            return VAMPIRE_RULE_EQUALITY_RESOLUTION;
        case Kernel::InferenceRule::EQUALITY_FACTORING:
            return VAMPIRE_RULE_EQUALITY_FACTORING;
        case Kernel::InferenceRule::CLAUSIFY:
            return VAMPIRE_RULE_CLAUSIFY;
        default:
            return VAMPIRE_RULE_OTHER;
    }
}

static vampire_input_type_t convert_input_type(Kernel::UnitInputType input_type) {
    switch (input_type) {
        case Kernel::UnitInputType::AXIOM:
            return VAMPIRE_AXIOM;
        case Kernel::UnitInputType::NEGATED_CONJECTURE:
            return VAMPIRE_NEGATED_CONJECTURE;
        case Kernel::UnitInputType::CONJECTURE:
            return VAMPIRE_CONJECTURE;
        default:
            return VAMPIRE_AXIOM;
    }
}

int vampire_extract_proof(vampire_unit_t* refutation,
                          vampire_proof_step_t** out_steps,
                          size_t* out_count) {
    if (!refutation || !out_steps || !out_count) {
        return -1;
    }

    std::vector<Api::ProofStep> cpp_steps = Api::extractProof(TO_UNIT(refutation));

    if (cpp_steps.empty()) {
        *out_steps = nullptr;
        *out_count = 0;
        return 0;
    }

    vampire_proof_step_t* steps = (vampire_proof_step_t*)malloc(cpp_steps.size() * sizeof(vampire_proof_step_t));
    if (!steps) {
        return -1;
    }

    for (size_t i = 0; i < cpp_steps.size(); i++) {
        const Api::ProofStep& cpp_step = cpp_steps[i];
        vampire_proof_step_t& step = steps[i];

        step.id = cpp_step.id;
        step.rule = convert_inference_rule(cpp_step.rule);
        step.input_type = convert_input_type(cpp_step.inputType);
        step.unit = FROM_UNIT(cpp_step.unit);
        step.premise_count = cpp_step.premiseIds.size();

        if (step.premise_count > 0) {
            step.premise_ids = (unsigned int*)malloc(step.premise_count * sizeof(unsigned int));
            if (!step.premise_ids) {
                // Clean up already allocated premise_ids
                for (size_t j = 0; j < i; j++) {
                    free(steps[j].premise_ids);
                }
                free(steps);
                return -1;
            }
            for (size_t j = 0; j < step.premise_count; j++) {
                step.premise_ids[j] = cpp_step.premiseIds[j];
            }
        } else {
            step.premise_ids = nullptr;
        }
    }

    *out_steps = steps;
    *out_count = cpp_steps.size();
    return 0;
}

void vampire_free_proof_steps(vampire_proof_step_t* steps, size_t count) {
    if (!steps) return;

    for (size_t i = 0; i < count; i++) {
        free(steps[i].premise_ids);
    }
    free(steps);
}

int vampire_get_literals(vampire_clause_t* clause,
                         vampire_literal_t*** out_literals,
                         size_t* out_count) {
    if (!clause || !out_literals || !out_count) {
        return -1;
    }

    std::vector<Kernel::Literal*> cpp_literals = Api::getLiterals(TO_CLAUSE(clause));

    if (cpp_literals.empty()) {
        *out_literals = nullptr;
        *out_count = 0;
        return 0;
    }

    vampire_literal_t** literals = (vampire_literal_t**)malloc(cpp_literals.size() * sizeof(vampire_literal_t*));
    if (!literals) {
        return -1;
    }

    for (size_t i = 0; i < cpp_literals.size(); i++) {
        literals[i] = FROM_LITERAL(cpp_literals[i]);
    }

    *out_literals = literals;
    *out_count = cpp_literals.size();
    return 0;
}

void vampire_free_literals(vampire_literal_t** literals) {
    free(literals);
}

vampire_clause_t* vampire_unit_as_clause(vampire_unit_t* unit) {
    if (!unit) return nullptr;

    Kernel::Unit* u = TO_UNIT(unit);
    if (u->isClause()) {
        return FROM_CLAUSE(static_cast<Kernel::Clause*>(u));
    }
    return nullptr;
}

bool vampire_clause_is_empty(vampire_clause_t* clause) {
    if (!clause) return false;
    return TO_CLAUSE(clause)->isEmpty();
}

/* ===========================================
 * String Conversions
 * =========================================== */

char* vampire_term_to_string(vampire_term_t* term) {
    if (!term) {
        return nullptr;
    }

    std::string str = Api::termToString(TO_TERM(term));
    char* result = static_cast<char*>(malloc(str.length() + 1));
    if (result) {
        std::strcpy(result, str.c_str());
    }
    return result;
}

char* vampire_literal_to_string(vampire_literal_t* literal) {
    if (!literal) {
        return nullptr;
    }

    std::string str = Api::literalToString(TO_LITERAL(literal));
    char* result = static_cast<char*>(malloc(str.length() + 1));
    if (result) {
        std::strcpy(result, str.c_str());
    }
    return result;
}

char* vampire_clause_to_string(vampire_clause_t* clause) {
    if (!clause) {
        return nullptr;
    }

    std::string str = Api::clauseToString(TO_CLAUSE(clause));
    char* result = static_cast<char*>(malloc(str.length() + 1));
    if (result) {
        std::strcpy(result, str.c_str());
    }
    return result;
}

char* vampire_formula_to_string(vampire_formula_t* formula) {
    if (!formula) {
        return nullptr;
    }

    std::string str = Api::formulaToString(TO_FORMULA(formula));
    char* result = static_cast<char*>(malloc(str.length() + 1));
    if (result) {
        std::strcpy(result, str.c_str());
    }
    return result;
}

void vampire_free_string(char* str) {
    free(str);
}

const char* vampire_rule_name(vampire_inference_rule_t rule) {
    switch (rule) {
        case VAMPIRE_RULE_INPUT: return "input";
        case VAMPIRE_RULE_RESOLUTION: return "resolution";
        case VAMPIRE_RULE_FACTORING: return "factoring";
        case VAMPIRE_RULE_SUPERPOSITION: return "superposition";
        case VAMPIRE_RULE_EQUALITY_RESOLUTION: return "equality_resolution";
        case VAMPIRE_RULE_EQUALITY_FACTORING: return "equality_factoring";
        case VAMPIRE_RULE_CLAUSIFY: return "clausify";
        case VAMPIRE_RULE_OTHER: return "other";
        default: return "unknown";
    }
}

const char* vampire_input_type_name(vampire_input_type_t input_type) {
    switch (input_type) {
        case VAMPIRE_AXIOM: return "axiom";
        case VAMPIRE_NEGATED_CONJECTURE: return "negated_conjecture";
        case VAMPIRE_CONJECTURE: return "conjecture";
        default: return "unknown";
    }
}

} // extern "C"
