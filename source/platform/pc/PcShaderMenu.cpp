#include "platform/pc/PcShaderMenu.hpp"

#include <SDL3/SDL.h>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <cwchar>
#include <filesystem>
#include <fstream>
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#include "game/client/audio/SoundEvents.hpp"
#include "game/client/render/Font.hpp"
#include "game/core/CConsoleMinecraftApp.hpp"
#include "game/core/ConsoleUIController.hpp"
#include "game/core/ExtraStrings.hpp"
#include "game/core/L10N.hpp"
#include "game/core/Minecraft.hpp"
#include "game/core/StringConversion.hpp"
#include "game/core/StringIds.hpp"
#include "game/core/System.hpp"
#include "game/ui/UILayer.hpp"
#include "platform/CInput.hpp"
#include "platform/pc/Log.hpp"
#include "platform/pc/PcWindow.hpp"
#include "platform/pc/shaderpack/ShaderPack.hpp"
#include "platform/pc/shaderpack/ShaderPackLang.hpp"
#include "platform/pc/shaderpack/ShaderPackManager.hpp"
#include "platform/pc/shaderpack/ShaderPipeline.hpp"

extern ConsoleUIController gConsoleUIController;

using namespace MenuColour;

namespace
{
const Logger sLog("Shaders");

enum
{
    eDialog_None,
    eDialog_Import,
    eDialog_Export
};

const int kPackInternal = -1;

const int kLayoutWidth = MenuFrame::kWidth;
const int kPanelLeft = 20;
const int kPanelRight = 620;
const int kPanelTop = 56;
const int kSplitRight = 330;
const int kDetailLeft = 336;
const int kHideTop = 8;
const int kHideBottom = 24;
const int kHideWidth = 64;
const int kTabTop = 38;
const int kTabWidth = 102;
const int kTabGap = 2;
const int kActionTop = 38;
const int kActionBottom = 54;
const int kActionWidth = 60;
const int kRowHeight = 20;
const int kRowPitch = 22;
const int kRowGap = kRowPitch - kRowHeight;
const int kLineHeight = 10;
const int kListLeft = kPanelLeft + 6;
const int kListTop = kPanelTop + 6;
const int kListRight = kSplitRight - 4;
const int kListRowRight = kListRight - 8;
const int kHeaderTop = kPanelTop + 6;
const int kHeaderBottom = kHeaderTop + 17;
const int kHeaderButtonWidth = 48;
const int kGridLeft = kPanelLeft + 6;
const int kGridRight = kPanelRight - 6;
const int kGridCellRight = kGridRight - 8;
const int kGridTop = 86;
const int kCellGap = 4;
const int kSettingValueWidth = 72;
const int kSwitchWidth = 30;
const int kModalWidth = 320;
const int kModalHeight = 124;
const long long kNoticeMilliseconds = 5000;
const long long kFlashMilliseconds = 450;
const long long kReloadWaitMilliseconds = 5000;
const char* const kDownloadUrl = "https://modrinth.com/shaders";

const unsigned int kTabFill = 0xFFACACAC;
const unsigned int kTabLight = 0xFFD8D8D8;
const unsigned int kTabTextDisabled = 0xFF8A8A8A;
const unsigned int kTabTextFocus = 0xFF2A3070;
const unsigned int kTextBody = 0xFFD4D4D4;
const unsigned int kTextHint = 0xFF5C5C5C;
const unsigned int kTextGrey = 0xFFA0A0A0;
const unsigned int kTextModified = 0xFFFFC94A;
const unsigned int kTextValue = 0xFF8EA8FF;
const unsigned int kTextOn = 0xFF55FF55;
const unsigned int kTextOff = 0xFFFF5555;
const unsigned int kTextWarning = 0xFFFFAA00;
const unsigned int kTextError = 0xFFFF6B6B;
const unsigned int kTextProblem = 0xFFFFB4B4;
const unsigned int kTextModal = 0xFF555555;
const unsigned int kDetailTrack = 0x40FFFFFF;
const unsigned int kDetailThumb = 0xA0FFFFFF;
const unsigned int kModalShade = 0x90000000;

const wchar_t* const kSettingDescriptions[ShaderSettings::Choice_Count] = {
    L"Smooths jagged edges with an FXAA pass after the shader pack's final pass.",
    L"Tells shader packs whether normal maps may be used. Off gives packs a flat normal map.",
    L"Tells shader packs whether specular maps may be used. Off gives packs an empty specular map.",
    L"Scales the resolution the shader pack renders at. Higher is sharper at a higher GPU cost.",
    L"Scales the shadow map size the shader pack asks for. 2x doubles it for sharper shadows at a higher GPU cost; "
    L"0.5x halves it.",
    L"Scales how deep the held item sits in the depth buffer, for packs with depth-based effects such as depth of "
    L"field.",
    L"ON lights the world with the brighter of the items in either hand. Default follows the shader pack.",
    L"ON keeps the fixed brightness per block face from the standard renderer. OFF leaves face shading to the shader "
    L"pack. Default follows the shader pack."};

struct DialogResult
{
    std::mutex mutex;
    const void* owner = nullptr;
    intptr_t request = 0;
    int kind = eDialog_None;
    bool ready = false;
    bool chosen = false;
    std::string path;
    std::string error;
};

DialogResult sDialog;
std::vector<UIScene_ShaderMenu*> sMenus;
std::vector<std::string> sDropped;

void SDLCALL DialogCallback(void* userdata, const char* const* files, int)
{
    std::lock_guard<std::mutex> lock(sDialog.mutex);
    if (reinterpret_cast<intptr_t>(userdata) != sDialog.request)
        return;
    sDialog.ready = true;
    sDialog.chosen = files != nullptr && files[0] != nullptr;
    sDialog.path = sDialog.chosen ? std::string(files[0]) : std::string();
    sDialog.error = files == nullptr ? std::string(SDL_GetError()) : std::string();
}

std::wstring Wide(const std::string& text)
{
    return convStringToWstring(text);
}

std::string Lower(std::string text)
{
    std::transform(text.begin(), text.end(), text.begin(),
                   [](unsigned char character) { return static_cast<char>(std::tolower(character)); });
    return text;
}

std::string TrimSpaces(const std::string& text)
{
    size_t start = text.find_first_not_of(" \t\r");
    if (start == std::string::npos)
        return std::string();
    size_t end = text.find_last_not_of(" \t\r");
    return text.substr(start, end - start + 1);
}

bool HasPackOptions(const std::string& text, const ShaderPackOptions& options)
{
    std::istringstream stream(text);
    std::string line;
    while (std::getline(stream, line))
    {
        size_t equals = line.find('=');
        if (equals == std::string::npos)
            continue;
        std::string name = TrimSpaces(line.substr(0, equals));
        if (!name.empty() && name[0] != '#' && options.Find(name) != nullptr)
            return true;
    }
    return false;
}

bool ShadersOn(const ShaderSettings& settings)
{
    return settings.UsesPack() || settings.UsesInternal();
}

std::wstring PackDisplayName(const std::string& name)
{
    std::string display = name;
    if (display.size() > 4 && Lower(display.substr(display.size() - 4)) == ".zip")
        display.resize(display.size() - 4);
    std::replace(display.begin(), display.end(), '_', ' ');
    return Wide(display);
}

std::wstring Join(const std::vector<std::wstring>& items, const std::wstring& separator)
{
    std::wstring out;
    for (size_t index = 0; index < items.size(); ++index)
    {
        if (index > 0)
            out += separator;
        out += items[index];
    }
    return out;
}

std::wstring SizeText(unsigned long long bytes)
{
    wchar_t text[32];
    if (bytes >= 1024ull * 1024ull)
        std::swprintf(text, sizeof(text) / sizeof(text[0]), L"%.1f MB", bytes / (1024.0 * 1024.0));
    else
        std::swprintf(text, sizeof(text) / sizeof(text[0]), L"%llu KB", (bytes + 1023ull) / 1024ull);
    return text;
}

std::wstring Plural(int count, const wchar_t* one, const wchar_t* many)
{
    return std::to_wstring(count) + L" " + (count == 1 ? one : many);
}

std::vector<std::wstring> Sentences(const std::wstring& text)
{
    std::vector<std::wstring> sentences;
    size_t start = 0;
    while (start < text.size())
    {
        size_t found = text.find(L". ", start);
        size_t end = found == std::wstring::npos ? text.size() : found + 1;
        std::wstring sentence = text.substr(start, end - start);
        while (!sentence.empty() && sentence[0] == L' ')
            sentence.erase(0, 1);
        if (!sentence.empty())
            sentences.push_back(sentence);
        if (found == std::wstring::npos)
            break;
        start = found + 2;
    }
    return sentences;
}

std::wstring SettingLabel(int setting, int index)
{
    const ShaderSettings::ChoiceInfo& info = ShaderSettings::Describe(setting);
    return index >= 0 && index < info.count ? Wide(info.labels[index]) : std::wstring();
}

unsigned int SettingColour(int setting, int index)
{
    int defaultIndex = ShaderSettings().GetChoice(setting);
    if (setting == ShaderSettings::Choice_NormalMap || setting == ShaderSettings::Choice_SpecularMap)
        return index == defaultIndex ? kTextLight : index != 0 ? kTextOn : kTextOff;
    if (setting == ShaderSettings::Choice_OldHandLight || setting == ShaderSettings::Choice_OldLighting)
        return index == 0 ? kTextLight : index == 1 ? kTextOn : kTextOff;
    return kTextValue;
}

unsigned int Grey(unsigned int colour)
{
    unsigned int red = (colour >> 16) & 255;
    unsigned int green = (colour >> 8) & 255;
    unsigned int blue = colour & 255;
    unsigned int grey = (red * 77 + green * 150 + blue * 29) >> 8;
    return (colour & 0xFF000000) | (grey << 16) | (grey << 8) | grey;
}

MenuRect Rect(int x0, int y0, int x1, int y1)
{
    MenuRect rect = {x0, y0, x1, y1};
    return rect;
}

bool Contains(const MenuRect& rect, float x, float y)
{
    return x >= rect.x0 && x < rect.x1 && y >= rect.y0 && y < rect.y1;
}

void PackIcon(MenuPainter& painter, int x, int y, bool colourful)
{
    const unsigned int skyTop = 0xFF5E93E8;
    const unsigned int skyBottom = 0xFFB8D8FF;
    const unsigned int sun = 0xFFFFE15A;
    const unsigned int hill = 0xFF5BA33B;
    const unsigned int hillDark = 0xFF3E7A28;
    painter.Outline(x, y, x + 14, y + 14, kBlack);
    painter.Gradient(x + 1, y + 1, x + 13, y + 9, colourful ? skyTop : Grey(skyTop),
                     colourful ? skyBottom : Grey(skyBottom));
    painter.Fill(x + 8, y + 3, x + 11, y + 6, colourful ? sun : Grey(sun));
    painter.Fill(x + 1, y + 9, x + 13, y + 13, colourful ? hill : Grey(hill));
    painter.Fill(x + 2, y + 7, x + 7, y + 9, colourful ? hill : Grey(hill));
    painter.Fill(x + 3, y + 6, x + 6, y + 7, colourful ? hill : Grey(hill));
    painter.Fill(x + 1, y + 12, x + 13, y + 13, colourful ? hillDark : Grey(hillDark));
    painter.Fill(x + 9, y + 10, x + 11, y + 11, colourful ? hillDark : Grey(hillDark));
}

void GrassIcon(MenuPainter& painter, int x, int y, bool colourful)
{
    const unsigned int dirt = colourful ? 0xFF8B5A2B : Grey(0xFF8B5A2B);
    const unsigned int dirtDark = colourful ? 0xFF6B4320 : Grey(0xFF6B4320);
    const unsigned int grass = colourful ? 0xFF5BA33B : Grey(0xFF5BA33B);
    const unsigned int grassLight = colourful ? 0xFF79C24F : Grey(0xFF79C24F);
    painter.Outline(x, y, x + 14, y + 14, kBlack);
    painter.Fill(x + 1, y + 1, x + 13, y + 13, dirt);
    painter.Fill(x + 1, y + 1, x + 13, y + 5, grass);
    painter.Fill(x + 3, y + 5, x + 4, y + 7, grass);
    painter.Fill(x + 9, y + 5, x + 10, y + 6, grass);
    painter.Fill(x + 5, y + 9, x + 7, y + 10, dirtDark);
    painter.Fill(x + 9, y + 11, x + 11, y + 12, dirtDark);
    painter.Fill(x + 1, y + 1, x + 13, y + 2, grassLight);
}

int WorldOrder(const std::string& folder)
{
    if (folder == "world0")
        return 0;
    if (folder == "world-1")
        return 1;
    if (folder == "world1")
        return 2;
    return 3;
}

std::wstring WorldName(const std::string& folder)
{
    if (folder == "world0")
        return L"Overworld";
    if (folder == "world-1")
        return L"Nether";
    if (folder == "world1")
        return L"The End";
    return Wide(folder);
}

struct PackSummary
{
    bool usable = false;
    bool folder = false;
    unsigned long long size = 0;
    std::wstring problem;
    std::vector<std::wstring> profiles;
    int mainPages = 0;
    int totalPages = 0;
    std::vector<std::wstring> worlds;
};

std::map<std::string, PackSummary> sSummaries;

void SummarisePack(const ShaderPack& pack, const ShaderPackProperties& properties, const ShaderPackLang& lang,
                   PackSummary& summary)
{
    for (const ShaderProfile& profile : properties.Profiles())
        summary.profiles.push_back(Wide(lang.ProfileLabel(profile.name)));
    summary.totalPages = static_cast<int>(properties.Screens().size());
    if (const ShaderScreen* root = properties.Screen(std::string()))
    {
        for (const std::string& item : root->items)
        {
            if (item.size() > 2 && item.front() == '[' && item.back() == ']')
                ++summary.mainPages;
        }
    }

    bool rootPrograms = false;
    for (const std::string& file : pack.ProgramFiles())
    {
        if (file.find('/') == std::string::npos)
            rootPrograms = true;
    }
    if (rootPrograms)
        summary.worlds.push_back(L"All worlds");
    std::vector<std::string> folders = pack.WorldFolders();
    std::stable_sort(folders.begin(), folders.end(), [](const std::string& left, const std::string& right) {
        return WorldOrder(left) < WorldOrder(right);
    });
    for (const std::string& folder : folders)
        summary.worlds.push_back(WorldName(folder));

    summary.usable = !pack.ProgramFiles().empty();
    if (!summary.usable)
        summary.problem = L"it contains no shader programs";
}

PackSummary Summarise(const std::string& name)
{
    PackSummary summary;
    ShaderPackManager& manager = ShaderPackManager::Get();
    std::filesystem::path path = manager.PacksFolder() / std::filesystem::u8path(name);
    std::error_code fileError;
    summary.folder = std::filesystem::is_directory(path, fileError);
    if (summary.folder)
    {
        for (std::filesystem::recursive_directory_iterator it(path, fileError), end; !fileError && it != end;
             it.increment(fileError))
        {
            std::error_code sizeError;
            if (it->is_regular_file(sizeError))
                summary.size += it->file_size(sizeError);
        }
    }
    else
    {
        std::uintmax_t size = std::filesystem::file_size(path, fileError);
        summary.size = fileError ? 0 : static_cast<unsigned long long>(size);
    }

    ShaderPack* active = manager.ActivePack();
    if (active != nullptr && active->Name() == name)
    {
        SummarisePack(*active, manager.Properties(), manager.Lang(), summary);
        return summary;
    }

    std::string error;
    std::unique_ptr<ShaderPack> pack = ShaderPack::Open(path, error);
    if (!pack)
    {
        summary.problem = error.empty() ? std::wstring(L"it could not be opened") : Wide(error);
        return summary;
    }
    ShaderPackLang lang;
    lang.Load(*pack, ShaderPackManager::LocaleCode());
    ShaderPackProperties properties;
    std::string text;
    if (pack->ReadFile("shaders.properties", text))
        properties.Parse(text, manager.StandardMacros(), ShaderMacros(), ShaderPackOptions());
    SummarisePack(*pack, properties, lang, summary);
    return summary;
}

const PackSummary& SummaryFor(const std::string& name)
{
    std::map<std::string, PackSummary>::iterator found = sSummaries.find(name);
    if (found == sSummaries.end())
        found = sSummaries.insert(std::make_pair(name, Summarise(name))).first;
    return found->second;
}

int ValueBoxWidth(Font& font, const std::wstring& text, int cellWidth, bool slider)
{
    int width = font.width(text) + 12;
    width = std::max(width, slider ? 72 : 44);
    return std::min(width, cellWidth / 2);
}
} // namespace

void UIScene_ShaderMenu::Text::Add(const std::wstring& line, unsigned int colour, int gap)
{
    Paragraph paragraph = {line, colour, gap};
    paragraphs.push_back(paragraph);
}

void UIScene_ShaderMenu::Text::AddSentences(const std::wstring& text, unsigned int colour, int gap)
{
    std::vector<std::wstring> sentences = Sentences(text);
    for (size_t sentence = 0; sentence < sentences.size(); ++sentence)
        Add(sentences[sentence], colour, sentence == 0 ? gap : 0);
}

UIScene_ShaderMenu::UIScene_ShaderMenu(int playerId, void*, UILayer* layer)
    : UIScene(playerId, layer),
      m_tab(eTab_Packs),
      m_columns(2),
      m_working(new ShaderSettings()),
      m_applied(new ShaderSettings()),
      m_options(new ShaderPackOptions()),
      m_properties(new ShaderPackProperties()),
      m_detailHeight(0),
      m_modal(eModal_None),
      m_modalFocus(0),
      m_hidden(false),
      m_awaitingReload(false),
      m_reloadTime(0),
      m_lastTime(0),
      m_lastRefresh(0),
      m_pulse(0.0f),
      m_dragIndex(-1),
      m_pointerX(-1.0f),
      m_pointerY(-1.0f),
      m_dropComplete(false)
{
    initialiseMovie();
    for (int view = 0; view < eView_Count; ++view)
        ResetScroll(view, 0.0f);
    m_notice.colour = kTextLight;
    m_notice.time = 0;
    m_flash.kind = eTarget_None;
    m_flash.index = 0;
    m_flash.time = 0;
    for (int index = 0; index < 16; ++index)
        m_tooltips[index] = -3;
    m_focus[eTab_Packs] = Focus{eTarget_Toggle, 0};
    m_focus[eTab_Options] = Focus{eTarget_Cell, -1};
    m_focus[eTab_Settings] = Focus{eTarget_Setting, 0};

    sSummaries.clear();
    ShaderPackManager& manager = ShaderPackManager::Get();
    *m_applied = manager.Settings();
    *m_working = *m_applied;
    ResetDraft();
    m_packs = manager.ListPacks();
    m_lastRefresh = System::processTimeInMilliSecs();
    BuildCells();
    FocusSelectedPack();

    sMenus.push_back(this);
    if (sMenus.size() == 1)
        PcWindowSetDropHandler(&UIScene_ShaderMenu::HandleDrop, NULL);
}

UIScene_ShaderMenu::~UIScene_ShaderMenu()
{
    sMenus.erase(std::remove(sMenus.begin(), sMenus.end(), this), sMenus.end());
    if (sMenus.empty())
    {
        PcWindowSetDropHandler(NULL, NULL);
        sDropped.clear();
    }
    std::lock_guard<std::mutex> lock(sDialog.mutex);
    if (sDialog.owner == this)
    {
        sDialog.owner = nullptr;
        ++sDialog.request;
    }
}

std::wstring UIScene_ShaderMenu::getMoviePath()
{
    return gApp.GetLocalPlayerCount() > 1 ? std::wstring(L"MultilistMenuSplit") : std::wstring(L"MultilistMenu");
}

int UIScene_ShaderMenu::getSceneType()
{
    return EUIScene_ShaderMenu;
}

bool UIScene_ShaderMenu::handlesPointer()
{
    return true;
}

void UIScene_ShaderMenu::updateComponents()
{
    bool inWorld = Minecraft::GetInstance()->mLevel != NULL;
    m_layer->showComponent(m_playerId, EUIScene_Panorama, !inWorld);
    m_layer->showComponent(m_playerId, EUIScene_Logo, false);
    if (inWorld)
        m_layer->showComponent(m_playerId, EUIScene_MenuBackground, !m_hidden);
}

void UIScene_ShaderMenu::HandleDrop(const char* path, void*)
{
    if (path != NULL)
    {
        sDropped.push_back(path);
        return;
    }
    if (sMenus.empty())
    {
        sDropped.clear();
        return;
    }
    UIScene_ShaderMenu* menu = sMenus.back();
    for (size_t index = sMenus.size(); index-- > 0;)
    {
        if (g_pInput->UsesKeyboard(sMenus[index]->m_playerId))
        {
            menu = sMenus[index];
            break;
        }
    }
    menu->m_dropped.insert(menu->m_dropped.end(), sDropped.begin(), sDropped.end());
    menu->m_dropComplete = true;
    sDropped.clear();
}

void UIScene_ShaderMenu::tick()
{
    UIScene::tick();
    SyncWithManager();
    HandlePointer();
    HandleDrops();
    HandleDialogResult();
    CheckReload();
    long long now = System::processTimeInMilliSecs();
    if (now - m_lastRefresh >= 1000)
    {
        m_lastRefresh = now;
        RefreshPacks(false);
    }
    BuildTargets();
    updateTooltips();
}

void UIScene_ShaderMenu::updateTooltips()
{
    int tips[16];
    for (int index = 0; index < 16; ++index)
        tips[index] = -1;
    bool inWorld = Minecraft::GetInstance()->mLevel != NULL;
    if (m_hidden)
    {
        tips[9] = IDS_LCE_SHADER_SHOW_MENU;
    }
    else if (m_modal != eModal_None)
    {
        tips[0] = IDS_TOOLTIPS_SELECT;
        tips[1] = IDS_TOOLTIPS_BACK;
    }
    else
    {
        tips[0] = IDS_TOOLTIPS_SELECT;
        tips[1] = IDS_TOOLTIPS_BACK;
        if (HasPendingChanges())
            tips[2] = IDS_LCE_SHADER_APPLY;
        if (m_tab == eTab_Packs)
            tips[3] = IDS_LCE_SHADER_REFRESH;
        tips[6] = -2;
        tips[7] = IDS_LCE_SHADER_SWITCH_TAB;
        if (inWorld)
            tips[9] = IDS_LCE_SHADER_HIDE_MENU;
        int index = FindTarget(m_focus[m_tab]);
        if (index >= 0)
        {
            const Target& target = m_targets[static_cast<size_t>(index)];
            switch (target.kind)
            {
            case eTarget_Cell:
                if (m_cells[static_cast<size_t>(target.index)].kind == eCell_Link)
                {
                    tips[0] = IDS_LCE_SHADER_OPEN;
                }
                else if (!CellChangeable(target.index))
                {
                    tips[0] = -1;
                }
                else
                {
                    tips[0] = IDS_LCE_DEBUG_CHANGE;
                    tips[4] = -2;
                    tips[5] = IDS_LCE_SHADER_PREVIOUS_NEXT;
                    tips[6] = -1;
                    tips[7] = -1;
                    if (m_cells[static_cast<size_t>(target.index)].kind == eCell_Option)
                        tips[3] = IDS_LCE_SHADER_DEFAULT;
                }
                break;
            case eTarget_Setting:
                tips[0] = IDS_LCE_DEBUG_CHANGE;
                tips[3] = IDS_LCE_SHADER_DEFAULT;
                tips[4] = -2;
                tips[5] = IDS_LCE_SHADER_PREVIOUS_NEXT;
                tips[6] = -1;
                tips[7] = -1;
                break;
            case eTarget_Toggle:
                tips[0] = IDS_LCE_DEBUG_CHANGE;
                break;
            case eTarget_Pack:
                if (PackName(target.index) == m_applied->shaderPack && m_applied->UsesPack() && !PackChangePending()
                    && ShaderPackManager::Get().ActivePack() != nullptr)
                    tips[0] = IDS_LCE_SHADER_PACK_SETTINGS;
                break;
            case eTarget_OpenFolder:
            case eTarget_Download:
            case eTarget_Import:
                tips[0] = IDS_LCE_SHADER_OPEN;
                break;
            default:
                break;
            }
            if (g_pInput->MenuQuickMoveHeld(m_playerId) && Resettable(target))
                tips[0] = IDS_LCE_SHADER_DEFAULT;
        }
    }
    if (std::memcmp(tips, m_tooltips, sizeof(tips)) == 0)
        return;
    std::memcpy(m_tooltips, tips, sizeof(tips));
    gConsoleUIController.SetTooltips(m_playerId, tips[0], tips[1], tips[2], tips[3], tips[4], tips[5], tips[6], tips[7],
                                     tips[8], tips[9], tips[10], tips[11], tips[12], tips[13], tips[14], tips[15],
                                     false, false);
}

int UIScene_ShaderMenu::ListBottom() const
{
    return m_frame.bottom - 50;
}

int UIScene_ShaderMenu::SettingsBottom() const
{
    return m_frame.bottom - 68;
}

int UIScene_ShaderMenu::GridBottom() const
{
    return m_frame.bottom - 70;
}

int UIScene_ShaderMenu::CommentTop() const
{
    return m_frame.bottom - 65;
}

int UIScene_ShaderMenu::ButtonsTop() const
{
    return m_frame.bottom - 28;
}

MenuRect UIScene_ShaderMenu::ModalRect() const
{
    int left = (kLayoutWidth - kModalWidth) / 2;
    int top = m_frame.height / 2 - 64;
    return Rect(left, top, left + kModalWidth, top + kModalHeight);
}

void UIScene_ShaderMenu::AddTarget(int kind, int index, const MenuRect& rect, int view, bool enabled)
{
    Target target = {kind, index, rect, view, enabled};
    m_targets.push_back(target);
}

void UIScene_ShaderMenu::BuildTargets()
{
    m_targets.clear();
    if (m_hidden)
        return;
    BuildFixedTargets();
    if (m_tab == eTab_Packs)
        BuildPackTargets();
    else if (m_tab == eTab_Options)
        BuildOptionTargets();
    else
        BuildSettingTargets();
    if (m_modal != eModal_None)
        BuildModalTargets();
    else
        FixFocus();
}

void UIScene_ShaderMenu::BuildFixedTargets()
{
    if (Minecraft::GetInstance()->mLevel != NULL)
        AddTarget(eTarget_Hide, 0, Rect(kPanelLeft, kHideTop, kPanelLeft + kHideWidth, kHideBottom), eView_Fixed,
                  true);
    for (int tab = 0; tab < eTab_Count; ++tab)
    {
        int x0 = kPanelLeft + tab * (kTabWidth + kTabGap);
        AddTarget(eTarget_Tab, tab, Rect(x0, kTabTop, x0 + kTabWidth, kPanelTop), eView_Fixed, TabEnabled(tab));
    }
    const int kinds[3] = {eTarget_Cancel, eTarget_Apply, eTarget_Done};
    for (int action = 0; action < 3; ++action)
    {
        int x1 = kPanelRight - (2 - action) * (kActionWidth + 4);
        AddTarget(kinds[action], 0, Rect(x1 - kActionWidth, kActionTop, x1, kActionBottom), eView_Fixed,
                  kinds[action] != eTarget_Apply || HasPendingChanges());
    }
}

void UIScene_ShaderMenu::BuildPackTargets()
{
    AddTarget(eTarget_Toggle, 0, Rect(kListLeft, 0, kListRowRight, kRowHeight), eView_List, true);
    int rowTop = kRowPitch + 4;
    AddTarget(eTarget_Pack, kPackInternal, Rect(kListLeft, rowTop, kListRowRight, rowTop + kRowHeight), eView_List,
              true);
    for (size_t index = 0; index < m_packs.size(); ++index)
    {
        rowTop += kRowPitch;
        AddTarget(eTarget_Pack, static_cast<int>(index), Rect(kListLeft, rowTop, kListRowRight, rowTop + kRowHeight),
                  eView_List, true);
    }
    int middle = (kListLeft + kListRowRight) / 2;
    int top = ButtonsTop();
    AddTarget(eTarget_OpenFolder, 0, Rect(kListLeft, top, middle - 2, top + 20), eView_Fixed, true);
    AddTarget(eTarget_Download, 0, Rect(middle + 2, top, kListRowRight, top + 20), eView_Fixed, true);
}

void UIScene_ShaderMenu::BuildOptionTargets()
{
    if (PackChangePending() || m_optionsPack.empty() || ShaderPackManager::Get().ActivePack() == nullptr)
        return;
    if (!m_history.empty())
        AddTarget(eTarget_Back, 0, Rect(kGridLeft, kHeaderTop, kGridLeft + kHeaderButtonWidth, kHeaderBottom),
                  eView_Fixed, true);
    const int kinds[3] = {eTarget_Import, eTarget_Export, eTarget_Reset};
    for (int button = 0; button < 3; ++button)
    {
        int x1 = kGridRight - (2 - button) * (kHeaderButtonWidth + 4);
        AddTarget(kinds[button], 0, Rect(x1 - kHeaderButtonWidth, kHeaderTop, x1, kHeaderBottom), eView_Fixed, true);
    }

    int columns = std::max(1, m_columns);
    int cellWidth = (kGridCellRight - kGridLeft - (columns - 1) * kCellGap) / columns;
    for (size_t index = 0; index < m_cells.size(); ++index)
    {
        const Cell& cell = m_cells[index];
        if (cell.kind == eCell_Empty)
            continue;
        int row = static_cast<int>(index) / columns;
        int x0 = kGridLeft + static_cast<int>(index) % columns * (cellWidth + kCellGap);
        bool enabled = true;
        if (cell.kind == eCell_Option)
        {
            const ShaderOption* option = m_options->Find(cell.name);
            enabled = option != nullptr && !option->ambiguous;
        }
        AddTarget(eTarget_Cell, static_cast<int>(index),
                  Rect(x0, row * kRowPitch, x0 + cellWidth, row * kRowPitch + kRowHeight), eView_Grid, enabled);
    }
}

void UIScene_ShaderMenu::BuildSettingTargets()
{
    for (int setting = 0; setting < ShaderSettings::Choice_Count; ++setting)
        AddTarget(eTarget_Setting, setting,
                  Rect(kListLeft, setting * kRowPitch, kListRowRight, setting * kRowPitch + kRowHeight),
                  eView_Settings, true);
}

void UIScene_ShaderMenu::BuildModalTargets()
{
    MenuRect modal = ModalRect();
    int middle = (modal.x0 + modal.x1) / 2;
    AddTarget(eTarget_Modal, 0, Rect(modal.x0 + 16, modal.y0 + 96, middle - 4, modal.y0 + 114), eView_Fixed, true);
    AddTarget(eTarget_Modal, 1, Rect(middle + 4, modal.y0 + 96, modal.x1 - 16, modal.y0 + 114), eView_Fixed, true);
}

void UIScene_ShaderMenu::FixFocus()
{
    Focus& focus = m_focus[m_tab];
    int current = FindTarget(focus);
    if (current >= 0 && m_targets[static_cast<size_t>(current)].enabled)
        return;
    if (focus.kind == eTarget_Apply)
    {
        focus = Focus{eTarget_Done, 0};
        return;
    }
    int fallback = -1;
    for (size_t index = 0; index < m_targets.size(); ++index)
    {
        const Target& target = m_targets[index];
        if (!target.enabled)
            continue;
        bool content = target.kind != eTarget_Tab && target.kind != eTarget_Cancel && target.kind != eTarget_Apply
                       && target.kind != eTarget_Done && target.kind != eTarget_Hide;
        if (content && target.view != eView_Fixed)
        {
            fallback = static_cast<int>(index);
            break;
        }
        if (fallback < 0 && content)
            fallback = static_cast<int>(index);
    }
    if (fallback < 0)
        fallback = FindTarget(Focus{eTarget_Tab, m_tab});
    if (fallback >= 0)
        focus = Focus{m_targets[static_cast<size_t>(fallback)].kind, m_targets[static_cast<size_t>(fallback)].index};
}

int UIScene_ShaderMenu::FindTarget(const Focus& focus) const
{
    for (size_t index = 0; index < m_targets.size(); ++index)
    {
        if (m_targets[index].kind == focus.kind && m_targets[index].index == focus.index)
            return static_cast<int>(index);
    }
    return -1;
}

MenuRect UIScene_ShaderMenu::ViewRect(int view) const
{
    switch (view)
    {
    case eView_List:
        return Rect(kListLeft, kListTop, kListRight, ListBottom());
    case eView_Grid:
        return Rect(kGridLeft, kGridTop, kGridRight, GridBottom());
    case eView_Settings:
        return Rect(kListLeft, kListTop, kListRight, SettingsBottom());
    case eView_Details:
        if (m_tab == eTab_Options)
            return Rect(kGridLeft + 6, CommentTop() + 4, kGridRight - 6, m_frame.bottom - 8);
        return Rect(kDetailLeft + 6, kPanelTop + 6, kPanelRight - 6, m_frame.bottom - 6);
    default:
        return Rect(0, 0, kLayoutWidth, m_frame.height);
    }
}

int UIScene_ShaderMenu::ContentHeight(int view) const
{
    switch (view)
    {
    case eView_List:
        return static_cast<int>(m_packs.size() + 2) * kRowPitch + 4 - kRowGap;
    case eView_Grid:
    {
        int columns = std::max(1, m_columns);
        int rows = (static_cast<int>(m_cells.size()) + columns - 1) / columns;
        return rows > 0 ? rows * kRowPitch - kRowGap : 0;
    }
    case eView_Settings:
        return ShaderSettings::Choice_Count * kRowPitch - kRowGap;
    case eView_Details:
        return m_detailHeight;
    default:
        return 0;
    }
}

MenuRect UIScene_ShaderMenu::ScreenRect(const Target& target) const
{
    MenuRect rect = target.rect;
    if (target.view == eView_Fixed)
        return rect;
    int offset = ViewRect(target.view).y0 - static_cast<int>(m_scroll[target.view].position + 0.5f);
    rect.y0 += offset;
    rect.y1 += offset;
    return rect;
}

int UIScene_ShaderMenu::TargetAt(float x, float y) const
{
    for (size_t index = 0; index < m_targets.size(); ++index)
    {
        const Target& target = m_targets[index];
        if (!target.enabled || (m_modal != eModal_None) != (target.kind == eTarget_Modal))
            continue;
        if (target.view != eView_Fixed && !Contains(ViewRect(target.view), x, y))
            continue;
        if (Contains(ScreenRect(target), x, y))
            return static_cast<int>(index);
    }
    return -1;
}

UIScene_ShaderMenu::Focus& UIScene_ShaderMenu::CurrentFocus()
{
    return m_focus[m_tab];
}

float UIScene_ShaderMenu::FlashAmount(int kind, int index) const
{
    long long since = System::processTimeInMilliSecs() - m_flash.time;
    if (m_flash.kind != kind || m_flash.index != index || since >= kFlashMilliseconds)
        return 0.0f;
    return 1.0f - static_cast<float>(since) / kFlashMilliseconds;
}

void UIScene_ShaderMenu::StartFlash(const Target& target)
{
    m_flash.kind = target.kind;
    m_flash.index = target.index;
    m_flash.time = System::processTimeInMilliSecs();
}

bool UIScene_ShaderMenu::Focused(int kind, int index) const
{
    if (m_modal != eModal_None)
        return kind == eTarget_Modal && index == m_modalFocus;
    return m_focus[m_tab].kind == kind && m_focus[m_tab].index == index;
}

void UIScene_ShaderMenu::SetFocus(const Target& target, bool keyboard)
{
    if (m_modal != eModal_None)
    {
        if (target.kind == eTarget_Modal)
            m_modalFocus = target.index;
        return;
    }
    Focus& focus = CurrentFocus();
    if (focus.kind != target.kind || focus.index != target.index)
        ResetScroll(eView_Details, 0.0f);
    focus = Focus{target.kind, target.index};
    if (keyboard)
        EnsureVisible(target);
}

void UIScene_ShaderMenu::EnsureVisible(const Target& target)
{
    if (target.view == eView_Fixed)
        return;
    MenuRect view = ViewRect(target.view);
    Scroll& scroll = m_scroll[target.view];
    float visible = static_cast<float>(view.y1 - view.y0);
    float margin = 4.0f;
    float position = scroll.mouse ? scroll.position : scroll.target;
    if (target.rect.y0 - margin < position)
        position = target.rect.y0 - margin;
    if (target.rect.y1 + margin > position + visible)
        position = target.rect.y1 + margin - visible;
    scroll.target = std::max(0.0f, std::min(position, MaxScroll(target.view)));
    scroll.mouse = false;
}

float UIScene_ShaderMenu::MaxScroll(int view) const
{
    MenuRect rect = ViewRect(view);
    return std::max(0.0f, static_cast<float>(ContentHeight(view) - (rect.y1 - rect.y0)));
}

void UIScene_ShaderMenu::ScrollView(int view, float amount)
{
    Scroll& scroll = m_scroll[view];
    if (!scroll.mouse)
        scroll.target = scroll.position;
    scroll.mouse = true;
    scroll.target = std::max(0.0f, std::min(scroll.target + amount, MaxScroll(view)));
}

void UIScene_ShaderMenu::ResetScroll(int view, float position)
{
    m_scroll[view].position = position;
    m_scroll[view].target = position;
    m_scroll[view].mouse = false;
}

void UIScene_ShaderMenu::Move(int dx, int dy)
{
    if (m_modal != eModal_None)
    {
        int next = dx > 0 ? 1 : 0;
        if (dx != 0 && next != m_modalFocus)
        {
            m_modalFocus = next;
            gConsoleUIController.PlayUISFX(SoundEvents::UI_FOCUS);
        }
        return;
    }
    int current = FindTarget(CurrentFocus());
    if (current < 0)
        return;
    MenuRect from = ScreenRect(m_targets[static_cast<size_t>(current)]);
    int centreX = (from.x0 + from.x1) / 2;
    int centreY = (from.y0 + from.y1) / 2;
    int best = -1;
    int bestScore = 0;
    for (size_t index = 0; index < m_targets.size(); ++index)
    {
        if (static_cast<int>(index) == current || !m_targets[index].enabled)
            continue;
        MenuRect to = ScreenRect(m_targets[index]);
        int targetX = (to.x0 + to.x1) / 2;
        int targetY = (to.y0 + to.y1) / 2;
        int primary = 0;
        int secondary = 0;
        if (dy != 0)
        {
            primary = (targetY - centreY) * dy;
            if (primary <= 2)
                continue;
            if (to.x1 <= from.x0)
                secondary = from.x0 - to.x1;
            else if (to.x0 >= from.x1)
                secondary = to.x0 - from.x1;
            secondary += std::abs(targetX - centreX) / 8;
        }
        else
        {
            primary = (targetX - centreX) * dx;
            if (primary <= 2 || to.y1 <= from.y0 || to.y0 >= from.y1)
                continue;
            secondary = std::abs(targetY - centreY);
        }
        int score = primary + secondary * 3;
        if (best < 0 || score < bestScore)
        {
            best = static_cast<int>(index);
            bestScore = score;
        }
    }
    if (best < 0)
        return;
    SetFocus(m_targets[static_cast<size_t>(best)], true);
    gConsoleUIController.PlayUISFX(SoundEvents::UI_FOCUS);
}

bool UIScene_ShaderMenu::TabEnabled(int tab) const
{
    if (tab == eTab_Options)
        return m_working->UsesPack() && (PackChangePending() || ShaderPackManager::Get().ActivePack() != nullptr);
    return true;
}

void UIScene_ShaderMenu::SetTab(int tab)
{
    if (tab == m_tab || tab < 0 || tab >= eTab_Count || !TabEnabled(tab))
        return;
    m_tab = tab;
    m_dragIndex = -1;
    ResetScroll(eView_Details, 0.0f);
    BuildCells();
    BuildTargets();
    int index = FindTarget(CurrentFocus());
    if (index >= 0)
        EnsureVisible(m_targets[static_cast<size_t>(index)]);
    gConsoleUIController.PlayUISFX(SoundEvents::UI_FOCUS);
}

void UIScene_ShaderMenu::CycleTab(int step)
{
    for (int offset = 1; offset < eTab_Count; ++offset)
    {
        int tab = ((m_tab + step * offset) % eTab_Count + eTab_Count) % eTab_Count;
        if (TabEnabled(tab))
        {
            SetTab(tab);
            return;
        }
    }
}

void UIScene_ShaderMenu::Activate(const Target& target)
{
    StartFlash(target);
    switch (target.kind)
    {
    case eTarget_Hide:
        ToggleHidden();
        break;
    case eTarget_Tab:
        SetTab(target.index);
        break;
    case eTarget_Cancel:
        gConsoleUIController.PlayUISFX(SoundEvents::UI_BACK);
        navigateBack();
        break;
    case eTarget_Apply:
        if (HasPendingChanges())
        {
            gConsoleUIController.PlayUISFX(SoundEvents::UI_PRESS);
            Apply(false);
        }
        break;
    case eTarget_Done:
        Done();
        break;
    case eTarget_Toggle:
        SetShadersOn(!ShadersOn(*m_working));
        break;
    case eTarget_Pack:
        SelectPack(target.index);
        break;
    case eTarget_OpenFolder:
        gConsoleUIController.PlayUISFX(SoundEvents::UI_PRESS);
        OpenPackFolder();
        break;
    case eTarget_Download:
        OpenModal(eModal_Download);
        break;
    case eTarget_Back:
        CloseScreen();
        break;
    case eTarget_Import:
    case eTarget_Export:
        gConsoleUIController.PlayUISFX(SoundEvents::UI_PRESS);
        ShowFileDialog(target.kind == eTarget_Export);
        break;
    case eTarget_Reset:
        OpenModal(eModal_Reset);
        break;
    case eTarget_Cell:
    {
        const Cell& cell = m_cells[static_cast<size_t>(target.index)];
        if (cell.kind == eCell_Link)
            OpenScreen(cell.name);
        else
            Change(target, 1);
        break;
    }
    case eTarget_Setting:
        ChangeSetting(target.index, 1);
        break;
    case eTarget_Modal:
        CloseModal(target.index == 0);
        break;
    default:
        break;
    }
}

void UIScene_ShaderMenu::Change(const Target& target, int step)
{
    StartFlash(target);
    switch (target.kind)
    {
    case eTarget_Toggle:
        SetShadersOn(!ShadersOn(*m_working));
        break;
    case eTarget_Cell:
    {
        const Cell& cell = m_cells[static_cast<size_t>(target.index)];
        if (cell.kind == eCell_Profile)
            CycleProfile(step);
        else if (cell.kind == eCell_Option)
            CycleOption(cell.name, step);
        break;
    }
    case eTarget_Setting:
        ChangeSetting(target.index, step);
        break;
    default:
        break;
    }
}

bool UIScene_ShaderMenu::Changeable(const Target& target) const
{
    return target.kind == eTarget_Toggle || target.kind == eTarget_Setting
           || (target.kind == eTarget_Cell && CellChangeable(target.index));
}

bool UIScene_ShaderMenu::Resettable(const Target& target) const
{
    if (target.kind == eTarget_Toggle || target.kind == eTarget_Setting)
        return true;
    return target.kind == eTarget_Cell && m_cells[static_cast<size_t>(target.index)].kind == eCell_Option
           && CellChangeable(target.index);
}

bool UIScene_ShaderMenu::ChangesSideways(const Target& target) const
{
    if (target.kind == eTarget_Toggle || target.kind == eTarget_Setting)
        return true;
    return target.kind == eTarget_Cell && m_columns == 1 && CellChangeable(target.index);
}

void UIScene_ShaderMenu::ResetTarget(const Target& target)
{
    StartFlash(target);
    if (target.kind == eTarget_Cell)
    {
        const Cell& cell = m_cells[static_cast<size_t>(target.index)];
        const ShaderOption* option = cell.kind == eCell_Option ? m_options->Find(cell.name) : nullptr;
        if (option == nullptr || m_options->Value(cell.name) == option->defaultValue)
            return;
        if (m_options->SetValue(cell.name, option->defaultValue))
        {
            gConsoleUIController.PlayUISFX(SoundEvents::UI_SCROLL);
            EditDraft();
        }
        return;
    }
    if (target.kind == eTarget_Setting)
    {
        int defaultIndex = ShaderSettings().GetChoice(target.index);
        if (m_working->GetChoice(target.index) == defaultIndex)
            return;
        m_working->SetChoice(target.index, defaultIndex);
        gConsoleUIController.PlayUISFX(SoundEvents::UI_SCROLL);
        return;
    }
    if (target.kind == eTarget_Toggle)
        SetShadersOn(true);
}

void UIScene_ShaderMenu::Back()
{
    if (m_modal != eModal_None)
    {
        CloseModal(false);
        return;
    }
    if (m_hidden)
    {
        ToggleHidden();
        return;
    }
    if (m_tab == eTab_Options && !m_history.empty())
    {
        CloseScreen();
        return;
    }
    if (HasPendingChanges())
    {
        OpenModal(eModal_Discard);
        return;
    }
    navigateBack();
}

void UIScene_ShaderMenu::Click(float x, float y, unsigned int clicks)
{
    int index = TargetAt(x, y);
    bool secondary = (clicks & C_4JInput::EPcMenuClick_Secondary) != 0;
    if (index < 0)
    {
        if (secondary)
        {
            gConsoleUIController.PlayUISFX(SoundEvents::UI_BACK);
            Back();
        }
        return;
    }
    Target target = m_targets[static_cast<size_t>(index)];
    if (m_modal != eModal_None)
    {
        CloseModal(!secondary && target.index == 0);
        return;
    }
    SetFocus(target, false);
    if ((clicks & C_4JInput::EPcMenuClick_Quick) != 0)
    {
        if (Resettable(target))
            ResetTarget(target);
        else
            Activate(target);
        return;
    }
    if (secondary)
    {
        if (Changeable(target))
        {
            Change(target, -1);
        }
        else
        {
            gConsoleUIController.PlayUISFX(SoundEvents::UI_BACK);
            Back();
        }
        return;
    }
    if ((clicks & C_4JInput::EPcMenuClick_Primary) == 0)
        return;
    if (target.kind == eTarget_Cell && IsSlider(m_cells[static_cast<size_t>(target.index)]))
    {
        int boxX0 = 0;
        int boxX1 = 0;
        CellValueRect(target.index, &boxX0, &boxX1);
        if (x >= boxX0 && x < boxX1)
        {
            m_dragIndex = target.index;
            StartFlash(target);
            DragSlider(x);
            return;
        }
    }
    Activate(target);
}

void UIScene_ShaderMenu::HandlePointer()
{
    float x = 0.0f;
    float y = 0.0f;
    bool hasPointer = m_frame.Pointer(m_playerId, &x, &y);
    unsigned int clicks = g_pInput->ConsumeMenuClicks(m_playerId);
    int wheel = g_pInput->ConsumeMenuWheel(m_playerId);
    if (m_hidden)
    {
        if (clicks != 0)
            ToggleHidden();
        return;
    }

    if (hasPointer && (x != m_pointerX || y != m_pointerY))
    {
        m_pointerX = x;
        m_pointerY = y;
        int index = TargetAt(x, y);
        if (index >= 0 && m_dragIndex < 0)
        {
            const Target& target = m_targets[static_cast<size_t>(index)];
            if (!Focused(target.kind, target.index))
            {
                SetFocus(target, false);
                gConsoleUIController.PlayUISFX(SoundEvents::UI_FOCUS);
            }
        }
    }

    if (m_dragIndex >= 0)
    {
        if (hasPointer && g_pInput->MenuPointerHeld(m_playerId))
            DragSlider(x);
        else
            m_dragIndex = -1;
    }

    if (clicks != 0 && hasPointer)
        Click(x, y, clicks);
    else if ((clicks & C_4JInput::EPcMenuClick_Secondary) != 0)
        Back();

    if (wheel != 0 && m_modal == eModal_None)
    {
        int view = m_tab == eTab_Packs ? eView_List : m_tab == eTab_Options ? eView_Grid : eView_Settings;
        float step = kRowPitch * 2.0f;
        MenuRect details = ViewRect(eView_Details);
        if (hasPointer && x >= details.x0 - 6 && x < details.x1 + 6 && y >= details.y0 - 6 && y < details.y1 + 6)
        {
            view = eView_Details;
            step = kLineHeight * 3.0f;
        }
        ScrollView(view, -static_cast<float>(wheel) * step);
    }
}

void UIScene_ShaderMenu::DragSlider(float x)
{
    if (m_dragIndex < 0 || m_dragIndex >= static_cast<int>(m_cells.size()))
    {
        m_dragIndex = -1;
        return;
    }
    const Cell& cell = m_cells[static_cast<size_t>(m_dragIndex)];
    int count = 0;
    int current = SliderPosition(cell, &count);
    if (count < 2)
        return;
    int boxX0 = 0;
    int boxX1 = 0;
    CellValueRect(m_dragIndex, &boxX0, &boxX1);
    float amount = (x - (boxX0 + 3)) / static_cast<float>(std::max(1, boxX1 - boxX0 - 6));
    int position = static_cast<int>(std::floor(amount * (count - 1) + 0.5f));
    position = std::max(0, std::min(position, count - 1));
    if (position != current)
    {
        SetSliderPosition(cell, position);
        gConsoleUIController.PlayUISFX(SoundEvents::UI_SCROLL);
    }
}

void UIScene_ShaderMenu::Animate()
{
    long long now = System::processTimeInMilliSecs();
    float elapsed = m_lastTime == 0 ? 1000.0f : static_cast<float>(now - m_lastTime);
    elapsed = std::max(0.0f, std::min(elapsed, 250.0f));
    m_lastTime = now;
    m_pulse = static_cast<float>(now % 1200) / 1200.0f;
    for (int view = 0; view < eView_Count; ++view)
    {
        Scroll& scroll = m_scroll[view];
        scroll.target = std::max(0.0f, std::min(scroll.target, MaxScroll(view)));
        scroll.position = MenuApproach(scroll.position, scroll.target, elapsed, 70.0f);
        if (std::fabs(scroll.position - scroll.target) < 0.05f)
            scroll.position = scroll.target;
    }
}

void UIScene_ShaderMenu::HandleModalInput(int action, bool repeat)
{
    switch (action)
    {
    case EControllerActions_MenuLeft:
        Move(-1, 0);
        break;
    case EControllerActions_MenuRight:
        Move(1, 0);
        break;
    case EControllerActions_MenuOk:
        if (!repeat)
            CloseModal(m_modalFocus == 0);
        break;
    case EControllerActions_MenuCancel:
        if (!repeat)
            CloseModal(false);
        break;
    default:
        break;
    }
}

void UIScene_ShaderMenu::handleInput(int, int action, bool repeat, bool pressed, bool released, bool& handled)
{
    gConsoleUIController.AnimateKeyPress(m_playerId, action, repeat, pressed, released);
    if (!pressed)
        return;
    handled = true;
    if (m_hidden)
    {
        if (!repeat
            && (action == EControllerActions_MenuOtherStickPress || action == EControllerActions_MenuCancel
                || action == EControllerActions_MenuOk))
            ToggleHidden();
        return;
    }
    if (m_modal != eModal_None)
    {
        HandleModalInput(action, repeat);
        BuildTargets();
        updateTooltips();
        return;
    }

    int index = FindTarget(CurrentFocus());
    Target target = index >= 0 ? m_targets[static_cast<size_t>(index)] : Target();
    switch (action)
    {
    case EControllerActions_MenuUp:
        Move(0, -1);
        break;
    case EControllerActions_MenuDown:
        Move(0, 1);
        break;
    case EControllerActions_MenuLeft:
    case EControllerActions_MenuRight:
    {
        int step = action == EControllerActions_MenuRight ? 1 : -1;
        if (index >= 0 && target.kind == eTarget_Toggle)
            SetShadersOn(step < 0);
        else if (index >= 0 && ChangesSideways(target))
            Change(target, step);
        else
            Move(step, 0);
        break;
    }
    case EControllerActions_MenuOk:
        if (!repeat && index >= 0)
        {
            if (g_pInput->MenuQuickMoveHeld(m_playerId) && Resettable(target))
                ResetTarget(target);
            else
                Activate(target);
        }
        break;
    case EControllerActions_MenuOtherStickUp:
    case EControllerActions_MenuOtherStickDown:
        ScrollView(eView_Details,
                   action == EControllerActions_MenuOtherStickUp ? -kLineHeight * 2.0f : kLineHeight * 2.0f);
        break;
    case EControllerActions_MenuPageUp:
    case EControllerActions_MenuPageDown:
        if (index >= 0)
            Change(target, action == EControllerActions_MenuPageUp ? -1 : 1);
        break;
    case EControllerActions_MenuX:
        if (!repeat && HasPendingChanges())
        {
            gConsoleUIController.PlayUISFX(SoundEvents::UI_PRESS);
            Apply(false);
        }
        break;
    case EControllerActions_MenuY:
        if (repeat)
            break;
        if (m_tab == eTab_Packs)
        {
            gConsoleUIController.PlayUISFX(SoundEvents::UI_PRESS);
            RefreshPacks(true);
            Notify(L"Shader pack list refreshed", kTextLight);
        }
        else if (index >= 0)
        {
            ResetTarget(target);
        }
        break;
    case EControllerActions_MenuLeftScroll:
        if (!repeat)
            CycleTab(-1);
        break;
    case EControllerActions_MenuRightScroll:
    case EControllerActions_MenuBack:
        if (!repeat)
            CycleTab(1);
        break;
    case EControllerActions_MenuOtherStickPress:
        if (!repeat)
            ToggleHidden();
        break;
    case EControllerActions_MenuCancel:
        if (!repeat)
        {
            gConsoleUIController.PlayUISFX(SoundEvents::UI_BACK);
            Back();
        }
        break;
    default:
        handled = false;
        break;
    }
    BuildTargets();
    updateTooltips();
}

void UIScene_ShaderMenu::SyncWithManager()
{
    ShaderPackManager& manager = ShaderPackManager::Get();
    bool changed = false;
    if (manager.Settings() != *m_applied)
    {
        bool untouched = *m_working == *m_applied;
        *m_applied = manager.Settings();
        if (untouched)
            *m_working = *m_applied;
        changed = true;
    }
    std::string pack = manager.ActivePack() != nullptr ? manager.ActivePack()->Name() : std::string();
    if (pack != m_optionsPack)
    {
        ResetDraft();
        changed = true;
    }
    else if (!pack.empty())
    {
        std::string applied = manager.OptionValues();
        if (applied != m_appliedOptions)
        {
            bool untouched = m_options->SaveValues() == m_appliedOptions;
            m_appliedOptions = applied;
            if (untouched)
            {
                *m_options = manager.Options();
                *m_properties = manager.Properties();
            }
            changed = true;
        }
    }
    if (!changed)
        return;
    BuildCells();
    BuildTargets();
}

void UIScene_ShaderMenu::ResetDraft()
{
    ShaderPackManager& manager = ShaderPackManager::Get();
    ShaderPack* pack = manager.ActivePack();
    m_optionsPack = pack != nullptr ? pack->Name() : std::string();
    *m_options = manager.Options();
    *m_properties = manager.Properties();
    m_appliedOptions = manager.OptionValues();
    m_screen.clear();
    m_history.clear();
    m_focus[eTab_Options] = Focus{eTarget_Cell, -1};
    ResetScroll(eView_Grid, 0.0f);
}

void UIScene_ShaderMenu::EditDraft()
{
    *m_properties = ShaderPackManager::Get().ParseProperties(*m_options);
    BuildCells();
}

bool UIScene_ShaderMenu::SplitScreen() const
{
    return gApp.GetLocalPlayerCount() > 1;
}

std::string UIScene_ShaderMenu::PackName(int index) const
{
    if (index == kPackInternal)
        return ShaderSettings::kPackInternal;
    if (index < 0 || index >= static_cast<int>(m_packs.size()))
        return std::string();
    return m_packs[static_cast<size_t>(index)];
}

bool UIScene_ShaderMenu::PackUsable(int index) const
{
    if (index == kPackInternal)
        return true;
    std::string name = PackName(index);
    return !name.empty() && SummaryFor(name).usable;
}

void UIScene_ShaderMenu::SetShadersOn(bool on)
{
    if (on == ShadersOn(*m_working))
        return;
    if (on && m_working->shaderPack == ShaderSettings::kPackOff)
    {
        gConsoleUIController.PlayUISFX(SoundEvents::UI_BACK);
        Notify(L"Choose a shader pack to turn shaders on", kTextLight);
        return;
    }
    gConsoleUIController.PlayUISFX(SoundEvents::UI_SCROLL);
    m_working->enableShaders = on;
}

void UIScene_ShaderMenu::RefreshPacks(bool force)
{
    std::vector<std::string> packs = ShaderPackManager::Get().ListPacks();
    if (!force && packs == m_packs)
        return;
    Focus& focus = m_focus[eTab_Packs];
    std::string focused = focus.kind == eTarget_Pack && focus.index >= 0 ? PackName(focus.index) : std::string();
    if (force)
        sSummaries.clear();
    m_packs = packs;
    if (!focused.empty())
    {
        std::vector<std::string>::const_iterator found = std::find(m_packs.begin(), m_packs.end(), focused);
        if (found != m_packs.end())
            focus.index = static_cast<int>(found - m_packs.begin());
        else if (focus.index >= static_cast<int>(m_packs.size()))
            focus.index = m_packs.empty() ? kPackInternal : static_cast<int>(m_packs.size()) - 1;
    }
    BuildTargets();
}

void UIScene_ShaderMenu::FocusSelectedPack()
{
    Focus& focus = m_focus[eTab_Packs];
    if (m_working->shaderPack == ShaderSettings::kPackInternal)
    {
        focus = Focus{eTarget_Pack, kPackInternal};
    }
    else
    {
        std::vector<std::string>::const_iterator found =
            std::find(m_packs.begin(), m_packs.end(), m_working->shaderPack);
        focus = found != m_packs.end() ? Focus{eTarget_Pack, static_cast<int>(found - m_packs.begin())}
                                       : Focus{eTarget_Toggle, 0};
    }
    BuildTargets();
    int index = FindTarget(focus);
    if (index >= 0)
        EnsureVisible(m_targets[static_cast<size_t>(index)]);
    m_scroll[eView_List].position = m_scroll[eView_List].target;
}

void UIScene_ShaderMenu::SelectPack(int index)
{
    std::string name = PackName(index);
    if (name.empty())
        return;
    if (!PackUsable(index))
    {
        gConsoleUIController.PlayUISFX(SoundEvents::UI_BACK);
        Notify(L"That shader pack can't be used: " + SummaryFor(name).problem, kTextError);
        return;
    }
    gConsoleUIController.PlayUISFX(SoundEvents::UI_PRESS);
    if (name == m_applied->shaderPack && m_applied->UsesPack() && !PackChangePending())
    {
        if (ShaderPackManager::Get().ActivePack() != nullptr)
            SetTab(eTab_Options);
        else
            Apply(true);
        return;
    }
    m_working->shaderPack = name;
    m_working->enableShaders = true;
}

void UIScene_ShaderMenu::BuildCells()
{
    m_cells.clear();
    m_columns = 2;
    if (m_optionsPack.empty())
    {
        m_screen.clear();
        m_history.clear();
        return;
    }
    if (!m_screen.empty() && m_properties->Screen(m_screen) == nullptr)
    {
        m_screen.clear();
        m_history.clear();
    }

    std::vector<std::string> items;
    const ShaderScreen* screen = m_properties->Screen(m_screen);
    if (screen != nullptr)
    {
        items = screen->items;
        m_columns = std::max(1, std::min(screen->columns, 4));
    }
    else if (m_screen.empty())
    {
        items.push_back("*");
    }

    std::set<std::string> listed;
    for (const ShaderScreen& other : m_properties->Screens())
        listed.insert(other.items.begin(), other.items.end());

    for (const std::string& item : items)
    {
        if (item.empty() || item == "<empty>")
        {
            m_cells.push_back(Cell{eCell_Empty, std::string()});
        }
        else if (item == "<profile>")
        {
            m_cells.push_back(Cell{m_properties->Profiles().empty() ? eCell_Empty : eCell_Profile, std::string()});
        }
        else if (item.size() > 2 && item.front() == '[' && item.back() == ']')
        {
            std::string name = item.substr(1, item.size() - 2);
            m_cells.push_back(Cell{m_properties->Screen(name) != nullptr ? eCell_Link : eCell_Empty, name});
        }
        else if (item == "*")
        {
            for (const ShaderOption& option : m_options->Options())
            {
                if (listed.count(option.name) == 0 && (option.IsBoolean() || option.values.size() > 1))
                    m_cells.push_back(Cell{eCell_Option, option.name});
            }
        }
        else
        {
            const ShaderOption* option = m_options->Find(item);
            bool usable = option != nullptr && (option->IsBoolean() || !option->values.empty());
            m_cells.push_back(Cell{usable ? eCell_Option : eCell_Empty, item});
        }
    }
    while (!m_cells.empty() && m_cells.back().kind == eCell_Empty)
        m_cells.pop_back();
}

void UIScene_ShaderMenu::OpenScreen(const std::string& screen)
{
    gConsoleUIController.PlayUISFX(SoundEvents::UI_PRESS);
    Page page = {m_screen, m_focus[eTab_Options], m_scroll[eView_Grid].position};
    m_history.push_back(page);
    m_screen = screen;
    m_focus[eTab_Options] = Focus{eTarget_Cell, -1};
    ResetScroll(eView_Grid, 0.0f);
    BuildCells();
    BuildTargets();
}

void UIScene_ShaderMenu::CloseScreen()
{
    if (m_history.empty())
        return;
    gConsoleUIController.PlayUISFX(SoundEvents::UI_BACK);
    Page page = m_history.back();
    m_history.pop_back();
    m_screen = page.screen;
    m_focus[eTab_Options] = page.focus;
    ResetScroll(eView_Grid, page.scroll);
    BuildCells();
    BuildTargets();
}

bool UIScene_ShaderMenu::CellChangeable(int index) const
{
    if (index < 0 || index >= static_cast<int>(m_cells.size()))
        return false;
    const Cell& cell = m_cells[static_cast<size_t>(index)];
    if (cell.kind == eCell_Profile)
        return true;
    if (cell.kind != eCell_Option)
        return false;
    const ShaderOption* option = m_options->Find(cell.name);
    return option != nullptr && !option->ambiguous && (option->IsBoolean() || option->values.size() > 1);
}

bool UIScene_ShaderMenu::IsSlider(const Cell& cell) const
{
    if (cell.kind != eCell_Option)
        return false;
    const ShaderOption* option = m_options->Find(cell.name);
    return option != nullptr && !option->IsBoolean() && option->values.size() > 1 && m_properties->IsSlider(cell.name);
}

int UIScene_ShaderMenu::SliderPosition(const Cell& cell, int* count) const
{
    const ShaderOption* option = m_options->Find(cell.name);
    *count = option != nullptr ? static_cast<int>(option->values.size()) : 0;
    if (option == nullptr)
        return 0;
    std::vector<std::string>::const_iterator found =
        std::find(option->values.begin(), option->values.end(), m_options->Value(cell.name));
    return found == option->values.end() ? 0 : static_cast<int>(found - option->values.begin());
}

void UIScene_ShaderMenu::SetSliderPosition(const Cell& cell, int position)
{
    const ShaderOption* option = m_options->Find(cell.name);
    if (option == nullptr || position < 0 || position >= static_cast<int>(option->values.size()))
        return;
    if (m_options->SetValue(cell.name, option->values[static_cast<size_t>(position)]))
        EditDraft();
}

std::wstring UIScene_ShaderMenu::CellValue(const Cell& cell, unsigned int* colour) const
{
    const ShaderPackLang& lang = ShaderPackManager::Get().Lang();
    if (cell.kind == eCell_Profile)
    {
        std::string active = m_properties->ActiveProfile(*m_options);
        *colour = active.empty() ? kTextYellow : kTextLight;
        return active.empty() ? std::wstring(L"Custom") : Wide(lang.ProfileLabel(active));
    }
    const ShaderOption* option = m_options->Find(cell.name);
    if (option == nullptr)
    {
        *colour = kTextGrey;
        return std::wstring();
    }
    std::string value = m_options->Value(cell.name);
    if (option->IsBoolean())
        *colour = value == option->defaultValue ? kTextLight : value == "true" ? kTextOn : kTextOff;
    else
        *colour = kTextValue;
    return Wide(lang.ValueLabel(*option, value));
}

void UIScene_ShaderMenu::CellValueRect(int index, int* x0, int* x1) const
{
    *x0 = 0;
    *x1 = 0;
    Minecraft* minecraft = Minecraft::GetInstance();
    if (minecraft->mFont == NULL || index < 0 || index >= static_cast<int>(m_cells.size()))
        return;
    int columns = std::max(1, m_columns);
    int cellWidth = (kGridCellRight - kGridLeft - (columns - 1) * kCellGap) / columns;
    int cellX0 = kGridLeft + (index % columns) * (cellWidth + kCellGap);
    unsigned int colour = 0;
    const Cell& cell = m_cells[static_cast<size_t>(index)];
    int width = ValueBoxWidth(*minecraft->mFont, CellValue(cell, &colour), cellWidth, IsSlider(cell));
    *x1 = cellX0 + cellWidth - 2;
    *x0 = *x1 - width;
}

bool UIScene_ShaderMenu::OptionModified(const std::string& name) const
{
    return m_options->Find(name) != nullptr && m_options->Value(name) != ShaderPackManager::Get().Options().Value(name);
}

void UIScene_ShaderMenu::CycleOption(const std::string& name, int step)
{
    const ShaderOption* option = m_options->Find(name);
    if (option == nullptr || option->ambiguous)
        return;
    std::string value = m_options->Value(name);
    std::string next;
    if (option->IsBoolean())
    {
        next = value == "true" ? "false" : "true";
    }
    else
    {
        int count = static_cast<int>(option->values.size());
        if (count < 2)
            return;
        std::vector<std::string>::const_iterator found = std::find(option->values.begin(), option->values.end(), value);
        int index = found == option->values.end() ? (step > 0 ? -1 : count)
                                                  : static_cast<int>(found - option->values.begin());
        if (m_properties->IsSlider(name))
            index = std::max(0, std::min(index + step, count - 1));
        else
            index = ((index + step) % count + count) % count;
        next = option->values[static_cast<size_t>(index)];
    }
    if (next != value && m_options->SetValue(name, next))
    {
        gConsoleUIController.PlayUISFX(SoundEvents::UI_SCROLL);
        EditDraft();
    }
}

void UIScene_ShaderMenu::CycleProfile(int step)
{
    const std::vector<ShaderProfile>& profiles = m_properties->Profiles();
    int count = static_cast<int>(profiles.size());
    if (count == 0)
        return;
    std::string active = m_properties->ActiveProfile(*m_options);
    int index = -1;
    for (int candidate = 0; candidate < count; ++candidate)
    {
        if (profiles[static_cast<size_t>(candidate)].name == active)
            index = candidate;
    }
    index = index < 0 ? (step > 0 ? 0 : count - 1) : ((index + step) % count + count) % count;
    ShaderProfile profile = profiles[static_cast<size_t>(index)];
    ShaderPackProperties::ApplyProfile(profile, *m_options);
    gConsoleUIController.PlayUISFX(SoundEvents::UI_SCROLL);
    EditDraft();
}

void UIScene_ShaderMenu::ChangeSetting(int setting, int step)
{
    const ShaderSettings::ChoiceInfo& info = ShaderSettings::Describe(setting);
    int current = m_working->GetChoice(setting);
    int index = info.slider ? std::max(0, std::min(current + step, info.count - 1))
                            : ((current + step) % info.count + info.count) % info.count;
    if (index == current)
        return;
    m_working->SetChoice(setting, index);
    gConsoleUIController.PlayUISFX(SoundEvents::UI_SCROLL);
}

bool UIScene_ShaderMenu::SettingModified(int setting) const
{
    return m_working->GetChoice(setting) != m_applied->GetChoice(setting);
}

bool UIScene_ShaderMenu::PackChangePending() const
{
    return m_working->shaderPack != m_applied->shaderPack || m_working->enableShaders != m_applied->enableShaders;
}

bool UIScene_ShaderMenu::SettingsChangePending() const
{
    for (int setting = 0; setting < ShaderSettings::Choice_Count; ++setting)
    {
        if (SettingModified(setting))
            return true;
    }
    return false;
}

bool UIScene_ShaderMenu::OptionsChangePending() const
{
    return !m_optionsPack.empty() && m_options->SaveValues() != m_appliedOptions;
}

bool UIScene_ShaderMenu::HasPendingChanges() const
{
    return PackChangePending() || SettingsChangePending() || OptionsChangePending();
}

void UIScene_ShaderMenu::Apply(bool reload)
{
    ShaderPackManager& manager = ShaderPackManager::Get();
    bool packChanged = reload || m_working->shaderPack != m_applied->shaderPack;
    if (OptionsChangePending())
    {
        manager.LoadOptionValues(m_options->SaveValues());
        manager.SaveOptions();
    }
    manager.EditSettings() = *m_working;
    if (packChanged)
        manager.SelectPack(m_working->shaderPack);
    else
        manager.SaveSettings();
    ShaderPipeline::Get().RequestReload();

    *m_applied = manager.Settings();
    *m_working = *m_applied;
    if (packChanged)
        ResetDraft();
    else
        m_appliedOptions = manager.OptionValues();
    sSummaries.erase(m_applied->shaderPack);
    BuildCells();

    m_awaitingReload = false;
    if (m_applied->UsesPack() && manager.ActivePack() == nullptr)
    {
        Notify(L"The shader pack could not be loaded. See the log for details.", kTextError);
    }
    else if (!ShadersOn(*m_applied))
    {
        Notify(L"Shaders are off", kTextLight);
    }
    else if (Minecraft::GetInstance()->mLevel == NULL)
    {
        Notify(L"Applied. The shader pack loads when you enter a world.", kTextLight);
    }
    else if (SplitScreen())
    {
        Notify(L"Applied. Shaders show when only one player is playing.", kTextLight);
    }
    else
    {
        m_awaitingReload = true;
        m_reloadTime = System::processTimeInMilliSecs();
        Notify(L"Reloading shaders...", kTextLight);
    }
}

void UIScene_ShaderMenu::Done()
{
    if (HasPendingChanges())
        Apply(false);
    navigateBack();
}

void UIScene_ShaderMenu::CheckReload()
{
    if (!m_awaitingReload)
        return;
    ShaderPipeline& pipeline = ShaderPipeline::Get();
    if (pipeline.ReloadRequested())
    {
        if (System::processTimeInMilliSecs() - m_reloadTime > kReloadWaitMilliseconds)
            m_awaitingReload = false;
        return;
    }
    m_awaitingReload = false;
    sSummaries.erase(m_applied->shaderPack);
    if (!pipeline.Enabled())
    {
        Notify(L"The shader pack could not be loaded. See the log for details.", kTextError);
        return;
    }
    int problems = static_cast<int>(pipeline.Errors().size());
    if (problems == 0)
        Notify(L"Shaders reloaded", kTextOn);
    else
        Notify(L"Shaders reloaded with " + Plural(problems, L"problem", L"problems") + L"; see the pack's details",
               kTextWarning);
}

void UIScene_ShaderMenu::OpenPackFolder()
{
    std::error_code error;
    std::filesystem::path folder = ShaderPackManager::Get().PacksFolder();
    std::filesystem::create_directories(folder, error);
    std::string path = folder.generic_u8string();
    std::string url = "file://" + std::string(path.empty() || path[0] != '/' ? "/" : "") + path;
    if (!SDL_OpenURL(url.c_str()))
        Notify(L"The shader pack folder could not be opened", kTextError);
}

void UIScene_ShaderMenu::ShowFileDialog(bool save)
{
    static const SDL_DialogFileFilter kFilters[] = {{"Shader pack settings (*.txt)", "txt"}, {"All files", "*"}};
    ShaderPackManager& manager = ShaderPackManager::Get();
    if (m_optionsPack.empty())
        return;
    intptr_t request = 0;
    {
        std::lock_guard<std::mutex> lock(sDialog.mutex);
        if (sDialog.owner != nullptr && sDialog.owner != this)
        {
            Notify(L"Another player has a file window open", kTextError);
            return;
        }
        sDialog.owner = this;
        sDialog.kind = save ? eDialog_Export : eDialog_Import;
        sDialog.ready = false;
        request = ++sDialog.request;
    }
    std::error_code error;
    std::filesystem::create_directories(manager.PacksFolder(), error);
    void* userdata = reinterpret_cast<void*>(request);
    if (save)
    {
        std::string name = m_optionsPack;
        if (name.size() > 4 && Lower(name.substr(name.size() - 4)) == ".zip")
            name.resize(name.size() - 4);
        std::string location = (manager.PacksFolder() / std::filesystem::u8path(name + " settings.txt")).u8string();
        SDL_ShowSaveFileDialog(&DialogCallback, userdata, PcWindowHandle(), kFilters, 2, location.c_str());
    }
    else
    {
        std::string location = manager.PacksFolder().u8string();
        SDL_ShowOpenFileDialog(&DialogCallback, userdata, PcWindowHandle(), kFilters, 2, location.c_str(), false);
    }
}

void UIScene_ShaderMenu::HandleDialogResult()
{
    int kind = eDialog_None;
    bool chosen = false;
    std::string path;
    std::string error;
    {
        std::lock_guard<std::mutex> lock(sDialog.mutex);
        if (!sDialog.ready || sDialog.owner != this)
            return;
        sDialog.ready = false;
        sDialog.owner = nullptr;
        kind = sDialog.kind;
        chosen = sDialog.chosen;
        path = sDialog.path;
        error = sDialog.error;
    }
    if (!error.empty())
        Notify(L"A file window could not be opened: " + Wide(error), kTextError);
    else if (chosen && kind == eDialog_Import)
        ImportSettings(path);
    else if (chosen && kind == eDialog_Export)
        ExportSettings(path);
}

void UIScene_ShaderMenu::ImportSettings(const std::string& path)
{
    if (m_optionsPack.empty())
        return;
    std::filesystem::path file = std::filesystem::u8path(path);
    std::wstring name = Wide(file.filename().u8string());
    std::ifstream stream(file, std::ios::binary);
    if (!stream)
    {
        Notify(L"Could not read " + name, kTextError);
        return;
    }
    std::ostringstream contents;
    contents << stream.rdbuf();
    std::string text = contents.str();
    bool empty = text.find_first_not_of(" \t\r\n") == std::string::npos;
    if (!empty && !HasPackOptions(text, *m_options))
    {
        Notify(name + L" has no settings for this shader pack", kTextError);
        return;
    }
    m_options->LoadValues(text);
    EditDraft();
    LOG_INFO(sLog, "Imported shader settings from %s", path.c_str());
    if (OptionsChangePending())
        Notify(L"Imported settings from " + name + L". Apply to use them.", kTextYellow);
    else
        Notify(L"Imported settings from " + name + L". They match the applied settings.", kTextLight);
}

void UIScene_ShaderMenu::ExportSettings(const std::string& path)
{
    std::filesystem::path file = std::filesystem::u8path(path);
    if (!file.has_extension())
        file += ".txt";
    std::wstring name = Wide(file.filename().u8string());
    std::ofstream stream(file, std::ios::binary | std::ios::trunc);
    if (!stream || !(stream << m_options->SaveValues()))
    {
        Notify(L"Could not write " + name, kTextError);
        return;
    }
    LOG_INFO(sLog, "Exported shader settings to %s", file.u8string().c_str());
    Notify(L"Exported the settings to " + name, kTextYellow);
}

void UIScene_ShaderMenu::HandleDrops()
{
    if (!m_dropComplete || m_modal != eModal_None)
        return;
    m_dropComplete = false;
    std::vector<std::string> files;
    files.swap(m_dropped);
    if (files.empty())
        return;
    if (m_hidden)
        ToggleHidden();

    if (m_tab == eTab_Options && !m_optionsPack.empty() && !ShaderPack::IsShaderPack(std::filesystem::u8path(files[0])))
    {
        if (files.size() == 1)
            ImportSettings(files[0]);
        else
            Notify(L"Drop one settings file at a time to import it", kTextError);
        return;
    }

    ShaderPackManager& manager = ShaderPackManager::Get();
    int added = 0;
    std::string lastName;
    std::wstring failure;
    for (const std::string& file : files)
    {
        std::string name;
        std::string error;
        if (manager.InstallPack(std::filesystem::u8path(file), name, error))
        {
            ++added;
            lastName = name;
        }
        else
        {
            failure = Wide(name) + L": " + Wide(error);
        }
    }
    RefreshPacks(true);
    SetTab(eTab_Packs);
    if (added == 0)
    {
        Notify(files.size() == 1 ? L"Could not add " + failure : std::wstring(L"Those files are not shader packs"),
               kTextError);
        return;
    }
    gConsoleUIController.PlayUISFX(SoundEvents::UI_PRESS);
    if (added > 1)
    {
        Notify(L"Added " + Plural(added, L"shader pack", L"shader packs"), kTextYellow);
        return;
    }
    std::vector<std::string>::const_iterator found = std::find(m_packs.begin(), m_packs.end(), lastName);
    if (found != m_packs.end())
    {
        int index = static_cast<int>(found - m_packs.begin());
        if (PackUsable(index))
        {
            m_working->shaderPack = lastName;
            m_working->enableShaders = true;
        }
        m_focus[eTab_Packs] = Focus{eTarget_Pack, index};
        BuildTargets();
        int target = FindTarget(m_focus[eTab_Packs]);
        if (target >= 0)
            EnsureVisible(m_targets[static_cast<size_t>(target)]);
    }
    Notify(L"Added shader pack " + PackDisplayName(lastName), kTextYellow);
}

void UIScene_ShaderMenu::ResetAllOptions()
{
    bool changed = !m_options->SaveValues().empty();
    m_options->ResetAll();
    EditDraft();
    if (!changed)
        Notify(L"Every option is already at its default", kTextLight);
    else if (OptionsChangePending())
        Notify(L"Every option is back to its default. Apply to use them.", kTextYellow);
    else
        Notify(L"Every option is back to its default", kTextLight);
}

void UIScene_ShaderMenu::OpenModal(int modal)
{
    gConsoleUIController.PlayUISFX(SoundEvents::UI_PRESS);
    m_modal = modal;
    m_modalFocus = modal == eModal_Download ? 0 : 1;
    m_dragIndex = -1;
    BuildTargets();
}

void UIScene_ShaderMenu::CloseModal(bool accept)
{
    int modal = m_modal;
    m_modal = eModal_None;
    gConsoleUIController.PlayUISFX(accept ? SoundEvents::UI_PRESS : SoundEvents::UI_BACK);
    if (accept && modal == eModal_Reset)
        ResetAllOptions();
    else if (accept && modal == eModal_Download && !SDL_OpenURL(kDownloadUrl))
        Notify(L"Your web browser could not be opened", kTextError);
    else if (accept && modal == eModal_Discard)
    {
        navigateBack();
        return;
    }
    BuildTargets();
}

void UIScene_ShaderMenu::Notify(const std::wstring& text, unsigned int colour)
{
    m_notice.text = text;
    m_notice.colour = colour;
    m_notice.time = System::processTimeInMilliSecs();
}

void UIScene_ShaderMenu::ToggleHidden()
{
    if (!m_hidden && Minecraft::GetInstance()->mLevel == NULL)
        return;
    m_hidden = !m_hidden;
    m_dragIndex = -1;
    gConsoleUIController.PlayUISFX(m_hidden ? SoundEvents::UI_PRESS : SoundEvents::UI_BACK);
    updateComponents();
    BuildTargets();
    updateTooltips();
}

std::wstring UIScene_ShaderMenu::Breadcrumb() const
{
    const ShaderPackLang& lang = ShaderPackManager::Get().Lang();
    std::wstring title = Wide(lang.ScreenLabel(std::string()));
    if (title.empty())
        title = PackDisplayName(m_optionsPack);
    std::vector<std::wstring> parts(1, title);
    for (const Page& page : m_history)
    {
        if (!page.screen.empty())
            parts.push_back(Wide(lang.ScreenLabel(page.screen)));
    }
    if (!m_screen.empty())
        parts.push_back(Wide(lang.ScreenLabel(m_screen)));
    return Join(parts, MenuPainter::FormatReset() + L" > ");
}

std::wstring UIScene_ShaderMenu::ButtonLabel(int kind, int index) const
{
    switch (kind)
    {
    case eTarget_Hide:
        return L10N::GetString(IDS_LCE_SHADER_HIDE_MENU);
    case eTarget_Cancel:
        return L10N::GetString(IDS_CANCEL);
    case eTarget_Apply:
        return L10N::GetString(IDS_LCE_SHADER_APPLY);
    case eTarget_Done:
        return L10N::GetString(IDS_DONE);
    case eTarget_OpenFolder:
        return L"Open Pack Folder";
    case eTarget_Download:
        return L"Download Shaders";
    case eTarget_Back:
        return std::wstring(L"< ") + L10N::GetString(IDS_BACK);
    case eTarget_Import:
        return L"Import";
    case eTarget_Export:
        return L"Export";
    case eTarget_Reset:
        return L"Reset";
    case eTarget_Modal:
        if (index != 0)
            return L10N::GetString(IDS_CANCEL);
        return m_modal == eModal_Reset ? L"Reset" : m_modal == eModal_Discard ? L"Discard" : L"Open";
    default:
        return std::wstring();
    }
}

UIScene_ShaderMenu::Text UIScene_ShaderMenu::DescribePack(int index) const
{
    Text text;
    std::string name = PackName(index);
    bool applied = name == m_applied->shaderPack && m_applied->enableShaders;
    bool selected = name == m_working->shaderPack && m_working->enableShaders;
    ShaderPackManager& manager = ShaderPackManager::Get();

    if (index == kPackInternal)
    {
        text.Add(L"(internal)", kTextYellow, 0);
        if (applied && !PackChangePending())
            text.Add(L"In use", kTextOn, 2);
        else if (selected)
            text.Add(L"Selected. Apply to use it.", kTextWarning, 2);
        text.Add(L"Runs the shader pipeline with built-in programs that match the standard look. Useful for checking "
                 L"that the pipeline itself works.",
                 kTextBody, 6);
        return text;
    }

    const PackSummary& summary = SummaryFor(name);
    text.Add(PackDisplayName(name), kTextYellow, 0);
    if (!summary.usable)
        text.Add(L"Can't be used: " + summary.problem, kTextError, 2);
    else if (applied && manager.ActivePack() == nullptr)
        text.Add(L"Failed to load. See the log for details, or select it again to retry.", kTextError, 2);
    else if (applied && !PackChangePending())
        text.Add(L"In use", kTextOn, 2);
    else if (selected)
        text.Add(L"Selected. Apply to load it.", kTextWarning, 2);
    else if (name == m_applied->shaderPack && !m_applied->enableShaders)
        text.Add(L"Chosen, but shaders are off", kTextGrey, 2);
    if (applied && SplitScreen())
        text.Add(L"Shaders show when only one player is playing.", kTextGrey, 2);

    if (!summary.profiles.empty())
    {
        text.Add(L"Profiles", kTextWhite, 7);
        text.Add(Join(summary.profiles, MenuPainter::FormatReset() + L", "), kTextBody, 0);
    }
    if (summary.totalPages > 0)
    {
        text.Add(L"Settings", kTextWhite, 7);
        text.Add(Plural(summary.mainPages, L"main page", L"main pages") + L", "
                     + Plural(summary.totalPages, L"page", L"pages") + L" in total",
                 kTextBody, 0);
    }
    if (name == m_optionsPack)
    {
        int booleans = 0;
        int values = 0;
        for (const ShaderOption& option : m_options->Options())
        {
            if (option.IsBoolean())
                ++booleans;
            else if (option.values.size() > 1)
                ++values;
        }
        text.Add(L"Options", kTextWhite, 7);
        text.Add(std::to_wstring(booleans) + L" on/off and " + std::to_wstring(values) + L" value options", kTextBody,
                 0);
    }
    if (!summary.worlds.empty())
    {
        text.Add(L"Worlds", kTextWhite, 7);
        text.Add(Join(summary.worlds, L", "), kTextBody, 0);
    }
    text.Add(L"File", kTextWhite, 7);
    text.Add(Wide(name) + L" (" + (summary.folder ? std::wstring(L"folder, ") : std::wstring())
                 + SizeText(summary.size) + L")",
             kTextBody, 0);

    ShaderPipeline& pipeline = ShaderPipeline::Get();
    if (applied && m_applied->UsesPack() && pipeline.Enabled() && !pipeline.ReloadRequested())
    {
        std::vector<std::string> problems = manager.Errors();
        problems.insert(problems.end(), pipeline.Errors().begin(), pipeline.Errors().end());
        if (!problems.empty())
        {
            text.Add(Plural(static_cast<int>(problems.size()), L"problem", L"problems"), kTextError, 7);
            size_t shown = std::min<size_t>(problems.size(), 20);
            for (size_t problem = 0; problem < shown; ++problem)
                text.Add(Wide(problems[problem]), kTextProblem, problem == 0 ? 0 : 2);
            if (problems.size() > shown)
                text.Add(L"and " + std::to_wstring(problems.size() - shown) + L" more in the log", kTextGrey, 2);
        }
    }
    return text;
}

UIScene_ShaderMenu::Text UIScene_ShaderMenu::DescribeCell(int index) const
{
    Text text;
    if (index < 0 || index >= static_cast<int>(m_cells.size()))
        return text;
    const Cell& cell = m_cells[static_cast<size_t>(index)];
    const ShaderPackLang& lang = ShaderPackManager::Get().Lang();
    if (cell.kind == eCell_Link)
    {
        text.Add(Wide(lang.ScreenLabel(cell.name)), kTextYellow, 0);
        std::wstring comment = Wide(lang.ScreenComment(cell.name));
        if (comment.empty())
            text.Add(L"Opens this page of settings.", kTextGrey, 3);
        else
            text.AddSentences(comment, kTextBody, 3);
        return text;
    }
    if (cell.kind == eCell_Profile)
    {
        text.Add(L"Profile", kTextYellow, 0);
        std::wstring comment = Wide(lang.Get("profile.comment", std::string()));
        if (comment.empty())
            comment = L"Sets many options at once to a preset chosen by the shader pack. Changing an option the "
                      L"profile controls shows Custom.";
        text.AddSentences(comment, kTextBody, 3);
        std::vector<std::wstring> labels;
        for (const ShaderProfile& profile : m_properties->Profiles())
            labels.push_back(Wide(lang.ProfileLabel(profile.name)));
        text.Add(L"Profiles: " + Join(labels, MenuPainter::FormatReset() + L", "), kTextGrey, 5);
        return text;
    }
    const ShaderOption* option = m_options->Find(cell.name);
    if (option == nullptr)
        return text;
    text.Add(Wide(lang.OptionLabel(option->name)), kTextYellow, 0);
    text.AddSentences(Wide(lang.OptionComment(*option)), kTextBody, 3);
    if (!option->IsBoolean() && option->values.size() < 2)
        return text;
    std::wstring footer = L"Default: " + Wide(lang.ValueLabel(*option, option->defaultValue));
    if (option->ambiguous)
        text.Add(L"The pack sets this option in more than one way, so it can't be changed here.", kTextGrey, 5);
    else if (OptionModified(option->name))
        text.Add(footer + MenuPainter::FormatReset() + L"   Not applied yet", kTextModified, 5);
    else
        text.Add(footer, kTextGrey, 5);
    return text;
}

UIScene_ShaderMenu::Text UIScene_ShaderMenu::DescribeSetting(int setting) const
{
    Text text;
    const ShaderSettings::ChoiceInfo& info = ShaderSettings::Describe(setting);
    text.Add(Wide(info.name), kTextYellow, 0);
    text.Add(kSettingDescriptions[setting], kTextBody, 3);
    std::vector<std::wstring> values;
    for (int index = 0; index < info.count; ++index)
        values.push_back(SettingLabel(setting, index));
    text.Add(L"Values", kTextWhite, 7);
    text.Add(Join(values, L", "), kTextBody, 0);
    if (info.macro != nullptr)
    {
        text.Add(L"Shader packs read this as", kTextWhite, 7);
        text.Add(Wide(info.macro), kTextBody, 0);
    }
    text.Add(L"Default: " + SettingLabel(setting, ShaderSettings().GetChoice(setting)), kTextGrey, 7);
    if (SettingModified(setting))
        text.Add(L"Not applied yet", kTextModified, 2);
    return text;
}

UIScene_ShaderMenu::Text UIScene_ShaderMenu::DescribeButton(int kind, int index) const
{
    Text text;
    switch (kind)
    {
    case eTarget_Hide:
        text.Add(ButtonLabel(kind, index), kTextYellow, 0);
        text.Add(L"Hides this menu so you can see the world with the shader pack. Press any button or click to bring "
                 L"it back.",
                 kTextBody, 3);
        break;
    case eTarget_Tab:
        if (index == eTab_Packs)
        {
            text.Add(L10N::GetString(IDS_LCE_SHADER_PACKS), kTextYellow, 0);
            text.Add(L"Choose the shader pack the world is drawn with, or turn shaders off.", kTextBody, 3);
        }
        else if (index == eTab_Options)
        {
            text.Add(L10N::GetString(IDS_LCE_SHADER_PACK_SETTINGS), kTextYellow, 0);
            text.Add(L"Change the options of the shader pack in use: its profile and every page of settings it "
                     L"offers.",
                     kTextBody, 3);
            if (!TabEnabled(eTab_Options))
                text.Add(L"Choose a shader pack first.", kTextGrey, 5);
        }
        else
        {
            text.Add(L10N::GetString(IDS_LCE_SHADER_SETTINGS), kTextYellow, 0);
            text.Add(L"Settings that apply to every shader pack, such as antialiasing and render quality.", kTextBody,
                     3);
        }
        break;
    case eTarget_Cancel:
        text.Add(ButtonLabel(kind, index), kTextYellow, 0);
        text.Add(L"Closes this menu and throws away the changes that have not been applied.", kTextBody, 3);
        break;
    case eTarget_Apply:
        text.Add(ButtonLabel(kind, index), kTextYellow, 0);
        text.Add(L"Loads the chosen shader pack with its settings now, so you can see the result without closing the "
                 L"menu.",
                 kTextBody, 3);
        if (!HasPendingChanges())
            text.Add(L"Nothing has changed.", kTextGrey, 5);
        break;
    case eTarget_Done:
        text.Add(ButtonLabel(kind, index), kTextYellow, 0);
        text.Add(L"Applies any changes and closes this menu.", kTextBody, 3);
        break;
    case eTarget_Toggle:
        text.Add(ShadersOn(*m_working) ? L"Shaders: ON" : L"Shaders: OFF", kTextYellow, 0);
        text.Add(L"Turns shader packs on or off. The chosen pack is remembered while shaders are off.", kTextBody, 3);
        if (m_working->shaderPack == ShaderSettings::kPackOff)
            text.Add(L"Choose a shader pack below to turn shaders on.", kTextGrey, 5);
        else if (ShadersOn(*m_working) != ShadersOn(*m_applied))
            text.Add(L"Not applied yet", kTextModified, 5);
        break;
    case eTarget_OpenFolder:
        text.Add(L"Open Shader Pack Folder", kTextYellow, 0);
        text.Add(L"Opens " + Wide(ShaderPackManager::Get().PacksFolder().u8string()) + L".", kTextBody, 3);
        text.Add(L"Put shader packs there as .zip files or folders, or drag them onto this window. The list updates "
                 L"by itself.",
                 kTextBody, 3);
        break;
    case eTarget_Download:
        text.Add(ButtonLabel(kind, index), kTextYellow, 0);
        text.Add(L"Opens modrinth.com/shaders in your web browser, where many shader packs made for OptiFine and Iris "
                 L"can be downloaded.",
                 kTextBody, 3);
        break;
    case eTarget_Back:
        text.Add(L10N::GetString(IDS_BACK), kTextYellow, 0);
        text.Add(L"Returns to the previous page of settings.", kTextBody, 3);
        break;
    case eTarget_Import:
        text.Add(L"Import Settings", kTextYellow, 0);
        text.Add(L"Loads the options in a settings file, such as one exported from this menu, OptiFine or Iris. "
                 L"Dragging a settings file onto this window does the same.",
                 kTextBody, 3);
        break;
    case eTarget_Export:
        text.Add(L"Export Settings", kTextYellow, 0);
        text.Add(L"Saves the options shown here to a file, to share them or keep a copy.", kTextBody, 3);
        break;
    case eTarget_Reset:
        text.Add(ButtonLabel(kind, index), kTextYellow, 0);
        text.Add(L"Puts every option of this shader pack back to its default, after asking first.", kTextBody, 3);
        break;
    default:
        break;
    }
    return text;
}

UIScene_ShaderMenu::Text UIScene_ShaderMenu::DescribeFocus() const
{
    const Focus& focus = m_focus[m_tab];
    switch (focus.kind)
    {
    case eTarget_Pack:
        return DescribePack(focus.index);
    case eTarget_Cell:
        return DescribeCell(focus.index);
    case eTarget_Setting:
        return DescribeSetting(focus.index);
    default:
        return DescribeButton(focus.kind, focus.index);
    }
}

int UIScene_ShaderMenu::DrawParagraphs(MenuPainter& painter, const Text& text, const MenuRect& area, int scroll)
{
    int y = area.y0 - scroll;
    int top = y;
    for (const Paragraph& paragraph : text.paragraphs)
    {
        y += paragraph.gap;
        std::vector<std::wstring> lines = painter.Wrap(paragraph.text, area.x1 - area.x0);
        for (const std::wstring& line : lines)
        {
            if (y + kLineHeight > area.y0 && y < area.y1)
                painter.Text(line, area.x0, y, paragraph.colour, true);
            y += kLineHeight;
        }
    }
    return y - top;
}

void UIScene_ShaderMenu::DrawTitle(MenuPainter& painter)
{
    long long age = System::processTimeInMilliSecs() - m_notice.time;
    std::wstring subtitle = L"More Options";
    unsigned int colour = kTextSubtitle;
    if (!m_notice.text.empty() && age < kNoticeMilliseconds)
    {
        subtitle = m_notice.text;
        float fade = age > kNoticeMilliseconds - 500 ? static_cast<float>(kNoticeMilliseconds - age) / 500.0f : 1.0f;
        colour = MenuPainter::WithAlpha(m_notice.colour, fade);
    }
    else if (HasPendingChanges())
    {
        subtitle = L"Changes not applied yet";
        colour = kTextModified;
    }
    else if (SplitScreen() && ShadersOn(*m_applied))
    {
        subtitle = L"Shaders show when only one player is playing";
        colour = kTextGrey;
    }
    painter.Title(L"Shaders", subtitle, colour);
}

void UIScene_ShaderMenu::DrawTabs(MenuPainter& painter, bool selected)
{
    static const int kLabels[eTab_Count] = {IDS_LCE_SHADER_PACKS, IDS_LCE_SHADER_PACK_SETTINGS,
                                            IDS_LCE_SHADER_SETTINGS};
    for (int tab = 0; tab < eTab_Count; ++tab)
    {
        bool current = tab == m_tab;
        if (current != selected)
            continue;
        int x0 = kPanelLeft + tab * (kTabWidth + kTabGap);
        int x1 = x0 + kTabWidth;
        int y0 = current ? kTabTop : kTabTop + 2;
        int y1 = current ? kPanelTop + 2 : kPanelTop;
        unsigned int fill = current ? kPanelFill : kTabFill;
        painter.Fill(x0 + 2, y0, x1 - 2, y0 + 1, kBlack);
        painter.Fill(x0 + 1, y0 + 1, x0 + 2, y0 + 2, kBlack);
        painter.Fill(x1 - 2, y0 + 1, x1 - 1, y0 + 2, kBlack);
        painter.Fill(x0, y0 + 2, x0 + 1, y1, kBlack);
        painter.Fill(x1 - 1, y0 + 2, x1, y1, kBlack);
        painter.Fill(x0 + 2, y0 + 1, x1 - 2, y1, fill);
        painter.Fill(x0 + 1, y0 + 2, x1 - 1, y1, fill);
        painter.Fill(x0 + 2, y0 + 1, x1 - 2, y0 + 2, current ? kPanelLight : kTabLight);
        painter.Fill(x0 + 1, y0 + 2, x0 + 2, y1, current ? kPanelLight : kTabLight);
        painter.Fill(x1 - 2, y0 + 2, x1 - 1, y1, current ? kPanelFill : kPanelShade);
        if (current && tab == 0)
            painter.Fill(x0, y1 - 2, x0 + 1, y1 + 2, kBlack);
        bool focused = Focused(eTarget_Tab, tab);
        unsigned int colour = !TabEnabled(tab) ? kTabTextDisabled : focused ? kTabTextFocus : kTextDark;
        painter.Centred(painter.Trim(L10N::GetString(kLabels[tab]), kTabWidth - 8), (x0 + x1) / 2, y0 + 6, colour,
                        false);
        if (focused)
            painter.Ring(x0, y0, x1, y1, m_pulse);
    }
}

void UIScene_ShaderMenu::DrawButtons(MenuPainter& painter, bool modal)
{
    for (const Target& target : m_targets)
    {
        if ((target.kind == eTarget_Modal) != modal)
            continue;
        std::wstring label = ButtonLabel(target.kind, target.index);
        if (label.empty())
            continue;
        const MenuRect& rect = target.rect;
        bool focused = Focused(target.kind, target.index);
        painter.Button(rect.x0, rect.y0, rect.x1, rect.y1, focused, target.enabled);
        unsigned int colour = !target.enabled ? kTextDisabled : focused ? kTextYellow : kTextLight;
        painter.Centred(painter.Trim(label, rect.x1 - rect.x0 - 6), (rect.x0 + rect.x1) / 2,
                        rect.y0 + (rect.y1 - rect.y0 - 7) / 2, colour, true);
        if (target.kind == eTarget_Apply && target.enabled && !focused)
            painter.Fill(rect.x0 + 2, rect.y0 + 2, rect.x0 + 4, rect.y1 - 2, kTextModified);
        if (focused)
            painter.Ring(rect.x0, rect.y0, rect.x1, rect.y1, m_pulse);
    }
}

void UIScene_ShaderMenu::DrawToggleRow(MenuPainter& painter, const MenuRect& rect, bool focused)
{
    bool on = ShadersOn(*m_working);
    bool modified = on != ShadersOn(*m_applied);
    painter.Text(L"Shaders", rect.x0 + 6, rect.y0 + 6, focused ? kTextYellow : modified ? kTextModified : kTextLight,
                 true);
    if (modified)
        painter.Fill(rect.x0 + 1, rect.y0 + 1, rect.x0 + 3, rect.y1 - 1, kTextModified);
    int left = rect.x1 - 4 - kSwitchWidth * 2 - 1;
    painter.SwitchSegment(left, rect.y0 + 4, left + kSwitchWidth, rect.y1 - 4, L"ON", on, kChipGreen, kChipGreenEdge);
    left += kSwitchWidth + 1;
    painter.SwitchSegment(left, rect.y0 + 4, left + kSwitchWidth, rect.y1 - 4, L"OFF", !on, kChipRed, kChipRedEdge);
}

void UIScene_ShaderMenu::DrawPackRow(MenuPainter& painter, int index, const MenuRect& rect, bool focused)
{
    std::string name = PackName(index);
    bool usable = PackUsable(index);
    bool applied = name == m_applied->shaderPack && m_applied->enableShaders;
    bool selected = name == m_working->shaderPack && m_working->enableShaders;
    bool dimmed = !ShadersOn(*m_working) && !focused;
    if (index == kPackInternal)
        GrassIcon(painter, rect.x0 + 3, rect.y0 + 3, !dimmed);
    else
        PackIcon(painter, rect.x0 + 3, rect.y0 + 3, usable && !dimmed);

    int chipRight = rect.x1 - 4;
    int chipLeft = chipRight;
    int chipTop = rect.y0 + 4;
    if (applied && !PackChangePending())
    {
        if (index != kPackInternal && ShaderPackManager::Get().ActivePack() == nullptr)
            chipLeft = painter.Chip(chipRight, chipTop, L"Failed", kChipRed, kChipRedEdge);
        else
            chipLeft = painter.Chip(chipRight, chipTop, L"In use", kChipGreen, kChipGreenEdge);
    }
    else if (selected)
    {
        chipLeft = painter.Chip(chipRight, chipTop, L"Selected", kChipAmber, kChipAmberEdge);
    }
    else if (applied)
    {
        chipLeft = painter.Chip(chipRight, chipTop, L"In use", kChipGrey, kChipGreyEdge);
    }
    else if (!usable)
    {
        chipLeft = painter.Chip(chipRight, chipTop, L"Incompatible", kChipRed, kChipRedEdge);
    }
    else if (name == m_applied->shaderPack && !m_applied->enableShaders)
    {
        chipLeft = painter.Chip(chipRight, chipTop, L"Off", kChipGrey, kChipGreyEdge);
    }
    std::wstring label = index == kPackInternal ? std::wstring(L"(internal)") : PackDisplayName(name);
    unsigned int colour = focused ? kTextYellow : !usable || dimmed ? kTextDisabled : kTextLight;
    painter.Text(painter.Trim(label, chipLeft - (rect.x0 + 22) - 4), rect.x0 + 22, rect.y0 + 6, colour, true);
}

void UIScene_ShaderMenu::DrawPacks(MenuPainter& painter)
{
    painter.Panel(kPanelLeft, kPanelTop, kSplitRight, m_frame.bottom);
    MenuRect list = ViewRect(eView_List);
    m_frame.Clip(list.x0 - 6, list.y0, list.x1, list.y1);
    for (const Target& target : m_targets)
    {
        if (target.view != eView_List)
            continue;
        MenuRect rect = ScreenRect(target);
        if (rect.y1 < list.y0 - 4 || rect.y0 > list.y1 + 4)
            continue;
        bool focused = Focused(target.kind, target.index);
        painter.Button(rect.x0, rect.y0, rect.x1, rect.y1, focused, true);
        if (target.kind == eTarget_Toggle)
            DrawToggleRow(painter, rect, focused);
        else
            DrawPackRow(painter, target.index, rect, focused);
        if (focused)
            painter.Ring(rect.x0, rect.y0, rect.x1, rect.y1, m_pulse);
    }
    if (m_packs.empty())
    {
        int y = list.y0 + 2 * kRowPitch + 8 - static_cast<int>(m_scroll[eView_List].position + 0.5f);
        painter.Centred(L"No shader packs yet", (kListLeft + kListRowRight) / 2, y, kTextHint, false);
    }
    m_frame.Unclip();
    painter.Scrollbar(kListRight - 5, list.y0, list.y1, ContentHeight(eView_List), m_scroll[eView_List].position,
                      kScrollTrack, kScrollThumb);
    painter.Centred(L"Drop shader packs on this window to add them", (kListLeft + kListRowRight) / 2,
                    ButtonsTop() - 16, kTextHint, false);

    painter.Description(kDetailLeft, kPanelTop, kPanelRight, m_frame.bottom);
    DrawDetails(painter);
}

void UIScene_ShaderMenu::DrawCell(MenuPainter& painter, int index, const MenuRect& rect, bool focused)
{
    const Cell& cell = m_cells[static_cast<size_t>(index)];
    const ShaderPackLang& lang = ShaderPackManager::Get().Lang();
    const ShaderOption* option = cell.kind == eCell_Option ? m_options->Find(cell.name) : nullptr;
    bool enabled = cell.kind != eCell_Option || (option != nullptr && !option->ambiguous);
    painter.Button(rect.x0, rect.y0, rect.x1, rect.y1, focused, enabled);
    if (cell.kind == eCell_Link)
    {
        unsigned int colour = focused ? kTextYellow : kTextLight;
        painter.Centred(painter.Trim(Wide(lang.ScreenLabel(cell.name)), rect.x1 - rect.x0 - 24),
                        (rect.x0 + rect.x1) / 2, rect.y0 + 6, colour, true);
        painter.Text(L">", rect.x1 - 10, rect.y0 + 6, colour, true);
    }
    else
    {
        unsigned int valueColour = kTextLight;
        std::wstring value = CellValue(cell, &valueColour);
        bool slider = IsSlider(cell);
        int boxX0 = 0;
        int boxX1 = 0;
        CellValueRect(index, &boxX0, &boxX1);
        painter.Box(boxX0, rect.y0 + 2, boxX1, rect.y1 - 2, focused);
        int count = 0;
        int position = slider ? SliderPosition(cell, &count) : 0;
        if (focused && enabled && option != nullptr && g_pInput->MenuQuickMoveHeld(m_playerId)
            && m_options->Value(cell.name) != option->defaultValue)
        {
            value = Wide(ShaderPackLang::StripFormatting(lang.ValueLabel(*option, option->defaultValue)));
            valueColour = kTextOn;
            std::vector<std::string>::const_iterator found =
                std::find(option->values.begin(), option->values.end(), option->defaultValue);
            if (found != option->values.end())
                position = static_cast<int>(found - option->values.begin());
        }
        if (slider)
            painter.Slider(boxX0, boxX1, rect.y1, position, count, focused, valueColour);
        painter.Highlight(boxX0 + 1, rect.y0 + 3, boxX1 - 1, rect.y1 - 3, FlashAmount(eTarget_Cell, index));
        painter.Centred(painter.Trim(value, boxX1 - boxX0 - 6), (boxX0 + boxX1) / 2, slider ? rect.y0 + 4 : rect.y0 + 6,
                        enabled ? valueColour : kTextDisabled, true);

        std::wstring label = cell.kind == eCell_Profile ? std::wstring(L"Profile") : Wide(lang.OptionLabel(cell.name));
        bool modified = cell.kind == eCell_Option && OptionModified(cell.name);
        unsigned int colour = !enabled ? kTextDisabled : focused ? kTextYellow : modified ? kTextModified : kTextLight;
        if (modified)
            painter.Fill(rect.x0 + 1, rect.y0 + 1, rect.x0 + 3, rect.y1 - 1, kTextModified);
        painter.Text(painter.Trim(label, boxX0 - rect.x0 - 10), rect.x0 + 6, rect.y0 + 6, colour, true);
    }
    if (focused)
        painter.Ring(rect.x0, rect.y0, rect.x1, rect.y1, m_pulse);
}

void UIScene_ShaderMenu::DrawOptions(MenuPainter& painter)
{
    painter.Panel(kPanelLeft, kPanelTop, kPanelRight, m_frame.bottom);
    int middle = (kPanelTop + m_frame.bottom) / 2;
    if (PackChangePending())
    {
        painter.Centred(L"Apply to load " + PackDisplayName(m_working->shaderPack), kLayoutWidth / 2, middle - 10,
                        kTextDark, false);
        painter.Centred(L"Its settings show here once it has loaded.", kLayoutWidth / 2, middle + 4, kTextHint, false);
        return;
    }
    if (m_optionsPack.empty() || ShaderPackManager::Get().ActivePack() == nullptr)
    {
        painter.Centred(L"No shader pack is loaded", kLayoutWidth / 2, middle - 10, kTextDark, false);
        painter.Centred(L"Choose one on the Shader Packs tab and apply it.", kLayoutWidth / 2, middle + 4, kTextHint,
                        false);
        return;
    }

    int titleLeft = kGridLeft + 4;
    int titleRight = kGridRight - 3 * (kHeaderButtonWidth + 4) - 4;
    int back = FindTarget(Focus{eTarget_Back, 0});
    if (back >= 0)
        titleLeft = m_targets[static_cast<size_t>(back)].rect.x1 + 6;
    painter.Centred(painter.TrimStart(Breadcrumb(), titleRight - titleLeft), (titleLeft + titleRight) / 2,
                    kHeaderTop + 5, kTextDark, false);
    painter.Fill(kGridLeft, kHeaderBottom + 3, kGridRight, kHeaderBottom + 4, kPanelShade);
    painter.Fill(kGridLeft, kHeaderBottom + 4, kGridRight, kHeaderBottom + 5, kPanelLight);

    MenuRect grid = ViewRect(eView_Grid);
    m_frame.Clip(grid.x0 - 6, grid.y0, grid.x1 + 6, grid.y1);
    for (const Target& target : m_targets)
    {
        if (target.view != eView_Grid)
            continue;
        MenuRect rect = ScreenRect(target);
        if (rect.y1 >= grid.y0 - 4 && rect.y0 <= grid.y1 + 4)
            DrawCell(painter, target.index, rect, Focused(target.kind, target.index));
    }
    if (m_cells.empty())
        painter.Centred(L"This page has no options", kLayoutWidth / 2, grid.y0 + 20, kTextHint, false);
    m_frame.Unclip();
    painter.Scrollbar(kGridRight - 5, grid.y0, grid.y1, ContentHeight(eView_Grid), m_scroll[eView_Grid].position,
                      kScrollTrack, kScrollThumb);

    painter.Description(kGridLeft, CommentTop(), kGridRight, m_frame.bottom - 6);
    DrawDetails(painter);
}

void UIScene_ShaderMenu::DrawSettings(MenuPainter& painter)
{
    painter.Panel(kPanelLeft, kPanelTop, kSplitRight, m_frame.bottom);
    MenuRect view = ViewRect(eView_Settings);
    m_frame.Clip(view.x0 - 6, view.y0, view.x1, view.y1);
    for (const Target& target : m_targets)
    {
        if (target.view != eView_Settings)
            continue;
        MenuRect rect = ScreenRect(target);
        bool focused = Focused(target.kind, target.index);
        int setting = target.index;
        painter.Button(rect.x0, rect.y0, rect.x1, rect.y1, focused, true);
        int boxX1 = rect.x1 - 2;
        int boxX0 = boxX1 - kSettingValueWidth;
        painter.Box(boxX0, rect.y0 + 2, boxX1, rect.y1 - 2, focused);
        int index = m_working->GetChoice(setting);
        unsigned int valueColour = SettingColour(setting, index);
        int defaultIndex = ShaderSettings().GetChoice(setting);
        if (focused && g_pInput->MenuQuickMoveHeld(m_playerId) && index != defaultIndex)
        {
            index = defaultIndex;
            valueColour = kTextOn;
        }
        const ShaderSettings::ChoiceInfo& info = ShaderSettings::Describe(setting);
        if (info.slider)
            painter.Slider(boxX0, boxX1, rect.y1, index, info.count, focused, valueColour);
        painter.Highlight(boxX0 + 1, rect.y0 + 3, boxX1 - 1, rect.y1 - 3, FlashAmount(eTarget_Setting, setting));
        painter.Centred(SettingLabel(setting, index), (boxX0 + boxX1) / 2, info.slider ? rect.y0 + 4 : rect.y0 + 6,
                        valueColour, true);
        bool modified = SettingModified(setting);
        if (modified)
            painter.Fill(rect.x0 + 1, rect.y0 + 1, rect.x0 + 3, rect.y1 - 1, kTextModified);
        painter.Text(Wide(info.name), rect.x0 + 6, rect.y0 + 6,
                     focused ? kTextYellow : modified ? kTextModified : kTextLight, true);
        if (focused)
            painter.Ring(rect.x0, rect.y0, rect.x1, rect.y1, m_pulse);
    }
    m_frame.Unclip();
    painter.Scrollbar(kListRight - 5, view.y0, view.y1, ContentHeight(eView_Settings),
                      m_scroll[eView_Settings].position, kScrollTrack, kScrollThumb);
    painter.Centred(L"These settings apply to every shader pack", (kListLeft + kListRowRight) / 2,
                    SettingsBottom() + 10, kTextHint, false);

    painter.Description(kDetailLeft, kPanelTop, kPanelRight, m_frame.bottom);
    DrawDetails(painter);
}

void UIScene_ShaderMenu::DrawDetails(MenuPainter& painter)
{
    MenuRect area = ViewRect(eView_Details);
    m_frame.Clip(area.x0, area.y0, area.x1 + 2, area.y1);
    MenuRect text = Rect(area.x0, area.y0, area.x1 - 4, area.y1);
    m_detailHeight = DrawParagraphs(painter, DescribeFocus(), text,
                                    static_cast<int>(m_scroll[eView_Details].position + 0.5f));
    m_frame.Unclip();
    painter.Scrollbar(area.x1, area.y0, area.y1, m_detailHeight, m_scroll[eView_Details].position, kDetailTrack,
                      kDetailThumb);
}

void UIScene_ShaderMenu::DrawModal(MenuPainter& painter)
{
    if (m_modal == eModal_None)
        return;
    MenuRect area = m_frame.Area();
    painter.Fill(area.x0, area.y0, area.x1, area.y1, kModalShade);
    MenuRect modal = ModalRect();
    painter.Panel(modal.x0, modal.y0, modal.x1, modal.y1);
    std::wstring title;
    std::wstring message;
    if (m_modal == eModal_Reset)
    {
        title = L"Reset Every Option?";
        message = L"Every option of this shader pack goes back to its default. Cancel at the top of the menu still "
                  L"undoes it until you apply.";
    }
    else if (m_modal == eModal_Discard)
    {
        title = L"Discard Changes?";
        message = L"Some changes have not been applied. Close the menu without them?";
    }
    else
    {
        title = L"Download Shaders";
        message = L"Open modrinth.com/shaders in your web browser? Drag the shader packs you download onto this "
                  L"window to add them.";
    }
    painter.Centred(title, kLayoutWidth / 2, modal.y0 + 10, kTextDark, false);
    std::vector<std::wstring> lines = painter.Wrap(message, kModalWidth - 32);
    int y = modal.y0 + 30;
    for (const std::wstring& line : lines)
    {
        painter.Centred(line, kLayoutWidth / 2, y, kTextModal, false);
        y += kLineHeight;
    }
    DrawButtons(painter, true);
}

void UIScene_ShaderMenu::render(int, int, C4JRender::eViewportType viewport)
{
    Minecraft* minecraft = Minecraft::GetInstance();
    if (!m_hasTicked || minecraft == NULL || minecraft->mFont == NULL)
        return;
    int bottom = m_frame.bottom;
    int height = m_frame.height;
    m_frame.Fit(viewport);
    if (bottom != m_frame.bottom || height != m_frame.height)
        BuildTargets();
    Animate();
    if (m_hidden)
        return;

    MenuPainter painter(m_gui, *minecraft->mFont, m_frame);
    m_frame.Begin();
    DrawTitle(painter);
    DrawTabs(painter, false);
    if (m_tab == eTab_Packs)
        DrawPacks(painter);
    else if (m_tab == eTab_Options)
        DrawOptions(painter);
    else
        DrawSettings(painter);
    DrawTabs(painter, true);
    DrawButtons(painter, false);
    DrawModal(painter);
    m_frame.End();
}
