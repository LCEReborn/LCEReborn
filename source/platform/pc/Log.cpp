#include "platform/pc/Log.hpp"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <cwchar>
#include <filesystem>
#include <mutex>
#include <system_error>
#include <thread>
#include <vector>

#include <zlib.h>

#if defined(_WIN32)
#include <windows.h>
#endif

#include "platform/C4JThread.hpp"

namespace fs = std::filesystem;

namespace
{
const char* const kLevelNames[] = {"TRACE", "DEBUG", "INFO", "WARN", "ERROR", "FATAL"};
const std::size_t kMaxPending = 65536;
const Logger sLog("Log");

struct Record
{
    std::chrono::system_clock::time_point time;
    LogLevel level;
    const char* logger;
    char thread[32];
    std::string text;
};

struct LogState
{
    std::mutex mutex;
    std::condition_variable wake;
    std::condition_variable written;
    std::vector<Record> pending;
    unsigned long long queued = 0;
    unsigned long long flushed = 0;
    bool running = false;
    bool stopping = false;
    std::thread writer;
    std::thread compressor;
    std::FILE* file = nullptr;
    std::mutex directMutex;
#if defined(_WIN32)
    unsigned int consoleCodePage = 0;
#endif
};

LogState& State()
{
    static LogState* state = new LogState;
    return *state;
}

thread_local bool sIsWriterThread = false;

void CurrentThreadName(char (&out)[32])
{
    const char* name = C4JThread::getCurrentThreadName();
    if (name != nullptr && name[0] != '\0')
    {
        std::snprintf(out, sizeof(out), "%s", name);
        return;
    }
    static std::atomic<unsigned int> nextNumber(1);
    thread_local unsigned int number = nextNumber++;
    std::snprintf(out, sizeof(out), "Thread %u", number);
}

void AppendUtf8(std::string& out, unsigned int codePoint)
{
    if (codePoint < 0x80)
        out += static_cast<char>(codePoint);
    else if (codePoint < 0x800)
    {
        out += static_cast<char>(0xc0 | (codePoint >> 6));
        out += static_cast<char>(0x80 | (codePoint & 0x3f));
    }
    else if (codePoint < 0x10000)
    {
        out += static_cast<char>(0xe0 | (codePoint >> 12));
        out += static_cast<char>(0x80 | ((codePoint >> 6) & 0x3f));
        out += static_cast<char>(0x80 | (codePoint & 0x3f));
    }
    else
    {
        out += static_cast<char>(0xf0 | (codePoint >> 18));
        out += static_cast<char>(0x80 | ((codePoint >> 12) & 0x3f));
        out += static_cast<char>(0x80 | ((codePoint >> 6) & 0x3f));
        out += static_cast<char>(0x80 | (codePoint & 0x3f));
    }
}

std::string WideToUtf8(const wchar_t* text, std::size_t length)
{
    std::string result;
    result.reserve(length);
    for (std::size_t index = 0; index < length; ++index)
    {
        unsigned int codePoint = static_cast<unsigned int>(text[index]);
        if (sizeof(wchar_t) == 2 && codePoint >= 0xd800 && codePoint < 0xdc00 && index + 1 < length)
        {
            const unsigned int low = static_cast<unsigned int>(text[index + 1]);
            if (low >= 0xdc00 && low < 0xe000)
            {
                codePoint = 0x10000 + ((codePoint - 0xd800) << 10) + (low - 0xdc00);
                ++index;
            }
        }
        AppendUtf8(result, codePoint);
    }
    return result;
}

std::string FormatV(const char* format, va_list arguments)
{
    char buffer[512];
    va_list copy;
    va_copy(copy, arguments);
    const int length = std::vsnprintf(buffer, sizeof(buffer), format, copy);
    va_end(copy);
    if (length < 0)
        return std::string(format);
    if (static_cast<std::size_t>(length) < sizeof(buffer))
        return std::string(buffer, static_cast<std::size_t>(length));
    std::string text(static_cast<std::size_t>(length), '\0');
    std::vsnprintf(&text[0], text.size() + 1, format, arguments);
    return text;
}

std::string FormatWideV(const wchar_t* format, va_list arguments)
{
    std::vector<wchar_t> buffer(512);
    for (;;)
    {
        va_list copy;
        va_copy(copy, arguments);
        const int length = std::vswprintf(buffer.data(), buffer.size(), format, copy);
        va_end(copy);
        if (length >= 0)
            return WideToUtf8(buffer.data(), static_cast<std::size_t>(length));
        if (buffer.size() >= 65536)
            return WideToUtf8(format, std::wcslen(format));
        buffer.resize(buffer.size() * 4);
    }
}

struct TimeCache
{
    std::time_t second = -1;
    std::tm local = {};
};

void AppendRecord(std::string& out, const Record& record, TimeCache& cache)
{
    using namespace std::chrono;
    const std::time_t second = system_clock::to_time_t(record.time);
    if (second != cache.second)
    {
        cache.second = second;
#if defined(_WIN32)
        localtime_s(&cache.local, &second);
#else
        localtime_r(&second, &cache.local);
#endif
    }
    const int milliseconds =
        static_cast<int>(duration_cast<std::chrono::milliseconds>(record.time.time_since_epoch()).count() % 1000);

    char header[128];
    const int headerLength =
        std::snprintf(header, sizeof(header), "[%02d:%02d:%02d.%03d] [%s/%s] [%s]: ", cache.local.tm_hour,
                      cache.local.tm_min, cache.local.tm_sec, milliseconds < 0 ? 0 : milliseconds, record.thread,
                      kLevelNames[static_cast<int>(record.level)], record.logger);
    const std::size_t prefix = headerLength > 0 ? static_cast<std::size_t>(headerLength) : 0;

    const std::string& text = record.text;
    std::size_t end = text.size();
    while (end > 0 && (text[end - 1] == '\n' || text[end - 1] == '\r'))
        --end;

    std::size_t start = 0;
    for (;;)
    {
        std::size_t lineEnd = text.find('\n', start);
        if (lineEnd == std::string::npos || lineEnd > end)
            lineEnd = end;
        std::size_t contentEnd = lineEnd;
        if (contentEnd > start && text[contentEnd - 1] == '\r')
            --contentEnd;
        out.append(header, prefix);
        out.append(text, start, contentEnd - start);
        out += '\n';
        if (lineEnd >= end)
            break;
        start = lineEnd + 1;
    }
}

void Emit(const std::string& text, std::FILE* file)
{
    if (text.empty())
        return;
    std::fwrite(text.data(), 1, text.size(), stdout);
    std::fflush(stdout);
    if (file != nullptr)
    {
        std::fwrite(text.data(), 1, text.size(), file);
        std::fflush(file);
    }
}

std::FILE* OpenFile(const fs::path& path, const char* mode)
{
#if defined(_WIN32)
    std::wstring wideMode(mode, mode + std::strlen(mode));
    return _wfopen(path.c_str(), wideMode.c_str());
#else
    return std::fopen(path.c_str(), mode);
#endif
}

bool CompressArchive(const fs::path& source, std::string& error)
{
    fs::path target = source;
    target += ".gz";
    fs::path temporary = target;
    temporary += ".tmp";

    std::FILE* input = OpenFile(source, "rb");
    if (input == nullptr)
    {
        error = "could not open " + source.u8string();
        return false;
    }
    std::FILE* output = OpenFile(temporary, "wb");
    if (output == nullptr)
    {
        std::fclose(input);
        error = "could not create " + temporary.u8string();
        return false;
    }

    z_stream stream;
    std::memset(&stream, 0, sizeof(stream));
    bool ok = deflateInit2(&stream, Z_DEFAULT_COMPRESSION, Z_DEFLATED, 15 + 16, 8, Z_DEFAULT_STRATEGY) == Z_OK;
    if (!ok)
        error = "zlib could not start compressing";

    std::vector<unsigned char> in(1 << 16);
    std::vector<unsigned char> out(1 << 16);
    int flush = Z_NO_FLUSH;
    while (ok && flush != Z_FINISH)
    {
        const std::size_t read = std::fread(in.data(), 1, in.size(), input);
        if (std::ferror(input))
        {
            error = "could not read " + source.u8string();
            ok = false;
            break;
        }
        flush = std::feof(input) ? Z_FINISH : Z_NO_FLUSH;
        stream.next_in = in.data();
        stream.avail_in = static_cast<uInt>(read);
        do
        {
            stream.next_out = out.data();
            stream.avail_out = static_cast<uInt>(out.size());
            const int result = deflate(&stream, flush);
            if (result == Z_STREAM_ERROR)
            {
                error = "zlib failed while compressing";
                ok = false;
                break;
            }
            const std::size_t produced = out.size() - stream.avail_out;
            if (produced > 0 && std::fwrite(out.data(), 1, produced, output) != produced)
            {
                error = "could not write " + temporary.u8string();
                ok = false;
                break;
            }
        } while (stream.avail_out == 0);
    }
    deflateEnd(&stream);
    std::fclose(input);
    if (std::fclose(output) != 0 && ok)
    {
        error = "could not write " + temporary.u8string();
        ok = false;
    }

    std::error_code fileError;
    if (ok)
    {
        fs::rename(temporary, target, fileError);
        if (fileError)
        {
            error = "could not rename " + temporary.u8string() + ": " + fileError.message();
            ok = false;
        }
    }
    if (!ok)
    {
        fs::remove(temporary, fileError);
        return false;
    }
    fs::remove(source, fileError);
    return true;
}

void CompressArchives(const std::vector<fs::path>& archives)
{
    for (const fs::path& archive : archives)
    {
        std::string error;
        if (!CompressArchive(archive, error))
            sLog.Write(LogLevel::Warn, "Previous log %s was kept uncompressed: %s", archive.u8string().c_str(),
                       error.c_str());
    }
}

void WriterLoop()
{
    sIsWriterThread = true;
    LogState& state = State();

    std::vector<Record> batch;
    std::string text;
    TimeCache cache;
    std::unique_lock<std::mutex> lock(state.mutex);
    for (;;)
    {
        state.wake.wait(lock, [&state] { return !state.pending.empty() || state.stopping; });
        if (state.pending.empty())
            break;
        batch.swap(state.pending);
        const unsigned long long upTo = state.queued;
        lock.unlock();
        state.written.notify_all();

        text.clear();
        for (const Record& record : batch)
            AppendRecord(text, record, cache);
        batch.clear();
        Emit(text, state.file);

        lock.lock();
        state.flushed = upTo;
        state.written.notify_all();
    }
}

void WriteDirect(const Record& record)
{
    LogState& state = State();
    TimeCache cache;
    std::string text;
    AppendRecord(text, record, cache);
    std::lock_guard<std::mutex> lock(state.directMutex);
    Emit(text, nullptr);
}

void Submit(LogLevel level, const char* logger, std::string text)
{
    Record record;
    record.time = std::chrono::system_clock::now();
    record.level = level;
    record.logger = logger;
    CurrentThreadName(record.thread);
    record.text = std::move(text);

    LogState& state = State();
    std::unique_lock<std::mutex> lock(state.mutex);
    if (!sIsWriterThread)
        state.written.wait(lock, [&state] { return state.pending.size() < kMaxPending || !state.running; });
    if (!state.running)
    {
        lock.unlock();
        WriteDirect(record);
        return;
    }
    state.pending.push_back(std::move(record));
    const unsigned long long sequence = ++state.queued;
    state.wake.notify_one();
    if (level >= LogLevel::Error && !sIsWriterThread)
        state.written.wait(lock, [&state, sequence] { return state.flushed >= sequence || !state.running; });
}

fs::path ArchiveBase(const fs::path& directory, const fs::path& latest)
{
    std::error_code error;
    std::time_t stamp = std::time(nullptr);
    const fs::file_time_type modified = fs::last_write_time(latest, error);
    if (!error)
    {
        const auto system = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
            modified - fs::file_time_type::clock::now() + std::chrono::system_clock::now());
        stamp = std::chrono::system_clock::to_time_t(system);
    }
    std::tm local = {};
#if defined(_WIN32)
    localtime_s(&local, &stamp);
#else
    localtime_r(&stamp, &local);
#endif
    char date[16];
    std::snprintf(date, sizeof(date), "%04d-%02d-%02d", local.tm_year + 1900, local.tm_mon + 1, local.tm_mday);

    for (int number = 1;; ++number)
    {
        const fs::path base = directory / (std::string(date) + "-" + std::to_string(number) + ".log");
        fs::path compressed = base;
        compressed += ".gz";
        if (!fs::exists(base, error) && !fs::exists(compressed, error))
            return base;
    }
}
}

void Logger::Write(LogLevel level, const char* format, ...) const
{
    va_list arguments;
    va_start(arguments, format);
    WriteV(level, format, arguments);
    va_end(arguments);
}

void Logger::WriteV(LogLevel level, const char* format, va_list arguments) const
{
    Submit(level, mName, FormatV(format, arguments));
}

void Logger::WriteWideV(LogLevel level, const wchar_t* format, va_list arguments) const
{
    Submit(level, mName, FormatWideV(format, arguments));
}

void Logger::WriteText(LogLevel level, const std::string& text) const
{
    Submit(level, mName, text);
}

void Log::Start(const std::string& directory)
{
    LogState& state = State();
    {
        std::lock_guard<std::mutex> lock(state.mutex);
        if (state.running)
            return;
    }

#if defined(_WIN32)
    state.consoleCodePage = GetConsoleOutputCP();
    SetConsoleOutputCP(CP_UTF8);
#endif

    std::vector<std::string> warnings;
    std::vector<fs::path> archives;
    const fs::path folder = fs::absolute(fs::u8path(directory));
    std::error_code error;
    fs::create_directories(folder, error);
    if (error || !fs::is_directory(folder, error))
    {
        warnings.push_back("The logs folder " + folder.u8string() + " could not be created ("
                           + (error ? error.message() : std::string("not a folder"))
                           + "); logging to the console only.");
    }
    else
    {
        for (fs::directory_iterator entry(folder, error), end; !error && entry != end; entry.increment(error))
        {
            const fs::path& path = entry->path();
            if (path.extension() == ".tmp" && entry->is_regular_file(error))
                fs::remove(path, error);
            else if (path.extension() == ".log" && path.filename() != "latest.log" && entry->is_regular_file(error))
                archives.push_back(path);
        }

        const fs::path latest = folder / "latest.log";
        if (fs::exists(latest, error))
        {
            const fs::path archive = ArchiveBase(folder, latest);
            fs::rename(latest, archive, error);
            if (error)
                warnings.push_back("The previous latest.log could not be archived: " + error.message());
            else
                archives.push_back(archive);
        }

        state.file = OpenFile(latest, "wb");
        if (state.file == nullptr)
            warnings.push_back("Could not open " + latest.u8string() + "; logging to the console only.");
        else
            std::setvbuf(state.file, nullptr, _IOFBF, 1 << 16);
    }

    {
        std::lock_guard<std::mutex> lock(state.mutex);
        state.stopping = false;
        state.running = true;
    }
    state.writer = std::thread(WriterLoop);
    if (!archives.empty())
        state.compressor = std::thread(CompressArchives, archives);
    std::atexit(Log::Stop);

    for (const std::string& warning : warnings)
        sLog.WriteText(LogLevel::Warn, warning);
}

void Log::Flush()
{
    if (sIsWriterThread)
        return;
    LogState& state = State();
    std::unique_lock<std::mutex> lock(state.mutex);
    const unsigned long long target = state.queued;
    state.written.wait(lock, [&state, target] { return state.flushed >= target || !state.running; });
}

void Log::Stop()
{
    LogState& state = State();
    if (state.compressor.joinable() && state.compressor.get_id() != std::this_thread::get_id())
        state.compressor.join();
    {
        std::lock_guard<std::mutex> lock(state.mutex);
        if (!state.running)
            return;
        state.running = false;
        state.stopping = true;
    }
    state.wake.notify_all();
    state.written.notify_all();
    if (state.writer.joinable() && state.writer.get_id() != std::this_thread::get_id())
        state.writer.join();
    if (state.file != nullptr)
    {
        std::fclose(state.file);
        state.file = nullptr;
    }
#if defined(_WIN32)
    if (state.consoleCodePage != 0)
        SetConsoleOutputCP(state.consoleCodePage);
#endif
}
