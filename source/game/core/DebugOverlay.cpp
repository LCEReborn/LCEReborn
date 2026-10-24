#include "game/core/DebugOverlay.hpp"

#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <wchar.h>

#include "game/client/render/Font.hpp"
#include "game/client/render/LevelRenderer.hpp"
#include "game/core/FrameTimer.hpp"
#include "game/core/ConsoleUIController.hpp"
#include "game/core/GameSettings.hpp"
#include "game/core/GlStateManager.hpp"
#include "game/core/GuiComponent.hpp"
#include "game/core/Minecraft.hpp"
#include "game/core/StringConversion.hpp"
#include "game/core/System.hpp"
#include "game/network/CGameNetworkManager.hpp"
#include "game/server/MinecraftServer.hpp"
#include "game/world/Dimension.hpp"
#include "game/world/DimensionType.hpp"
#include "game/world/Level.hpp"
#include "game/world/LevelSource.hpp"
#include "game/world/biome/Biome.hpp"
#include "game/world/block/BlockPos.hpp"
#include "game/world/generation/MultiplayerLocalPlayer.hpp"
#include "game/ui/UILayer.hpp"
#include "game/client/render/Renderer.hpp"
#if LCE_PLATFORM_PC
#include "platform/pc/PcSystemInfo.hpp"
#else
#include <stddef.h>
extern "C" size_t dlmalloc_footprint(void);
extern char* gHeapStart;
extern char* gHeapEnd;
#endif

extern ConsoleUIController gConsoleUIController;

namespace
{
const int kLineHeight = 9;
const int kHeaderHeight = 10;
const int kPadding = 3;
const int kGroupGap = 3;
const float kSlideSeconds = 0.18f;

const int kGroups[DebugOverlay::EEntry_Count] = {
    DebugOverlay::EGroup_Position,    DebugOverlay::EGroup_Position,    DebugOverlay::EGroup_Position,
    DebugOverlay::EGroup_Position,    DebugOverlay::EGroup_Position,    DebugOverlay::EGroup_World,
    DebugOverlay::EGroup_World,       DebugOverlay::EGroup_World,       DebugOverlay::EGroup_World,
    DebugOverlay::EGroup_Performance, DebugOverlay::EGroup_Performance, DebugOverlay::EGroup_Performance,
    DebugOverlay::EGroup_System,      DebugOverlay::EGroup_System,      DebugOverlay::EGroup_Server,
    DebugOverlay::EGroup_Server};

const wchar_t* const kEntryNames[DebugOverlay::EEntry_Count] = {
    L"XYZ",    L"Block",         L"Chunk",    L"Facing", L"Dimension", L"Biome",  L"Light",  L"Time",
    L"Weather", L"Frame rate", L"Chunk updates", L"Entities", L"Memory", L"System", L"Server", L"Players"};

const wchar_t* const kGroupNames[DebugOverlay::EGroup_Count] = {L"Position", L"World", L"Performance", L"System",
                                                                 L"Server"};

const wchar_t* const kStateNames[DebugOverlay::EState_Count] = {L"With F3", L"Always", L"Off"};

const wchar_t* const kSampleValues[DebugOverlay::EEntry_Count] = {
    L"-10.136 / 66.743 / 6.759", L"-11 66 6",          L"-1 4 0 [5 2 6]",    L"east (Towards positive X)",
    L"Overworld",                L"Savanna",           L"15 (15 sky, 0 block)", L"Day 3, 08:12",
    L"Clear",                    L"60 fps",            L"12 per second",     L"42",
    L"914 / 4096 MiB",      L"Your system",       L"Integrated @ 9.3 ms", L"3 / 8"};

struct PadState
{
    bool open;
    float amount[DebugOverlay::EEntry_Count];
    long long lastTime;
};

PadState sPads[4];
long long sChunkSampleTime = 0;
unsigned int sChunkSampleCount = 0;
unsigned int sChunkRate = 0;

std::wstring Format(const wchar_t* format, ...)
{
    wchar_t buffer[160];
    va_list arguments;
    va_start(arguments, format);
    vswprintf(buffer, sizeof(buffer) / sizeof(buffer[0]), format, arguments);
    va_end(arguments);
    return buffer;
}

unsigned int WithAlpha(unsigned int colour, float amount)
{
    int alpha = static_cast<int>(static_cast<float>((colour >> 24) & 255) * amount);
    if (alpha < 4)
        alpha = 4;
    if (alpha > 255)
        alpha = 255;
    return (static_cast<unsigned int>(alpha) << 24) | (colour & 0xFFFFFF);
}

void Text(Font& font, const std::wstring& text, int x, int y, unsigned int colour)
{
    GlStateManager::enableBlend();
    GlStateManager::blendFunc(4, 5);
    GlStateManager::enableTexture();
    font.draw(text, x, y, static_cast<int>(colour));
}

int FrameRate(Minecraft* minecraft)
{
    FrameTimer* timer = minecraft->getFrameTimer();
    if (timer == NULL)
        return 0;
    const long long* log = timer->getLog();
    const int end = timer->getLogEnd();
    const int samples = timer->getLogLength() < 60 ? timer->getLogLength() : 60;
    long long total = 0;
    int count = 0;
    for (int back = 1; back <= samples; ++back)
    {
        const long long duration = log[((end - back) % 240 + 240) % 240];
        if (duration <= 0)
            break;
        total += duration;
        ++count;
    }
    if (count == 0 || total <= 0)
        return 0;
    return static_cast<int>(1000000000.0 * count / static_cast<double>(total) + 0.5);
}

unsigned int ChunkRate()
{
    const long long now = System::processTimeInMilliSecs();
    const unsigned int count = LevelRenderer::sChunkUpdates;
    if (sChunkSampleTime == 0)
    {
        sChunkSampleTime = now;
        sChunkSampleCount = count;
    }
    else if (now - sChunkSampleTime >= 1000)
    {
        sChunkRate = static_cast<unsigned int>((count - sChunkSampleCount) * 1000ULL
                                               / static_cast<unsigned long long>(now - sChunkSampleTime));
        sChunkSampleTime = now;
        sChunkSampleCount = count;
    }
    return sChunkRate;
}

std::wstring MemoryText()
{
#if LCE_PLATFORM_PC
    unsigned long long used = 0;
    unsigned long long total = 0;
    if (!PcMemoryUsage(&used, &total) || total == 0)
        return L"Unknown";
#else
    const unsigned long long used = dlmalloc_footprint();
    const unsigned long long total = static_cast<unsigned long long>(gHeapEnd - gHeapStart);
    if (total == 0)
        return L"Unknown";
#endif
    return Format(L"%d / %d MiB", static_cast<int>(used >> 20), static_cast<int>(total >> 20));
}

std::wstring SystemText()
{
#if LCE_PLATFORM_PC
    static std::wstring text;
    if (text.empty())
    {
        std::string gpu = PcGpuName();
        const std::string::size_type slash = gpu.find('/');
        if (slash != std::string::npos)
            gpu = gpu.substr(0, slash);
        std::string cpu = PcCpuName();
        text = convStringToWstring(cpu.empty() ? gpu : (gpu.empty() ? cpu : cpu + ", " + gpu));
        if (text.empty())
            text = L"PC";
    }
    return text;
#else
    return L"Wii U";
#endif
}

std::wstring FacingText(float yaw, float pitch)
{
    static const wchar_t* const kNames[4] = {L"south (Towards positive Z)", L"west (Towards negative X)",
                                             L"north (Towards negative Z)", L"east (Towards positive X)"};
    float wrapped = fmodf(yaw, 360.0f);
    if (wrapped >= 180.0f)
        wrapped -= 360.0f;
    if (wrapped < -180.0f)
        wrapped += 360.0f;
    const int direction = static_cast<int>(floorf(yaw * 4.0f / 360.0f + 0.5f)) & 3;
    return Format(L"%ls (%.1f / %.1f)", kNames[direction], wrapped + 0.0f, pitch + 0.0f);
}
}

namespace DebugOverlay
{
int Group(int entry)
{
    return kGroups[entry];
}

const wchar_t* GroupName(int group)
{
    return kGroupNames[group];
}

const wchar_t* EntryName(int entry)
{
    return kEntryNames[entry];
}

const wchar_t* StateName(int state)
{
    return kStateNames[state];
}

int GetState(int pad, int entry)
{
    return GameSettings::GetDebugOverlayState(pad, entry);
}

void SetState(int pad, int entry, int state)
{
    GameSettings::SetDebugOverlayState(pad, entry, state);
}

bool IsShown(int pad, int entry, bool open)
{
    const int state = GetState(pad, entry);
    return state == EState_Always || (open && state == EState_WithF3);
}

bool IsOpen(int pad)
{
    return pad >= 0 && pad < 4 && sPads[pad].open;
}

void Toggle(int pad)
{
    if (pad >= 0 && pad < 4)
        sPads[pad].open = !sPads[pad].open;
}

void Values(int pad, std::wstring values[EEntry_Count])
{
    Minecraft* minecraft = Minecraft::GetInstance();
    Level* level = minecraft->mLevel;
    MultiplayerLocalPlayer* player = pad >= 0 && pad < 4 ? minecraft->mLocalPlayers[pad].get() : NULL;
    if (level == NULL || player == NULL)
    {
        for (int entry = 0; entry < EEntry_Count; ++entry)
            values[entry] = kSampleValues[entry];
        values[EEntry_System] = SystemText();
        return;
    }

    const BlockPos pos(player);
    values[EEntry_Position] = Format(L"%.3f / %.3f / %.3f", player->mX, player->mY, player->mZ);
    values[EEntry_Block] = Format(L"%d %d %d", pos.x, pos.y, pos.z);
    values[EEntry_Chunk] = Format(L"%d %d %d [%d %d %d]", pos.x >> 4, pos.y >> 4, pos.z >> 4, pos.x & 15, pos.y & 15,
                                  pos.z & 15);
    values[EEntry_Facing] = FacingText(player->mYRot, player->mXRot);
    values[EEntry_Dimension] = level->mDimension->getType()->getName();

    Biome* biome = level->getBiome(pos);
    std::wstring biomeName = biome != NULL ? biome->getName(false) : std::wstring();
    if (biomeName.empty() && biome != NULL)
        biomeName = biome->mBiomeName;
    values[EEntry_Biome] = biomeName;

    values[EEntry_Light] = Format(L"%d (%d sky, %d block)", level->getRawBrightness(pos),
                                  level->getBrightness(LightLayer::SKY, pos), level->getBrightness(LightLayer::BLOCK, pos));

    const long long dayTime = level->getDayTime();
    const long long ticks = ((dayTime % 24000) + 24000) % 24000;
    const int hours = static_cast<int>((ticks / 1000 + 6) % 24);
    const int minutes = static_cast<int>((ticks % 1000) * 60 / 1000);
    values[EEntry_Time] = Format(L"Day %d, %02d:%02d", static_cast<int>(dayTime / 24000) + 1, hours, minutes);
    values[EEntry_Weather] = level->isThundering() ? L"Thunder" : (level->isRaining() ? L"Rain" : L"Clear");

    values[EEntry_FrameRate] = Format(L"%d fps", FrameRate(minecraft));
    values[EEntry_ChunkUpdates] = Format(L"%u per second", ChunkRate());
    values[EEntry_Entities] =
        Format(L"%d", minecraft->mLevelRenderer != NULL ? minecraft->mLevelRenderer->GetEntityCount() : 0);
    values[EEntry_Memory] = MemoryText();
    values[EEntry_System] = SystemText();

    if (gMinecraftServer != NULL && CGameNetworkManager::sInstance.IsHost())
        values[EEntry_Server] = Format(L"Integrated @ %.1f ms", static_cast<double>(MinecraftServer::sAverageTickMilliseconds));
    else
        values[EEntry_Server] = L"Remote";
    values[EEntry_Players] = Format(L"%d / %u", CGameNetworkManager::sInstance.GetPlayerCount(),
                                    CGameNetworkManager::sInstance.GetMaxPlayers());
}

void Draw(GuiComponent& gui, Font& font, int x, int y, const std::wstring values[EEntry_Count], const Look& look,
          int* highlightX, int* highlightY)
{
    int top = y;
    for (int group = 0; group < EGroup_Count; ++group)
    {
        float groupAmount = 0.0f;
        int labelWidth = 0;
        int valueWidth = 0;
        float lines = 0.0f;
        for (int entry = 0; entry < EEntry_Count; ++entry)
        {
            if (kGroups[entry] != group || look.amount[entry] <= 0.01f)
                continue;
            if (look.amount[entry] > groupAmount)
                groupAmount = look.amount[entry];
            const int label = font.width(std::wstring(kEntryNames[entry]) + L":");
            const int value = font.width(values[entry]);
            labelWidth = label > labelWidth ? label : labelWidth;
            valueWidth = value > valueWidth ? value : valueWidth;
            lines += look.amount[entry];
        }
        if (groupAmount <= 0.01f)
            continue;

        int width = labelWidth + 4 + valueWidth;
        const int headerWidth = font.width(kGroupNames[group]);
        width = (headerWidth > width ? headerWidth : width) + kPadding * 2;
        const int height = static_cast<int>(kHeaderHeight * groupAmount + lines * kLineHeight + 0.5f) + 1;
        gui.fill(x, top, x + width, top + height, static_cast<int>(WithAlpha(0x8C505050, groupAmount)));
        gui.fill(x, top, x + width, top + static_cast<int>(kHeaderHeight * groupAmount),
                 static_cast<int>(WithAlpha(0x993C3C3C, groupAmount)));
        if (groupAmount > 0.6f)
            Text(font, kGroupNames[group], x + kPadding, top + 1, WithAlpha(0xFFFFFFFF, groupAmount));

        float lineTop = static_cast<float>(top) + kHeaderHeight * groupAmount;
        for (int entry = 0; entry < EEntry_Count; ++entry)
        {
            const float amount = look.amount[entry];
            if (kGroups[entry] != group || amount <= 0.01f)
                continue;
            const int lineY = static_cast<int>(lineTop + 0.5f);
            const int slide = static_cast<int>((1.0f - amount) * -12.0f);
            if (look.flash[entry] > 0.0f)
                gui.fill(x, lineY, x + width, lineY + kLineHeight,
                         static_cast<int>(WithAlpha(0x99FFFFFF, look.flash[entry])));
            unsigned int colour = 0xFFE0E0E0;
            if (entry == look.highlight)
            {
                gui.fill(x, lineY, x + width, lineY + kLineHeight,
                         static_cast<int>(WithAlpha(0xFFFFFF55, 0.1f + 0.2f * look.pulse)));
                colour = 0xFFFFFF55;
                if (highlightX != NULL)
                    *highlightX = x + width / 2;
                if (highlightY != NULL)
                    *highlightY = lineY + kLineHeight / 2;
            }
            if (amount > 0.35f)
            {
                const std::wstring label = std::wstring(kEntryNames[entry]) + L":";
                Text(font, label, x + kPadding + labelWidth - font.width(label) + slide, lineY + 1,
                     WithAlpha(colour, amount));
                Text(font, values[entry], x + kPadding + labelWidth + 4 + slide, lineY + 1,
                     WithAlpha(entry == look.highlight ? colour : 0xFFFFFFFF, amount));
            }
            lineTop += kLineHeight * amount;
        }
        top += height + kGroupGap;
    }
}

void RenderScreen(int pad, int viewport, int width, int height)
{
    if (pad < 0 || pad >= 4)
        return;
    PadState& state = sPads[pad];
    const long long now = System::processTimeInMilliSecs();
    float step = state.lastTime == 0 ? 1.0f : static_cast<float>(now - state.lastTime) / 1000.0f / kSlideSeconds;
    if (step > 1.0f)
        step = 1.0f;
    state.lastTime = now;

    Look look;
    look.highlight = -1;
    look.pulse = 0.0f;
    bool any = false;
    for (int entry = 0; entry < EEntry_Count; ++entry)
    {
        const float target = IsShown(pad, entry, state.open) ? 1.0f : 0.0f;
        float& amount = state.amount[entry];
        amount = amount < target ? (amount + step > target ? target : amount + step)
                                 : (amount - step < target ? target : amount - step);
        look.amount[entry] = amount;
        look.flash[entry] = 0.0f;
        any = any || amount > 0.01f;
    }
    Minecraft* minecraft = Minecraft::GetInstance();
    if (!any || minecraft == NULL || minecraft->mFont == NULL
        || GameSettings::GetGameSettings(pad, eGameSetting_DisplayHUD) == 0
        || gConsoleUIController.IsSceneInStack(pad, EUIScene_DebugOverlayMenu))
        return;

    std::wstring values[EEntry_Count];
    Values(pad, values);
    int originX = 0;
    int originY = 0;
    ViewportOrigin(viewport, &originX, &originY);
    float screenWidth = 0.0f;
#if LCE_PLATFORM_PC
    const Renderer::ELayout layout = gRenderer->GetLayout();
    const bool fullWindow = viewport == C4JRender::eViewportType_FullScreen;
    if (fullWindow)
    {
        gRenderer->SetLayout(Renderer::ELayout_Screen);
        screenWidth = gConsoleUIController.getScreenHeight() * gRenderer->GetScreenAspect();
    }
#endif
    GuiComponent gui;
    BeginScreenDraw(originX, originY, ScreenUnit(width, height), screenWidth);
    Draw(gui, *minecraft->mFont, 2, 2, values, look, NULL, NULL);
    EndScreenDraw();
#if LCE_PLATFORM_PC
    if (fullWindow)
        gRenderer->SetLayout(layout);
#endif
}

float PixelScale()
{
#if LCE_PLATFORM_PC
    const float scale = gRenderer->GetPixelScale();
    return scale > 0.0f ? scale : 1.0f;
#else
    return 1.0f;
#endif
}

float ScreenUnit(int width, int height)
{
    const float fit = static_cast<float>(width) / 640.0f < static_cast<float>(height) / 360.0f
                          ? static_cast<float>(width) / 640.0f
                          : static_cast<float>(height) / 360.0f;
    if (fit >= 2.0f)
        return 2.0f * floorf(fit * 0.5f);
    if (fit >= 1.0f)
        return 1.0f;
    return fit > 0.25f ? fit : 0.25f;
}

float SnapToPixel(float value)
{
    const float pixels = PixelScale();
    return floorf(value * pixels + 0.5f) / pixels;
}

void ViewportOrigin(int viewport, int* x, int* y)
{
    int width = 0;
    int height = 0;
    ViewportArea(viewport, x, y, &width, &height);
}

void ViewportArea(int viewport, int* x, int* y, int* width, int* height)
{
    const int screenWidth = static_cast<int>(gConsoleUIController.getScreenWidth());
    const int screenHeight = static_cast<int>(gConsoleUIController.getScreenHeight());
    *x = 0;
    *y = 0;
    *width = screenWidth;
    *height = screenHeight;
    switch (viewport)
    {
    case C4JRender::eViewportType_TopHalf:
        *height = screenHeight / 2;
        break;
    case C4JRender::eViewportType_BottomHalf:
        *y = screenHeight / 2;
        *height = screenHeight - *y;
        break;
    case C4JRender::eViewportType_LeftHalf:
        *width = screenWidth / 2;
        break;
    case C4JRender::eViewportType_RightHalf:
        *x = screenWidth / 2;
        *width = screenWidth - *x;
        break;
    case C4JRender::eViewportType_TopLeftQuarter:
        *width = screenWidth / 2;
        *height = screenHeight / 2;
        break;
    case C4JRender::eViewportType_TopRightQuarter:
        *x = screenWidth / 2;
        *width = screenWidth - *x;
        *height = screenHeight / 2;
        break;
    case C4JRender::eViewportType_BottomLeftQuarter:
        *y = screenHeight / 2;
        *width = screenWidth / 2;
        *height = screenHeight - *y;
        break;
    case C4JRender::eViewportType_BottomRightQuarter:
        *x = screenWidth / 2;
        *y = screenHeight / 2;
        *width = screenWidth - *x;
        *height = screenHeight - *y;
        break;
    default:
        break;
    }
}

void BeginScreenDraw(float originX, float originY, float unit, float screenWidth)
{
    GlStateManager::matrixMode(1);
    GlStateManager::pushMatrix();
    GlStateManager::loadIdentity();
    GlStateManager::ortho(0.0, screenWidth > 0.0f ? screenWidth : gConsoleUIController.getScreenWidth(),
                          gConsoleUIController.getScreenHeight(), 0.0, 1000.0, 3000.0);
    GlStateManager::matrixMode(0);
    GlStateManager::pushMatrix();
    GlStateManager::loadIdentity();
    GlStateManager::translatef(originX, originY, -2000.0f);
    GlStateManager::scalef(unit, unit, 1.0f);
    gRenderer->Set_matrixDirty();
    GlStateManager::disableDepthTest();
    GlStateManager::disableAlphaTest();
}

void EndScreenDraw()
{
    GlStateManager::enableTexture();
    GlStateManager::disableBlend();
    GlStateManager::matrixMode(1);
    GlStateManager::popMatrix();
    GlStateManager::matrixMode(0);
    GlStateManager::popMatrix();
    gRenderer->Set_matrixDirty();
}
}
