#include "net/NetDiscovery.hpp"

#include "net/NetLink.hpp"
#include "net/NetLog.hpp"
#include "platform/C4JEventImpl.hpp"
#include "platform/C4JThreadImpl.hpp"

namespace lcenet
{
namespace
{
const int kBeaconIntervalMs = 1500;
const int64_t kAddressRefreshMs = 10000;
const int64_t kSessionTimeoutMs = 5000;
const uint32_t kLoopback = 0x7f000001u;
const int kThreadStack = 0x10000;
}

Beacon::Beacon() : m_wake(new C4JEventImpl(C4JEvent::Mode0)), m_thread(0), m_stop(false)
{
    InitializeCriticalSection(&m_lock);
}

Beacon::~Beacon()
{
    Stop();
    delete m_wake;
    DeleteCriticalSection(&m_lock);
}

void Beacon::Start()
{
    if (m_thread != 0)
        return;
    m_stop = false;
    m_thread = new C4JThreadImpl(&Beacon::Entry, this, "NetBeacon", kThreadStack);
    m_thread->Run();
}

void Beacon::Stop()
{
    if (m_thread == 0)
        return;
    EnterCriticalSection(&m_lock);
    m_stop = true;
    LeaveCriticalSection(&m_lock);
    m_wake->Set();
    m_thread->WaitForCompletion(-1);
    delete m_thread;
    m_thread = 0;
    m_payload.clear();
}

void Beacon::SetSession(const SessionDescription& session)
{
    ByteWriter writer;
    writer.U32(kBeaconMagic);
    WriteSession(writer, session);
    EnterCriticalSection(&m_lock);
    m_payload = writer.Data();
    LeaveCriticalSection(&m_lock);
}

int Beacon::Entry(void* beacon)
{
    static_cast<Beacon*>(beacon)->Loop();
    return 0;
}

bool Beacon::Stopped()
{
    EnterCriticalSection(&m_lock);
    bool stop = m_stop;
    LeaveCriticalSection(&m_lock);
    return stop;
}

void Beacon::Loop()
{
    UdpSocket socket;
    std::string error;
    if (!socket.Open(0, error))
    {
        NET_LOG_WARN("Can't announce the game on the local network: %s", error.c_str());
        return;
    }
    std::vector<uint32_t> targets;
    int64_t targetsRead = 0;
    while (!Stopped())
    {
        EnterCriticalSection(&m_lock);
        std::vector<unsigned char> payload = m_payload;
        LeaveCriticalSection(&m_lock);
        if (!payload.empty())
        {
            int64_t now = NowMs();
            if (targets.empty() || now - targetsRead > kAddressRefreshMs)
            {
                targets = BroadcastAddresses();
                targets.push_back(kLoopback);
                targetsRead = now;
            }
            for (size_t index = 0; index < targets.size(); ++index)
            {
                Address target;
                target.ip = targets[index];
                target.port = kDefaultPort;
                socket.SendTo(target, &payload[0], payload.size());
            }
        }
        int64_t wakeAt = NowMs() + kBeaconIntervalMs;
        while (!Stopped() && NowMs() < wakeAt)
            m_wake->WaitForSignal(100);
    }
}

Scanner::Scanner() : m_thread(0), m_stop(false), m_revision(0)
{
    InitializeCriticalSection(&m_lock);
}

Scanner::~Scanner()
{
    Stop();
    DeleteCriticalSection(&m_lock);
}

void Scanner::Start()
{
    if (m_thread != 0)
        return;
    m_stop = false;
    m_thread = new C4JThreadImpl(&Scanner::Entry, this, "NetScanner", kThreadStack);
    m_thread->Run();
}

void Scanner::Stop()
{
    if (m_thread == 0)
        return;
    EnterCriticalSection(&m_lock);
    m_stop = true;
    LeaveCriticalSection(&m_lock);
    m_thread->WaitForCompletion(-1);
    delete m_thread;
    m_thread = 0;
    EnterCriticalSection(&m_lock);
    if (!m_sessions.empty())
    {
        m_sessions.clear();
        ++m_revision;
    }
    LeaveCriticalSection(&m_lock);
}

unsigned int Scanner::Revision()
{
    EnterCriticalSection(&m_lock);
    unsigned int revision = m_revision;
    LeaveCriticalSection(&m_lock);
    return revision;
}

std::vector<LanSession> Scanner::Sessions()
{
    EnterCriticalSection(&m_lock);
    std::vector<LanSession> sessions = m_sessions;
    LeaveCriticalSection(&m_lock);
    return sessions;
}

int Scanner::Entry(void* scanner)
{
    static_cast<Scanner*>(scanner)->Loop();
    return 0;
}

bool Scanner::Stopped()
{
    EnterCriticalSection(&m_lock);
    bool stop = m_stop;
    LeaveCriticalSection(&m_lock);
    return stop;
}

void Scanner::Loop()
{
    UdpSocket socket;
    bool warned = false;
    unsigned char buffer[2048];
    while (!Stopped())
    {
        if (!socket.IsOpen())
        {
            std::string error;
            if (!socket.Open(kDefaultPort, error))
            {
                if (!warned)
                    NET_LOG_WARN("Can't look for games on the local network: %s", error.c_str());
                warned = true;
                for (int wait = 0; wait < 20 && !Stopped(); ++wait)
                    C4JThreadImpl::Sleep(100);
                continue;
            }
        }
        Address from;
        int received = socket.ReceiveFrom(buffer, sizeof(buffer), from, 200);
        if (received > 0)
            Receive(buffer, received, from);
        Expire();
    }
}

void Scanner::Receive(const unsigned char* data, int size, const Address& from)
{
    ByteReader reader(data, (size_t)size);
    if (reader.U32() != kBeaconMagic)
        return;
    SessionDescription session;
    if (!ReadSession(reader, session) || session.port == 0)
        return;

    Address address;
    address.ip = from.ip;
    address.port = session.port;
    std::vector<unsigned char> raw(data, data + size);
    int64_t now = NowMs();

    EnterCriticalSection(&m_lock);
    bool known = false;
    for (size_t index = 0; index < m_sessions.size() && !known; ++index)
    {
        LanSession& entry = m_sessions[index];
        if (entry.session.nonce != session.nonce || entry.address.port != session.port)
            continue;
        known = true;
        if (entry.address.IsLoopback() || !address.IsLoopback())
            entry.address = address;
        entry.lastSeen = now;
        if (entry.raw != raw)
        {
            entry.raw = raw;
            entry.session = session;
            ++m_revision;
        }
    }
    if (!known)
    {
        LanSession entry;
        entry.address = address;
        entry.session = session;
        entry.raw = raw;
        entry.lastSeen = now;
        m_sessions.push_back(entry);
        ++m_revision;
    }
    LeaveCriticalSection(&m_lock);
}

void Scanner::Expire()
{
    int64_t now = NowMs();
    EnterCriticalSection(&m_lock);
    for (size_t index = 0; index < m_sessions.size();)
    {
        if (now - m_sessions[index].lastSeen > kSessionTimeoutMs)
        {
            m_sessions.erase(m_sessions.begin() + index);
            ++m_revision;
        }
        else
        {
            ++index;
        }
    }
    LeaveCriticalSection(&m_lock);
}
}
