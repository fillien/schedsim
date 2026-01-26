#include <simulator/factory.hpp>

#include <simulator/allocator.hpp>
#include <simulator/allocators/counting.hpp>
#include <simulator/allocators/ff_big_first.hpp>
#include <simulator/allocators/ff_cap.hpp>
#include <simulator/allocators/ff_cap_adaptive_linear.hpp>
#include <simulator/allocators/ff_cap_adaptive_poly.hpp>
#include <simulator/allocators/ff_lb.hpp>
#include <simulator/allocators/ff_little_first.hpp>
#include <simulator/allocators/ff_sma.hpp>
#include <simulator/allocators/ff_u_cap_fitted.hpp>
#include <simulator/scheduler.hpp>
#include <simulator/schedulers/csf.hpp>
#include <simulator/schedulers/csf_timer.hpp>
#include <simulator/schedulers/ffa.hpp>
#include <simulator/schedulers/ffa_timer.hpp>
#include <simulator/schedulers/parallel.hpp>
#include <simulator/schedulers/power_aware.hpp>

#include <exception>
#include <stdexcept>
#include <unordered_set>

auto parse_allocator_args(const std::vector<std::string>& raw_args)
    -> std::unordered_map<std::string, std::string>
{
        std::unordered_map<std::string, std::string> result;
        for (const auto& arg : raw_args) {
                const auto pos = arg.find('=');
                if (pos == std::string::npos) {
                        throw std::invalid_argument(
                            "Allocator arguments must follow the key=value format");
                }

                auto key = arg.substr(0, pos);
                auto value = arg.substr(pos + 1);

                if (key.empty() || value.empty()) {
                        throw std::invalid_argument(
                            "Allocator arguments require both a non-empty key and value");
                }

                if (!result.emplace(std::move(key), std::move(value)).second) {
                        throw std::invalid_argument("Duplicate allocator argument: " + arg);
                }
        }

        return result;
}

auto select_alloc(
    const std::string& choice,
    Engine& sim,
    const std::unordered_map<std::string, std::string>& alloc_args)
    -> std::unique_ptr<allocators::Allocator>
{
        using namespace allocators;

        const auto ensure_allowed_args = [&](std::initializer_list<const char*> allowed_keys) {
                std::unordered_set<std::string> allowed;
                allowed.reserve(allowed_keys.size());
                for (const auto* key : allowed_keys) {
                        allowed.emplace(key);
                }

                for (const auto& [key, value] : alloc_args) {
                        if (allowed.find(key) == allowed.end()) {
                                throw std::invalid_argument(
                                    "Undefined allocator argument '" + key + "' for policy '" +
                                    choice + "'");
                        }
                }
        };

        if (choice == "ff_big_first") {
                ensure_allowed_args({});
                return std::make_unique<FFBigFirst>(sim);
        }
        if (choice == "counting") {
                ensure_allowed_args({});
                return std::make_unique<Counting>(sim);
        }
        if (choice == "ff_little_first") {
                ensure_allowed_args({});
                return std::make_unique<FFLittleFirst>(sim);
        }
        if (choice == "ff_cap") {
                ensure_allowed_args({});
                return std::make_unique<FFCap>(sim);
        }
        if (choice == "ff_u_cap_fitted") {
                ensure_allowed_args({});
                return std::make_unique<FFUCapFitted>(sim);
        }
        if (choice == "ff_lb") {
                ensure_allowed_args({});
                return std::make_unique<FirstFitLoadBalancer>(sim);
        }
        if (choice == "ff_sma") {
                ensure_allowed_args({"sample_rate", "num_samples"});

                double sample_rate = 0.5;
                if (const auto it = alloc_args.find("sample_rate"); it != alloc_args.end()) {
                        try {
                                sample_rate = std::stod(it->second);
                        }
                        catch (const std::exception& e) {
                                throw std::invalid_argument(
                                    "Invalid value for ff_sma sample_rate: " + it->second);
                        }
                }

                int num_samples = 5;
                if (const auto it = alloc_args.find("num_samples"); it != alloc_args.end()) {
                        try {
                                num_samples = std::stoi(it->second);
                        }
                        catch (const std::exception& e) {
                                throw std::invalid_argument(
                                    "Invalid value for ff_sma num_samples: " + it->second);
                        }
                }

                return std::make_unique<FFSma>(sim, sample_rate, num_samples);
        }
        if (choice == "ff_cap_adaptive_linear") {
                ensure_allowed_args({});
                return std::make_unique<FFCapAdaptiveLinear>(sim);
        }
        if (choice == "ff_cap_adaptive_poly") {
                ensure_allowed_args({});
                return std::make_unique<FFCapAdaptivePoly>(sim);
        }
        throw std::invalid_argument("Undefined allocation policy");
}

auto select_sched(const std::string& choice, Engine& sim)
    -> std::unique_ptr<scheds::Scheduler>
{
        using namespace scheds;
        if (choice.empty() || choice == "grub") { return std::make_unique<Parallel>(sim); }
        if (choice == "pa") { return std::make_unique<PowerAware>(sim); }
        if (choice == "ffa") { return std::make_unique<Ffa>(sim); }
        if (choice == "csf") { return std::make_unique<Csf>(sim); }
        if (choice == "ffa_timer") { return std::make_unique<FfaTimer>(sim); }
        if (choice == "csf_timer") { return std::make_unique<CsfTimer>(sim); }
        throw std::invalid_argument("Undefined scheduling policy");
}
