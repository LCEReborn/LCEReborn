#ifndef PLATFORM_PC_PCSHADERMENU_HPP
#define PLATFORM_PC_PCSHADERMENU_HPP

#include <memory>
#include <string>
#include <vector>

#include "game/core/GuiComponent.hpp"
#include "game/ui/UIMenuPainter.hpp"
#include "game/ui/UIScene.hpp"

class UILayer;
class ShaderPackOptions;
class ShaderPackProperties;
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
        eModal_Download,
        eModal_Discard
    };

    struct Target
    {
        int kind;
        int index;
        MenuRect rect;
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

    struct Scroll
    {
        float position;
        float target;
        bool mouse;
    };

    struct Notice
    {
        std::wstring text;
        unsigned int colour;
        long long time;
    };

    struct Flash
    {
        int kind;
        int index;
        long long time;
    };

    struct Paragraph
    {
        std::wstring text;
        unsigned int colour;
        int gap;
    };

    struct Text
    {
        void Add(const std::wstring& line, unsigned int colour, int gap);
        void AddSentences(const std::wstring& text, unsigned int colour, int gap);

        std::vector<Paragraph> paragraphs;
    };

    static void HandleDrop(const char* path, void* context);

    int ListBottom() const;
    int SettingsBottom() const;
    int GridBottom() const;
    int CommentTop() const;
    int ButtonsTop() const;
    MenuRect ModalRect() const;

    void BuildTargets();
    void BuildFixedTargets();
    void BuildPackTargets();
    void BuildOptionTargets();
    void BuildSettingTargets();
    void BuildModalTargets();
    void FixFocus();
    void AddTarget(int kind, int index, const MenuRect& rect, int view, bool enabled);
    int FindTarget(const Focus& focus) const;
    int TargetAt(float x, float y) const;
    MenuRect ScreenRect(const Target& target) const;
    MenuRect ViewRect(int view) const;
    int ContentHeight(int view) const;
    float MaxScroll(int view) const;
    Focus& CurrentFocus();
    void SetFocus(const Target& target, bool keyboard);
    void Move(int dx, int dy);
    void EnsureVisible(const Target& target);
    void ScrollView(int view, float amount);
    void ResetScroll(int view, float position);

    void SetTab(int tab);
    void CycleTab(int step);
    bool TabEnabled(int tab) const;
    void Activate(const Target& target);
    void Change(const Target& target, int step);
    void ResetTarget(const Target& target);
    bool Resettable(const Target& target) const;
    bool Changeable(const Target& target) const;
    bool ChangesSideways(const Target& target) const;
    void StartFlash(const Target& target);
    void Back();
    void Click(float x, float y, unsigned int clicks);
    void HandlePointer();
    void HandleModalInput(int action, bool repeat);
    void DragSlider(float x);
    void Animate();

    void SyncWithManager();
    void ResetDraft();
    void EditDraft();
    bool SplitScreen() const;

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
    bool CellChangeable(int index) const;
    bool IsSlider(const Cell& cell) const;
    int SliderPosition(const Cell& cell, int* count) const;
    void SetSliderPosition(const Cell& cell, int position);
    std::wstring CellValue(const Cell& cell, unsigned int* colour) const;
    void CellValueRect(int index, int* x0, int* x1) const;

    void ChangeSetting(int setting, int step);
    bool SettingModified(int setting) const;

    bool PackChangePending() const;
    bool SettingsChangePending() const;
    bool OptionsChangePending() const;
    bool HasPendingChanges() const;
    void Apply(bool reload);
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
    Text DescribeButton(int kind, int index) const;
    Text DescribePack(int index) const;
    Text DescribeCell(int index) const;
    Text DescribeSetting(int setting) const;
    std::wstring Breadcrumb() const;
    std::wstring ButtonLabel(int kind, int index) const;

    void DrawTitle(MenuPainter& painter);
    void DrawTabs(MenuPainter& painter, bool selected);
    void DrawButtons(MenuPainter& painter, bool modal);
    void DrawPacks(MenuPainter& painter);
    void DrawToggleRow(MenuPainter& painter, const MenuRect& rect, bool focused);
    void DrawPackRow(MenuPainter& painter, int index, const MenuRect& rect, bool focused);
    void DrawOptions(MenuPainter& painter);
    void DrawCell(MenuPainter& painter, int index, const MenuRect& rect, bool focused);
    void DrawSettings(MenuPainter& painter);
    void DrawDetails(MenuPainter& painter);
    void DrawModal(MenuPainter& painter);
    int DrawParagraphs(MenuPainter& painter, const Text& text, const MenuRect& area, int scroll);
    bool Focused(int kind, int index) const;
    float FlashAmount(int kind, int index) const;

    GuiComponent m_gui;
    MenuFrame m_frame;
    int m_tab;
    Focus m_focus[eTab_Count];
    std::vector<Target> m_targets;
    std::vector<std::string> m_packs;
    std::vector<Cell> m_cells;
    int m_columns;
    std::string m_screen;
    std::vector<Page> m_history;
    std::unique_ptr<ShaderSettings> m_working;
    std::unique_ptr<ShaderSettings> m_applied;
    std::unique_ptr<ShaderPackOptions> m_options;
    std::unique_ptr<ShaderPackProperties> m_properties;
    std::string m_optionsPack;
    std::string m_appliedOptions;
    Scroll m_scroll[eView_Count];
    int m_detailHeight;
    Notice m_notice;
    Flash m_flash;
    int m_modal;
    int m_modalFocus;
    bool m_hidden;
    bool m_awaitingReload;
    long long m_reloadTime;
    long long m_lastTime;
    long long m_lastRefresh;
    float m_pulse;
    int m_dragIndex;
    float m_pointerX;
    float m_pointerY;
    std::vector<std::string> m_dropped;
    bool m_dropComplete;
    int m_tooltips[16];
};

#endif
