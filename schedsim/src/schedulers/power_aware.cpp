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

namespace {
auto get_max_utilization(
    const std::vector<std::shared_ptr<server>>& servers, const double& new_utilization = 0)
    -> double
{
        if (std::distance(std::begin(servers), std::end(servers)) > 0) {
                auto u_max = std::max_element(
                    std::begin(servers),
                    std::end(servers),
                    [](const std::shared_ptr<server>& first,
                       const std::shared_ptr<server>& second) {
                            return (first->utilization() < second->utilization());
                    });
                return std::max((*u_max)->utilization(), new_utilization);
        }
        return new_utilization;
}
} // namespace

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
