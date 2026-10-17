#include "Skins.hpp"

#include <QBuffer>
#include <QDataStream>
#include <QDirIterator>
#include <QFile>
#include <QHash>
#include <QPainter>
#include <QPolygonF>

#include <algorithm>
#include <cmath>
#include <limits>

#include "LauncherPaths.hpp"
#include "Profiles.hpp"

namespace
{
const quint16 kOptionsVersion = 0x22;
const qint64 kSkinIdOffset = 0x4e;
const quint32 kDlcSkinBit = 0x80000000u;
const quint32 kUgcSkinMask = 0x7fffffe0u;
const quint32 kDefaultSkinMask = 0x1fu;
const quint32 kNestedPackType = 11;
const quint32 kHeadHiddenBit = 1u << 10;
const quint32 kHatHiddenBit = 1u << 16;

struct Box
{
    float x;
    float y;
    float z;
    float width;
    float height;
    float depth;
    float u;
    float v;
    float inflate;
    bool mirror;
};

struct Skin
{
    QImage texture;
    QVector<Box> headBoxes;
    QVector<Box> bodyBoxes;
    quint32 animFlags = 0;
};

struct PackItem
{
    QByteArray data;
    QVector<QPair<QString, QString>> parameters;
};

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

bool FindPackItem(QIODevice& device, const QString& itemName, int depth, PackItem* found)
{
    QDataStream stream(&device);
    stream.setByteOrder(QDataStream::BigEndian);

    quint32 version = 0;
    quint32 keyCount = 0;
    stream >> version >> keyCount;
    if (version < 3 || keyCount > 4096)
        return false;
    QHash<quint32, QString> keys;
    for (quint32 index = 0; index < keyCount; ++index)
    {
        quint32 id = 0;
        quint32 length = 0;
        stream >> id >> length;
        if (length > 4096)
            return false;
        keys.insert(id, ReadUtf16(stream, length));
        stream.skipRawData(4);
    }
    if (keys.values().contains("XMLVERSION"))
        stream.skipRawData(4);

    quint32 itemCount = 0;
    stream >> itemCount;
    if (itemCount > 65536 || stream.status() != QDataStream::Ok)
        return false;
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
            return false;
        item.name = ReadUtf16(stream, nameLength);
        stream.skipRawData(4);
        items.append(item);
    }
    for (const Item& item : items)
    {
        quint32 parameterCount = 0;
        stream >> parameterCount;
        if (parameterCount > 4096)
            return false;
        const bool wanted =
            item.type == 0 && item.name.section('/', -1).compare(itemName, Qt::CaseInsensitive) == 0;
        QVector<QPair<QString, QString>> parameters;
        for (quint32 index = 0; index < parameterCount; ++index)
        {
            quint32 id = 0;
            quint32 length = 0;
            stream >> id >> length;
            if (length > 65536)
                return false;
            if (wanted)
                parameters.append({keys.value(id), ReadUtf16(stream, length)});
            else
                stream.skipRawData(static_cast<int>(length * 2));
            stream.skipRawData(4);
        }
        if (stream.status() != QDataStream::Ok)
            return false;

        if (wanted || (item.type == kNestedPackType && depth < 2))
        {
            QByteArray data(static_cast<int>(item.size), Qt::Uninitialized);
            if (stream.readRawData(data.data(), data.size()) != data.size())
                return false;
            if (wanted)
            {
                found->data = data;
                found->parameters = parameters;
                return true;
            }
            QBuffer nested(&data);
            nested.open(QIODevice::ReadOnly);
            if (FindPackItem(nested, itemName, depth + 1, found))
                return true;
            continue;
        }
        stream.skipRawData(static_cast<int>(item.size));
    }
    return false;
}

bool ParseBox(const QString& text, Box* box, QString* part)
{
    const QStringList fields = text.simplified().split(' ');
    if (fields.size() < 9)
        return false;
    float values[11] = {};
    for (int index = 0; index < 11 && index + 1 < fields.size(); ++index)
    {
        bool ok = false;
        values[index] = fields[index + 1].toFloat(&ok);
        if (!ok)
            return false;
    }
    *part = fields[0];
    *box = {values[0], values[1], values[2], values[3], values[4], values[5], values[6], values[7], values[10],
            values[9] != 0.0f};
    return true;
}

Skin DlcSkin(quint32 skinId)
{
    Skin skin;
    const QString name = QString("dlcskin%1.png").arg(skinId & ~kDlcSkinBit, 8, 10, QChar('0'));
    for (const QString& root : {QString("/WiiU/DLC"), QString("/WiiU/CU/DLC")})
    {
        QDirIterator packs(LauncherPaths::AssetsDir() + root, {"*.pck"}, QDir::Files, QDirIterator::Subdirectories);
        while (packs.hasNext())
        {
            QFile file(packs.next());
            PackItem item;
            if (!file.open(QIODevice::ReadOnly) || !FindPackItem(file, name, 0, &item))
                continue;
            skin.texture = QImage::fromData(item.data, "PNG");
            if (skin.texture.isNull())
                continue;
            for (const auto& [key, value] : item.parameters)
            {
                Box box;
                QString part;
                if (key == "ANIM")
                    skin.animFlags = value.trimmed().toUInt(nullptr, 0);
                else if (key == "BOX" && ParseBox(value, &box, &part))
                {
                    if (part == "HEAD" || part == "HEADWEAR")
                        skin.headBoxes.append(box);
                    else if ((part == "BODY" || part == "JACKET") && box.y - box.inflate < 0)
                        skin.bodyBoxes.append(box);
                }
            }
            return skin;
        }
    }
    return skin;
}

Skin SkinFor(const Profile& profile)
{
    const quint32 skinId = SelectedSkinId(profile);
    if ((skinId & kDlcSkinBit) != 0)
    {
        const Skin skin = DlcSkin(skinId);
        if (!skin.texture.isNull())
            return skin;
    }
    const quint32 index = (skinId & kDlcSkinBit) == 0 && (skinId & kUgcSkinMask) == 0 ? (skinId & kDefaultSkinMask) : 0;
    Skin skin;
    skin.texture = QImage(DefaultSkinPath(index == 0 ? 1 : index));
    return skin;
}

bool HatHasTransparency(const QImage& texture, int scale)
{
    for (int y = 8 * scale; y < 16 * scale; ++y)
    {
        for (int x = 40 * scale; x < 48 * scale; ++x)
        {
            if (qAlpha(texture.pixel(x, y)) < 255)
                return true;
        }
    }
    return false;
}

QImage Render(const QVector<Box>& boxes, const QImage& texture, int scale, float left, float top, int units)
{
    const int size = units * scale;
    QImage image(size, size, QImage::Format_ARGB32);
    image.fill(Qt::transparent);
    for (const Box& box : boxes)
    {
        const float boxLeft = box.x - box.inflate;
        const float boxTop = box.y - box.inflate;
        const float spanX = box.width + 2 * box.inflate;
        const float spanY = box.height + 2 * box.inflate;
        for (int py = 0; py < size; ++py)
        {
            const float y = top + (py + 0.5f) / scale;
            if (y < boxTop || y >= boxTop + spanY)
                continue;
            for (int px = 0; px < size; ++px)
            {
                const float x = left + (px + 0.5f) / scale;
                if (x < boxLeft || x >= boxLeft + spanX)
                    continue;
                int column = static_cast<int>((x - boxLeft) / spanX * box.width * scale);
                if (box.mirror)
                    column = static_cast<int>(box.width * scale) - 1 - column;
                const int row = static_cast<int>((y - boxTop) / spanY * box.height * scale);
                const int tx = static_cast<int>((box.u + box.depth) * scale) + column;
                const int ty = static_cast<int>((box.v + box.depth) * scale) + row;
                if (tx < 0 || ty < 0 || tx >= texture.width() || ty >= texture.height())
                    continue;
                const QRgb pixel = texture.pixel(tx, ty);
                if (qAlpha(pixel) > 0)
                    image.setPixel(px, py, pixel | 0xff000000u);
            }
        }
    }
    return image;
}

bool Covers(const QImage& image, double fraction)
{
    if (image.isNull())
        return false;
    int opaque = 0;
    for (int y = 0; y < image.height(); ++y)
    {
        for (int x = 0; x < image.width(); ++x)
        {
            if (qAlpha(image.pixel(x, y)) > 0)
                ++opaque;
        }
    }
    return opaque >= image.width() * image.height() * fraction;
}

QVector<Box> ClassicBoxes(const QImage& texture, int scale, bool withHat)
{
    QVector<Box> boxes{{-4, -8, -4, 8, 8, 8, 0, 0, 0, false}};
    if (withHat && HatHasTransparency(texture, scale))
        boxes.append({-4, -8, -4, 8, 8, 8, 32, 0, 0.5f, false});
    return boxes;
}

bool IsSolid(const Box& box)
{
    return box.width > 0 && box.height > 0 && box.depth > 0;
}

struct Frame
{
    float left;
    float top;
    float size;
};

Frame FrontFrame(const QVector<Box>& boxes, bool aboveNeckOnly)
{
    const bool anySolid = std::any_of(boxes.begin(), boxes.end(), IsSolid);
    float minX = std::numeric_limits<float>::max();
    float minY = minX;
    float maxX = -minX;
    float maxY = -minX;
    for (const Box& box : boxes)
    {
        if (anySolid && !IsSolid(box))
            continue;
        minX = std::min(minX, box.x - box.inflate);
        minY = std::min(minY, box.y - box.inflate);
        maxX = std::max(maxX, box.x + box.width + box.inflate);
        maxY = std::max(maxY, box.y + box.height + box.inflate);
    }
    if (aboveNeckOnly)
        maxY = std::min(maxY, 0.0f);
    const float size = std::max(1.0f, std::ceil(std::max(maxX - minX, maxY - minY)));
    return {(minX + maxX - size) / 2, (minY + maxY - size) / 2, size};
}

QImage FlatRender(const QVector<Box>& boxes, const QImage& texture, int scale, bool aboveNeckOnly)
{
    QVector<Box> sorted = boxes;
    std::stable_sort(sorted.begin(), sorted.end(),
                     [](const Box& a, const Box& b) { return a.z - a.inflate > b.z - b.inflate; });
    const Frame frame = FrontFrame(boxes, aboveNeckOnly);
    return Render(sorted, texture, scale, frame.left, frame.top, static_cast<int>(frame.size));
}

struct Vec3
{
    float x;
    float y;
    float z;
};

Vec3 operator+(const Vec3& a, const Vec3& b)
{
    return {a.x + b.x, a.y + b.y, a.z + b.z};
}

Vec3 operator*(const Vec3& a, float factor)
{
    return {a.x * factor, a.y * factor, a.z * factor};
}

class View
{
public:
    View(float yawDegrees, float pitchDegrees)
        : mYawCos(std::cos(yawDegrees * 3.14159265f / 180)), mYawSin(std::sin(yawDegrees * 3.14159265f / 180)),
          mPitchCos(std::cos(pitchDegrees * 3.14159265f / 180)), mPitchSin(std::sin(pitchDegrees * 3.14159265f / 180))
    {
    }

    Vec3 operator()(const Vec3& point) const
    {
        const float x = point.x * mYawCos - point.z * mYawSin;
        const float z = point.x * mYawSin + point.z * mYawCos;
        return {x, point.y * mPitchCos - z * mPitchSin, point.y * mPitchSin + z * mPitchCos};
    }

private:
    float mYawCos;
    float mYawSin;
    float mPitchCos;
    float mPitchSin;
};

struct Texel
{
    QPolygonF polygon;
    float depth;
    QRgb colour;
};

QImage HeadRender(const QVector<Box>& boxes, const QImage& texture, int scale, bool aboveNeckOnly)
{
    const int size = 128;
    const View view(45.0f, 25.0f);

    const bool anySolid = std::any_of(boxes.begin(), boxes.end(), IsSolid);
    float minX = std::numeric_limits<float>::max();
    float minY = minX;
    float maxX = -minX;
    float maxY = -minX;
    for (const Box& box : boxes)
    {
        if (anySolid && !IsSolid(box))
            continue;
        const float x0 = box.x - box.inflate;
        const float y0 = box.y - box.inflate;
        const float z0 = box.z - box.inflate;
        const float x1 = box.x + box.width + box.inflate;
        const float y1 = aboveNeckOnly ? std::min(box.y + box.height + box.inflate, 0.0f)
                                       : box.y + box.height + box.inflate;
        const float z1 = box.z + box.depth + box.inflate;
        for (const Vec3& corner : {Vec3{x0, y0, z0}, Vec3{x1, y0, z0}, Vec3{x0, y1, z0}, Vec3{x1, y1, z0},
                                   Vec3{x0, y0, z1}, Vec3{x1, y0, z1}, Vec3{x0, y1, z1}, Vec3{x1, y1, z1}})
        {
            const Vec3 projected = view(corner);
            minX = std::min(minX, projected.x);
            maxX = std::max(maxX, projected.x);
            minY = std::min(minY, projected.y);
            maxY = std::max(maxY, projected.y);
        }
    }
    const float extent = std::max(maxX - minX, maxY - minY) * 1.04f;
    const float pixelsPerUnit = size / extent;
    const float originX = (minX + maxX) / 2;
    const float originY = (minY + maxY) / 2;
    auto toImage = [&](const Vec3& projected) {
        return QPointF((projected.x - originX) * pixelsPerUnit + size / 2.0,
                       (projected.y - originY) * pixelsPerUnit + size / 2.0);
    };

    QVector<Texel> texels;
    for (const Box& box : boxes)
    {
        const float x0 = box.x - box.inflate;
        const float y0 = box.y - box.inflate;
        const float z0 = box.z - box.inflate;
        const float x1 = box.x + box.width + box.inflate;
        const float y1 = box.y + box.height + box.inflate;
        const float z1 = box.z + box.depth + box.inflate;
        const int u = static_cast<int>(box.u * scale);
        const int v = static_cast<int>(box.v * scale);
        const int w = static_cast<int>(box.width * scale);
        const int h = static_cast<int>(box.height * scale);
        const int d = static_cast<int>(box.depth * scale);
        struct Face
        {
            int textureX;
            int textureY;
            int columns;
            int rows;
            Vec3 origin;
            Vec3 across;
            Vec3 down;
            Vec3 normal;
            float shade;
        };
        const int rightSideX = box.mirror ? u + d + w : u;
        const int leftSideX = box.mirror ? u : u + d + w;
        const Face faces[] = {
            {u + d, v + d, w, h, {x0, y0, z0}, {x1 - x0, 0, 0}, {0, y1 - y0, 0}, {0, 0, -1}, 0.86f},
            {rightSideX, v + d, d, h, {x0, y0, z1}, {0, 0, z0 - z1}, {0, y1 - y0, 0}, {-1, 0, 0}, 0.70f},
            {leftSideX, v + d, d, h, {x1, y0, z0}, {0, 0, z1 - z0}, {0, y1 - y0, 0}, {1, 0, 0}, 0.70f},
            {u + 2 * d + w, v + d, w, h, {x1, y0, z1}, {x0 - x1, 0, 0}, {0, y1 - y0, 0}, {0, 0, 1}, 0.86f},
            {u + d, v, w, d, {x0, y0, z1}, {x1 - x0, 0, 0}, {0, 0, z0 - z1}, {0, -1, 0}, 1.0f},
            {u + d + w, v, w, d, {x0, y1, z0}, {x1 - x0, 0, 0}, {0, 0, z1 - z0}, {0, 1, 0}, 0.55f},
        };
        for (const Face& face : faces)
        {
            if (face.columns <= 0 || face.rows <= 0 || view(face.normal).z >= 0)
                continue;
            const Vec3 stepAcross = face.across * (1.0f / face.columns);
            const Vec3 stepDown = face.down * (1.0f / face.rows);
            for (int row = 0; row < face.rows; ++row)
            {
                for (int column = 0; column < face.columns; ++column)
                {
                    const int sourceColumn = box.mirror ? face.columns - 1 - column : column;
                    const int tx = face.textureX + sourceColumn;
                    const int ty = face.textureY + row;
                    if (tx < 0 || ty < 0 || tx >= texture.width() || ty >= texture.height())
                        continue;
                    const QRgb pixel = texture.pixel(tx, ty);
                    if (qAlpha(pixel) == 0)
                        continue;
                    const Vec3 corner = face.origin + stepAcross * static_cast<float>(column)
                                        + stepDown * static_cast<float>(row);
                    const Vec3 a = view(corner);
                    const Vec3 b = view(corner + stepAcross);
                    const Vec3 c = view(corner + stepAcross + stepDown);
                    const Vec3 e = view(corner + stepDown);
                    QPolygonF polygon;
                    polygon << toImage(a) << toImage(b) << toImage(c) << toImage(e);
                    const QRgb colour = qRgb(static_cast<int>(qRed(pixel) * face.shade),
                                             static_cast<int>(qGreen(pixel) * face.shade),
                                             static_cast<int>(qBlue(pixel) * face.shade));
                    texels.append({polygon, (a.z + b.z + c.z + e.z) / 4, colour});
                }
            }
        }
    }
    std::stable_sort(texels.begin(), texels.end(), [](const Texel& a, const Texel& b) { return a.depth > b.depth; });

    QImage image(size, size, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    QPainter painter(&image);
    for (const Texel& texel : texels)
    {
        painter.setPen(QPen(QColor(texel.colour), 0.6));
        painter.setBrush(QColor(texel.colour));
        painter.drawPolygon(texel.polygon);
    }
    return image;
}

QImage FrontView(const Skin& skin)
{
    const QImage texture = skin.texture.convertToFormat(QImage::Format_ARGB32);
    const int scale = texture.width() / 64;
    if (scale < 1 || texture.height() < 16 * scale)
        return QImage();

    QVector<Box> boxes;
    if ((skin.animFlags & kHeadHiddenBit) == 0)
        boxes.append({-4, -8, -4, 8, 8, 8, 0, 0, 0, false});
    if ((skin.animFlags & kHatHiddenBit) == 0 && HatHasTransparency(texture, scale))
        boxes.append({-4, -8, -4, 8, 8, 8, 32, 0, 0.5f, false});
    boxes += skin.headBoxes;
    bool aboveNeckOnly = false;
    if (boxes.isEmpty())
    {
        boxes = skin.bodyBoxes;
        aboveNeckOnly = true;
    }
    if (!boxes.isEmpty() && Covers(FlatRender(boxes, texture, scale, aboveNeckOnly), 0.125))
        return HeadRender(boxes, texture, scale, aboveNeckOnly);

    const QVector<Box> classic = ClassicBoxes(texture, scale, (skin.animFlags & kHatHiddenBit) == 0);
    if (Covers(FlatRender(classic, texture, scale, false), 0.4))
        return HeadRender(classic, texture, scale, false);
    const QImage steve = QImage(DefaultSkinPath(1)).convertToFormat(QImage::Format_ARGB32);
    if (steve.isNull())
        return QImage();
    const int steveScale = std::max(1, steve.width() / 64);
    return HeadRender(ClassicBoxes(steve, steveScale, true), steve, steveScale, false);
}
}

namespace Skins
{
QImage FaceFor(const Profile& profile)
{
    return FrontView(SkinFor(profile));
}
}
