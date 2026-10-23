#include "platform/pc/PcShaderMenu.hpp"

#include <SDL3/SDL.h>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <cwchar>
#include <cwctype>
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
#include "game/client/render/Renderer.hpp"
#include "game/core/CConsoleMinecraftApp.hpp"
#include "game/core/ConsoleUIController.hpp"
#include "game/core/DebugOverlay.hpp"
#include "game/core/ExtraStrings.hpp"
#include "game/core/GlStateManager.hpp"
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
#include "platform/pc/shaderpack/ShaderPackManager.hpp"
#include "platform/pc/shaderpack/ShaderPipeline.hpp"

extern ConsoleUIController gConsoleUIController;

namespace
{
const Logger sLog("Shaders");

enum
{
    eSetting_Antialiasing,
    eSetting_NormalMap,
    eSetting_SpecularMap,
    eSetting_RenderQuality,
    eSetting_ShadowQuality,
    eSetting_HandDepth,
    eSetting_OldHandLight,
    eSetting_OldLighting,
    eSetting_Count
};

enum
{
    eDialog_None,
    eDialog_Import,
    eDialog_Export
};

const int kPackInternal = -1;

const int kLayoutWidth = 640;
const int kLayoutHeight = 360;
const int kPanelLeft = 20;
const int kPanelRight = 620;
const int kPanelTop = 56;
const int kPanelBottom = 316;
const int kSplitRight = 330;
const int kDetailLeft = 336;
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
const int kListBottom = 266;
const int kListRight = kSplitRight - 4;
const int kListRowRight = kListRight - 8;
const int kHintTop = 272;
const int kButtonsTop = 288;
const int kButtonsBottom = 308;
const int kSettingsBottom = 248;
const int kHeaderTop = kPanelTop + 6;
const int kHeaderBottom = kHeaderTop + 17;
const int kHeaderButtonWidth = 48;
const int kGridLeft = kPanelLeft + 6;
const int kGridRight = kPanelRight - 6;
const int kGridCellRight = kGridRight - 8;
const int kGridTop = 86;
const int kGridBottom = 246;
const int kCellGap = 4;
const int kCommentTop = 251;
const int kCommentBottom = kPanelBottom - 6;
const int kSettingValueWidth = 72;
const int kModalLeft = 160;
const int kModalRight = 480;
const int kModalTop = 116;
const int kModalBottom = 240;
const int kModalButtonsTop = 212;
const int kModalButtonsBottom = 230;
const long long kNoticeMilliseconds = 5000;
const long long kFlashMilliseconds = 450;
const long long kReloadWaitMilliseconds = 5000;
const char* const kDownloadUrl = "https://modrinth.com/shaders";

const unsigned int kBlack = 0xFF000000;
const unsigned int kPanelFill = 0xFFD0D0D0;
const unsigned int kPanelLight = 0xFFFFFFFF;
const unsigned int kPanelShade = 0xFF717171;
const unsigned int kTabFill = 0xFFACACAC;
const unsigned int kTabLight = 0xFFD8D8D8;
const unsigned int kButtonFill = 0xFF848484;
const unsigned int kButtonLight = 0xFFB8B8B8;
const unsigned int kButtonShade = 0xFF585858;
const unsigned int kFocusFill = 0xFF919ACA;
const unsigned int kFocusLight = 0xFFC9D0FF;
const unsigned int kFocusShade = 0xFF757FB1;
const unsigned int kDisabledFill = 0xFF6E6E6E;
const unsigned int kDisabledLight = 0xFF808080;
const unsigned int kDisabledShade = 0xFF5A5A5A;
const unsigned int kBoxFill = 0xFF373737;
const unsigned int kBoxFocusFill = 0xFF323A6E;
const unsigned int kBoxEdge = 0xFF1C1C1C;
const unsigned int kHandleFill = 0xFFB4B4B4;
const unsigned int kDescriptionFill = 0xD8394A52;
const unsigned int kDescriptionEdge = 0xFFE4E4E4;
const unsigned int kTextWhite = 0xFFFFFFFF;
const unsigned int kTextLight = 0xFFE0E0E0;
const unsigned int kTextBody = 0xFFD4D4D4;
const unsigned int kTextYellow = 0xFFFFFF55;
const unsigned int kTextDark = 0xFF424242;
const unsigned int kTextHint = 0xFF5C5C5C;
const unsigned int kTextGrey = 0xFFA0A0A0;
const unsigned int kTextDisabled = 0xFFB0B0B0;
const unsigned int kTextModified = 0xFFFFC94A;
const unsigned int kTextValue = 0xFF8EA8FF;
const unsigned int kTextOn = 0xFF55FF55;
const unsigned int kTextOff = 0xFFFF5555;
const unsigned int kTextWarning = 0xFFFFAA00;
const unsigned int kTextError = 0xFFFF6B6B;
const unsigned int kTextProblem = 0xFFFFB4B4;
const unsigned int kChipGreen = 0xFF3C8527;
const unsigned int kChipGreenEdge = 0xFF1D4D13;
const unsigned int kChipAmber = 0xFFC79A1E;
const unsigned int kChipAmberEdge = 0xFF6E5310;
const unsigned int kChipRed = 0xFF8A2B2B;
const unsigned int kChipRedEdge = 0xFF4A1414;
const unsigned int kChipGrey = 0xFF4E4E4E;
const unsigned int kChipGreyEdge = 0xFF262626;

const int kAntialiasingLevels[3] = {0, 2, 4};
const wchar_t* const kAntialiasingLabels[3] = {L"OFF", L"FXAA 2x", L"FXAA 4x"};
const wchar_t* const kQualityLabels[5] = {L"0.5x", L"0.7x", L"1x", L"1.4x", L"2x"};
const wchar_t* const kHandDepthLabels[3] = {L"0.5x", L"1x", L"2x"};
const wchar_t* const kTristateLabels[3] = {L"Default", L"ON", L"OFF"};
const ShaderSettings::Tristate kTristates[3] = {ShaderSettings::Default, ShaderSettings::On, ShaderSettings::Off};
const wchar_t* const kSettingNames[eSetting_Count] = {L"Antialiasing", L"Normal Map",     L"Specular Map",
                                                      L"Render Quality", L"Shadow Quality", L"Hand Depth",
                                                      L"Old Hand Light", L"Old Lighting"};

const wchar_t kFormatSign = static_cast<wchar_t>(0xA7);

struct DialogResult
{
    std::mutex mutex;
    const void* owner = nullptr;
    int kind = eDialog_None;
    bool ready = false;
    bool chosen = false;
    std::string path;
    std::string error;
};

DialogResult sDialog;

void SDLCALL DialogCallback(void* userdata, const char* const* files, int)
{
    std::lock_guard<std::mutex> lock(sDialog.mutex);
    sDialog.ready = true;
    sDialog.kind = static_cast<int>(reinterpret_cast<intptr_t>(userdata));
    sDialog.chosen = files != nullptr && files[0] != nullptr;
    sDialog.path = sDialog.chosen ? std::string(files[0]) : std::string();
    sDialog.error = files == nullptr ? std::string(SDL_GetError()) : std::string();
}

std::wstring Wide(const std::string& text)
{
    return convStringToWstring(text);
}

std::wstring FormatReset()
{
    return std::wstring(1, kFormatSign) + L"r";
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

bool HasPackOptions(const std::string& text)
{
    const ShaderPackOptions& options = ShaderPackManager::Get().Options();
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

float Approach(float value, float target, float milliseconds, float timeConstant)
{
    return value + (target - value) * (1.0f - std::exp(-milliseconds / timeConstant));
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

int SettingCount(int setting)
{
    switch (setting)
    {
    case eSetting_NormalMap:
    case eSetting_SpecularMap:
        return 2;
    case eSetting_RenderQuality:
    case eSetting_ShadowQuality:
        return 5;
    default:
        return 3;
    }
}

bool SettingIsSlider(int setting)
{
    return setting == eSetting_Antialiasing || setting == eSetting_RenderQuality || setting == eSetting_ShadowQuality
           || setting == eSetting_HandDepth;
}

int SettingIndex(const ShaderSettings& settings, int setting)
{
    switch (setting)
    {
    case eSetting_Antialiasing:
        return settings.antialiasingLevel >= 4 ? 2 : settings.antialiasingLevel >= 2 ? 1 : 0;
    case eSetting_NormalMap:
        return settings.normalMapEnabled ? 1 : 0;
    case eSetting_SpecularMap:
        return settings.specularMapEnabled ? 1 : 0;
    case eSetting_RenderQuality:
        return NearestIndex(ShaderSettings::kQualityValues, 5, settings.renderResMul);
    case eSetting_ShadowQuality:
        return NearestIndex(ShaderSettings::kQualityValues, 5, settings.shadowResMul);
    case eSetting_HandDepth:
        return NearestIndex(ShaderSettings::kHandDepthValues, 3, settings.handDepthMul);
    case eSetting_OldHandLight:
        return TristateIndex(settings.oldHandLight);
    case eSetting_OldLighting:
        return TristateIndex(settings.oldLighting);
    default:
        return 0;
    }
}

void SetSettingIndex(ShaderSettings& settings, int setting, int index)
{
    switch (setting)
    {
    case eSetting_Antialiasing:
        settings.antialiasingLevel = kAntialiasingLevels[index];
        break;
    case eSetting_NormalMap:
        settings.normalMapEnabled = index != 0;
        break;
    case eSetting_SpecularMap:
        settings.specularMapEnabled = index != 0;
        break;
    case eSetting_RenderQuality:
        settings.renderResMul = ShaderSettings::kQualityValues[index];
        break;
    case eSetting_ShadowQuality:
        settings.shadowResMul = ShaderSettings::kQualityValues[index];
        break;
    case eSetting_HandDepth:
        settings.handDepthMul = ShaderSettings::kHandDepthValues[index];
        break;
    case eSetting_OldHandLight:
        settings.oldHandLight = kTristates[index];
        break;
    case eSetting_OldLighting:
        settings.oldLighting = kTristates[index];
        break;
    default:
        break;
    }
}

std::wstring SettingLabel(int setting, int index)
{
    switch (setting)
    {
    case eSetting_Antialiasing:
        return kAntialiasingLabels[index];
    case eSetting_NormalMap:
    case eSetting_SpecularMap:
        return index != 0 ? L"ON" : L"OFF";
    case eSetting_RenderQuality:
    case eSetting_ShadowQuality:
        return kQualityLabels[index];
    case eSetting_HandDepth:
        return kHandDepthLabels[index];
    default:
        return kTristateLabels[index];
    }
}

unsigned int SettingColour(int setting, int index)
{
    int defaultIndex = SettingIndex(ShaderSettings(), setting);
    if (setting == eSetting_NormalMap || setting == eSetting_SpecularMap)
        return index == defaultIndex ? kTextLight : index != 0 ? kTextOn : kTextOff;
    if (setting == eSetting_OldHandLight || setting == eSetting_OldLighting)
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

unsigned int WithAlpha(unsigned int colour, float alpha)
{
    if (alpha >= 1.0f)
        return colour;
    unsigned int value = static_cast<unsigned int>(((colour >> 24) & 255) * (alpha < 0.0f ? 0.0f : alpha));
    return (colour & 0x00FFFFFF) | (value << 24);
}

class Painter
{
public:
    Painter(GuiComponent& gui, Font& font) : m_gui(gui), m_font(font) {}

    void Fill(int x0, int y0, int x1, int y1, unsigned int colour)
    {
        if (x1 > x0 && y1 > y0)
            m_gui.fill(x0, y0, x1, y1, static_cast<int>(colour));
    }

    void Gradient(int x0, int y0, int x1, int y1, unsigned int top, unsigned int bottom)
    {
        m_gui.fillGradient(x0, y0, x1, y1, static_cast<int>(top), static_cast<int>(bottom));
    }

    int Width(const std::wstring& text) { return m_font.width(text); }

    void Text(const std::wstring& text, int x, int y, unsigned int colour, bool shadow)
    {
        GlStateManager::enableBlend();
        GlStateManager::blendFunc(4, 5);
        GlStateManager::enableTexture();
        if (shadow)
            m_font.drawShadow(text, x, y, static_cast<int>(colour));
        else
            m_font.draw(text, x, y, static_cast<int>(colour));
    }

    void Centred(const std::wstring& text, int centreX, int y, unsigned int colour, bool shadow)
    {
        Text(text, centreX - Width(text) / 2, y, colour, shadow);
    }

    void Outline(int x0, int y0, int x1, int y1, unsigned int colour)
    {
        Fill(x0, y0, x1, y0 + 1, colour);
        Fill(x0, y1 - 1, x1, y1, colour);
        Fill(x0, y0 + 1, x0 + 1, y1 - 1, colour);
        Fill(x1 - 1, y0 + 1, x1, y1 - 1, colour);
    }

    void Panel(int x0, int y0, int x1, int y1)
    {
        Fill(x0 + 2, y0, x1 - 2, y0 + 1, kBlack);
        Fill(x0 + 2, y1 - 1, x1 - 2, y1, kBlack);
        Fill(x0, y0 + 2, x0 + 1, y1 - 2, kBlack);
        Fill(x1 - 1, y0 + 2, x1, y1 - 2, kBlack);
        Fill(x0 + 2, y0 + 1, x1 - 2, y1 - 1, kPanelFill);
        Fill(x0 + 1, y0 + 2, x0 + 2, y1 - 2, kPanelFill);
        Fill(x1 - 2, y0 + 2, x1 - 1, y1 - 2, kPanelFill);
        Fill(x0 + 2, y0 + 1, x1 - 2, y0 + 2, kPanelLight);
        Fill(x0 + 1, y0 + 2, x0 + 2, y1 - 2, kPanelLight);
        Fill(x0 + 2, y1 - 2, x1 - 2, y1 - 1, kPanelShade);
        Fill(x1 - 2, y0 + 2, x1 - 1, y1 - 2, kPanelShade);
        Fill(x0 + 1, y0 + 1, x0 + 2, y0 + 2, kBlack);
        Fill(x1 - 2, y0 + 1, x1 - 1, y0 + 2, kBlack);
        Fill(x0 + 1, y1 - 2, x0 + 2, y1 - 1, kBlack);
        Fill(x1 - 2, y1 - 2, x1 - 1, y1 - 1, kBlack);
    }

    void Button(int x0, int y0, int x1, int y1, bool focused, bool enabled)
    {
        unsigned int fill = !enabled ? kDisabledFill : focused ? kFocusFill : kButtonFill;
        unsigned int light = !enabled ? kDisabledLight : focused ? kFocusLight : kButtonLight;
        unsigned int shade = !enabled ? kDisabledShade : focused ? kFocusShade : kButtonShade;
        Outline(x0, y0, x1, y1, kBlack);
        Fill(x0 + 1, y0 + 1, x1 - 1, y1 - 1, fill);
        Fill(x0 + 1, y0 + 1, x1 - 1, y0 + 2, light);
        Fill(x0 + 1, y0 + 2, x0 + 2, y1 - 2, light);
        Fill(x0 + 1, y1 - 2, x1 - 1, y1 - 1, shade);
        Fill(x1 - 2, y0 + 2, x1 - 1, y1 - 2, shade);
    }

    void Ring(int x0, int y0, int x1, int y1, float pulse)
    {
        int grow = 1 + static_cast<int>(pulse * 3.0f);
        unsigned int alpha = static_cast<unsigned int>((1.0f - pulse) * 200.0f);
        unsigned int colour = (alpha << 24) | 0xFFFF55;
        Outline(x0 - grow, y0 - grow, x1 + grow, y1 + grow, colour);
    }

    void Highlight(int x0, int y0, int x1, int y1, float amount)
    {
        if (amount > 0.0f)
            Fill(x0, y0, x1, y1, WithAlpha(0xFFFFFFFF, amount * 0.35f));
    }

    void Box(int x0, int y0, int x1, int y1, bool focused)
    {
        Outline(x0, y0, x1, y1, kBoxEdge);
        Fill(x0 + 1, y0 + 1, x1 - 1, y1 - 1, focused ? kBoxFocusFill : kBoxFill);
        Fill(x0 + 1, y0 + 1, x1 - 1, y0 + 2, 0x60000000);
    }

    void Slider(int boxX0, int boxX1, int y1, int position, int count, bool focused, unsigned int colour)
    {
        int left = boxX0 + 4;
        int right = boxX1 - 4;
        int handle = count > 1 ? left + (right - left) * position / (count - 1) : left;
        Fill(left, y1 - 6, right, y1 - 5, 0xFF1E1E1E);
        Fill(left, y1 - 6, handle, y1 - 5, WithAlpha(colour, 0.85f));
        Fill(handle - 2, y1 - 8, handle + 2, y1 - 3, kBlack);
        Fill(handle - 1, y1 - 7, handle + 1, y1 - 4, focused ? kFocusLight : kHandleFill);
    }

    void Description(int x0, int y0, int x1, int y1)
    {
        Fill(x0 + 1, y0 + 1, x1 - 1, y1 - 1, kDescriptionFill);
        Outline(x0, y0, x1, y1, kDescriptionEdge);
    }

    int Chip(int right, int y, const std::wstring& text, unsigned int fill, unsigned int edge)
    {
        int width = Width(text) + 8;
        int left = right - width;
        Outline(left, y, right, y + 11, edge);
        Fill(left + 1, y + 1, right - 1, y + 10, fill);
        Text(text, left + 4, y + 2, kTextWhite, true);
        return left;
    }

    void Scrollbar(int x, int y0, int y1, int content, float scroll, unsigned int track, unsigned int thumb)
    {
        int visible = y1 - y0;
        if (content <= visible || visible <= 0)
            return;
        int barHeight = std::max(8, visible * visible / content);
        int barTop = y0 + static_cast<int>((visible - barHeight) * scroll / (content - visible));
        Fill(x, y0, x + 3, y1, track);
        Fill(x, barTop, x + 3, barTop + barHeight, thumb);
    }

    void PackIcon(int x, int y, bool colourful)
    {
        unsigned int skyTop = colourful ? 0xFF5E93E8 : Grey(0xFF5E93E8);
        unsigned int skyBottom = colourful ? 0xFFB8D8FF : Grey(0xFFB8D8FF);
        unsigned int sun = colourful ? 0xFFFFE15A : Grey(0xFFFFE15A);
        unsigned int hill = colourful ? 0xFF5BA33B : Grey(0xFF5BA33B);
        unsigned int hillDark = colourful ? 0xFF3E7A28 : Grey(0xFF3E7A28);
        Outline(x, y, x + 14, y + 14, kBlack);
        Gradient(x + 1, y + 1, x + 13, y + 9, skyTop, skyBottom);
        Fill(x + 8, y + 3, x + 11, y + 6, sun);
        Fill(x + 1, y + 9, x + 13, y + 13, hill);
        Fill(x + 2, y + 7, x + 7, y + 9, hill);
        Fill(x + 3, y + 6, x + 6, y + 7, hill);
        Fill(x + 1, y + 12, x + 13, y + 13, hillDark);
        Fill(x + 9, y + 10, x + 11, y + 11, hillDark);
    }

    void GrassIcon(int x, int y, bool colourful)
    {
        unsigned int dirt = colourful ? 0xFF8B5A2B : Grey(0xFF8B5A2B);
        unsigned int dirtDark = colourful ? 0xFF6B4320 : Grey(0xFF6B4320);
        unsigned int grass = colourful ? 0xFF5BA33B : Grey(0xFF5BA33B);
        unsigned int grassLight = colourful ? 0xFF79C24F : Grey(0xFF79C24F);
        Outline(x, y, x + 14, y + 14, kBlack);
        Fill(x + 1, y + 1, x + 13, y + 13, dirt);
        Fill(x + 1, y + 1, x + 13, y + 5, grass);
        Fill(x + 3, y + 5, x + 4, y + 7, grass);
        Fill(x + 9, y + 5, x + 10, y + 6, grass);
        Fill(x + 5, y + 9, x + 7, y + 10, dirtDark);
        Fill(x + 9, y + 11, x + 11, y + 12, dirtDark);
        Fill(x + 1, y + 1, x + 13, y + 2, grassLight);
    }

    std::wstring Trim(const std::wstring& text, int width)
    {
        if (Width(text) <= width)
            return text;
        const std::wstring dots = L"...";
        int limit = width - Width(dots);
        std::wstring out;
        for (size_t index = 0; index < text.size(); ++index)
        {
            if (text[index] == kFormatSign && index + 1 < text.size())
            {
                out += text[index];
                out += text[++index];
                continue;
            }
            std::wstring next = out + text[index];
            if (Width(next) > limit)
                break;
            out = next;
        }
        return out + FormatReset() + dots;
    }

    std::wstring TrimStart(const std::wstring& text, int width)
    {
        if (Width(text) <= width)
            return text;
        const std::wstring dots = L"...";
        std::wstring out = text;
        while (!out.empty() && Width(dots + out) > width)
            out.erase(0, 1);
        return dots + out;
    }

    std::vector<std::wstring> Wrap(const std::wstring& text, int width)
    {
        std::vector<std::wstring> lines;
        size_t start = 0;
        while (start <= text.size())
        {
            size_t end = text.find(L'\n', start);
            if (end == std::wstring::npos)
                end = text.size();
            WrapLine(text.substr(start, end - start), width, lines);
            start = end + 1;
        }
        return lines;
    }

private:
    static std::wstring ActiveFormat(const std::wstring& text)
    {
        std::wstring format;
        for (size_t index = 0; index + 1 < text.size(); ++index)
        {
            if (text[index] != kFormatSign)
                continue;
            wchar_t code = static_cast<wchar_t>(std::towlower(text[index + 1]));
            if ((code >= L'0' && code <= L'9') || (code >= L'a' && code <= L'f'))
                format = std::wstring(1, kFormatSign) + code;
            else if (code == L'r')
                format.clear();
            else if (code >= L'k' && code <= L'o')
                format += std::wstring(1, kFormatSign) + code;
            ++index;
        }
        return format;
    }

    void WrapLine(const std::wstring& text, int width, std::vector<std::wstring>& lines)
    {
        std::wstring line;
        std::wstring carried;
        size_t start = 0;
        while (start <= text.size())
        {
            size_t end = text.find(L' ', start);
            if (end == std::wstring::npos)
                end = text.size();
            std::wstring word = text.substr(start, end - start);
            std::wstring candidate = line.empty() ? carried + word : line + L" " + word;
            if (Width(candidate) <= width || line.empty())
            {
                line = candidate;
                while (Width(line) > width && line.size() > 1)
                {
                    size_t cut = line.size();
                    while (cut > 1 && Width(line.substr(0, cut)) > width)
                        --cut;
                    lines.push_back(line.substr(0, cut));
                    carried = ActiveFormat(line.substr(0, cut));
                    line = carried + line.substr(cut);
                }
            }
            else
            {
                lines.push_back(line);
                carried = ActiveFormat(line);
                line = carried + word;
            }
            start = end + 1;
        }
        if (!line.empty() || lines.empty())
            lines.push_back(line);
    }

    GuiComponent& m_gui;
    Font& m_font;
};

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

int CountMainPages(const std::vector<std::string>& items)
{
    int pages = 0;
    for (const std::string& item : items)
    {
        if (item.size() > 2 && item.front() == '[' && item.back() == ']')
            ++pages;
    }
    return pages;
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
        const ShaderPackProperties& properties = manager.Properties();
        for (const ShaderProfile& profile : properties.Profiles())
            summary.profiles.push_back(Wide(manager.Lang().ProfileLabel(profile.name)));
        summary.totalPages = static_cast<int>(properties.Screens().size());
        if (const ShaderScreen* root = properties.Screen(std::string()))
            summary.mainPages = CountMainPages(root->items);
        summary.usable = !active->ProgramFiles().empty();
        SummariseWorlds(*active, summary);
        if (!summary.usable)
            summary.problem = L"it contains no shader programs";
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
    std::string text;
    std::set<std::string> screens;
    if (pack->ReadFile("shaders.properties", text))
    {
        std::set<std::string> profiles;
        for (const auto& entry : ShaderParseProperties(text))
        {
            const std::string& key = entry.first;
            if (key.compare(0, 8, "profile.") == 0 && key.find('.', 8) == std::string::npos
                && profiles.insert(key.substr(8)).second)
                summary.profiles.push_back(Wide(lang.ProfileLabel(key.substr(8))));
            else if (key == "screen" || (key.compare(0, 7, "screen.") == 0 && key.find('.', 7) == std::string::npos))
            {
                screens.insert(key);
                if (key == "screen")
                    summary.mainPages = CountMainPages(ShaderSplitList(entry.second));
            }
        }
    }
    summary.totalPages = static_cast<int>(screens.size());
    summary.usable = !pack->ProgramFiles().empty();
    SummariseWorlds(*pack, summary);
    if (!summary.usable)
        summary.problem = L"it contains no shader programs";
    return summary;
}

const PackSummary& SummaryFor(const std::string& name)
{
    std::map<std::string, PackSummary>::iterator found = sSummaries.find(name);
    if (found == sSummaries.end())
        found = sSummaries.insert(std::make_pair(name, Summarise(name))).first;
    return found->second;
}

std::wstring CellValueText(bool profile, const std::string& name, unsigned int* colour)
{
    ShaderPackManager& manager = ShaderPackManager::Get();
    if (profile)
    {
        std::string active = manager.Properties().ActiveProfile(manager.Options());
        *colour = active.empty() ? kTextYellow : kTextLight;
        return active.empty() ? std::wstring(L"Custom") : Wide(manager.Lang().ProfileLabel(active));
    }
    const ShaderOption* option = manager.Options().Find(name);
    if (option == nullptr)
    {
        *colour = kTextGrey;
        return std::wstring();
    }
    std::string value = manager.Options().Value(name);
    if (option->IsBoolean())
        *colour = value == option->defaultValue ? kTextLight : value == "true" ? kTextOn : kTextOff;
    else
        *colour = kTextValue;
    return Wide(manager.Lang().ValueLabel(*option, value));
}

int ValueBoxWidth(Font& font, const std::wstring& text, int cellWidth, bool slider)
{
    int width = font.width(text) + 12;
    width = std::max(width, slider ? 72 : 44);
    return std::min(width, cellWidth / 2);
}
} // namespace

UIScene_ShaderMenu::UIScene_ShaderMenu(int playerId, void*, UILayer* layer)
    : UIScene(playerId, layer),
      m_tab(eTab_Packs),
      m_columns(2),
      m_workingSettings(new ShaderSettings()),
      m_appliedSettings(new ShaderSettings()),
      m_detailHeight(0),
      m_noticeColour(kTextLight),
      m_noticeTime(0),
      m_modal(eModal_None),
      m_modalFocus(0),
      m_hidden(false),
      m_awaitingReload(false),
      m_reloadTime(0),
      m_lastTime(0),
      m_lastRefresh(0),
      m_pulse(0.0f),
      m_flashKind(eTarget_None),
      m_flashIndex(0),
      m_flashTime(0),
      m_dragIndex(-1),
      m_originX(0),
      m_originY(0),
      m_unit(1.0f),
      m_pointerX(-1.0f),
      m_pointerY(-1.0f),
      m_dropComplete(false)
{
    initialiseMovie();
    for (int view = 0; view < eView_Count; ++view)
    {
        m_scroll[view] = 0.0f;
        m_scrollTarget[view] = 0.0f;
        m_mouseScroll[view] = false;
    }
    for (int index = 0; index < 4; ++index)
        m_savedScissor[index] = 0;
    for (int index = 0; index < 16; ++index)
        m_tooltips[index] = -3;
    m_focus[eTab_Packs] = Focus{eTarget_Toggle, 0};
    m_focus[eTab_Options] = Focus{eTarget_Cell, 0};
    m_focus[eTab_Settings] = Focus{eTarget_Setting, 0};
    {
        std::lock_guard<std::mutex> lock(sDialog.mutex);
        sDialog.ready = false;
        sDialog.owner = nullptr;
    }

    sSummaries.clear();
    ShaderPackManager& manager = ShaderPackManager::Get();
    *m_appliedSettings = manager.Settings();
    *m_workingSettings = *m_appliedSettings;
    SnapshotOptions();
    m_packs = manager.ListPacks();
    m_lastRefresh = System::processTimeInMilliSecs();
    BuildCells();
    FocusSelectedPack();
    PcWindowSetDropHandler(&UIScene_ShaderMenu::HandleDrop, this);
}

UIScene_ShaderMenu::~UIScene_ShaderMenu()
{
    PcWindowSetDropHandler(NULL, NULL);
    std::lock_guard<std::mutex> lock(sDialog.mutex);
    if (sDialog.owner == this)
        sDialog.owner = nullptr;
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

void UIScene_ShaderMenu::HandleDrop(const char* path, void* context)
{
    UIScene_ShaderMenu* menu = static_cast<UIScene_ShaderMenu*>(context);
    if (path == NULL)
        menu->m_dropComplete = true;
    else
        menu->m_dropped.push_back(path);
}

void UIScene_ShaderMenu::tick()
{
    UIScene::tick();
    BuildTargets();
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
                if (PackName(target.index) == m_appliedSettings->shaderPack && m_appliedSettings->UsesPack()
                    && !PackChangePending())
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

void UIScene_ShaderMenu::AddTarget(int kind, int index, int x0, int y0, int x1, int y1, int view, bool enabled)
{
    Target target = {kind, index, x0, y0, x1, y1, view, enabled};
    m_targets.push_back(target);
}

void UIScene_ShaderMenu::BuildTargets()
{
    m_targets.clear();
    if (m_hidden)
        return;
    if (m_modal != eModal_None)
    {
        int middle = (kModalLeft + kModalRight) / 2;
        AddTarget(eTarget_Modal, 0, kModalLeft + 16, kModalButtonsTop, middle - 4, kModalButtonsBottom, eView_Fixed,
                  true);
        AddTarget(eTarget_Modal, 1, middle + 4, kModalButtonsTop, kModalRight - 16, kModalButtonsBottom, eView_Fixed,
                  true);
        return;
    }

    if (Minecraft::GetInstance()->mLevel != NULL)
        AddTarget(eTarget_Hide, 0, kPanelLeft, 8, kPanelLeft + 64, 24, eView_Fixed, true);
    for (int tab = 0; tab < eTab_Count; ++tab)
    {
        int x0 = kPanelLeft + tab * (kTabWidth + kTabGap);
        AddTarget(eTarget_Tab, tab, x0, kTabTop, x0 + kTabWidth, kPanelTop, eView_Fixed, TabEnabled(tab));
    }
    AddTarget(eTarget_Cancel, 0, kPanelRight - 3 * kActionWidth - 8, kActionTop, kPanelRight - 2 * kActionWidth - 8,
              kActionBottom, eView_Fixed, true);
    AddTarget(eTarget_Apply, 0, kPanelRight - 2 * kActionWidth - 4, kActionTop, kPanelRight - kActionWidth - 4,
              kActionBottom, eView_Fixed, HasPendingChanges());
    AddTarget(eTarget_Done, 0, kPanelRight - kActionWidth, kActionTop, kPanelRight, kActionBottom, eView_Fixed, true);

    ShaderPackManager& manager = ShaderPackManager::Get();
    if (m_tab == eTab_Packs)
    {
        AddTarget(eTarget_Toggle, 0, kListLeft, 0, kListRowRight, kRowHeight, eView_List, true);
        int rowTop = kRowPitch + 4;
        AddTarget(eTarget_Pack, kPackInternal, kListLeft, rowTop, kListRowRight, rowTop + kRowHeight, eView_List, true);
        for (size_t index = 0; index < m_packs.size(); ++index)
        {
            rowTop += kRowPitch;
            AddTarget(eTarget_Pack, static_cast<int>(index), kListLeft, rowTop, kListRowRight, rowTop + kRowHeight,
                      eView_List, true);
        }
        int middle = (kListLeft + kListRowRight) / 2;
        AddTarget(eTarget_OpenFolder, 0, kListLeft, kButtonsTop, middle - 2, kButtonsBottom, eView_Fixed, true);
        AddTarget(eTarget_Download, 0, middle + 2, kButtonsTop, kListRowRight, kButtonsBottom, eView_Fixed, true);
    }
    else if (m_tab == eTab_Options && manager.ActivePack() != nullptr)
    {
        if (!m_history.empty())
            AddTarget(eTarget_Back, 0, kGridLeft, kHeaderTop, kGridLeft + kHeaderButtonWidth, kHeaderBottom,
                      eView_Fixed, true);
        int right = kGridRight;
        AddTarget(eTarget_Reset, 0, right - kHeaderButtonWidth, kHeaderTop, right, kHeaderBottom, eView_Fixed, true);
        right -= kHeaderButtonWidth + 4;
        AddTarget(eTarget_Export, 0, right - kHeaderButtonWidth, kHeaderTop, right, kHeaderBottom, eView_Fixed, true);
        right -= kHeaderButtonWidth + 4;
        AddTarget(eTarget_Import, 0, right - kHeaderButtonWidth, kHeaderTop, right, kHeaderBottom, eView_Fixed, true);

        int columns = std::max(1, m_columns);
        int cellWidth = (kGridCellRight - kGridLeft - (columns - 1) * kCellGap) / columns;
        for (size_t index = 0; index < m_cells.size(); ++index)
        {
            const Cell& cell = m_cells[index];
            if (cell.kind == eCell_Empty)
                continue;
            int row = static_cast<int>(index) / columns;
            int column = static_cast<int>(index) % columns;
            int x0 = kGridLeft + column * (cellWidth + kCellGap);
            bool enabled = true;
            if (cell.kind == eCell_Option)
            {
                const ShaderOption* option = manager.Options().Find(cell.name);
                enabled = option != nullptr && !option->ambiguous;
            }
            AddTarget(eTarget_Cell, static_cast<int>(index), x0, row * kRowPitch, x0 + cellWidth,
                      row * kRowPitch + kRowHeight, eView_Grid, enabled);
        }
    }
    else if (m_tab == eTab_Settings)
    {
        for (int setting = 0; setting < eSetting_Count; ++setting)
            AddTarget(eTarget_Setting, setting, kListLeft, setting * kRowPitch, kListRowRight,
                      setting * kRowPitch + kRowHeight, eView_Settings, true);
    }

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
    {
        for (size_t index = 0; index < m_targets.size(); ++index)
        {
            if (m_targets[index].kind == eTarget_Tab && m_targets[index].index == m_tab)
                fallback = static_cast<int>(index);
        }
    }
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

void UIScene_ShaderMenu::ViewRect(int view, int* x0, int* y0, int* x1, int* y1) const
{
    switch (view)
    {
    case eView_List:
        *x0 = kListLeft;
        *y0 = kListTop;
        *x1 = kListRight;
        *y1 = kListBottom;
        return;
    case eView_Grid:
        *x0 = kGridLeft;
        *y0 = kGridTop;
        *x1 = kGridRight;
        *y1 = kGridBottom;
        return;
    case eView_Settings:
        *x0 = kListLeft;
        *y0 = kListTop;
        *x1 = kListRight;
        *y1 = kSettingsBottom;
        return;
    case eView_Details:
        if (m_tab == eTab_Options)
        {
            *x0 = kGridLeft + 6;
            *y0 = kCommentTop + 4;
            *x1 = kGridRight - 6;
            *y1 = kCommentBottom - 2;
            return;
        }
        *x0 = kDetailLeft + 6;
        *y0 = kPanelTop + 6;
        *x1 = kPanelRight - 6;
        *y1 = kPanelBottom - 6;
        return;
    default:
        *x0 = 0;
        *y0 = 0;
        *x1 = kLayoutWidth;
        *y1 = kLayoutHeight;
        return;
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
        return eSetting_Count * kRowPitch - kRowGap;
    case eView_Details:
        return m_detailHeight;
    default:
        return 0;
    }
}

void UIScene_ShaderMenu::ScreenRect(const Target& target, int* x0, int* y0, int* x1, int* y1) const
{
    *x0 = target.x0;
    *x1 = target.x1;
    *y0 = target.y0;
    *y1 = target.y1;
    if (target.view == eView_Fixed)
        return;
    int viewX0 = 0;
    int viewY0 = 0;
    int viewX1 = 0;
    int viewY1 = 0;
    ViewRect(target.view, &viewX0, &viewY0, &viewX1, &viewY1);
    int offset = viewY0 - static_cast<int>(m_scroll[target.view] + 0.5f);
    *y0 += offset;
    *y1 += offset;
}

int UIScene_ShaderMenu::TargetAt(float x, float y) const
{
    for (size_t index = 0; index < m_targets.size(); ++index)
    {
        const Target& target = m_targets[index];
        if (!target.enabled)
            continue;
        if (target.view != eView_Fixed)
        {
            int viewX0 = 0;
            int viewY0 = 0;
            int viewX1 = 0;
            int viewY1 = 0;
            ViewRect(target.view, &viewX0, &viewY0, &viewX1, &viewY1);
            if (x < viewX0 || x >= viewX1 || y < viewY0 || y >= viewY1)
                continue;
        }
        int x0 = 0;
        int y0 = 0;
        int x1 = 0;
        int y1 = 0;
        ScreenRect(target, &x0, &y0, &x1, &y1);
        if (x >= x0 && x < x1 && y >= y0 && y < y1)
            return static_cast<int>(index);
    }
    return -1;
}

UIScene_ShaderMenu::Focus& UIScene_ShaderMenu::CurrentFocus()
{
    return m_focus[m_tab];
}

float UIScene_ShaderMenu::Flash(int kind, int index) const
{
    long long since = System::processTimeInMilliSecs() - m_flashTime;
    if (m_flashKind != kind || m_flashIndex != index || since >= kFlashMilliseconds)
        return 0.0f;
    return 1.0f - static_cast<float>(since) / kFlashMilliseconds;
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
    {
        m_scroll[eView_Details] = 0.0f;
        m_scrollTarget[eView_Details] = 0.0f;
        m_mouseScroll[eView_Details] = false;
    }
    focus = Focus{target.kind, target.index};
    if (keyboard)
        EnsureVisible(target);
}

void UIScene_ShaderMenu::EnsureVisible(const Target& target)
{
    if (target.view == eView_Fixed)
        return;
    int viewX0 = 0;
    int viewY0 = 0;
    int viewX1 = 0;
    int viewY1 = 0;
    ViewRect(target.view, &viewX0, &viewY0, &viewX1, &viewY1);
    float visible = static_cast<float>(viewY1 - viewY0);
    float margin = 4.0f;
    float scroll = m_mouseScroll[target.view] ? m_scroll[target.view] : m_scrollTarget[target.view];
    if (target.y0 - margin < scroll)
        scroll = target.y0 - margin;
    if (target.y1 + margin > scroll + visible)
        scroll = target.y1 + margin - visible;
    m_scrollTarget[target.view] = std::max(0.0f, std::min(scroll, MaxScroll(target.view)));
    m_mouseScroll[target.view] = false;
}

float UIScene_ShaderMenu::MaxScroll(int view) const
{
    int viewX0 = 0;
    int viewY0 = 0;
    int viewX1 = 0;
    int viewY1 = 0;
    ViewRect(view, &viewX0, &viewY0, &viewX1, &viewY1);
    return std::max(0.0f, static_cast<float>(ContentHeight(view) - (viewY1 - viewY0)));
}

void UIScene_ShaderMenu::ScrollView(int view, float amount)
{
    if (!m_mouseScroll[view])
        m_scrollTarget[view] = m_scroll[view];
    m_mouseScroll[view] = true;
    m_scrollTarget[view] = std::max(0.0f, std::min(m_scrollTarget[view] + amount, MaxScroll(view)));
}

void UIScene_ShaderMenu::Move(int dx, int dy)
{
    if (m_modal != eModal_None)
    {
        if (dx != 0)
        {
            int next = dx > 0 ? 1 : 0;
            if (next != m_modalFocus)
            {
                m_modalFocus = next;
                gConsoleUIController.PlayUISFX(SoundEvents::UI_FOCUS);
            }
        }
        return;
    }
    int current = FindTarget(CurrentFocus());
    if (current < 0)
        return;
    int cx0 = 0;
    int cy0 = 0;
    int cx1 = 0;
    int cy1 = 0;
    ScreenRect(m_targets[static_cast<size_t>(current)], &cx0, &cy0, &cx1, &cy1);
    int centreX = (cx0 + cx1) / 2;
    int centreY = (cy0 + cy1) / 2;
    int best = -1;
    int bestScore = 0;
    for (size_t index = 0; index < m_targets.size(); ++index)
    {
        if (static_cast<int>(index) == current || !m_targets[index].enabled)
            continue;
        int x0 = 0;
        int y0 = 0;
        int x1 = 0;
        int y1 = 0;
        ScreenRect(m_targets[index], &x0, &y0, &x1, &y1);
        int targetX = (x0 + x1) / 2;
        int targetY = (y0 + y1) / 2;
        int primary = 0;
        int secondary = 0;
        if (dy != 0)
        {
            primary = (targetY - centreY) * dy;
            if (primary <= 2)
                continue;
            if (x1 <= cx0)
                secondary = cx0 - x1;
            else if (x0 >= cx1)
                secondary = x0 - cx1;
            secondary += std::abs(targetX - centreX) / 8;
        }
        else
        {
            primary = (targetX - centreX) * dx;
            if (primary <= 2 || y1 <= cy0 || y0 >= cy1)
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
        return m_workingSettings->UsesPack()
               && (PackChangePending() || ShaderPackManager::Get().ActivePack() != nullptr);
    return true;
}

void UIScene_ShaderMenu::SetTab(int tab)
{
    if (tab == m_tab || tab < 0 || tab >= eTab_Count || !TabEnabled(tab))
        return;
    if (tab == eTab_Options && PackChangePending())
        Apply();
    m_tab = tab;
    m_dragIndex = -1;
    m_scroll[eView_Details] = 0.0f;
    m_scrollTarget[eView_Details] = 0.0f;
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
    m_flashKind = target.kind;
    m_flashIndex = target.index;
    m_flashTime = System::processTimeInMilliSecs();
    switch (target.kind)
    {
    case eTarget_Hide:
        ToggleHidden();
        break;
    case eTarget_Tab:
        SetTab(target.index);
        break;
    case eTarget_Cancel:
        Cancel();
        break;
    case eTarget_Apply:
        if (HasPendingChanges())
        {
            gConsoleUIController.PlayUISFX(SoundEvents::UI_PRESS);
            Apply();
        }
        break;
    case eTarget_Done:
        Done();
        break;
    case eTarget_Toggle:
        SetShadersOn(!ShadersOn(*m_workingSettings));
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
        gConsoleUIController.PlayUISFX(SoundEvents::UI_PRESS);
        ShowFileDialog(false);
        break;
    case eTarget_Export:
        gConsoleUIController.PlayUISFX(SoundEvents::UI_PRESS);
        ShowFileDialog(true);
        break;
    case eTarget_Reset:
        OpenModal(eModal_Reset);
        break;
    case eTarget_Cell:
    {
        const Cell& cell = m_cells[static_cast<size_t>(target.index)];
        if (cell.kind == eCell_Link)
            OpenScreen(cell.name);
        else if (cell.kind == eCell_Profile)
            CycleProfile(1);
        else if (cell.kind == eCell_Option)
            CycleOption(cell.name, 1);
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
    m_flashKind = target.kind;
    m_flashIndex = target.index;
    m_flashTime = System::processTimeInMilliSecs();
    switch (target.kind)
    {
    case eTarget_Toggle:
        SetShadersOn(!ShadersOn(*m_workingSettings));
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
    m_flashKind = target.kind;
    m_flashIndex = target.index;
    m_flashTime = System::processTimeInMilliSecs();
    if (target.kind == eTarget_Cell)
    {
        const Cell& cell = m_cells[static_cast<size_t>(target.index)];
        if (cell.kind != eCell_Option)
            return;
        ShaderPackManager& manager = ShaderPackManager::Get();
        const ShaderOption* option = manager.Options().Find(cell.name);
        if (option == nullptr || manager.Options().Value(cell.name) == option->defaultValue)
            return;
        if (manager.SetOption(cell.name, option->defaultValue))
        {
            gConsoleUIController.PlayUISFX(SoundEvents::UI_SCROLL);
            BuildCells();
        }
        return;
    }
    if (target.kind == eTarget_Setting)
    {
        int defaultIndex = SettingIndex(ShaderSettings(), target.index);
        if (SettingIndex(*m_workingSettings, target.index) == defaultIndex)
            return;
        SetSettingIndex(*m_workingSettings, target.index, defaultIndex);
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
    Done();
}

void UIScene_ShaderMenu::Click(float x, float y, unsigned int clicks)
{
    int index = TargetAt(x, y);
    if (index < 0)
    {
        if ((clicks & C_4JInput::EPcMenuClick_Secondary) != 0)
        {
            gConsoleUIController.PlayUISFX(SoundEvents::UI_BACK);
            Back();
        }
        return;
    }
    Target target = m_targets[static_cast<size_t>(index)];
    if (m_modal != eModal_None)
    {
        if ((clicks & C_4JInput::EPcMenuClick_Secondary) != 0)
            CloseModal(false);
        else
            CloseModal(target.index == 0);
        return;
    }
    SetFocus(target, false);
    bool changeable = target.kind == eTarget_Toggle || target.kind == eTarget_Setting
                      || (target.kind == eTarget_Cell && CellChangeable(target.index));
    if ((clicks & C_4JInput::EPcMenuClick_Quick) != 0)
    {
        if (Resettable(target))
            ResetTarget(target);
        else
            Activate(target);
        return;
    }
    if ((clicks & C_4JInput::EPcMenuClick_Secondary) != 0)
    {
        if (changeable)
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
            m_flashKind = target.kind;
            m_flashIndex = target.index;
            m_flashTime = System::processTimeInMilliSecs();
            DragSlider(x);
            return;
        }
    }
    Activate(target);
}

void UIScene_ShaderMenu::HandlePointer()
{
    float pointerX = 0.0f;
    float pointerY = 0.0f;
    bool hasPointer = g_pInput->GetMenuPointer(m_playerId, &pointerX, &pointerY) && m_unit > 0.0f;
    float x = (pointerX - m_originX) / m_unit;
    float y = (pointerY - m_originY) / m_unit;
    unsigned int clicks = g_pInput->ConsumeMenuClicks(m_playerId);
    int wheel = g_pInput->ConsumeMenuWheel(m_playerId);
    if (m_hidden)
    {
        if (clicks != 0)
            ToggleHidden();
        return;
    }

    if (hasPointer && (pointerX != m_pointerX || pointerY != m_pointerY))
    {
        m_pointerX = pointerX;
        m_pointerY = pointerY;
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
        if ((SDL_GetMouseState(NULL, NULL) & SDL_BUTTON_LMASK) != 0 && hasPointer)
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
        if (hasPointer)
        {
            int viewX0 = 0;
            int viewY0 = 0;
            int viewX1 = 0;
            int viewY1 = 0;
            ViewRect(eView_Details, &viewX0, &viewY0, &viewX1, &viewY1);
            if (x >= viewX0 - 6 && x < viewX1 + 6 && y >= viewY0 - 6 && y < viewY1 + 6)
            {
                view = eView_Details;
                step = kLineHeight * 3.0f;
            }
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
        m_scrollTarget[view] = std::max(0.0f, std::min(m_scrollTarget[view], MaxScroll(view)));
        m_scroll[view] = Approach(m_scroll[view], m_scrollTarget[view], elapsed, 70.0f);
        if (std::fabs(m_scroll[view] - m_scrollTarget[view]) < 0.05f)
            m_scroll[view] = m_scrollTarget[view];
    }
}

void UIScene_ShaderMenu::handleInput(int, int action, bool repeat, bool pressed, bool released, bool& handled)
{
    gConsoleUIController.AnimateKeyPress(m_playerId, action, repeat, pressed, released);
    if (!pressed)
        return;
    BuildTargets();
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
        return;
    }

    int index = FindTarget(CurrentFocus());
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
        if (index >= 0 && ChangesSideways(m_targets[static_cast<size_t>(index)]))
        {
            Target target = m_targets[static_cast<size_t>(index)];
            if (target.kind == eTarget_Toggle)
                SetShadersOn(step < 0);
            else
                Change(target, step);
        }
        else
        {
            Move(step, 0);
        }
        break;
    }
    case EControllerActions_MenuOk:
        if (!repeat && index >= 0)
        {
            Target target = m_targets[static_cast<size_t>(index)];
            if (g_pInput->MenuQuickMoveHeld(m_playerId) && Resettable(target))
                ResetTarget(target);
            else
                Activate(target);
        }
        break;
    case EControllerActions_MenuOtherStickUp:
    case EControllerActions_MenuOtherStickDown:
        ScrollView(eView_Details, action == EControllerActions_MenuOtherStickUp ? -kLineHeight * 2.0f
                                                                                 : kLineHeight * 2.0f);
        break;
    case EControllerActions_MenuPageUp:
    case EControllerActions_MenuPageDown:
        if (index >= 0)
        {
            Target target = m_targets[static_cast<size_t>(index)];
            Change(target, action == EControllerActions_MenuPageUp ? -1 : 1);
        }
        break;
    case EControllerActions_MenuX:
        if (!repeat && HasPendingChanges())
        {
            gConsoleUIController.PlayUISFX(SoundEvents::UI_PRESS);
            Apply();
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
            Target target = m_targets[static_cast<size_t>(index)];
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
    if (on == ShadersOn(*m_workingSettings))
        return;
    if (on && m_workingSettings->shaderPack == ShaderSettings::kPackOff)
    {
        gConsoleUIController.PlayUISFX(SoundEvents::UI_BACK);
        Notify(L"Choose a shader pack to turn shaders on", kTextLight);
        return;
    }
    gConsoleUIController.PlayUISFX(SoundEvents::UI_SCROLL);
    m_workingSettings->enableShaders = on;
}

void UIScene_ShaderMenu::RefreshPacks(bool force)
{
    ShaderPackManager& manager = ShaderPackManager::Get();
    std::vector<std::string> packs = manager.ListPacks();
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
    if (m_workingSettings->shaderPack == ShaderSettings::kPackInternal)
    {
        focus = Focus{eTarget_Pack, kPackInternal};
    }
    else
    {
        std::vector<std::string>::const_iterator found =
            std::find(m_packs.begin(), m_packs.end(), m_workingSettings->shaderPack);
        focus = found != m_packs.end() ? Focus{eTarget_Pack, static_cast<int>(found - m_packs.begin())}
                                       : Focus{eTarget_Toggle, 0};
    }
    BuildTargets();
    int index = FindTarget(focus);
    if (index >= 0)
        EnsureVisible(m_targets[static_cast<size_t>(index)]);
    m_scroll[eView_List] = m_scrollTarget[eView_List];
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
    if (name == m_appliedSettings->shaderPack && m_appliedSettings->UsesPack() && !PackChangePending()
        && ShaderPackManager::Get().ActivePack() != nullptr)
    {
        gConsoleUIController.PlayUISFX(SoundEvents::UI_PRESS);
        SetTab(eTab_Options);
        return;
    }
    gConsoleUIController.PlayUISFX(SoundEvents::UI_PRESS);
    m_workingSettings->shaderPack = name;
    m_workingSettings->enableShaders = true;
}

void UIScene_ShaderMenu::BuildCells()
{
    m_cells.clear();
    m_columns = 2;
    ShaderPackManager& manager = ShaderPackManager::Get();
    if (manager.ActivePack() == nullptr)
    {
        m_screen.clear();
        m_history.clear();
        return;
    }
    const ShaderPackProperties& properties = manager.Properties();
    const ShaderPackOptions& options = manager.Options();
    if (!m_screen.empty() && properties.Screen(m_screen) == nullptr)
    {
        m_screen.clear();
        m_history.clear();
    }

    std::vector<std::string> items;
    const ShaderScreen* screen = properties.Screen(m_screen);
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
    for (const ShaderScreen& other : properties.Screens())
    {
        for (const std::string& item : other.items)
            listed.insert(item);
    }

    for (const std::string& item : items)
    {
        if (item.empty() || item == "<empty>")
        {
            m_cells.push_back(Cell{eCell_Empty, std::string()});
            continue;
        }
        if (item == "<profile>")
        {
            m_cells.push_back(Cell{properties.Profiles().empty() ? eCell_Empty : eCell_Profile, std::string()});
            continue;
        }
        if (item.size() > 2 && item.front() == '[' && item.back() == ']')
        {
            std::string name = item.substr(1, item.size() - 2);
            m_cells.push_back(Cell{properties.Screen(name) != nullptr ? eCell_Link : eCell_Empty, name});
            continue;
        }
        if (item == "*")
        {
            for (const ShaderOption& option : options.Options())
            {
                if (listed.count(option.name) == 0 && (option.IsBoolean() || option.values.size() > 1))
                    m_cells.push_back(Cell{eCell_Option, option.name});
            }
            continue;
        }
        const ShaderOption* option = options.Find(item);
        bool usable = option != nullptr && (option->IsBoolean() || !option->values.empty());
        m_cells.push_back(Cell{usable ? eCell_Option : eCell_Empty, item});
    }
    while (!m_cells.empty() && m_cells.back().kind == eCell_Empty)
        m_cells.pop_back();
}

void UIScene_ShaderMenu::OpenScreen(const std::string& screen)
{
    gConsoleUIController.PlayUISFX(SoundEvents::UI_PRESS);
    Page page = {m_screen, m_focus[eTab_Options], m_scroll[eView_Grid]};
    m_history.push_back(page);
    m_screen = screen;
    m_focus[eTab_Options] = Focus{eTarget_Cell, -1};
    m_scroll[eView_Grid] = 0.0f;
    m_scrollTarget[eView_Grid] = 0.0f;
    m_mouseScroll[eView_Grid] = false;
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
    m_scroll[eView_Grid] = page.scroll;
    m_scrollTarget[eView_Grid] = page.scroll;
    m_mouseScroll[eView_Grid] = false;
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
    const ShaderOption* option = ShaderPackManager::Get().Options().Find(cell.name);
    return option != nullptr && !option->ambiguous && (option->IsBoolean() || option->values.size() > 1);
}

bool UIScene_ShaderMenu::IsSlider(const Cell& cell) const
{
    if (cell.kind != eCell_Option)
        return false;
    ShaderPackManager& manager = ShaderPackManager::Get();
    const ShaderOption* option = manager.Options().Find(cell.name);
    return option != nullptr && !option->IsBoolean() && option->values.size() > 1
           && manager.Properties().IsSlider(cell.name);
}

int UIScene_ShaderMenu::SliderPosition(const Cell& cell, int* count) const
{
    ShaderPackManager& manager = ShaderPackManager::Get();
    const ShaderOption* option = manager.Options().Find(cell.name);
    *count = option != nullptr ? static_cast<int>(option->values.size()) : 0;
    if (option == nullptr)
        return 0;
    std::vector<std::string>::const_iterator found =
        std::find(option->values.begin(), option->values.end(), manager.Options().Value(cell.name));
    return found == option->values.end() ? 0 : static_cast<int>(found - option->values.begin());
}

void UIScene_ShaderMenu::SetSliderPosition(const Cell& cell, int position)
{
    ShaderPackManager& manager = ShaderPackManager::Get();
    const ShaderOption* option = manager.Options().Find(cell.name);
    if (option == nullptr || position < 0 || position >= static_cast<int>(option->values.size()))
        return;
    std::string name = cell.name;
    if (manager.SetOption(name, option->values[static_cast<size_t>(position)]))
        BuildCells();
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
    std::wstring value = CellValueText(cell.kind == eCell_Profile, cell.name, &colour);
    int width = ValueBoxWidth(*minecraft->mFont, value, cellWidth, IsSlider(cell));
    *x1 = cellX0 + cellWidth - 2;
    *x0 = *x1 - width;
}

std::string UIScene_ShaderMenu::AppliedValue(const ShaderOption& option) const
{
    std::map<std::string, std::string>::const_iterator found = m_appliedValues.find(option.name);
    return found != m_appliedValues.end() ? found->second : option.defaultValue;
}

bool UIScene_ShaderMenu::OptionModified(const std::string& name) const
{
    ShaderPackManager& manager = ShaderPackManager::Get();
    const ShaderOption* option = manager.Options().Find(name);
    return option != nullptr && manager.Options().Value(name) != AppliedValue(*option);
}

void UIScene_ShaderMenu::CycleOption(const std::string& name, int step)
{
    ShaderPackManager& manager = ShaderPackManager::Get();
    const ShaderOption* option = manager.Options().Find(name);
    if (option == nullptr || option->ambiguous)
        return;
    std::string value = manager.Options().Value(name);
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
        if (manager.Properties().IsSlider(name))
            index = std::max(0, std::min(index + step, count - 1));
        else
            index = ((index + step) % count + count) % count;
        next = option->values[static_cast<size_t>(index)];
    }
    if (next == value)
        return;
    std::string key = name;
    if (manager.SetOption(key, next))
    {
        gConsoleUIController.PlayUISFX(SoundEvents::UI_SCROLL);
        BuildCells();
    }
}

void UIScene_ShaderMenu::CycleProfile(int step)
{
    ShaderPackManager& manager = ShaderPackManager::Get();
    const std::vector<ShaderProfile>& profiles = manager.Properties().Profiles();
    int count = static_cast<int>(profiles.size());
    if (count == 0)
        return;
    std::string active = manager.Properties().ActiveProfile(manager.Options());
    int index = -1;
    for (int candidate = 0; candidate < count; ++candidate)
    {
        if (profiles[static_cast<size_t>(candidate)].name == active)
            index = candidate;
    }
    if (index < 0)
        index = step > 0 ? 0 : count - 1;
    else
        index = ((index + step) % count + count) % count;
    std::string name = profiles[static_cast<size_t>(index)].name;
    manager.ApplyProfile(name);
    gConsoleUIController.PlayUISFX(SoundEvents::UI_SCROLL);
    BuildCells();
}

void UIScene_ShaderMenu::ChangeSetting(int setting, int step)
{
    int count = SettingCount(setting);
    int index = SettingIndex(*m_workingSettings, setting);
    if (SettingIsSlider(setting))
        index = std::max(0, std::min(index + step, count - 1));
    else
        index = ((index + step) % count + count) % count;
    if (index == SettingIndex(*m_workingSettings, setting))
        return;
    SetSettingIndex(*m_workingSettings, setting, index);
    gConsoleUIController.PlayUISFX(SoundEvents::UI_SCROLL);
}

bool UIScene_ShaderMenu::SettingModified(int setting) const
{
    return SettingIndex(*m_workingSettings, setting) != SettingIndex(*m_appliedSettings, setting);
}

void UIScene_ShaderMenu::SnapshotOptions()
{
    m_appliedOptions = ShaderPackManager::Get().OptionValues();
    m_appliedValues.clear();
    std::istringstream stream(m_appliedOptions);
    std::string line;
    while (std::getline(stream, line))
    {
        size_t equals = line.find('=');
        if (equals != std::string::npos)
            m_appliedValues[line.substr(0, equals)] = line.substr(equals + 1);
    }
}

bool UIScene_ShaderMenu::PackChangePending() const
{
    return m_workingSettings->shaderPack != m_appliedSettings->shaderPack
           || m_workingSettings->enableShaders != m_appliedSettings->enableShaders;
}

bool UIScene_ShaderMenu::SettingsChangePending() const
{
    for (int setting = 0; setting < eSetting_Count; ++setting)
    {
        if (SettingModified(setting))
            return true;
    }
    return false;
}

bool UIScene_ShaderMenu::OptionsChangePending() const
{
    ShaderPackManager& manager = ShaderPackManager::Get();
    return manager.ActivePack() != nullptr && manager.OptionValues() != m_appliedOptions;
}

bool UIScene_ShaderMenu::HasPendingChanges() const
{
    return PackChangePending() || SettingsChangePending() || OptionsChangePending();
}

void UIScene_ShaderMenu::Apply()
{
    ShaderPackManager& manager = ShaderPackManager::Get();
    bool packChanged = m_workingSettings->shaderPack != m_appliedSettings->shaderPack;
    manager.EditSettings() = *m_workingSettings;
    if (packChanged)
    {
        manager.SelectPack(m_workingSettings->shaderPack);
    }
    else
    {
        manager.SaveSettings();
        if (manager.ActivePack() != nullptr)
            manager.SaveOptions();
    }
    ShaderPipeline::Get().RequestReload();

    *m_appliedSettings = manager.Settings();
    *m_workingSettings = *m_appliedSettings;
    SnapshotOptions();
    if (packChanged)
    {
        m_screen.clear();
        m_history.clear();
        m_focus[eTab_Options] = Focus{eTarget_Cell, -1};
        m_scroll[eView_Grid] = 0.0f;
        m_scrollTarget[eView_Grid] = 0.0f;
    }
    sSummaries.erase(m_appliedSettings->shaderPack);
    BuildCells();

    m_awaitingReload = false;
    if (m_appliedSettings->UsesPack() && manager.ActivePack() == nullptr)
    {
        Notify(L"The shader pack could not be loaded. See the log for details.", kTextError);
    }
    else if (!ShadersOn(*m_appliedSettings))
    {
        Notify(L"Shaders are off", kTextLight);
    }
    else if (Minecraft::GetInstance()->mLevel != NULL)
    {
        m_awaitingReload = true;
        m_reloadTime = System::processTimeInMilliSecs();
        Notify(L"Reloading shaders...", kTextLight);
    }
    else
    {
        Notify(L"Applied. The shader pack loads when you enter a world.", kTextLight);
    }
}

void UIScene_ShaderMenu::Cancel()
{
    ShaderPackManager& manager = ShaderPackManager::Get();
    if (OptionsChangePending())
        manager.LoadOptionValues(m_appliedOptions);
    *m_workingSettings = *m_appliedSettings;
    gConsoleUIController.PlayUISFX(SoundEvents::UI_BACK);
    navigateBack();
}

void UIScene_ShaderMenu::Done()
{
    if (HasPendingChanges())
        Apply();
    navigateBack();
}

void UIScene_ShaderMenu::CheckReload()
{
    if (!m_awaitingReload)
        return;
    ShaderPipeline& pipeline = ShaderPipeline::Get();
    long long now = System::processTimeInMilliSecs();
    if (pipeline.ReloadRequested())
    {
        if (now - m_reloadTime > kReloadWaitMilliseconds)
            m_awaitingReload = false;
        return;
    }
    m_awaitingReload = false;
    sSummaries.erase(m_appliedSettings->shaderPack);
    if (!pipeline.Enabled())
    {
        Notify(L"The shader pack could not be loaded. See the log for details.", kTextError);
        return;
    }
    size_t problems = pipeline.Errors().size();
    if (problems == 0)
        Notify(L"Shaders reloaded", kTextOn);
    else
        Notify(L"Shaders reloaded with " + std::to_wstring(problems) + (problems == 1 ? L" problem" : L" problems")
                   + L"; see the pack's details",
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
    if (manager.ActivePack() == nullptr)
        return;
    {
        std::lock_guard<std::mutex> lock(sDialog.mutex);
        sDialog.owner = this;
        sDialog.ready = false;
    }
    std::error_code error;
    std::filesystem::create_directories(manager.PacksFolder(), error);
    if (save)
    {
        std::string name = manager.ActivePack()->Name();
        if (name.size() > 4 && Lower(name.substr(name.size() - 4)) == ".zip")
            name.resize(name.size() - 4);
        std::string location = (manager.PacksFolder() / std::filesystem::u8path(name + " settings.txt")).u8string();
        SDL_ShowSaveFileDialog(&DialogCallback, reinterpret_cast<void*>(static_cast<intptr_t>(eDialog_Export)),
                               PcWindowHandle(), kFilters, 2, location.c_str());
    }
    else
    {
        std::string location = manager.PacksFolder().u8string();
        SDL_ShowOpenFileDialog(&DialogCallback, reinterpret_cast<void*>(static_cast<intptr_t>(eDialog_Import)),
                               PcWindowHandle(), kFilters, 2, location.c_str(), false);
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
    {
        Notify(L"A file window could not be opened: " + Wide(error), kTextError);
        return;
    }
    if (!chosen)
        return;
    if (kind == eDialog_Import)
        ImportSettings(path);
    else if (kind == eDialog_Export)
        ExportSettings(path);
}

void UIScene_ShaderMenu::ImportSettings(const std::string& path)
{
    ShaderPackManager& manager = ShaderPackManager::Get();
    if (manager.ActivePack() == nullptr)
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
    if (!empty && !HasPackOptions(text))
    {
        Notify(name + L" has no settings for this shader pack", kTextError);
        return;
    }
    manager.LoadOptionValues(text);
    BuildCells();
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
    if (!stream || !(stream << m_appliedOptions))
    {
        Notify(L"Could not write " + name, kTextError);
        return;
    }
    LOG_INFO(sLog, "Exported shader settings to %s", file.u8string().c_str());
    Notify(L"Exported the applied settings to " + name, kTextYellow);
}

void UIScene_ShaderMenu::HandleDrops()
{
    if (!m_dropComplete)
        return;
    m_dropComplete = false;
    std::vector<std::string> files;
    files.swap(m_dropped);
    if (files.empty() || m_modal != eModal_None)
        return;
    if (m_hidden)
        ToggleHidden();

    ShaderPackManager& manager = ShaderPackManager::Get();
    if (m_tab == eTab_Options && manager.ActivePack() != nullptr
        && !ShaderPack::IsShaderPack(std::filesystem::u8path(files[0])))
    {
        if (files.size() != 1)
        {
            Notify(L"Drop one settings file at a time to import it", kTextError);
            return;
        }
        ImportSettings(files[0]);
        return;
    }

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
    if (m_tab != eTab_Packs)
        SetTab(eTab_Packs);
    if (added == 0)
    {
        Notify(files.size() == 1 ? L"Could not add " + failure : std::wstring(L"Those files are not shader packs"),
               kTextError);
        return;
    }
    if (added == 1)
    {
        std::vector<std::string>::const_iterator found = std::find(m_packs.begin(), m_packs.end(), lastName);
        if (found != m_packs.end())
        {
            int index = static_cast<int>(found - m_packs.begin());
            if (PackUsable(index))
            {
                m_workingSettings->shaderPack = lastName;
                m_workingSettings->enableShaders = true;
            }
            m_focus[eTab_Packs] = Focus{eTarget_Pack, index};
            BuildTargets();
            int target = FindTarget(m_focus[eTab_Packs]);
            if (target >= 0)
                EnsureVisible(m_targets[static_cast<size_t>(target)]);
        }
        Notify(L"Added shader pack " + PackDisplayName(lastName), kTextYellow);
    }
    else
    {
        Notify(L"Added " + std::to_wstring(added) + L" shader packs", kTextYellow);
    }
    gConsoleUIController.PlayUISFX(SoundEvents::UI_PRESS);
}

void UIScene_ShaderMenu::ResetAllOptions()
{
    ShaderPackManager& manager = ShaderPackManager::Get();
    bool changed = !manager.OptionValues().empty();
    manager.ResetOptions();
    BuildCells();
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
    m_modalFocus = modal == eModal_Reset ? 1 : 0;
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
    BuildTargets();
}

void UIScene_ShaderMenu::Notify(const std::wstring& text, unsigned int colour)
{
    m_notice = text;
    m_noticeColour = colour;
    m_noticeTime = System::processTimeInMilliSecs();
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
    ShaderPackManager& manager = ShaderPackManager::Get();
    const ShaderPackLang& lang = manager.Lang();
    std::wstring title = Wide(lang.ScreenLabel(std::string()));
    if (title.empty() && manager.ActivePack() != nullptr)
        title = PackDisplayName(manager.ActivePack()->Name());
    std::vector<std::wstring> parts(1, title);
    for (const Page& page : m_history)
    {
        if (!page.screen.empty())
            parts.push_back(Wide(lang.ScreenLabel(page.screen)));
    }
    if (!m_screen.empty())
        parts.push_back(Wide(lang.ScreenLabel(m_screen)));
    return Join(parts, FormatReset() + L" > ");
}

UIScene_ShaderMenu::Text UIScene_ShaderMenu::DescribePack(int index) const
{
    Text text;
    auto add = [&text](const std::wstring& line, unsigned int colour, int gap) {
        text.push_back(Paragraph{line, colour, gap});
    };
    std::string name = PackName(index);
    bool applied = name == m_appliedSettings->shaderPack && m_appliedSettings->enableShaders;
    bool selected = name == m_workingSettings->shaderPack && m_workingSettings->enableShaders;
    ShaderPackManager& manager = ShaderPackManager::Get();

    if (index == kPackInternal)
    {
        add(L"(internal)", kTextYellow, 0);
        if (applied)
            add(L"In use", kTextOn, 2);
        else if (selected)
            add(L"Selected. Apply to use it.", kTextWarning, 2);
        add(L"Runs the shader pipeline with built-in programs that match the standard look. Useful for checking "
            L"that the pipeline itself works.",
            kTextBody, 6);
        return text;
    }

    const PackSummary& summary = SummaryFor(name);
    add(PackDisplayName(name), kTextYellow, 0);
    if (!summary.usable)
        add(L"Can't be used: " + summary.problem, kTextError, 2);
    else if (applied && manager.ActivePack() == nullptr)
        add(L"Failed to load. See the log for details.", kTextError, 2);
    else if (applied && !PackChangePending())
        add(L"In use", kTextOn, 2);
    else if (selected)
        add(L"Selected. Apply to load it.", kTextWarning, 2);
    else if (name == m_appliedSettings->shaderPack && !m_appliedSettings->enableShaders)
        add(L"Chosen, but shaders are off", kTextGrey, 2);

    if (!summary.profiles.empty())
    {
        add(L"Profiles", kTextWhite, 7);
        add(Join(summary.profiles, FormatReset() + L", "), kTextBody, 0);
    }
    if (summary.totalPages > 0)
    {
        add(L"Settings", kTextWhite, 7);
        add(std::to_wstring(summary.mainPages) + (summary.mainPages == 1 ? L" main page, " : L" main pages, ")
                + std::to_wstring(summary.totalPages)
                + (summary.totalPages == 1 ? L" page in total" : L" pages in total"),
            kTextBody, 0);
    }
    ShaderPack* active = manager.ActivePack();
    if (active != nullptr && active->Name() == name)
    {
        int booleans = 0;
        int values = 0;
        for (const ShaderOption& option : manager.Options().Options())
        {
            if (option.IsBoolean())
                ++booleans;
            else if (option.values.size() > 1)
                ++values;
        }
        add(L"Options", kTextWhite, 7);
        add(std::to_wstring(booleans) + L" on/off and " + std::to_wstring(values) + L" value options", kTextBody, 0);
    }
    if (!summary.worlds.empty())
    {
        add(L"Worlds", kTextWhite, 7);
        add(Join(summary.worlds, L", "), kTextBody, 0);
    }
    add(L"File", kTextWhite, 7);
    add(Wide(name) + L" (" + (summary.folder ? std::wstring(L"folder, ") : std::wstring()) + SizeText(summary.size)
            + L")",
        kTextBody, 0);

    ShaderPipeline& pipeline = ShaderPipeline::Get();
    if (applied && m_appliedSettings->UsesPack() && pipeline.Enabled() && !pipeline.ReloadRequested())
    {
        std::vector<std::string> problems = manager.Errors();
        problems.insert(problems.end(), pipeline.Errors().begin(), pipeline.Errors().end());
        if (!problems.empty())
        {
            add(problems.size() == 1 ? std::wstring(L"1 problem") : std::to_wstring(problems.size()) + L" problems",
                kTextError, 7);
            size_t shown = std::min<size_t>(problems.size(), 20);
            for (size_t problem = 0; problem < shown; ++problem)
                add(Wide(problems[problem]), kTextProblem, problem == 0 ? 0 : 2);
            if (problems.size() > shown)
                add(L"and " + std::to_wstring(problems.size() - shown) + L" more in the log", kTextGrey, 2);
        }
    }
    return text;
}

UIScene_ShaderMenu::Text UIScene_ShaderMenu::DescribeCell(int index) const
{
    Text text;
    auto add = [&text](const std::wstring& line, unsigned int colour, int gap) {
        text.push_back(Paragraph{line, colour, gap});
    };
    if (index < 0 || index >= static_cast<int>(m_cells.size()))
        return text;
    const Cell& cell = m_cells[static_cast<size_t>(index)];
    ShaderPackManager& manager = ShaderPackManager::Get();
    const ShaderPackLang& lang = manager.Lang();
    if (cell.kind == eCell_Link)
    {
        add(Wide(lang.ScreenLabel(cell.name)), kTextYellow, 0);
        std::vector<std::wstring> sentences = Sentences(Wide(lang.ScreenComment(cell.name)));
        for (size_t sentence = 0; sentence < sentences.size(); ++sentence)
            add(sentences[sentence], kTextBody, sentence == 0 ? 3 : 0);
        if (sentences.empty())
            add(L"Opens this page of settings.", kTextGrey, 3);
        return text;
    }
    if (cell.kind == eCell_Profile)
    {
        add(L"Profile", kTextYellow, 0);
        std::wstring comment = Wide(lang.Get("profile.comment", std::string()));
        if (comment.empty())
            comment = L"Sets many options at once to a preset chosen by the shader pack. Changing an option the "
                      L"profile controls shows Custom.";
        std::vector<std::wstring> sentences = Sentences(comment);
        for (size_t sentence = 0; sentence < sentences.size(); ++sentence)
            add(sentences[sentence], kTextBody, sentence == 0 ? 3 : 0);
        std::vector<std::wstring> labels;
        for (const ShaderProfile& profile : manager.Properties().Profiles())
            labels.push_back(Wide(lang.ProfileLabel(profile.name)));
        add(L"Profiles: " + Join(labels, FormatReset() + L", "), kTextGrey, 5);
        return text;
    }
    const ShaderOption* option = manager.Options().Find(cell.name);
    if (option == nullptr)
        return text;
    add(Wide(lang.OptionLabel(option->name)), kTextYellow, 0);
    std::vector<std::wstring> sentences = Sentences(Wide(lang.OptionComment(*option)));
    for (size_t sentence = 0; sentence < sentences.size(); ++sentence)
        add(sentences[sentence], kTextBody, sentence == 0 ? 3 : 0);
    std::wstring footer = L"Default: " + Wide(lang.ValueLabel(*option, option->defaultValue));
    if (!option->IsBoolean() && option->values.size() < 2)
        return text;
    if (option->ambiguous)
        add(L"The pack sets this option in more than one way, so it can't be changed here.", kTextGrey, 5);
    else if (OptionModified(option->name))
        add(footer + FormatReset() + L"   Not applied yet", kTextModified, 5);
    else
        add(footer, kTextGrey, 5);
    return text;
}

UIScene_ShaderMenu::Text UIScene_ShaderMenu::DescribeSetting(int setting) const
{
    Text text;
    auto add = [&text](const std::wstring& line, unsigned int colour, int gap) {
        text.push_back(Paragraph{line, colour, gap});
    };
    add(kSettingNames[setting], kTextYellow, 0);
    std::wstring body;
    std::wstring macro;
    switch (setting)
    {
    case eSetting_Antialiasing:
        body = L"Smooths jagged edges with an FXAA pass after the shader pack's final pass.";
        macro = L"MC_FXAA_LEVEL";
        break;
    case eSetting_NormalMap:
        body = L"Tells shader packs whether normal maps may be used. Off gives packs a flat normal map.";
        macro = L"MC_NORMAL_MAP";
        break;
    case eSetting_SpecularMap:
        body = L"Tells shader packs whether specular maps may be used. Off gives packs an empty specular map.";
        macro = L"MC_SPECULAR_MAP";
        break;
    case eSetting_RenderQuality:
        body = L"Scales the resolution the shader pack renders at. Higher is sharper at a higher GPU cost.";
        macro = L"MC_RENDER_QUALITY";
        break;
    case eSetting_ShadowQuality:
        body = L"Scales the shadow map size the shader pack asks for. 2x doubles it for sharper shadows at a higher "
               L"GPU cost; 0.5x halves it.";
        macro = L"MC_SHADOW_QUALITY";
        break;
    case eSetting_HandDepth:
        body = L"Scales how deep the held item sits in the depth buffer, for packs with depth-based effects such as "
               L"depth of field.";
        macro = L"MC_HAND_DEPTH";
        break;
    case eSetting_OldHandLight:
        body = L"ON lights the world with the brighter of the items in either hand. Default follows the shader pack.";
        break;
    case eSetting_OldLighting:
        body = L"ON keeps the fixed brightness per block face from the standard renderer. OFF leaves face shading to "
               L"the shader pack. Default follows the shader pack.";
        break;
    default:
        break;
    }
    add(body, kTextBody, 3);
    std::vector<std::wstring> values;
    for (int index = 0; index < SettingCount(setting); ++index)
        values.push_back(SettingLabel(setting, index));
    add(L"Values", kTextWhite, 7);
    add(Join(values, L", "), kTextBody, 0);
    if (!macro.empty())
    {
        add(L"Shader packs read this as", kTextWhite, 7);
        add(macro, kTextBody, 0);
    }
    add(L"Default: " + SettingLabel(setting, SettingIndex(ShaderSettings(), setting)), kTextGrey, 7);
    if (SettingModified(setting))
        add(L"Not applied yet", kTextModified, 2);
    return text;
}

UIScene_ShaderMenu::Text UIScene_ShaderMenu::DescribeFocus() const
{
    Text text;
    auto add = [&text](const std::wstring& line, unsigned int colour, int gap) {
        text.push_back(Paragraph{line, colour, gap});
    };
    const Focus& focus = m_focus[m_tab];
    switch (focus.kind)
    {
    case eTarget_Hide:
        add(L"Hide Menu", kTextYellow, 0);
        add(L"Hides this menu so you can see the world with the shader pack. Press any button or click to bring it "
            L"back.",
            kTextBody, 3);
        break;
    case eTarget_Tab:
        if (focus.index == eTab_Packs)
        {
            add(L"Shader Packs", kTextYellow, 0);
            add(L"Choose the shader pack the world is drawn with, or turn shaders off.", kTextBody, 3);
        }
        else if (focus.index == eTab_Options)
        {
            add(L"Pack Settings", kTextYellow, 0);
            add(L"Change the options of the shader pack in use: its profile and every page of settings it offers.",
                kTextBody, 3);
            if (!TabEnabled(eTab_Options))
                add(L"Choose a shader pack first.", kTextGrey, 5);
        }
        else
        {
            add(L"Shader Settings", kTextYellow, 0);
            add(L"Settings that apply to every shader pack, such as antialiasing and render quality.", kTextBody, 3);
        }
        break;
    case eTarget_Cancel:
        add(L"Cancel", kTextYellow, 0);
        add(L"Closes this menu and throws away the changes that have not been applied.", kTextBody, 3);
        break;
    case eTarget_Apply:
        add(L"Apply", kTextYellow, 0);
        add(L"Loads the chosen shader pack with its settings now, so you can see the result without closing the menu.",
            kTextBody, 3);
        if (!HasPendingChanges())
            add(L"Nothing has changed.", kTextGrey, 5);
        break;
    case eTarget_Done:
        add(L"Done", kTextYellow, 0);
        add(L"Applies any changes and closes this menu.", kTextBody, 3);
        break;
    case eTarget_Toggle:
        add(ShadersOn(*m_workingSettings) ? L"Shaders: ON" : L"Shaders: OFF", kTextYellow, 0);
        add(L"Turns shader packs on or off. The chosen pack is remembered while shaders are off.", kTextBody, 3);
        if (m_workingSettings->shaderPack == ShaderSettings::kPackOff)
            add(L"Choose a shader pack below to turn shaders on.", kTextGrey, 5);
        else if (ShadersOn(*m_workingSettings) != ShadersOn(*m_appliedSettings))
            add(L"Not applied yet", kTextModified, 5);
        break;
    case eTarget_Pack:
        return DescribePack(focus.index);
    case eTarget_OpenFolder:
        add(L"Open Shader Pack Folder", kTextYellow, 0);
        add(L"Opens " + Wide(ShaderPackManager::Get().PacksFolder().u8string()) + L".", kTextBody, 3);
        add(L"Put shader packs there as .zip files or folders, or drag them onto this window. The list updates by "
            L"itself.",
            kTextBody, 3);
        break;
    case eTarget_Download:
        add(L"Download Shaders", kTextYellow, 0);
        add(L"Opens modrinth.com/shaders in your web browser, where many shader packs made for OptiFine and Iris can "
            L"be downloaded.",
            kTextBody, 3);
        break;
    case eTarget_Back:
        add(L"Back", kTextYellow, 0);
        add(L"Returns to the previous page of settings.", kTextBody, 3);
        break;
    case eTarget_Import:
        add(L"Import Settings", kTextYellow, 0);
        add(L"Loads the options in a settings file, such as one exported from this menu, OptiFine or Iris. Dragging "
            L"a settings file onto this window does the same.",
            kTextBody, 3);
        break;
    case eTarget_Export:
        add(L"Export Settings", kTextYellow, 0);
        add(L"Saves the applied options of this shader pack to a file, to share them or keep a copy.", kTextBody, 3);
        break;
    case eTarget_Reset:
        add(L"Reset", kTextYellow, 0);
        add(L"Puts every option of this shader pack back to its default, after asking first.", kTextBody, 3);
        break;
    case eTarget_Cell:
        return DescribeCell(focus.index);
    case eTarget_Setting:
        return DescribeSetting(focus.index);
    default:
        break;
    }
    return text;
}

void UIScene_ShaderMenu::Clip(int x0, int y0, int x1, int y1)
{
    int left = m_originX + static_cast<int>(x0 * m_unit);
    int top = m_originY + static_cast<int>(y0 * m_unit);
    int right = m_originX + static_cast<int>(x1 * m_unit);
    int bottom = m_originY + static_cast<int>(y1 * m_unit);
    gRenderer->StateSetScissorRect(
        static_cast<unsigned int>(std::max(0, left)), static_cast<unsigned int>(std::max(0, top)),
        static_cast<unsigned int>(std::max(0, right)), static_cast<unsigned int>(std::max(0, bottom)));
}

void UIScene_ShaderMenu::Unclip()
{
    gRenderer->StateSetScissorRect(m_savedScissor[0], m_savedScissor[1], m_savedScissor[2], m_savedScissor[3]);
}

int UIScene_ShaderMenu::DrawParagraphs(Font& font, const Text& text, int x0, int y0, int x1, int y1, int scroll)
{
    Painter painter(m_gui, font);
    int y = y0 - scroll;
    int top = y;
    for (const Paragraph& paragraph : text)
    {
        y += paragraph.gap;
        std::vector<std::wstring> lines = painter.Wrap(paragraph.text, x1 - x0);
        for (const std::wstring& line : lines)
        {
            if (y + kLineHeight > y0 && y < y1)
                painter.Text(line, x0, y, paragraph.colour, true);
            y += kLineHeight;
        }
    }
    return y - top;
}

void UIScene_ShaderMenu::DrawTitle(Font& font)
{
    Painter painter(m_gui, font);
    const std::wstring title = L"Shaders";
    GlStateManager::pushMatrix();
    GlStateManager::translatef(kLayoutWidth * 0.5f, 6.0f, 0.0f);
    GlStateManager::scalef(2.0f, 2.0f, 1.0f);
    painter.Text(title, -painter.Width(title) / 2, 0, kTextWhite, true);
    GlStateManager::popMatrix();

    long long age = System::processTimeInMilliSecs() - m_noticeTime;
    std::wstring subtitle = L"More Options";
    unsigned int colour = 0xFFAAAAAA;
    if (!m_notice.empty() && age < kNoticeMilliseconds)
    {
        subtitle = m_notice;
        float fade = age > kNoticeMilliseconds - 500 ? static_cast<float>(kNoticeMilliseconds - age) / 500.0f : 1.0f;
        colour = WithAlpha(m_noticeColour, fade);
    }
    else if (HasPendingChanges())
    {
        subtitle = L"Changes not applied yet";
        colour = kTextModified;
    }
    painter.Centred(painter.Trim(subtitle, kLayoutWidth - 200), kLayoutWidth / 2, 26, colour, true);

    for (const Target& target : m_targets)
    {
        if (target.kind != eTarget_Hide)
            continue;
        bool focused = Focused(target.kind, target.index);
        painter.Button(target.x0, target.y0, target.x1, target.y1, focused, true);
        painter.Centred(L"Hide Menu", (target.x0 + target.x1) / 2, target.y0 + 4, focused ? kTextYellow : kTextLight,
                        true);
        if (focused)
            painter.Ring(target.x0, target.y0, target.x1, target.y1, m_pulse);
    }
}

void UIScene_ShaderMenu::DrawTabs(Font& font, bool selected)
{
    Painter painter(m_gui, font);
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
        bool enabled = TabEnabled(tab);
        bool focused = Focused(eTarget_Tab, tab);
        std::wstring label = painter.Trim(L10N::GetString(kLabels[tab]), kTabWidth - 8);
        unsigned int colour = !enabled ? 0xFF8A8A8A : focused ? 0xFF2A3070 : kTextDark;
        painter.Centred(label, (x0 + x1) / 2, y0 + 6, colour, false);
        if (focused)
            painter.Ring(x0, y0, x1, y1, m_pulse);
    }
}

void UIScene_ShaderMenu::DrawActions(Font& font)
{
    Painter painter(m_gui, font);
    for (const Target& target : m_targets)
    {
        const wchar_t* label = NULL;
        if (target.kind == eTarget_Cancel)
            label = L"Cancel";
        else if (target.kind == eTarget_Apply)
            label = L"Apply";
        else if (target.kind == eTarget_Done)
            label = L"Done";
        if (label == NULL)
            continue;
        bool focused = Focused(target.kind, target.index);
        painter.Button(target.x0, target.y0, target.x1, target.y1, focused, target.enabled);
        unsigned int colour = !target.enabled ? kTextDisabled : focused ? kTextYellow : kTextLight;
        painter.Centred(label, (target.x0 + target.x1) / 2, target.y0 + 4, colour, true);
        if (target.kind == eTarget_Apply && target.enabled && !focused)
            painter.Fill(target.x0 + 2, target.y0 + 2, target.x0 + 4, target.y1 - 2, kTextModified);
        if (focused)
            painter.Ring(target.x0, target.y0, target.x1, target.y1, m_pulse);
    }
}

void UIScene_ShaderMenu::DrawPacks(Font& font)
{
    Painter painter(m_gui, font);
    ShaderPackManager& manager = ShaderPackManager::Get();
    painter.Panel(kPanelLeft, kPanelTop, kSplitRight, kPanelBottom);

    Clip(kListLeft - 6, kListTop, kListRight, kListBottom);
    for (const Target& target : m_targets)
    {
        if (target.view != eView_List)
            continue;
        int x0 = 0;
        int y0 = 0;
        int x1 = 0;
        int y1 = 0;
        ScreenRect(target, &x0, &y0, &x1, &y1);
        if (y1 < kListTop - 4 || y0 > kListBottom + 4)
            continue;
        bool focused = Focused(target.kind, target.index);
        painter.Button(x0, y0, x1, y1, focused, true);
        if (target.kind == eTarget_Toggle)
        {
            bool modified = ShadersOn(*m_workingSettings) != ShadersOn(*m_appliedSettings);
            unsigned int colour = focused ? kTextYellow : modified ? kTextModified : kTextLight;
            painter.Text(L"Shaders", x0 + 6, y0 + 6, colour, true);
            if (modified)
                painter.Fill(x0 + 1, y0 + 1, x0 + 3, y1 - 1, kTextModified);
            const wchar_t* const states[2] = {L"ON", L"OFF"};
            int switchWidth = 30;
            int left = x1 - 4 - switchWidth * 2 - 1;
            for (int state = 0; state < 2; ++state)
            {
                bool on = state == 0;
                bool chosen = on == ShadersOn(*m_workingSettings);
                int sx0 = left + state * (switchWidth + 1);
                unsigned int edge = chosen ? (on ? kChipGreenEdge : kChipRedEdge) : 0xFF1D1D1D;
                unsigned int fill = chosen ? (on ? kChipGreen : kChipRed) : 0xFF3B3B3B;
                painter.Outline(sx0, y0 + 4, sx0 + switchWidth, y1 - 4, edge);
                painter.Fill(sx0 + 1, y0 + 5, sx0 + switchWidth - 1, y1 - 5, fill);
                painter.Centred(states[state], sx0 + switchWidth / 2, y0 + 6, chosen ? kTextWhite : 0xFF9A9A9A, chosen);
            }
        }
        else
        {
            std::string name = PackName(target.index);
            bool usable = PackUsable(target.index);
            bool applied = name == m_appliedSettings->shaderPack && m_appliedSettings->enableShaders;
            bool selected = name == m_workingSettings->shaderPack && m_workingSettings->enableShaders;
            bool dimmed = !ShadersOn(*m_workingSettings) && !focused;
            if (target.index == kPackInternal)
                painter.GrassIcon(x0 + 3, y0 + 3, !dimmed);
            else
                painter.PackIcon(x0 + 3, y0 + 3, usable && !dimmed);

            int chipRight = x1 - 4;
            int chipLeft = chipRight;
            if (applied && !PackChangePending())
            {
                bool failed = target.index != kPackInternal && manager.ActivePack() == nullptr;
                chipLeft = failed ? painter.Chip(chipRight, y0 + 4, L"Failed", kChipRed, kChipRedEdge)
                                  : painter.Chip(chipRight, y0 + 4, L"In use", kChipGreen, kChipGreenEdge);
            }
            else if (selected)
            {
                chipLeft = painter.Chip(chipRight, y0 + 4, L"Selected", kChipAmber, kChipAmberEdge);
            }
            else if (applied)
            {
                chipLeft = painter.Chip(chipRight, y0 + 4, L"In use", kChipGrey, kChipGreyEdge);
            }
            else if (!usable)
            {
                chipLeft = painter.Chip(chipRight, y0 + 4, L"Incompatible", kChipRed, kChipRedEdge);
            }
            else if (name == m_appliedSettings->shaderPack && !m_appliedSettings->enableShaders)
            {
                chipLeft = painter.Chip(chipRight, y0 + 4, L"Off", kChipGrey, kChipGreyEdge);
            }
            std::wstring label = target.index == kPackInternal ? std::wstring(L"(internal)") : PackDisplayName(name);
            unsigned int colour = focused ? kTextYellow : !usable || dimmed ? kTextDisabled : kTextLight;
            painter.Text(painter.Trim(label, chipLeft - (x0 + 22) - 4), x0 + 22, y0 + 6, colour, true);
        }
        if (focused)
            painter.Ring(x0, y0, x1, y1, m_pulse);
    }
    if (m_packs.empty())
    {
        int y = kListTop + 2 * kRowPitch + 4 - static_cast<int>(m_scroll[eView_List] + 0.5f) + 4;
        painter.Centred(L"No shader packs yet", (kListLeft + kListRowRight) / 2, y, kTextHint, false);
    }
    Unclip();
    painter.Scrollbar(kListRight - 5, kListTop, kListBottom, ContentHeight(eView_List), m_scroll[eView_List],
                      0xFF8B8B8B, 0xFF555555);

    painter.Centred(L"Drop shader packs on this window to add them", (kListLeft + kListRowRight) / 2, kHintTop,
                    kTextHint, false);

    for (const Target& target : m_targets)
    {
        const wchar_t* label = NULL;
        if (target.kind == eTarget_OpenFolder)
            label = L"Open Pack Folder";
        else if (target.kind == eTarget_Download)
            label = L"Download Shaders";
        if (label == NULL)
            continue;
        bool focused = Focused(target.kind, target.index);
        painter.Button(target.x0, target.y0, target.x1, target.y1, focused, true);
        painter.Centred(label, (target.x0 + target.x1) / 2, target.y0 + 6, focused ? kTextYellow : kTextLight, true);
        if (focused)
            painter.Ring(target.x0, target.y0, target.x1, target.y1, m_pulse);
    }

    painter.Description(kDetailLeft, kPanelTop, kPanelRight, kPanelBottom);
    DrawDetails(font);
}

void UIScene_ShaderMenu::DrawCell(Font& font, int index, int x0, int y0, int x1, int y1, bool focused)
{
    Painter painter(m_gui, font);
    const Cell& cell = m_cells[static_cast<size_t>(index)];
    ShaderPackManager& manager = ShaderPackManager::Get();
    const ShaderPackLang& lang = manager.Lang();
    const ShaderOption* option = cell.kind == eCell_Option ? manager.Options().Find(cell.name) : nullptr;
    bool enabled = cell.kind != eCell_Option || (option != nullptr && !option->ambiguous);
    painter.Button(x0, y0, x1, y1, focused, enabled);
    if (cell.kind == eCell_Link)
    {
        std::wstring label = painter.Trim(Wide(lang.ScreenLabel(cell.name)), x1 - x0 - 24);
        painter.Centred(label, (x0 + x1) / 2, y0 + 6, focused ? kTextYellow : kTextLight, true);
        painter.Text(L">", x1 - 10, y0 + 6, focused ? kTextYellow : kTextLight, true);
    }
    else
    {
        unsigned int valueColour = kTextLight;
        std::wstring value = CellValueText(cell.kind == eCell_Profile, cell.name, &valueColour);
        bool slider = IsSlider(cell);
        int boxX0 = 0;
        int boxX1 = 0;
        CellValueRect(index, &boxX0, &boxX1);
        painter.Box(boxX0, y0 + 2, boxX1, y1 - 2, focused);
        int count = 0;
        int position = slider ? SliderPosition(cell, &count) : 0;
        if (focused && enabled && option != nullptr && g_pInput->MenuQuickMoveHeld(m_playerId)
            && manager.Options().Value(cell.name) != option->defaultValue)
        {
            value = Wide(ShaderPackLang::StripFormatting(lang.ValueLabel(*option, option->defaultValue)));
            valueColour = kTextOn;
            std::vector<std::string>::const_iterator found =
                std::find(option->values.begin(), option->values.end(), option->defaultValue);
            if (found != option->values.end())
                position = static_cast<int>(found - option->values.begin());
        }
        if (slider)
            painter.Slider(boxX0, boxX1, y1, position, count, focused, valueColour);
        painter.Highlight(boxX0 + 1, y0 + 3, boxX1 - 1, y1 - 3, Flash(eTarget_Cell, index));
        painter.Centred(painter.Trim(value, boxX1 - boxX0 - 6), (boxX0 + boxX1) / 2, slider ? y0 + 4 : y0 + 6,
                        enabled ? valueColour : kTextDisabled, true);

        std::wstring label = cell.kind == eCell_Profile ? std::wstring(L"Profile") : Wide(lang.OptionLabel(cell.name));
        bool modified = cell.kind == eCell_Option && OptionModified(cell.name);
        unsigned int colour = !enabled ? kTextDisabled : focused ? kTextYellow : modified ? kTextModified : kTextLight;
        if (modified)
            painter.Fill(x0 + 1, y0 + 1, x0 + 3, y1 - 1, kTextModified);
        painter.Text(painter.Trim(label, boxX0 - x0 - 10), x0 + 6, y0 + 6, colour, true);
    }
    if (focused)
        painter.Ring(x0, y0, x1, y1, m_pulse);
}

void UIScene_ShaderMenu::DrawOptions(Font& font)
{
    Painter painter(m_gui, font);
    ShaderPackManager& manager = ShaderPackManager::Get();
    painter.Panel(kPanelLeft, kPanelTop, kPanelRight, kPanelBottom);
    if (manager.ActivePack() == nullptr)
    {
        int middle = (kPanelTop + kPanelBottom) / 2;
        painter.Centred(L"No shader pack is loaded", kLayoutWidth / 2, middle - 10, kTextDark, false);
        painter.Centred(L"Choose one on the Shader Packs tab and apply it.", kLayoutWidth / 2, middle + 4, kTextHint,
                        false);
        return;
    }

    int titleLeft = kGridLeft + 4;
    int titleRight = kGridRight - 3 * (kHeaderButtonWidth + 4) - 4;
    for (const Target& target : m_targets)
    {
        const wchar_t* label = NULL;
        switch (target.kind)
        {
        case eTarget_Back:
            label = L"< Back";
            titleLeft = target.x1 + 6;
            break;
        case eTarget_Import:
            label = L"Import";
            break;
        case eTarget_Export:
            label = L"Export";
            break;
        case eTarget_Reset:
            label = L"Reset";
            break;
        default:
            break;
        }
        if (label == NULL)
            continue;
        bool focused = Focused(target.kind, target.index);
        painter.Button(target.x0, target.y0, target.x1, target.y1, focused, true);
        painter.Centred(label, (target.x0 + target.x1) / 2, target.y0 + 5, focused ? kTextYellow : kTextLight, true);
        if (focused)
            painter.Ring(target.x0, target.y0, target.x1, target.y1, m_pulse);
    }
    std::wstring crumb = painter.TrimStart(Breadcrumb(), titleRight - titleLeft);
    painter.Centred(crumb, (titleLeft + titleRight) / 2, kHeaderTop + 5, kTextDark, false);
    painter.Fill(kGridLeft, kHeaderBottom + 3, kGridRight, kHeaderBottom + 4, kPanelShade);
    painter.Fill(kGridLeft, kHeaderBottom + 4, kGridRight, kHeaderBottom + 5, kPanelLight);

    Clip(kGridLeft - 6, kGridTop, kGridRight + 6, kGridBottom);
    for (const Target& target : m_targets)
    {
        if (target.view != eView_Grid)
            continue;
        int x0 = 0;
        int y0 = 0;
        int x1 = 0;
        int y1 = 0;
        ScreenRect(target, &x0, &y0, &x1, &y1);
        if (y1 < kGridTop - 4 || y0 > kGridBottom + 4)
            continue;
        DrawCell(font, target.index, x0, y0, x1, y1, Focused(target.kind, target.index));
    }
    if (m_cells.empty())
        painter.Centred(L"This page has no options", kLayoutWidth / 2, kGridTop + 20, kTextHint, false);
    Unclip();
    painter.Scrollbar(kGridRight - 5, kGridTop, kGridBottom, ContentHeight(eView_Grid), m_scroll[eView_Grid],
                      0xFF8B8B8B, 0xFF555555);

    painter.Description(kGridLeft, kCommentTop, kGridRight, kCommentBottom);
    DrawDetails(font);
}

void UIScene_ShaderMenu::DrawDetails(Font& font)
{
    Painter painter(m_gui, font);
    int viewX0 = 0;
    int viewY0 = 0;
    int viewX1 = 0;
    int viewY1 = 0;
    ViewRect(eView_Details, &viewX0, &viewY0, &viewX1, &viewY1);
    Clip(viewX0, viewY0, viewX1 + 2, viewY1);
    m_detailHeight = DrawParagraphs(font, DescribeFocus(), viewX0, viewY0, viewX1 - 4, viewY1,
                                    static_cast<int>(m_scroll[eView_Details] + 0.5f));
    Unclip();
    painter.Scrollbar(viewX1, viewY0, viewY1, m_detailHeight, m_scroll[eView_Details], 0x40FFFFFF, 0xA0FFFFFF);
}

void UIScene_ShaderMenu::DrawSettings(Font& font)
{
    Painter painter(m_gui, font);
    painter.Panel(kPanelLeft, kPanelTop, kSplitRight, kPanelBottom);
    for (const Target& target : m_targets)
    {
        if (target.view != eView_Settings)
            continue;
        int x0 = 0;
        int y0 = 0;
        int x1 = 0;
        int y1 = 0;
        ScreenRect(target, &x0, &y0, &x1, &y1);
        bool focused = Focused(target.kind, target.index);
        int setting = target.index;
        painter.Button(x0, y0, x1, y1, focused, true);
        int boxX1 = x1 - 2;
        int boxX0 = boxX1 - kSettingValueWidth;
        painter.Box(boxX0, y0 + 2, boxX1, y1 - 2, focused);
        int index = SettingIndex(*m_workingSettings, setting);
        unsigned int valueColour = SettingColour(setting, index);
        int defaultIndex = SettingIndex(ShaderSettings(), setting);
        if (focused && g_pInput->MenuQuickMoveHeld(m_playerId) && index != defaultIndex)
        {
            index = defaultIndex;
            valueColour = kTextOn;
        }
        bool slider = SettingIsSlider(setting);
        if (slider)
            painter.Slider(boxX0, boxX1, y1, index, SettingCount(setting), focused, valueColour);
        painter.Highlight(boxX0 + 1, y0 + 3, boxX1 - 1, y1 - 3, Flash(eTarget_Setting, setting));
        painter.Centred(SettingLabel(setting, index), (boxX0 + boxX1) / 2, slider ? y0 + 4 : y0 + 6, valueColour, true);
        bool modified = SettingModified(setting);
        if (modified)
            painter.Fill(x0 + 1, y0 + 1, x0 + 3, y1 - 1, kTextModified);
        painter.Text(kSettingNames[setting], x0 + 6, y0 + 6,
                     focused ? kTextYellow : modified ? kTextModified : kTextLight, true);
        if (focused)
            painter.Ring(x0, y0, x1, y1, m_pulse);
    }
    painter.Centred(L"These settings apply to every shader pack", (kListLeft + kListRowRight) / 2, kSettingsBottom + 10,
                    kTextHint, false);

    painter.Description(kDetailLeft, kPanelTop, kPanelRight, kPanelBottom);
    DrawDetails(font);
}

void UIScene_ShaderMenu::DrawModal(Font& font)
{
    if (m_modal == eModal_None)
        return;
    Painter painter(m_gui, font);
    painter.Fill(0, 0, kLayoutWidth, kLayoutHeight, 0x90000000);
    painter.Panel(kModalLeft, kModalTop, kModalRight, kModalBottom);
    std::wstring title;
    std::wstring message;
    const wchar_t* accept = L"OK";
    if (m_modal == eModal_Reset)
    {
        title = L"Reset Every Option?";
        message = L"Every option of this shader pack goes back to its default. Press Cancel in the top corner "
                  L"afterwards to undo it.";
        accept = L"Reset";
    }
    else
    {
        title = L"Download Shaders";
        message = L"Open modrinth.com/shaders in your web browser? Drag the shader packs you download onto this "
                  L"window to add them.";
        accept = L"Open";
    }
    painter.Centred(title, kLayoutWidth / 2, kModalTop + 10, kTextDark, false);
    std::vector<std::wstring> lines = painter.Wrap(message, kModalRight - kModalLeft - 32);
    int y = kModalTop + 30;
    for (const std::wstring& line : lines)
    {
        painter.Centred(line, kLayoutWidth / 2, y, 0xFF555555, false);
        y += kLineHeight;
    }
    for (const Target& target : m_targets)
    {
        if (target.kind != eTarget_Modal)
            continue;
        bool focused = Focused(target.kind, target.index);
        painter.Button(target.x0, target.y0, target.x1, target.y1, focused, true);
        painter.Centred(target.index == 0 ? accept : L"Cancel", (target.x0 + target.x1) / 2, target.y0 + 5,
                        focused ? kTextYellow : kTextLight, true);
        if (focused)
            painter.Ring(target.x0, target.y0, target.x1, target.y1, m_pulse);
    }
}

void UIScene_ShaderMenu::render(int width, int height, C4JRender::eViewportType viewport)
{
    Minecraft* minecraft = Minecraft::GetInstance();
    if (!m_hasTicked || minecraft == NULL || minecraft->mFont == NULL)
        return;
    Font& font = *minecraft->mFont;
    Animate();
    BuildTargets();

    int viewportX = 0;
    int viewportY = 0;
    DebugOverlay::ViewportOrigin(viewport, &viewportX, &viewportY);
    const float unit = DebugOverlay::ScreenUnit(width, height);
    m_originX = viewportX + static_cast<int>((width - kLayoutWidth * unit) * 0.5f);
    m_originY = viewportY + static_cast<int>((height - kLayoutHeight * unit) * 0.5f);
    m_unit = unit;
    if (m_hidden)
        return;

    gRenderer->StateGetScissorRect(m_savedScissor[0], m_savedScissor[1], m_savedScissor[2], m_savedScissor[3]);
    DebugOverlay::BeginScreenDraw(m_originX, m_originY, unit);
    DrawTitle(font);
    DrawTabs(font, false);
    if (m_tab == eTab_Packs)
        DrawPacks(font);
    else if (m_tab == eTab_Options)
        DrawOptions(font);
    else
        DrawSettings(font);
    DrawTabs(font, true);
    DrawActions(font);
    DrawModal(font);
    Unclip();
    DebugOverlay::EndScreenDraw();
}
