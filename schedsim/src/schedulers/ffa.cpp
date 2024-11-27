#include <cassert>
#include <cmath>
#include <schedulers/ffa.hpp>

auto ffa::compute_freq_min(
    const double& freq_max,
    const double& total_util,
    const double& max_util,
    const double& nb_procs) -> double
{
        return (freq_max * (total_util + (nb_procs - 1) * max_util)) / nb_procs;
}

auto ffa::get_nb_active_procs(const double& new_utilization) const -> std::size_t
{
        return nb_active_procs;
}

void ffa::update_platform()
{
        const auto chip = sim()->chip();
        const double total_util{get_active_bandwidth()};
        const double max_util{get_max_utilization(servers)};
        const double max_nb_procs{static_cast<double>(chip->processors.size())};
        const auto freq_eff{chip->freq_eff()};
        const auto freq_max{chip->freq_max()};
        const auto freq_min{compute_freq_min(freq_max, total_util, max_util, max_nb_procs)};

        double next_active_procs{-1};
        double next_freq{0};

        if (freq_min < freq_eff) {
                next_freq = freq_eff;
                next_active_procs = std::ceil(max_nb_procs * (freq_min / freq_eff));
        }
        else {
                assert(freq_min <= chip->freq_max());
                next_freq = chip->ceil_to_mode(freq_min);
                next_active_procs = max_nb_procs;
        }

        chip->set_freq(next_freq);
        nb_active_procs = static_cast<std::size_t>(clamp(next_active_procs));

        assert(nb_active_procs >= 1 && nb_active_procs <= sim()->chip()->processors.size());
}
