# FIYA - Flamegraphs in Your App

## Introduction

Flamegraphs are very useful visualization tool used to analyze performance bottlenecks in software programs, typically focusing on CPU usage or time spent in functions. Here is a typical flamegraph produced by running FFMPEG through speedscope.app:

![alt text](https://johnnysswlab.com/wp-content/uploads/speedscope-timeorder-zoomin.png)

If we look at this image, we see the following
* Each rectangle represents a function: The width of the rectangle shows how much time that function consumed in relation to the entire program's runtime.
* Stacks represent the call hierarchy: Functions that call other functions stack on top of each other. The top-most bar is your main function or starting point.

Apart from flamgraphs capturing time spent in functions, which is the most common, there is a whole other category of flamegraphs depicting memory allocation, interrupts, time spent in system mode, branch prediction misses etc. Flamegraphs allow users to very easily spot places in their code consuming a lot of certain type of resource.

Creating flamegraphs is usually done using a profiler. The profiler collects the statistics about program execution, the flamegraph tools generate flamegraphs from this input.

The main issue with flamegraphs generated in this way:
* Need to use a profiler.
* Need to have your binary compiled with debug symbols.
* Difficult to use in production environment or on embedded system where tooling support is limited (e.g. QNX OS doesn't have a good profiler).
* Too much detail: maybe you want to show information not per function, but per component.
* Overhead of profiling.

## Description
FIYA, shortened for _Flamegraphs In Your App_ is a light-weight, mostly header-only library
that allows you to programatically generate flamegraphs from your application.
You programatically set where the scope of the _label_ starts and where it ends.
It can be the beginning and the end of a function, but also the beginning or the end of a
component.

To explain FIYA in more detail, let's assume you want to do the following:
* You have several components in your program, whose names are stored in `enum component { component1, component2, ... }`.
* You want to measure runtime for each component. The runtime is measured as `std::chrono::high_resolution_clock::duration` type.
* You want to represent your program runtime as a flamegraph. In the flamegraph, you want to
  represent components. You also want to record component nesting.

You can achieve all this using FIYA class `recorder_t`. The class requires two template arguments, one is called `LabelType` which is in our case `enum component { ... }` and the other is called `MeasureType`, which is in our case:

```cpp
struct time_value_t {
    std::chrono::high_resolution_clock::duration m_duration;
    std::chrono::time_point<std::chrono::high_resolution_clock> m_start;
 }
```

The reason we need two members in `time_value_t` is because, when the component starts, we need to save the current clock value to `m_start`, and when the component ends we need to save the measured time to `m_duration`.

The `recorder_t` class offers three important methods:
* `recorder_t::begin_scope(component c)` to let the recorder know we are entering the scope of a new label. On the flamegraph this corresponds to entering a new function from the context of current function.
* `recorder_t::end_scope()` to let the recorder know we are exiting the scope of the current label and that it needs to restore the caller label. On the flamegraph this corrensponds leaving the current function to its caller.
* `recorder_t::cnt()` returns the counter for the current component in the current stack.

So, if we want to measure the time using FIYA, when entering a new component:
* We save the spent time for the current component: `my_recorder.cnt().m_duration += get_thread_time() - my_recorder.cnt().m_start;`.
* We start the new component `m_recorder.begin_scope(new_component);`.
* We save the current clock time for the new component `my_recorder.cnt().m_start = get_thread_time();`.

When exiting a component:
* We save the spent time for the current component: `my_recorder.cnt().m_duration += get_thread_time() - m_recorder.cnt().m_start;`.
* We exit the current component: `my_recorder.end_scope()`.
* We set the current clock time for the old component with `m_recorder.cnt().m_start = get_thread_time();`

### Generating report

For generating reports, FIYA offers two functions.

#### Collapsed stack
The function signature looks like this:

```cpp
     void to_collapsed_stacks(
        std::ostream& os,
        const std::function<void(std::ostream& os, const LabelType& l)> & l_out,
        const std::function<void(std::ostream& os, const MeasureType& m)> & m_out)
```
  This function generates the collapsed_stack that outputs to `ostream os`.
  The first argument `l_out` is a function pointer to print the `LabelType`
  to the output stream, and the second argument `m_out` is a function pointer
  to print the `MeasureType` to the output stream.
  If any of the types overloaded `operator<<`, you don't have to provide
  the corresponding function pointer.

  This report is in the `collapsed_stack` format, a format understood by both speedscope.app and Brendan Gregg's tool.

  The function is typically used like this:

  ```cpp
std::ofstream my_file("my_file.txt");
my_recorder.to_collapsed_stacks(
    my_file, 
    [] (std::ostream& os, component const & c) {
        os << to_string(c);
   },
    [] (std::ostream& os, const time_value_t& m) {
      os << std::chrono::duration_cast<std::chrono::microseconds>(m.m_duration.count());
    });
my_file.close()
```

The above function produces a report and saves it to file `my_file.txt`.
* For speedscope.app, open the site https://www.speedscope.app/ and drag the produced file to visualize it.
  Then click the button `Left heavy`.
* For Brendan Gregg's tools, download and install the tools according the instructions
  from his site, then run `./flamegraph.pl my_file.txt > my_file.svg` and open
  the produced `.svg` file in a web browser (the image is interactive).

#### Self/total report

* The second report function has a following signature:
```cpp
    template<typename Operation>
    report_t<LabelType, MeasureType> to_report(const Operation& accumulate_op);
```
  and the `report_t` type returned by the function looks like this:

```cpp
template<typename LabelType, typename MeasureType>
class report_t {
public:
    struct report_entry_t {
        MeasureType self;
        MeasureType total;
    };

    std::unordered_map<LabelType, report_entry_t> report;
};
```
  For each value of label, the report contains:
* `self` value is the value recorded while the label was active excluding the value recorded by all the other nested labels.
* `total` value is the value recorder while the label was active including all the nested labels that were started while this label was active.

The example report looks like this:

```
Label    Self    Total
root       33   562229
func1   76199   562196
func2  112081   308637
func3  332985   360274
func4   40929    40929
```

## Features
* Mostly header-only, lightweight.
* Allows recording of events where a counter is read when a label scope of starts and 
  ends - time is the most important of such event.
* Allows recording og asynchronous events, i.e. events that happen at any time while the
  program is running - memory allocations being the example of such events.
* Special overload for labels that are `const char*`, so you can use `__FUNCTION__` macro as a label.
  This overload allows proper handling of `const char *` strings, including comparison of strings
  as well as keeping the same string only once.
* Predefined templates for measuring time in `fiya-measure-time.h`
* Predefined templates for measure heap usage in `fiya-measure-heap.h`.
* Predefined templates for measuring time usage using GCC's and CLANG's
  instrumentation framework (`-finstrument-functions`) in `fiya-measure-time.h`.

### Multithreading
The recorder is not thread-safe, but it can be used in multithreaded environement.
For each thread you will need to create a separate recorder instance, and 
when you want to create collapsed stack for all threads in the program,
you will need to write all the collapsed stack from each individual
recorder into a single file. The flamegraph visualization toolkit will take care of correct representation.

## Requirements
* C++ 11
* Windows or any POSIX operating system (Linux).
* Tested with GCC, CLANG and MSVC. Not tested with Mac, but will probably work there.

## Installation
Copy the needed headers into your project:
* `fiya_recorder.h` and `fiya-string-db.h` are mandatory.
* In addition:
  * Predefined templates for measuring time:
    * Include also `fiya-time-measure.h`
    * See `examples/fiya-time-measure.cpp` on how to measure time.
  * Predefined templates for measure heap usage:
    * Include also `fiya-heap-measure.h`.
    * Compile and link `fiya-heap-overloads.cpp` in your code base.
      The functions there overload `operator new` and `operator delete`.
    * See `examples/fiya-heap-measure.cpp`
  * Predefined templates for measuring time usage using GCC's and CLANG's
    instrumentation framework
    * Include `fiya-time-measure.h`
    * See `examples/fiya-cyg-time-measure.cpp` on how to measure time.


## Usage

To use FIYA:
* Include `fiya-recorder.h` and `fiya-string-db.h` in your project for all of the
  bellow scenarios.

### Measuring time with the predefined template
* Include `fiya-time-measure.h`.
* In this header, there is a class called `measure_time_t<LabelType>`. You will 
  need to specialize this class with the label type you wish to use. Typically
  a label type can be an `int`, `enum`, `const char*`. Don't use `std::string`, it will work but with a lot of duplicates. Use `const char*` or `std::string_view` instead.
* You will need an instance of recorder. The class `measure_time_t<LabelType>` requires 
  a specific type of recorder and this type is declared in `measure_time_t::recorder_type`.
* The class `measure_time_t` is RAII, it will call `recorder_t::begin_scope` when
  created and `recorder_t::end_scope` when destroyed. Therefore:

```cpp
/** We specialize measure_time_t using const char* labels */
using my_measure_time_t = measure_time_t<const char*>;

/** We need a recorder, one instance per thread. */
thread_local my_measure_time_t::recorder_type my_recorder({}, "root", time_value_t::now());

void my_code() {
  /** Entering scope named "my_code" */
  my_measure_time_t m(__FUNCTION__, &my_recorder);
  
  ...

  /** Scope of "my_code" ends here, when variable m is destroyed */
}

```
### Measuring heap usage with the predefined template

The full example is given in `examples/fiya-time-measure.cpp`
* Include also `fiya-heap-measure.h` and `fiya-heap-overloads.cpp`
* In the header `fiya-heap-measure.h`, there is a class called 
  `measure_heap_t<LabelType>`. You will  need to specialize this class with the label type you wish to use. Typically  a label type can be an `int`, `enum`, `const char*`. Don't use `std::string`, it will work but with a lot of duplicates. Use `const char*` or `std::string_view` instead.
* You will need an instance of recorder. The class `measure_heap_t<LabelType>` 
  requires a specific type of recorder and this type is declared in `measure_heap_t::recorder_type`.
* The `MeasureType` for `measure_heap_t` is 
```cpp
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
  }
```
* The class `measure_heap_t` is RAII, it will call `recorder_t::begin_scope` when
  created and `recorder_t::end_scope` when destroyed. Therefore:

```cpp
/** Type used for labels. */
enum class function_e {
    root,
    func1,
    func2,
    func3,
    func4
};

/** Customize the measure_heap_t to use function_e as label. */
using my_measure_heap_t = measure_heap_t<function_e>;

/** Instance of my recorder. Each thread gets it's own recorder. */
thread_local my_measure_heap_t::recorder_type my_recorder(
  heap_usage_t{}, function_e::root, heap_usage_t{});

/** We need to export this function for fiya-heap-overloads.cpp */
fiya::counter_interface_t<fiya::heap_usage_t> * get_heap_counter() {
    return &my_recorder;
}
```

## Contributing
To contribute
* Fork the repository
* Create a feature branch (`git checkout -b feature-name`)
* Commit your changes (`git commit -m 'Add feature'`)
* Push to the branch (`git push origin feature-name`)
* Open a Pull Request

## License
This project is licensed under the MIT License - see the LICENSE file for details.
