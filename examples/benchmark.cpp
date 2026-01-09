/*
 * Benchmark: Measure proof throughput
 */

#include <iostream>
#include <chrono>
#include "Api/VampireAPI.hpp"

using namespace Api;
using namespace Kernel;

// Simple proof: P(a), P(x)->Q(x) |- Q(a)
// Uses fresh symbols each time (requires full reset)
bool runTrivialProof() {
    unsigned a = addFunction("a", 0);
    unsigned P = addPredicate("P", 1);
    unsigned Q = addPredicate("Q", 1);

    TermList aConst = constant(a);
    TermList x = var(0);

    Clause* c1 = axiom({lit(P, true, {aConst})});
    Clause* c2 = axiom({lit(P, false, {x}), lit(Q, true, {x})});
    Clause* c3 = conjecture({lit(Q, false, {aConst})});

    Problem* prb = problem({c1, c2, c3});
    ProofResult result = prove(prb);

    delete prb;

    return result == ProofResult::PROOF;
}

int main(int argc, char** argv) {
    int numProofs = 100;

    if (argc > 1) {
        numProofs = std::atoi(argv[1]);
    }

    init();
    options().setTimeLimitInSeconds(10);

    std::cout << "Running " << numProofs << " trivial proofs with full reset..." << std::endl;

    auto start = std::chrono::high_resolution_clock::now();

    int succeeded = 0;
    for (int i = 0; i < numProofs; i++) {
        bool ok = runTrivialProof();
        if (ok) {
            succeeded++;
        }
        if (i % 2500 == 0) std::cout << "Completed " << i << std::endl;
        // Full reset between proofs (allows reusing symbol names)
        reset();
    }

    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> elapsed = end - start;

    std::cout << "\nResults:" << std::endl;
    std::cout << "  Proofs completed: " << succeeded << "/" << numProofs << std::endl;
    std::cout << "  Total time: " << elapsed.count() << " seconds" << std::endl;
    std::cout << "  Throughput: " << (numProofs / elapsed.count()) << " proofs/second" << std::endl;
    std::cout << "  Average time per proof: " << (elapsed.count() / numProofs * 1000) << " ms" << std::endl;

    return succeeded == numProofs ? 0 : 1;
}
