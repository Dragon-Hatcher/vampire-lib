/*
 * Example: Using Vampire as a library
 *
 * This example proves a simple theorem:
 *   Given: P(a), forall x. P(x) -> Q(x)
 *   Prove: Q(a)
 *
 * In clausal form:
 *   1. P(a)           (axiom)
 *   2. ~P(X) | Q(X)   (axiom, from forall x. P(x) -> Q(x))
 *   3. ~Q(a)          (negated conjecture)
 *
 * The prover should find a refutation (proof).
 */

#include <iostream>
#include "Api/VampireAPI.hpp"

using namespace Api;
using namespace Kernel;

int main() {
    // Initialize the library
    init();

    // Configure options (optional - uses defaults otherwise)
    options().setTimeLimitInSeconds(60);

    // Register symbols
    unsigned a = addFunction("a", 0);    // constant a
    unsigned P = addPredicate("P", 1);   // unary predicate P
    unsigned Q = addPredicate("Q", 1);   // unary predicate Q

    // Create terms
    TermList aConst = constant(a);       // the constant 'a'
    TermList x = var(0);                 // variable X0

    // Create clauses:
    // 1. P(a)
    Clause* c1 = axiom({lit(P, true, {aConst})});

    // 2. ~P(X) | Q(X)  (forall x. P(x) -> Q(x) in CNF)
    Clause* c2 = axiom({lit(P, false, {x}), lit(Q, true, {x})});

    // 3. ~Q(a) (negated conjecture: we want to prove Q(a))
    Clause* c3 = conjecture({lit(Q, false, {aConst})});

    // Create the problem
    Problem* prb = problem({c1, c2, c3});

    // Run the prover
    std::cout << "Running Vampire..." << std::endl;
    ProofResult result = prove(prb);

    // Output result
    switch (result) {
        case ProofResult::PROOF:
            std::cout << "Theorem proved!" << std::endl;
            std::cout << "\n--- Proof ---" << std::endl;
            printProof(std::cout, getRefutation());
            break;
        case ProofResult::SATISFIABLE:
            std::cout << "Not a theorem (satisfiable)" << std::endl;
            break;
        case ProofResult::TIMEOUT:
            std::cout << "Timeout" << std::endl;
            break;
        case ProofResult::MEMORY_LIMIT:
            std::cout << "Memory limit exceeded" << std::endl;
            break;
        default:
            std::cout << "Unknown result" << std::endl;
    }

    // Cleanup
    delete prb;

    return result == ProofResult::PROOF ? 0 : 1;
}
