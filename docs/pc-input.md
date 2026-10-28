# PC input

The game reads input through `C_4JInput`: players are bound to controller
slots, the game asks whether an action (`EControllerActions`) is down, pressed
or released, and reads stick values for movement and looking. The Wii U layer
answers those questions from a GamePad or a Pro Controller. On PC the same
questions are answered by two kinds of device: gamepads, and the keyboard and
mouse together, which a touchscreen joins.

The keyboard and mouse are a device in their own right. They are not dressed up
as a gamepad: actions are bound to keys and mouse buttons, looking uses the raw
mouse movement, the hotbar is selected directly, and menus are driven by a
pointer.

## Devices and slots

`CInput` owns five controller slots, the same as the Wii U layer.

| Slot | Device |
| --- | --- |
| 0 | keyboard and mouse, and a touchscreen (always connected) |
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
With `--profiles` (the launcher passes its own), a player who joins first
picks which unused launcher profile to play as, with its name and identity.
Each player sets their own field of view in More Options.

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

## Touchscreen

A touchscreen plays the way Minecraft plays on phones and tablets, drawn in the
interface's own style. It belongs to slot 0 with the keyboard and mouse: the
first touch makes it the device in use, which shows the touch controls, hides
the system cursor and turns the prompts into plain words; the next key press
or mouse movement goes back to the keyboard and mouse. Only screens touched
directly count. Touchpads, which also report fingers, stay a mouse, and the
mouse events the system makes up from touches are ignored, so a touch is never
seen twice. `platform/pc/PcTouch.hpp` keeps the fingers and draws the controls
over the finished frame.

**In menus** a finger is a pointer, as on the GamePad's touch screen: touching
a control highlights it and lifting the finger presses it, dragging scrolls
and moves sliders, and the prompts along the bottom are buttons, so Back,
Select and the rest can be tapped.

Containers (the inventory, chests, crafting, the creative menu) move items as
Minecraft does on phones, through the container's own take, place and quick
move so every move is one the server already understands:

- A tap takes the whole stack under the finger, and a tap on another slot
  places it there, swapping or merging as a click would.
- Holding still on a stack for 0.525 seconds starts splitting it: a bar over
  the finger fills and the amount above it grows to the whole stack over the
  next second, and lifting the finger takes that many. Holding on a slot
  while carrying a stack places that many from it instead.
- Resting on a stack for 0.18 seconds and then moving drags it; it goes to
  the slot the finger lifts over, or is dropped if that is outside the
  container. Whatever the drop leaves on the finger (the other stack of a
  swap, what did not fit on a full stack, or the whole stack if the slot does
  not take it) goes back to the slot the drag started from. In the creative
  menu an item dragged from the list goes to the hotbar slot it is dropped
  on, and a hotbar item dragged back onto the list is deleted.
- A double tap sends the stack to the other part of the container, as quick
  move does.
- Swiping up or down before the finger has rested scrolls, which turns the
  creative menu's pages.

**In game** the Controls menu's Touch Controls row picks one of three layouts,
those of Minecraft on phones (stored per profile in a spare settings field):

| Layout | Moving | Breaking and using |
| --- | --- | --- |
| Joystick & Tap (default) | a joystick that appears under the thumb anywhere in the lower left | at the finger |
| Joystick & Crosshair | the same joystick | at the crosshair, with Attack and Use buttons |
| D-Pad & Tap | arrow buttons; diagonals show while walking forward, and the finger can slide between them | at the finger |

- Dragging anywhere that is not a control looks around: a drag across the
  whole width of the screen turns 180 degrees at the default sensitivity, and
  Look Sensitivity and Invert Look apply as they do to the mouse.
- With the tap layouts there is no crosshair. A short tap uses what is held on
  the block under the finger (placing a block, opening a door or a chest) or
  attacks a mob there; holding still breaks the block under the finger, or
  attacks the mob, until the finger lifts, and dragging while holding keeps
  breaking as the view turns. Food, bows, shields and other held items are
  used by holding instead. Taps and holds aim through the touched point, so
  the block outline appears under the finger.
- Jump is at the bottom right; tapping it twice quickly starts and stops
  flying, as the jump button does. Sneak sits beside it (the centre of the
  d-pad in that layout): a tap turns sneaking on or off, and holding it sneaks
  until it is let go. While flying or swimming they become up and down, and
  while riding Sneak becomes Dismount. Tapping fly down twice quickly stops
  flying, as on Bedrock.
- The joystick layouts have a sprint button beside sneak. Tapping it turns
  sprinting on for as long as the player keeps walking forward, and again
  turns it off; it fades while the player is too hungry to sprint. Pushing
  the joystick past its edge while walking forward also sprints (Sprint Using
  the Joystick), and double-tapping forward on the d-pad sprints as it does on
  a stick.
- When the middle of the view is on a mob or vehicle the player can do
  something with, a button above the hotbar names it, as Bedrock's interact
  button does: Ride, Mount, Sail, Feed, Milk, Shear, Trade, Name, Leash and
  the rest, the same words the game's own prompts use. Tapping it uses what is
  held on that mob, so mobs can be ridden, fed and traded with even though a
  tap on a mob attacks it. The label is written in the game's font; in a
  language the font cannot write, the button shows an icon instead.
- Tapping a hotbar slot selects it; holding one drops its items one at a time.
  The two buttons after the hotbar open the inventory and the crafting menu,
  and pause and chat are at the top right.
- The controls scale by whole numbers with the window's height, as the rest
  of the interface does, and fade with the Interface Opacity setting (fully
  drawn at its default of 80% and above). On narrow windows, where they would
  meet the hotbar's buttons, jump, sneak and sprint move up above them.
- Tutorial hints and How to Play name the touch buttons by their icons while
  the touchscreen is in use, so "press Sneak" in a boat shows the Dismount
  button rather than a key.

Below Touch Controls, the Controls menu has Bedrock's other touch options.
They are shown when the computer has a touchscreen or one has been used, and
are stored with the layout in the profile:

| Option | Default | Effect |
| --- | --- | --- |
| Pick Block Button | off | a button above jump; tapping it and then a block picks that block, or picks at once with the crosshair |
| Left-Handed Inventory | off | the inventory and crafting buttons sit left of the hotbar |
| Sprint Using the Joystick | on | pushing the joystick past its edge sprints |
| Camera Perspective Button | off | a button beside chat that changes the camera view |
| Joystick | Always Visible | Always Visible, Hidden When Unused or Always Hidden; it works the same either way |
| Swap Jump and Sneak | off | sneak takes the outer corner and jump moves beside it |

The buttons, their names and options come from Bedrock Edition 26.32's
touch settings, button textures (`interact`, `sprint`, `pick_block`,
`perspective`) and input names (`button.sprint_toggle`,
`button.block_select_toggle`), and its HUD keeps the interact button above
the hotbar. The timings follow Minecraft on phones too (read from the same
version): a
tap lifts before the hold time, which is 0.3 seconds with the joystick and
0.4 seconds with the d-pad; a finger moving further than 3% of the screen's
height looks around instead; sneaking turns into holding after 0.4 seconds;
and in containers a drag needs 0.18 seconds of rest and splitting starts after
0.525 seconds (Bedrock's `ctrl_dwellbeforedragtime` and
`ctrl_stacksplittingtriggertime`). The art is `resources/pc/ui/touch.png`, drawn by
`tools/pcart/make_ui_art.py` like the rest of the PC interface art: button
frames shaped like the hotbar's slots, an icon for each button and the
joystick's pad.

## What changes in the game

The game core stays platform-pure. Where PC input needs the game to do
something new (turning by the mouse or a finger, selecting a hotbar slot,
hovering a control, aiming at the touched point), the change sits behind
`#if LCE_PLATFORM_PC` and the Wii U code is untouched. For the touch controls
the HUD reports to the input layer each frame where the hotbar is, the touch
options and Interface Opacity, and whether the player is flying, swimming,
riding, sneaking, able to sprint or holding something used by holding;
`GameRenderer::pick` aims through the touched point and reports whether it
found a block or a mob. With the tap layouts and no finger aiming, it picks
through the middle of the view only to ask the mob there for its interaction
prompt (`Entity::GetInteractTooltip`) for the interact button, then drops
that pick so nothing is outlined or used there.
