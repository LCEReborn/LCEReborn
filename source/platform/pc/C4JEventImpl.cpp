#include "platform/pc/C4JEventImpl.hpp"

#include <chrono>

C4JEventImpl::C4JEventImpl(C4JEvent::EMode mode) : m_autoReset(mode != C4JEvent::Mode1), m_signalled(false)
{
}

C4JEventImpl::~C4JEventImpl()
{
}

void C4JEventImpl::Set()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_signalled = true;
    if (m_autoReset)
        m_condition.notify_one();
    else
        m_condition.notify_all();
}

void C4JEventImpl::Clear()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_signalled = false;
}

unsigned int C4JEventImpl::WaitForSignal(int timeout)
{
    std::unique_lock<std::mutex> lock(m_mutex);
    if (timeout == -1)
    {
        m_condition.wait(lock, [this] { return m_signalled; });
    }
    else if (!m_condition.wait_for(lock, std::chrono::milliseconds(timeout), [this] { return m_signalled; }))
    {
        return WaitTimeout;
    }
    if (m_autoReset)
        m_signalled = false;
    return WaitSignalled;
}
