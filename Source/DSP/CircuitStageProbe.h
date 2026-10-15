#pragma once
#include "CircuitSolver.h"
#include <array>

// Read-only identification ports. Values are actual differential volts within
// the fully loaded circuit, with DC retained. Probing never stamps or steps it.
struct CircuitStageNodes {
    const char* name;
    int inputPositive, inputNegative, outputPositive, outputNegative;
};
struct CircuitStageProbe {const char* name;double inputVolts,outputVolts;};
template<size_t N>
std::array<CircuitStageProbe,N> readCircuitStages(const CircuitSolver& solver,const std::array<CircuitStageNodes,N>& nodes) noexcept {
    std::array<CircuitStageProbe,N> result{};
    for(size_t i=0;i<N;++i){const auto& p=nodes[i];result[i]={p.name,solver.voltage(p.inputPositive)-solver.voltage(p.inputNegative),solver.voltage(p.outputPositive)-solver.voltage(p.outputNegative)};}
    return result;
}
