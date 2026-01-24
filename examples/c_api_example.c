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
 * @file c_api_example.c
 * Example program demonstrating the Vampire C API.
 *
 * This example proves a simple theorem:
 *   Axiom: P(a)
 *   Axiom: forall X. (P(X) => Q(X))
 *   Conjecture: Q(a)
 *
 * Compile and link with the Vampire library.
 */

#include "vampire_c_api.h"
#include <stdio.h>
#include <stdlib.h>

int main(void) {
    printf("=== Vampire C API Example ===\n\n");

    // Initialize the library
    vampire_init();

    // Configure options
    vampire_set_time_limit(10);  // 10 second timeout
    vampire_set_show_proof(true);

    // Register symbols
    unsigned int a = vampire_add_function("a", 0);  // constant 'a'
    unsigned int P = vampire_add_predicate("P", 1); // predicate P/1
    unsigned int Q = vampire_add_predicate("Q", 1); // predicate Q/1

    // Build terms
    vampire_term_t* const_a = vampire_constant(a);
    vampire_term_t* var_X = vampire_var(0);  // variable X

    // Build formulas

    // Axiom 1: P(a)
    vampire_literal_t* Pa_lit = vampire_lit(P, true, &const_a, 1);
    vampire_formula_t* Pa = vampire_atom(Pa_lit);
    vampire_unit_t* axiom1 = vampire_axiom_formula(Pa);

    // Axiom 2: forall X. (P(X) => Q(X))
    vampire_literal_t* PX_lit = vampire_lit(P, true, &var_X, 1);
    vampire_literal_t* QX_lit = vampire_lit(Q, true, &var_X, 1);
    vampire_formula_t* PX = vampire_atom(PX_lit);
    vampire_formula_t* QX = vampire_atom(QX_lit);
    vampire_formula_t* PX_imp_QX = vampire_imp(PX, QX);
    vampire_formula_t* forall_PX_imp_QX = vampire_forall(0, PX_imp_QX);
    vampire_unit_t* axiom2 = vampire_axiom_formula(forall_PX_imp_QX);

    // Conjecture: Q(a)
    vampire_literal_t* Qa_lit = vampire_lit(Q, true, &const_a, 1);
    vampire_formula_t* Qa = vampire_atom(Qa_lit);
    vampire_unit_t* conj = vampire_conjecture_formula(Qa);

    // Create problem
    vampire_unit_t* units[] = {axiom1, axiom2, conj};
    vampire_problem_t* problem = vampire_problem_from_units(units, 3);

    // Solve
    printf("Proving: Q(a) from P(a) and forall X. (P(X) => Q(X))\n\n");
    vampire_proof_result_t result = vampire_prove(problem);

    // Check result
    switch (result) {
        case VAMPIRE_PROOF:
            printf("\n*** PROOF FOUND ***\n\n");
            break;
        case VAMPIRE_SATISFIABLE:
            printf("\n*** SATISFIABLE (no proof) ***\n\n");
            break;
        case VAMPIRE_TIMEOUT:
            printf("\n*** TIMEOUT ***\n\n");
            break;
        case VAMPIRE_MEMORY_LIMIT:
            printf("\n*** MEMORY LIMIT ***\n\n");
            break;
        case VAMPIRE_INCOMPLETE:
            printf("\n*** INCOMPLETE ***\n\n");
            break;
        default:
            printf("\n*** UNKNOWN ***\n\n");
            break;
    }

    // Print proof if found
    if (result == VAMPIRE_PROOF) {
        vampire_unit_t* refutation = vampire_get_refutation();
        if (refutation) {
            printf("Proof structure:\n");
            vampire_print_proof(refutation);

            // Extract structured proof
            printf("\n\nStructured proof steps:\n");
            vampire_proof_step_t* steps;
            size_t step_count;

            if (vampire_extract_proof(refutation, &steps, &step_count) == 0) {
                printf("Found %zu proof steps:\n\n", step_count);

                for (size_t i = 0; i < step_count; i++) {
                    vampire_proof_step_t* step = &steps[i];
                    printf("Step %u: [id=%u, rule=%s, input=%s]\n",
                           (unsigned int)i,
                           step->id,
                           vampire_rule_name(step->rule),
                           vampire_input_type_name(step->input_type));

                    // Print clause if available
                    vampire_clause_t* clause = vampire_unit_as_clause(step->unit);
                    if (clause) {
                        char buffer[1024];
                        if (vampire_clause_to_string(clause, buffer, sizeof(buffer)) >= 0) {
                            printf("  Clause: %s\n", buffer);
                        }

                        if (vampire_clause_is_empty(clause)) {
                            printf("  >>> EMPTY CLAUSE (refutation) <<<\n");
                        }
                    }

                    // Print premises
                    if (step->premise_count > 0) {
                        printf("  Premises: ");
                        for (size_t j = 0; j < step->premise_count; j++) {
                            printf("%u", step->premise_ids[j]);
                            if (j + 1 < step->premise_count) printf(", ");
                        }
                        printf("\n");
                    }

                    printf("\n");
                }

                vampire_free_proof_steps(steps, step_count);
            } else {
                printf("Failed to extract proof steps\n");
            }
        }
    }

    printf("\n=== Example Complete ===\n");

    return (result == VAMPIRE_PROOF) ? 0 : 1;
}
