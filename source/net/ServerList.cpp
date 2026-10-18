#include "net/ServerList.hpp"

#include <stdio.h>
#include <string.h>

#include "net/NetLog.hpp"
#include "net/NetMessages.hpp"
#include "platform/Platform.hpp"

#if LCE_PLATFORM_WIIU
#include <coreinit/filesystem.h>
#include <coreinit/memdefaultheap.h>
#include <nn/act/client_cpp.h>
#include <nn/save/save.h>

#include "platform/wiiu/CProfile.hpp"
#elif LCE_PLATFORM_PC
#include "platform/pc/PcPaths.hpp"
#endif

namespace lcenet
{
namespace
{
const uint32_t kServerListMagic = 0x4c434553;
const unsigned int kServerListVersion = 1;
const size_t kMaxServers = 64;
const char* const kFileName = "servers.dat";

#if LCE_PLATFORM_WIIU
size_t AlignedSize(size_t size)
{
    return (size + 0x3f) & ~(size_t)0x3f;
}

class SaveFile
{
public:
    SaveFile()
        : m_client(static_cast<FSClient*>(MEMAllocFromDefaultHeapEx(sizeof(FSClient), 0x40))),
          m_block(static_cast<FSCmdBlock*>(MEMAllocFromDefaultHeapEx(sizeof(FSCmdBlock), 0x40))),
          m_ready(false)
    {
        gProfileInstance->EnterInitSaveDirCS();
        if (m_client != 0 && m_block != 0 && FSAddClient(m_client, FS_ERROR_FLAG_ALL) == FS_STATUS_OK)
        {
            FSInitCmdBlock(m_block);
            m_ready = true;
        }
    }

    ~SaveFile()
    {
        if (m_ready)
            FSDelClient(m_client, FS_ERROR_FLAG_ALL);
        if (m_client != 0)
            MEMFreeToDefaultHeap(m_client);
        if (m_block != 0)
            MEMFreeToDefaultHeap(m_block);
        gProfileInstance->LeaveInitSaveDirCS();
    }

    bool Read(std::vector<unsigned char>& data)
    {
        FSFileHandle handle;
        if (!m_ready
            || SAVEOpenFile(m_client, m_block, nn::act::GetSlotNo(), kFileName, "r", &handle, FS_ERROR_FLAG_ALL)
                   != FS_STATUS_OK)
            return false;
        FSStat stat;
        bool ok = FSGetStatFile(m_client, m_block, handle, &stat, FS_ERROR_FLAG_ALL) == FS_STATUS_OK;
        uint8_t* buffer = 0;
        if (ok && stat.size > 0)
        {
            buffer = static_cast<uint8_t*>(MEMAllocFromDefaultHeapEx(AlignedSize(stat.size), 0x40));
            ok = buffer != 0
                 && FSReadFile(m_client, m_block, buffer, 1, stat.size, handle, 0, FS_ERROR_FLAG_ALL)
                        == (FSStatus)stat.size;
            if (ok)
                data.assign(buffer, buffer + stat.size);
            if (buffer != 0)
                MEMFreeToDefaultHeap(buffer);
        }
        FSCloseFile(m_client, m_block, handle, FS_ERROR_FLAG_ALL);
        return ok;
    }

    bool Write(const std::vector<unsigned char>& data)
    {
        FSFileHandle handle;
        uint8_t slot = nn::act::GetSlotNo();
        if (!m_ready)
            return false;
        if (SAVEOpenFile(m_client, m_block, slot, kFileName, "w", &handle, FS_ERROR_FLAG_ALL) != FS_STATUS_OK)
        {
            SAVERollbackQuota(m_client, m_block, slot, FS_ERROR_FLAG_ALL);
            return false;
        }
        uint8_t* buffer = static_cast<uint8_t*>(MEMAllocFromDefaultHeapEx(AlignedSize(data.size()), 0x40));
        bool ok = buffer != 0;
        if (ok)
        {
            memcpy(buffer, &data[0], data.size());
            ok = FSWriteFile(m_client, m_block, buffer, 1, data.size(), handle, 0, FS_ERROR_FLAG_ALL)
                 == (FSStatus)data.size();
            MEMFreeToDefaultHeap(buffer);
        }
        FSCloseFile(m_client, m_block, handle, FS_ERROR_FLAG_ALL);
        if (ok)
            ok = SAVEFlushQuota(m_client, m_block, slot, FS_ERROR_FLAG_ALL) == FS_STATUS_OK;
        if (!ok)
            SAVERollbackQuota(m_client, m_block, slot, FS_ERROR_FLAG_ALL);
        return ok;
    }

private:
    SaveFile(const SaveFile&);
    SaveFile& operator=(const SaveFile&);

    FSClient* m_client;
    FSCmdBlock* m_block;
    bool m_ready;
};

bool ReadServerFile(std::vector<unsigned char>& data)
{
    SaveFile file;
    return file.Read(data);
}

bool WriteServerFile(const std::vector<unsigned char>& data)
{
    SaveFile file;
    return file.Write(data);
}
#elif LCE_PLATFORM_PC
std::string ServerFilePath()
{
    return PcSaveRoot() + "/" + kFileName;
}

bool ReadServerFile(std::vector<unsigned char>& data)
{
    FILE* file = fopen(ServerFilePath().c_str(), "rb");
    if (file == 0)
        return false;
    unsigned char chunk[4096];
    size_t read;
    while ((read = fread(chunk, 1, sizeof(chunk), file)) > 0)
        data.insert(data.end(), chunk, chunk + read);
    bool ok = ferror(file) == 0;
    fclose(file);
    return ok;
}

bool WriteServerFile(const std::vector<unsigned char>& data)
{
    std::string path = ServerFilePath();
    std::string temporary = path + ".tmp";
    FILE* file = fopen(temporary.c_str(), "wb");
    if (file == 0)
        return false;
    bool ok = fwrite(&data[0], 1, data.size(), file) == data.size();
    ok = fclose(file) == 0 && ok;
    if (!ok)
    {
        remove(temporary.c_str());
        return false;
    }
    remove(path.c_str());
    return rename(temporary.c_str(), path.c_str()) == 0;
}
#endif
}

ServerList& SavedServers()
{
    static ServerList* servers = 0;
    if (servers == 0)
    {
        servers = new ServerList;
        servers->Load();
    }
    return *servers;
}

std::string NarrowAddress(const std::wstring& address)
{
    std::string text;
    for (size_t index = 0; index < address.size(); ++index)
        text.push_back(address[index] < 0x80 ? (char)address[index] : '?');
    return text;
}

void ServerList::Load()
{
    m_servers.clear();
    std::vector<unsigned char> data;
    if (!ReadServerFile(data) || data.empty())
        return;
    ByteReader reader(data);
    if (reader.U32() != kServerListMagic || reader.U16() != kServerListVersion)
    {
        NET_LOG_WARN("Ignored a saved server list this version can't read");
        return;
    }
    unsigned int count = reader.U16();
    std::vector<SavedServer> servers;
    for (unsigned int index = 0; index < count && index < kMaxServers; ++index)
    {
        SavedServer server;
        server.name = reader.Text();
        server.address = reader.Text();
        servers.push_back(server);
    }
    if (!reader.Ok())
    {
        NET_LOG_WARN("Ignored a damaged saved server list");
        return;
    }
    m_servers.swap(servers);
}

bool ServerList::Save()
{
    ByteWriter writer;
    writer.U32(kServerListMagic);
    writer.U16(kServerListVersion);
    writer.U16((unsigned int)m_servers.size());
    for (size_t index = 0; index < m_servers.size(); ++index)
    {
        writer.Text(m_servers[index].name);
        writer.Text(m_servers[index].address);
    }
    bool saved = WriteServerFile(writer.Data());
    if (!saved)
        NET_LOG_WARN("Couldn't save the server list");
    return saved;
}

void ServerList::Add(const SavedServer& server)
{
    if (m_servers.size() < kMaxServers)
        m_servers.push_back(server);
}

void ServerList::Update(size_t index, const SavedServer& server)
{
    if (index < m_servers.size())
        m_servers[index] = server;
}

void ServerList::Remove(size_t index)
{
    if (index < m_servers.size())
        m_servers.erase(m_servers.begin() + index);
}
}
