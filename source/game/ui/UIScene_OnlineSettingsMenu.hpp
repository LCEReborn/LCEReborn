#ifndef GAME_UI_UISCENE_ONLINESETTINGSMENU_HPP
#define GAME_UI_UISCENE_ONLINESETTINGSMENU_HPP

#include <string>
#include <vector>

#include "game/ui/UIControl_Button.hpp"
#include "game/ui/UIControl_HTMLLabel.hpp"
#include "game/ui/UIScene.hpp"
#include "platform/C4JStorage.hpp"

class BufferedImage;

class UIScene_OnlineSettingsMenu : public UIScene
{
public:
    enum EMode
    {
        eMode_BeforeLoad,
        eMode_OpenToLan,
        eMode_LanWorld
    };

    UIScene_OnlineSettingsMenu(int playerId, void* data, UILayer* layer);
    virtual ~UIScene_OnlineSettingsMenu();

    virtual std::wstring getMoviePath();
    virtual unsigned int mapElementsAndNames();
    virtual int getSceneType();
    virtual void tick();
    virtual void updateTooltips();
    virtual void updateComponents();
    virtual void handleInput(int playerId, int action, bool repeat, bool pressed, bool released, bool& handled);
    virtual void handlePress(int controlId, int childId);
    virtual void handleFocusChange(int focus, int childFocus);
    virtual void render(int width, int height, C4JRender::eViewportType viewport);

    static void Open(int playerId, EMode mode);
    static std::wstring Summary();

private:
    struct PlayerRow
    {
        std::wstring name;
        std::wstring note;
        unsigned int skinId;
        int defaultSkin;
        int texture;
        BufferedImage* image;
    };

    enum ERow
    {
        eRow_Port,
        eRow_Visible,
        eRow_Message,
        eRow_GameMode,
        eRow_HostPrivileges,
        eRow_Count
    };

    enum
    {
        eControl_Top = 0,
        eControl_FirstRow = 1,
        eConfirm_None = -1,
        eConfirm_Stop = -2
    };

    void LayoutRows();
    void RefreshRow(int row);
    void RefreshDescription();
    void RefreshInfo();
    int FocusedRow() const;
    void ChangeRow(int row, int step);
    void SetGameMode(int gameType);
    void SetHostPrivileges(bool enabled);
    void SendSetting(int setting, unsigned int value);
    void StartLanWorld();
    void StopLanWorld();
    void RequestText(int row);
    void Confirm(int titleId, int messageId, int acceptId, int pending);
    std::wstring RowLabel(int row) const;
    std::wstring RowValue(int row) const;
    std::wstring RowDescription(int row) const;
    std::wstring NetworkInfo() const;
    void RefreshPlayers();
    void ReleaseHeads();
    void DrawPlayers(int width, int height, C4JRender::eViewportType viewport);

    static bool AchievementsEnabled();
    static std::wstring GameModeName(int gameType);
    static std::string RowName(int row);
    static int TextEntered(void* context, bool accepted);
    static int ConfirmReturned(void* context, int pad, C4JStorage::EMessageResult result);

    EMode m_mode;
    int m_rowCount;
    int m_gameMode;
    bool m_hostPrivileges;
    bool m_achievementsWarned;
    int m_pending;
    std::wstring m_error;
    std::vector<PlayerRow> m_players;
    long long m_lastPlayerCheck;
    bool m_focusPlaced;
    UIControl_Button m_topButton;
    UIControl_Button m_rows[eRow_Count];
    UIControl_HTMLLabel m_title;
    UIControl_HTMLLabel m_description;
    UIControl_HTMLLabel m_info;

    static int sTextRow;
};

#endif
