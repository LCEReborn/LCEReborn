#include "platform/pc/PcAvatar.hpp"

#include "game/client/render/Textures.hpp"
#include "game/core/BufferedImage.hpp"
#include "game/core/CConsoleMinecraftApp.hpp"
#include "game/core/CMinecraftApp.hpp"
#include "game/core/DLCSkinFile.hpp"
#include "game/core/GameSettings.hpp"
#include "game/core/Minecraft.hpp"
#include "vendor/SKIN_BOX.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <string>
#include <vector>

namespace
{
const unsigned int kHeadHiddenBit = 1u << 10;
const unsigned int kHatHiddenBit = 1u << 16;
const int kHeadPart = 1;
const int kBodyPart = 2;
const int kHeadwearPart = 7;
const int kJacketPart = 8;
const int kSupersample = 4;

struct DefaultSkin
{
    _TEXTURE_NAME texture;
    const wchar_t* path;
};

const DefaultSkin kDefaultSkins[] = {
    {TN_MOB_STEVE, L"mob/steve"},       {TN_MOB_STEVE, L"mob/steve"}, {TN_MOB_CHAR1, L"mob/char1"},
    {TN_MOB_CHAR2, L"mob/char2"},       {TN_MOB_CHAR3, L"mob/char3"}, {TN_MOB_CHAR4, L"mob/char4"},
    {TN_MOB_CHAR5, L"mob/char5"},       {TN_MOB_CHAR6, L"mob/char6"}, {TN_MOB_CHAR7, L"mob/char7"},
    {TN_MOB_ALEX, L"mob/alex"},         {TN_MOB_ALEX1, L"mob/alex1"}, {TN_MOB_ALEX2, L"mob/alex2"},
    {TN_MOB_ALEX3, L"mob/alex3"},       {TN_MOB_ALEX4, L"mob/alex4"}, {TN_MOB_ALEX5, L"mob/alex5"},
    {TN_MOB_ALEX6, L"mob/alex6"},       {TN_MOB_ALEX7, L"mob/alex7"}, {TN_MOB_DEVALEX, L"mob/DevAlex"},
    {TN_MOB_DEVSTEVE, L"mob/DevSteve"},
};

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

struct Texture
{
    const int* data;
    int width;
    int height;

    unsigned int At(int x, int y) const
    {
        if (x < 0 || y < 0 || x >= width || y >= height)
            return 0;
        return static_cast<unsigned int>(data[y * width + x]);
    }
};

struct Frame
{
    float left;
    float top;
    float size;
};

struct Vec3
{
    float x;
    float y;
    float z;
};

Vec3 Add(const Vec3& a, const Vec3& b)
{
    Vec3 result = {a.x + b.x, a.y + b.y, a.z + b.z};
    return result;
}

Vec3 Scale(const Vec3& a, float factor)
{
    Vec3 result = {a.x * factor, a.y * factor, a.z * factor};
    return result;
}

class View
{
public:
    View(float yawDegrees, float pitchDegrees)
        : mYawCos(std::cos(yawDegrees * 3.14159265f / 180)),
          mYawSin(std::sin(yawDegrees * 3.14159265f / 180)),
          mPitchCos(std::cos(pitchDegrees * 3.14159265f / 180)),
          mPitchSin(std::sin(pitchDegrees * 3.14159265f / 180))
    {
    }

    Vec3 operator()(const Vec3& point) const
    {
        const float x = point.x * mYawCos - point.z * mYawSin;
        const float z = point.x * mYawSin + point.z * mYawCos;
        Vec3 result = {x, point.y * mPitchCos - z * mPitchSin, point.y * mPitchSin + z * mPitchCos};
        return result;
    }

private:
    float mYawCos;
    float mYawSin;
    float mPitchCos;
    float mPitchSin;
};

struct Quad
{
    float x[4];
    float y[4];
    float depth;
    unsigned int colour;
};

Box MakeBox(float x, float y, float z, float width, float height, float depth, float u, float v, float inflate)
{
    Box box = {x, y, z, width, height, depth, u, v, inflate, false};
    return box;
}

Box FromSkinBox(const SKIN_BOX& skinBox)
{
    Box box = {skinBox.x,     skinBox.y,        skinBox.z,        skinBox.width, skinBox.height,
               skinBox.depth, skinBox.xTexOffs, skinBox.yTexOffs, skinBox.delta, skinBox.mirror != 0};
    return box;
}

bool IsSolid(const Box& box)
{
    return box.width > 0 && box.height > 0 && box.depth > 0;
}

bool HatHasTransparency(const Texture& texture, int scale)
{
    for (int y = 8 * scale; y < 16 * scale; ++y)
    {
        for (int x = 40 * scale; x < 48 * scale; ++x)
        {
            if ((texture.At(x, y) >> 24) < 255)
                return true;
        }
    }
    return false;
}

std::vector<Box> ClassicBoxes(const Texture& texture, int scale, bool withHat)
{
    std::vector<Box> boxes(1, MakeBox(-4, -8, -4, 8, 8, 8, 0, 0, 0));
    if (withHat && HatHasTransparency(texture, scale))
        boxes.push_back(MakeBox(-4, -8, -4, 8, 8, 8, 32, 0, 0.5f));
    return boxes;
}

Frame FrontFrame(const std::vector<Box>& boxes, bool aboveNeckOnly)
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
    Frame frame = {(minX + maxX - size) / 2, (minY + maxY - size) / 2, size};
    return frame;
}

double FrontCoverage(const std::vector<Box>& boxes, const Texture& texture, int scale, bool aboveNeckOnly)
{
    if (boxes.empty())
        return 0.0;
    const Frame frame = FrontFrame(boxes, aboveNeckOnly);
    const int size = static_cast<int>(frame.size) * scale;
    int opaque = 0;
    for (int py = 0; py < size; ++py)
    {
        const float y = frame.top + (py + 0.5f) / scale;
        for (int px = 0; px < size; ++px)
        {
            const float x = frame.left + (px + 0.5f) / scale;
            for (const Box& box : boxes)
            {
                const float left = box.x - box.inflate;
                const float top = box.y - box.inflate;
                const float spanX = box.width + 2 * box.inflate;
                const float spanY = box.height + 2 * box.inflate;
                if (x < left || x >= left + spanX || y < top || y >= top + spanY)
                    continue;
                int column = static_cast<int>((x - left) / spanX * box.width * scale);
                if (box.mirror)
                    column = static_cast<int>(box.width * scale) - 1 - column;
                const int row = static_cast<int>((y - top) / spanY * box.height * scale);
                const unsigned int pixel = texture.At(static_cast<int>((box.u + box.depth) * scale) + column,
                                                      static_cast<int>((box.v + box.depth) * scale) + row);
                if ((pixel >> 24) != 0)
                {
                    ++opaque;
                    break;
                }
            }
        }
    }
    return size > 0 ? static_cast<double>(opaque) / (static_cast<double>(size) * size) : 0.0;
}

unsigned int Shade(unsigned int pixel, float shade)
{
    unsigned int result = 0xff000000u;
    for (int shift = 0; shift < 24; shift += 8)
    {
        const unsigned int channel = static_cast<unsigned int>(((pixel >> shift) & 0xff) * shade);
        result |= std::min(channel, 255u) << shift;
    }
    return result;
}

void FillQuad(const Quad& quad, std::vector<unsigned int>& canvas, int canvasSize)
{
    float cx = 0;
    float cy = 0;
    for (int i = 0; i < 4; ++i)
    {
        cx += quad.x[i] / 4;
        cy += quad.y[i] / 4;
    }
    float x[4];
    float y[4];
    for (int i = 0; i < 4; ++i)
    {
        const float dx = quad.x[i] - cx;
        const float dy = quad.y[i] - cy;
        const float length = std::sqrt(dx * dx + dy * dy);
        const float grow = length > 0 ? 0.35f / length : 0;
        x[i] = quad.x[i] + dx * grow;
        y[i] = quad.y[i] + dy * grow;
    }
    float area = 0;
    for (int i = 0; i < 4; ++i)
        area += x[i] * y[(i + 1) % 4] - x[(i + 1) % 4] * y[i];
    const float sign = area >= 0 ? 1.0f : -1.0f;

    const int minX = std::max(0, static_cast<int>(std::floor(*std::min_element(x, x + 4))));
    const int maxX = std::min(canvasSize - 1, static_cast<int>(std::ceil(*std::max_element(x, x + 4))));
    const int minY = std::max(0, static_cast<int>(std::floor(*std::min_element(y, y + 4))));
    const int maxY = std::min(canvasSize - 1, static_cast<int>(std::ceil(*std::max_element(y, y + 4))));
    for (int py = minY; py <= maxY; ++py)
    {
        const float sampleY = py + 0.5f;
        for (int px = minX; px <= maxX; ++px)
        {
            const float sampleX = px + 0.5f;
            bool inside = true;
            for (int i = 0; i < 4 && inside; ++i)
            {
                const int next = (i + 1) % 4;
                const float cross = (x[next] - x[i]) * (sampleY - y[i]) - (y[next] - y[i]) * (sampleX - x[i]);
                inside = cross * sign >= 0;
            }
            if (inside)
                canvas[py * canvasSize + px] = quad.colour;
        }
    }
}

BufferedImage* HeadRender(const std::vector<Box>& boxes, const Texture& texture, int scale, bool aboveNeckOnly,
                          int size)
{
    const View view(45.0f, 25.0f);
    const int canvasSize = size * kSupersample;

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
        const Vec3 corners[] = {{x0, y0, z0}, {x1, y0, z0}, {x0, y1, z0}, {x1, y1, z0},
                                {x0, y0, z1}, {x1, y0, z1}, {x0, y1, z1}, {x1, y1, z1}};
        for (const Vec3& corner : corners)
        {
            const Vec3 projected = view(corner);
            minX = std::min(minX, projected.x);
            maxX = std::max(maxX, projected.x);
            minY = std::min(minY, projected.y);
            maxY = std::max(maxY, projected.y);
        }
    }
    const float extent = std::max(maxX - minX, maxY - minY) * 1.04f;
    const float pixelsPerUnit = canvasSize / extent;
    const float originX = (minX + maxX) / 2;
    const float originY = (minY + maxY) / 2;

    std::vector<Quad> quads;
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
            const Vec3 stepAcross = Scale(face.across, 1.0f / face.columns);
            const Vec3 stepDown = Scale(face.down, 1.0f / face.rows);
            for (int row = 0; row < face.rows; ++row)
            {
                for (int column = 0; column < face.columns; ++column)
                {
                    const int sourceColumn = box.mirror ? face.columns - 1 - column : column;
                    const unsigned int pixel = texture.At(face.textureX + sourceColumn, face.textureY + row);
                    if ((pixel >> 24) == 0)
                        continue;
                    const Vec3 corner = Add(Add(face.origin, Scale(stepAcross, static_cast<float>(column))),
                                            Scale(stepDown, static_cast<float>(row)));
                    const Vec3 points[] = {view(corner), view(Add(corner, stepAcross)),
                                           view(Add(Add(corner, stepAcross), stepDown)), view(Add(corner, stepDown))};
                    Quad quad;
                    quad.depth = 0;
                    for (int i = 0; i < 4; ++i)
                    {
                        quad.x[i] = (points[i].x - originX) * pixelsPerUnit + canvasSize / 2.0f;
                        quad.y[i] = (points[i].y - originY) * pixelsPerUnit + canvasSize / 2.0f;
                        quad.depth += points[i].z / 4;
                    }
                    quad.colour = Shade(pixel, face.shade);
                    quads.push_back(quad);
                }
            }
        }
    }
    std::stable_sort(quads.begin(), quads.end(), [](const Quad& a, const Quad& b) {
        return a.depth > b.depth;
    });

    std::vector<unsigned int> canvas(static_cast<size_t>(canvasSize) * canvasSize, 0);
    for (const Quad& quad : quads)
        FillQuad(quad, canvas, canvasSize);

    BufferedImage* image = new BufferedImage(size, size, 1);
    int* out = image->getData();
    for (int y = 0; y < size; ++y)
    {
        for (int x = 0; x < size; ++x)
        {
            unsigned int sums[3] = {0, 0, 0};
            unsigned int covered = 0;
            for (int sy = 0; sy < kSupersample; ++sy)
            {
                for (int sx = 0; sx < kSupersample; ++sx)
                {
                    const unsigned int sample = canvas[(y * kSupersample + sy) * canvasSize + x * kSupersample + sx];
                    if ((sample >> 24) == 0)
                        continue;
                    ++covered;
                    for (int channel = 0; channel < 3; ++channel)
                        sums[channel] += (sample >> (channel * 8)) & 0xff;
                }
            }
            unsigned int pixel = 0;
            if (covered != 0)
            {
                pixel = (covered * 255 / (kSupersample * kSupersample)) << 24;
                for (int channel = 0; channel < 3; ++channel)
                    pixel |= (sums[channel] / covered) << (channel * 8);
            }
            out[y * size + x] = static_cast<int>(pixel);
        }
    }
    return image;
}

BufferedImage* LoadDefaultSkin(int index)
{
    Minecraft* minecraft = Minecraft::GetInstance();
    if (minecraft == NULL || minecraft->getTextureManager() == NULL)
        return NULL;
    if (index < 0 || index >= static_cast<int>(sizeof(kDefaultSkins) / sizeof(kDefaultSkins[0])))
        index = 0;
    return minecraft->getTextureManager()->readImage(kDefaultSkins[index].texture, kDefaultSkins[index].path);
}
}

BufferedImage* PcRenderAvatarHead(int quadrant, int size)
{
    return PcRenderSkinHead(GameSettings::GetPlayerSkinId(quadrant), quadrant + 1, size);
}

BufferedImage* PcRenderSkinHead(unsigned int skinId, int defaultIndex, int size)
{
    std::vector<Box> headBoxes;
    std::vector<Box> bodyBoxes;
    unsigned int animFlags = 0;
    BufferedImage* skin = NULL;

    if ((skinId >> 31) != 0)
    {
        const std::wstring path = CMinecraftApp::getSkinPathFromId(skinId);
        unsigned char* data = NULL;
        unsigned int length = 0;
        gApp.GetMemFileDetails(path, &data, &length);
        if (data != NULL && length != 0)
        {
            skin = new BufferedImage(data, length);
            DLCSkinFile* file = static_cast<DLCSkinFile*>(gApp.mDLCManager.getSkinFile(path));
            if (file != NULL)
            {
                animFlags = file->getSkinAdjustments().mAnimFlags;
                const std::vector<SKIN_BOX*>* boxes = file->getAdditionalBoxes();
                for (std::vector<SKIN_BOX*>::const_iterator it = boxes->begin(); it != boxes->end(); ++it)
                {
                    const SKIN_BOX& box = **it;
                    if (box.id == kHeadPart || box.id == kHeadwearPart)
                        headBoxes.push_back(FromSkinBox(box));
                    else if ((box.id == kBodyPart || box.id == kJacketPart) && box.y - box.delta < 0)
                        bodyBoxes.push_back(FromSkinBox(box));
                }
            }
        }
    }
    if (skin == NULL)
    {
        int index = 0;
        if ((skinId >> 31) == 0 && (skinId & 0x7fffffe0u) == 0)
            index = static_cast<int>(skinId & 0x1fu);
        if (index == 0)
            index = defaultIndex;
        skin = LoadDefaultSkin(index);
    }
    if (skin == NULL)
        return NULL;

    Texture texture = {skin->getData(), static_cast<int>(skin->getWidth()), static_cast<int>(skin->getHeight())};
    const int scale = texture.width / 64;
    BufferedImage* result = NULL;
    if (texture.data != NULL && scale >= 1 && texture.height >= 16 * scale)
    {
        std::vector<Box> boxes;
        if ((animFlags & kHeadHiddenBit) == 0)
            boxes.push_back(MakeBox(-4, -8, -4, 8, 8, 8, 0, 0, 0));
        if ((animFlags & kHatHiddenBit) == 0 && HatHasTransparency(texture, scale))
            boxes.push_back(MakeBox(-4, -8, -4, 8, 8, 8, 32, 0, 0.5f));
        boxes.insert(boxes.end(), headBoxes.begin(), headBoxes.end());
        bool aboveNeckOnly = false;
        if (boxes.empty())
        {
            boxes = bodyBoxes;
            aboveNeckOnly = true;
        }
        if (FrontCoverage(boxes, texture, scale, aboveNeckOnly) >= 0.125)
        {
            result = HeadRender(boxes, texture, scale, aboveNeckOnly, size);
        }
        else
        {
            const std::vector<Box> classic = ClassicBoxes(texture, scale, (animFlags & kHatHiddenBit) == 0);
            if (FrontCoverage(classic, texture, scale, false) >= 0.4)
                result = HeadRender(classic, texture, scale, false, size);
        }
    }
    delete skin;
    if (result != NULL)
        return result;

    BufferedImage* steve = LoadDefaultSkin(0);
    if (steve == NULL)
        return NULL;
    Texture steveTexture = {steve->getData(), static_cast<int>(steve->getWidth()),
                            static_cast<int>(steve->getHeight())};
    const int steveScale = std::max(1, steveTexture.width / 64);
    result = HeadRender(ClassicBoxes(steveTexture, steveScale, true), steveTexture, steveScale, false, size);
    delete steve;
    return result;
}
