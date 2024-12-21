#include "severity.h"
#include "log_time.h"
#include "flags.h"
#include <cstdint>
#include <string>
#include <memory>
#include <thread>
#include <tuple>

namespace slog{
    class LogLine{
    public:
        LogLine(LogSeverity level, char const* file, char const* func, uint32_t line);
        ~LogLine();

        LogLine(LogLine&&) = default;
        LogLine& operator=(LogLine&&) = default;

        LogLine& operator<<(char arg);
        LogLine& operator<<(const std::string& arg);
        LogLine& operator<<(int32_t arg);
        LogLine& operator<<(int64_t arg);
        LogLine& operator<<(uint32_t arg);
        LogLine& operator<<(uint64_t arg);
        LogLine& operator<<(double arg);

        template<size_t N>
        LogLine& operator<<(const char (&arg)[N]){
            encode(arg);
            return *this;
        }

        template<typename T>
        typename std::enable_if<std::is_same<T, const char*>::value, LogLine&>::type
        operator<<(const T& arg){
            encode(arg);
            return *this;
        }   // enable if const char*

        template<typename T>
	    typename std::enable_if<std::is_same<T, char*>::value, LogLine&>::type
	    operator<<(const T& arg)
	    {
	        encode(arg);
	        return *this;
	    }

        struct string_literal_t{
            const char* s;
            explicit string_literal_t(const char* s) : s(s){}
        };

        void stream_to_string(std::ostream& s);

    private:
        size_t used_bytes;
        size_t buffer_size;
        std::unique_ptr<char[]> heap_buffer;
        char stack_buffer[256 - 2 * sizeof(size_t) - sizeof(decltype(heap_buffer)) - 8];

        typedef std::tuple<char, char*, int32_t, int64_t, uint32_t, uint64_t, double, LogLine::string_literal_t> DataTypes;
        
        char* buffer();

        template<typename T>
        void encode(T arg);
        template<typename T>
        void encode(T arg, uint8_t id);
        void encode(char* arg);
        void encode(const char* arg);
        void encode(string_literal_t arg);

        void encode_string(const char* arg, size_t len);

        void resize_buffer(size_t bytes);

        void stream_to_string(std::ostream& s, char* start, const char* const end);
    };

    struct Slog{
        bool operator+=(LogLine& logline);
    };
    
    void init(const std::string& dir, const std::string name, uint32_t roll_size);

}

namespace{
    std::thread::id this_thread_id(){
        static const thread_local std::thread::id id = std::this_thread::get_id();
        return id;
    }

    template<typename T, typename Tuple>
    struct TupleIndexHelper;

    template <typename T>
    struct TupleIndexHelper<T, std::tuple<>>{
        static constexpr const std::size_t value = 0; 
    };

    template<typename T, typename...Types>
    struct TupleIndexHelper<T, std::tuple<T, Types...>>{
        static constexpr std::size_t value = 0;
    };

    template<typename T, typename U, typename...Types>
    struct TupleIndexHelper<T, std::tuple<U, Types...>>{
        static constexpr std::size_t value = 1 + TupleIndexHelper<T, std::tuple<Types...>>::value;
    };  // recursive steps to find Type index

    bool strcasecmp(const char* s1, const char* s2) {
        while (*s1 && *s2) {
            if (tolower(*s1) != tolower(*s2)) {
                return false;
            }
            s1++;
            s2++;
        }
        return *s1 == *s2;
    }

}

#define SLOG(LEVEL) slog::Slog() += slog::LogLine(LEVEL, __FILE__, __func__, __LINE__)
#define LOG_DEBUG SLOG(slog::LogSeverity::DEBUG)
#define LOG_INFO SLOG(slog::LogSeverity::INFO)
#define LOG_ERROR SLOG(slog::LogSeverity::ERROR)
#define LOG_WARN SLOG(slog::LogSeverity::WARN)
#define LOG_FATAL SLOG(slog::LogSeverity::FATAL)

#define CHECK(condition) \
    if (!(condition)){ \
        LOG_FATAL << "CHECK failed: " << #condition; \
    }

#define CHECK_F(condition) \
    if (!(condition)){ \
        LOG_WARN << "CHECK failed: " << #condition; \
        std::abort(); \
    }

#define CHECK_EQ(a, b) \
    if ((a) != (b)){ \
        LOG_WARN<< "CHECK_EQ failed: " << #a << " != " << #b; \
    }

#define CHECK_EQ_F(a, b) \
    if ((a) != (b)){ \
        LOG_FATAL<< "CHECK_EQ failed: " << #a << " != " << #b; \
        std::abort(); \
    }

#define CHECK_STREQ(str1, str2) \
    if (strcmp(str1, str2) != 0) { \
        LOG_WARN << "CHECK_STREQ failed: \"" << str1 << "\" != \"" << str2 << "\""; \
    }

#define CHECK_STREQ_F(str1, str2) \
    if (strcmp(str1, str2) != 0) { \
        LOG_FATAL << "CHECK_STREQ failed: \"" << str1 << "\" != \"" << str2 << "\""; \
        std::abort(); \
    }

#define CHECK_STREQ_CASE(str1, str2) \
    if (!strcasecmp(str1, str2)) { \
        LOG_WARN << "CHECK_STREQ_CASE failed: \"" << str1 << "\" != \"" << str2 << "\""; \
    }

#define CHECK_STREQ_CASE_F(str1, str2) \
    if (!strcasecmp(str1, str2)) { \
        LOG_WARN << "CHECK_STREQ_CASE_F failed: \"" << str1 << "\" != \"" << str2 << "\""; \
        std::abort(); \
    }

#define CHECK_P(ptr) \
    if ((ptr) == nullptr){ \
        LOG_WARN<< "CHECK_P failed: pointer " << #ptr << " is null"; \
    }

#define CHECK_P_F(ptr) \
    if ((ptr) == nullptr){ \
        LOG_FATAL<< "CHECK_P failed: pointer " << #ptr << " is null"; \
        std::abort(); \
    }

#define CHECK_T(v, type) \
    static_assert(std::is_same_v<decltype(v), type>, "Variable " #v " is not of type " #type);

