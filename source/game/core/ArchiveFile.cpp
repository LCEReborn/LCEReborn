#include "game/core/ArchiveFile.hpp"
#include <cstring>
#include "game/core/ByteArrayInputStream.hpp"
#include "game/core/Compression.hpp"
#include "game/core/CConsoleMinecraftApp.hpp"
#include "game/core/DataInputStream.hpp"
#include "game/core/FileInputStream.hpp"
#include "platform/FileSystem.hpp"
#include "platform/FileApi.hpp"
#include "platform/Runtime.hpp"

char* wstringtofilename(const std::wstring& path, char (&filename)[256]);

ArchiveFile::ArchiveFile(File file)
{
    m_cachedData = false;
    m_data = NULL;
    File::operator=(file);
    if (!file.exists())
        gApp.FatalLoadError(L"<ArchiveFile@%d> Failed to load archive file", 0x54);
    FileInputStream input(file);
    DataInputStream stream(&input);
    _readHeader(&stream);
    stream.close();
    input.close();
}

ArchiveFile::ArchiveFile(unsigned char* data, unsigned int size)
{
    m_data = data;
    m_cachedData = true;
    arrayWithLength<unsigned char> bytes;
    bytes.data = data;
    bytes.length = size;
    ByteArrayInputStream input(bytes);
    DataInputStream stream((InputStream*)&input);
    _readHeader(&stream);
    stream.close();
    input.reset();
}

ArchiveFile::~ArchiveFile()
{
    delete m_data;
    boost::unordered_map<std::wstring, _MetaData*>::iterator iterator = m_files.begin();
    while (iterator != m_files.end())
    {
        delete iterator->second;
        ++iterator;
    }
}

void ArchiveFile::_readHeader(DataInputStream* stream)
{
    int count = stream->readInt();
    for (int i = 0; i < count; ++i)
    {
        _MetaData* metadata = new _MetaData();
        metadata->name = stream->readUTF();
        metadata->offset = static_cast<unsigned int>(stream->readInt());
        metadata->size = static_cast<unsigned int>(stream->readInt());
        if (metadata->name[0] == L'*')
        {
            metadata->name = metadata->name.substr(1);
            metadata->compressed = true;
        }
        else
        {
            metadata->compressed = false;
        }
        m_files.insert(std::make_pair(metadata->name, metadata));
    }
}

bool ArchiveFile::hasFile(const std::wstring& name)
{
    return m_files.find(name) != m_files.end();
}

arrayWithLength<unsigned char> ArchiveFile::getFile(const std::wstring& name)
{
    arrayWithLength<unsigned char> result;
    boost::unordered_map<std::wstring, _MetaData*>::iterator found = m_files.find(name);
    if (found == m_files.end())
    {
        gApp.FatalLoadError(L"<ArchiveFile@%d> Failed to find %ls in %ls", 0xB1, name.c_str(), getPath().c_str());
        return result;
    }
    _MetaData* metadata = found->second;
    if (m_cachedData)
    {
        result = arrayWithLength<unsigned char>(metadata->size, true);
        std::memcpy(result.data, m_data + metadata->offset, metadata->size);
    }
    else
    {
        char filename[256];
        int file = CreateFileA(wstringtofilename(getPath(), filename), GENERIC_READ, 0, NULL, OPEN_EXISTING,
                               FILE_ATTRIBUTE_NORMAL, 0);
        if (file != INVALID_HANDLE_VALUE)
        {
            if (SetFilePointer(file, metadata->offset, NULL, 0) != 0)
            {
                unsigned char* data = new unsigned char[metadata->size];
                unsigned int bytesRead = 0;
                if (ReadFile(file, data, metadata->size, &bytesRead, NULL) == 0)
                {
                    gApp.FatalLoadError(L"<ArchiveFile@%d> Failed to read %ls from %ls", 0xEE, name.c_str(),
                                        getPath().c_str());
                }
                result.data = data;
                result.length = metadata->size;
                CloseHandle(file);
            }
            else
            {
                gApp.FatalLoadError(L"<ArchiveFile@%d> Failed to set file pointer %ls", 0xF5, name.c_str());
                CloseHandle(file);
            }
        }
        else
        {
            gApp.FatalLoadError(L"<ArchiveFile@%d> Failed to load archive %ls to find %ls", 0xFD, getPath().c_str(),
                                name.c_str());
        }
    }
    if (metadata->compressed && result.data != NULL)
    {
        ByteArrayInputStream input(result);
        DataInputStream stream((InputStream*)&input);
        unsigned int decompressedSize = static_cast<unsigned int>(stream.readInt());
        stream.close();
        input.reset();
        unsigned char* decompressed = new unsigned char[decompressedSize];
        Compression::getCompression()->Decompress(decompressed, &decompressedSize, result.data + 4, result.length - 4);
        delete[] result.data;
        result.data = decompressed;
        result.length = decompressedSize;
    }
    return result;
}

unsigned int ArchiveFile::getFileSize(const std::wstring& name)
{
    if (!hasFile(name))
        return static_cast<unsigned int>(-1);
    return m_files.at(name)->size;
}

std::vector<std::wstring> ArchiveFile::getFileList()
{
    std::vector<std::wstring> result;
    boost::unordered_map<std::wstring, _MetaData*>::const_iterator iterator = m_files.begin();
    while (iterator != m_files.end())
    {
        result.push_back(iterator->first);
        ++iterator;
    }
    return result;
}

void ArchiveFile::dereferenceCachedData()
{
    if (m_cachedData && m_data != NULL)
        delete[] m_data;
    m_data = NULL;
    m_cachedData = false;
}
