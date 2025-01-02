#include <iostream>
#include <fstream>
#include "../fiya-heap-measure.h"

using namespace fiya;

/** Type used for labels. */
enum class function_e {
    root,
    func1,
    func2,
    func3,
    func4
};

/** Customize the measure_heap+t to use function_e as label. */
using my_measure_heap_t = measure_heap_t<function_e>;

/** Instance of my recorder. Each thread gets it's own recorder. */
thread_local my_measure_heap_t::recorder_type my_recorder(heap_usage_t{}, function_e::root, heap_usage_t{});

/** We need to export this function for fiya-heap-overloads.cpp */
fiya::counter_interface_t<fiya::heap_usage_t> * get_heap_counter() {
    return &my_recorder;
}

/** Test vector, used for allocating memory. */
std::vector<int*> test_vector;

void alloc_dealloc(std::size_t n) {
    for (size_t i = 0; i < n; i++) {
        test_vector.push_back(new int);
    }
}

/** Test functions */
void func4() {
    my_measure_heap_t m(function_e::func4, &my_recorder);
    alloc_dealloc(1);
}

void func3() {
    my_measure_heap_t m(function_e::func3, &my_recorder);
    alloc_dealloc(10);
    func4();
    alloc_dealloc(2);
}

void func2() {
    my_measure_heap_t m(function_e::func2, &my_recorder);
    alloc_dealloc(1);
    func3();
    alloc_dealloc(2);
    func4();
    alloc_dealloc(5);
}

void func1() {
    my_measure_heap_t m(function_e::func1, &my_recorder);
    alloc_dealloc(1);
    func2();
    alloc_dealloc(3);
    func3();
    alloc_dealloc(1);
}

const char * fileName = "fiya-heap-measure.txt";

int main(int argc, char **argv) {
    func1();

    std::ofstream myfile(fileName);
    /** Convert the result to collapsed stack and print it in the console. */
    my_recorder.to_collapsed_stacks(
        myfile, 
        [] (std::ostream& os, const function_e& f) {
            switch(f) {
                case function_e::root: os << "root"; break;
                case function_e::func1: os << "func1"; break;
                case function_e::func2: os << "func2"; break;
                case function_e::func3: os << "func3"; break;
                case function_e::func4: os << "func4"; break;
            };
        },
         [] (std::ostream& os, const heap_usage_t& m) {
            os << m.peak_allocations;
        });
    myfile.close();
}