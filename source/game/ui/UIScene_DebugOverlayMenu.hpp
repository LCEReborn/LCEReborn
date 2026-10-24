#ifndef GAME_UI_UISCENE_DEBUGOVERLAYMENU_HPP
#define GAME_UI_UISCENE_DEBUGOVERLAYMENU_HPP

#include <string>

#include "game/core/DebugOverlay.hpp"
#include "game/core/GuiComponent.hpp"
#include "game/ui/UIMenuPainter.hpp"
#include "game/ui/UIScene.hpp"

class UIScene_DebugOverlayMenu : public UIScene
{
public:
    UIScene_DebugOverlayMenu(int playerId, void* data, UILayer* layer);
    virtual ~UIScene_DebugOverlayMenu();

    virtual std::wstring getMoviePath();
    virtual int getSceneType();
    virtual void tick();
    virtual void updateTooltips();
    virtual void updateComponents();
    virtual void render(int width, int height, C4JRender::eViewportType viewport);
    virtual void handleInput(int playerId, int action, bool repeat, bool pressed, bool released, bool& handled);
    virtual bool handlesPointer();

private:
    enum
    {
        ePreset_Minimal,
        ePreset_Builder,
        ePreset_Speedrun,
        ePreset_Everything,
        ePreset_Default,
        ePreset_Count
    };

    void Animate();
    void MoveFocus(int step);
    void ChangeState(int step);
    void SetEntryState(int entry, int state);
    void HandlePointer();
    void Click(float x, float y);
    int RowAt(float x, float y) const;
    int SwitchAt(int entry, float x, float y) const;
    int PresetAt(float x, float y) const;
    void ApplyPreset(int preset);
    int MatchingPreset() const;
    static int PresetState(int preset, int entry);
    static const wchar_t* PresetName(int preset);

    int RowTop(int entry) const;
    int ContentHeight() const;
    int ListBottom() const;
    int PresetTop() const;
    void DrawList(MenuPainter& painter);
    void DrawPresets(MenuPainter& painter);
    void DrawPreview(MenuPainter& painter, Font& font);
    void DrawSwitch(MenuPainter& painter, int x, int y, int entry);

    GuiComponent m_gui;
    MenuFrame m_frame;
    int m_focus;
    int m_preset;
    bool m_previewOpen;
    float m_focusY;
    float m_scroll;
    float m_carousel;
    float m_zoom;
    float m_cameraX;
    float m_cameraY;
    float m_pulse;
    float m_elapsed;
    long long m_lastTime;
    long long m_changeTime[DebugOverlay::EEntry_Count];
    long long m_pendingTime[DebugOverlay::EEntry_Count];
    int m_pendingState[DebugOverlay::EEntry_Count];
    DebugOverlay::Look m_look;
    float m_pointerX;
    float m_pointerY;
    float m_scrollTarget;
    bool m_mouseScroll;
};

#endif
