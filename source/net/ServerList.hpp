#ifndef NET_SERVERLIST_HPP
#define NET_SERVERLIST_HPP

#include <stddef.h>
#include <string>
#include <vector>

namespace lcenet
{
struct SavedServer
{
    std::wstring name;
    std::wstring address;
};

class ServerList
{
public:
    void Load();
    bool Save();
    const std::vector<SavedServer>& Servers() const { return m_servers; }
    void Add(const SavedServer& server);
    void Update(size_t index, const SavedServer& server);
    void Remove(size_t index);

private:
    std::vector<SavedServer> m_servers;
};

ServerList& SavedServers();
std::string NarrowAddress(const std::wstring& address);
}

#endif
