#pragma once

#include <chrono>
#include <functional>

#include "fiya-recorder.h"

#if defined (__unix__) || (defined (__APPLE__) && defined (__MACH__))
#include <unistd.h>
#include <time.h>

#ifdef _POSIX_THREAD_CPUTIME
#define FIYA_USE_POSIX_TIME
#endif

#endif

#if defined(_WIN32)
#include <windows.h>
#define FIYA_USE_WINDOWS_TIME
#endif

namespace fiya {

#ifdef FIYA_USE_POSIX_TIME

template <typename T>
std::chrono::time_point<std::chrono::high_resolution_clock> get_thread_time() {
    struct timespec ts;
    if (clock_gettime(CLOCK_THREAD_CPUTIME_ID, &ts) == 0) {
        return std::chrono::time_point<std::chrono::high_resolution_clock>{} + 
            std::chrono::duration<decltype(ts.tv_nsec), std::nano>(ts.tv_nsec) +
            std::chrono::duration<decltype(ts.tv_sec)>(ts.tv_sec);
    } else {
        return std::chrono::time_point<std::chrono::high_resolution_clock>{};
    }
}

#elif defined(FIYA_USE_WINDOWS_TIME)

template <typename T>
std::chrono::time_point<std::chrono::high_resolution_clock> get_thread_time() {
    FILETIME creationTime, exitTime, kernelTime, userTime;
    
    if (GetThreadTimes(GetCurrentThread(), &creationTime, &exitTime, &kernelTime, &userTime)) {
        ULARGE_INTEGER kt, ut;
        kt.LowPart = kernelTime.dwLowDateTime;
        kt.HighPart = kernelTime.dwHighDateTime;

        ut.LowPart = userTime.dwLowDateTime;
        ut.HighPart = userTime.dwHighDateTime;

        return std::chrono::time_point<std::chrono::high_resolution_clock>{} + 
        std::chrono::duration<uint64_t, std::ratio<1, 10000000> >(static_cast<uint64_t>(kt.QuadPart )) +
        std::chrono::duration<uint64_t, std::ratio<1, 10000000> >(static_cast<uint64_t>(ut.QuadPart ));
    } else {
        return std::chrono::time_point<std::chrono::high_resolution_clock>{};
    }
}

#else
#error Missing get_thread_time() function for this platform
#endif

class time_value_t {
public:
    time_value_t() = default;

    std::chrono::high_resolution_clock::duration get_duration() const {
        return m_duration;
    }

    operator std::chrono::high_resolution_clock::duration() const {
        return m_duration;
    }

    static time_value_t now() {
        time_value_t result;
        result.m_start = get_thread_time<void>();
        result.m_duration = decltype(result.m_duration){0};
        return result;
    }

    time_value_t operator+(const time_value_t& other) const {
        time_value_t result;
        result.m_start = m_start;
        result.m_duration = m_duration + other.m_duration;
        return result;
    }
private:
    std::chrono::high_resolution_clock::duration m_duration;
    
    std::chrono::time_point<std::chrono::high_resolution_clock> m_start;

    template<typename T>
    friend class measure_time_t;
    
    template<typename T>
    friend class cyg_measure_time_t;
};

template <typename LabelType>
class measure_time_t {
public:
    using measure_type = time_value_t;
    using recorder_type = recorder_t<LabelType, measure_type>;

    measure_time_t(const LabelType& label, recorder_type * recorder):
        m_recorder(recorder)
    {
        m_recorder->cnt().m_duration += get_thread_time<void>() - m_recorder->cnt().m_start;
        m_recorder->begin_scope(label);
        m_recorder->cnt().m_start = get_thread_time<void>();

    }

    ~measure_time_t() {
        m_recorder->cnt().m_duration += get_thread_time<void>() - m_recorder->cnt().m_start;

        m_recorder->end_scope();
        m_recorder->cnt().m_start = get_thread_time<void>();
    }
private:
    recorder_type * m_recorder;
};

template <typename LabelType>
class cyg_measure_time_t: public scoping_interface_t<LabelType> {
public:
    using measure_type = time_value_t;
    using recorder_type = recorder_t<LabelType, measure_type>;

    static const time_value_t zero;

    cyg_measure_time_t(recorder_type* recorder) : 
        m_recorder(recorder)
    { }

    void begin_scope(const LabelType& label) override {
        m_recorder->cnt().m_duration += get_thread_time<void>() - m_recorder->cnt().m_start;
        m_recorder->begin_scope(label);
        m_recorder->cnt().m_start = get_thread_time<void>();
    }
    
    void end_scope() override {
        end_scope_local();
    }

    void end_scope(const LabelType& label) override {
        end_scope_local(label);
    }

    bool recorder_internal_running() const override {
        return m_recorder->recorder_internal_running();
    }
private:
    template<typename... Args>
    void end_scope_local(Args... args) {
        m_recorder->cnt().m_duration += get_thread_time<void>() - m_recorder->cnt().m_start;
        m_recorder->end_scope(args...);
        m_recorder->cnt().m_start = get_thread_time<void>();
    }

    recorder_type * m_recorder;
};

template <typename LabelType>
const time_value_t cyg_measure_time_t<LabelType>::zero = {};


}

