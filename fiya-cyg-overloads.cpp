#include "fiya-recorder.h"

/** You will need to define this function in your code. It must return a thread_local
 *  object in multithreaded program; otherwise it can create race conditions.
 */
extern fiya::scoping_interface_t<void*> * get_recorder();

/** Once cyg profiling starts, we set this flag to true.
 *  This is becase we want to avoid recursive calls to __cyg_profile-* 
 *  functions.
 */
static thread_local bool cyg_profiling_ongoing = false;

extern "C" {

/** These two functons get called everytime you enter a function, if -finstrument-functions
 *  is passed at the compile line.
 */
void __cyg_profile_func_enter (void *, void *) __attribute__((no_instrument_function));
void __cyg_profile_func_exit (void *, void *) __attribute__((no_instrument_function));

void __cyg_profile_func_enter(void *this_fn, void *call_site) {
    if (!cyg_profiling_ongoing) {
        cyg_profiling_ongoing = true;
        fiya::scoping_interface_t<void*> * my_recorder = get_recorder();
        if (my_recorder && !my_recorder->recorder_internal_running()) {
            my_recorder->begin_scope(this_fn);
        }
        cyg_profiling_ongoing = false;
    }
}

void __cyg_profile_func_exit(void *this_fn, void *call_site) {
    if (!cyg_profiling_ongoing) {
        cyg_profiling_ongoing = true;
        fiya::scoping_interface_t<void*> * my_recorder = get_recorder();
        if (my_recorder && !my_recorder->recorder_internal_running()) {
            my_recorder->end_scope(this_fn);
        }
        cyg_profiling_ongoing = false;
    }
}

}