#ifndef WIN

#define TERM_DEBUG      "\e[1;36m"
#define TERM_INFO       "\e[1;32m"
#define TERM_WARN       "\e[1;33m"
#define TERM_ERROR      "\e[1;31m"
#define TERM_FATAL      "\e[1;31m"
#define TERM_BOLD       "\e[1m"
#define TERM_RESET      "\e[0m"

#else

#define TERM_DEBUG      "\e[1;36m"
#define TERM_INFO       "\e[1;32m"
#define TERM_WARN       "\e[1;33m"
#define TERM_ERROR      "\e[1;31m"
#define TERM_FATAL      "\e[1;31m"
#define TERM_RESET      "\e[0m"

#endif