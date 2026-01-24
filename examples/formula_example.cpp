/*
 * Example: Using first-order formulas (non-CNF)
 *
 * This example shows how to construct first-order logic formulas
 * and have Vampire clausify and prove them.
 */

#include <iostream>
#include "Api/VampireAPI.hpp"

using namespace Api;
using namespace Kernel;

int main() {
    options().setTimeLimitInSeconds(10);

    // Register symbols
    unsigned person = addPredicate("person", 1);   // person(x)
    unsigned mortal = addPredicate("mortal", 1);   // mortal(x)
    unsigned socrates = addFunction("socrates", 0); // socrates constant

    // Build: forall x. (person(x) => mortal(x))
    // "All persons are mortal"
    TermList x = var(0);
    Formula* personX = atom(lit(person, true, {x}));
    Formula* mortalX = atom(lit(mortal, true, {x}));
    Formula* allPersonsMortal = forallF(0, impF(personX, mortalX));

    // Build: person(socrates)
    // "Socrates is a person"
    TermList soc = constant(socrates);
    Formula* personSocrates = atom(lit(person, true, {soc}));

    // Build: mortal(socrates)
    // "Socrates is mortal" (this is what we want to prove)
    Formula* mortalSocrates = atom(lit(mortal, true, {soc}));

    // Create problem with axioms and conjecture
    Unit* ax1 = axiomF(allPersonsMortal);
    Unit* ax2 = axiomF(personSocrates);
    Unit* conj = conjectureF(mortalSocrates);

    Problem* prb = problem({ax1, ax2, conj});

    std::cout << "Proving: All persons are mortal. Socrates is a person. "
              << "Therefore, Socrates is mortal." << std::endl;

    ProofResult result = prove(prb);

    if (result == ProofResult::PROOF) {
        std::cout << "PROVED!" << std::endl;

        // Extract and display proof
        Unit* refutation = getRefutation();
        auto steps = extractProof(refutation);

        std::cout << "\nProof has " << steps.size() << " steps:" << std::endl;
        for (const auto& step : steps) {
            std::cout << "  [" << step.id << "] ";
            if (step.isInput()) {
                std::cout << step.inputTypeName() << ": ";
            } else {
                std::cout << step.ruleName() << " from {";
                for (size_t i = 0; i < step.premiseIds.size(); i++) {
                    if (i > 0) std::cout << ", ";
                    std::cout << step.premiseIds[i];
                }
                std::cout << "}: ";
            }

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
