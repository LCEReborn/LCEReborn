#include "platform/pc/KeyboardMouse.hpp"

#include <SDL3/SDL.h>

#include <algorithm>
#include <cctype>
#include <cstring>
#include <fstream>
#include <sstream>

#include "game/core/GameSettings.hpp"
#include "platform/C4JInput.hpp"

namespace
{
const int kMouseInputBase = PcKeyboardMouse::SCANCODE_COUNT;
const float kMouseActivityDistance = 2.0f;

struct ActionNameEntry
{
    int action;
    const char* name;
};

const ActionNameEntry kActionNames[] = {
    {EControllerActions_MenuA, "MenuAccept"},
    {EControllerActions_MenuB, "MenuBack"},
    {EControllerActions_MenuX, "MenuX"},
    {EControllerActions_MenuY, "MenuY"},
    {EControllerActions_MenuUp, "MenuUp"},
    {EControllerActions_MenuDown, "MenuDown"},
    {EControllerActions_MenuRight, "MenuRight"},
    {EControllerActions_MenuLeft, "MenuLeft"},
    {EControllerActions_MenuPageUp, "MenuPageUp"},
    {EControllerActions_MenuPageDown, "MenuPageDown"},
    {EControllerActions_MenuRightScroll, "MenuNextTab"},
    {EControllerActions_MenuLeftScroll, "MenuPreviousTab"},
    {EControllerActions_MenuStickPress, "MenuStickPress"},
    {EControllerActions_MenuOtherStickPress, "MenuInfo"},
    {EControllerActions_MenuOtherStickUp, "MenuScrollUp"},
    {EControllerActions_MenuOtherStickDown, "MenuScrollDown"},
    {EControllerActions_MenuOtherStickLeft, "MenuScrollLeft"},
    {EControllerActions_MenuOtherStickRight, "MenuScrollRight"},
    {EControllerActions_MenuStart, "MenuStart"},
    {EControllerActions_MenuBack, "MenuSelect"},
    {EControllerActions_MenuOk, "MenuOk"},
    {EControllerActions_MenuCancel, "MenuCancel"},
    {EControllerActions_Pause, "Pause"},
    {EControllerActions_Scoreboard, "PlayerList"},
    {EControllerActions_MoveForward, "Forward"},
    {EControllerActions_MoveBackward, "Back"},
    {EControllerActions_MoveLeft, "Left"},
    {EControllerActions_MoveRight, "Right"},
    {EControllerActions_LookUp, "LookUp"},
    {EControllerActions_LookDown, "LookDown"},
    {EControllerActions_LookLeft, "LookLeft"},
    {EControllerActions_LookRight, "LookRight"},
    {EControllerActions_Jump, "Jump"},
    {EControllerActions_Use, "Use"},
    {EControllerActions_Action, "Attack"},
    {EControllerActions_CycleHeldItemLeft, "HotbarPrevious"},
    {EControllerActions_CycleHeldItemRight, "HotbarNext"},
    {EControllerActions_Inventory, "Inventory"},
    {EControllerActions_Drop, "Drop"},
    {EControllerActions_Sneak, "Sneak"},
    {EControllerActions_Crafting, "Crafting"},
    {EControllerActions_ChangeCameraMode, "Perspective"},
    {EControllerActions_FlyLeft, "FlyLeft"},
    {EControllerActions_FlyRight, "FlyRight"},
    {EControllerActions_FlyUp, "FlyUp"},
    {EControllerActions_FlyDown, "FlyDown"},
    {EControllerActions_Sprint, "Sprint"},
    {EControllerActions_PickBlock, "PickBlock"},
    {EControllerActions_SpectatePrevious, "SpectatePrevious"},
    {EControllerActions_SpectateNext, "SpectateNext"},
    {EControllerActions_SpectateNoise, "SpectateNoise"},
    {EControllerActions_SpectateCancel, "SpectateCancel"},
    {EControllerActions_ConfirmReady, "Ready"},
    {EControllerActions_Vote, "Vote"},
    {EControllerActions_RestartSection, "RestartSection"},
    {EControllerActions_RestartRace, "RestartRace"},
    {EControllerActions_LookBehind, "LookBehind"},
    {EControllerActions_AltFlyLeft, "AltFlyLeft"},
    {EControllerActions_AltFlyRight, "AltFlyRight"},
    {EControllerActions_AltFlyUp, "AltFlyUp"},
    {EControllerActions_AltFlyDown, "AltFlyDown"},
    {EControllerActions_QuickMoveModifier, "QuickMove"},
    {EControllerActions_ToggleHud, "ToggleHud"},
    {EControllerActions_Chat, "Chat"},
    {EControllerActions_ChatCommand, "ChatCommand"},
    {EControllerActions_DebugOverlay, "DebugOverlay"}};

const char* const kContextNames[EPcInputContext_Count] = {"menu", "game"};

const char* const kMouseButtonNames[PcKeyboardMouse::MOUSE_BUTTON_COUNT] = {
    "Mouse 0", "Mouse Left", "Mouse Middle", "Mouse Right", "Mouse Back", "Mouse Forward", "Mouse 6", "Mouse 7"};

int Key(SDL_Scancode scancode)
{
    return static_cast<int>(scancode);
}

int MouseButton(int button)
{
    return kMouseInputBase + button;
}

std::string Trim(const std::string& text)
{
    size_t begin = 0;
    while (begin < text.size() && std::isspace(static_cast<unsigned char>(text[begin])))
        ++begin;
    size_t end = text.size();
    while (end > begin && std::isspace(static_cast<unsigned char>(text[end - 1])))
        --end;
    return text.substr(begin, end - begin);
}

bool SameText(const std::string& first, const std::string& second)
{
    if (first.size() != second.size())
        return false;
    for (size_t index = 0; index < first.size(); ++index)
    {
        if (std::tolower(static_cast<unsigned char>(first[index]))
            != std::tolower(static_cast<unsigned char>(second[index])))
            return false;
    }
    return true;
}

int ActionFromName(const std::string& name)
{
    for (size_t index = 0; index < sizeof(kActionNames) / sizeof(kActionNames[0]); ++index)
    {
        if (SameText(name, kActionNames[index].name))
            return kActionNames[index].action;
    }
    return -1;
}

const std::vector<int>& EmptyBindings()
{
    static const std::vector<int> empty;
    return empty;
}
}

PcKeyboardMouse::PcKeyboardMouse()
{
    std::memset(mInputDown, 0, sizeof(mInputDown));
    mContext = EPcInputContext_Menu;
    mPendingAnyPressed = false;
    mMouseSensitivity = 100;
    mToggleSneak = false;
    mToggleSprint = false;
    mSprintLatched = false;
    mWheelX = mWheelY = 0.0f;
    mCaptureModifier = -1;
    for (int input = 0; input < INPUT_COUNT; ++input)
        mInputContext[input] = EPcInputContext_Menu;
    std::memset(mActions, 0, sizeof(mActions));
    std::memset(mFakeTicks, 0, sizeof(mFakeTicks));
    mCapturing = false;
    mCaptured = C_4JInput::EKeyCapture_None;
    mAnyPressed = false;
    mMoveX = 0.0f;
    mMoveY = 0.0f;
    mLookX = 0.0f;
    mLookY = 0.0f;
    mHotbarSlot = -1;
    mHotbarScroll = 0;
    mMenuWheel = 0;
    mPointerX = 0.0f;
    mPointerY = 0.0f;
    mPointerMoved = false;
    mMotionSinceTick = 0.0f;
    mMovedThisTick = false;
    mClicks = 0;
    mRepeatDelay = 0;
    SetDefaultBindings();
}

void PcKeyboardMouse::AddBinding(EPcInputContext context, int action, int input)
{
    if (action >= 0 && action < ACTION_COUNT && input >= 0 && (input & INPUT_MASK) > 0
        && (input & INPUT_MASK) < INPUT_COUNT && (input & ~(INPUT_MASK | MOD_MASK)) == 0)
        mBindings[context][action].push_back(input);
}

void PcKeyboardMouse::SetDefaultBindings()
{
    for (int context = 0; context < EPcInputContext_Count; ++context)
    {
        for (int action = 0; action < ACTION_COUNT; ++action)
            mBindings[context][action].clear();
    }

    const EPcInputContext menu = EPcInputContext_Menu;
    AddBinding(menu, EControllerActions_MenuA, Key(SDL_SCANCODE_RETURN));
    AddBinding(menu, EControllerActions_MenuA, Key(SDL_SCANCODE_KP_ENTER));
    AddBinding(menu, EControllerActions_MenuA, Key(SDL_SCANCODE_SPACE));
    AddBinding(menu, EControllerActions_MenuOk, Key(SDL_SCANCODE_RETURN));
    AddBinding(menu, EControllerActions_MenuOk, Key(SDL_SCANCODE_KP_ENTER));
    AddBinding(menu, EControllerActions_MenuOk, Key(SDL_SCANCODE_SPACE));
    AddBinding(menu, EControllerActions_MenuB, Key(SDL_SCANCODE_ESCAPE));
    AddBinding(menu, EControllerActions_MenuB, Key(SDL_SCANCODE_BACKSPACE));
    AddBinding(menu, EControllerActions_MenuB, Key(SDL_SCANCODE_E));
    AddBinding(menu, EControllerActions_MenuCancel, Key(SDL_SCANCODE_ESCAPE));
    AddBinding(menu, EControllerActions_MenuCancel, Key(SDL_SCANCODE_BACKSPACE));
    AddBinding(menu, EControllerActions_MenuCancel, Key(SDL_SCANCODE_E));
    AddBinding(menu, EControllerActions_MenuX, Key(SDL_SCANCODE_X));
    AddBinding(menu, EControllerActions_MenuY, Key(SDL_SCANCODE_Y));
    AddBinding(menu, EControllerActions_MenuUp, Key(SDL_SCANCODE_UP));
    AddBinding(menu, EControllerActions_MenuUp, Key(SDL_SCANCODE_W));
    AddBinding(menu, EControllerActions_MenuDown, Key(SDL_SCANCODE_DOWN));
    AddBinding(menu, EControllerActions_MenuDown, Key(SDL_SCANCODE_S));
    AddBinding(menu, EControllerActions_MenuLeft, Key(SDL_SCANCODE_LEFT));
    AddBinding(menu, EControllerActions_MenuLeft, Key(SDL_SCANCODE_A));
    AddBinding(menu, EControllerActions_MenuRight, Key(SDL_SCANCODE_RIGHT));
    AddBinding(menu, EControllerActions_MenuRight, Key(SDL_SCANCODE_D));
    AddBinding(menu, EControllerActions_MenuPageUp, Key(SDL_SCANCODE_PAGEUP));
    AddBinding(menu, EControllerActions_MenuPageDown, Key(SDL_SCANCODE_PAGEDOWN));
    AddBinding(menu, EControllerActions_MenuLeftScroll, Key(SDL_SCANCODE_Q));
    AddBinding(menu, EControllerActions_MenuRightScroll, Key(SDL_SCANCODE_R));
    AddBinding(menu, EControllerActions_MenuOtherStickPress, Key(SDL_SCANCODE_F1));
    AddBinding(menu, EControllerActions_MenuStickPress, Key(SDL_SCANCODE_F3));
    AddBinding(menu, EControllerActions_MenuStart, Key(SDL_SCANCODE_RETURN));
    AddBinding(menu, EControllerActions_MenuBack, Key(SDL_SCANCODE_TAB));
    AddBinding(menu, EControllerActions_QuickMoveModifier, Key(SDL_SCANCODE_LSHIFT));
    AddBinding(menu, EControllerActions_QuickMoveModifier, Key(SDL_SCANCODE_RSHIFT));

    const EPcInputContext game = EPcInputContext_Game;
    AddBinding(game, EControllerActions_MoveForward, Key(SDL_SCANCODE_W));
    AddBinding(game, EControllerActions_MoveBackward, Key(SDL_SCANCODE_S));
    AddBinding(game, EControllerActions_MoveLeft, Key(SDL_SCANCODE_A));
    AddBinding(game, EControllerActions_MoveRight, Key(SDL_SCANCODE_D));
    AddBinding(game, EControllerActions_Jump, Key(SDL_SCANCODE_SPACE));
    AddBinding(game, EControllerActions_Sneak, Key(SDL_SCANCODE_LSHIFT));
    AddBinding(game, EControllerActions_Sprint, Key(SDL_SCANCODE_LCTRL));
    AddBinding(game, EControllerActions_Action, MouseButton(SDL_BUTTON_LEFT));
    AddBinding(game, EControllerActions_Use, MouseButton(SDL_BUTTON_RIGHT));
    AddBinding(game, EControllerActions_PickBlock, MouseButton(SDL_BUTTON_MIDDLE));
    AddBinding(game, EControllerActions_Inventory, Key(SDL_SCANCODE_E));
    AddBinding(game, EControllerActions_Crafting, Key(SDL_SCANCODE_C));
    AddBinding(game, EControllerActions_Drop, Key(SDL_SCANCODE_Q));
    AddBinding(game, EControllerActions_ChangeCameraMode, Key(SDL_SCANCODE_F5));
    AddBinding(game, EControllerActions_Pause, Key(SDL_SCANCODE_ESCAPE));
    AddBinding(game, EControllerActions_Scoreboard, Key(SDL_SCANCODE_TAB));
    AddBinding(game, EControllerActions_FlyUp, Key(SDL_SCANCODE_SPACE));
    AddBinding(game, EControllerActions_FlyDown, Key(SDL_SCANCODE_LSHIFT));
    AddBinding(game, EControllerActions_AltFlyUp, Key(SDL_SCANCODE_SPACE));
    AddBinding(game, EControllerActions_AltFlyDown, Key(SDL_SCANCODE_LSHIFT));
    AddBinding(game, EControllerActions_SpectatePrevious, MouseButton(SDL_BUTTON_RIGHT));
    AddBinding(game, EControllerActions_SpectateNext, MouseButton(SDL_BUTTON_LEFT));
    AddBinding(game, EControllerActions_SpectateCancel, Key(SDL_SCANCODE_LSHIFT));
    AddBinding(game, EControllerActions_ConfirmReady, Key(SDL_SCANCODE_RETURN));
    AddBinding(game, EControllerActions_Vote, Key(SDL_SCANCODE_V));
    AddBinding(game, EControllerActions_RestartSection, Key(SDL_SCANCODE_R));
    AddBinding(game, EControllerActions_RestartRace, Key(SDL_SCANCODE_BACKSPACE));
    AddBinding(game, EControllerActions_LookBehind, Key(SDL_SCANCODE_F));
    AddBinding(game, EControllerActions_ToggleHud, Key(SDL_SCANCODE_F1));
    AddBinding(game, EControllerActions_Chat, Key(SDL_SCANCODE_T));
    AddBinding(game, EControllerActions_ChatCommand, Key(SDL_SCANCODE_SLASH));
    AddBinding(game, EControllerActions_DebugOverlay, Key(SDL_SCANCODE_F3));
    AddBinding(game, EControllerActions_MenuA, Key(SDL_SCANCODE_RETURN));
    AddBinding(game, EControllerActions_MenuA, Key(SDL_SCANCODE_SPACE));
    AddBinding(game, EControllerActions_MenuOk, Key(SDL_SCANCODE_RETURN));
    AddBinding(game, EControllerActions_MenuB, Key(SDL_SCANCODE_BACKSPACE));
    AddBinding(game, EControllerActions_MenuCancel, Key(SDL_SCANCODE_BACKSPACE));
    RebuildBindingIndex();
}

const std::vector<int>& PcKeyboardMouse::GetBindings(EPcInputContext context, int action) const
{
    if (context < 0 || context >= EPcInputContext_Count || action < 0 || action >= ACTION_COUNT)
        return EmptyBindings();
    return mBindings[context][action];
}

void PcKeyboardMouse::SetBindings(EPcInputContext context, int action, const std::vector<int>& inputs)
{
    if (context < 0 || context >= EPcInputContext_Count || action < 0 || action >= ACTION_COUNT)
        return;
    mBindings[context][action].clear();
    for (size_t index = 0; index < inputs.size(); ++index)
        AddBinding(context, action, inputs[index]);
    RebuildBindingIndex();
}

const char* PcKeyboardMouse::ActionName(int action)
{
    for (size_t index = 0; index < sizeof(kActionNames) / sizeof(kActionNames[0]); ++index)
    {
        if (kActionNames[index].action == action)
            return kActionNames[index].name;
    }
    return 0;
}

std::string PcKeyboardMouse::InputName(int input)
{
    if ((input & MOD_MASK) != 0)
    {
        std::string prefix;
        if (input & MOD_CTRL)
            prefix += "Ctrl+";
        if (input & MOD_SHIFT)
            prefix += "Shift+";
        if (input & MOD_ALT)
            prefix += "Alt+";
        return prefix + InputName(input & INPUT_MASK);
    }
    if (input == INPUT_WHEEL_LEFT)
        return "Wheel Left";
    if (input == INPUT_WHEEL_RIGHT)
        return "Wheel Right";
    if (input == INPUT_WHEEL_UP)
        return "Wheel Up";
    if (input == INPUT_WHEEL_DOWN)
        return "Wheel Down";
    if (input >= kMouseInputBase && input < kMouseInputBase + MOUSE_BUTTON_COUNT)
        return kMouseButtonNames[input - kMouseInputBase];
    if (input >= 0 && input < SCANCODE_COUNT)
    {
        const char* name = SDL_GetScancodeName(static_cast<SDL_Scancode>(input));
        if (name != 0 && name[0] != 0)
            return name;
    }
    return std::string();
}

bool PcKeyboardMouse::PointerPromptInput(int action, int* input, int* modifier)
{
    switch (action)
    {
    case EControllerActions_MenuA:
    case EControllerActions_MenuOk:
        *input = kMouseInputBase + SDL_BUTTON_LEFT;
        return true;
    case EControllerActions_MenuX:
        *input = kMouseInputBase + SDL_BUTTON_RIGHT;
        return true;
    case EControllerActions_MenuY:
        *modifier = SDL_SCANCODE_LSHIFT;
        *input = kMouseInputBase + SDL_BUTTON_LEFT;
        return true;
    default:
        return false;
    }
}

bool PcKeyboardMouse::IsLookAction(int action)
{
    return action == EControllerActions_LookUp || action == EControllerActions_LookDown
           || action == EControllerActions_LookLeft || action == EControllerActions_LookRight;
}

EPcInputContext PcKeyboardMouse::ActionContext(int action)
{
    return action <= EControllerActions_MenuCancel || action == EControllerActions_QuickMoveModifier
               ? EPcInputContext_Menu
               : EPcInputContext_Game;
}

std::string PcKeyboardMouse::DisplayName(int input)
{
    if ((input & MOD_MASK) != 0)
    {
        std::string prefix;
        if (input & MOD_CTRL)
            prefix += "Ctrl+";
        if (input & MOD_SHIFT)
            prefix += "Shift+";
        if (input & MOD_ALT)
            prefix += "Alt+";
        return prefix + DisplayName(input & INPUT_MASK);
    }
    if (input >= kMouseInputBase && input < kMouseInputBase + MOUSE_BUTTON_COUNT)
    {
        static const char* const kShortNames[MOUSE_BUTTON_COUNT] = {"Mouse 0", "LMB",     "MMB",     "RMB",
                                                                    "Mouse 4", "Mouse 5", "Mouse 6", "Mouse 7"};
        return kShortNames[input - kMouseInputBase];
    }
    if (input >= 0 && input < SCANCODE_COUNT)
    {
        SDL_Keycode key = SDL_GetKeyFromScancode(static_cast<SDL_Scancode>(input), SDL_KMOD_NONE, false);
        const char* name = SDL_GetKeyName(key);
        if (name != 0 && name[0] != 0)
            return name;
    }
    return InputName(input);
}

int PcKeyboardMouse::InputFromName(const std::string& name)
{
    size_t plus = name.find('+');
    if (plus != std::string::npos)
    {
        std::string prefix = Trim(name.substr(0, plus));
        int modifier = SameText(prefix, "Ctrl")    ? MOD_CTRL
                       : SameText(prefix, "Shift") ? MOD_SHIFT
                       : SameText(prefix, "Alt")   ? MOD_ALT
                                                   : 0;
        if (modifier != 0)
        {
            int input = InputFromName(Trim(name.substr(plus + 1)));
            return input > 0 ? input | modifier : -1;
        }
    }
    if (SameText(name, "Wheel Left"))
        return INPUT_WHEEL_LEFT;
    if (SameText(name, "Wheel Right"))
        return INPUT_WHEEL_RIGHT;
    if (SameText(name, "Wheel Up"))
        return INPUT_WHEEL_UP;
    if (SameText(name, "Wheel Down"))
        return INPUT_WHEEL_DOWN;
    for (int button = 0; button < MOUSE_BUTTON_COUNT; ++button)
    {
        if (SameText(name, kMouseButtonNames[button]))
            return kMouseInputBase + button;
    }
    SDL_Scancode scancode = SDL_GetScancodeFromName(name.c_str());
    if (scancode != SDL_SCANCODE_UNKNOWN)
        return static_cast<int>(scancode);
    return -1;
}

int PcKeyboardMouse::GetSetting(int setting) const
{
    switch (setting)
    {
    case C_4JInput::EKeyboardMouseSetting_Sensitivity:
        return mMouseSensitivity;
    case C_4JInput::EKeyboardMouseSetting_ToggleSneak:
        return mToggleSneak;
    case C_4JInput::EKeyboardMouseSetting_ToggleSprint:
        return mToggleSprint;
    default:
        return 0;
    }
}

void PcKeyboardMouse::SetSetting(int setting, int value)
{
    switch (setting)
    {
    case C_4JInput::EKeyboardMouseSetting_Sensitivity:
        mMouseSensitivity = std::max(1, std::min(200, value));
        break;
    case C_4JInput::EKeyboardMouseSetting_ToggleSneak:
        mToggleSneak = value != 0;
        break;
    case C_4JInput::EKeyboardMouseSetting_ToggleSprint:
        if (mToggleSprint != (value != 0))
            SetToggleSprint(value != 0);
        break;
    }
}

void PcKeyboardMouse::LoadSettings(const std::string& path)
{
    std::ifstream file(path.c_str());
    std::string line;
    while (std::getline(file, line))
    {
        size_t equals = line.find('=');
        if (equals == std::string::npos)
            continue;
        std::string key = Trim(line.substr(0, equals));
        std::istringstream stream(line.substr(equals + 1));
        int value;
        if (!(stream >> value))
            continue;
        if (key == "MouseSensitivity")
            SetSetting(C_4JInput::EKeyboardMouseSetting_Sensitivity, value);
        else if (key == "ToggleSneak")
            SetSetting(C_4JInput::EKeyboardMouseSetting_ToggleSneak, value);
        else if (key == "ToggleSprint")
            SetSetting(C_4JInput::EKeyboardMouseSetting_ToggleSprint, value);
    }
}

bool PcKeyboardMouse::SaveSettings(const std::string& path) const
{
    std::ofstream file(path.c_str(), std::ios::trunc);
    file << "MouseSensitivity = " << mMouseSensitivity << '\n';
    file << "ToggleSneak = " << mToggleSneak << '\n';
    file << "ToggleSprint = " << mToggleSprint << '\n';
    return static_cast<bool>(file);
}

void PcKeyboardMouse::LoadBindings(const std::string& path)
{
    std::ifstream file(path.c_str());
    if (!file)
    {
        SaveBindings(path);
        return;
    }

    std::string line;
    int lineNumber = 0;
    while (std::getline(file, line))
    {
        ++lineNumber;
        line = Trim(line);
        if (line.empty() || line[0] == '#')
            continue;
        size_t equals = line.find('=');
        size_t dot = line.find('.');
        if (equals == std::string::npos || dot == std::string::npos || dot > equals)
        {
            SDL_LogWarn(SDL_LOG_CATEGORY_INPUT, "%s:%d: expected context.Action = inputs", path.c_str(), lineNumber);
            continue;
        }
        std::string contextName = Trim(line.substr(0, dot));
        std::string actionName = Trim(line.substr(dot + 1, equals - dot - 1));
        int context = -1;
        for (int index = 0; index < EPcInputContext_Count; ++index)
        {
            if (SameText(contextName, kContextNames[index]))
                context = index;
        }
        int action = ActionFromName(actionName);
        if (context < 0 || action < 0)
        {
            SDL_LogWarn(SDL_LOG_CATEGORY_INPUT, "%s:%d: unknown binding %s.%s", path.c_str(), lineNumber,
                        contextName.c_str(), actionName.c_str());
            continue;
        }
        std::vector<int> inputs;
        bool valid = true;
        std::stringstream list(line.substr(equals + 1));
        std::string inputName;
        while (std::getline(list, inputName, ','))
        {
            inputName = Trim(inputName);
            if (inputName.empty())
                continue;
            int input = InputFromName(inputName);
            if (input < 0)
            {
                SDL_LogWarn(SDL_LOG_CATEGORY_INPUT, "%s:%d: unknown input %s", path.c_str(), lineNumber,
                            inputName.c_str());
                valid = false;
                continue;
            }
            inputs.push_back(input);
        }
        if (valid)
            SetBindings(static_cast<EPcInputContext>(context), action, inputs);
    }
}

bool PcKeyboardMouse::SaveBindings(const std::string& path) const
{
    std::ofstream file(path.c_str(), std::ios::trunc);
    if (!file)
    {
        SDL_LogWarn(SDL_LOG_CATEGORY_INPUT, "could not write key bindings to %s", path.c_str());
        return false;
    }
    file << "# Keyboard and mouse bindings: context.Action = input, input\n";
    file
        << "# SDL key names, Mouse Left/Middle/Right/Back/Forward, Wheel Up/Down/Left/Right; Ctrl+/Shift+/Alt+ chords.\n";
    for (int context = 0; context < EPcInputContext_Count; ++context)
    {
        for (size_t index = 0; index < sizeof(kActionNames) / sizeof(kActionNames[0]); ++index)
        {
            const std::vector<int>& inputs = mBindings[context][kActionNames[index].action];
            file << kContextNames[context] << '.' << kActionNames[index].name << " = ";
            for (size_t input = 0; input < inputs.size(); ++input)
            {
                if (input != 0)
                    file << ", ";
                file << InputName(inputs[input]);
            }
            file << '\n';
        }
    }
    return static_cast<bool>(file);
}

int PcKeyboardMouse::ModifierForInput(int input)
{
    if (input == SDL_SCANCODE_LCTRL || input == SDL_SCANCODE_RCTRL)
        return MOD_CTRL;
    if (input == SDL_SCANCODE_LSHIFT || input == SDL_SCANCODE_RSHIFT)
        return MOD_SHIFT;
    if (input == SDL_SCANCODE_LALT || input == SDL_SCANCODE_RALT)
        return MOD_ALT;
    return 0;
}

int PcKeyboardMouse::Modifiers() const
{
    int modifiers = 0;
    if (mInputDown[SDL_SCANCODE_LCTRL] || mInputDown[SDL_SCANCODE_RCTRL])
        modifiers |= MOD_CTRL;
    if (mInputDown[SDL_SCANCODE_LSHIFT] || mInputDown[SDL_SCANCODE_RSHIFT])
        modifiers |= MOD_SHIFT;
    if (mInputDown[SDL_SCANCODE_LALT] || mInputDown[SDL_SCANCODE_RALT])
        modifiers |= MOD_ALT;
    return modifiers;
}

void PcKeyboardMouse::RebuildBindingIndex()
{
    ReleaseAll();
    for (int context = 0; context < EPcInputContext_Count; ++context)
    {
        for (int input = 0; input < INPUT_COUNT; ++input)
            mBindingIndex[context][input].clear();
        for (int action = 0; action < ACTION_COUNT; ++action)
        {
            const std::vector<int>& bindings = mBindings[context][action];
            for (size_t index = 0; index < bindings.size(); ++index)
            {
                BindingTarget target = {action, bindings[index] & MOD_MASK};
                mBindingIndex[context][bindings[index] & INPUT_MASK].push_back(target);
            }
        }
    }
}

void PcKeyboardMouse::ChangeAction(int action, int change, unsigned long long timestamp)
{
    ActionState& state = mActions[action];
    bool wasHeld = state.held != 0;
    state.held += change;
    if (!wasHeld && state.held != 0)
    {
        state.pendingPressed = true;
        state.pressTime = timestamp;
        state.repeatStarted = false;
        if (action == EControllerActions_Sprint && mToggleSprint)
            mSprintLatched = !mSprintLatched;
    }
    if (wasHeld && state.held == 0)
        state.pendingReleased = true;
}

void PcKeyboardMouse::RefreshInput(int input, unsigned long long timestamp, bool activate)
{
    std::vector<int> next;
    if (mInputDown[input] && mInputContext[input] == mContext)
    {
        int modifiers = Modifiers();
        int best = -1;
        const std::vector<BindingTarget>& targets = mBindingIndex[mContext][input];
        for (size_t index = 0; index < targets.size(); ++index)
        {
            int required = targets[index].modifiers;
            if ((required & modifiers) != required)
                continue;
            int count = ((required & MOD_CTRL) != 0) + ((required & MOD_SHIFT) != 0) + ((required & MOD_ALT) != 0);
            if (count > best)
                best = count;
        }
        for (size_t index = 0; index < targets.size(); ++index)
        {
            int required = targets[index].modifiers;
            int count = ((required & MOD_CTRL) != 0) + ((required & MOD_SHIFT) != 0) + ((required & MOD_ALT) != 0);
            if (count == best && (required & modifiers) == required
                && (activate
                    || std::find(mActiveActions[input].begin(), mActiveActions[input].end(), targets[index].action)
                           != mActiveActions[input].end())
                && std::find(next.begin(), next.end(), targets[index].action) == next.end())
                next.push_back(targets[index].action);
        }
    }
    const std::vector<int>& active = mActiveActions[input];
    for (size_t index = 0; index < active.size(); ++index)
    {
        if (std::find(next.begin(), next.end(), active[index]) == next.end())
            ChangeAction(active[index], -1, timestamp);
    }
    for (size_t index = 0; index < next.size(); ++index)
    {
        if (std::find(active.begin(), active.end(), next[index]) == active.end())
            ChangeAction(next[index], 1, timestamp);
    }
    mActiveActions[input].swap(next);
}

void PcKeyboardMouse::RefreshModifiers(unsigned long long timestamp)
{
    for (int input = 1; input < INPUT_COUNT; ++input)
    {
        if (mInputDown[input])
            RefreshInput(input, timestamp, false);
    }
}

void PcKeyboardMouse::PressInput(int input, unsigned long long timestamp)
{
    if (input <= 0 || input >= INPUT_COUNT || mInputDown[input])
        return;
    mPendingAnyPressed = true;
    mInputDown[input] = true;
    mInputContext[input] = mContext;
    RefreshInput(input, timestamp);
    if (ModifierForInput(input))
        RefreshModifiers(timestamp);
}

void PcKeyboardMouse::ReleaseInput(int input, unsigned long long timestamp)
{
    if (input <= 0 || input >= INPUT_COUNT || !mInputDown[input])
        return;
    mInputDown[input] = false;
    RefreshInput(input, timestamp);
    if (ModifierForInput(input))
        RefreshModifiers(timestamp);
}

void PcKeyboardMouse::SetContext(EPcInputContext context)
{
    if (context == mContext)
        return;
    mContext = context;
    for (int input = 0; input < INPUT_COUNT; ++input)
        mInputContext[input] = EPcInputContext_Count;
    for (int input = 0; input < INPUT_COUNT; ++input)
        mActiveActions[input].clear();
    std::memset(mActions, 0, sizeof(mActions));
    std::memset(mFakeTicks, 0, sizeof(mFakeTicks));
    mSprintLatched = false;
    mPendingAnyPressed = false;
    mAnyPressed = false;
    mMoveX = mMoveY = 0.0f;
    DiscardRelativeInput();
}

void PcKeyboardMouse::SetToggleSprint(bool enabled)
{
    mToggleSprint = enabled;
    mSprintLatched = false;
}

void PcKeyboardMouse::BeginCapture()
{
    ReleaseAll();
    mCaptureModifier = -1;
    mCapturing = true;
    mCaptured = C_4JInput::EKeyCapture_None;
}

void PcKeyboardMouse::CancelCapture()
{
    mCapturing = false;
    mCaptureModifier = -1;
    ReleaseAll();
    mCaptured = C_4JInput::EKeyCapture_None;
}

int PcKeyboardMouse::ConsumeCapture()
{
    int captured = mCaptured;
    mCaptured = C_4JInput::EKeyCapture_None;
    return captured;
}

bool PcKeyboardMouse::QuickMoveHeld() const
{
    const std::vector<int>& inputs = mBindings[EPcInputContext_Menu][EControllerActions_QuickMoveModifier];
    for (size_t index = 0; index < inputs.size(); ++index)
    {
        int input = inputs[index];
        if (mInputDown[input & INPUT_MASK] && (Modifiers() & (input & MOD_MASK)) == (input & MOD_MASK))
            return true;
    }
    return false;
}

bool PcKeyboardMouse::CaptureEvent(const SDL_Event& event)
{
    int input = C_4JInput::EKeyCapture_None;
    switch (event.type)
    {
    case SDL_EVENT_KEY_DOWN:
        if (event.key.repeat || event.key.scancode >= SCANCODE_COUNT)
            return true;
        if (ModifierForInput(event.key.scancode))
        {
            mInputDown[event.key.scancode] = true;
            mCaptureModifier = event.key.scancode;
            return true;
        }
        input = event.key.scancode == SDL_SCANCODE_ESCAPE ? static_cast<int>(C_4JInput::EKeyCapture_Cancelled)
                                                          : static_cast<int>(event.key.scancode) | Modifiers();
        break;
    case SDL_EVENT_KEY_UP:
        if (ModifierForInput(event.key.scancode))
        {
            mInputDown[event.key.scancode] = false;
            if (event.key.scancode == mCaptureModifier)
                input = mCaptureModifier;
        }
        if (input == C_4JInput::EKeyCapture_None)
            return true;
        break;
    case SDL_EVENT_MOUSE_BUTTON_DOWN:
        if (event.button.button >= MOUSE_BUTTON_COUNT)
            return true;
        input = (kMouseInputBase + event.button.button) | Modifiers();
        break;
    case SDL_EVENT_MOUSE_WHEEL:
    {
        float x = event.wheel.x;
        float y = event.wheel.y;
        if (event.wheel.direction == SDL_MOUSEWHEEL_FLIPPED)
        {
            x = -x;
            y = -y;
        }
        if (x == 0.0f && y == 0.0f)
            return true;
        input = (SDL_fabsf(y) >= SDL_fabsf(x) ? (y > 0 ? INPUT_WHEEL_UP : INPUT_WHEEL_DOWN)
                                              : (x > 0 ? INPUT_WHEEL_RIGHT : INPUT_WHEEL_LEFT))
                | Modifiers();
        break;
    }
    default:
        return false;
    }
    mCapturing = false;
    ReleaseAll();
    mCaptured = input;
    return true;
}

bool PcKeyboardMouse::PulseInput(int input, unsigned long long timestamp)
{
    PressInput(input, timestamp);
    bool bound = !mActiveActions[input].empty();
    ReleaseInput(input, timestamp);
    return bound;
}

void PcKeyboardMouse::Wheel(float x, float y, unsigned long long timestamp)
{
    mWheelX += x;
    mWheelY += y;
    int stepsX = static_cast<int>(mWheelX);
    int stepsY = static_cast<int>(mWheelY);
    mWheelX -= stepsX;
    mWheelY -= stepsY;
    for (int step = 0; step < SDL_abs(stepsX); ++step)
        PulseInput(stepsX > 0 ? INPUT_WHEEL_RIGHT : INPUT_WHEEL_LEFT, timestamp);
    for (int step = 0; step < SDL_abs(stepsY); ++step)
    {
        bool bound = PulseInput(stepsY > 0 ? INPUT_WHEEL_UP : INPUT_WHEEL_DOWN, timestamp);
        if (!bound)
        {
            if (mContext == EPcInputContext_Game)
            {
                mHotbarSlot = -1;
                mHotbarScroll += stepsY > 0 ? -1 : 1;
            }
            else
                mMenuWheel += stepsY > 0 ? 1 : -1;
        }
    }
}

void PcKeyboardMouse::HandleEvent(const SDL_Event& event, EPcInputContext context, bool mouseCaptured)
{
    SetContext(context);
    unsigned long long timestamp = event.common.timestamp != 0 ? event.common.timestamp : SDL_GetTicksNS();
    if (mCapturing && CaptureEvent(event))
        return;
    switch (event.type)
    {
    case SDL_EVENT_KEY_DOWN:
        if (event.key.repeat)
            break;
        PressInput(static_cast<int>(event.key.scancode), timestamp);
        if (context == EPcInputContext_Game && Modifiers() == 0 && event.key.scancode >= SDL_SCANCODE_1
            && event.key.scancode <= SDL_SCANCODE_9)
        {
            mHotbarSlot = static_cast<int>(event.key.scancode - SDL_SCANCODE_1);
            mHotbarScroll = 0;
        }
        break;
    case SDL_EVENT_KEY_UP:
        ReleaseInput(static_cast<int>(event.key.scancode), timestamp);
        break;
    case SDL_EVENT_MOUSE_MOTION:
        mPointerX = event.motion.x;
        mPointerY = event.motion.y;
        mMotionSinceTick += SDL_fabsf(event.motion.xrel) + SDL_fabsf(event.motion.yrel);
        if (mouseCaptured)
        {
            mLookX += event.motion.xrel;
            mLookY += event.motion.yrel;
        }
        else
        {
            mPointerMoved = true;
        }
        break;
    case SDL_EVENT_MOUSE_BUTTON_DOWN:
        if (event.button.button < MOUSE_BUTTON_COUNT)
            PressInput(kMouseInputBase + event.button.button, timestamp);
        if (!mouseCaptured)
        {
            mPointerX = event.button.x;
            mPointerY = event.button.y;
            if (event.button.button == SDL_BUTTON_LEFT)
                mClicks |= QuickMoveHeld() ? C_4JInput::EPcMenuClick_Quick : C_4JInput::EPcMenuClick_Primary;
            else if (event.button.button == SDL_BUTTON_RIGHT)
                mClicks |= C_4JInput::EPcMenuClick_Secondary;
        }
        break;
    case SDL_EVENT_MOUSE_BUTTON_UP:
        if (event.button.button < MOUSE_BUTTON_COUNT)
            ReleaseInput(kMouseInputBase + event.button.button, timestamp);
        break;
    case SDL_EVENT_MOUSE_WHEEL:
    {
        float direction = event.wheel.direction == SDL_MOUSEWHEEL_FLIPPED ? -1.0f : 1.0f;
        Wheel(event.wheel.x * direction, event.wheel.y * direction, timestamp);
        break;
    }
    default:
        break;
    }
}

void PcKeyboardMouse::Update(EPcInputContext context, float initialRepeatDelay, float repeatDelay)
{
    SetContext(context);
    unsigned long long now = SDL_GetTicksNS();
    mMovedThisTick = mMotionSinceTick >= kMouseActivityDistance;
    mMotionSinceTick = 0.0f;
    mAnyPressed = mPendingAnyPressed;
    mPendingAnyPressed = false;
    for (int action = 0; action < ACTION_COUNT; ++action)
    {
        ActionState& state = mActions[action];
        bool fake = mFakeTicks[action] > 0;
        if (fake)
            --mFakeTicks[action];
        bool pressed = state.pendingPressed || (fake && !state.down);
        bool down = state.held != 0 || state.pendingPressed || fake;
        if (action == EControllerActions_Sprint && mToggleSprint)
            down = mSprintLatched;
        state.released = state.pendingReleased || (state.down && !down);
        state.pressed = pressed;
        if (pressed)
        {
            if (fake)
                state.pressTime = now;
            state.nextRepeat = state.pressTime + static_cast<unsigned long long>(initialRepeatDelay * 1.0e9f);
        }
        state.down = down;
        state.pendingPressed = false;
        state.pendingReleased = false;
        if (!down)
            state.repeatStarted = false;
    }
    mRepeatDelay = static_cast<unsigned long long>(repeatDelay * 1.0e9f);

    float forward = (mActions[EControllerActions_MoveForward].down ? 1.0f : 0.0f)
                    - (mActions[EControllerActions_MoveBackward].down ? 1.0f : 0.0f);
    float right = (mActions[EControllerActions_MoveRight].down ? 1.0f : 0.0f)
                  - (mActions[EControllerActions_MoveLeft].down ? 1.0f : 0.0f);
    if (context != EPcInputContext_Game)
    {
        forward = 0.0f;
        right = 0.0f;
    }
    mMoveX = right;
    mMoveY = forward;
}

bool PcKeyboardMouse::ActionRepeat(int action)
{
    if (action < 0 || action >= ACTION_COUNT)
        return false;
    ActionState& state = mActions[action];
    if (!state.down)
        return false;
    if (!state.repeatStarted)
    {
        state.repeatStarted = true;
        return true;
    }
    unsigned long long now = SDL_GetTicksNS();
    if (now >= state.nextRepeat)
    {
        state.nextRepeat = now + mRepeatDelay;
        return true;
    }
    return false;
}

void PcKeyboardMouse::ReleaseAll()
{
    for (int input = 0; input < INPUT_COUNT; ++input)
    {
        mInputDown[input] = false;
        mActiveActions[input].clear();
    }
    for (int action = 0; action < ACTION_COUNT; ++action)
    {
        bool released = mActions[action].pendingReleased || mActions[action].down || mActions[action].held != 0;
        std::memset(&mActions[action], 0, sizeof(ActionState));
        mActions[action].pendingReleased = released;
    }
    std::memset(mFakeTicks, 0, sizeof(mFakeTicks));
    mMoveX = mMoveY = 0.0f;
    mMotionSinceTick = 0.0f;
    mPendingAnyPressed = mAnyPressed = false;
    mSprintLatched = false;
    DiscardRelativeInput();
}

bool PcKeyboardMouse::ActionDown(int action) const
{
    return action >= 0 && action < ACTION_COUNT && mActions[action].down;
}

bool PcKeyboardMouse::ActionPressed(int action) const
{
    return action >= 0 && action < ACTION_COUNT && mActions[action].pressed;
}

bool PcKeyboardMouse::ActionReleased(int action) const
{
    return action >= 0 && action < ACTION_COUNT && mActions[action].released;
}

bool PcKeyboardMouse::AnyInputPressed() const
{
    return mAnyPressed;
}

float PcKeyboardMouse::MoveX() const
{
    return mMoveX;
}

float PcKeyboardMouse::MoveY() const
{
    return mMoveY;
}

bool PcKeyboardMouse::ConsumeLook(float* x, float* y)
{
    if (mLookX == 0.0f && mLookY == 0.0f)
        return false;
    *x = mLookX;
    *y = mLookY;
    mLookX = 0.0f;
    mLookY = 0.0f;
    return true;
}

bool PcKeyboardMouse::ConsumeHotbar(int* slot, int* scroll)
{
    if (mHotbarSlot < 0 && mHotbarScroll == 0)
        return false;
    *slot = mHotbarSlot;
    *scroll = mHotbarScroll;
    mHotbarSlot = -1;
    mHotbarScroll = 0;
    return true;
}

int PcKeyboardMouse::ConsumeMenuWheel()
{
    int steps = mMenuWheel;
    mMenuWheel = 0;
    return steps;
}

bool PcKeyboardMouse::MouseMovedThisTick() const
{
    return mMovedThisTick;
}

bool PcKeyboardMouse::PointerMoved() const
{
    return mPointerMoved;
}

void PcKeyboardMouse::ClearPointerMoved()
{
    mPointerMoved = false;
}

float PcKeyboardMouse::PointerX() const
{
    return mPointerX;
}

float PcKeyboardMouse::PointerY() const
{
    return mPointerY;
}

void PcKeyboardMouse::SetPointer(float x, float y)
{
    mPointerX = x;
    mPointerY = y;
}

unsigned int PcKeyboardMouse::ConsumeClicks()
{
    unsigned int clicks = mClicks;
    mClicks = 0;
    return clicks;
}

void PcKeyboardMouse::DiscardRelativeInput()
{
    mLookX = 0.0f;
    mLookY = 0.0f;
    mWheelX = mWheelY = 0.0f;
    mHotbarSlot = -1;
    mHotbarScroll = 0;
    mMenuWheel = 0;
    mClicks = 0;
    mPointerMoved = false;
}

bool PcKeyboardMouse::AnyInputDown() const
{
    for (int input = 0; input < INPUT_COUNT; ++input)
    {
        if (mInputDown[input])
            return true;
    }
    return false;
}

void PcKeyboardMouse::SetFakeAction(int action, int ticks)
{
    if (action >= 0 && action < ACTION_COUNT && ticks > mFakeTicks[action])
        mFakeTicks[action] = ticks;
}
