#pragma once

#include <chrono>
#include "fiya-recorder.h"

namespace fiya {

/** Struct containing interesting information
 *  about heap usage.
 */
struct heap_usage_t {
    /** Maximum memory allocated during
     *  the label runtime.
     */
    uint64_t peak_allocations;
    /** Total allocations, as though there
      * was no memory freeing.
      */
    uint64_t total_allocations;
    /** Current amount of the memory allocated by
     *  the label.
     */
    uint64_t current_allocations;
    /** Memory freed using operator delete, but not
     *  allocated using operator new 
     */
    uint64_t bad_deallocations;

    heap_usage_t() :
        peak_allocations(0ULL),
        total_allocations(0ULL),
        current_allocations(0ULL),
        bad_deallocations(0ULL) {}
};

/** RAII wrapper for measuring heap consumption. Bear in mind
 *  that you need to link fiya-heap-overloads.cpp as well
 *  for this code to actually measure memory consumptions.
 */
template<typename LabelType>
class measure_heap_t {
public:
    /** This RAII wrapper requires a pointer to this recorder_type */
    using recorder_type = recorder_t<LabelType, heap_usage_t>;

    /** Constructor will open the scope for the label provided to it */
    measure_heap_t(const LabelType& label, recorder_type* recorder) :
        m_recorder(recorder)
    {
        m_recorder->begin_scope(label);
    }

    /** Destructor will close the scope for the currently active label. */
    ~measure_heap_t() {
        m_recorder->end_scope();
    }
private:
    /** Pointer to the recorder. */
    recorder_type * m_recorder;
};

}