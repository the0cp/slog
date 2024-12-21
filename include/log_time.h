#ifndef SLOGTIME_H
#define SLOGTIME_H
#include <chrono>

namespace slogtime{
    class LogLineTime{
    public:
        LogLineTime();
        explicit LogLineTime(std::chrono::system_clock::time_point now);
        const std::chrono::system_clock::time_point& when() const noexcept{return timestamp;}
        int sec() const noexcept{return tm_.tm_sec;}
        int usec() const noexcept{return usecs.count();}
        int min() const noexcept{return tm_.tm_min;}
        int hour() const noexcept{return tm_.tm_hour;}
        int day() const noexcept{return tm_.tm_mday;}
        int month() const noexcept{return tm_.tm_mon + 1;}
        int year() const noexcept{return tm_.tm_year + 1900;}
        int dayOfWeek() const noexcept{return tm_.tm_wday;}
        int dayInYear() const noexcept{return tm_.tm_yday;}
        int dst() const noexcept{return tm_.tm_isdst;}
        std::chrono::seconds gmtoffset() const noexcept{return gmtoffset_;}
        const std::tm& tm() const noexcept{return tm_;}
    private:
        std::tm tm_{};  // time of creation of LogLine
        std::chrono::system_clock::time_point timestamp;
        std::chrono::microseconds usecs;
        std::chrono::seconds gmtoffset_;
    };
}

#endif // SLOGTIME_H