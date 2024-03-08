#include "power_aware.hpp"
#include "../engine.hpp"
#include "../event.hpp"
#include "../platform.hpp"
#include "../processor.hpp"
#include "../server.hpp"

#include <algorithm>
#include <bits/ranges_algo.h>
#include <bits/ranges_base.h>
#include <bits/ranges_util.h>
#include <cassert>
#include <cmath>
#include <iostream>
#include <iterator>
#include <memory>
#include <numeric>
#include <ostream>
#include <ranges>
#include <vector>

auto sched_power_aware::get_nb_active_procs(const double& new_utilization) const -> std::size_t
{
        return sim()->get_platform()->processors.size();
}

void sched_power_aware::on_active_utilization_updated()
{
        const auto U_MAX{get_max_utilization(servers)};
        const auto NB_PROCS{static_cast<double>(sim()->get_platform()->processors.size())};
        const auto SPEED = (get_active_bandwidth() + ((NB_PROCS - 1) * U_MAX)) / NB_PROCS;

        assert(SPEED <= 1);

        sim()->get_platform()->set_freq(SPEED);
}
