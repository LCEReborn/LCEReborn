#include "platform/pc/C4JEventArrayImpl.hpp"

#include <chrono>

static const unsigned int WAIT_SIGNALLED = 0;
static const unsigned int WAIT_TIMEOUT = 0x102;

C4JEventArrayImpl::C4JEventArrayImpl(int count, C4JEventArray::EMode mode)
    : m_count(count), m_autoReset(mode != C4JEventArray::Mode1), m_signalled(static_cast<size_t>(count), false)
{
}

C4JEventArrayImpl::~C4JEventArrayImpl()
{
}

void C4JEventArrayImpl::Set(int index)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_signalled[index] = true;
    m_condition.notify_all();
}

void C4JEventArrayImpl::Clear(int index)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_signalled[index] = false;
}

void C4JEventArrayImpl::SetAll()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    for (int index = 0; index < m_count; ++index)
        m_signalled[index] = true;
    m_condition.notify_all();
}

void C4JEventArrayImpl::ClearAll()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    for (int index = 0; index < m_count; ++index)
        m_signalled[index] = false;
}

bool C4JEventArrayImpl::AllSignalled() const
{
    for (int index = 0; index < m_count; ++index)
    {
        if (!m_signalled[index])
            return false;
    }
    return true;
}

unsigned int C4JEventArrayImpl::WaitForAll(int timeout)
{
    std::unique_lock<std::mutex> lock(m_mutex);
    if (timeout == -1)
        m_condition.wait(lock, [this] { return AllSignalled(); });
    else if (!m_condition.wait_for(lock, std::chrono::milliseconds(timeout), [this] { return AllSignalled(); }))
        return WAIT_TIMEOUT;
    if (m_autoReset)
    {
        for (int index = 0; index < m_count; ++index)
            m_signalled[index] = false;
    }
    return WAIT_SIGNALLED;
}

unsigned int C4JEventArrayImpl::WaitForAny(int timeout)
{
    int signalled = -1;
    std::unique_lock<std::mutex> lock(m_mutex);
    auto anySignalled = [this, &signalled]
    {
        for (int index = 0; index < m_count; ++index)
        {
            if (m_signalled[index])
            {
                signalled = index;
                return true;
            }
        }
        return false;
    };
    if (timeout == -1)
        m_condition.wait(lock, anySignalled);
    else if (!m_condition.wait_for(lock, std::chrono::milliseconds(timeout), anySignalled))
        return WAIT_TIMEOUT;
    if (m_autoReset)
        m_signalled[signalled] = false;
    return WAIT_SIGNALLED;
}

unsigned int C4JEventArrayImpl::WaitForSingle(int index, int timeout)
{
    std::unique_lock<std::mutex> lock(m_mutex);
    if (timeout == -1)
    {
        m_condition.wait(lock, [this, index] { return m_signalled[index]; });
        if (m_autoReset)
            m_signalled[index] = false;
        return WAIT_SIGNALLED;
    }
    if (m_condition.wait_for(lock, std::chrono::milliseconds(timeout), [this, index] { return m_signalled[index]; }) &&
        m_autoReset)
    {
        m_signalled[index] = false;
    }
    return WAIT_TIMEOUT;
}

void C4JEventArrayImpl::Cancel()
{
}
