#include "platform/pc/PcShaderMenu.hpp"

#include <SDL3/SDL.h>
#include <stb_image.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "game/client/audio/SoundEvents.hpp"
#include "game/client/render/TexturePack.hpp"
#include "game/client/render/TexturePackRepository.hpp"
#include "game/core/ConsoleUIController.hpp"
#include "game/core/ExtraStrings.hpp"
#include "game/core/L10N.hpp"
#include "game/core/Minecraft.hpp"
#include "game/core/StringConversion.hpp"
#include "game/core/StringIds.hpp"
#include "game/ui/FJ_LaunchMoreOptionsMenu.hpp"
#include "game/ui/UICustomMenuPage.hpp"
#include "game/ui/UILayer.hpp"
#include "game/ui/UIScene_CustomMenu.hpp"
#include "game/ui/UIString.hpp"
#include "game/ui/fuiFile.hpp"
#include "game/ui/fuiRenderNode.hpp"
#include "platform/pc/Renderer.hpp"
#include "platform/pc/shaderpack/ShaderPack.hpp"
#include "platform/pc/shaderpack/ShaderPackManager.hpp"
#include "platform/pc/shaderpack/ShaderPipeline.hpp"

extern ConsoleUIController gConsoleUIController;

namespace
{
enum
{
    eTab_Packs,
    eTab_Settings
};

enum
{
    ePack_Off = 0x100,
    ePack_Internal,
    ePack_OpenFolder,
    ePack_First
};

enum
{
    eSetting_Antialiasing = 0x100,
    eSetting_NormalMap,
    eSetting_SpecularMap,
    eSetting_RenderQuality,
    eSetting_ShadowQuality,
    eSetting_HandDepth,
    eSetting_OldHandLight,
    eSetting_OldLighting
};

const wchar_t* const kTitleColour = L"#ffff55";
const wchar_t* const kHeadingColour = L"#ffffff";
const wchar_t* const kBodyColour = L"#c8c8c8";
const wchar_t* const kNoteColour = L"#a0a0a0";
const wchar_t* const kActiveColour = L"#55ff55";
const wchar_t* const kPendingColour = L"#ffaa00";
const wchar_t* const kErrorColour = L"#ff5555";

const wchar_t* const kIconColourName = L"shaderPackIcon";
const wchar_t* const kIconGreyName = L"shaderPackIconGrey";

const int kAntialiasingLevels[3] = {0, 2, 4};
const wchar_t* const kAntialiasingLabels[3] = {L"OFF", L"FXAA 2x", L"FXAA 4x"};
const wchar_t* const kQualityLabels[5] = {L"0.5x", L"0.7x", L"1x", L"1.4x", L"2x"};
const wchar_t* const kHandDepthLabels[3] = {L"0.5x", L"1x", L"2x"};
const wchar_t* const kTristateLabels[3] = {L"Default", L"ON", L"OFF"};
const ShaderSettings::Tristate kTristates[3] = {ShaderSettings::Default, ShaderSettings::On, ShaderSettings::Off};

std::wstring Wide(const std::string& text)
{
    return convStringToWstring(text);
}

std::wstring Plain(const std::string& text)
{
    return Wide(ShaderPackLang::StripFormatting(text));
}

std::wstring Escape(const std::wstring& text)
{
    std::wstring out;
    out.reserve(text.size());
    for (wchar_t character : text)
    {
        switch (character)
        {
        case L'&':
            out += L"&amp;";
            break;
        case L'<':
            out += L"&lt;";
            break;
        case L'>':
            out += L"&gt;";
            break;
        case L'\n':
            out += L"<br>";
            break;
        default:
            out += character;
            break;
        }
    }
    return out;
}

std::wstring Coloured(const wchar_t* colour, const std::wstring& html)
{
    return L"<font color=\"" + std::wstring(colour) + L"\">" + html + L"</font>";
}

std::wstring Title(const std::wstring& text)
{
    return Coloured(kTitleColour, Escape(text));
}

std::wstring Paragraph(const std::wstring& text)
{
    return L"<br>" + Coloured(kBodyColour, Escape(text));
}

std::wstring Section(const std::wstring& heading, const std::wstring& body)
{
    return L"<br><br>" + Coloured(kHeadingColour, Escape(heading)) + L"<br>" + Coloured(kBodyColour, Escape(body));
}

std::wstring Note(const std::wstring& text)
{
    return L"<br><br>" + Coloured(kNoteColour, Escape(text));
}

std::wstring Badge(const wchar_t* colour, const std::wstring& text)
{
    return L"<br>" + Coloured(colour, Escape(text));
}

std::wstring Join(const std::vector<std::wstring>& items)
{
    std::wstring out;
    for (size_t index = 0; index < items.size(); ++index)
    {
        if (index > 0)
            out += L", ";
        out += items[index];
    }
    return out;
}

int NearestIndex(const float* values, int count, float value)
{
    int best = 0;
    for (int index = 1; index < count; ++index)
    {
        if (std::fabs(values[index] - value) < std::fabs(values[best] - value))
            best = index;
    }
    return best;
}

int TristateIndex(ShaderSettings::Tristate value)
{
    for (int index = 0; index < 3; ++index)
    {
        if (kTristates[index] == value)
            return index;
    }
    return 0;
}

unsigned char* CopyBytes(const unsigned char* data, size_t size)
{
    unsigned char* copy = new unsigned char[size];
    std::memcpy(copy, data, size);
    return copy;
}

std::vector<unsigned char> GreyPng(const unsigned char* png, unsigned int size)
{
    int width = 0;
    int height = 0;
    int components = 0;
    unsigned char* pixels = stbi_load_from_memory(png, static_cast<int>(size), &width, &height, &components, 4);
    if (pixels == nullptr)
        return std::vector<unsigned char>();
    for (int index = 0; index < width * height; ++index)
    {
        unsigned char* pixel = pixels + index * 4;
        unsigned char grey = static_cast<unsigned char>((pixel[0] * 77 + pixel[1] * 150 + pixel[2] * 29) >> 8);
        pixel[0] = grey;
        pixel[1] = grey;
        pixel[2] = grey;
    }
    std::vector<unsigned char> encoded(static_cast<size_t>(width) * height * 5 + 0x400);
    int written = 0;
    gRenderer->SaveTextureDataToMemory(encoded.data(), static_cast<int>(encoded.size()), &written, width, height,
                                       reinterpret_cast<int*>(pixels));
    stbi_image_free(pixels);
    encoded.resize(written > 0 && written < static_cast<int>(encoded.size()) ? static_cast<size_t>(written) : 0);
    return encoded;
}

std::wstring PackRow(const std::wstring& name, const std::wstring& status)
{
    return Escape(name) + L"<br>" + Escape(status);
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
    std::wstring problem;
    std::vector<std::wstring> profiles;
    int mainPages = 0;
    int totalPages = 0;
    std::vector<std::wstring> worlds;
};

std::map<std::string, PackSummary> sSummaries;

void SummariseWorlds(const ShaderPack& pack, PackSummary& summary)
{
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
}

PackSummary Summarise(const std::string& name)
{
    PackSummary summary;
    ShaderPackManager& manager = ShaderPackManager::Get();
    ShaderPack* active = manager.ActivePack();
    if (active != nullptr && active->Name() == name)
    {
        const ShaderPackProperties& properties = manager.Properties();
        for (const ShaderProfile& profile : properties.Profiles())
            summary.profiles.push_back(Plain(manager.Lang().ProfileLabel(profile.name)));
        summary.totalPages = static_cast<int>(properties.Screens().size());
        if (const ShaderScreen* root = properties.Screen(std::string()))
        {
            for (const std::string& item : root->items)
            {
                if (item.size() > 2 && item.front() == '[' && item.back() == ']')
                    ++summary.mainPages;
            }
        }
        summary.usable = !active->ProgramFiles().empty();
        SummariseWorlds(*active, summary);
        if (!summary.usable)
            summary.problem = L"Contains no shader programs";
        return summary;
    }

    std::string error;
    std::unique_ptr<ShaderPack> pack = ShaderPack::Open(manager.PacksFolder() / std::filesystem::u8path(name), error);
    if (!pack)
    {
        summary.problem = error.empty() ? std::wstring(L"Could not be opened") : Wide(error);
        return summary;
    }

    std::string text;
    std::set<std::string> screens;
    if (pack->ReadFile("shaders.properties", text))
    {
        std::set<std::string> profiles;
        for (const auto& entry : ShaderParseProperties(text))
        {
            const std::string& key = entry.first;
            if (key.compare(0, 8, "profile.") == 0 && profiles.insert(key.substr(8)).second)
                summary.profiles.push_back(Wide(key.substr(8)));
            else if (key == "screen" || (key.compare(0, 7, "screen.") == 0 && key.find('.', 7) == std::string::npos))
            {
                screens.insert(key);
                if (key == "screen")
                {
                    summary.mainPages = 0;
                    for (const std::string& item : ShaderSplitList(entry.second))
                    {
                        if (item.size() > 2 && item.front() == '[' && item.back() == ']')
                            ++summary.mainPages;
                    }
                }
            }
        }
    }
    summary.totalPages = static_cast<int>(screens.size());
    summary.usable = !pack->ProgramFiles().empty();
    SummariseWorlds(*pack, summary);
    if (!summary.usable)
        summary.problem = L"Contains no shader programs";
    return summary;
}

const PackSummary& SummaryFor(const std::string& name)
{
    std::map<std::string, PackSummary>::iterator found = sSummaries.find(name);
    if (found == sSummaries.end())
        found = sSummaries.insert(std::make_pair(name, Summarise(name))).first;
    return found->second;
}

std::wstring PackDisplayName(const std::string& name)
{
    std::string display = name;
    if (display.size() > 4 && display.compare(display.size() - 4, 4, ".zip") == 0)
        display.resize(display.size() - 4);
    return Wide(display);
}

void ReloadShaders()
{
    ShaderPipeline::Get().RequestReload();
}

class ShaderOptionsPage : public UICustomMenuPage
{
public:
    ShaderOptionsPage(const std::string& screen, const std::wstring& path) : m_screen(screen), m_path(path) {}

    virtual void Build(UIScene_CustomMenu& menu);
    virtual void OnPress(UIScene_CustomMenu& menu, int id);
    virtual void OnToggle(UIScene_CustomMenu& menu, int id, bool checked);
    virtual void OnSlide(UIScene_CustomMenu& menu, int id, int value);
    virtual void OnClose(UIScene_CustomMenu& menu);
    virtual void OnGainFocus(UIScene_CustomMenu& menu);
    virtual bool OnAction(UIScene_CustomMenu& menu, int action);
    virtual bool KeepDescriptionOpen() const { return true; }
    virtual bool GetTooltips(int& tooltipX, int& tooltipY) const;
    virtual bool ShowLogo() const { return false; }

private:
    enum EntryKind
    {
        eEntry_Option,
        eEntry_Screen,
        eEntry_Profile
    };

    struct Entry
    {
        EntryKind kind;
        std::string name;
    };

    static const int kFirstId = 0x200;

    void CollectEntries();
    std::wstring Describe(const Entry& entry) const;
    std::wstring ValueText(const ShaderOption& option, const std::string& value) const;
    std::wstring ProfileText() const;
    std::vector<std::string> ProfileNames() const;
    void RefreshDynamic(UIScene_CustomMenu& menu);
    void Apply();

    static bool sPending;

    std::string m_screen;
    std::wstring m_path;
    std::vector<Entry> m_entries;
};

bool ShaderOptionsPage::sPending = false;

void ShaderOptionsPage::CollectEntries()
{
    ShaderPackManager& manager = ShaderPackManager::Get();
    const ShaderPackProperties& properties = manager.Properties();
    const ShaderPackOptions& options = manager.Options();
    m_entries.clear();

    std::vector<std::string> items;
    const ShaderScreen* screen = properties.Screen(m_screen);
    if (screen != nullptr)
        items = screen->items;
    else if (m_screen.empty() && properties.Screens().empty())
        items.push_back("*");

    std::set<std::string> listed;
    for (const ShaderScreen& other : properties.Screens())
    {
        for (const std::string& item : other.items)
            listed.insert(item);
    }

    for (const std::string& item : items)
    {
        if (item == "<empty>" || item.empty())
            continue;
        if (item == "<profile>")
        {
            if (!properties.Profiles().empty())
                m_entries.push_back(Entry{eEntry_Profile, std::string()});
            continue;
        }
        if (item.size() > 2 && item.front() == '[' && item.back() == ']')
        {
            std::string name = item.substr(1, item.size() - 2);
            if (properties.Screen(name) != nullptr)
                m_entries.push_back(Entry{eEntry_Screen, name});
            continue;
        }
        if (item == "*")
        {
            for (const ShaderOption& option : options.Options())
            {
                if (listed.count(option.name) == 0 && (option.IsBoolean() || option.values.size() > 1))
                    m_entries.push_back(Entry{eEntry_Option, option.name});
            }
            continue;
        }
        const ShaderOption* option = options.Find(item);
        if (option != nullptr && (option->IsBoolean() || option->values.size() > 1))
            m_entries.push_back(Entry{eEntry_Option, item});
    }
}

std::vector<std::string> ShaderOptionsPage::ProfileNames() const
{
    std::vector<std::string> names;
    for (const ShaderProfile& profile : ShaderPackManager::Get().Properties().Profiles())
        names.push_back(profile.name);
    return names;
}

std::wstring ShaderOptionsPage::ProfileText() const
{
    ShaderPackManager& manager = ShaderPackManager::Get();
    std::string active = manager.Properties().ActiveProfile(manager.Options());
    return active.empty() ? std::wstring(L"Custom") : Plain(manager.Lang().ProfileLabel(active));
}

std::wstring ShaderOptionsPage::ValueText(const ShaderOption& option, const std::string& value) const
{
    return Plain(ShaderPackManager::Get().Lang().ValueLabel(option, value));
}

std::wstring ShaderOptionsPage::Describe(const Entry& entry) const
{
    ShaderPackManager& manager = ShaderPackManager::Get();
    const ShaderPackLang& lang = manager.Lang();
    std::wstring html;
    if (entry.kind == eEntry_Screen)
    {
        html = Title(Plain(lang.ScreenLabel(entry.name)));
        std::wstring comment = Plain(lang.ScreenComment(entry.name));
        if (!comment.empty())
            html += Paragraph(comment);
    }
    else if (entry.kind == eEntry_Profile)
    {
        html = Title(L"Profile") + Paragraph(L"Sets many options at once to a preset chosen by the shader pack.");
        std::vector<std::wstring> labels;
        for (const std::string& name : ProfileNames())
            labels.push_back(Plain(lang.ProfileLabel(name)));
        html += Section(L"Values", Join(labels));
        html += Note(L"Changing an option the profile controls shows Custom here.");
    }
    else
    {
        const ShaderOption* option = manager.Options().Find(entry.name);
        if (option == nullptr)
            return std::wstring();
        html = Title(Plain(lang.OptionLabel(option->name)));
        std::wstring comment = Plain(lang.OptionComment(*option));
        if (!comment.empty())
            html += Paragraph(comment);
        std::vector<std::wstring> values;
        std::vector<std::string> list = option->IsBoolean() ? std::vector<std::string>{"true", "false"}
                                                            : option->values;
        for (const std::string& value : list)
        {
            std::wstring text = ValueText(*option, value);
            if (value == option->defaultValue)
                text += L" (Default)";
            values.push_back(text);
        }
        html += Section(L"Values", Join(values));
    }
    if (sPending)
        html += Badge(kPendingColour, L"Not applied yet");
    return html;
}

void ShaderOptionsPage::Build(UIScene_CustomMenu& menu)
{
    ShaderPackManager& manager = ShaderPackManager::Get();
    CollectEntries();
    menu.AddLabel(0, m_path);
    const ShaderPackLang& lang = manager.Lang();
    const ShaderPackOptions& options = manager.Options();
    for (size_t index = 0; index < m_entries.size(); ++index)
    {
        const Entry& entry = m_entries[index];
        int id = kFirstId + static_cast<int>(index);
        if (entry.kind == eEntry_Screen)
        {
            menu.AddButton(id, Plain(lang.ScreenLabel(entry.name)), Describe(entry));
            continue;
        }
        if (entry.kind == eEntry_Profile)
        {
            std::vector<std::string> names = ProfileNames();
            std::string active = manager.Properties().ActiveProfile(options);
            int position = static_cast<int>(std::find(names.begin(), names.end(), active) - names.begin());
            menu.AddLabel(0, L"Profile");
            menu.AddSlider(id, ProfileText(), 0, static_cast<int>(names.size()), position, Describe(entry));
            continue;
        }
        const ShaderOption* option = options.Find(entry.name);
        if (option == nullptr)
            continue;
        std::string value = options.Value(option->name);
        if (option->IsBoolean())
        {
            menu.AddCheckbox(id, Plain(lang.OptionLabel(option->name)), value == "true", Describe(entry));
            continue;
        }
        int position = static_cast<int>(std::find(option->values.begin(), option->values.end(), value)
                                        - option->values.begin());
        if (position >= static_cast<int>(option->values.size()))
            position = 0;
        menu.AddLabel(0, Plain(lang.OptionLabel(option->name)));
        menu.AddSlider(id, ValueText(*option, value), 0, static_cast<int>(option->values.size()) - 1, position,
                       Describe(entry));
    }
}

void ShaderOptionsPage::RefreshDynamic(UIScene_CustomMenu& menu)
{
    ShaderPackManager& manager = ShaderPackManager::Get();
    std::vector<std::string> names = ProfileNames();
    std::string active = manager.Properties().ActiveProfile(manager.Options());
    for (size_t index = 0; index < m_entries.size(); ++index)
    {
        const Entry& entry = m_entries[index];
        int id = kFirstId + static_cast<int>(index);
        if (entry.kind == eEntry_Profile)
        {
            int position = static_cast<int>(std::find(names.begin(), names.end(), active) - names.begin());
            menu.SetSliderValue(id, position);
            menu.SetItemLabel(id, ProfileText());
        }
        menu.SetDescription(id, Describe(entry));
    }
}

void ShaderOptionsPage::OnPress(UIScene_CustomMenu& menu, int id)
{
    size_t index = static_cast<size_t>(id - kFirstId);
    if (id < kFirstId || index >= m_entries.size() || m_entries[index].kind != eEntry_Screen)
        return;
    const std::string& name = m_entries[index].name;
    menu.OpenPage(new ShaderOptionsPage(name, m_path + L" > "
                                                  + Plain(ShaderPackManager::Get().Lang().ScreenLabel(name))));
}

void ShaderOptionsPage::OnToggle(UIScene_CustomMenu& menu, int id, bool checked)
{
    size_t index = static_cast<size_t>(id - kFirstId);
    if (id < kFirstId || index >= m_entries.size() || m_entries[index].kind != eEntry_Option)
        return;
    if (ShaderPackManager::Get().SetOption(m_entries[index].name, checked ? "true" : "false"))
    {
        sPending = true;
        RefreshDynamic(menu);
    }
}

void ShaderOptionsPage::OnSlide(UIScene_CustomMenu& menu, int id, int value)
{
    size_t index = static_cast<size_t>(id - kFirstId);
    if (id < kFirstId || index >= m_entries.size())
        return;
    ShaderPackManager& manager = ShaderPackManager::Get();
    const Entry& entry = m_entries[index];
    if (entry.kind == eEntry_Profile)
    {
        std::vector<std::string> names = ProfileNames();
        if (value < 0 || value >= static_cast<int>(names.size()))
        {
            menu.SetItemLabel(id, ProfileText());
            return;
        }
        manager.ApplyProfile(names[static_cast<size_t>(value)]);
        sPending = true;
        menu.Rebuild();
        return;
    }
    const ShaderOption* option = manager.Options().Find(entry.name);
    if (option == nullptr || value < 0 || value >= static_cast<int>(option->values.size()))
        return;
    const std::string& chosen = option->values[static_cast<size_t>(value)];
    if (manager.SetOption(option->name, chosen))
        sPending = true;
    menu.SetItemLabel(id, ValueText(*option, manager.Options().Value(option->name)));
    RefreshDynamic(menu);
}

void ShaderOptionsPage::Apply()
{
    ShaderPackManager::Get().SaveOptions();
    ReloadShaders();
    sPending = false;
}

void ShaderOptionsPage::OnClose(UIScene_CustomMenu&)
{
    if (m_screen.empty() && sPending)
        Apply();
}

void ShaderOptionsPage::OnGainFocus(UIScene_CustomMenu& menu)
{
    menu.Rebuild();
}

bool ShaderOptionsPage::OnAction(UIScene_CustomMenu& menu, int action)
{
    if (action == EControllerActions_MenuX)
    {
        if (sPending)
        {
            Apply();
            gConsoleUIController.PlayUISFX(SoundEvents::UI_PRESS);
            RefreshDynamic(menu);
        }
        return true;
    }
    if (action == EControllerActions_MenuY)
    {
        ShaderPackManager::Get().ResetOptions();
        sPending = true;
        gConsoleUIController.PlayUISFX(SoundEvents::UI_PRESS);
        menu.Rebuild();
        return true;
    }
    return false;
}

bool ShaderOptionsPage::GetTooltips(int& tooltipX, int& tooltipY) const
{
    tooltipX = IDS_LCE_SHADER_APPLY;
    tooltipY = IDS_LCE_DEBUG_RESET;
    return true;
}

std::wstring SettingTitle(int id)
{
    switch (id)
    {
    case eSetting_Antialiasing:
        return L"Antialiasing";
    case eSetting_NormalMap:
        return L"Normal Map";
    case eSetting_SpecularMap:
        return L"Specular Map";
    case eSetting_RenderQuality:
        return L"Render Quality";
    case eSetting_ShadowQuality:
        return L"Shadow Quality";
    case eSetting_HandDepth:
        return L"Hand Depth";
    case eSetting_OldHandLight:
        return L"Old Hand Light";
    case eSetting_OldLighting:
        return L"Old Lighting";
    default:
        return std::wstring();
    }
}

std::wstring SettingDescription(int id)
{
    std::wstring html = Title(SettingTitle(id));
    switch (id)
    {
    case eSetting_Antialiasing:
        html += Paragraph(L"Smooths jagged edges with an FXAA pass after the shader pack's final pass.");
        html += Section(L"Values", L"OFF, FXAA 2x, FXAA 4x");
        html += Section(L"Shader packs read this as", L"MC_FXAA_LEVEL");
        break;
    case eSetting_NormalMap:
        html += Paragraph(L"Tells shader packs whether normal maps may be used. Off gives packs a flat normal map.");
        html += Section(L"Shader packs read this as", L"MC_NORMAL_MAP");
        break;
    case eSetting_SpecularMap:
        html += Paragraph(
            L"Tells shader packs whether specular maps may be used. Off gives packs an empty specular map.");
        html += Section(L"Shader packs read this as", L"MC_SPECULAR_MAP");
        break;
    case eSetting_RenderQuality:
        html += Paragraph(L"Scales the resolution the shader pack renders at. Higher is sharper at a higher GPU cost.");
        html += Section(L"Values", L"0.5x, 0.7x, 1x, 1.4x, 2x");
        html += Section(L"Shader packs read this as", L"MC_RENDER_QUALITY");
        break;
    case eSetting_ShadowQuality:
        html += Paragraph(L"Scales the shadow map size the shader pack asks for. 2x doubles it for sharper shadows at "
                          L"a higher GPU cost; 0.5x halves it.");
        html += Section(L"Values", L"0.5x, 0.7x, 1x, 1.4x, 2x");
        html += Section(L"Shader packs read this as", L"MC_SHADOW_QUALITY");
        break;
    case eSetting_HandDepth:
        html += Paragraph(L"Scales how deep the held item sits in the depth buffer, for packs with depth-based "
                          L"effects such as depth of field.");
        html += Section(L"Values", L"0.5x, 1x, 2x");
        html += Section(L"Shader packs read this as", L"MC_HAND_DEPTH");
        break;
    case eSetting_OldHandLight:
        html += Paragraph(L"ON lights the world with the brighter of the items in either hand. Default follows the "
                          L"shader pack.");
        html += Section(L"Values", L"Default, ON, OFF");
        break;
    case eSetting_OldLighting:
        html += Paragraph(L"ON keeps the fixed brightness per block face from the standard renderer. OFF leaves face "
                          L"shading to the shader pack. Default follows the shader pack.");
        html += Section(L"Values", L"Default, ON, OFF");
        break;
    default:
        break;
    }
    html += Note(L"These settings apply to every shader pack.");
    return html;
}
} // namespace

UIScene_ShaderMenu::UIScene_ShaderMenu(int playerId, void*, UILayer* layer)
    : UIScene(playerId, layer),
      m_tab(eTab_Packs)
{
    m_focus[0] = -1;
    m_focus[1] = -1;
    initialiseMovie();
    sSummaries.clear();
    RegisterIcons();
    Build();
    FocusSelectedPack();
}

UIScene_ShaderMenu::~UIScene_ShaderMenu()
{
}

std::wstring UIScene_ShaderMenu::getMoviePath()
{
    return std::wstring(L"LaunchMoreOptionsMenu");
}

unsigned int UIScene_ShaderMenu::mapElementsAndNames()
{
    fuiRenderNode* root = m_movie->getRootNode();
    m_settingsLabel.setupControl(this, root, std::string("LabelGame"));
    m_controls.push_back(&m_settingsLabel);
    m_packsLabel.setupControl(this, root, std::string("LabelWorld"));
    m_controls.push_back(&m_packsLabel);

    m_packsPanel.setupControl(this, root, std::string("WorldOptions"));
    m_controls.push_back(&m_packsPanel);
    fuiRenderNode* packs = m_packsPanel.getFuiRenderNode();
    m_packsList.setupControl(this, packs, std::string("WorldOptionsList"));
    m_controls.push_back(&m_packsList);
    m_packsTab.setupControl(this, packs, std::string("TouchTabWorld"));
    m_controls.push_back(&m_packsTab);
    m_packsDescription.setupControl(this, packs, std::string("Description_WorldOptions"));
    m_controls.push_back(&m_packsDescription);
    m_unusedSizeLabel.setupControl(this, packs, std::string("WorldSize"));
    m_controls.push_back(&m_unusedSizeLabel);
    m_unusedResizeLabel.setupControl(this, packs, std::string("ResizeLabel"));
    m_controls.push_back(&m_unusedResizeLabel);

    m_settingsPanel.setupControl(this, root, std::string("GameOptions"));
    m_controls.push_back(&m_settingsPanel);
    fuiRenderNode* settings = m_settingsPanel.getFuiRenderNode();
    m_settingsList.setupControl(this, settings, std::string("GameOptionsList"));
    m_controls.push_back(&m_settingsList);
    m_settingsTab.setupControl(this, settings, std::string("TouchTabGame"));
    m_controls.push_back(&m_settingsTab);
    m_settingsDescription.setupControl(this, settings, std::string("Description_GameOptions"));
    m_controls.push_back(&m_settingsDescription);
    return 1;
}

int UIScene_ShaderMenu::getSceneType()
{
    return EUIScene_ShaderMenu;
}

void UIScene_ShaderMenu::Build()
{
    m_packsLabel.init(UIString(L10N::GetString(IDS_LCE_SHADER_PACKS)));
    m_settingsLabel.init(UIString(L10N::GetString(IDS_LCE_SHADER_SETTINGS)));
    m_packsPanel.setId(0x32);
    m_settingsPanel.setId(0x33);
    m_unusedSizeLabel.init(UIString(L""));
    m_unusedResizeLabel.init(UIString(L""));
    m_unusedSizeLabel.setVisible(false);
    m_unusedResizeLabel.setVisible(false);
    BuildPacks();
    BuildSettings();
    FJ_LaunchMoreOptionsMenu* menu = static_cast<FJ_LaunchMoreOptionsMenu*>(m_movie->getRootNode()->getFuiNode());
    menu->SetMenuType(m_tab == eTab_Packs ? 0 : 1);
    m_packsTab.init(0x34);
    m_settingsTab.init(0x35);
    UpdateDescription();
    updateTooltips();
    gConsoleUIController.TouchBoxRebuild(this);
}

void UIScene_ShaderMenu::RegisterIcons()
{
    TexturePack* texturePack = Minecraft::GetInstance()->mTexturePackRepository->getSelected();
    unsigned int size = 0;
    const unsigned char* icon = static_cast<const unsigned char*>(texturePack->getPackIcon(size));
    if (icon == nullptr || size == 0)
        return;
    registerSubstitutionTexture(kIconColourName, CopyBytes(icon, size), size, true);
    m_iconColour = kIconColourName;
    std::vector<unsigned char> grey = GreyPng(icon, size);
    if (grey.empty())
    {
        m_iconGrey = m_iconColour;
        return;
    }
    registerSubstitutionTexture(kIconGreyName, CopyBytes(grey.data(), grey.size()),
                                static_cast<unsigned int>(grey.size()), true);
    m_iconGrey = kIconGreyName;
}

void UIScene_ShaderMenu::FocusSelectedPack()
{
    const std::string& selected = ShaderPackManager::Get().Settings().shaderPack;
    int id = ePack_Off;
    if (selected == ShaderSettings::kPackInternal)
        id = ePack_Internal;
    std::vector<std::string>::const_iterator found = std::find(m_packs.begin(), m_packs.end(), selected);
    if (found != m_packs.end())
        id = ePack_First + static_cast<int>(found - m_packs.begin());
    int index = m_packsList.getListIndex(id);
    if (index >= 0)
        m_packsList.setCurrentSelection(index, false, true);
    m_focus[eTab_Packs] = id;
    UpdateDescription();
}

void UIScene_ShaderMenu::BuildPacks()
{
    ShaderPackManager& manager = ShaderPackManager::Get();
    const std::string& selected = manager.Settings().shaderPack;
    m_packs = manager.ListPacks();
    m_packsList.clearList();

    m_packsList.AddNewIconButton(PackRow(L"OFF", selected == ShaderSettings::kPackOff ? L"Active"
                                                                                       : L"Standard renderer"),
                                 m_iconColour, ePack_Off);
    m_packsList.AddNewIconButton(PackRow(L"(internal)", selected == ShaderSettings::kPackInternal ? L"Active"
                                                                                                  : L"Built-in shaders"),
                                 m_iconColour, ePack_Internal);
    for (size_t index = 0; index < m_packs.size(); ++index)
    {
        const std::string& name = m_packs[index];
        const PackSummary& summary = SummaryFor(name);
        std::wstring state;
        if (!summary.usable)
            state = L"Shader pack incompatible!";
        else if (name == selected && manager.ActivePack() != nullptr)
            state = L"Active";
        else if (name == selected)
            state = L"Failed to load";
        else
            state = L"Shader pack";
        int id = ePack_First + static_cast<int>(index);
        m_packsList.AddNewIconButton(PackRow(Wide(name), state), m_iconGrey, id);
        if (!summary.usable)
            m_packsList.SetItemOpacity(id, 0.5f, true);
    }
    m_packsList.AddNewButton(L"Open Shader Pack Folder", ePack_OpenFolder, false);
}

void UIScene_ShaderMenu::BuildSettings()
{
    const ShaderSettings& settings = ShaderPackManager::Get().Settings();
    m_settingsList.clearList();

    int antialiasing = settings.antialiasingLevel >= 4 ? 2 : settings.antialiasingLevel >= 2 ? 1 : 0;
    m_settingsList.AddNewLabel(SettingTitle(eSetting_Antialiasing), 0);
    m_settingsList.AddNewSlider(kAntialiasingLabels[antialiasing], eSetting_Antialiasing, 0, 2, antialiasing, true, 7,
                                3);
    m_settingsList.AddNewCheckbox(SettingTitle(eSetting_NormalMap), eSetting_NormalMap, settings.normalMapEnabled);
    m_settingsList.AddNewCheckbox(SettingTitle(eSetting_SpecularMap), eSetting_SpecularMap,
                                  settings.specularMapEnabled);

    int render = NearestIndex(ShaderSettings::kQualityValues, 5, settings.renderResMul);
    m_settingsList.AddNewLabel(SettingTitle(eSetting_RenderQuality), 0);
    m_settingsList.AddNewSlider(kQualityLabels[render], eSetting_RenderQuality, 0, 4, render, true, 7, 3);

    int shadow = NearestIndex(ShaderSettings::kQualityValues, 5, settings.shadowResMul);
    m_settingsList.AddNewLabel(SettingTitle(eSetting_ShadowQuality), 0);
    m_settingsList.AddNewSlider(kQualityLabels[shadow], eSetting_ShadowQuality, 0, 4, shadow, true, 7, 3);

    int hand = NearestIndex(ShaderSettings::kHandDepthValues, 3, settings.handDepthMul);
    m_settingsList.AddNewLabel(SettingTitle(eSetting_HandDepth), 0);
    m_settingsList.AddNewSlider(kHandDepthLabels[hand], eSetting_HandDepth, 0, 2, hand, true, 7, 3);

    int handLight = TristateIndex(settings.oldHandLight);
    m_settingsList.AddNewLabel(SettingTitle(eSetting_OldHandLight), 0);
    m_settingsList.AddNewSlider(kTristateLabels[handLight], eSetting_OldHandLight, 0, 2, handLight, true, 7, 3);

    int lighting = TristateIndex(settings.oldLighting);
    m_settingsList.AddNewLabel(SettingTitle(eSetting_OldLighting), 0);
    m_settingsList.AddNewSlider(kTristateLabels[lighting], eSetting_OldLighting, 0, 2, lighting, true, 7, 3);
}

void UIScene_ShaderMenu::Layout()
{
    FJ_LaunchMoreOptionsMenu* menu = static_cast<FJ_LaunchMoreOptionsMenu*>(m_movie->getRootNode()->getFuiNode());
    menu->SetActiveTab(m_tab == eTab_Packs ? 1 : 0, true);
    gConsoleUIController.TouchBoxRebuild(this);
}

void UIScene_ShaderMenu::SetTab(int tab)
{
    if (tab == m_tab)
        return;
    gConsoleUIController.PlayUISFX(SoundEvents::UI_FOCUS);
    m_tab = tab;
    static_cast<FJ_LaunchMoreOptionsMenu*>(m_movie->getRootNode()->getFuiNode())->ChangeTab();
    UpdateDescription();
    updateTooltips();
    gConsoleUIController.TouchBoxRebuild(this);
}

void UIScene_ShaderMenu::UpdateDescription()
{
    int id = m_focus[m_tab];
    std::wstring html;
    if (m_tab == eTab_Settings)
    {
        html = SettingDescription(id >= eSetting_Antialiasing ? id : eSetting_Antialiasing);
        m_settingsDescription.setLabel(UIString(html), false, false);
        return;
    }

    ShaderPackManager& manager = ShaderPackManager::Get();
    const std::string& selected = manager.Settings().shaderPack;
    if (id == ePack_Off || id < 0)
    {
        html = Title(L"OFF") + (selected == ShaderSettings::kPackOff ? Badge(kActiveColour, L"Active") : L"")
               + Paragraph(L"Renders the world with the standard renderer.");
    }
    else if (id == ePack_Internal)
    {
        html = Title(L"(internal)") + (selected == ShaderSettings::kPackInternal ? Badge(kActiveColour, L"Active") : L"")
               + Paragraph(L"Runs the shader pipeline with built-in programs that match the standard look. Useful "
                           L"for checking that the pipeline itself works.");
    }
    else if (id == ePack_OpenFolder)
    {
        html = Title(L"Open Shader Pack Folder")
               + Paragraph(L"Opens " + Wide(manager.PacksFolder().u8string())
                           + L". Put shader pack .zip files or folders here, then press Y to refresh.");
    }
    else if (id >= ePack_First && id - ePack_First < static_cast<int>(m_packs.size()))
    {
        const std::string& name = m_packs[static_cast<size_t>(id - ePack_First)];
        const PackSummary& summary = SummaryFor(name);
        html = Title(Wide(name));
        if (!summary.usable)
            html += Badge(kErrorColour, L"Shader pack incompatible: " + summary.problem);
        else if (name == selected && manager.ActivePack() != nullptr)
            html += Badge(kActiveColour, L"Active");
        else if (name == selected)
            html += Badge(kErrorColour, L"Failed to load");
        if (!summary.profiles.empty())
            html += Section(L"Profiles", Join(summary.profiles));
        if (summary.totalPages > 0)
            html += Section(L"Settings", std::to_wstring(summary.mainPages) + L" main pages, "
                                             + std::to_wstring(summary.totalPages) + L" pages in total");
        if (!summary.worlds.empty())
            html += Section(L"Worlds", Join(summary.worlds));
    }
    m_packsDescription.setLabel(UIString(html), false, false);
}

void UIScene_ShaderMenu::updateTooltips()
{
    if (m_tab == eTab_Packs)
    {
        int packSettings = ShaderPackManager::Get().ActivePack() != nullptr ? static_cast<int>(IDS_LCE_SHADER_PACK_SETTINGS)
                                                                             : -1;
        gConsoleUIController.SetTooltips(m_playerId, IDS_TOOLTIPS_SELECT, IDS_TOOLTIPS_BACK, packSettings,
                                         IDS_LCE_SHADER_REFRESH, -1, -1, -2, IDS_LCE_SHADER_SETTINGS, -1, -1, -1, -1,
                                         -1, -1, -1, -1, false, false);
        return;
    }
    gConsoleUIController.SetTooltips(m_playerId, IDS_TOOLTIPS_SELECT, IDS_TOOLTIPS_BACK, -1,
                                     IDS_LCE_SHADER_RESET_DEFAULTS, -1, -1, -2, IDS_LCE_SHADER_PACKS, -1, -1, -1, -1,
                                     -1, -1, -1, -1, false, false);
}

void UIScene_ShaderMenu::updateComponents()
{
    bool inWorld = Minecraft::GetInstance()->mLevel != NULL;
    m_layer->showComponent(m_playerId, EUIScene_Panorama, !inWorld);
    m_layer->showComponent(m_playerId, EUIScene_Logo, false);
}

void UIScene_ShaderMenu::handleReload()
{
    int tab = m_tab;
    Build();
    if (tab != m_tab)
        SetTab(tab);
}

void UIScene_ShaderMenu::handleGainFocus(bool hadFocus)
{
    UIScene::handleGainFocus(hadFocus);
    if (!hadFocus)
        return;
    sSummaries.clear();
    BuildPacks();
    Layout();
    UpdateDescription();
    updateTooltips();
}

void UIScene_ShaderMenu::handleInput(int, int action, bool repeat, bool pressed, bool released, bool& handled)
{
    gConsoleUIController.AnimateKeyPress(m_playerId, action, repeat, pressed, released);
    switch (action)
    {
    case EControllerActions_MenuCancel:
        if (pressed)
        {
            navigateBack();
            handled = true;
        }
        return;
    case EControllerActions_MenuUp:
    case EControllerActions_MenuDown:
    case EControllerActions_MenuRight:
    case EControllerActions_MenuLeft:
    case EControllerActions_MenuPageUp:
    case EControllerActions_MenuPageDown:
    case EControllerActions_MenuOtherStickUp:
    case EControllerActions_MenuOtherStickDown:
    case EControllerActions_MenuOk:
        sendInputToMovie(action, repeat, pressed, released);
        handled = true;
        return;
    case EControllerActions_MenuLeftScroll:
    case EControllerActions_MenuRightScroll:
        if (pressed)
        {
            SetTab(m_tab == eTab_Packs ? eTab_Settings : eTab_Packs);
            handled = true;
        }
        return;
    case EControllerActions_MenuX:
        if (pressed && m_tab == eTab_Packs)
        {
            OpenPackSettings();
            handled = true;
        }
        return;
    case EControllerActions_MenuY:
        if (!pressed)
            return;
        handled = true;
        gConsoleUIController.PlayUISFX(SoundEvents::UI_PRESS);
        if (m_tab == eTab_Packs)
        {
            sSummaries.clear();
            BuildPacks();
            Layout();
            UpdateDescription();
        }
        else
        {
            ResetSettings();
        }
        return;
    default:
        return;
    }
}

void UIScene_ShaderMenu::handleFocusChange(int, int childFocus)
{
    m_focus[m_tab] = childFocus;
    UpdateDescription();
}

void UIScene_ShaderMenu::handlePress(int, int childId)
{
    gConsoleUIController.PlayUISFX(SoundEvents::UI_PRESS);
    if (m_tab != eTab_Packs)
        return;
    if (childId == ePack_OpenFolder)
    {
        std::error_code error;
        std::filesystem::path folder = ShaderPackManager::Get().PacksFolder();
        std::filesystem::create_directories(folder, error);
        std::string url = "file:///" + folder.generic_u8string();
        SDL_OpenURL(url.c_str());
        return;
    }
    SelectPack(childId);
}

void UIScene_ShaderMenu::SelectPack(int id)
{
    ShaderPackManager& manager = ShaderPackManager::Get();
    std::string name;
    if (id == ePack_Off)
        name = ShaderSettings::kPackOff;
    else if (id == ePack_Internal)
        name = ShaderSettings::kPackInternal;
    else if (id >= ePack_First && id - ePack_First < static_cast<int>(m_packs.size()))
        name = m_packs[static_cast<size_t>(id - ePack_First)];
    else
        return;

    if (id >= ePack_First && !SummaryFor(name).usable)
        return;
    manager.SelectPack(name);
    ReloadShaders();
    sSummaries.erase(name);
    BuildPacks();
    Layout();
    int index = m_packsList.getListIndex(id);
    if (index >= 0)
        m_packsList.setCurrentSelection(index, false, true);
    m_focus[eTab_Packs] = id;
    UpdateDescription();
    updateTooltips();
}

void UIScene_ShaderMenu::OpenPackSettings()
{
    ShaderPackManager& manager = ShaderPackManager::Get();
    if (manager.ActivePack() == nullptr)
        return;
    gConsoleUIController.PlayUISFX(SoundEvents::UI_PRESS);
    std::wstring title = Plain(manager.Lang().ScreenLabel(std::string()));
    if (title.empty())
        title = PackDisplayName(manager.ActivePack()->Name());
    gConsoleUIController.NavigateToScene(m_playerId, EUIScene_CustomMenu, new ShaderOptionsPage(std::string(), title),
                                         EUILayer_Scene, EUIGroup_PlayerDefault);
}

void UIScene_ShaderMenu::SaveSettings()
{
    ShaderPackManager::Get().SaveSettings();
    ReloadShaders();
    UpdateDescription();
}

void UIScene_ShaderMenu::ResetSettings()
{
    ShaderSettings& settings = ShaderPackManager::Get().EditSettings();
    std::string pack = settings.shaderPack;
    settings = ShaderSettings();
    settings.shaderPack = pack;
    BuildSettings();
    Layout();
    SaveSettings();
}

void UIScene_ShaderMenu::handleCheckboxElementToggled(int, int childId, bool checked)
{
    gConsoleUIController.PlayUISFX(SoundEvents::UI_PRESS);
    ShaderSettings& settings = ShaderPackManager::Get().EditSettings();
    if (childId == eSetting_NormalMap)
        settings.normalMapEnabled = checked;
    else if (childId == eSetting_SpecularMap)
        settings.specularMapEnabled = checked;
    else
        return;
    SaveSettings();
}

void UIScene_ShaderMenu::handleSliderElementMove(int, int childId, int value)
{
    gConsoleUIController.PlayUISFX(SoundEvents::UI_SCROLL);
    ShaderSettings& settings = ShaderPackManager::Get().EditSettings();
    std::wstring label;
    switch (childId)
    {
    case eSetting_Antialiasing:
        value = std::max(0, std::min(value, 2));
        settings.antialiasingLevel = kAntialiasingLevels[value];
        label = kAntialiasingLabels[value];
        break;
    case eSetting_RenderQuality:
        value = std::max(0, std::min(value, 4));
        settings.renderResMul = ShaderSettings::kQualityValues[value];
        label = kQualityLabels[value];
        break;
    case eSetting_ShadowQuality:
        value = std::max(0, std::min(value, 4));
        settings.shadowResMul = ShaderSettings::kQualityValues[value];
        label = kQualityLabels[value];
        break;
    case eSetting_HandDepth:
        value = std::max(0, std::min(value, 2));
        settings.handDepthMul = ShaderSettings::kHandDepthValues[value];
        label = kHandDepthLabels[value];
        break;
    case eSetting_OldHandLight:
        value = std::max(0, std::min(value, 2));
        settings.oldHandLight = kTristates[value];
        label = kTristateLabels[value];
        break;
    case eSetting_OldLighting:
        value = std::max(0, std::min(value, 2));
        settings.oldLighting = kTristates[value];
        label = kTristateLabels[value];
        break;
    default:
        return;
    }
    m_settingsList.SetItemLabel(childId, UIString(label), false);
    SaveSettings();
}

void UIScene_ShaderMenu::handleTouchInput(unsigned int, int, int, int touchBox, bool pressed, bool, bool)
{
    if (!pressed)
        return;
    if (touchBox == 0x34)
        SetTab(eTab_Packs);
    else if (touchBox == 0x35)
        SetTab(eTab_Settings);
}

UIControl* UIScene_ShaderMenu::GetMainPanel()
{
    return m_tab == eTab_Packs ? &m_packsPanel : &m_settingsPanel;
}
