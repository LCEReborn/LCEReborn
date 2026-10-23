#include "net/NetLink.hpp"

#include <algorithm>

#include "game/core/System.hpp"
#include "platform/C4JEventImpl.hpp"
#include "platform/C4JThreadImpl.hpp"

namespace lcenet
{
namespace
{
const size_t kHeaderSize = 6;
const size_t kMaxPayload = 16 * 1024 * 1024;
const int kConnectTimeoutMs = 5000;
const int kThreadStack = 0x10000;
}

int64_t NowMs()
{
    return System::processTimeInMilliSecs();
}

Link::Link(unsigned int systemId, LinkHandler* handler)
    : m_systemId(systemId),
      m_handler(handler),
      m_wake(new C4JEventImpl(C4JEvent::Mode0)),
      m_reader(0),
      m_writer(0),
      m_connecting(false),
      m_connected(false),
      m_stop(false),
      m_closed(false),
      m_flushThenClose(false),
      m_openedAt(NowMs()),
      m_connectedAt(0),
      m_queuedBytes(0)
{
    InitializeCriticalSection(&m_lock);
}

Link::~Link()
{
    Close();
    if (m_reader != 0)
    {
        m_reader->WaitForCompletion(-1);
        delete m_reader;
    }
    if (m_writer != 0)
    {
        m_writer->WaitForCompletion(-1);
        delete m_writer;
    }
    m_socket.Close();
    delete m_wake;
    DeleteCriticalSection(&m_lock);
}

void Link::Start()
{
    m_reader = new C4JThreadImpl(&Link::ReadEntry, this, "NetLinkRead", kThreadStack);
    m_writer = new C4JThreadImpl(&Link::WriteEntry, this, "NetLinkWrite", kThreadStack);
    m_reader->Run();
    m_writer->Run();
}

void Link::Connect(const Address& address)
{
    m_target = address;
    m_peer = address;
    m_connecting = true;
    Start();
}

void Link::Connect(const std::string& address)
{
    m_targetText = address;
    m_connecting = true;
    Start();
}

void Link::Adopt(TcpSocket& socket, const Address& peer)
{
    m_socket.Swap(socket);
    m_peer = peer;
    m_connected = true;
    m_connectedAt = NowMs();
    Start();
}

void Link::Send(unsigned char channel, unsigned char flags, const void* data, size_t size)
{
    std::vector<unsigned char> frame(kHeaderSize + size);
    frame[0] = channel;
    frame[1] = flags;
    frame[2] = (unsigned char)(size >> 24);
    frame[3] = (unsigned char)(size >> 16);
    frame[4] = (unsigned char)(size >> 8);
    frame[5] = (unsigned char)size;
    if (size > 0)
    {
        const unsigned char* bytes = static_cast<const unsigned char*>(data);
        std::copy(bytes, bytes + size, frame.begin() + kHeaderSize);
    }
    EnterCriticalSection(&m_lock);
    if (!m_closed && !m_flushThenClose)
    {
        m_queuedBytes += frame.size();
        m_outbox.push_back(std::vector<unsigned char>());
        m_outbox.back().swap(frame);
    }
    LeaveCriticalSection(&m_lock);
    m_wake->Set();
}

void Link::SendControl(unsigned char channel, const std::vector<unsigned char>& payload)
{
    Send(channel, 0, payload.empty() ? 0 : &payload[0], payload.size());
}

bool Link::TakeControl(Frame& frame)
{
    EnterCriticalSection(&m_lock);
    bool found = !m_inbox.empty();
    if (found)
    {
        frame.channel = m_inbox.front().channel;
        frame.receivedAt = m_inbox.front().receivedAt;
        frame.payload.swap(m_inbox.front().payload);
        m_inbox.pop_front();
    }
    LeaveCriticalSection(&m_lock);
    return found;
}

void Link::CloseAfterFlush()
{
    EnterCriticalSection(&m_lock);
    m_flushThenClose = true;
    LeaveCriticalSection(&m_lock);
    m_wake->Set();
}

void Link::Close()
{
    EnterCriticalSection(&m_lock);
    m_stop = true;
    m_closed = true;
    m_socket.Shutdown();
    LeaveCriticalSection(&m_lock);
    m_wake->Set();
}

int64_t Link::ConnectedAt()
{
    EnterCriticalSection(&m_lock);
    int64_t connectedAt = m_connectedAt;
    LeaveCriticalSection(&m_lock);
    return connectedAt;
}

bool Link::IsClosed()
{
    EnterCriticalSection(&m_lock);
    bool closed = m_closed;
    LeaveCriticalSection(&m_lock);
    return closed;
}

std::string Link::Error()
{
    EnterCriticalSection(&m_lock);
    std::string error = m_error;
    LeaveCriticalSection(&m_lock);
    return error;
}

Address Link::Peer()
{
    EnterCriticalSection(&m_lock);
    Address peer = m_peer;
    LeaveCriticalSection(&m_lock);
    return peer;
}

size_t Link::QueuedFrames()
{
    EnterCriticalSection(&m_lock);
    size_t frames = m_outbox.size();
    LeaveCriticalSection(&m_lock);
    return frames;
}

size_t Link::QueuedBytes()
{
    EnterCriticalSection(&m_lock);
    size_t bytes = m_queuedBytes;
    LeaveCriticalSection(&m_lock);
    return bytes;
}

bool Link::Cancelled() const
{
    EnterCriticalSection(&m_lock);
    bool stop = m_stop;
    LeaveCriticalSection(&m_lock);
    return stop;
}

bool Link::Stopped()
{
    return Cancelled();
}

void Link::Fail(const std::string& error)
{
    EnterCriticalSection(&m_lock);
    if (m_error.empty() && !m_stop)
        m_error = error;
    m_stop = true;
    m_closed = true;
    m_socket.Shutdown();
    LeaveCriticalSection(&m_lock);
    m_wake->Set();
}

int Link::ReadEntry(void* link)
{
    static_cast<Link*>(link)->ReadLoop();
    return 0;
}

int Link::WriteEntry(void* link)
{
    static_cast<Link*>(link)->WriteLoop();
    return 0;
}

bool Link::ReadExact(unsigned char* data, size_t size)
{
    while (size > 0)
    {
        if (Stopped())
            return false;
        int received = m_socket.Receive(data, size, 100);
        if (received < 0)
            return false;
        data += received;
        size -= (size_t)received;
    }
    return true;
}

void Link::ReadLoop()
{
    if (m_connecting)
    {
        std::string error;
        if (!m_targetText.empty())
        {
            Address resolved;
            if (!Resolve(m_targetText, resolved, error))
            {
                Fail(error);
                return;
            }
            EnterCriticalSection(&m_lock);
            m_target = resolved;
            m_peer = resolved;
            LeaveCriticalSection(&m_lock);
        }
        TcpSocket socket;
        if (!socket.Connect(m_target, kConnectTimeoutMs, *this, error))
        {
            Fail(error);
            return;
        }
        EnterCriticalSection(&m_lock);
        bool stopped = m_stop;
        if (!stopped)
        {
            m_socket.Swap(socket);
            m_connected = true;
            m_connectedAt = NowMs();
        }
        LeaveCriticalSection(&m_lock);
        if (stopped)
            return;
        m_wake->Set();
    }

    unsigned char header[kHeaderSize];
    std::vector<unsigned char> payload;
    while (!Stopped())
    {
        if (!ReadExact(header, kHeaderSize))
            break;
        size_t size = ((size_t)header[2] << 24) | ((size_t)header[3] << 16) | ((size_t)header[4] << 8) | header[5];
        if (size > kMaxPayload)
        {
            Fail("The other side sent something this version doesn't understand.");
            return;
        }
        payload.resize(size);
        if (size > 0 && !ReadExact(&payload[0], size))
            break;

        if (header[0] < kFirstControlChannel)
        {
            m_handler->OnLinkData(*this, header[0], header[1], size > 0 ? &payload[0] : 0, size);
        }
        else
        {
            EnterCriticalSection(&m_lock);
            m_inbox.push_back(Frame());
            m_inbox.back().channel = header[0];
            m_inbox.back().payload = payload;
            m_inbox.back().receivedAt = NowMs();
            LeaveCriticalSection(&m_lock);
        }
    }
    Fail("The connection was closed.");
}

void Link::WriteLoop()
{
    while (true)
    {
        std::vector<unsigned char> frame;
        bool shutdownNow = false;
        EnterCriticalSection(&m_lock);
        if (m_stop)
        {
            LeaveCriticalSection(&m_lock);
            return;
        }
        if (m_connected && !m_outbox.empty())
        {
            frame.swap(m_outbox.front());
            m_outbox.pop_front();
        }
        else if (m_connected && m_flushThenClose)
        {
            shutdownNow = true;
            m_socket.Shutdown();
        }
        LeaveCriticalSection(&m_lock);

        if (shutdownNow)
            return;
        if (frame.empty())
        {
            m_wake->WaitForSignal(100);
            continue;
        }
        bool sent = m_socket.SendAll(&frame[0], frame.size());
        EnterCriticalSection(&m_lock);
        m_queuedBytes -= frame.size();
        LeaveCriticalSection(&m_lock);
        if (!sent)
        {
            Fail("The connection was lost.");
            return;
        }
    }
}
}
