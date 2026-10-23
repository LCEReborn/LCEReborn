#ifndef GAME_NETWORK_PACKTRANSFER_HPP
#define GAME_NETWORK_PACKTRANSFER_HPP

#include <deque>
#include <string>
#include <vector>

#include "game/core/Sha1.hpp"
#include "game/world/biome/arrayWithLength.hpp"
#include "platform/C4JStorage.hpp"

class ClientPacketListener;
class ServerGamePacketListenerImpl;
struct PackManifest;

class PackSender
{
public:
    PackSender();
    ~PackSender();

    void offer(ServerGamePacketListenerImpl* connection, unsigned int packId, bool required);
    void handle(arrayWithLength<unsigned char> data);
    void tick(ServerGamePacketListenerImpl* connection);
    bool isBusy() const;
    bool sendRemovedIfUnanswered(ServerGamePacketListenerImpl* connection);

private:
    bool startNextFile(ServerGamePacketListenerImpl* connection);
    void stopUpload();

    std::vector<unsigned int> m_offered;
    std::vector<std::pair<unsigned int, long long> > m_awaiting;
    std::deque<unsigned int> m_requested;
    const PackManifest* m_manifest;
    size_t m_fileIndex;
    int m_file;
    long long m_fileRemaining;
};

class PackReceiver
{
public:
    static PackReceiver& Get();

    void handle(ClientPacketListener* connection, arrayWithLength<unsigned char> data);
    void tick(ClientPacketListener* connection);
    void disconnected(ClientPacketListener* connection);
    void releasePacks();
    void showLeaveMessage();

private:
    struct Offer
    {
        unsigned int packId;
        unsigned char hash[Sha1::DIGEST_SIZE];
        long long size;
        std::wstring name;
        bool required;
    };

    struct Download
    {
        unsigned int packId;
        std::string folder;
        std::wstring name;
    };

    PackReceiver();

    bool isKnown(unsigned int packId);
    void handleOffer(const Offer& offer);
    void handleFile(unsigned int packId, const std::wstring& path, long long size);
    void handleData(unsigned int packId, const unsigned char* data, unsigned int length);
    void handleDone(unsigned int packId);
    bool startDownload(unsigned int packId);
    void abortDownload(const wchar_t* reason);
    void leave(const Offer& offer, const wchar_t* reason);
    void showMessage(const std::wstring& message);
    void askNext();
    void queueLoad(const Offer& offer, const std::string& folder);
    void reply(unsigned char type, unsigned int packId);
    static int PromptReturned(void* context, int pad, C4JStorage::EMessageResult result);

    ClientPacketListener* m_connection;
    std::deque<Offer> m_prompts;
    bool m_prompting;
    std::wstring m_promptText;
    std::deque<Offer> m_downloads;
    bool m_downloading;
    Sha1 m_hash;
    std::string m_partFolder;
    int m_file;
    long long m_fileRemaining;
    long long m_received;
    std::vector<Download> m_toLoad;
    std::vector<unsigned int> m_loading;
    bool m_releasePending;
    std::wstring m_leaveMessage;
    std::wstring m_leaveText;
};

#endif
