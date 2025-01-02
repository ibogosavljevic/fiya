#include <iostream>
#include <dlfcn.h>
#include <cxxabi.h>
#include <fstream>

#include "../fiya-time-measure.h"

using namespace fiya;

int main(int argc, char **argv);

/** My recorder type. For label we use function address.
 *  which is void* and forwarded to __cyg_profile_func_enter
 *  and __cyg_profile_func_exit functions.
 */
thread_local cyg_measure_time_t<void*>::recorder_type my_recorder({}, nullptr, time_value_t::now());

/** Helper class for the cyg profiling. */
thread_local cyg_measure_time_t<void*> my_measure_time(&my_recorder);

/** Provided for functions in fiya-cyg-overloads.cpp */
extern fiya::scoping_interface_t<void*> * get_recorder() {
    return &my_measure_time;
}

void busy_wait(int64_t v) {
    int64_t num = v * 1000000;
    while (num--) (void) rand();
}

/** 
 *  Test functions. Notice the lack of any
 *  marker API. The compiler automaticallly inserts
 *  call to __cyg_profile_func_enter on entering
 *  function and __cyg_profile_func_exit when
 *  leaving.
 */
void func4() {
    busy_wait(1);
}

void func3() {
    busy_wait(10);
    func4();
    busy_wait(2);
}

void func2() {
    busy_wait(1);
    func3();
    busy_wait(2);
    func4();
    busy_wait(5);
}

void func1() {
    busy_wait(1);
    func2();
    busy_wait(3);
    func3();
    busy_wait(1);
}

const char * fileName = "fiya-cyg-time-measure.txt";

int main(int argc, char **argv) {
    func1();

    std::ofstream myfile(fileName);
    /** Print collapsed stack */
    my_recorder.to_collapsed_stacks(
        myfile, 
        [] (std::ostream& os, void* const & l) {
            Dl_info info;
            int status;
            /** Looking up function name instead of pointer. */
            if (dladdr(l, &info)) {
                if (info.dli_sname) {
                    char * demangled_name = abi::__cxa_demangle((info.dli_sname ? info.dli_sname: "null"), NULL, NULL, &status);
                    if (demangled_name) {
                        os << demangled_name;
                        free(demangled_name);
                    } else {
                        os << info.dli_sname;
                    }
                } else {
                    os << "unknown2";
                }
            } else {
                os << "unknown1";
            }
        },
         [] (std::ostream& os, const time_value_t& m) {
            os << std::chrono::duration_cast<std::chrono::microseconds>(m.get_duration()).count();
        }
    );
    myfile.close();
    std::cout << "Output written to " << fileName << "\n";
    std::cout << "Open site speedscope.app and drag the file there.\n";
    std::cout << "If you have Brendan Gregg's ";
}