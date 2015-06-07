#include <iostream>
#include <assert.h>

#include "worker.h"

Worker::Worker(const worker_init_data init_data, struct global_state *global_state)
    : global_state(global_state), worker_data(init_data),
      total_gets(0), total_puts(0)
{
    // empty
}

Worker::~Worker()
{
    DEBUG_WORKER("Total gets for core #" << worker_data.core_id << ": "
                 << total_gets << std::endl);
    DEBUG_WORKER("Total puts for core #" << worker_data.core_id << ": "
                 << total_puts << std::endl);
}

/**
 * Listen for incoming requests by busy waiting and inspecting the queue of
 * requests for new PENDING requests to service.
 */
void Worker::listen()
{
  std::map<int, int> *old_state = NULL;
    while(!*this->worker_data.should_exit) {
        int time = this->worker_data.core_id % 10;
        sleep(10 + time / 10.0);

        // We figure most gets on the old map are done by the time
        // we're swapping out the map.
        if(old_state != NULL) {
            delete old_state;
        }

        std::map<int, int> core_local_puts;
        pthread_spin_lock(&this->worker_data.maps->puts_lock);
        core_local_puts.swap(this->worker_data.maps->local_puts);
        pthread_spin_unlock(&this->worker_data.maps->puts_lock);

        global_state->lock();
        global_state->global_data.insert(core_local_puts.begin(), core_local_puts.end());

        std::map<int, int> *new_state = new std::map<int, int>(
            global_state->global_data.begin(), global_state->global_data.end());                                                              

        // TODO: THIS IS SO SO SO UNSAFE YIKES.
        pthread_spin_lock(&this->worker_data.maps->local_state_lock);
        old_state = this->worker_data.maps->local_state;
        this->worker_data.maps->local_state = new_state;
        pthread_spin_unlock(&this->worker_data.maps->local_state_lock);

        // If we're okay with a looser persistence model, we
        // can move this oepration to after the unlock.
        persist(core_local_puts);
        global_state->unlock();
    }
}

/**
 * Persist a map of puts.
 *
 * @return 0 on success, or a negative value on failure.
 */
int Worker::persist(const std::map<int, int>& puts)
{
    for (auto kv: puts) {
        global_state->outputLog << kv.first << '\t' << kv.second << '\n';
    }
    global_state->outputLog << std::flush;

    return 0;
}
