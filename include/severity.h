#ifndef BASE_LOG_SEVERITY_H__
#define BASE_LOG_SEVERITY_H__
#include <cstdint>

namespace slog{
    enum class LogSeverity : uint8_t { 
        DEBUG, 
        INFO, 
        ERROR, 
        WARN, 
        FATAL 
    };
}

#endif