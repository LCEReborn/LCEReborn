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
highlight away from the keys. Containers (inventory, chests, crafting) move
their own pointer, which follows the mouse, and handle their own clicks.

## Prompts

The button prompts along the bottom of the screen name the key or mouse
button bound to each action while the keyboard is in use, for example
`[E] Inventory` or `[Escape] Back`, and switch back to the controller's
button pictures as soon as a gamepad is used. A signed-in player's prompts
follow the device that player is signed in on; before anyone is signed in they
follow whichever device was used last (`UseKeyboardPrompts`). The names come
from the player's own bindings (`GetPromptKeyName`) and use the keyboard
layout's key names, so rebinding an action changes its prompt.

In-game prompts already know their action. Menu prompts are given by button
position (A, B, X, Y, triggers, bumpers...), and each position stands for the
menu action the controller maps to it: A is `MenuOk`, B is `MenuCancel`, the
triggers page, the bumpers change tab, and so on.

## What changes in the game

The game core stays platform-pure. Where PC input needs the game to do
something new (turning by the mouse, selecting a hotbar slot, hovering a
control), the change sits behind `#if LCE_PLATFORM_PC` and the Wii U code is
untouched, so the Wii U build still produces the same RPX.
