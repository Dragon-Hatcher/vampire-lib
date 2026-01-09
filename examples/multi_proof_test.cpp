/*
 * Test: Running multiple proofs in sequence
 *
 * This tests that the library can handle multiple independent
 * proving tasks in a row.
 */

#include <iostream>
#include "Api/VampireAPI.hpp"

using namespace Api;
using namespace Kernel;

bool runProof1() {
    std::cout << "=== Proof 1: P(a), P(x)->Q(x) |- Q(a) ===" << std::endl;

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

    if (result == ProofResult::PROOF) {
        std::cout << "PASSED: Theorem proved" << std::endl;
        printProof(std::cout, getRefutation());
        delete prb;
        return true;
    } else {
        std::cout << "FAILED: Expected proof" << std::endl;
        delete prb;
        return false;
    }
}

bool runProof2() {
    std::cout << "\n=== Proof 2: R(b,c), R(x,y)->S(y) |- S(c) ===" << std::endl;

    unsigned b = addFunction("b", 0);
    unsigned c = addFunction("c", 0);
    unsigned R = addPredicate("R", 2);
    unsigned S = addPredicate("S", 1);

    TermList bConst = constant(b);
    TermList cConst = constant(c);
    TermList x = var(0);
    TermList y = var(1);

    Clause* c1 = axiom({lit(R, true, {bConst, cConst})});
    Clause* c2 = axiom({lit(R, false, {x, y}), lit(S, true, {y})});
    Clause* c3 = conjecture({lit(S, false, {cConst})});

    Problem* prb = problem({c1, c2, c3});
    ProofResult result = prove(prb);

    if (result == ProofResult::PROOF) {
        std::cout << "PASSED: Theorem proved" << std::endl;
        printProof(std::cout, getRefutation());
        delete prb;
        return true;
    } else {
        std::cout << "FAILED: Expected proof" << std::endl;
        delete prb;
        return false;
    }
}

bool runProof3() {
    std::cout << "\n=== Proof 3: Equality - f(a)=b, b=c |- f(a)=c ===" << std::endl;

    unsigned a = addFunction("a", 0);
    unsigned b = addFunction("b", 0);
    unsigned c = addFunction("c", 0);
    unsigned f = addFunction("f", 1);

    TermList aConst = constant(a);
    TermList bConst = constant(b);
    TermList cConst = constant(c);
    TermList fa = term(f, {aConst});

    // f(a) = b
    Clause* c1 = axiom({eq(true, fa, bConst)});
    // b = c
    Clause* c2 = axiom({eq(true, bConst, cConst)});
    // ~(f(a) = c) - negated conjecture
    Clause* c3 = conjecture({eq(false, fa, cConst)});

    Problem* prb = problem({c1, c2, c3});
    ProofResult result = prove(prb);

    if (result == ProofResult::PROOF) {
        std::cout << "PASSED: Theorem proved" << std::endl;
        printProof(std::cout, getRefutation());
        delete prb;
        return true;
    } else {
        std::cout << "FAILED: Expected proof" << std::endl;
        delete prb;
        return false;
    }
}

bool runProof4() {
    std::cout << "\n=== Proof 4: Satisfiable (should NOT find proof) ===" << std::endl;

    // P(a) alone - no contradiction
    unsigned a = addFunction("a", 0);
    unsigned P = addPredicate("P", 1);

    TermList aConst = constant(a);
    Clause* c1 = axiom({lit(P, true, {aConst})});

    Problem* prb = problem({c1});
    ProofResult result = prove(prb);

    if (result == ProofResult::SATISFIABLE) {
        std::cout << "PASSED: Correctly identified as satisfiable" << std::endl;
        delete prb;
        return true;
    } else if (result == ProofResult::PROOF) {
        std::cout << "FAILED: Should not find proof for satisfiable problem" << std::endl;
        delete prb;
        return false;
    } else {
        std::cout << "PASSED: No proof found (result: " << static_cast<int>(result) << ")" << std::endl;
        delete prb;
        return true;
    }
}

int main() {
    init();
    options().setTimeLimitInSeconds(10);

    int passed = 0;
    int failed = 0;

    // Proof 1
    if (runProof1()) passed++; else failed++;
    prepareForNextProof();

    // Proof 2
    if (runProof2()) passed++; else failed++;
    prepareForNextProof();

    // Proof 3 (with equality)
    if (runProof3()) passed++; else failed++;
    prepareForNextProof();

    // Proof 4 (satisfiable - no proof)
    if (runProof4()) passed++; else failed++;

    std::cout << "\n=== Summary ===" << std::endl;
    std::cout << "Passed: " << passed << std::endl;
    std::cout << "Failed: " << failed << std::endl;

    return failed == 0 ? 0 : 1;
}
