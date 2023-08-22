#include "processor.hpp"
#include "task.hpp"

processor::processor(const std::weak_ptr<engine> sim, const int id) : entity(sim), id(id){};

void processor::dequeue(std::weak_ptr<task> task_to_remove) {
        assert(!task_to_remove.expired());
        for (auto itr = runqueue.begin(); itr != runqueue.end(); ++itr) {
                if ((*itr).lock()->id == task_to_remove.lock()->id) {
                        runqueue.erase(itr);
                        break;
                }
        }
}
