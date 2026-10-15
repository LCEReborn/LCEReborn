#ifndef PLATFORM_PC_C4JEVENTARRAYIMPL_HPP
#define PLATFORM_PC_C4JEVENTARRAYIMPL_HPP

#include <condition_variable>
#include <mutex>
#include <vector>

#include "platform/C4JEventArray.hpp"

class C4JEventArrayImpl : public C4JEventArray
{
public:
    C4JEventArrayImpl(int count, C4JEventArray::EMode mode);
    virtual ~C4JEventArrayImpl();

    virtual void Set(int index);
    virtual void Clear(int index);
    virtual void SetAll();
    virtual void ClearAll();
    virtual unsigned int WaitForAll(int timeout);
    virtual unsigned int WaitForAny(int timeout);
    virtual unsigned int WaitForSingle(int index, int timeout);
    virtual void Cancel();

private:
    bool AllSignalled() const;

    int m_count;
    bool m_autoReset;
    std::vector<bool> m_signalled;
    std::mutex m_mutex;
    std::condition_variable m_condition;
};

#endif
