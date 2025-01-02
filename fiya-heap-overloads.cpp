#include "fiya-heap-measure.h"

/** You will need to define this function in your code. It must return a thread_local
 *  object in multithreaded program; otherwise it can create race conditions.
 */
extern fiya::counter_interface_t<fiya::heap_usage_t> * get_heap_counter();

/** This is a magic number we set when allocating a memory chunk
 *  and check when deallocating it. It enables us to detect
 *  chunks that we allocated compared to the ones that could
 *  have been allocated somehow else.
 */
static constexpr uint32_t ALLOC_MAGIC_PATTERN {  0x4321cba9 };
/** We use header to write the size of the chunk we allocated. The header
 *  is located 8 bytes before the allocated chunk.
 */
static constexpr uint32_t HEADER_SIZE { 2U };

/** Overload of operator new */
void * operator new(std::size_t n) {
    static constexpr std::size_t max_uint32 { 
        static_cast<std::size_t>(std::numeric_limits<uint32_t>::max())
    };
    /** Allocate the memory for the chunk, but also for the headers */
    void * p = malloc(n + HEADER_SIZE * sizeof(uint32_t));
    if (p == nullptr) {
        throw std::bad_alloc();
    }

    uint32_t allocated_bytes { static_cast<uint32_t>(n > max_uint32 ? max_uint32 : n) };
    uint32_t * pu { reinterpret_cast<uint32_t*>(p) };
    pu[0] = ALLOC_MAGIC_PATTERN;
    pu[1] = allocated_bytes;

    fiya::counter_interface_t<fiya::heap_usage_t> * counter = get_heap_counter();
    if (counter && !counter->recorder_internal_running()) {
        fiya::heap_usage_t & hu = counter->cnt();

        hu.total_allocations += static_cast<uint64_t>(allocated_bytes);
        hu.current_allocations += static_cast<uint64_t>(allocated_bytes);
        hu.peak_allocations = std::max(hu.peak_allocations, hu.current_allocations);
    }

    return reinterpret_cast<void*>(pu + 2);
}

/** Overload of operator delete */
void operator delete(void * p)
{
    if (p == nullptr) {
        return;
    }

    uint32_t* pu { reinterpret_cast<uint32_t*>(p) };
    /** The header is 8 bytes before the memory chunk. */
    pu -= 2;

    fiya::counter_interface_t<fiya::heap_usage_t> * counter = get_heap_counter();
    fiya::heap_usage_t* hu = nullptr;

    if (counter && !counter->recorder_internal_running()) {
        hu = &counter->cnt();
    }

    if (pu[0] == ALLOC_MAGIC_PATTERN) {
        if (hu) {
            uint32_t allocation_bytes { pu[1] };
            hu->current_allocations -= static_cast<uint64_t>(allocation_bytes);
        }
        free(pu);
    } else {
        if (hu) {
            hu->bad_deallocations += 1;
        }
        free(p);
    }
}