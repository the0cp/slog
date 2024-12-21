#include "include/slog.h"
#include "include/colors.h"
#include <string.h>
#include <queue>
#include <iostream>
#include <fstream>
#include <filesystem>
#include <typeinfo>
#include <chrono>
#include <ctime>
#include <atomic>

namespace slogtime{
    LogLineTime::LogLineTime() : LogLineTime(std::chrono::system_clock::now()) {}

    LogLineTime::LogLineTime(std::chrono::system_clock::time_point now) : timestamp(now) {
        time_t tt = std::chrono::system_clock::to_time_t(now);
        gmtoffset_ = std::chrono::seconds(std::localtime(&tt)->tm_gmtoff);
        std::tm* ptm = std::localtime(&tt);
        tm_ = *ptm;
        usecs = std::chrono::duration_cast<std::chrono::microseconds>(now - std::chrono::system_clock::from_time_t(tt));
    }
}

namespace slog{
    const char* level_to_string(LogSeverity level){
        switch(level){
            case LogSeverity::DEBUG:
                return "Debug";
            case LogSeverity::INFO:
                return "Info";
            case LogSeverity::WARN:
                return "Warning";
            case LogSeverity::ERROR:
                return "Error";
            case LogSeverity::FATAL:
                return "Fatal";
            default:
                return "Debug";
        }
        return "Unknown";
    }

    const char* color_level(LogSeverity level){
        switch(level){
            case LogSeverity::DEBUG:
                return TERM_DEBUG;
            case LogSeverity::INFO:
                return TERM_INFO;
            case LogSeverity::WARN:
                return TERM_WARN;
            case LogSeverity::ERROR:
                return TERM_ERROR;
            case LogSeverity::FATAL:
                return TERM_FATAL;
            default:
                return TERM_DEBUG;
        }
        return "";
    }

    char* LogLine::buffer(){
        return !heap_buffer ? &stack_buffer[used_bytes] : &(heap_buffer.get())[used_bytes];
    }

    void LogLine::resize_buffer(size_t bytes){
        const size_t new_size = used_bytes + bytes;
        if(new_size <= buffer_size) return;
        if(!heap_buffer){
            buffer_size = std::max(static_cast<size_t>(512), new_size);
            heap_buffer.reset(new char[buffer_size]);
            memcpy(heap_buffer.get(), stack_buffer, used_bytes);
        }else{
            buffer_size = std::max(static_cast<size_t>(2 * buffer_size), new_size);
            std::unique_ptr<char[]>new_heap_buffer(new char[buffer_size]);
            memcpy(new_heap_buffer.get(), heap_buffer.get(), used_bytes);
            heap_buffer.swap(new_heap_buffer);
        }
    }

    void LogLine::encode_string(const char* arg, size_t len){
        if(len == 0)    return;
        resize_buffer(len + 2);
        char* b = buffer();
        auto id = TupleIndexHelper<char*, DataTypes>::value;
        *reinterpret_cast<uint8_t*>(b++) = static_cast<uint8_t>(id);
        memcpy(b, arg, len + 1);
        used_bytes += len + 2;
    }

    #if (ENABLE_CONSOLE_OUT == false)
    template<typename T>
    char* decode(std::ostream& s, char* data, T* dummy){
        T arg = *reinterpret_cast<T*>(data);
        s << arg;
        return data + sizeof(T);
    }

    template<>
    char* decode(std::ostream& s, char* data, LogLine::string_literal_t* dummy){
        LogLine::string_literal_t sliteral = *reinterpret_cast<LogLine::string_literal_t*>(data);
        s << sliteral.s;
        return data + sizeof(LogLine::string_literal_t);
    }

    template<>
    char* decode(std::ostream& s, char* data, char** dummy){
        while(*data != '\0'){
            s << *data;
            ++data;
        }
        return ++data;
    }
    #endif

    #if (ENABLE_CONSOLE_OUT == true)
    template<typename T>
    char* decode(std::ostream& s, char* data, T dummy){
        T arg = *reinterpret_cast<T*>(data);
        s << arg;
        std::cout << arg;
        return data + sizeof(T);
    }

    template<>
    char* decode(std::ostream& s, char* data, LogLine::string_literal_t* dummy){
        LogLine::string_literal_t sliteral = *reinterpret_cast<LogLine::string_literal_t*>(data);
        s << sliteral.s;
        std::cout << sliteral.s;
        return data + sizeof(LogLine::string_literal_t);
    }

    template<>
    char* decode(std::ostream& s, char* data, char** dummy){
        while(*data != '\0'){
            s << *data;
            std::cout << *data;
            ++data;
        }
        data++;
        return data;
    }
    #endif

    template<typename T>
    void LogLine::encode(T arg){
        *reinterpret_cast<T*>(buffer()) = arg;
        used_bytes += sizeof(T);
    }

    template<typename T>
    void LogLine::encode(T arg, uint8_t id){
        resize_buffer(sizeof(T) + sizeof(uint8_t));
        encode<uint8_t>(id);
        encode<T>(arg);
    }

    void LogLine::encode(const char* arg){
        if(arg != nullptr)  encode_string(arg, strlen(arg));
    }

    void LogLine::encode(char* arg){
        if(arg != nullptr)  encode_string(arg, strlen(arg));
    }

    void LogLine::encode(string_literal_t arg){
        encode<string_literal_t>(arg, TupleIndexHelper<string_literal_t, DataTypes>::value);
    }

    LogLine::LogLine(LogSeverity level, const char* file, const char* func, uint32_t line)
        : used_bytes(0), buffer_size(sizeof(stack_buffer)){
        /* time, thread, file, func, line, level*/
        slogtime::LogLineTime now;
        encode<slogtime::LogLineTime>(now);
        encode<std::thread::id>(this_thread_id());
        encode<string_literal_t>(string_literal_t(file));
        encode<string_literal_t>(string_literal_t(func));
        encode<uint32_t>(line);
        encode<LogSeverity>(level);
    }

    LogLine::~LogLine() = default;

    void LogLine::stream_to_string(std::ostream& s){
        char* data = !heap_buffer ? stack_buffer : heap_buffer.get();
        const char* const end = data + used_bytes;

        slogtime::LogLineTime timenow = *reinterpret_cast<slogtime::LogLineTime*>(data);
        data += sizeof(slogtime::LogLineTime);
        
        std::thread::id threadid = *reinterpret_cast<std::thread::id*>(data);
        data += sizeof(std::thread::id);

        string_literal_t file = *reinterpret_cast<string_literal_t*>(data);
        data += sizeof(string_literal_t);

        string_literal_t function = *reinterpret_cast<string_literal_t*>(data);
        data += sizeof(string_literal_t);

        uint32_t line = *reinterpret_cast<uint32_t*>(data); 
        data += sizeof(uint32_t);

        LogSeverity loglevel = *reinterpret_cast<LogSeverity*>(data);
        data += sizeof(LogSeverity);

        
        
        s << '[' << timenow.year() << '-' \
                 << timenow.month() << '-' \
                 << timenow.day() << '-' \
                 << timenow.hour() << timenow.min() << timenow.sec();
        if(FLAG_DEFAULT_WITH_MILLISEC) s << '-' << timenow.usec();
        if(FLAG_GMT_OFFSET) s << '+' << timenow.gmtoffset().count();
        if(FLAG_IS_DST) s << "-DST" << timenow.dst();
        

       
        s << '[' << level_to_string(loglevel) << ']'
        << '[' << threadid << ']'
        << '[' << file.s
        << ':' << function.s
        << ':' << line << "] ";

        if(ENABLE_CONSOLE_OUT){
            std::cout << '[' << timenow.year() << '-' \
                     << timenow.month() << '-' \
                     << timenow.day() << '-' \
                     << timenow.hour() << timenow.min() << timenow.sec();
            if(FLAG_DEFAULT_WITH_MILLISEC) std::cout << '-' << timenow.usec();
            if(FLAG_GMT_OFFSET) std::cout << '+' << timenow.gmtoffset().count();
            if(FLAG_IS_DST) std::cout << "-DST" << timenow.dst();
            std::cout << ']';



            std::cout << '[' << color_level(loglevel) << level_to_string(loglevel) << TERM_RESET << ']'
            << '[' << threadid << ']'
            << TERM_BOLD
            << file.s
            << ':' << function.s
            << ':' << line
            << ": " << TERM_RESET;
        }

        stream_to_string(s, data, end);

        s << std::endl;
        if(ENABLE_CONSOLE_OUT) std::cout << std::endl;

        if (loglevel >= LogSeverity::FATAL) {
            s.flush();
        }      
    }

    void LogLine::stream_to_string(std::ostream& s, char* start, const char* const end){
        if(start == end)    return;
        int id = static_cast<int>(*start);
        start++;
        
        switch(id){
            case 0:
                stream_to_string(s, decode(s, start, static_cast<std::tuple_element<0, DataTypes>::type*>(nullptr)), end);
                return;
            case 1:
                stream_to_string(s, decode(s, start, static_cast<std::tuple_element<1, DataTypes>::type*>(nullptr)), end);
                return;
            case 2:
                stream_to_string(s, decode(s, start, static_cast<std::tuple_element<2, DataTypes>::type*>(nullptr)), end);
                return;
            case 3:
                stream_to_string(s, decode(s, start, static_cast<std::tuple_element<3, DataTypes>::type*>(nullptr)), end);
                return;
            case 4:
                stream_to_string(s, decode(s, start, static_cast<std::tuple_element<4, DataTypes>::type*>(nullptr)), end);
                return;
            case 5:
                stream_to_string(s, decode(s, start, static_cast<std::tuple_element<5, DataTypes>::type*>(nullptr)), end);
                return;
            case 6:
                stream_to_string(s, decode(s, start, static_cast<std::tuple_element<6, DataTypes>::type*>(nullptr)), end);
                return;
            case 7:
                stream_to_string(s, decode(s, start, static_cast<std::tuple_element<7, DataTypes>::type*>(nullptr)), end);
                return;
        }
    }    

    /* operators reload */
    LogLine& LogLine::operator<<(char arg){
        encode<char>(arg, TupleIndexHelper<char, DataTypes>::value);
        return *this;
    }

    LogLine& LogLine::operator<<(const std::string& arg){
        encode_string(arg.c_str(), arg.length());
        return *this;
    }

    LogLine& LogLine::operator<<(int32_t arg){
        encode<int32_t>(arg, TupleIndexHelper<int32_t, DataTypes>::value);
        return *this;
    }

    LogLine& LogLine::operator<<(int64_t arg){
        encode<int64_t>(arg, TupleIndexHelper<int64_t, DataTypes>::value);
        return *this;
    }

    LogLine& LogLine::operator<<(uint32_t arg){
        encode<uint32_t>(arg, TupleIndexHelper<uint32_t, DataTypes>::value);
        return *this;
    }

    LogLine& LogLine::operator<<(uint64_t arg){
        encode<uint64_t>(arg, TupleIndexHelper<uint64_t, DataTypes>::value);
        return *this;
    }

    LogLine& LogLine::operator<<(double arg){
        encode<double>(arg, TupleIndexHelper<double, DataTypes>::value);
        return *this;
    }

    class SpinLock{
    public:
        SpinLock(std::atomic_flag& flag) : flag(flag){
            while(flag.test_and_set(std::memory_order_acquire));
        }

        ~SpinLock(){
            flag.clear(std::memory_order_release);
        }

    private:
        std::atomic_flag& flag;
    };

    class BufferBase{
    public:
        virtual ~BufferBase() = default;
        virtual void push(LogLine&& logline) = 0;
        virtual bool pop(LogLine& logline) = 0;
    };

    class Buffer{
    public:
        struct Item{
            char padding[256 - sizeof(LogLine)];
            LogLine logline;
            Item(LogLine&& logline) : logline(std::move(logline)){}
        };

        static constexpr const size_t size = 32768;

        Buffer() : items(static_cast<Item*>(std::malloc(size * sizeof(Item)))){
            for(size_t i = 0; i <= size; i++)   write_state[i].store(0, std::memory_order_relaxed);
            static_assert(sizeof(Item) == 256);
        }

        ~Buffer(){
            unsigned int write_cnt = write_state[size].load();
            for(size_t i = 0; i < write_cnt; i++)   items[i].~Item();
            std::free(items);
        }

        bool push(LogLine&& logline, const unsigned int write_index){
            new(&items[write_index]) Item(std::move(logline));
            write_state[write_index].store(1, std::memory_order_release);
            return write_state[size].fetch_add(1, std::memory_order_acquire) + 1 == size;
        }

        bool pop(LogLine& logline, const unsigned int read_index){
            if(write_state[read_index].load(std::memory_order_acquire)){
                Item& item = items[read_index];
                logline = std::move(item.logline);
                return true;
            }
            return false;
        }

        Buffer(const Buffer&) = delete;
        Buffer& operator=(const Buffer&) = delete;

    private:
        Item *items;
        std::atomic<unsigned int>write_state[size + 1]; // write_state[size]: write count
    };

    class QueueBuffer : public BufferBase{
    public:
        QueueBuffer() : r_cursor{nullptr}, write_index(0), flag{ATOMIC_FLAG_INIT}, read_index(0){
            create_buffer();
        }

        void push(LogLine&& logline) override{
            unsigned int windex = write_index.fetch_add(1, std::memory_order_relaxed);
            if(windex < Buffer::size){
                if(w_cursor.load(std::memory_order_acquire) -> push(std::move(logline), windex)){
                    create_buffer();
                }
            }else{
                while(write_index.load(std::memory_order_acquire) >= Buffer::size);
                // wait until buffer is available
                push(std::move(logline));
            }
        }

        bool pop(LogLine& logline) override{
            if(r_cursor == nullptr)  r_cursor = get_rbuffer();
            Buffer *rcursor = r_cursor; // avoid race conditions
            if(rcursor == nullptr)  return false;
            if(rcursor -> pop(logline, read_index)){
                read_index++;
                if(read_index == Buffer::size){
                    read_index = 0;
                    r_cursor = nullptr;
                    SpinLock spinlock(flag);
                    buffers.pop();
                }
                return true;
            }
            return false;
        }

        QueueBuffer(const QueueBuffer&) = delete;
        QueueBuffer& operator=(const QueueBuffer&) = delete;
        // disable copy constructor
        
    private:
        std::queue<std::unique_ptr<Buffer>>buffers;
        std::atomic<Buffer*>w_cursor;   //current write buffer
        Buffer* r_cursor;    //current read buffer
        std::atomic<unsigned int>write_index;
        unsigned int read_index;
        std::atomic_flag flag;

        void create_buffer(){
            std::unique_ptr<Buffer>next_wbuffer(new Buffer());
            w_cursor.store(next_wbuffer.get(), std::memory_order_release);
            SpinLock spinlock(flag);
            buffers.push(std::move(next_wbuffer));
            write_index.store(0, std::memory_order_relaxed);
        }

        Buffer* get_rbuffer(){
            SpinLock spinlock(flag);
            return buffers.empty() ? nullptr : buffers.front().get();
        }
    };

    class FileWriter{
    public:
        FileWriter(const std::string& dir, const std::string& filename, uint32_t roll_size)
          : roll_bytes(roll_size * 1024 * 1024),
          path(dir + filename){
            roll();
        }

        void write(LogLine& logline){
            auto w_pos = s -> tellp();
            logline.stream_to_string(*s);
            bytes_written += s -> tellp() - w_pos;
            if(bytes_written > roll_bytes)  roll();
        }

    private:
        const uint32_t roll_bytes;
        const std::string path;
        std::unique_ptr<std::ofstream>s;
        std::streamoff bytes_written = 0;
        uint32_t file_index = 0;

        void roll(){
            if(s){
                s -> flush();
                s -> close();
            }

            bytes_written = 0;
            s.reset(new std::ofstream());

            std::filesystem::path log_file = path;
            log_file += ".";
            log_file += std::to_string(++file_index);
            log_file += ".txt";
	        s -> open(log_file);
        }
    };

    class Logger{
    public:
        Logger(const std::string& dir, const std::string& filename, uint32_t roll_size)
          : state(State::INIT),
          buffer_queue(new QueueBuffer()),
          file_writer(dir, filename, std::max(1u, roll_size)),
          thread(&Logger::pop, this){
            state.store(State::ENABLED, std::memory_order_release);
        }
        
        ~Logger(){
            state.store(State::DISABLED);
            thread.join();
        }

        void add(LogLine&& logline){
            buffer_queue -> push(std::move(logline));
        }

        void pop(){
            while(state.load(std::memory_order_acquire) == State::INIT);
            // wait until constructor is finished
            LogLine logline(LogSeverity::INFO, nullptr, nullptr, 0);
            while(state.load(std::memory_order_seq_cst) == State::ENABLED){
                if(buffer_queue -> pop(logline))    file_writer.write(logline);
            }
            // read remaining log
            while(buffer_queue -> pop(logline)) file_writer.write(logline);
        }
          
    private:
        enum class State{
            INIT,
            ENABLED,
            DISABLED,
        };
        std::atomic<State>state;
        std::unique_ptr<BufferBase>buffer_queue;
        FileWriter file_writer;
        std::thread thread;
    };

    std::unique_ptr<Logger>logger;
    std::atomic<Logger*>atomic_logger;

    bool Slog::operator+=(LogLine& logline){
        atomic_logger.load(std::memory_order_acquire) -> add(std::move(logline));
        return true;
    }

    void init(const std::string& dir, const std::string filename, uint32_t roll_size){
        logger.reset(new Logger(dir, filename, roll_size));
        atomic_logger.store(logger.get(), std::memory_order_seq_cst);
    }
}