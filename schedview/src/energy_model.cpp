#include "energy_model.hpp"

auto energy::compute_power(const double& freq) -> double
{
        constexpr double MHZ_TO_HZ{1};

        const double frequency = freq * MHZ_TO_HZ; // Convert MHz to Hz
        const double coef3 = power_constants::F3C * frequency * frequency * frequency;
        const double coef2 = power_constants::F2C * frequency * frequency;
        const double coef1 = power_constants::F1C * frequency;
        const double coef0 = power_constants::F0C;
        return coef3 + coef2 + coef1 + coef0;
}
