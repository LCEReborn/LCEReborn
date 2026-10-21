#ifndef GAME_UI_UICUSTOMMENUPAGE_HPP
#define GAME_UI_UICUSTOMMENUPAGE_HPP

class UIScene_CustomMenu;

class UICustomMenuPage
{
public:
    virtual ~UICustomMenuPage();

    virtual void Build(UIScene_CustomMenu& menu) = 0;
    virtual void OnPress(UIScene_CustomMenu& menu, int id);
    virtual void OnToggle(UIScene_CustomMenu& menu, int id, bool checked);
    virtual void OnSlide(UIScene_CustomMenu& menu, int id, int value);
    virtual void OnClose(UIScene_CustomMenu& menu);
    virtual void OnGainFocus(UIScene_CustomMenu& menu);
    virtual bool OnAction(UIScene_CustomMenu& menu, int action);
    virtual bool KeepDescriptionOpen() const;
    virtual bool GetTooltips(int& tooltipX, int& tooltipY) const;
    virtual bool ShowLogo() const;
};

#endif
