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

**Action state.** Once per input tick the bound inputs are sampled, and each
action records whether it is down, whether it was pressed or released this
tick, and when it should next repeat. `ButtonDown`, `ButtonPressed`,
`ButtonReleased` and `GetValue` read this directly for slot 0.

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
clicks; the system cursor is hidden there so only one pointer shows. The
system cursor is also hidden while a gamepad is the device in use.

## Players and devices

Each player plays on one device, as on a console. On the title screen and in
the main menu any keyboard or controller drives the menus, and the device that
picks a main-menu option becomes player 1. In game, double-tapping Start on
another controller, or Escape on a keyboard nobody is playing on, joins it as
the next split-screen player.

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
  cancels. The captured press never reaches the menus, so Enter, Esc or a
  click can be bound like any other input.
- On PC a controller row waits for a physical button rather than a menu
  action, so a remapped menu button never hides the real one.
- **Inventory** rows (Quick Move, Take Half, Take Everything) remap the
  container buttons. A controller keeps them in three spare bytes of the
  profile after the action mappings (0 means the original button, so older
  profiles are unchanged) and accepts any button except confirm, back and
  the d-pad. On the keyboard, Quick Move is the key held while clicking
  (Shift by default), Take Half and Take Everything are the X and Q keys.
- **Other** (keyboard only): Player List (Tab) and Hide HUD (F1), which hides
  the HUD and hand together and shows them again.
- Both devices share the game's conflict rule: binding an input already used
  by another action in the same group clears that action and asks the player
  to give it a new one. Leaving the menu with a required action unbound
  raises the game's warning. Reset to Defaults resets the keyboard bindings as
  well when the keyboard is the player's device.

## What changes in the game

The game core stays platform-pure. Where PC input needs the game to do
something new (turning by the mouse, selecting a hotbar slot, hovering a
control), the change sits behind `#if LCE_PLATFORM_PC` and the Wii U code is
untouched, so the Wii U build still produces the same RPX.
