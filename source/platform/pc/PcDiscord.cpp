#include "platform/pc/PcDiscord.hpp"

#include <cerrno>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <mutex>
#include <thread>
#include <vector>

#include "platform/pc/Log.hpp"

#if defined(_WIN32)
#include <windows.h>
#else
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#endif

namespace
{
const Logger sLog("Discord");
const unsigned int kOpHandshake = 0;
const unsigned int kOpFrame = 1;
const size_t kMaxDetailsLength = 127;
const int kRetrySeconds = 15;

struct DiscordState
{
    std::mutex mutex;
    std::condition_variable changed;
    std::string applicationId;
    std::string details;
    unsigned int revision = 0;
    long long startTime = 0;
};

DiscordState* sState = NULL;

class Pipe
{
public:
    ~Pipe() { Close(); }

    bool Open()
    {
        for (int index = 0; index < 10; ++index)
        {
#if defined(_WIN32)
            const std::string name = "\\\\?\\pipe\\discord-ipc-" + std::to_string(index);
            mHandle = CreateFileA(name.c_str(), GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
            if (mHandle != INVALID_HANDLE_VALUE)
                return true;
#else
            static const char* const kVariables[] = {"XDG_RUNTIME_DIR", "TMPDIR", "TMP", "TEMP"};
            std::vector<std::string> folders;
            for (const char* variable : kVariables)
            {
                const char* value = std::getenv(variable);
                if (value != NULL && value[0] != '\0')
                    folders.push_back(value);
            }
            folders.push_back("/tmp");
            for (const std::string& folder : folders)
            {
                const std::string path = folder + "/discord-ipc-" + std::to_string(index);
                sockaddr_un address;
                std::memset(&address, 0, sizeof(address));
                if (path.size() >= sizeof(address.sun_path))
                    continue;
                address.sun_family = AF_UNIX;
                std::memcpy(address.sun_path, path.c_str(), path.size());
                mSocket = socket(AF_UNIX, SOCK_STREAM, 0);
                if (mSocket < 0)
                    return false;
                if (connect(mSocket, reinterpret_cast<sockaddr*>(&address), sizeof(address)) == 0)
                    return true;
                close(mSocket);
                mSocket = -1;
            }
#endif
        }
        return false;
    }

    void Close()
    {
#if defined(_WIN32)
        if (mHandle != INVALID_HANDLE_VALUE)
            CloseHandle(mHandle);
        mHandle = INVALID_HANDLE_VALUE;
#else
        if (mSocket >= 0)
            close(mSocket);
        mSocket = -1;
#endif
    }

    bool Send(unsigned int opcode, const std::string& json)
    {
        std::vector<unsigned char> message(8 + json.size());
        const uint32_t header[2] = {opcode, static_cast<uint32_t>(json.size())};
        for (int word = 0; word < 2; ++word)
            for (int byte = 0; byte < 4; ++byte)
                message[word * 4 + byte] = static_cast<unsigned char>(header[word] >> (byte * 8));
        std::memcpy(message.data() + 8, json.data(), json.size());
#if defined(_WIN32)
        DWORD written = 0;
        return WriteFile(mHandle, message.data(), static_cast<DWORD>(message.size()), &written, NULL)
               && written == message.size();
#else
        size_t sent = 0;
        while (sent < message.size())
        {
            const ssize_t result = send(mSocket, message.data() + sent, message.size() - sent, MSG_NOSIGNAL);
            if (result <= 0)
                return false;
            sent += static_cast<size_t>(result);
        }
        return true;
#endif
    }

    bool Drain()
    {
        unsigned char buffer[4096];
#if defined(_WIN32)
        DWORD available = 0;
        while (PeekNamedPipe(mHandle, NULL, 0, NULL, &available, NULL))
        {
            if (available == 0)
                return true;
            DWORD read = 0;
            if (!ReadFile(mHandle, buffer, available < sizeof(buffer) ? available : sizeof(buffer), &read, NULL)
                || read == 0)
                return false;
        }
        return false;
#else
        for (;;)
        {
            const ssize_t result = recv(mSocket, buffer, sizeof(buffer), MSG_DONTWAIT);
            if (result > 0)
                continue;
            if (result == 0)
                return false;
            return errno == EAGAIN || errno == EWOULDBLOCK;
        }
#endif
    }

private:
#if defined(_WIN32)
    HANDLE mHandle = INVALID_HANDLE_VALUE;
#else
    int mSocket = -1;
#endif
};

std::string Utf8(const std::wstring& text)
{
    std::string result;
    for (size_t index = 0; index < text.size(); ++index)
    {
        uint32_t codepoint = static_cast<uint32_t>(text[index]);
        if (codepoint >= 0xd800 && codepoint < 0xdc00 && index + 1 < text.size())
        {
            const uint32_t low = static_cast<uint32_t>(text[index + 1]);
            if (low >= 0xdc00 && low < 0xe000)
            {
                codepoint = 0x10000 + ((codepoint - 0xd800) << 10) + (low - 0xdc00);
                ++index;
            }
        }
        if (codepoint < 0x80)
            result += static_cast<char>(codepoint);
        else if (codepoint < 0x800)
        {
            result += static_cast<char>(0xc0 | (codepoint >> 6));
            result += static_cast<char>(0x80 | (codepoint & 0x3f));
        }
        else if (codepoint < 0x10000)
        {
            result += static_cast<char>(0xe0 | (codepoint >> 12));
            result += static_cast<char>(0x80 | ((codepoint >> 6) & 0x3f));
            result += static_cast<char>(0x80 | (codepoint & 0x3f));
        }
        else
        {
            result += static_cast<char>(0xf0 | (codepoint >> 18));
            result += static_cast<char>(0x80 | ((codepoint >> 12) & 0x3f));
            result += static_cast<char>(0x80 | ((codepoint >> 6) & 0x3f));
            result += static_cast<char>(0x80 | (codepoint & 0x3f));
        }
    }
    return result;
}

std::string JsonString(const std::string& text)
{
    std::string result = "\"";
    for (unsigned char character : text)
    {
        if (character == '"' || character == '\\')
        {
            result += '\\';
            result += static_cast<char>(character);
        }
        else if (character < 0x20)
        {
            char escaped[8];
            std::snprintf(escaped, sizeof(escaped), "\\u%04x", character);
            result += escaped;
        }
        else
            result += static_cast<char>(character);
    }
    return result + "\"";
}

int ProcessId()
{
#if defined(_WIN32)
    return static_cast<int>(GetCurrentProcessId());
#else
    return static_cast<int>(getpid());
#endif
}

std::string ActivityJson(const std::string& details, long long startTime, unsigned int nonce)
{
    std::string activity = "{\"timestamps\":{\"start\":" + std::to_string(startTime) + "}";
    if (!details.empty())
        activity += ",\"details\":" + JsonString(details);
    activity += "}";
    return "{\"cmd\":\"SET_ACTIVITY\",\"args\":{\"pid\":" + std::to_string(ProcessId()) + ",\"activity\":" + activity
           + "},\"nonce\":\"" + std::to_string(nonce) + "\"}";
}

void Run(DiscordState* state)
{
    Pipe pipe;
    bool connected = false;
    bool announced = false;
    unsigned int sentRevision = 0;
    unsigned int nonce = 0;
    bool first = true;
    for (;;)
    {
        std::string details;
        unsigned int revision;
        {
            std::unique_lock<std::mutex> lock(state->mutex);
            if (!first)
                state->changed.wait_for(lock, std::chrono::seconds(connected ? 2 : kRetrySeconds),
                                        [&] { return connected && state->revision != sentRevision; });
            first = false;
            details = state->details;
            revision = state->revision;
        }

        if (!connected)
        {
            if (!pipe.Open())
                continue;
            if (!pipe.Send(kOpHandshake, "{\"v\":1,\"client_id\":" + JsonString(state->applicationId) + "}"))
            {
                pipe.Close();
                continue;
            }
            connected = true;
            sentRevision = revision - 1;
            if (!announced)
                LOG_INFO(sLog, "Connected to Discord");
            announced = true;
        }

        if (!pipe.Drain())
        {
            pipe.Close();
            connected = false;
            continue;
        }
        if (revision != sentRevision)
        {
            if (!pipe.Send(kOpFrame, ActivityJson(details, state->startTime, ++nonce)))
            {
                pipe.Close();
                connected = false;
                continue;
            }
            sentRevision = revision;
        }
    }
}
}

void PcDiscordStart(const std::string& applicationId)
{
    if (sState != NULL || applicationId.empty())
        return;
    sState = new DiscordState;
    sState->applicationId = applicationId;
    sState->startTime = static_cast<long long>(std::time(NULL));
    std::thread(Run, sState).detach();
}

void PcDiscordSetPresence(const std::wstring& details)
{
    if (sState == NULL)
        return;
    std::string text = Utf8(details);
    if (text.size() > kMaxDetailsLength)
    {
        size_t length = kMaxDetailsLength;
        while (length > 0 && (static_cast<unsigned char>(text[length]) & 0xc0) == 0x80)
            --length;
        text.resize(length);
    }
    std::lock_guard<std::mutex> lock(sState->mutex);
    if (text == sState->details)
        return;
    sState->details = text;
    ++sState->revision;
    sState->changed.notify_one();
}
