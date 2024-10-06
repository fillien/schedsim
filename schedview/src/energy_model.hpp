#ifndef ENERGY_MODEL
#define ENERGY_MODEL

namespace energy {
namespace power_constants {
constexpr double F3C{0.00000000004609381282};
constexpr double F2C{0.00000002193142733};
constexpr double F1C{0.000003410453667};
constexpr double F0C{0.04433100178};
} // namespace power_constants

auto compute_power(const double& freq) -> double;
} // namespace energy

#endif
