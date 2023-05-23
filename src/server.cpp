#include "server.hpp"
#include "task.hpp"

#include <iostream>
#include <memory>

server::server(const std::weak_ptr<task> attached_task) : attached_task(attached_task){};

auto operator<<(std::ostream& out, const server& serv) -> std::ostream& {
        return out << "S" << serv.id() << " P=" << serv.period() << " U=" << serv.utilization()
                   << " D=" << serv.relative_deadline << " V=" << serv.virtual_time;
}
