#include "Skins.hpp"

#include <QDataStream>
#include <QDirIterator>
#include <QFile>
#include <QPainter>

#include "LauncherPaths.hpp"
#include "Profiles.hpp"

namespace
{
const quint16 kOptionsVersion = 0x22;
const qint64 kSkinIdOffset = 0x4e;
const quint32 kDlcSkinBit = 0x80000000u;
const quint32 kUgcSkinMask = 0x7fffffe0u;
const quint32 kDefaultSkinMask = 0x1fu;

quint32 SelectedSkinId(const Profile& profile)
{
    QFile file(ProfileStore::GameDirFor(profile) + "/saves/options/options_" + profile.uuid + ".dat");
    if (!file.open(QIODevice::ReadOnly) || file.size() < kSkinIdOffset + 4)
        return 0;
    const QByteArray data = file.read(kSkinIdOffset + 4);
    QDataStream stream(data);
    stream.setByteOrder(QDataStream::LittleEndian);
    quint16 version = 0;
    stream >> version;
    if (version != kOptionsVersion)
        return 0;
    stream.skipRawData(static_cast<int>(kSkinIdOffset - 2));
    quint32 skinId = 0;
    stream >> skinId;
    return stream.status() == QDataStream::Ok ? skinId : 0;
}

QString DefaultSkinPath(quint32 index)
{
    const QString titleUpdate = LauncherPaths::AssetsDir() + "/Common/res/TitleUpdate/res/mob/";
    if (index >= 2 && index <= 8)
        return LauncherPaths::AssetsDir() + QString("/Common/res/mob/char%1.png").arg(index - 1);
    if (index == 9)
        return titleUpdate + "alex.png";
    if (index >= 10 && index <= 16)
        return titleUpdate + QString("alex%1.png").arg(index - 9);
    if (index == 17)
        return titleUpdate + "DevAlex.png";
    if (index == 18)
        return titleUpdate + "DevSteve.png";
    return titleUpdate + "steve.png";
}

QString ReadUtf16(QDataStream& stream, quint32 length)
{
    QString text;
    text.reserve(static_cast<int>(length));
    for (quint32 index = 0; index < length && stream.status() == QDataStream::Ok; ++index)
    {
        quint16 unit = 0;
        stream >> unit;
        text.append(QChar(unit));
    }
    return text;
}

QByteArray FindPackItem(const QString& packPath, const QString& itemName)
{
    QFile file(packPath);
    if (!file.open(QIODevice::ReadOnly))
        return QByteArray();
    QDataStream stream(&file);
    stream.setByteOrder(QDataStream::BigEndian);

    quint32 version = 0;
    quint32 parameterCount = 0;
    stream >> version >> parameterCount;
    if (version < 3 || parameterCount > 4096)
        return QByteArray();
    bool hasXmlVersion = false;
    for (quint32 index = 0; index < parameterCount; ++index)
    {
        quint32 id = 0;
        quint32 length = 0;
        stream >> id >> length;
        if (length > 4096)
            return QByteArray();
        if (ReadUtf16(stream, length) == "XMLVERSION")
            hasXmlVersion = true;
        stream.skipRawData(4);
    }
    if (hasXmlVersion)
        stream.skipRawData(4);

    quint32 itemCount = 0;
    stream >> itemCount;
    if (itemCount > 65536 || stream.status() != QDataStream::Ok)
        return QByteArray();
    struct Item
    {
        quint32 size;
        quint32 type;
        QString name;
    };
    QVector<Item> items;
    items.reserve(static_cast<int>(itemCount));
    for (quint32 index = 0; index < itemCount; ++index)
    {
        Item item;
        quint32 nameLength = 0;
        stream >> item.size >> item.type >> nameLength;
        if (nameLength > 4096)
            return QByteArray();
        item.name = ReadUtf16(stream, nameLength);
        stream.skipRawData(4);
        items.append(item);
    }
    for (const Item& item : items)
    {
        quint32 parameters = 0;
        stream >> parameters;
        if (parameters > 4096)
            return QByteArray();
        for (quint32 index = 0; index < parameters; ++index)
        {
            quint32 id = 0;
            quint32 length = 0;
            stream >> id >> length;
            if (length > 65536)
                return QByteArray();
            stream.skipRawData(static_cast<int>(length * 2 + 4));
        }
        if (stream.status() != QDataStream::Ok)
            return QByteArray();
        if (item.type == 0 && item.name.compare(itemName, Qt::CaseInsensitive) == 0)
        {
            QByteArray data(static_cast<int>(item.size), Qt::Uninitialized);
            if (stream.readRawData(data.data(), data.size()) != data.size())
                return QByteArray();
            return data;
        }
        stream.skipRawData(static_cast<int>(item.size));
    }
    return QByteArray();
}

QImage DlcSkin(quint32 skinId)
{
    const QString name = QString("dlcskin%1.png").arg(skinId & ~kDlcSkinBit, 8, 10, QChar('0'));
    for (const QString& root : {QString("/WiiU/DLC"), QString("/WiiU/CU/DLC")})
    {
        QDirIterator packs(LauncherPaths::AssetsDir() + root, {"*.pck"}, QDir::Files, QDirIterator::Subdirectories);
        while (packs.hasNext())
        {
            const QByteArray data = FindPackItem(packs.next(), name);
            if (!data.isEmpty())
            {
                const QImage image = QImage::fromData(data, "PNG");
                if (!image.isNull())
                    return image;
            }
        }
    }
    return QImage();
}
}

namespace Skins
{
QImage SkinFor(const Profile& profile)
{
    const quint32 skinId = SelectedSkinId(profile);
    if ((skinId & kDlcSkinBit) != 0)
    {
        const QImage skin = DlcSkin(skinId);
        if (!skin.isNull())
            return skin;
    }
    const quint32 index = (skinId & kDlcSkinBit) == 0 && (skinId & kUgcSkinMask) == 0 ? (skinId & kDefaultSkinMask) : 0;
    return QImage(DefaultSkinPath(index == 0 ? 1 : index));
}

QImage Face(const QImage& skin)
{
    if (skin.width() < 64 || skin.height() < 16)
        return QImage();
    QImage face = skin.copy(8, 8, 8, 8).convertToFormat(QImage::Format_ARGB32);
    const QImage hat = skin.copy(40, 8, 8, 8).convertToFormat(QImage::Format_ARGB32);
    bool hatHasTransparency = false;
    for (int y = 0; y < 8 && !hatHasTransparency; ++y)
    {
        for (int x = 0; x < 8; ++x)
        {
            if (qAlpha(hat.pixel(x, y)) < 255)
            {
                hatHasTransparency = true;
                break;
            }
        }
    }
    if (hatHasTransparency)
    {
        QPainter painter(&face);
        painter.drawImage(0, 0, hat);
    }
    return face;
}
}
