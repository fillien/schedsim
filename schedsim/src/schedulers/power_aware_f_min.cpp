#include <cassert>
#include <cmath>
#include <schedulers/power_aware_f_min.hpp>

auto compute_freq_min(
    const double& freq_max,
    const double& total_util,
    const double& max_util,
    const double& nb_procs) -> double
{
        return (freq_max * (total_util + (nb_procs - 1) * max_util)) / nb_procs;
}

auto pa_f_min::get_nb_active_procs(const double& new_utilization) const -> std::size_t
{
        return nb_active_procs;
}

void pa_f_min::update_platform()
{
        double next_active_procs{0};

        const double total_util{0};
        const double max_util{0};
        const double max_nb_procs{static_cast<double>(sim()->chip()->processors.size())};
        const auto freq_eff{sim()->chip()->freq_eff()};
        const auto freq_max{sim()->chip()->freq_max()};

        const auto freq_min{compute_freq_min(freq_max, total_util, max_util, max_nb_procs)};

        if (freq_min < freq_eff) {
                sim()->chip()->set_freq(freq_eff);
                next_active_procs = std::ceil(max_nb_procs * (freq_min / freq_eff));
        }
        else {
                assert(freq_min <= sim()->chip()->freq_max());
                sim()->chip()->set_freq(sim()->chip()->ceil_to_mode(freq_min));
                next_active_procs = max_nb_procs;
        }

        nb_active_procs = static_cast<std::size_t>(clamp(next_active_procs));

        assert(nb_active_procs >= 1 && nb_active_procs <= sim()->chip()->processors.size());
}
