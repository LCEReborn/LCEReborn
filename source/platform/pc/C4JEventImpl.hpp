#ifndef PLATFORM_PC_C4JEVENTIMPL_HPP
#define PLATFORM_PC_C4JEVENTIMPL_HPP

#include <condition_variable>
#include <mutex>

#include "platform/C4JEvent.hpp"

class C4JEventImpl : public C4JEvent
{
public:
    C4JEventImpl(C4JEvent::EMode mode = C4JEvent::Mode0);
    virtual ~C4JEventImpl();

    virtual void Set();
    virtual void Clear();
    virtual unsigned int WaitForSignal(int timeout);

private:
    bool m_autoReset;
    bool m_signalled;
    std::mutex m_mutex;
    std::condition_variable m_condition;
};

#endif
