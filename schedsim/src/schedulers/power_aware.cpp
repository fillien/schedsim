#include <cassert>
#include <schedulers/power_aware.hpp>

auto sched_power_aware::get_nb_active_procs(const double& new_utilization) const -> std::size_t
{
        return sim()->chip()->processors.size();
}

void sched_power_aware::update_platform()
{
        const auto U_MAX{get_max_utilization(servers)};
        const auto NB_PROCS{static_cast<double>(sim()->chip()->processors.size())};
        const auto TOTAL_U{get_total_utilization()};
        const auto F_MAX{sim()->chip()->freq_max()};
        const auto new_freq{(F_MAX * ((NB_PROCS - 1) * U_MAX + TOTAL_U)) / NB_PROCS};

        assert(new_freq <= sim()->chip()->freq_max());
        sim()->chip()->set_freq(new_freq);
}
