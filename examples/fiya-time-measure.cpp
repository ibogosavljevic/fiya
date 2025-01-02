#include <iostream>
#include <fstream>
#include "../fiya-time-measure.h"

using namespace fiya;

/** We specialize measure_time_t using const char* labels */
using my_measure_time_t = measure_time_t<const char*>;

/** We need a recorder, once per thread. */
thread_local my_measure_time_t::recorder_type my_recorder({}, "root", time_value_t::now());

/** Convenient function wrapper  */
#define MEASURE_FUNC my_measure_time_t m(__FUNCTION__, &my_recorder)

/** We use this to busy wait in a function */
void busy_wait(int64_t v) {
    int64_t num = v * 1000000;
    while (num--) (void) rand();
}

/** A few test functions */
void func4() {
    MEASURE_FUNC;
    busy_wait(1);
}

void func3() {
    MEASURE_FUNC;
    busy_wait(10);
    func4();
    busy_wait(2);
}

void func2() {
    MEASURE_FUNC;
    busy_wait(1);
    func3();
    busy_wait(2);
    func4();
    busy_wait(5);
}

void func1() {
    MEASURE_FUNC;
    busy_wait(1);
    func2();
    busy_wait(3);
    func3();
    busy_wait(1);
}

const char * fileName = "fiya-time-measure.txt";

int main(int argc, char **argv) {
    func1();
    std::ofstream myfile(fileName);
    /** Conver the result to collapsed stack and print it in the console. */
    my_recorder.to_collapsed_stacks(
        myfile, 
        [] (std::ostream& os, const char* const & l) {
            os << l;
        },
         [] (std::ostream& os, const my_measure_time_t::measure_type& m) {
            os << std::chrono::duration_cast<std::chrono::microseconds>(m.get_duration()).count();
        });
    myfile.close();
    auto my_report = my_recorder.to_report();

    std::cout << "\n";
    /** Print also self and total reports in the console. */
    for (const auto& v: my_report.report) {
        std::cout << v.first <<
            ": self " << std::chrono::duration_cast<std::chrono::microseconds>(v.second.self.get_duration()).count() <<
            ", total " << std::chrono::duration_cast<std::chrono::microseconds>(v.second.total.get_duration()).count() << "\n";
    }
    std::cout << "Output written to " << fileName << "\n";
    std::cout << "Open site speedscope.app and drag the file there.\n";
    std::cout << "If you have Brendan Gregg's ";
}