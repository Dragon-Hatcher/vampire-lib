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
 */

#include "vampire_c_api.h"
#include "VampireAPI.hpp"
#include "Lib/Environment.hpp"
#include "Kernel/Signature.hpp"

#include <cstring>
#include <fstream>
#include <functional>
#include <sstream>
#include <vector>
#include <iostream>

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

static bool formulaEqual(const Kernel::Formula* a, const Kernel::Formula* b) {
    if (a == b) return true;
    if (a->connective() != b->connective()) return false;
    switch (a->connective()) {
        case Kernel::TRUE: case Kernel::FALSE: return true;
        case Kernel::LITERAL: return a->literal() == b->literal();
        case Kernel::NOT: return formulaEqual(a->uarg(), b->uarg());
        case Kernel::AND: case Kernel::OR: {
            auto* la = a->args(); auto* lb = b->args();
            while (la && lb) { if (!formulaEqual(la->head(), lb->head())) return false; la = la->tail(); lb = lb->tail(); }
            return !la && !lb;
        }
        case Kernel::IMP: case Kernel::IFF: case Kernel::XOR:
            return formulaEqual(a->left(), b->left()) && formulaEqual(a->right(), b->right());
        case Kernel::FORALL: case Kernel::EXISTS: {
            auto* va = a->vars(); auto* vb = b->vars();
            while (va && vb) { if (va->head() != vb->head()) return false; va = va->tail(); vb = vb->tail(); }
            return !va && !vb && formulaEqual(a->qarg(), b->qarg());
        }
        default: return false;
    }
}

static size_t hashCombine(size_t h, size_t v) { return h ^ (v + 0x9e3779b9 + (h << 6) + (h >> 2)); }
static size_t formulaHash(const Kernel::Formula* f) {
    size_t h = std::hash<unsigned>{}(static_cast<unsigned>(f->connective()));
    switch (f->connective()) {
        case Kernel::LITERAL: h = hashCombine(h, std::hash<const void*>{}(f->literal())); break;
        case Kernel::NOT: h = hashCombine(h, formulaHash(f->uarg())); break;
        case Kernel::AND: case Kernel::OR: { auto* l = f->args(); while (l) { h = hashCombine(h, formulaHash(l->head())); l = l->tail(); } break; }
        case Kernel::IMP: case Kernel::IFF: case Kernel::XOR: h = hashCombine(h, formulaHash(f->left())); h = hashCombine(h, formulaHash(f->right())); break;
        case Kernel::FORALL: case Kernel::EXISTS: { auto* v = f->vars(); while (v) { h = hashCombine(h, std::hash<unsigned>{}(v->head())); v = v->tail(); } h = hashCombine(h, formulaHash(f->qarg())); break; }
        default: break;
    }
    return h;
}

extern "C" {

void vampire_prepare_for_next_proof(void) { Api::prepareForNextProof(); }
void vampire_reset(void) { Api::reset(); }

void vampire_set_time_limit_milliseconds(int ms) { Api::options().setTimeLimitInMilliseconds(ms); }

void vampire_set_saturation_algorithm(const char* algorithm) {
    try { Api::options().set("saturation_algorithm", algorithm); } catch (...) {}
}

int vampire_set_option(const char* name, const char* value) {
    try {
        Api::options().set(name, value);
        return 0;
    } catch (Lib::UserErrorException& e) {
        std::cerr << "[Vampire] set_option error: " << e.msg() << std::endl;
        return -1;
    } catch (...) {
        return -1;
    }
}

unsigned int vampire_add_function(const char* name, unsigned int arity) { return Api::addFunction(std::string(name), arity); }
unsigned int vampire_add_predicate(const char* name, unsigned int arity) { return Api::addPredicate(std::string(name), arity); }

static Interpretation map_interpretation(vampire_interpretation_t interp) {
    switch (interp) {
        case VAMPIRE_INTERP_EQUAL: return Theory::EQUAL;
        case VAMPIRE_INTERP_INT_GREATER: return Theory::INT_GREATER;
        case VAMPIRE_INTERP_INT_GREATER_EQUAL: return Theory::INT_GREATER_EQUAL;
        case VAMPIRE_INTERP_INT_LESS: return Theory::INT_LESS;
        case VAMPIRE_INTERP_INT_LESS_EQUAL: return Theory::INT_LESS_EQUAL;
        case VAMPIRE_INTERP_INT_DIVIDES: return Theory::INT_DIVIDES;
        case VAMPIRE_INTERP_INT_SUCCESSOR: return Theory::INT_SUCCESSOR;
        case VAMPIRE_INTERP_INT_UNARY_MINUS: return Theory::INT_UNARY_MINUS;
        case VAMPIRE_INTERP_INT_PLUS: return Theory::INT_PLUS;
        case VAMPIRE_INTERP_INT_MINUS: return Theory::INT_MINUS;
        case VAMPIRE_INTERP_INT_MULTIPLY: return Theory::INT_MULTIPLY;
        case VAMPIRE_INTERP_INT_QUOTIENT_E: return Theory::INT_QUOTIENT_E;
        case VAMPIRE_INTERP_INT_QUOTIENT_T: return Theory::INT_QUOTIENT_T;
        case VAMPIRE_INTERP_INT_QUOTIENT_F: return Theory::INT_QUOTIENT_F;
        case VAMPIRE_INTERP_INT_REMAINDER_E: return Theory::INT_REMAINDER_E;
        case VAMPIRE_INTERP_INT_REMAINDER_T: return Theory::INT_REMAINDER_T;
        case VAMPIRE_INTERP_INT_REMAINDER_F: return Theory::INT_REMAINDER_F;
        case VAMPIRE_INTERP_INT_ABS: return Theory::INT_ABS;
        case VAMPIRE_INTERP_RAT_GREATER: return Theory::RAT_GREATER;
        case VAMPIRE_INTERP_RAT_GREATER_EQUAL: return Theory::RAT_GREATER_EQUAL;
        case VAMPIRE_INTERP_RAT_LESS: return Theory::RAT_LESS;
        case VAMPIRE_INTERP_RAT_LESS_EQUAL: return Theory::RAT_LESS_EQUAL;
        case VAMPIRE_INTERP_RAT_PLUS: return Theory::RAT_PLUS;
        case VAMPIRE_INTERP_RAT_MINUS: return Theory::RAT_MINUS;
        case VAMPIRE_INTERP_RAT_MULTIPLY: return Theory::RAT_MULTIPLY;
        case VAMPIRE_INTERP_RAT_QUOTIENT: return Theory::RAT_QUOTIENT;
        case VAMPIRE_INTERP_REAL_GREATER: return Theory::REAL_GREATER;
        case VAMPIRE_INTERP_REAL_GREATER_EQUAL: return Theory::REAL_GREATER_EQUAL;
        case VAMPIRE_INTERP_REAL_LESS: return Theory::REAL_LESS;
        case VAMPIRE_INTERP_REAL_LESS_EQUAL: return Theory::REAL_LESS_EQUAL;
        case VAMPIRE_INTERP_REAL_PLUS: return Theory::REAL_PLUS;
        case VAMPIRE_INTERP_REAL_MINUS: return Theory::REAL_MINUS;
        case VAMPIRE_INTERP_REAL_MULTIPLY: return Theory::REAL_MULTIPLY;
        case VAMPIRE_INTERP_REAL_QUOTIENT: return Theory::REAL_QUOTIENT;
        default: return Theory::EQUAL;
    }
}

unsigned int vampire_add_interpreted_function(const char* name, vampire_interpretation_t interp) { return Api::addInterpretedFunction(std::string(name), map_interpretation(interp)); }
unsigned int vampire_add_interpreted_predicate(const char* name, vampire_interpretation_t interp) { return Api::addInterpretedPredicate(std::string(name), map_interpretation(interp)); }
unsigned int vampire_add_integer_constant(const char* value) { return Api::addIntegerConstant(std::string(value)); }
unsigned int vampire_add_rational_constant(const char* value) { return Api::addRationalConstant(std::string(value)); }
unsigned int vampire_add_real_constant(const char* value) { return Api::addRealConstant(std::string(value)); }

unsigned int vampire_add_sort(const char* name) { return Api::addSort(std::string(name)); }
unsigned int vampire_sort_default(void) { return Api::defaultSort(); }
unsigned int vampire_sort_int(void) { return Api::sortInt(); }
unsigned int vampire_sort_real(void) { return Api::sortReal(); }
unsigned int vampire_sort_rational(void) { return Api::sortRational(); }

unsigned int vampire_add_typed_function(const char* name, unsigned int* arg_sorts, size_t arity, unsigned int res_sort) {
    std::vector<unsigned> args(arg_sorts, arg_sorts + arity);
    return Api::addTypedFunction(std::string(name), args, res_sort);
}
unsigned int vampire_add_typed_predicate(const char* name, unsigned int* arg_sorts, size_t arity) {
    std::vector<unsigned> args(arg_sorts, arg_sorts + arity);
    return Api::addTypedPredicate(std::string(name), args);
}

vampire_formula_t* vampire_forall_typed(unsigned int var, unsigned int sort, vampire_formula_t* f) { return FROM_FORMULA(Api::forallTyped(var, sort, TO_FORMULA(f))); }
vampire_formula_t* vampire_exists_typed(unsigned int var, unsigned int sort, vampire_formula_t* f) { return FROM_FORMULA(Api::existsTyped(var, sort, TO_FORMULA(f))); }
vampire_literal_t* vampire_typed_eq(bool pos, vampire_term_t* lhs, vampire_term_t* rhs, unsigned int sort) { return FROM_LITERAL(Api::typedEq(pos, TO_TERM(lhs), TO_TERM(rhs), sort)); }

vampire_term_t* vampire_var(unsigned int index) { return FROM_TERM(Api::var(index)); }
vampire_term_t* vampire_constant(unsigned int functor) { return FROM_TERM(Api::constant(functor)); }
vampire_term_t* vampire_term(unsigned int functor, vampire_term_t** args, size_t count) {
    std::vector<Kernel::TermList> cpp_args;
    for (size_t i = 0; i < count; i++) cpp_args.push_back(TO_TERM(args[i]));
    return FROM_TERM(Api::term(functor, cpp_args));
}
char* vampire_term_to_string(vampire_term_t* t) { return strdup(Api::termToString(TO_TERM(t)).c_str()); }

vampire_literal_t* vampire_eq(bool pos, vampire_term_t* lhs, vampire_term_t* rhs) { return FROM_LITERAL(Api::eq(pos, TO_TERM(lhs), TO_TERM(rhs))); }
vampire_literal_t* vampire_lit(unsigned int pred, bool positive, vampire_term_t** args, size_t count) {
    std::vector<Kernel::TermList> cpp_args;
    for (size_t i = 0; i < count; i++) cpp_args.push_back(TO_TERM(args[i]));
    return FROM_LITERAL(Api::lit(pred, positive, cpp_args));
}
vampire_literal_t* vampire_neg(vampire_literal_t* l) { return FROM_LITERAL(Api::neg(TO_LITERAL(l))); }

vampire_formula_t* vampire_atom(vampire_literal_t* l) { return FROM_FORMULA(Api::atom(TO_LITERAL(l))); }
vampire_formula_t* vampire_not(vampire_formula_t* f) { return FROM_FORMULA(Api::notF(TO_FORMULA(f))); }
vampire_formula_t* vampire_and(vampire_formula_t** formulas, size_t count) {
    std::vector<Kernel::Formula*> cpp_fs;
    for (size_t i = 0; i < count; i++) cpp_fs.push_back(TO_FORMULA(formulas[i]));
    return FROM_FORMULA(Api::andF(cpp_fs));
}
vampire_formula_t* vampire_or(vampire_formula_t** formulas, size_t count) {
    std::vector<Kernel::Formula*> cpp_fs;
    for (size_t i = 0; i < count; i++) cpp_fs.push_back(TO_FORMULA(formulas[i]));
    return FROM_FORMULA(Api::orF(cpp_fs));
}
vampire_formula_t* vampire_imp(vampire_formula_t* lhs, vampire_formula_t* rhs) { return FROM_FORMULA(Api::impF(TO_FORMULA(lhs), TO_FORMULA(rhs))); }
vampire_formula_t* vampire_iff(vampire_formula_t* lhs, vampire_formula_t* rhs) { return FROM_FORMULA(Api::iffF(TO_FORMULA(lhs), TO_FORMULA(rhs))); }
vampire_formula_t* vampire_forall(unsigned int var, vampire_formula_t* f) { return FROM_FORMULA(Api::forallF(var, TO_FORMULA(f))); }
vampire_formula_t* vampire_exists(unsigned int var, vampire_formula_t* f) { return FROM_FORMULA(Api::existsF(var, TO_FORMULA(f))); }
vampire_formula_t* vampire_true(void) { return FROM_FORMULA(Api::trueF()); }
vampire_formula_t* vampire_false(void) { return FROM_FORMULA(Api::falseF()); }
char* vampire_formula_to_string(vampire_formula_t* f) { return strdup(Api::formulaToString(TO_FORMULA(f)).c_str()); }

vampire_unit_t* vampire_axiom_formula(vampire_formula_t* f) { return FROM_UNIT(Api::axiomF(TO_FORMULA(f))); }
vampire_unit_t* vampire_conjecture_formula(vampire_formula_t* f) { return FROM_UNIT(Api::conjectureF(TO_FORMULA(f))); }

vampire_problem_t* vampire_problem_from_units(vampire_unit_t** units, size_t count) {
    std::vector<Kernel::Unit*> cpp_units;
    for (size_t i = 0; i < count; i++) cpp_units.push_back(TO_UNIT(units[i]));
    return FROM_PROBLEM(Api::problem(cpp_units));
}

vampire_proof_result_t vampire_prove(vampire_problem_t* problem) {
    try {
        Api::ProofResult result = Api::prove(TO_PROBLEM(problem));
        if (result == Api::ProofResult::PROOF) return VAMPIRE_PROOF;
        if (result == Api::ProofResult::SATISFIABLE) return VAMPIRE_SATISFIABLE;
        if (result == Api::ProofResult::TIMEOUT) return VAMPIRE_TIMEOUT;
        if (result == Api::ProofResult::MEMORY_LIMIT) return VAMPIRE_MEMORY_LIMIT;
        if (result == Api::ProofResult::INCOMPLETE) return VAMPIRE_INCOMPLETE;
        return VAMPIRE_UNKNOWN;
    } catch (std::exception& e) {
        std::cerr << "[Vampire] prove() std::exception: " << e.what() << std::endl;
        return VAMPIRE_UNKNOWN;
    } catch (...) {
        std::cerr << "[Vampire] prove() unknown exception" << std::endl;
        return VAMPIRE_UNKNOWN;
    }
}

size_t vampire_clausify(vampire_problem_t* problem) { Api::clausify(TO_PROBLEM(problem)); return Api::getCNF(TO_PROBLEM(problem)).size(); }
int vampire_get_cnf(vampire_problem_t* problem, char*** out_clauses, size_t* out_count) {
    std::vector<std::string> cpp_clauses = Api::getCNF(TO_PROBLEM(problem));
    *out_count = cpp_clauses.size();
    char** clauses = (char**)malloc(cpp_clauses.size() * sizeof(char*));
    for (size_t i = 0; i < cpp_clauses.size(); i++) clauses[i] = strdup(cpp_clauses[i].c_str());
    *out_clauses = clauses;
    return 0;
}
void vampire_free_string_array(char** strings, size_t count) { for (size_t i = 0; i < count; i++) free(strings[i]); free(strings); }

vampire_unit_t* vampire_get_refutation(void) { return FROM_UNIT(Api::getRefutation()); }
int vampire_extract_proof(vampire_unit_t* ref, vampire_proof_step_t** out_steps, size_t* out_count) {
    std::vector<Api::ProofStep> cpp_steps = Api::extractProof(TO_UNIT(ref));
    *out_count = cpp_steps.size();
    vampire_proof_step_t* steps = (vampire_proof_step_t*)malloc(cpp_steps.size() * sizeof(vampire_proof_step_t));
    for (size_t i = 0; i < cpp_steps.size(); i++) {
        steps[i].id = cpp_steps[i].id; steps[i].rule = (vampire_inference_rule_t)cpp_steps[i].rule;
        steps[i].unit = FROM_UNIT(cpp_steps[i].unit); steps[i].premise_count = cpp_steps[i].premiseIds.size();
        steps[i].premise_ids = (unsigned int*)malloc(steps[i].premise_count * sizeof(unsigned int));
        for (size_t j = 0; j < steps[i].premise_count; j++) steps[i].premise_ids[j] = cpp_steps[i].premiseIds[j];
    }
    *out_steps = steps; return 0;
}
void vampire_free_proof_steps(vampire_proof_step_t* steps, size_t count) { for (size_t i = 0; i < count; i++) free(steps[i].premise_ids); free(steps); }
vampire_formula_t* vampire_unit_as_formula(vampire_unit_t* u) { return FROM_FORMULA(TO_UNIT(u)->getFormula()); }
void vampire_free_unit(vampire_unit_t* u) { Api::destroyUnit(TO_UNIT(u)); }

char* vampire_clause_to_string(vampire_clause_t* c) { return strdup(Api::clauseToString(TO_CLAUSE(c)).c_str()); }
void vampire_free_string(char* s) { free(s); }
bool vampire_term_equal(vampire_term_t* a, vampire_term_t* b) { return TO_TERM(a) == TO_TERM(b); }
uint64_t vampire_term_hash(vampire_term_t* a) { return TO_TERM(a).content(); }
bool vampire_formula_equal(vampire_formula_t* a, vampire_formula_t* b) { return formulaEqual(TO_FORMULA(a), TO_FORMULA(b)); }
uint64_t vampire_formula_hash(vampire_formula_t* a) { return static_cast<uint64_t>(formulaHash(TO_FORMULA(a))); }

// ---- Structured Literal / Term readers -------------------------------------
//
// Thin wrappers over Kernel::Literal / Kernel::Term / Kernel::TermList that
// surface the structure the Rust IR layer needs to rebuild a vampire_prover
// clause without round-tripping through string parsing.  All returned
// vampire_term_t* pointers wrap heap-allocated TermList copies (matching
// FROM_TERM's convention elsewhere in this file).

unsigned int vampire_literal_predicate(vampire_literal_t* lit) {
    return TO_LITERAL(lit)->functor();
}

bool vampire_literal_is_positive(vampire_literal_t* lit) {
    return TO_LITERAL(lit)->isPositive();
}

bool vampire_literal_is_equality(vampire_literal_t* lit) {
    return TO_LITERAL(lit)->isEquality();
}

size_t vampire_literal_arity(vampire_literal_t* lit) {
    return TO_LITERAL(lit)->arity();
}

vampire_term_t* vampire_literal_arg(vampire_literal_t* lit, size_t i) {
    return FROM_TERM(*TO_LITERAL(lit)->nthArgument(static_cast<int>(i)));
}

bool vampire_term_is_var(vampire_term_t* term) {
    return TO_TERM(term).isVar();
}

unsigned int vampire_term_var_index(vampire_term_t* term) {
    return TO_TERM(term).var();
}

unsigned int vampire_term_functor(vampire_term_t* term) {
    return TO_TERM(term).term()->functor();
}

size_t vampire_term_arity(vampire_term_t* term) {
    Kernel::TermList& tl = TO_TERM(term);
    if (tl.isVar()) return 0;
    return tl.term()->arity();
}

vampire_term_t* vampire_term_arg(vampire_term_t* term, size_t i) {
    return FROM_TERM(*TO_TERM(term).term()->nthArgument(static_cast<int>(i)));
}

const char* vampire_functor_name(unsigned int functor_idx) {
    return Lib::env.signature->getFunction(functor_idx)->name().c_str();
}

const char* vampire_predicate_name(unsigned int predicate_idx) {
    return Lib::env.signature->getPredicate(predicate_idx)->name().c_str();
}

} // extern "C"
