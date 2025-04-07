#include "energy_model.hpp"
#include "protocols/hardware.hpp"

auto energy::compute_power(const double& freq, const protocols::hardware::hardware& hw) -> double
{
        constexpr double MHZ_TO_HZ{1};
        assert(hw.power_model.size() == 4);
        const double frequency = freq * MHZ_TO_HZ; // Convert MHz to Hz
        const double coef3 = hw.power_model.at(3) * frequency * frequency * frequency;
        const double coef2 = hw.power_model.at(2) * frequency * frequency;
        const double coef1 = hw.power_model.at(1) * frequency;
        const double coef0 = hw.power_model.at(0);
        return coef3 + coef2 + coef1 + coef0;
}
