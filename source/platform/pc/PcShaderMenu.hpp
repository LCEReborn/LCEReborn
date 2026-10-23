#ifndef PLATFORM_PC_PCSHADERMENU_HPP
#define PLATFORM_PC_PCSHADERMENU_HPP

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "game/core/GuiComponent.hpp"
#include "game/ui/UIScene.hpp"

class Font;
class UILayer;
struct ShaderOption;
struct ShaderSettings;

class UIScene_ShaderMenu : public UIScene
{
public:
    UIScene_ShaderMenu(int playerId, void* data, UILayer* layer);
    virtual ~UIScene_ShaderMenu();

    virtual std::wstring getMoviePath();
    virtual int getSceneType();
    virtual void tick();
    virtual void updateTooltips();
    virtual void updateComponents();
    virtual void render(int width, int height, C4JRender::eViewportType viewport);
    virtual void handleInput(int playerId, int action, bool repeat, bool pressed, bool released, bool& handled);
    virtual bool handlesPointer();

private:
    enum ETab
    {
        eTab_Packs,
        eTab_Options,
        eTab_Settings,
        eTab_Count
    };

    enum ETarget
    {
        eTarget_None,
        eTarget_Hide,
        eTarget_Tab,
        eTarget_Cancel,
        eTarget_Apply,
        eTarget_Done,
        eTarget_Toggle,
        eTarget_Pack,
        eTarget_OpenFolder,
        eTarget_Download,
        eTarget_Back,
        eTarget_Import,
        eTarget_Export,
        eTarget_Reset,
        eTarget_Cell,
        eTarget_Setting,
        eTarget_Modal
    };

    enum EView
    {
        eView_Fixed,
        eView_List,
        eView_Grid,
        eView_Settings,
        eView_Details,
        eView_Count
    };

    enum ECell
    {
        eCell_Empty,
        eCell_Option,
        eCell_Link,
        eCell_Profile
    };

    enum EModal
    {
        eModal_None,
        eModal_Reset,
        eModal_Download
    };

    struct Target
    {
        int kind;
        int index;
        int x0;
        int y0;
        int x1;
        int y1;
        int view;
        bool enabled;
    };

    struct Focus
    {
        int kind;
        int index;
    };

    struct Cell
    {
        int kind;
        std::string name;
    };

    struct Page
    {
        std::string screen;
        Focus focus;
        float scroll;
    };

    struct Paragraph
    {
        std::wstring text;
        unsigned int colour;
        int gap;
    };

    typedef std::vector<Paragraph> Text;

    static void HandleDrop(const char* path, void* context);

    void BuildTargets();
    void AddTarget(int kind, int index, int x0, int y0, int x1, int y1, int view, bool enabled);
    int FindTarget(const Focus& focus) const;
    int TargetAt(float x, float y) const;
    void ScreenRect(const Target& target, int* x0, int* y0, int* x1, int* y1) const;
    void ViewRect(int view, int* x0, int* y0, int* x1, int* y1) const;
    int ContentHeight(int view) const;
    float MaxScroll(int view) const;
    Focus& CurrentFocus();
    void SetFocus(const Target& target, bool keyboard);
    void Move(int dx, int dy);
    void EnsureVisible(const Target& target);
    void ScrollView(int view, float amount);

    void SetTab(int tab);
    void CycleTab(int step);
    bool TabEnabled(int tab) const;
    void Activate(const Target& target);
    void Change(const Target& target, int step);
    void ResetTarget(const Target& target);
    bool Resettable(const Target& target) const;
    bool ChangesSideways(const Target& target) const;
    void Back();
    void Click(float x, float y, unsigned int clicks);
    void HandlePointer();
    void DragSlider(float x);
    void Animate();

    void SetShadersOn(bool on);
    void RefreshPacks(bool force);
    void FocusSelectedPack();
    void SelectPack(int index);
    std::string PackName(int index) const;
    bool PackUsable(int index) const;

    void BuildCells();
    void OpenScreen(const std::string& screen);
    void CloseScreen();
    void CycleOption(const std::string& name, int step);
    void CycleProfile(int step);
    bool OptionModified(const std::string& name) const;
    std::string AppliedValue(const ShaderOption& option) const;
    bool CellChangeable(int index) const;
    bool IsSlider(const Cell& cell) const;
    int SliderPosition(const Cell& cell, int* count) const;
    void SetSliderPosition(const Cell& cell, int position);
    void CellValueRect(int index, int* x0, int* x1) const;

    void ChangeSetting(int setting, int step);
    bool SettingModified(int setting) const;

    void SnapshotOptions();
    bool PackChangePending() const;
    bool SettingsChangePending() const;
    bool OptionsChangePending() const;
    bool HasPendingChanges() const;
    void Apply();
    void Cancel();
    void Done();
    void OpenPackFolder();
    void ImportSettings(const std::string& path);
    void ExportSettings(const std::string& path);
    void ShowFileDialog(bool save);
    void HandleDialogResult();
    void HandleDrops();
    void CheckReload();
    void ResetAllOptions();
    void OpenModal(int modal);
    void CloseModal(bool accept);
    void Notify(const std::wstring& text, unsigned int colour);
    void ToggleHidden();

    Text DescribeFocus() const;
    Text DescribePack(int index) const;
    Text DescribeCell(int index) const;
    Text DescribeSetting(int setting) const;
    std::wstring Breadcrumb() const;

    void Clip(int x0, int y0, int x1, int y1);
    void Unclip();
    void DrawTitle(Font& font);
    void DrawTabs(Font& font, bool selected);
    void DrawActions(Font& font);
    void DrawPacks(Font& font);
    void DrawOptions(Font& font);
    void DrawSettings(Font& font);
    void DrawCell(Font& font, int index, int x0, int y0, int x1, int y1, bool focused);
    void DrawDetails(Font& font);
    void DrawModal(Font& font);
    int DrawParagraphs(Font& font, const Text& text, int x0, int y0, int x1, int y1, int scroll);
    bool Focused(int kind, int index) const;
    float Flash(int kind, int index) const;

    GuiComponent m_gui;
    int m_tab;
    Focus m_focus[eTab_Count];
    std::vector<Target> m_targets;
    std::vector<std::string> m_packs;
    std::vector<Cell> m_cells;
    int m_columns;
    std::string m_screen;
    std::vector<Page> m_history;
    std::unique_ptr<ShaderSettings> m_workingSettings;
    std::unique_ptr<ShaderSettings> m_appliedSettings;
    std::string m_appliedOptions;
    std::map<std::string, std::string> m_appliedValues;
    float m_scroll[eView_Count];
    float m_scrollTarget[eView_Count];
    bool m_mouseScroll[eView_Count];
    int m_detailHeight;
    std::wstring m_notice;
    unsigned int m_noticeColour;
    long long m_noticeTime;
    int m_modal;
    int m_modalFocus;
    bool m_hidden;
    bool m_awaitingReload;
    long long m_reloadTime;
    long long m_lastTime;
    long long m_lastRefresh;
    float m_pulse;
    int m_flashKind;
    int m_flashIndex;
    long long m_flashTime;
    int m_dragIndex;
    int m_originX;
    int m_originY;
    float m_unit;
    float m_pointerX;
    float m_pointerY;
    unsigned int m_savedScissor[4];
    std::vector<std::string> m_dropped;
    bool m_dropComplete;
    int m_tooltips[16];
};

#endif
