#include "game/ui/UIScene_OnlineSettingsMenu.hpp"

#include <stdint.h>
#include <cstring>
#include <cwchar>

#include <boost/shared_ptr.hpp>

#include "game/client/audio/SoundEvents.hpp"
#include "game/core/CConsoleMinecraftApp.hpp"
#include "game/core/ConsoleUIController.hpp"
#include "game/core/ExtraStrings.hpp"
#include "game/core/DebugOverlay.hpp"
#include "game/core/GameSettings.hpp"
#include "game/core/GameType.hpp"
#include "game/core/GlStateManager.hpp"
#include "game/core/Gui.hpp"
#include "game/core/HtmlString.hpp"
#include "game/core/L10N.hpp"
#include "game/core/Minecraft.hpp"
#include "game/core/StringFormat.hpp"
#include "game/core/StringIds.hpp"
#include "game/core/StringUtils.hpp"
#include "game/core/System.hpp"
#include "game/core/VideoSettings.hpp"
#include "game/network/CGameNetworkManager.hpp"
#include "game/network/ClientPacketListener.hpp"
#include "game/network/INetworkPlayer.hpp"
#include "game/network/ServerSettingsChangedPacket.hpp"
#include "game/client/render/Font.hpp"
#include "game/client/render/Tesselator.hpp"
#include "game/client/render/Textures.hpp"
#include "game/core/BufferBuilder.hpp"
#include "game/core/BufferedImage.hpp"
#include "game/ui/FJ_Base.hpp"
#include "game/ui/FJ_Button.hpp"
#include "game/ui/FJ_Factory.hpp"
#include "game/ui/FJ_FuiNode.hpp"
#include "game/ui/FJ_FuiNodeStage.hpp"
#include "game/ui/FJ_HtmlText.hpp"
#include "game/ui/UILayer.hpp"
#include "game/ui/UIString.hpp"
#include "game/ui/fuiFile.hpp"
#include "game/ui/fuiRenderNode.hpp"
#include "game/world/Level.hpp"
#include "game/world/generation/MultiplayerLocalPlayer.hpp"
#include "game/world/generation/Player.hpp"
#if LCE_PLATFORM_PC
#include "platform/pc/PcAvatar.hpp"
#endif
#include "platform/CInput.hpp"
#include "platform/CProfile.hpp"

extern ConsoleUIController gConsoleUIController;

int UIScene_OnlineSettingsMenu::sTextRow = -1;

namespace
{
const char* const kRowSymbol = "fourj.Buttons.FJ_ListButtonController_Normal";
const char* const kTextSymbol = "fourj.HtmlText.FJ_HtmlText_WhiteNormal";
const float kRowLeft = 242.0f;
const float kRowTop = 264.0f;
const float kRowWidth = 426.0f;
const float kRowStep = 48.0f;
const float kDescriptionLeft = 250.0f;
const float kDescriptionWidth = 420.0f;
const float kDescriptionHeight = 84.0f;
const long long kPlayerCheckMilliseconds = 1000;
const float kMovieWidth = 1280.0f;
const float kMovieHeight = 720.0f;
const float kPlayersTop = 92.0f;
const float kPlayersInset = 20.0f;
const float kPlayerStep = 34.0f;
const float kHeadSize = 28.0f;
const int kHeadPixels = 56;

std::wstring Html(const std::wstring& text, eMinecraftColour colour)
{
    return HtmlString(text, colour, false, false, 0).ToString();
}

std::wstring Text(const std::wstring& text)
{
    return Html(text, eMinecraftColour_HTMLColor_f);
}

std::wstring Muted(const std::wstring& text)
{
    return Html(text, eMinecraftColour_HTMLColor_7);
}

std::wstring Address(const std::wstring& text)
{
    return Html(text, eMinecraftColour_HTMLColor_b);
}

std::wstring PanelText(const std::wstring& text)
{
    return L"<font color=\"#3f3f3f\">" + escapeXML(text) + L"</font>";
}

std::wstring PortText()
{
    return formatwstr(L"%d", VideoSettings::GetHostPort());
}

std::wstring MessageText()
{
    const std::wstring message = VideoSettings::GetHostMessage();
    return message.empty() ? std::wstring(L"None") : message;
}

int LocalPlayerCount()
{
    int count = 0;
    for (int index = 0; index < CGameNetworkManager::sInstance.GetPlayerCount(); ++index)
    {
        INetworkPlayer* player = CGameNetworkManager::sInstance.GetPlayerByIndex(index);
        if (player != NULL && player->IsLocal())
            ++count;
    }
    return count;
}

void Attach(fuiRenderNode* root, FJ_FuiNode* node)
{
    if (node != NULL && root->getFuiNode() != NULL)
        root->getFuiNode()->addChild(node);
}

void Place(FJ_FuiNode* node, float x, float y, float width, float height)
{
    if (node == NULL)
        return;
    node->setX(x);
    node->setY(y);
    node->setWidth(width);
    node->setHeight(height);
}

FJ_FuiNode* NodeOf(UIControl& control)
{
    fuiRenderNode* renderNode = control.getFuiRenderNode();
    return renderNode != NULL ? renderNode->getFuiNode() : NULL;
}
}

UIScene_OnlineSettingsMenu::UIScene_OnlineSettingsMenu(int playerId, void* data, UILayer* layer)
    : UIScene(playerId, layer),
      m_mode(static_cast<EMode>(reinterpret_cast<intptr_t>(data))),
      m_rowCount(0),
      m_gameMode(static_cast<int>(gApp.GetGameHostOption(eGameHostOption_GameType))),
      m_hostPrivileges(gApp.GetGameHostOption(eGameHostOption_CheatsEnabled) != 0),
      m_achievementsWarned(false),
      m_pending(eConfirm_None),
      m_lastPlayerCheck(0),
      m_focusPlaced(false)
{
    if (m_gameMode < GameType::SURVIVAL->getId() || m_gameMode > GameType::ADVENTURE->getId())
        m_gameMode = GameType::SURVIVAL->getId();
    m_rowCount = m_mode == eMode_BeforeLoad ? eRow_Message + 1 : static_cast<int>(eRow_Count);
    initialiseMovie();

    if (m_mode == eMode_BeforeLoad)
    {
        m_topButton.init(UIString(L10N::GetString(IDS_LCE_ONLINE_SETTINGS)), eControl_Top);
        m_topButton.setVisible(false);
        if (m_title.getFuiRenderNode() != NULL)
            m_title.init(UIString(L"<p align=\"center\">" + Text(L10N::GetString(IDS_LCE_ONLINE_SETTINGS)) + L"</p>"));
    }
    else
    {
        m_topButton.init(UIString(m_mode == eMode_OpenToLan ? std::wstring(L"Start LAN World")
                                                            : std::wstring(L10N::GetString(IDS_LCE_STOP_LAN_WORLD))),
                         eControl_Top);
    }
    for (int row = 0; row < m_rowCount; ++row)
        if (m_rows[row].getFuiRenderNode() != NULL)
            m_rows[row].init(UIString(RowLabel(row)), eControl_FirstRow + row);
    if (m_description.getFuiRenderNode() != NULL)
        m_description.init(UIString(L""));
    m_info.init(UIString(L""));

    LayoutRows();
    for (int row = 0; row < m_rowCount; ++row)
        RefreshRow(row);
    if (m_mode == eMode_LanWorld)
        m_rows[eRow_Port].setEnable(false, true);
    RefreshPlayers();
    RefreshInfo();
    RefreshDescription();
    gConsoleUIController.TouchBoxRebuild(this);
}

void UIScene_OnlineSettingsMenu::Open(int playerId, EMode mode)
{
    gConsoleUIController.NavigateToScene(playerId, EUIScene_OnlineSettingsMenu,
                                         reinterpret_cast<void*>(static_cast<intptr_t>(mode)), EUILayer_Scene,
                                         EUIGroup_PlayerDefault);
}

std::wstring UIScene_OnlineSettingsMenu::getMoviePath()
{
    return std::wstring(L"JoinMenu");
}

int UIScene_OnlineSettingsMenu::getSceneType()
{
    return EUIScene_OnlineSettingsMenu;
}

std::string UIScene_OnlineSettingsMenu::RowName(int row)
{
    char name[16];
    std::snprintf(name, sizeof(name), "LanRow%d", row);
    return name;
}

unsigned int UIScene_OnlineSettingsMenu::mapElementsAndNames()
{
    UIScene::mapElementsAndNames();
    fuiRenderNode* root = m_movie->getRootNode();
    m_topButton.setupControl(this, root, "JoinGame");
    m_controls.push_back(&m_topButton);
    m_info.setupControl(this, root, "GameInfo");
    m_controls.push_back(&m_info);

    fuiRenderNode* players = root->findNode("GamePlayers");
    if (players != NULL && players->getFuiNode() != NULL)
        players->getFuiNode()->setVisible(false);

    for (int row = 0; row < m_rowCount; ++row)
    {
        const std::string name = RowName(row);
        Attach(root, FJ_Factory::createObject(kRowSymbol, root, true, name.c_str()));
        m_rows[row].setupControl(this, root, name);
        m_controls.push_back(&m_rows[row]);
    }
    FJ_FuiNode* description = FJ_Factory::createObject(kTextSymbol, root, true, "LanDescription");
    Attach(root, description);
    if (description != NULL && description->isFJ_HtmlText())
        description->asFJ_HtmlText()->m_bNoBackground = true;
    Place(description, kDescriptionLeft, kRowTop + kRowStep * eRow_Count + 4.0f, kDescriptionWidth,
          kDescriptionHeight);
    m_description.setupControl(this, root, "LanDescription");
    m_controls.push_back(&m_description);
    if (m_mode == eMode_BeforeLoad)
    {
        FJ_FuiNode* top = NodeOf(m_topButton);
        FJ_FuiNode* title = FJ_Factory::createObject(kTextSymbol, root, true, "LanTitle");
        Attach(root, title);
        if (title != NULL && title->isFJ_HtmlText())
            title->asFJ_HtmlText()->m_bNoBackground = true;
        if (top != NULL)
            Place(title, top->getX(), top->getY() + 8.0f, top->getWidth(), top->getHeight());
        m_title.setupControl(this, root, "LanTitle");
        m_controls.push_back(&m_title);
    }
    return 1;
}

void UIScene_OnlineSettingsMenu::LayoutRows()
{
    FJ_FuiNode* top = NodeOf(m_topButton);
    for (int row = 0; row < m_rowCount; ++row)
    {
        FJ_FuiNode* node = NodeOf(m_rows[row]);
        if (node == NULL)
            continue;
        if (node->isFJ_Base())
            node->asFJ_Base()->SetNewObjectWidth(static_cast<int>(kRowWidth));
        node->setX(kRowLeft);
        node->setY(kRowTop + kRowStep * row);
        if (node->isFJ_Base())
        {
            const std::string up = row > 0 ? RowName(row - 1)
                                            : (m_mode == eMode_BeforeLoad ? std::string() : std::string("JoinGame"));
            const std::string down = row + 1 < m_rowCount ? RowName(row + 1) : std::string();
            node->asFJ_Base()->SetNavigation(up, down);
        }
        m_rows[row].UpdateControl();
    }
    if (top != NULL && top->isFJ_Base())
        top->asFJ_Base()->SetNavigation(std::string(), m_mode == eMode_BeforeLoad ? std::string() : RowName(0));

}

std::wstring UIScene_OnlineSettingsMenu::RowLabel(int row) const
{
    switch (row)
    {
    case eRow_Port:
        return L"Port";
    case eRow_Visible:
        return L"Show on local network";
    case eRow_Message:
        return L"Message";
    case eRow_GameMode:
        return L"Game Mode";
    default:
        return L10N::GetString(IDS_HOST_PRIVILEGES);
    }
}

std::wstring UIScene_OnlineSettingsMenu::RowValue(int row) const
{
    switch (row)
    {
    case eRow_Port:
        return m_mode == eMode_LanWorld ? formatwstr(L"%d", CGameNetworkManager::sInstance.GetListenPort())
                                        : PortText();
    case eRow_Visible:
        return VideoSettings::GetHostVisible() ? L"On" : L"Off";
    case eRow_Message:
        return MessageText();
    case eRow_GameMode:
        return GameModeName(m_gameMode);
    default:
        return m_hostPrivileges ? L"On" : L"Off";
    }
}

std::wstring UIScene_OnlineSettingsMenu::RowDescription(int row) const
{
    switch (row)
    {
    case eRow_Port:
        return m_mode == eMode_LanWorld ? L"The port other players connect to. Stop the LAN world to change it."
                                        : L"The port other players connect to. Kept for every game you host.";
    case eRow_Visible:
    {
        const std::wstring address = CGameNetworkManager::sInstance.LocalAddressText();
        if (m_mode == eMode_LanWorld && !VideoSettings::GetHostVisible() && !address.empty())
            return formatwstr(L"Hidden from the Join tab. Players on your network add it as a server with %ls:%d.",
                              address.c_str(), CGameNetworkManager::sInstance.GetListenPort());
        return L"When on, players on your network see this world in their Join tab.";
    }
    case eRow_Message:
        return m_mode == eMode_LanWorld ? L"Shown under the world name in the Join tab. Others see the change "
                                          L"right away."
                                        : L"Shown under the world name in the Join tab. Use & and a colour code "
                                          L"to add colour.";
    case eRow_GameMode:
        return L"The game mode for everyone in this world, including players who join.";
    default:
        return L"Lets the host fly, turn off hunger and go invisible, and use commands.";
    }
}

void UIScene_OnlineSettingsMenu::RefreshRow(int row)
{
    FJ_FuiNode* node = NodeOf(m_rows[row]);
    if (node != NULL && node->isFJ_Button())
        node->asFJ_Button()->SetSecondaryText(RowValue(row), false, false);
}

int UIScene_OnlineSettingsMenu::FocusedRow() const
{
    const int row = const_cast<UIScene_OnlineSettingsMenu*>(this)->getControlFocus() - eControl_FirstRow;
    return row >= 0 && row < m_rowCount ? row : -1;
}

void UIScene_OnlineSettingsMenu::RefreshDescription()
{
    std::wstring text = m_error;
    if (text.empty())
    {
        const int row = FocusedRow();
        if (row >= 0)
            text = RowDescription(row);
        else if (m_mode == eMode_OpenToLan)
            text = L"Lets other players join this world until you leave it or stop it.";
        else if (m_mode == eMode_LanWorld)
            text = L"Disconnects players who joined over the network and stops new ones joining.";
        else
            text = RowDescription(eRow_Port);
    }
    m_description.setLabel(UIString(PanelText(text)), true, false);
}

void UIScene_OnlineSettingsMenu::RefreshInfo()
{
    m_info.setLabel(UIString(NetworkInfo()), true, false);
}

std::wstring UIScene_OnlineSettingsMenu::NetworkInfo() const
{
    const std::wstring address = CGameNetworkManager::sInstance.LocalAddressText();
    const bool visible = VideoSettings::GetHostVisible();
    if (m_mode == eMode_LanWorld)
    {
        return Muted(formatwstr(L"Open on port %d", CGameNetworkManager::sInstance.GetListenPort())) + L"<br><br>"
               + Html(formatwstr(L"%ls: %d/%u", L10N::GetString(IDS_PLAYERS),
                                 CGameNetworkManager::sInstance.GetPlayerCount(),
                                 CGameNetworkManager::sInstance.GetMaxPlayers()),
                      eMinecraftColour_HTMLColor_6);
    }

    const int port = VideoSettings::GetHostPort();
    std::wstring info = Muted(L"On your network") + L"<br>";
    if (visible)
        info += Text(L"It shows up in their Join tab.");
    else if (address.empty())
        info += Text(L"Hidden from their Join tab. They add it as a server with this PC's address and the port.");
    else
        info += Text(L"Hidden from their Join tab. They add it as a server with ")
                + Address(formatwstr(L"%ls:%d", address.c_str(), port));
    info += L"<br><br>" + Muted(L"Over the internet") + L"<br>"
            + Text(formatwstr(L"They join with your public IP and port %d. On your router, forward that port to "
                              L"this PC",
                              port));
    info += address.empty() ? Text(L".") : Text(L":") + L"<br>" + Address(address);
    if (m_mode == eMode_OpenToLan)
        info += L"<br><br>"
                + Html(formatwstr(L"%ls: %d/%u", L10N::GetString(IDS_PLAYERS),
                                  CGameNetworkManager::sInstance.GetPlayerCount(),
                                  CGameNetworkManager::sInstance.GetMaxPlayers()),
                       eMinecraftColour_HTMLColor_6);
    return info;
}

std::wstring UIScene_OnlineSettingsMenu::Summary()
{
    return Text(L"Others can join once the world has loaded.") + L"<br><br>" + Muted(L"Port: ") + Text(PortText())
           + L"<br>" + Muted(L"Show on local network: ") + Text(VideoSettings::GetHostVisible() ? L"On" : L"Off")
           + L"<br>" + Muted(L"Message: ") + Text(MessageText());
}

void UIScene_OnlineSettingsMenu::tick()
{
    UIScene::tick();
    if (m_mode == eMode_BeforeLoad && !m_focusPlaced)
    {
        m_focusPlaced = true;
        FJ_FuiNode* first = NodeOf(m_rows[0]);
        if (first != NULL && first->getStage() != NULL)
            first->getStage()->setFocus(first);
        SetFocusToElement(eControl_FirstRow);
        RefreshDescription();
    }
    if (m_mode != eMode_LanWorld)
        return;
    const long long now = System::processTimeInMilliSecs();
    if (now - m_lastPlayerCheck < kPlayerCheckMilliseconds && now >= m_lastPlayerCheck)
        return;
    m_lastPlayerCheck = now;
    RefreshPlayers();
}

UIScene_OnlineSettingsMenu::~UIScene_OnlineSettingsMenu()
{
    ReleaseHeads();
}

void UIScene_OnlineSettingsMenu::RefreshPlayers()
{
    if (m_mode != eMode_LanWorld)
        return;
    CGameNetworkManager& network = CGameNetworkManager::sInstance;
    Minecraft* minecraft = Minecraft::GetInstance();
    std::vector<PlayerRow> players;
    for (int index = 0; index < network.GetPlayerCount(); ++index)
    {
        INetworkPlayer* player = network.GetPlayerByIndex(index);
        if (player == NULL)
            continue;
        PlayerRow row;
        row.name = player->GetOnlineName();
        row.note = player->IsHost() ? L"Host" : (player->IsLocal() ? L"This console" : L"");
        row.skinId = 0;
        row.defaultSkin = 1;
        row.texture = -1;
        row.image = NULL;
        if (player->IsLocal())
        {
            row.skinId = GameSettings::GetPlayerSkinId(player->GetUserIndex());
            row.defaultSkin = player->GetUserIndex() + 1;
        }
        else if (minecraft->mLevel != NULL)
        {
            for (unsigned int entity = 0; entity < minecraft->mLevel->mPlayers.size(); ++entity)
            {
                boost::shared_ptr<Player> other = minecraft->mLevel->mPlayers[entity];
                if (other != NULL && other->getName() == row.name)
                    row.skinId = static_cast<unsigned int>(other->mCustomSkin);
            }
        }
        players.push_back(row);
    }

    bool changed = players.size() != m_players.size();
    for (size_t index = 0; index < players.size() && !changed; ++index)
        changed = players[index].name != m_players[index].name || players[index].note != m_players[index].note
                  || players[index].skinId != m_players[index].skinId;
    if (!changed)
        return;
    ReleaseHeads();
    m_players = players;
    RefreshInfo();
}

void UIScene_OnlineSettingsMenu::ReleaseHeads()
{
    Minecraft* minecraft = Minecraft::GetInstance();
    for (size_t index = 0; index < m_players.size(); ++index)
    {
        if (m_players[index].texture >= 0 && minecraft != NULL && minecraft->getTextureManager() != NULL)
            minecraft->getTextureManager()->releaseTexture(m_players[index].texture);
        delete m_players[index].image;
        m_players[index].texture = -1;
        m_players[index].image = NULL;
    }
}

void UIScene_OnlineSettingsMenu::render(int width, int height, C4JRender::eViewportType viewport)
{
    UIScene::render(width, height, viewport);
    if (m_mode == eMode_LanWorld && m_hasTicked && !m_players.empty())
        DrawPlayers(width, height, viewport);
}

void UIScene_OnlineSettingsMenu::DrawPlayers(int width, int height, C4JRender::eViewportType viewport)
{
#if LCE_PLATFORM_PC
    Minecraft* minecraft = Minecraft::GetInstance();
    if (minecraft == NULL || minecraft->mFont == NULL || minecraft->getTextureManager() == NULL)
        return;
    Font& font = *minecraft->mFont;
    Textures* textures = minecraft->getTextureManager();

    int viewportX = 0;
    int viewportY = 0;
    DebugOverlay::ViewportOrigin(viewport, &viewportX, &viewportY);
    const float scaleX = static_cast<float>(width) / kMovieWidth;
    const float scaleY = static_cast<float>(height) / kMovieHeight;
    const float scale = scaleX < scaleY ? scaleX : scaleY;
    const int originX = viewportX + static_cast<int>((width - kMovieWidth * scale) * 0.5f);
    const int originY = viewportY + static_cast<int>((height - kMovieHeight * scale) * 0.5f);

    const float left = static_cast<float>(m_info.getXPos());
    const float right = left + static_cast<float>(m_info.getWidth());
    const float top = static_cast<float>(m_info.getYPos()) + kPlayersTop;
    DebugOverlay::BeginScreenDraw(originX, originY, scale);
    GlStateManager::enableBlend();
    GlStateManager::blendFunc(4, 5);
    GlStateManager::enableTexture();
    for (size_t index = 0; index < m_players.size(); ++index)
    {
        PlayerRow& row = m_players[index];
        const float y = top + kPlayerStep * static_cast<float>(index);
        if (row.texture < 0 && row.image == NULL)
        {
            row.image = PcRenderSkinHead(row.skinId, row.defaultSkin, kHeadPixels);
            if (row.image != NULL)
                row.texture = textures->getTexture(row.image, C4JRender::eTextureFormat_RGBA8, false);
        }
        if (row.texture >= 0)
        {
            textures->bind(row.texture);
            GlStateManager::color4f(1.0f, 1.0f, 1.0f, 1.0f);
            BufferBuilder* builder = Tesselator::getInstance()->getBuilder();
            const float x0 = left + kPlayersInset;
            builder->begin();
            builder->vertexUV(x0, y + kHeadSize, 0.0f, 0.0f, 1.0f);
            builder->vertexUV(x0 + kHeadSize, y + kHeadSize, 0.0f, 1.0f, 1.0f);
            builder->vertexUV(x0 + kHeadSize, y, 0.0f, 1.0f, 0.0f);
            builder->vertexUV(x0, y, 0.0f, 0.0f, 0.0f);
            builder->end();
        }
        const float textY = y + (kHeadSize - 16.0f) * 0.5f;
        GlStateManager::pushMatrix();
        GlStateManager::translatef(left + kPlayersInset + kHeadSize + 10.0f, textY, 0.0f);
        GlStateManager::scalef(2.0f, 2.0f, 1.0f);
        font.drawShadow(row.name, 0, 0, static_cast<int>(0xFFE0E0E0));
        GlStateManager::popMatrix();
        if (!row.note.empty())
        {
            const float noteWidth = static_cast<float>(font.width(row.note)) * 2.0f;
            GlStateManager::pushMatrix();
            GlStateManager::translatef(right - kPlayersInset - noteWidth, textY, 0.0f);
            GlStateManager::scalef(2.0f, 2.0f, 1.0f);
            font.drawShadow(row.note, 0, 0, static_cast<int>(0xFFAAAAAA));
            GlStateManager::popMatrix();
        }
    }
    DebugOverlay::EndScreenDraw();
#else
    (void)width;
    (void)height;
    (void)viewport;
#endif
}

void UIScene_OnlineSettingsMenu::updateTooltips()
{
    gConsoleUIController.SetTooltips(m_playerId, IDS_TOOLTIPS_SELECT, IDS_TOOLTIPS_BACK, -1, -1, -1, -1, -1, -1, -1,
                                     -1, -1, -1, -1, -1, -1, -1, false, false);
}

void UIScene_OnlineSettingsMenu::updateComponents()
{
    const bool inGame = Minecraft::GetInstance()->mLevel != NULL;
    m_layer->showComponent(m_playerId, EUIScene_Panorama, !inGame);
    m_layer->showComponent(m_playerId, EUIScene_Logo, !inGame || gApp.GetLocalPlayerCount() == 1);
}

void UIScene_OnlineSettingsMenu::handleInput(int, int action, bool repeat, bool pressed, bool released,
                                             bool& handled)
{
    gConsoleUIController.AnimateKeyPress(m_playerId, action, repeat, pressed, released);
    switch (action)
    {
    case EControllerActions_MenuCancel:
        if (pressed)
        {
            gConsoleUIController.PlayUISFX(SoundEvents::UI_BACK);
            navigateBack();
        }
        break;
    case EControllerActions_MenuLeft:
    case EControllerActions_MenuRight:
        if (pressed)
        {
            const int row = FocusedRow();
            if (row == eRow_Visible || row == eRow_GameMode || row == eRow_HostPrivileges)
                ChangeRow(row, action == EControllerActions_MenuLeft ? -1 : 1);
        }
        break;
    case EControllerActions_MenuUp:
    case EControllerActions_MenuDown:
    case EControllerActions_MenuOtherStickUp:
    case EControllerActions_MenuOtherStickDown:
    case EControllerActions_MenuOk:
        sendInputToMovie(action, repeat, pressed, released);
        break;
    default:
        return;
    }
    handled = true;
}

void UIScene_OnlineSettingsMenu::handlePress(int controlId, int)
{
    gConsoleUIController.PlayUISFX(SoundEvents::UI_PRESS);
    if (controlId == eControl_Top)
    {
        if (m_mode == eMode_OpenToLan)
            StartLanWorld();
        else if (m_mode == eMode_LanWorld)
        {
            if (CGameNetworkManager::sInstance.GetPlayerCount() > LocalPlayerCount())
                Confirm(IDS_LCE_STOP_LAN_WORLD, IDS_LCE_STOP_LAN_WARNING, IDS_LCE_STOP_LAN_WORLD, eConfirm_Stop);
            else
                StopLanWorld();
        }
        return;
    }
    const int row = controlId - eControl_FirstRow;
    if (row >= 0 && row < m_rowCount)
        ChangeRow(row, 0);
}

void UIScene_OnlineSettingsMenu::handleFocusChange(int, int)
{
    m_error.clear();
    RefreshDescription();
    updateTooltips();
}

void UIScene_OnlineSettingsMenu::ChangeRow(int row, int step)
{
    switch (row)
    {
    case eRow_Port:
        if (m_mode != eMode_LanWorld)
            RequestText(row);
        break;
    case eRow_Message:
        RequestText(row);
        break;
    case eRow_Visible:
        VideoSettings::SetHostVisible(!VideoSettings::GetHostVisible());
        if (m_mode == eMode_LanWorld)
            CGameNetworkManager::sInstance.ApplyHostSettings();
        if (step != 0)
            gConsoleUIController.PlayUISFX(SoundEvents::UI_SCROLL);
        RefreshRow(row);
        RefreshInfo();
        RefreshDescription();
        break;
    case eRow_GameMode:
    {
        const int count = GameType::ADVENTURE->getId() + 1;
        const int next = (m_gameMode + (step < 0 ? count - 1 : 1)) % count;
        if (step != 0)
            gConsoleUIController.PlayUISFX(SoundEvents::UI_SCROLL);
        if (next == GameType::CREATIVE->getId() && AchievementsEnabled() && !m_achievementsWarned)
            Confirm(IDS_LCE_GAME_MODE, IDS_LCE_CREATIVE_WARNING, IDS_LCE_SET_TO_CREATIVE, eRow_GameMode);
        else
            SetGameMode(next);
        break;
    }
    case eRow_HostPrivileges:
        if (step != 0)
            gConsoleUIController.PlayUISFX(SoundEvents::UI_SCROLL);
        if (!m_hostPrivileges && AchievementsEnabled() && !m_achievementsWarned)
            Confirm(IDS_HOST_PRIVILEGES, IDS_LCE_HOST_PRIVILEGES_WARNING, IDS_LCE_TURN_ON, eRow_HostPrivileges);
        else
            SetHostPrivileges(!m_hostPrivileges);
        break;
    default:
        break;
    }
}

void UIScene_OnlineSettingsMenu::SetGameMode(int gameType)
{
    if (gameType == m_gameMode)
        return;
    m_gameMode = gameType;
    RefreshRow(eRow_GameMode);
    if (m_mode == eMode_LanWorld)
        SendSetting(ServerSettingsChangedPacket::eSetting_DefaultGameMode, static_cast<unsigned int>(gameType));
}

void UIScene_OnlineSettingsMenu::SetHostPrivileges(bool enabled)
{
    if (enabled == m_hostPrivileges)
        return;
    m_hostPrivileges = enabled;
    RefreshRow(eRow_HostPrivileges);
    if (m_mode == eMode_LanWorld)
        SendSetting(ServerSettingsChangedPacket::eSetting_HostPrivileges, enabled ? 1u : 0u);
}

void UIScene_OnlineSettingsMenu::SendSetting(int setting, unsigned int value)
{
    if (m_playerId < 0 || m_playerId >= 4)
        return;
    Minecraft* minecraft = Minecraft::GetInstance();
    if (minecraft->mLocalPlayers[m_playerId] == NULL)
        return;
    ClientPacketListener* connection = minecraft->getConnection(m_playerId);
    if (connection != NULL)
        connection->send(boost::shared_ptr<ServerSettingsChangedPacket>(
            new ServerSettingsChangedPacket(static_cast<char>(setting), value)));
}

void UIScene_OnlineSettingsMenu::StartLanWorld()
{
    const int port = VideoSettings::GetHostPort();
    if (!CGameNetworkManager::sInstance.OpenToNetwork())
    {
        m_error = formatwstr(L"Port %d could not be opened. Another program may be using it; choose another port "
                             L"and try again.",
                             port);
        RefreshDescription();
        return;
    }

    if (m_gameMode != static_cast<int>(gApp.GetGameHostOption(eGameHostOption_GameType)))
        SendSetting(ServerSettingsChangedPacket::eSetting_DefaultGameMode, static_cast<unsigned int>(m_gameMode));
    if (m_hostPrivileges != (gApp.GetGameHostOption(eGameHostOption_CheatsEnabled) != 0))
        SendSetting(ServerSettingsChangedPacket::eSetting_HostPrivileges, m_hostPrivileges ? 1u : 0u);

    gApp.SetXuiServerAction(gProfile->GetPrimaryPad(), eXuiServerAction_PauseServer, NULL);
    const int player = m_playerId;
    gConsoleUIController.CloseUIScenes(player, false);
    Minecraft* minecraft = Minecraft::GetInstance();
    if (minecraft->mGui != NULL)
        minecraft->mGui->addMessage(formatwstr(L"§eYour world is open to other players on port %d.", port), player,
                                    false, false, false, true);
}

void UIScene_OnlineSettingsMenu::StopLanWorld()
{
    gApp.SetXuiServerAction(gProfile->GetPrimaryPad(), eXuiServerAction_CloseToNetwork, NULL);
    const int player = m_playerId;
    gConsoleUIController.CloseUIScenes(player, false);
    Minecraft* minecraft = Minecraft::GetInstance();
    if (minecraft->mGui != NULL)
        minecraft->mGui->addMessage(L"§eYour world is no longer open to other players.", player, false, false, false,
                                    true);
}

void UIScene_OnlineSettingsMenu::RequestText(int row)
{
    const bool port = row == eRow_Port;
    const std::wstring current = port ? PortText() : VideoSettings::GetHostMessage();
    const unsigned int length = port ? 5 : VideoSettings::HOST_MESSAGE_MAX;
    wchar_t initialText[VideoSettings::HOST_MESSAGE_MAX + 1];
    std::memset(initialText, 0, sizeof(initialText));
    current.copy(initialText, length);
    sTextRow = row;
    unsigned int busy = g_pInput->RequestKeyboard(
        port ? L"Port" : L"Message", initialText, static_cast<unsigned int>(m_playerId), length, &TextEntered,
        reinterpret_cast<void*>(static_cast<uintptr_t>(GetCallbackUniqueId())), C_4JInput::EKeyboardMode_0);
    if (busy != 0)
        sTextRow = -1;
}

int UIScene_OnlineSettingsMenu::TextEntered(void* context, bool accepted)
{
    const int row = sTextRow;
    sTextRow = -1;
    if (!accepted || row < 0)
        return 0;

    wchar_t text[VideoSettings::HOST_MESSAGE_MAX + 1];
    std::memset(text, 0, sizeof(text));
    g_pInput->GetText(text, VideoSettings::HOST_MESSAGE_MAX);

    bool valid = true;
    if (row == eRow_Port)
    {
        wchar_t* end = NULL;
        const long port = std::wcstol(text, &end, 10);
        while (end != NULL && *end == L' ')
            ++end;
        valid = end != text && end != NULL && *end == 0 && port >= VideoSettings::HOST_PORT_MIN
                && port <= VideoSettings::HOST_PORT_MAX;
        if (valid)
            VideoSettings::SetHostPort(static_cast<int>(port));
    }
    else
    {
        VideoSettings::SetHostMessage(text);
    }

    gConsoleUIController.EnterCallbackIdCriticalSection();
    UIScene_OnlineSettingsMenu* scene = dynamic_cast<UIScene_OnlineSettingsMenu*>(
        gConsoleUIController.GetSceneFromCallbackId(static_cast<unsigned int>(reinterpret_cast<uintptr_t>(context))));
    if (scene != NULL)
    {
        if (!valid)
            scene->m_error = formatwstr(L"%ls is not a port that can be used. Choose a number from %d to %d.", text,
                                        static_cast<int>(VideoSettings::HOST_PORT_MIN),
                                        static_cast<int>(VideoSettings::HOST_PORT_MAX));
        else if (row == eRow_Message && scene->m_mode == eMode_LanWorld)
            CGameNetworkManager::sInstance.ApplyHostSettings();
        scene->RefreshRow(row);
        scene->RefreshInfo();
        scene->RefreshDescription();
    }
    gConsoleUIController.LeaveCallbackIdCriticalSection();
    return 0;
}

void UIScene_OnlineSettingsMenu::Confirm(int titleId, int messageId, int acceptId, int pending)
{
    static int buttons[2];
    buttons[0] = IDS_CONFIRM_CANCEL;
    buttons[1] = acceptId;
    m_pending = pending;
    gConsoleUIController.RequestAlertMessage(titleId, messageId, buttons, 2, static_cast<unsigned int>(m_playerId),
                                             &ConfirmReturned,
                                             reinterpret_cast<void*>(static_cast<uintptr_t>(GetCallbackUniqueId())),
                                             NULL, true);
}

int UIScene_OnlineSettingsMenu::ConfirmReturned(void* context, int, C4JStorage::EMessageResult result)
{
    gConsoleUIController.EnterCallbackIdCriticalSection();
    UIScene_OnlineSettingsMenu* scene = dynamic_cast<UIScene_OnlineSettingsMenu*>(
        gConsoleUIController.GetSceneFromCallbackId(static_cast<unsigned int>(reinterpret_cast<uintptr_t>(context))));
    if (scene != NULL)
    {
        const int pending = scene->m_pending;
        scene->m_pending = eConfirm_None;
        if (result == C4JStorage::EMessageResult_Decline)
        {
            if (pending == eConfirm_Stop)
            {
                scene->StopLanWorld();
            }
            else if (pending == eRow_HostPrivileges)
            {
                scene->m_achievementsWarned = true;
                scene->SetHostPrivileges(true);
            }
            else if (pending == eRow_GameMode)
            {
                scene->m_achievementsWarned = true;
                scene->SetGameMode(GameType::CREATIVE->getId());
            }
        }
    }
    gConsoleUIController.LeaveCallbackIdCriticalSection();
    return 0;
}

bool UIScene_OnlineSettingsMenu::AchievementsEnabled()
{
    return gApp.GetGameHostOption(eGameHostOption_HasBeenInCreative) == 0
           && gApp.GetGameHostOption(eGameHostOption_CheatsEnabled) == 0;
}

std::wstring UIScene_OnlineSettingsMenu::GameModeName(int gameType)
{
    if (gameType == GameType::CREATIVE->getId())
        return L"Creative";
    if (gameType == GameType::ADVENTURE->getId())
        return L"Adventure";
    return L"Survival";
}
