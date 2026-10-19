#include "GameArt.hpp"

#include <QFile>
#include <QFontDatabase>
#include <QtEndian>

#include <cstring>

#include "LauncherPaths.hpp"

namespace
{
const char* const kSkinArchiveEntry = "skinWiiU.fui";
const char* const kPanoramaSymbol = "Panorama_Background_S";
const int kFuiHeaderBytes = 152;
const int kFuiCountsOffset = 76;
const int kFuiSymbolBytes = 0x48;
const int kFuiBitmapBytes = 0x20;
const int kFuiObjectTypeBitmap = 3;
const int kFuiBitmapJpeg = 6;
const int kFuiBitmapJpegWithAlpha = 8;

enum FuiCount
{
    TimelineCount,
    TimelineEventNameCount,
    TimelineActionCount,
    ShapeCount,
    ShapeComponentCount,
    VertCount,
    TimelineFrameCount,
    TimelineEventCount,
    ReferenceCount,
    EdittextCount,
    SymbolCount,
    BitmapCount,
    FontCount,
    FontNameCount,
    ImportAssetCount,
    FuiCountTotal
};

struct Region
{
    FuiCount count;
    int stride;
};

const Region kRegionsBeforeSymbols[] = {
    {TimelineCount, 0x1c},   {TimelineActionCount, 0x84}, {ShapeCount, 0x1c},
    {ShapeComponentCount, 0x2c}, {VertCount, 8},          {TimelineFrameCount, 0x48},
    {TimelineEventCount, 0x48}, {TimelineEventNameCount, 0x40}, {ReferenceCount, 0x48},
    {EdittextCount, 0x138},  {FontNameCount, 0x104},
};

struct Art
{
    bool loaded = false;
    QString fontFamily;
    QImage panorama;
    QImage terrain;
};

Art& Cache()
{
    static Art art;
    return art;
}

QByteArray ArchiveEntry(const QString& archivePath, const QByteArray& wanted)
{
    QFile file(archivePath);
    if (!file.open(QIODevice::ReadOnly))
        return QByteArray();
    const QByteArray countBytes = file.read(4);
    if (countBytes.size() != 4)
        return QByteArray();
    const qint32 count = qFromBigEndian<qint32>(countBytes.constData());
    for (qint32 index = 0; index < count; ++index)
    {
        const QByteArray lengthBytes = file.read(2);
        if (lengthBytes.size() != 2)
            return QByteArray();
        const quint16 length = qFromBigEndian<quint16>(lengthBytes.constData());
        const QByteArray name = file.read(length);
        const QByteArray location = file.read(8);
        if (name.size() != length || location.size() != 8)
            return QByteArray();
        if (name != wanted)
            continue;
        const qint64 offset = qFromBigEndian<quint32>(location.constData());
        const qint64 size = qFromBigEndian<quint32>(location.constData() + 4);
        if (!file.seek(offset))
            return QByteArray();
        const QByteArray data = file.read(size);
        return data.size() == size ? data : QByteArray();
    }
    return QByteArray();
}

quint32 ReadLE32(const QByteArray& data, qint64 offset)
{
    if (offset < 0 || offset + 4 > data.size())
        return 0;
    return qFromLittleEndian<quint32>(data.constData() + offset);
}

QImage FuiBitmap(const QByteArray& fui, const char* symbolName)
{
    if (fui.size() < kFuiHeaderBytes)
        return QImage();
    quint32 counts[FuiCountTotal];
    for (int index = 0; index < FuiCountTotal; ++index)
        counts[index] = ReadLE32(fui, kFuiCountsOffset + index * 4);

    qint64 cursor = kFuiHeaderBytes;
    for (const Region& region : kRegionsBeforeSymbols)
        cursor += static_cast<qint64>(counts[region.count]) * region.stride;
    const qint64 symbols = cursor;
    const qint64 bitmaps = symbols + static_cast<qint64>(counts[SymbolCount]) * kFuiSymbolBytes
                           + static_cast<qint64>(counts[ImportAssetCount]) * 0x40;
    const qint64 imageData = bitmaps + static_cast<qint64>(counts[BitmapCount]) * kFuiBitmapBytes;
    if (imageData > fui.size())
        return QImage();

    for (quint32 index = 0; index < counts[SymbolCount]; ++index)
    {
        const qint64 symbol = symbols + static_cast<qint64>(index) * kFuiSymbolBytes;
        const char* name = fui.constData() + symbol;
        if (qstrnlen(name, 64) == 64 || std::strcmp(name, symbolName) != 0)
            continue;
        if (ReadLE32(fui, symbol + 64) != kFuiObjectTypeBitmap)
            return QImage();
        const quint32 bitmapIndex = ReadLE32(fui, symbol + 68);
        if (bitmapIndex >= counts[BitmapCount])
            return QImage();
        const qint64 bitmap = bitmaps + static_cast<qint64>(bitmapIndex) * kFuiBitmapBytes;
        const quint32 type = ReadLE32(fui, bitmap + 4);
        const quint32 offset = ReadLE32(fui, bitmap + 16);
        quint32 size = ReadLE32(fui, bitmap + 20);
        const quint32 jpegSize = ReadLE32(fui, bitmap + 24);
        if (type == kFuiBitmapJpegWithAlpha && jpegSize != 0)
            size = jpegSize;
        if (imageData + offset + size > fui.size())
            return QImage();
        const QByteArray bytes = fui.mid(imageData + offset, size);
        const char* format = type >= kFuiBitmapJpeg ? "JPG" : "PNG";
        return QImage::fromData(bytes, format);
    }
    return QImage();
}

void Load()
{
    Art& art = Cache();
    if (art.loaded)
        return;
    art.loaded = true;

    const QString assets = LauncherPaths::AssetsDir();
    const int font = QFontDatabase::addApplicationFont(assets + "/Common/Media/font/Mojangles.ttf");
    if (font >= 0)
    {
        const QStringList families = QFontDatabase::applicationFontFamilies(font);
        if (!families.isEmpty())
            art.fontFamily = families.first();
    }

    const QByteArray skin = ArchiveEntry(LauncherPaths::ContentCheckFile(), kSkinArchiveEntry);
    if (!skin.isEmpty())
        art.panorama = FuiBitmap(skin, kPanoramaSymbol).convertToFormat(QImage::Format_RGB32);

    art.terrain = QImage(assets + "/Common/res/TitleUpdate/res/terrain.png").convertToFormat(QImage::Format_ARGB32);
}
}

namespace GameArt
{
void Reload()
{
    Cache() = Art();
    Load();
}

QString FontFamily()
{
    Load();
    return Cache().fontFamily;
}

QImage Panorama()
{
    Load();
    return Cache().panorama;
}

QImage Terrain()
{
    Load();
    return Cache().terrain;
}
}
