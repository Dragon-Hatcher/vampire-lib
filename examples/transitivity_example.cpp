/*
 * Example: Proving transitivity of less-than
 *
 * Problem:
 *   Axiom 1: forall x,y,z. (x < y & y < z) => x < z   (transitivity)
 *   Axiom 2: a < b
 *   Axiom 3: b < c
 *   Goal: Prove a < c
 */

#include <iostream>
#include "Api/VampireAPI.hpp"

using namespace Api;
using namespace Kernel;

int main() {
    options().setTimeLimitInSeconds(10);

    // Register symbols
    unsigned lt = addPredicate("lt", 2);  // lt(x, y) means x < y
    unsigned a = addFunction("a", 0);     // constant a
    unsigned b = addFunction("b", 0);     // constant b
    unsigned c = addFunction("c", 0);     // constant c

    // Build variables
    TermList x = var(0);
    TermList y = var(1);
    TermList z = var(2);

    // Build constants
    TermList aConst = constant(a);
    TermList bConst = constant(b);
    TermList cConst = constant(c);

    // ------------------------------------------------------------------
    // Axiom 1: Transitivity
    // forall x,y,z. (x < y & y < z) => x < z
    // ------------------------------------------------------------------
    Formula* ltXY = atom(lit(lt, true, {x, y}));
    Formula* ltYZ = atom(lit(lt, true, {y, z}));
    Formula* ltXZ = atom(lit(lt, true, {x, z}));

    Formula* premise = andF({ltXY, ltYZ});
    Formula* implication = impF(premise, ltXZ);

    // Quantify over all three variables (innermost to outermost)
    Formula* transitivity = forallF(2,         // forall z
                              forallF(1,       // forall y
                                forallF(0,     // forall x
                                  implication)));

    // ------------------------------------------------------------------
    // Axiom 2: a < b
    // ------------------------------------------------------------------
    Formula* ltAB = atom(lit(lt, true, {aConst, bConst}));

    // ------------------------------------------------------------------
    // Axiom 3: b < c
    // ------------------------------------------------------------------
    Formula* ltBC = atom(lit(lt, true, {bConst, cConst}));

    // ------------------------------------------------------------------
    // Goal: a < c
    // ------------------------------------------------------------------
    Formula* ltAC = atom(lit(lt, true, {aConst, cConst}));

    // Create problem
    Unit* ax1 = axiomF(transitivity);
    Unit* ax2 = axiomF(ltAB);
    Unit* ax3 = axiomF(ltBC);
    Unit* conj = conjectureF(ltAC);

    Problem* prb = problem({ax1, ax2, ax3, conj});

    std::cout << "Problem: Prove transitivity of <" << std::endl;
    std::cout << "  Axiom 1: forall x,y,z. (x < y & y < z) => x < z" << std::endl;
    std::cout << "  Axiom 2: a < b" << std::endl;
    std::cout << "  Axiom 3: b < c" << std::endl;
    std::cout << "  Goal: a < c" << std::endl;
    std::cout << std::endl;

    ProofResult result = prove(prb);

    if (result == ProofResult::PROOF) {
        std::cout << "PROVED!" << std::endl << std::endl;

        // Extract and display proof
        Unit* refutation = getRefutation();
        printProof(std::cout, getRefutation());

        auto steps = extractProof(refutation);

        std::cout << "Proof steps:" << std::endl;
        for (const auto& step : steps) {
            std::cout << "  [" << step.id << "] ";

            if (step.isInput()) {
                std::cout << step.inputTypeName();
            } else {
                std::cout << step.ruleName();
                if (!step.premiseIds.empty()) {
                    std::cout << " from {";
                    for (size_t i = 0; i < step.premiseIds.size(); i++) {
                        if (i > 0) std::cout << ", ";
                        std::cout << step.premiseIds[i];
                    }
                    std::cout << "}";
                }
            }

            std::cout << ": ";
            if (step.clause()) {
                std::cout << clauseToString(step.clause());
            }
            std::cout << std::endl;
        }
    } else {
        std::cout << "Failed to prove (result: " << static_cast<int>(result) << ")" << std::endl;
    }

    delete prb;
    return result == ProofResult::PROOF ? 0 : 1;
}
