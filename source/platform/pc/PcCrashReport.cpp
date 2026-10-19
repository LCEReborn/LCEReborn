#include "platform/pc/PcCrashReport.hpp"

#include <atomic>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <filesystem>
#include <string>
#include <utility>
#include <vector>

#include "platform/pc/Log.hpp"

#if defined(_WIN32)
#include <windows.h>
#include <dbghelp.h>
#include <tlhelp32.h>
#else
#include <execinfo.h>
#include <pthread.h>
#include <unistd.h>
#endif

namespace fs = std::filesystem;

namespace
{
const Logger sLog("Crash");
const int kMaxFrames = 64;

struct Section
{
    std::string title;
    PcCrashReport::SectionWriter writer;
};

std::vector<Section>& Sections()
{
    static std::vector<Section> sections;
    return sections;
}

fs::path& ReportRoot()
{
    static fs::path root = fs::absolute(fs::path("crash-reports"));
    return root;
}

std::atomic<bool> sWriting(false);

std::string Timestamp(const char* format)
{
    const std::time_t now = std::time(nullptr);
    std::tm local = {};
#if defined(_WIN32)
    localtime_s(&local, &now);
#else
    localtime_r(&now, &local);
#endif
    char text[64];
    std::strftime(text, sizeof(text), format, &local);
    return text;
}

std::string Format(const char* format, ...)
{
    char buffer[1024];
    va_list arguments;
    va_start(arguments, format);
    std::vsnprintf(buffer, sizeof(buffer), format, arguments);
    va_end(arguments);
    return buffer;
}

bool WriteFile(const fs::path& path, const std::string& text)
{
    FILE* file = nullptr;
#if defined(_WIN32)
    file = _wfopen(path.c_str(), L"wb");
#else
    file = std::fopen(path.c_str(), "wb");
#endif
    if (file == nullptr)
        return false;
    const bool written = std::fwrite(text.data(), 1, text.size(), file) == text.size();
    return std::fclose(file) == 0 && written;
}

#if defined(_WIN32)
typedef HRESULT(WINAPI* SetThreadDescriptionFn)(HANDLE, PCWSTR);
typedef HRESULT(WINAPI* GetThreadDescriptionFn)(HANDLE, PWSTR*);

std::string Narrow(const wchar_t* text)
{
    if (text == nullptr || text[0] == L'\0')
        return std::string();
    const int length = WideCharToMultiByte(CP_UTF8, 0, text, -1, nullptr, 0, nullptr, nullptr);
    if (length <= 1)
        return std::string();
    std::string result(static_cast<size_t>(length - 1), '\0');
    WideCharToMultiByte(CP_UTF8, 0, text, -1, &result[0], length, nullptr, nullptr);
    return result;
}

std::string ThreadName(HANDLE thread)
{
    static GetThreadDescriptionFn getDescription = reinterpret_cast<GetThreadDescriptionFn>(
        GetProcAddress(GetModuleHandleW(L"kernel32.dll"), "GetThreadDescription"));
    if (getDescription == nullptr || thread == nullptr)
        return std::string();
    PWSTR description = nullptr;
    if (FAILED(getDescription(thread, &description)))
        return std::string();
    std::string name = Narrow(description);
    LocalFree(description);
    return name;
}

void AppendThreads(std::string& out, DWORD failingThread)
{
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
    if (snapshot == INVALID_HANDLE_VALUE)
    {
        out += "The thread list could not be read.\n";
        return;
    }
    const DWORD process = GetCurrentProcessId();
    THREADENTRY32 entry = {};
    entry.dwSize = sizeof(entry);
    int count = 0;
    for (BOOL more = Thread32First(snapshot, &entry); more; more = Thread32Next(snapshot, &entry))
    {
        if (entry.th32OwnerProcessID != process)
            continue;
        HANDLE thread = OpenThread(THREAD_QUERY_LIMITED_INFORMATION, FALSE, entry.th32ThreadID);
        std::string name = ThreadName(thread);
        if (thread != nullptr)
            CloseHandle(thread);
        out += Format("%6lu  %s%s\n", static_cast<unsigned long>(entry.th32ThreadID),
                      name.empty() ? "(unnamed)" : name.c_str(),
                      entry.th32ThreadID == failingThread ? "  <- failed here" : "");
        ++count;
    }
    CloseHandle(snapshot);
    out += Format("%d threads; every thread's stack is in minidump.dmp.\n", count);
}

std::string DescribeAddress(HANDLE process, DWORD64 address)
{
    std::string module = "?";
    HMODULE handle = nullptr;
    if (GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                           reinterpret_cast<LPCWSTR>(address), &handle))
    {
        wchar_t path[MAX_PATH];
        if (GetModuleFileNameW(handle, path, MAX_PATH) != 0)
            module = fs::path(path).filename().u8string();
    }

    std::string text = Format("%-20s 0x%016llx", module.c_str(), static_cast<unsigned long long>(address));
    alignas(SYMBOL_INFO) char storage[sizeof(SYMBOL_INFO) + MAX_SYM_NAME];
    SYMBOL_INFO* symbol = reinterpret_cast<SYMBOL_INFO*>(storage);
    symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
    symbol->MaxNameLen = MAX_SYM_NAME;
    DWORD64 displacement = 0;
    if (SymFromAddr(process, address, &displacement, symbol))
    {
        text += Format("  %s + 0x%llx", symbol->Name, static_cast<unsigned long long>(displacement));
        IMAGEHLP_LINE64 line = {};
        line.SizeOfStruct = sizeof(line);
        DWORD lineDisplacement = 0;
        if (SymGetLineFromAddr64(process, address, &lineDisplacement, &line))
            text += Format("  (%s:%lu)", fs::path(line.FileName).filename().u8string().c_str(),
                           static_cast<unsigned long>(line.LineNumber));
    }
    return text;
}

void AppendStack(std::string& out, EXCEPTION_POINTERS* exception)
{
    HANDLE process = GetCurrentProcess();
    SymSetOptions(SYMOPT_UNDNAME | SYMOPT_DEFERRED_LOADS | SYMOPT_LOAD_LINES);
    const bool symbols = SymInitialize(process, nullptr, TRUE) != FALSE;

    std::vector<DWORD64> frames;
#if defined(_M_X64)
    if (exception != nullptr)
    {
        CONTEXT context = *exception->ContextRecord;
        STACKFRAME64 frame = {};
        frame.AddrPC.Offset = context.Rip;
        frame.AddrPC.Mode = AddrModeFlat;
        frame.AddrFrame.Offset = context.Rbp;
        frame.AddrFrame.Mode = AddrModeFlat;
        frame.AddrStack.Offset = context.Rsp;
        frame.AddrStack.Mode = AddrModeFlat;
        while (frames.size() < kMaxFrames
               && StackWalk64(IMAGE_FILE_MACHINE_AMD64, process, GetCurrentThread(), &frame, &context, nullptr,
                              SymFunctionTableAccess64, SymGetModuleBase64, nullptr)
               && frame.AddrPC.Offset != 0)
            frames.push_back(frame.AddrPC.Offset);
    }
#endif
    if (frames.empty())
    {
        void* addresses[kMaxFrames];
        const USHORT captured = CaptureStackBackTrace(0, kMaxFrames, addresses, nullptr);
        for (USHORT index = 0; index < captured; ++index)
            frames.push_back(reinterpret_cast<DWORD64>(addresses[index]));
    }

    for (size_t index = 0; index < frames.size(); ++index)
        out += Format("%3d  ", static_cast<int>(index)) + DescribeAddress(process, frames[index]) + "\n";
    if (!symbols)
        out += "Symbols could not be loaded; open minidump.dmp with LCEReborn.pdb for names.\n";
    if (symbols)
        SymCleanup(process);
}

bool WriteMinidump(const fs::path& path, EXCEPTION_POINTERS* exception)
{
    HANDLE file = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE)
        return false;
    MINIDUMP_EXCEPTION_INFORMATION exceptionInfo = {};
    exceptionInfo.ThreadId = GetCurrentThreadId();
    exceptionInfo.ExceptionPointers = exception;
    exceptionInfo.ClientPointers = FALSE;
    const MINIDUMP_TYPE type = static_cast<MINIDUMP_TYPE>(MiniDumpWithThreadInfo | MiniDumpWithIndirectlyReferencedMemory
                                                          | MiniDumpWithUnloadedModules);
    const BOOL written = MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(), file, type,
                                           exception != nullptr ? &exceptionInfo : nullptr, nullptr, nullptr);
    CloseHandle(file);
    return written != FALSE;
}
#endif

#if defined(_WIN32)
std::string WriteReport(const char* reason, EXCEPTION_POINTERS* exception)
#else
std::string WriteReport(const char* reason)
#endif
{
    bool expected = false;
    if (!sWriting.compare_exchange_strong(expected, true))
        return std::string();
    Log::Flush();

    std::error_code error;
    fs::path folder = ReportRoot() / ("crash-" + Timestamp("%Y-%m-%d_%H-%M-%S"));
    fs::create_directories(folder, error);
    if (error)
    {
        LOG_ERROR(sLog, "The crash report folder %s could not be created: %s", folder.u8string().c_str(),
                  error.message().c_str());
        return std::string();
    }

    std::string report = "LCEReborn crash report\n";
    report += "Time: " + Timestamp("%Y-%m-%d %H:%M:%S") + "\n";
    report += std::string("Reason: ") + reason + "\n";
#if defined(_WIN32)
    const DWORD failingThread = GetCurrentThreadId();
    HANDLE current = OpenThread(THREAD_QUERY_LIMITED_INFORMATION, FALSE, failingThread);
    const std::string threadName = ThreadName(current);
    if (current != nullptr)
        CloseHandle(current);
    report += Format("Thread: %s (%lu)\n", threadName.empty() ? "(unnamed)" : threadName.c_str(),
                     static_cast<unsigned long>(failingThread));
#endif

    for (const Section& section : Sections())
    {
        report += "\n-- " + section.title + " --\n";
        section.writer(report);
    }

    report += "\n-- Stack of the failing thread --\n";
#if defined(_WIN32)
    AppendStack(report, exception);
    report += "\n-- Threads --\n";
    AppendThreads(report, failingThread);
    const bool dumped = WriteMinidump(folder / "minidump.dmp", exception);
    if (!dumped)
        report += "\nminidump.dmp could not be written.\n";
#else
    void* addresses[kMaxFrames];
    const int captured = backtrace(addresses, kMaxFrames);
    char** symbols = backtrace_symbols(addresses, captured);
    for (int index = 0; index < captured; ++index)
        report += Format("%3d  %s\n", index, symbols != nullptr ? symbols[index] : "?");
    free(symbols);
#endif

    if (!WriteFile(folder / "report.txt", report))
        LOG_ERROR(sLog, "%s could not be written", (folder / "report.txt").u8string().c_str());
    LOG_FATAL(sLog, "Crash report written to %s", folder.u8string().c_str());
    return folder.u8string();
}

#if defined(_WIN32)
LONG WINAPI UnhandledException(EXCEPTION_POINTERS* exception)
{
    const DWORD code = exception->ExceptionRecord->ExceptionCode;
    const std::string reason = Format("Unhandled exception 0x%08lX at %p", static_cast<unsigned long>(code),
                                      exception->ExceptionRecord->ExceptionAddress);
    LOG_FATAL(sLog, "%s", reason.c_str());
    WriteReport(reason.c_str(), exception);
    Log::Stop();
    return EXCEPTION_CONTINUE_SEARCH;
}
#endif
}

void PcCrashReport::Install(const std::string& logDirectory)
{
    ReportRoot() = fs::absolute(fs::u8path(logDirectory)).parent_path() / "crash-reports";
#if defined(_WIN32)
    SetUnhandledExceptionFilter(UnhandledException);
#endif
}

void PcCrashReport::AddSection(const char* title, SectionWriter writer)
{
    Sections().push_back(Section{title, writer});
}

void PcCrashReport::NameCurrentThread(const char* rawName)
{
    std::string trimmed(rawName);
    while (!trimmed.empty() && (trimmed.back() == '\n' || trimmed.back() == '\r' || trimmed.back() == ' '))
        trimmed.pop_back();
    const char* name = trimmed.c_str();
#if defined(_WIN32)
    static SetThreadDescriptionFn setDescription = reinterpret_cast<SetThreadDescriptionFn>(
        GetProcAddress(GetModuleHandleW(L"kernel32.dll"), "SetThreadDescription"));
    if (setDescription == nullptr)
        return;
    const int length = MultiByteToWideChar(CP_UTF8, 0, name, -1, nullptr, 0);
    if (length <= 0)
        return;
    std::wstring wide(static_cast<size_t>(length), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, name, -1, &wide[0], length);
    setDescription(GetCurrentThread(), wide.c_str());
#elif defined(__APPLE__)
    pthread_setname_np(name);
#else
    char shortName[16];
    std::snprintf(shortName, sizeof(shortName), "%s", name);
    pthread_setname_np(pthread_self(), shortName);
#endif
}

std::string PcCrashReport::Write(const char* reason)
{
#if defined(_WIN32)
    return WriteReport(reason, nullptr);
#else
    return WriteReport(reason);
#endif
}
