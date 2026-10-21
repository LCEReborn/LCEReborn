# PC input

The game reads input through `C_4JInput`: players are bound to controller
slots, the game asks whether an action (`EControllerActions`) is down, pressed
or released, and reads stick values for movement and looking. The Wii U layer
answers those questions from a GamePad or a Pro Controller. On PC the same
questions are answered by two kinds of device: gamepads, and the keyboard and
mouse together.

The keyboard and mouse are a device in their own right. They are not dressed up
as a gamepad: actions are bound to keys and mouse buttons, looking uses the raw
mouse movement, the hotbar is selected directly, and menus are driven by a
pointer.

## Devices and slots

`CInput` owns five controller slots, the same as the Wii U layer.

| Slot | Device |
| --- | --- |
| 0 | keyboard and mouse (always connected) |
| 1-4 | gamepads, in the order they are connected |

A player signs in on whichever device they press a button on, exactly as on a
console, so a gamepad and the keyboard can be used by two players at once.

## Keyboard and mouse

`platform/pc/KeyboardMouse.hpp` keeps the state of the keyboard and mouse and
answers the action queries for slot 0. It has three parts.

**Bindings.** Every action the game asks about has a list of inputs: keys,
mouse buttons or the wheel. Menu actions and in-game actions are bound
separately, as the game treats them separately. The defaults follow Minecraft
on PC; the player's own bindings are kept in `keybinds.txt` in the save folder,
one action per line, using SDL's key names, and that file is rewritten whenever
the bindings change.

**Action state.** SDL events update actions in arrival order through an index
from inputs to bindings. A press and release between frames preserves both
edges; releasing and pressing again also registers while an action was already
down. An action with several bindings stays held until its last held binding
releases. Repeat deadlines use the SDL event timestamp. Once per input tick,
`ButtonDown`, `ButtonPressed`, `ButtonReleased` and `GetValue` receive that
state. For the game's existing down queries, a short tap remains down for one
input tick as well.

Only the current menu or gameplay context can activate bindings. Changing
contexts clears pending actions, wheel fractions and sprint toggles; a key
held across the change must be released and pressed again. Losing focus clears
held keys, mouse motion and toggles immediately.

Wheel and touchpad input accumulates fractional steps separately on each axis.
Vertical scrolling selects hotbar slots or scrolls menus by default. An explicit
wheel binding consumes that direction instead; horizontal scrolling is bindable
as Wheel Left/Right. SDL's flipped wheel direction is normalized.

**Direct input.** Some PC controls have no console action:

| Control | How the game receives it |
| --- | --- |
| Mouse look | `ConsumeMouseLook`: the raw mouse delta, turned every frame in `GameRenderer::render` |
| Hotbar keys and wheel | `ConsumeHotbarInput`: a slot to select, or a number of slots to scroll, applied where the game cycles the held item |
| Menu pointer | `GetMenuPointer`: the cursor position in UI space; `ConsumeMenuPointer` also reports whether it moved |
| Menu clicks and wheel | `ConsumeMenuClicks` / `ConsumeMenuWheel`: the clicks and wheel steps since the last frame |
| Text | `RequestKeyboard`: the typed text, edited with the usual keys |

Movement keys feed the left stick for slot 0 so that the game's movement code,
which reads the stick, needs no change.

## Menus

Console menus move a highlight with the stick and press the highlighted
control. On PC the mouse also points at controls directly:

- moving the cursor over a control highlights it;
- clicking presses it;
- the wheel moves the highlight up and down, scrolling lists with it;
- the right button or Escape goes back.

The UI already knows where every control is on screen (the hit boxes it keeps
for the GamePad's touch screen); the pointer uses that.
`ConsoleUIController::HandlePointerInput` runs once per frame for the keyboard
player: when the cursor moves it highlights the control under it the way a
touch would, the wheel and the right button are sent to the scene as menu
actions, and a left click goes through the touch path, so it presses exactly
what a tap on the GamePad would. A cursor that is not moving never takes the
highlight away from the keys. Containers (inventory, chests, crafting) draw
their own pointer, which follows the mouse exactly (it does not jump to the
centre of the slot under it, as the stick's pointer does) and handle their own
clicks; the system cursor is hidden there so only one pointer shows. A
container's pointer opens where the mouse is, and when the game releases the
mouse it puts the cursor in the middle of the window (of the player's own
screen for a container), so the pointer never jumps on the first move. In the
creative menu the wheel turns the page. The system cursor is also hidden while
a gamepad is the device in use.

## Players and devices

Each player plays on one device, as on a console. On the title screen and in
the main menu any keyboard or controller drives the menus, and the device that
picks a main-menu option becomes player 1. In game, double-tapping Start on
another controller, or Escape on a keyboard nobody is playing on, joins it as
the next split-screen player. `--split-screen off` turns split screen off:
no one else can join, the player-select screen before a world is skipped, and
player 1 moves to whichever connected keyboard or controller was used last.
The prompt that asks other connected devices to join as a world loads is off
unless `--join-prompt on` is given, since the keyboard always counts as one.

A gamepad's face buttons follow the controller's own edition of the game: the
button labelled A or Cross (the button in the confirm position on a Nintendo
controller) confirms and the one labelled B or Circle goes back, while the top
button opens the inventory and the left one crafting, so Y and X on an Xbox
controller, Triangle and Square on a PlayStation one, and X and Y on a Nintendo
one (`CInput::ResolveFaceButtons`).

## Prompts

The button prompts along the bottom of the screen, and the buttons named in
tutorial and help text, show the buttons of the device the player is using.
A player's prompts follow the device that player is on; before anyone has
picked an option in the main menu, they follow whichever device was used last.

- On the keyboard they show the key bound to the action as a keycap (`E`
  Inventory, `Esc` Back), or a mouse with the button lit. Movement shows the
  four movement keys and looking shows the mouse. In containers, where the mouse
  moves the pointer, they show what the clicks do: the left button to take or
  place, the right button for half a stack or one item, Shift and the left
  button to move a stack across.
- On a gamepad they show that controller's own buttons. The controller is
  recognised from what SDL reports about it: Xbox (coloured A, B, X, Y, LB, RT,
  View and Menu), PlayStation 3, 4 or 5 (Cross, Circle, Square, Triangle, L1, R2,
  and Select, Share or Create), Nintendo (A, B, X, Y, L, ZR, plus and minus) or
  any other controller (plain A, B, X, Y). The face buttons are drawn from the
  label SDL gives the physical button, so the picture is the button the player
  has to press.

The game asks `C_4JInput::GetPromptImages` for the pictures for an action and
the console buttons it is mapped to; the platform answers for the player's
device, with keyboard inputs for the keyboard and controller buttons for a
gamepad (`platform/pc/CInput.cpp`). Prompt text shows them as inline images
(`<img src="pcprompt:N">`), which the interface's text renderer draws from
`platform/pc/PcPromptArt`. In-game prompts know their action; menu prompts are
given by button position (A, B, X, Y, triggers, bumpers...), and each position
stands for the menu action the controller maps to it: A is `MenuOk`, B is
`MenuCancel`, the triggers page, the bumpers change tab, and so on.

The art is in `resources/pc/ui`, drawn by `tools/pcart/make_ui_art.py` and
copied next to the executable when it is built: the keycap, the mouse, the
arrows, the controller button shapes (round, stick, bumper, trigger, pill and
d-pad, in marker colours each controller's colours replace) and the button
symbols. Labels are not part of the art: the game writes them in the Mojangles
font, on the font's own pixel grid, when it first needs a picture, and scales
the result by whole numbers so it stays sharp.

## Rebinding controls

Help & Options > Controls rebinds the controls of whichever device the player
is using, and every row shows that device's buttons or keys.

- **Controllers** use the game's own rebinding: choose a row, press the new
  button, and the mapping is stored in the player's profile in the game's
  button numbering, so it follows the face button positions on every
  controller family. The help text and the cancel prompt show the
  controller's own Back button (View, Create, Minus).
- **Keyboard and mouse:** choosing a row starts a capture in
  `PcKeyboardMouse`. The next key, mouse button or wheel step becomes that
  action's only binding and is written to `keybinds.txt` straight away; Esc
  cancels. Hold Ctrl, Shift or Alt while pressing a key, mouse button or wheel
  direction to capture a chord. Pressing and releasing a modifier by itself
  binds that modifier. Captured presses never reach menu actions.
  Chords display as labelled keycaps in the Controls menu and prompts.
- On PC a controller row waits for a physical button rather than a menu
  action, so a remapped menu button never hides the real one.
- **Inventory** rows (Quick Move, Take Half) remap the container buttons.
  A controller keeps them in two spare bytes of the profile after the action
  mappings (0 means the original button, so older profiles are unchanged) and
  accepts any button except confirm, back and the d-pad. On the keyboard,
  Quick Move is the key held while clicking (Shift by default) and Take Half
  is X. Take Everything stays on the tab-switching button it shares in the
  original game, so it has no row of its own.
- **Other**: Chat on every device. The keyboard also has Player List (Tab),
  Hide HUD (F1), which hides the HUD and hand together and shows them again,
  Command (`/`), which opens chat with a command started, and Debug Overlay
  (F3).
- Both devices share the game's conflict rule: binding an input already used
  by another action in the same group clears that action and asks the player
  to give it a new one. Leaving the menu with a required action unbound
  raises the game's warning. Reset to Defaults resets the keyboard bindings as
  well when the keyboard is the player's device.

Bindings in `keybinds.txt` can also use chords, for example:

```text
game.PickBlock = Ctrl+Q
game.Inventory = Alt+Wheel Right
menu.QuickMove = Left Shift, Right Shift
```

Ctrl/Shift/Alt match either side of the keyboard. For the same primary input,
the matching chord with the most modifiers takes precedence over a plain
binding. Chords activate when the primary input is pressed with its modifiers
held. Releasing a modifier releases that chord without firing the plain
binding. Modifier keys can still operate their own actions, such as Ctrl for
sprint. Empty binding lines persist deliberately unbound actions; a misspelled
input leaves the previous binding in place and logs a warning.

## Keyboard and mouse settings

Help & Options > Settings > Game Options shows Mouse Sensitivity, Toggle Sneak and
Toggle Sprint when using the keyboard. Mouse Sensitivity is independent of
the existing in-game sensitivity slider used for controllers. It defaults to
100% and ranges from 1% to 200%. Sneak and sprint default to hold; enabling
Toggle Sneak restores press-to-toggle crouching, and Toggle Sprint latches the
sprint key until another press, a context change or focus loss. The normal
in-game restrictions on sprinting still apply.

The Controls menu’s Hold to Sneak checkbox updates the same keyboard setting
as Toggle Sneak, with the opposite value. Controllers retain their profile’s
Hold to Sneak option.

These settings are shared by the keyboard device and saved in `input.txt`
beside `keybinds.txt`:

```text
MouseSensitivity = 100
ToggleSneak = 0
ToggleSprint = 0
```

## Native input and latency

Windows enables SDL's [raw keyboard events](https://wiki.libsdl.org/SDL3/SDL_HINT_WINDOWS_RAW_KEYBOARD).
Text entry continues through SDL text input and IME; committed text and
composition updates have separate paths. An explicit SDL environment hint
can override the raw-keyboard default. Linux uses SDL's Wayland relative
pointer and locking support, or the X11 backend when selected. Mouse look
uses [relative mouse mode](https://wiki.libsdl.org/SDL3/SDL_SetWindowRelativeMouseMode)
with system acceleration disabled by default. Fullscreen menus confine the
pointer while focused and release it on focus loss.

The frame waits for its swapchain image before sampling input. Near 60 Hz,
VSync supplies the frame timing with a slightly higher timer cap so the two
clocks do not drift into an extra wait after input. Other refresh rates and
VSync-off keep the existing 60 fps cap. The renderer pumps pending events
again immediately before applying mouse look, retaining action events for
the next input tick while consuming the latest motion for this frame.

The native Linux build and Wayland/Vulkan presentation can be verified here;
Windows raw keyboard behavior still needs a Windows runtime check.

The input regression suite is part of `LCE_BUILD_RENDER_TESTS`:

```sh
cmake --build build/linux --target lce_keyboard_mouse
ctest --test-dir build/linux --output-on-failure
```

## What changes in the game

The game core stays platform-pure. Where PC input needs the game to do
something new (turning by the mouse, selecting a hotbar slot, hovering a
control), the change sits behind `#if LCE_PLATFORM_PC` and the Wii U code is
untouched.
