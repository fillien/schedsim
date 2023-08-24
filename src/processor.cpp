#include "processor.hpp"
#include "engine.hpp"
#include "task.hpp"
#include <cassert>
#include <iostream>

processor::processor(const std::weak_ptr<engine> sim, const int id) : entity(sim), id(id){};

void processor::dequeue(std::weak_ptr<task> task_to_dequeue) {
        assert(!task_to_dequeue.expired());
        for (auto itr = runqueue.begin(); itr != runqueue.end(); ++itr) {
                if ((*itr).lock()->id == task_to_dequeue.lock()->id) {
                        runqueue.erase(itr);
                        break;
                }
        }
        if (runqueue.empty()) {
                change_state(state::idle);
        }
}

void processor::enqueue(std::weak_ptr<task> task_to_enqueue) {
        assert(!task_to_enqueue.expired());
        if (runqueue.empty()) {
                change_state(state::running);
        }
        runqueue.push_back(task_to_enqueue.lock());
}

void processor::change_state(state next_state) {
        assert(next_state != current_state);

        switch (next_state) {
        case state::idle: {
                current_state = state::idle;
                sim()->logging_system.add_trace(
                    {sim()->current_timestamp, types::PROC_IDLED, id, 0});
                break;
        }
        case state::running: {
                current_state = state::running;
                sim()->logging_system.add_trace(
                    {sim()->current_timestamp, types::PROC_ACTIVATED, id, 0});
                break;
        }
        }
}
