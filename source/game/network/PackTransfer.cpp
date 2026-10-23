#include "game/network/PackTransfer.hpp"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <cwchar>

#include "game/client/render/TexturePackRepository.hpp"
#include "game/core/ByteArrayInputStream.hpp"
#include "game/core/ByteArrayOutputStream.hpp"
#include "game/core/CConsoleMinecraftApp.hpp"
#include "game/core/ConsoleUIController.hpp"
#include "game/core/DLCManager.hpp"
#include "game/core/DLCPack.hpp"
#include "game/core/DataInputStream.hpp"
#include "game/core/DataOutputStream.hpp"
#include "game/core/Gui.hpp"
#include "game/core/Minecraft.hpp"
#include "game/core/StringConversion.hpp"
#include "game/core/StringIds.hpp"
#include "game/core/System.hpp"
#include "game/ui/UILayer.hpp"
#include "game/network/CGameNetworkManager.hpp"
#include "game/network/ClientPacketListener.hpp"
#include "game/network/ClientboundCustomPayloadPacket.hpp"
#include "game/network/ServerGamePacketListenerImpl.hpp"
#include "game/network/ServerboundCustomPayloadPacket.hpp"
#include "platform/CProfile.hpp"
#include "platform/FileApi.hpp"
#include "platform/FileSystem.hpp"
#include "platform/Runtime.hpp"
#if LCE_PLATFORM_PC
#include "platform/pc/Log.hpp"
#endif

extern ConsoleUIController gConsoleUIController;
extern C4JStorage* gStorage;

struct PackManifest
{
    unsigned int packId;
    std::string root;
    bool rootIsFile;
    std::wstring name;
    std::vector<std::string> files;
    std::vector<long long> sizes;
    long long size;
    unsigned char hash[Sha1::DIGEST_SIZE];

    std::string fullPath(size_t index) const { return rootIsFile ? root : root + "/" + files[index]; }
};

namespace
{
enum EMessage
{
    eMessage_Offer,
    eMessage_Request,
    eMessage_File,
    eMessage_Data,
    eMessage_Done,
    eMessage_Skip,
    eMessage_Removed
};

const unsigned int kChunkSize = 30000;
const int kMaxQueuedBytes = 256 * 1024;
const long long kMaxPackSize = 512LL * 1024 * 1024;
const long long kAnswerWaitMs = 60 * 1000;
const size_t kMaxPathLength = 160;

#if LCE_PLATFORM_PC
const Logger sLog("Packs");
#endif

void LogInfo(const std::string& message)
{
#if LCE_PLATFORM_PC
    sLog.WriteText(LogLevel::Info, message);
#else
    (void)message;
#endif
}

bool IsSafeCharacter(wchar_t character)
{
    if ((character >= L'a' && character <= L'z') || (character >= L'A' && character <= L'Z')
        || (character >= L'0' && character <= L'9'))
        return true;
    return character != 0 && std::wcschr(L" _-.&()'+,!", character) != NULL;
}

bool IsSafePath(const std::wstring& path)
{
    if (path.empty() || path.size() > kMaxPathLength)
        return false;
    size_t start = 0;
    while (start <= path.size())
    {
        size_t end = path.find(L'/', start);
        if (end == std::wstring::npos)
            end = path.size();
        const std::wstring part = path.substr(start, end - start);
        if (part.empty() || part == L"." || part == L"..")
            return false;
        for (size_t index = 0; index < part.size(); ++index)
            if (!IsSafeCharacter(part[index]))
                return false;
        start = end + 1;
    }
    return true;
}

std::string Ascii(const std::wstring& text)
{
    std::string result;
    for (size_t index = 0; index < text.size(); ++index)
        result += static_cast<char>(text[index]);
    return result;
}

std::wstring Wide(const std::string& text)
{
    return std::wstring(text.begin(), text.end());
}

int OpenFile(const std::string& path, bool write)
{
    return CreateFileA(path.c_str(), write ? GENERIC_WRITE : GENERIC_READ, 0, NULL,
                       write ? CREATE_ALWAYS : OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
}

void HashEntry(Sha1& hash, const std::string& path, long long size)
{
    hash.update(path.c_str(), path.size() + 1);
    unsigned char bytes[8];
    for (int index = 0; index < 8; ++index)
        bytes[index] = static_cast<unsigned char>(static_cast<unsigned long long>(size) >> (56 - index * 8));
    hash.update(bytes, sizeof(bytes));
}

std::vector<PackManifest*>& Manifests()
{
    static std::vector<PackManifest*> manifests;
    return manifests;
}

bool BuildManifest(DLCPack* pack, PackManifest& manifest)
{
    manifest.packId = pack->GetPackId();
    manifest.root = convWstringToString(pack->GetRootPath());
    manifest.name = pack->getLocalisedDisplayName();
    if (manifest.name.empty())
        manifest.name = pack->getName();
    manifest.rootIsFile = false;

    std::vector<unsigned long long> sizes;
    if (!gStorage->ListFiles(manifest.root, manifest.files, sizes))
    {
        const int file = OpenFile(manifest.root, false);
        if (file == INVALID_HANDLE_VALUE)
            return false;
        sizes.assign(1, GetFileSize(file, NULL));
        CloseHandle(file);
        const std::string::size_type slash = manifest.root.find_last_of('/');
        manifest.files.assign(1, slash == std::string::npos ? manifest.root : manifest.root.substr(slash + 1));
        manifest.rootIsFile = true;
    }

    manifest.size = 0;
    Sha1 hash;
    std::vector<unsigned char> buffer(65536);
    for (size_t index = 0; index < manifest.files.size(); ++index)
    {
        if (!IsSafePath(Wide(manifest.files[index])))
        {
            LogInfo("Pack file " + manifest.files[index] + " has a name that can't be sent; the pack won't be offered");
            return false;
        }
        manifest.sizes.push_back(static_cast<long long>(sizes[index]));
        manifest.size += manifest.sizes.back();
        HashEntry(hash, manifest.files[index], manifest.sizes.back());
        const int file = OpenFile(manifest.fullPath(index), false);
        if (file == INVALID_HANDLE_VALUE)
            return false;
        long long remaining = manifest.sizes.back();
        while (remaining > 0)
        {
            const unsigned int want =
                static_cast<unsigned int>(remaining < static_cast<long long>(buffer.size()) ? remaining : buffer.size());
            unsigned int read = 0;
            if (ReadFile(file, &buffer[0], want, &read, NULL) == 0 || read == 0)
            {
                CloseHandle(file);
                return false;
            }
            hash.update(&buffer[0], read);
            remaining -= read;
        }
        CloseHandle(file);
    }
    hash.finish(manifest.hash);
    return manifest.size <= kMaxPackSize;
}

const PackManifest* FindManifest(unsigned int packId)
{
    std::vector<PackManifest*>& manifests = Manifests();
    for (size_t index = 0; index < manifests.size(); ++index)
        if (manifests[index]->packId == packId)
            return manifests[index];
    DLCPack* pack = gApp.mDLCManager.getPackFromPackID(static_cast<int>(packId));
    if (pack == NULL || pack->GetRootPath().empty() || pack->GetSource() == C4JStorage::EPackSource_BuiltIn)
        return NULL;
    PackManifest* manifest = new PackManifest();
    if (!BuildManifest(pack, *manifest))
    {
        delete manifest;
        return NULL;
    }
    manifests.push_back(manifest);
    return manifest;
}

void SendToClient(ServerGamePacketListenerImpl* connection, ByteArrayOutputStream& bytes)
{
    connection->send(boost::shared_ptr<Packet>(
        new ClientboundCustomPayloadPacket(ClientboundCustomPayloadPacket::PACK_PACKET, bytes.toByteArray())));
}

std::wstring Megabytes(long long size)
{
    wchar_t text[32];
    std::swprintf(text, 32, L"%.1f MB", static_cast<double>(size) / (1024.0 * 1024.0));
    return text;
}
}

PackSender::PackSender() : m_manifest(NULL), m_fileIndex(0), m_file(INVALID_HANDLE_VALUE), m_fileRemaining(0)
{
}

PackSender::~PackSender()
{
    stopUpload();
}

void PackSender::offer(ServerGamePacketListenerImpl* connection, unsigned int packId, bool required)
{
    if (packId == 0 || std::find(m_offered.begin(), m_offered.end(), packId) != m_offered.end())
        return;
    const PackManifest* manifest = FindManifest(packId);
    if (manifest == NULL)
        return;
    m_offered.push_back(packId);
    if (required)
        m_awaiting.push_back(std::make_pair(packId, System::processTimeInMilliSecs()));
    LogInfo("Offering " + convWstringToString(manifest->name));
    ByteArrayOutputStream bytes;
    DataOutputStream output(&bytes);
    output.writeByte(eMessage_Offer);
    output.writeInt(static_cast<int>(packId));
    for (int index = 0; index < Sha1::DIGEST_SIZE; ++index)
        output.writeByte(manifest->hash[index]);
    output.writeLong(manifest->size);
    output.writeUTF(manifest->name);
    output.writeBoolean(required);
    SendToClient(connection, bytes);
}

void PackSender::handle(arrayWithLength<unsigned char> data)
{
    if (data.data == NULL || data.length < 5)
        return;
    ByteArrayInputStream byteStream(data);
    DataInputStream input(&byteStream);
    const unsigned char type = input.readByte();
    const unsigned int packId = static_cast<unsigned int>(input.readInt());
    byteStream.reset();
    if (type != eMessage_Request && type != eMessage_Skip)
        return;
    if (std::find(m_offered.begin(), m_offered.end(), packId) == m_offered.end())
        return;
    for (size_t index = 0; index < m_awaiting.size(); ++index)
    {
        if (m_awaiting[index].first == packId)
        {
            m_awaiting.erase(m_awaiting.begin() + index);
            break;
        }
    }
    if (type == eMessage_Skip)
        return;
    if (std::find(m_requested.begin(), m_requested.end(), packId) != m_requested.end())
        return;
    if (m_manifest != NULL && m_manifest->packId == packId)
        return;
    m_requested.push_back(packId);
    LogInfo("Sending a pack to a player who asked for it");
}

bool PackSender::startNextFile(ServerGamePacketListenerImpl* connection)
{
    if (m_file != INVALID_HANDLE_VALUE)
    {
        CloseHandle(m_file);
        m_file = INVALID_HANDLE_VALUE;
    }
    if (m_fileIndex >= m_manifest->files.size())
    {
        ByteArrayOutputStream bytes;
        DataOutputStream output(&bytes);
        output.writeByte(eMessage_Done);
        output.writeInt(static_cast<int>(m_manifest->packId));
        SendToClient(connection, bytes);
        stopUpload();
        return false;
    }
    m_file = OpenFile(m_manifest->fullPath(m_fileIndex), false);
    m_fileRemaining = m_manifest->sizes[m_fileIndex];
    ByteArrayOutputStream bytes;
    DataOutputStream output(&bytes);
    output.writeByte(eMessage_File);
    output.writeInt(static_cast<int>(m_manifest->packId));
    output.writeUTF(Wide(m_manifest->files[m_fileIndex]));
    output.writeLong(m_fileRemaining);
    SendToClient(connection, bytes);
    return true;
}

void PackSender::stopUpload()
{
    if (m_file != INVALID_HANDLE_VALUE)
        CloseHandle(m_file);
    m_file = INVALID_HANDLE_VALUE;
    m_manifest = NULL;
    m_fileIndex = 0;
    m_fileRemaining = 0;
}

bool PackSender::sendRemovedIfUnanswered(ServerGamePacketListenerImpl* connection)
{
    if (m_awaiting.empty())
        return false;
    ByteArrayOutputStream bytes;
    DataOutputStream output(&bytes);
    output.writeByte(eMessage_Removed);
    output.writeInt(static_cast<int>(m_awaiting.front().first));
    SendToClient(connection, bytes);
    const PackManifest* manifest = FindManifest(m_awaiting.front().first);
    LogInfo("Removing a player who didn't answer the download of "
            + (manifest != NULL ? convWstringToString(manifest->name) : std::string("a required pack")));
    return true;
}

bool PackSender::isBusy() const
{
    if (m_manifest != NULL || !m_requested.empty())
        return true;
    const long long now = System::processTimeInMilliSecs();
    for (size_t index = 0; index < m_awaiting.size(); ++index)
    {
        if (now - m_awaiting[index].second < kAnswerWaitMs)
            return true;
    }
    return false;
}

void PackSender::tick(ServerGamePacketListenerImpl* connection)
{
    std::vector<unsigned char> buffer;
    Connection* link = connection->getConnection();
    while (link != NULL && link->getEstimatedRemaining() < kMaxQueuedBytes)
    {
        if (m_manifest == NULL)
        {
            if (m_requested.empty())
                return;
            m_manifest = FindManifest(m_requested.front());
            m_requested.pop_front();
            if (m_manifest == NULL)
                continue;
            m_fileIndex = 0;
            startNextFile(connection);
            continue;
        }
        if (m_fileRemaining <= 0)
        {
            ++m_fileIndex;
            startNextFile(connection);
            continue;
        }

        const unsigned int want =
            static_cast<unsigned int>(m_fileRemaining < kChunkSize ? m_fileRemaining : kChunkSize);
        buffer.resize(5 + want);
        buffer[0] = eMessage_Data;
        const unsigned int packId = m_manifest->packId;
        for (int index = 0; index < 4; ++index)
            buffer[1 + index] = static_cast<unsigned char>(packId >> (24 - index * 8));
        unsigned int read = 0;
        if (m_file == INVALID_HANDLE_VALUE || ReadFile(m_file, &buffer[5], want, &read, NULL) == 0 || read == 0)
        {
            m_fileIndex = m_manifest->files.size();
            m_fileRemaining = 0;
            startNextFile(connection);
            continue;
        }
        arrayWithLength<unsigned char> payload(5 + read, false);
        std::memcpy(payload.data, &buffer[0], 5 + read);
        connection->send(boost::shared_ptr<Packet>(
            new ClientboundCustomPayloadPacket(ClientboundCustomPayloadPacket::PACK_PACKET, payload)));
        m_fileRemaining -= read;
    }
}

PackReceiver& PackReceiver::Get()
{
    static PackReceiver receiver;
    return receiver;
}

PackReceiver::PackReceiver()
    : m_connection(NULL),
      m_prompting(false),
      m_downloading(false),
      m_file(INVALID_HANDLE_VALUE),
      m_fileRemaining(0),
      m_received(0),
      m_releasePending(false)
{
}

void PackReceiver::showMessage(const std::wstring& message)
{
    Minecraft* minecraft = Minecraft::GetInstance();
    if (minecraft != NULL && minecraft->mGui != NULL)
        minecraft->mGui->addMessage(message, gProfile->GetPrimaryPad(), false, false, false, false);
}

void PackReceiver::handle(ClientPacketListener* connection, arrayWithLength<unsigned char> data)
{
    if (data.data == NULL || data.length < 5)
        return;
    m_connection = connection;
    ByteArrayInputStream byteStream(data);
    DataInputStream input(&byteStream);
    const unsigned char type = input.readByte();
    const unsigned int packId = static_cast<unsigned int>(input.readInt());
    Offer offer;
    std::wstring path;
    long long size = 0;
    if (type == eMessage_Offer)
    {
        offer.packId = packId;
        for (int index = 0; index < Sha1::DIGEST_SIZE; ++index)
            offer.hash[index] = input.readByte();
        offer.size = input.readLong();
        offer.name = input.readUTF();
        offer.required = input.readBoolean();
    }
    else if (type == eMessage_File)
    {
        path = input.readUTF();
        size = input.readLong();
    }
    byteStream.reset();
    switch (type)
    {
    case eMessage_Offer:
        handleOffer(offer);
        break;
    case eMessage_File:
        handleFile(packId, path, size);
        break;
    case eMessage_Data:
        handleData(packId, data.data + 5, data.length - 5);
        break;
    case eMessage_Done:
        handleDone(packId);
        break;
    case eMessage_Removed:
    {
        std::wstring name;
        for (size_t index = 0; index < m_prompts.size(); ++index)
        {
            if (m_prompts[index].packId == packId)
                name = m_prompts[index].name;
        }
        m_leaveMessage = name.empty() ? std::wstring(L"You were removed from the game because it needs a pack and "
                                                     L"you didn't answer in time.")
                                      : L"You were removed from the game because it needs " + name
                                            + L" and you didn't answer in time.";
        LogInfo("Removed from the game for not answering a pack download in time");
        break;
    }
    default:
        break;
    }
}

bool PackReceiver::isKnown(unsigned int packId)
{
    if (gApp.mDLCManager.getPackFromPackID(static_cast<int>(packId)) != NULL)
        return true;
    if (std::find(m_loading.begin(), m_loading.end(), packId) != m_loading.end())
        return true;
    for (size_t index = 0; index < m_toLoad.size(); ++index)
        if (m_toLoad[index].packId == packId)
            return true;
    for (size_t index = 0; index < m_prompts.size(); ++index)
        if (m_prompts[index].packId == packId)
            return true;
    for (size_t index = 0; index < m_downloads.size(); ++index)
        if (m_downloads[index].packId == packId)
            return true;
    return false;
}

void PackReceiver::reply(unsigned char type, unsigned int packId)
{
    if (m_connection == NULL)
        return;
    ByteArrayOutputStream bytes;
    DataOutputStream output(&bytes);
    output.writeByte(type);
    output.writeInt(static_cast<int>(packId));
    m_connection->send(boost::shared_ptr<Packet>(
        new ServerboundCustomPayloadPacket(ServerboundCustomPayloadPacket::PACK_PACKET, bytes.toByteArray())));
}

void PackReceiver::handleOffer(const Offer& offer)
{
    if (isKnown(offer.packId))
    {
        reply(eMessage_Skip, offer.packId);
        return;
    }
    if (gStorage->GetServerPacksFolder().empty() || offer.size < 0 || offer.size > kMaxPackSize)
    {
        if (offer.required)
            leave(offer, L"there is nowhere to keep it");
        else
            reply(eMessage_Skip, offer.packId);
        return;
    }
    m_prompts.push_back(offer);
    askNext();
}

void PackReceiver::askNext()
{
    if (m_prompting || m_prompts.empty() || !gApp.GetGameStarted() || !CGameNetworkManager::sInstance.IsInGameplay()
        || gConsoleUIController.IsSceneInStack(0xff, EUIScene_FullscreenProgress)
        || gConsoleUIController.IsExpectingOrReloadingSkin())
        return;
    m_prompting = true;
    const Offer& offer = m_prompts.front();
    std::wstring name;
    for (size_t index = 0; index < offer.name.size(); ++index)
    {
        name += offer.name[index];
        if (offer.name[index] == L'%')
            name += L'%';
    }
    if (offer.required)
        m_promptText = L"This game uses maps from " + name + L", which you don't have. Download it to keep playing? ("
                       + Megabytes(offer.size) + L")";
    else
        m_promptText = L"This world uses " + name + L", which you don't have. Download it? (" + Megabytes(offer.size)
                       + L")";
    static int parameters[2] = {static_cast<int>(IDS_CONFIRM_OK), static_cast<int>(IDS_CONFIRM_CANCEL)};
    gConsoleUIController.RequestAlertMessage(offer.required ? IDS_MINI_GAMES : IDS_DLC_TEXTUREPACK_NOT_PRESENT_TITLE,
                                             IDS_DLC_TEXTUREPACK_NOT_PRESENT, parameters, 2, gProfile->GetPrimaryPad(),
                                             PromptReturned, this, const_cast<wchar_t*>(m_promptText.c_str()), true);
}

int PackReceiver::PromptReturned(void* context, int, C4JStorage::EMessageResult result)
{
    PackReceiver* receiver = static_cast<PackReceiver*>(context);
    receiver->m_prompting = false;
    if (receiver->m_prompts.empty())
        return 0;
    const Offer offer = receiver->m_prompts.front();
    receiver->m_prompts.pop_front();
    if (result != C4JStorage::EMessageResult_Accept)
    {
        if (offer.required)
            receiver->leave(offer, L"you chose not to download it");
        else
            receiver->reply(eMessage_Skip, offer.packId);
    }
    else if (receiver->m_connection != NULL)
    {
        receiver->reply(eMessage_Request, offer.packId);
        receiver->m_downloads.push_back(offer);
        receiver->showMessage(L"Downloading " + offer.name + L" (" + Megabytes(offer.size) + L")");
    }
    receiver->askNext();
    return 0;
}

void PackReceiver::leave(const Offer& offer, const wchar_t* reason)
{
    LogInfo("Leaving the game without " + convWstringToString(offer.name) + ", which it needs: "
            + convWstringToString(reason));
    m_leaveMessage = L"You left the game because it needs " + offer.name + L" and " + reason + L".";
    if (m_file != INVALID_HANDLE_VALUE)
        CloseHandle(m_file);
    m_file = INVALID_HANDLE_VALUE;
    if (m_downloading)
        gStorage->DeletePath(m_partFolder);
    m_downloading = false;
    m_fileRemaining = 0;
    m_prompts.clear();
    m_downloads.clear();
    gApp.SetAction(gProfile->GetPrimaryPad(), eXuiAction_ExitWorld, NULL);
}

bool PackReceiver::startDownload(unsigned int packId)
{
    if (m_downloads.empty() || m_downloads.front().packId != packId)
        return false;
    const Offer& offer = m_downloads.front();
    const std::string part = gStorage->GetServerPacksFolder() + "/" + Sha1::toHex(offer.hash) + ".part";
    gStorage->DeletePath(part);
    if (!gStorage->CreateDirectories(part))
    {
        abortDownload(L"a folder could not be made");
        return false;
    }
    m_downloading = true;
    m_hash = Sha1();
    m_partFolder = part;
    m_received = 0;
    m_fileRemaining = 0;
    return true;
}

void PackReceiver::abortDownload(const wchar_t* reason)
{
    if (m_file != INVALID_HANDLE_VALUE)
        CloseHandle(m_file);
    m_file = INVALID_HANDLE_VALUE;
    m_fileRemaining = 0;
    if (m_downloading)
        gStorage->DeletePath(m_partFolder);
    m_downloading = false;
    if (m_downloads.empty())
        return;
    const Offer offer = m_downloads.front();
    m_downloads.pop_front();
    showMessage(offer.name + L" could not be downloaded: " + reason);
    LogInfo("A pack download failed: " + convWstringToString(reason));
    if (offer.required)
        leave(offer, reason);
}

void PackReceiver::handleFile(unsigned int packId, const std::wstring& path, long long size)
{
    if (!m_downloading && !startDownload(packId))
        return;
    if (m_downloads.front().packId != packId)
        return;
    if (m_fileRemaining != 0)
    {
        abortDownload(L"a file ended early");
        return;
    }
    if (m_file != INVALID_HANDLE_VALUE)
    {
        CloseHandle(m_file);
        m_file = INVALID_HANDLE_VALUE;
    }
    if (!IsSafePath(path) || size < 0 || m_received + size > m_downloads.front().size)
    {
        abortDownload(L"the server sent a file it shouldn't");
        return;
    }
    const std::string relative = Ascii(path);
    const std::string full = m_partFolder + "/" + relative;
    if (!gStorage->CreateDirectories(full.substr(0, full.find_last_of('/'))))
    {
        abortDownload(L"a folder could not be made");
        return;
    }
    m_file = OpenFile(full, true);
    if (m_file == INVALID_HANDLE_VALUE)
    {
        abortDownload(L"a file could not be written");
        return;
    }
    HashEntry(m_hash, relative, size);
    m_fileRemaining = size;
}

void PackReceiver::handleData(unsigned int packId, const unsigned char* data, unsigned int length)
{
    if (!m_downloading || m_downloads.front().packId != packId)
        return;
    if (m_file == INVALID_HANDLE_VALUE || static_cast<long long>(length) > m_fileRemaining)
    {
        abortDownload(L"the server sent more than it said");
        return;
    }
    unsigned int written = 0;
    if (WriteFile(m_file, data, length, &written, NULL) == 0 || written != length)
    {
        abortDownload(L"a file could not be written");
        return;
    }
    m_hash.update(data, length);
    m_fileRemaining -= length;
    m_received += length;
}

void PackReceiver::handleDone(unsigned int packId)
{
    if (!m_downloading && !startDownload(packId))
        return;
    if (m_downloads.front().packId != packId)
        return;
    if (m_file != INVALID_HANDLE_VALUE)
    {
        CloseHandle(m_file);
        m_file = INVALID_HANDLE_VALUE;
    }
    const Offer offer = m_downloads.front();
    unsigned char digest[Sha1::DIGEST_SIZE];
    m_hash.finish(digest);
    if (m_fileRemaining != 0 || m_received != offer.size || std::memcmp(digest, offer.hash, Sha1::DIGEST_SIZE) != 0)
    {
        abortDownload(L"what arrived doesn't match what the server offered");
        return;
    }
    const std::string folder = gStorage->GetServerPacksFolder() + "/" + Sha1::toHex(digest);
    gStorage->DeletePath(folder);
    if (!gStorage->RenamePath(m_partFolder, folder))
    {
        abortDownload(L"it could not be saved");
        return;
    }
    m_downloading = false;
    m_downloads.pop_front();
    queueLoad(offer, folder);
}

void PackReceiver::queueLoad(const Offer& offer, const std::string& folder)
{
    Download download;
    download.packId = offer.packId;
    download.folder = folder;
    download.name = offer.name;
    m_toLoad.push_back(download);
}

void PackReceiver::tick(ClientPacketListener* connection)
{
    m_connection = connection;
    askNext();
    while (!m_toLoad.empty())
    {
        const Download& download = m_toLoad.front();
        std::wstring name = download.name;
        if (name.empty() || gApp.mDLCManager.getPack(name) != NULL)
            name = Wide(download.folder.substr(download.folder.find_last_of('/') + 1));
        const int result = gApp.LoadPack(download.folder, name);
        if (result == 0)
            break;
        if (result > 0)
            m_loading.push_back(download.packId);
        m_toLoad.erase(m_toLoad.begin());
    }
    if (m_loading.empty() || gApp.DLCInstallProcessCompleted() == 0)
        return;
    for (size_t index = 0; index < m_loading.size(); ++index)
    {
        const unsigned int packId = m_loading[index];
        DLCPack* pack = gApp.mDLCManager.getPackFromPackID(static_cast<int>(packId));
        if (pack == NULL)
            continue;
        std::wstring name = pack->getLocalisedDisplayName();
        if (name.empty())
            name = pack->getName();
        showMessage(name + L" is ready");
        if ((gApp.GetRequiredTexturePackID() & 0x00ffffffu) == (packId & 0x00ffffffu))
            Minecraft::GetInstance()->mTexturePackRepository->selectTexturePackById(packId & 0x00ffffffu, false);
    }
    m_loading.clear();
    connection->applyNextGameMode();
}

void PackReceiver::disconnected(ClientPacketListener* connection)
{
    if (m_connection != connection)
        return;
    m_connection = NULL;
    if (m_file != INVALID_HANDLE_VALUE)
        CloseHandle(m_file);
    m_file = INVALID_HANDLE_VALUE;
    if (m_downloading)
        gStorage->DeletePath(m_partFolder);
    m_downloading = false;
    m_fileRemaining = 0;
    m_downloads.clear();
    m_prompts.clear();
    m_prompting = false;
    m_toLoad.clear();
    m_loading.clear();
    m_releasePending = true;
}

void PackReceiver::showLeaveMessage()
{
    if (m_leaveMessage.empty() || m_connection != NULL)
        return;
    m_leaveText.clear();
    for (size_t index = 0; index < m_leaveMessage.size(); ++index)
    {
        m_leaveText += m_leaveMessage[index];
        if (m_leaveMessage[index] == L'%')
            m_leaveText += L'%';
    }
    m_leaveMessage.clear();
    static int parameters[1] = {static_cast<int>(IDS_CONFIRM_OK)};
    gConsoleUIController.RequestAlertMessage(IDS_MINI_GAMES, IDS_DLC_TEXTUREPACK_NOT_PRESENT, parameters, 1,
                                             gProfile->GetPrimaryPad(), NULL, NULL,
                                             const_cast<wchar_t*>(m_leaveText.c_str()), true);
}

void PackReceiver::releasePacks()
{
    if (m_releasePending && m_connection == NULL && gApp.UnloadServerPacks())
        m_releasePending = false;
}
