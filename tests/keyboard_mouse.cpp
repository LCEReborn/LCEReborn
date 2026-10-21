#include "platform/pc/KeyboardMouse.hpp"
#include "platform/C4JInput.hpp"
#include "game/core/GameSettings.hpp"

#include <SDL3/SDL.h>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>

static void Require(bool condition, const char* message)
{
    if (!condition)
    {
        std::fprintf(stderr, "FAIL: %s\n", message);
        std::exit(1);
    }
}

static void Tick(PcKeyboardMouse& input, EPcInputContext context = EPcInputContext_Game)
{
    input.Update(context, 0.5f, 0.05f);
}

static void Key(PcKeyboardMouse& input, SDL_Scancode key, bool down,
                EPcInputContext context = EPcInputContext_Game, Uint64 timestamp = 0)
{
    SDL_Event event = {};
    event.type = down ? SDL_EVENT_KEY_DOWN : SDL_EVENT_KEY_UP;
    event.key.scancode = key;
    event.key.timestamp = timestamp;
    input.HandleEvent(event, context, context == EPcInputContext_Game);
}

static void Wheel(PcKeyboardMouse& input, float x, float y,
                  EPcInputContext context = EPcInputContext_Game, bool flipped = false)
{
    SDL_Event event = {};
    event.type = SDL_EVENT_MOUSE_WHEEL;
    event.wheel.x = x;
    event.wheel.y = y;
    event.wheel.direction = flipped ? SDL_MOUSEWHEEL_FLIPPED : SDL_MOUSEWHEEL_NORMAL;
    input.HandleEvent(event, context, context == EPcInputContext_Game);
}

static void Bind(PcKeyboardMouse& input, int action, int binding)
{
    input.SetBindings(EPcInputContext_Game, action, std::vector<int>(1, binding));
}

int main()
{
    Require(SDL_Init(SDL_INIT_EVENTS), "initialize SDL events");
    PcKeyboardMouse input;
    Key(input, SDL_SCANCODE_SPACE, true);
    Key(input, SDL_SCANCODE_SPACE, false);
    Tick(input);
    Require(input.ActionPressed(EControllerActions_Jump) && input.ActionReleased(EControllerActions_Jump),
            "short tap preserves both edges");
    Require(input.ActionDown(EControllerActions_Jump), "short tap reaches legacy down queries for one tick");
    Tick(input);
    Require(!input.ActionDown(EControllerActions_Jump), "short tap does not stick");
    Key(input, SDL_SCANCODE_SPACE, true);
    Tick(input);
    Key(input, SDL_SCANCODE_SPACE, false);
    Key(input, SDL_SCANCODE_SPACE, true);
    Tick(input);
    Require(input.ActionPressed(EControllerActions_Jump) && input.ActionReleased(EControllerActions_Jump)
            && input.ActionDown(EControllerActions_Jump), "release and repress between frames preserves edges");

    input.ReleaseAll();
    input.SetBindings(EPcInputContext_Game, EControllerActions_Jump,
                      {SDL_SCANCODE_SPACE, SDL_SCANCODE_J});
    Key(input, SDL_SCANCODE_SPACE, true);
    Key(input, SDL_SCANCODE_J, true);
    Tick(input);
    Key(input, SDL_SCANCODE_SPACE, false);
    Tick(input);
    Require(input.ActionDown(EControllerActions_Jump) && !input.ActionReleased(EControllerActions_Jump),
            "releasing one of two held bindings keeps the action held");
    Key(input, SDL_SCANCODE_J, false);
    Tick(input);
    Require(input.ActionReleased(EControllerActions_Jump) && !input.ActionDown(EControllerActions_Jump),
            "last binding releases action");

    input.SetDefaultBindings();
    Bind(input, EControllerActions_PickBlock, PcKeyboardMouse::MOD_CTRL | SDL_SCANCODE_Q);
    Key(input, SDL_SCANCODE_LCTRL, true);
    Key(input, SDL_SCANCODE_Q, true);
    Tick(input);
    Require(input.ActionPressed(EControllerActions_PickBlock) && !input.ActionPressed(EControllerActions_Drop),
            "specific chord suppresses plain binding");
    Key(input, SDL_SCANCODE_LCTRL, false);
    Tick(input);
    Require(!input.ActionDown(EControllerActions_PickBlock) && !input.ActionPressed(EControllerActions_Drop),
            "releasing chord modifier does not trigger plain action");
    Key(input, SDL_SCANCODE_Q, false);
    Key(input, SDL_SCANCODE_Q, true);
    Tick(input);
    Require(input.ActionPressed(EControllerActions_Drop), "plain binding works on a fresh key press");

    input.ReleaseAll();
    Key(input, SDL_SCANCODE_W, true);
    Tick(input);
    Require(input.MoveY() == 1.0f, "W moves forward");
    input.SetContext(EPcInputContext_Menu);
    Tick(input, EPcInputContext_Menu);
    Require(!input.ActionDown(EControllerActions_MoveForward) && !input.ActionPressed(EControllerActions_MenuUp),
            "opening menu clears gameplay without leaking W into navigation");
    input.SetContext(EPcInputContext_Game);
    Key(input, SDL_SCANCODE_LCTRL, true);
    Tick(input);
    Require(input.MoveY() == 0.0f, "modifier change cannot revive a key held across contexts");
    Key(input, SDL_SCANCODE_W, false);
    Key(input, SDL_SCANCODE_W, true);
    Tick(input);
    Require(input.MoveY() == 1.0f, "fresh press resumes gameplay");
    input.ReleaseAll();
    input.ReleaseAll();
    Require(!input.ActionDown(EControllerActions_MoveForward) && input.MoveY() == 0.0f,
            "focus loss clears state immediately");
    Tick(input);
    Require(input.ActionReleased(EControllerActions_MoveForward), "focus loss preserves release edge");
    Require(!input.AnyInputPressed() && !input.AnyInputDown(), "focus loss clears physical and pending input");

    SDL_Event motion = {};
    motion.type = SDL_EVENT_MOUSE_MOTION;
    motion.motion.xrel = 1.25f;
    motion.motion.yrel = -0.5f;
    input.HandleEvent(motion, EPcInputContext_Game, true);
    Tick(input);
    input.HandleEvent(motion, EPcInputContext_Game, true);
    float lookX = 0, lookY = 0;
    Require(input.ConsumeLook(&lookX, &lookY) && lookX == 2.5f && lookY == -1.0f,
            "relative motion accumulates across tick and late event pump");
    Require(!input.ConsumeLook(&lookX, &lookY), "relative motion is consumed once");
    input.HandleEvent(motion, EPcInputContext_Game, true);
    input.ReleaseAll();
    Require(!input.ConsumeLook(&lookX, &lookY), "focus loss discards relative motion");
    Key(input, SDL_SCANCODE_LSHIFT, true, EPcInputContext_Menu);
    SDL_Event click = {};
    click.type = SDL_EVENT_MOUSE_BUTTON_DOWN;
    click.button.button = SDL_BUTTON_LEFT;
    input.HandleEvent(click, EPcInputContext_Menu, false);
    Require(input.ConsumeClicks() == C_4JInput::EPcMenuClick_Quick, "Shift-click quick-moves in menus");
    input.ReleaseAll();

    input.SetToggleSprint(true);
    Key(input, SDL_SCANCODE_LCTRL, true);
    Key(input, SDL_SCANCODE_LCTRL, false);
    Tick(input);
    Require(input.ActionDown(EControllerActions_Sprint), "toggle sprint remains latched after release");
    Key(input, SDL_SCANCODE_LCTRL, true);
    Key(input, SDL_SCANCODE_LCTRL, false);
    Tick(input);
    Require(!input.ActionDown(EControllerActions_Sprint), "second tap disables toggle sprint");
    Key(input, SDL_SCANCODE_LCTRL, true);
    Tick(input);
    input.ReleaseAll();
    Tick(input);
    Require(!input.ActionDown(EControllerActions_Sprint), "focus loss resets toggle sprint");

    input.SetDefaultBindings();
    int slot = -1, scroll = 0;
    for (int n = 0; n < 3; ++n) Wheel(input, 0, 0.25f);
    Require(!input.ConsumeHotbar(&slot, &scroll), "partial wheel steps are retained");
    Wheel(input, 0, 0.25f);
    Require(input.ConsumeHotbar(&slot, &scroll) && scroll == -1, "fractional wheel adds to one hotbar step");
    Wheel(input, 0, 2, EPcInputContext_Game, true);
    Require(input.ConsumeHotbar(&slot, &scroll) && scroll == 2, "flipped wheel direction is normalized");
    Bind(input, EControllerActions_Inventory, PcKeyboardMouse::INPUT_WHEEL_RIGHT);
    Wheel(input, 0.5f, 0);
    Wheel(input, 0.5f, 0);
    Tick(input);
    Require(input.ActionPressed(EControllerActions_Inventory), "horizontal wheel binding");
    Bind(input, EControllerActions_Inventory, PcKeyboardMouse::MOD_CTRL | PcKeyboardMouse::INPUT_WHEEL_UP);
    Key(input, SDL_SCANCODE_RCTRL, true);
    Wheel(input, 0, 1);
    Tick(input);
    Require(input.ActionPressed(EControllerActions_Inventory) && !input.ConsumeHotbar(&slot, &scroll),
            "modified wheel binding consumes default hotbar scroll");
    input.ReleaseAll();
    Wheel(input, 0, 0.75f);
    input.SetContext(EPcInputContext_Menu);
    Wheel(input, 0, 0.25f, EPcInputContext_Menu);
    Require(input.ConsumeMenuWheel() == 0, "fractional wheel does not leak across contexts");
    Wheel(input, 0, 0.75f, EPcInputContext_Menu);
    Require(input.ConsumeMenuWheel() == 1, "fractional menu scrolling");

    input.BeginCapture();
    Key(input, SDL_SCANCODE_LCTRL, true, EPcInputContext_Menu);
    Require(input.ConsumeCapture() == C_4JInput::EKeyCapture_None, "capture waits for modifier chord");
    Key(input, SDL_SCANCODE_Q, true, EPcInputContext_Menu);
    Require(input.ConsumeCapture() == (PcKeyboardMouse::MOD_CTRL | SDL_SCANCODE_Q), "capture Ctrl+Q");
    Tick(input, EPcInputContext_Menu);
    Require(!input.AnyInputDown() && !input.AnyInputPressed(), "captured chord does not operate menus");
    input.BeginCapture();
    Key(input, SDL_SCANCODE_LSHIFT, true, EPcInputContext_Menu);
    Key(input, SDL_SCANCODE_LSHIFT, false, EPcInputContext_Menu);
    Require(input.ConsumeCapture() == SDL_SCANCODE_LSHIFT, "capture standalone modifier on release");
    input.BeginCapture();
    Wheel(input, -1, 0, EPcInputContext_Menu);
    Require(input.ConsumeCapture() == PcKeyboardMouse::INPUT_WHEEL_LEFT, "capture horizontal wheel");
    input.BeginCapture();
    Key(input, SDL_SCANCODE_ESCAPE, true, EPcInputContext_Menu);
    Require(input.ConsumeCapture() == C_4JInput::EKeyCapture_Cancelled, "Escape cancels capture");

    const char* names[] = {"Ctrl+Q", "Shift+Mouse Left", "Alt+Wheel Right", "Ctrl+Shift+Alt+Keypad +"};
    for (const char* name : names)
    {
        int binding = PcKeyboardMouse::InputFromName(name);
        Require(binding > 0 && PcKeyboardMouse::InputFromName(PcKeyboardMouse::InputName(binding)) == binding,
                "binding names round trip including keypad plus");
        Require(!PcKeyboardMouse::DisplayName(binding).empty(), "chord has display label");
    }
    const auto directory = std::filesystem::temp_directory_path()
                           / ("lce-input-test-" + std::to_string(SDL_GetTicksNS()));
    std::filesystem::create_directory(directory);
    const auto bindingsPath = directory / "keybinds.txt";
    const auto settingsPath = directory / "input.txt";
    input.SetBindings(EPcInputContext_Game, EControllerActions_Jump, {});
    Require(input.SaveBindings(bindingsPath.string()), "save bindings");
    PcKeyboardMouse loaded;
    loaded.LoadBindings(bindingsPath.string());
    Require(loaded.GetBindings(EPcInputContext_Game, EControllerActions_Jump).empty(),
            "unbound default stays unbound after restart");
    Require(loaded.GetBindings(EPcInputContext_Game, EControllerActions_Inventory)
            == input.GetBindings(EPcInputContext_Game, EControllerActions_Inventory), "chord persists after restart");
    {
        std::ofstream file(bindingsPath);
        file << "game.Jump = MisspelledKey\n";
    }
    loaded.SetDefaultBindings();
    loaded.LoadBindings(bindingsPath.string());
    Require(!loaded.GetBindings(EPcInputContext_Game, EControllerActions_Jump).empty(),
            "invalid config preserves working default");
    loaded.SetSetting(C_4JInput::EKeyboardMouseSetting_Sensitivity, 500);
    loaded.SetSetting(C_4JInput::EKeyboardMouseSetting_ToggleSneak, 1);
    loaded.SetSetting(C_4JInput::EKeyboardMouseSetting_ToggleSprint, 1);
    Require(loaded.SaveSettings(settingsPath.string()), "save PC settings");
    PcKeyboardMouse settings;
    settings.LoadSettings(settingsPath.string());
    Require(settings.GetSetting(C_4JInput::EKeyboardMouseSetting_Sensitivity) == 200
            && settings.GetSetting(C_4JInput::EKeyboardMouseSetting_ToggleSneak) == 1
            && settings.GetSetting(C_4JInput::EKeyboardMouseSetting_ToggleSprint) == 1,
            "settings clamp and persist");
    std::filesystem::remove_all(directory);

    input.ReleaseAll();
    input.SetDefaultBindings();
    input.SetContext(EPcInputContext_Game);
    Uint64 old = SDL_GetTicksNS();
    // An old event should repeat immediately once the initial query has run.
    Key(input, SDL_SCANCODE_SPACE, true, EPcInputContext_Game, old > SDL_NS_PER_SECOND ? old - SDL_NS_PER_SECOND : 1);
    input.Update(EPcInputContext_Game, 0.001f, 0.05f);
    Require(input.ActionRepeat(EControllerActions_Jump), "first repeat query reports initial press");
    if (SDL_GetTicksNS() < 1000001) SDL_Delay(2);
    Require(input.ActionRepeat(EControllerActions_Jump), "repeat deadline uses event timestamp");
    SDL_Quit();
    std::puts("PASS: ordered input edges, contexts, chords, toggles, wheel, capture and persistence");
}
