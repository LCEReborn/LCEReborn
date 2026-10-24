#ifndef GAME_CORE_DEBUGOVERLAY_HPP
#define GAME_CORE_DEBUGOVERLAY_HPP

#include <string>

class Font;
class GuiComponent;

namespace DebugOverlay
{
enum EEntry
{
    EEntry_Position,
    EEntry_Block,
    EEntry_Chunk,
    EEntry_Facing,
    EEntry_Dimension,
    EEntry_Biome,
    EEntry_Light,
    EEntry_Time,
    EEntry_Weather,
    EEntry_FrameRate,
    EEntry_ChunkUpdates,
    EEntry_Entities,
    EEntry_Memory,
    EEntry_System,
    EEntry_Server,
    EEntry_Players,
    EEntry_Count
};

enum EState
{
    EState_WithF3,
    EState_Always,
    EState_Off,
    EState_Count
};

enum EGroup
{
    EGroup_Position,
    EGroup_World,
    EGroup_Performance,
    EGroup_System,
    EGroup_Server,
    EGroup_Count
};

struct Look
{
    float amount[EEntry_Count];
    float flash[EEntry_Count];
    int highlight;
    float pulse;
};

int Group(int entry);
const wchar_t* GroupName(int group);
const wchar_t* EntryName(int entry);
const wchar_t* StateName(int state);

int GetState(int pad, int entry);
void SetState(int pad, int entry, int state);
bool IsShown(int pad, int entry, bool open);

bool IsOpen(int pad);
void Toggle(int pad);
void Values(int pad, std::wstring values[EEntry_Count]);

void Draw(GuiComponent& gui, Font& font, int x, int y, const std::wstring values[EEntry_Count], const Look& look,
          int* highlightX, int* highlightY);
void RenderScreen(int pad, int viewport, int width, int height);

float PixelScale();
float ScreenUnit(int width, int height);
float SnapToPixel(float value);
void ViewportOrigin(int viewport, int* x, int* y);
void ViewportArea(int viewport, int* x, int* y, int* width, int* height);
void BeginScreenDraw(float originX, float originY, float unit, float screenWidth = 0.0f);
void EndScreenDraw();
}

#endif
