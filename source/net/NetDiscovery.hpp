#ifndef NET_NETDISCOVERY_HPP
#define NET_NETDISCOVERY_HPP

#include <stdint.h>
#include <vector>

#include "game/core/CriticalSection.hpp"
#include "net/NetMessages.hpp"
#include "net/NetSocket.hpp"

class C4JEventImpl;
class C4JThreadImpl;

namespace lcenet
{
class Beacon
{
public:
    Beacon();
    ~Beacon();
    void Start();
    void Stop();
    bool IsRunning() const { return m_thread != 0; }
    void SetSession(const SessionDescription& session);

private:
    Beacon(const Beacon&);
    Beacon& operator=(const Beacon&);

    static int Entry(void* beacon);
    void Loop();
    bool Stopped();

    C4JCriticalSection m_lock;
    C4JEventImpl* m_wake;
    C4JThreadImpl* m_thread;
    bool m_stop;
    std::vector<unsigned char> m_payload;
};

struct LanSession
{
    LanSession() : lastSeen(0) {}

    Address address;
    SessionDescription session;
    std::vector<unsigned char> raw;
    int64_t lastSeen;
};

class Scanner
{
public:
    Scanner();
    ~Scanner();
    void Start();
    void Stop();
    bool IsRunning() const { return m_thread != 0; }
    unsigned int Revision();
    std::vector<LanSession> Sessions();

private:
    Scanner(const Scanner&);
    Scanner& operator=(const Scanner&);

    static int Entry(void* scanner);
    void Loop();
    bool Stopped();
    void Receive(const unsigned char* data, int size, const Address& from);
    void Expire();

    C4JCriticalSection m_lock;
    C4JThreadImpl* m_thread;
    bool m_stop;
    unsigned int m_revision;
    std::vector<LanSession> m_sessions;
};
}

#endif
