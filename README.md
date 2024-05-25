# ControllerImage

## What is this?

This is a library that provides resolution-independent images for game
controller inputs, so games can show the correct visual markers for the
gamepad in the user's hand.

This is intended, for example, to let you show a "Press `image` to continue"
message, where `image` might be a yellow 'Y' if the user has an Xbox
controller and a green triangle if they have a PlayStation controller.

It is intended to work with whatever looks like a gamepad to SDL3, but it
requires artwork, in SVG format, for whatever controller the user happens to
be using. The standard ones--Xbox, PlayStation, Switch--are supplied, and if
we can't offer anything better, we'll aim for a generic Xbox 360 controller,
under the assumption it _probably_ represents most generic third-party devices.

We need submissions of more controller art! It must be in
[SVG format](https://en.wikipedia.org/wiki/SVG), and ideally it's as small as
possible while still looking as accurate as possible. If you can help,
and offer artwork in the public domain, please get in touch.

## How do I use the library?

- This library relies on SDL3. If you are using SDL2, please upgrade. If you
  aren't using SDL at all, this library won't be usable.
- Build it. It's one C file and a few headers, you can copy them into your
  project. Just compile controllerimage.c as part of your project. You don't
  need to mess around with the CMake file to build a separate library if you
  don't want to.
- In your app near startup (preferably after SDL_Init)...
  ```c
  if (ControllerImage_Init() == -1) {
      SDL_Log("ControllerImage_Init() failed! why='%s'", SDL_GetError());
  }
  ```
- Load in the controller and image data...
  ```c
  // there are also versions that can load from an SDL_RWops or a memory buffer...
  if (ControllerImage_AddDataFromFile("controllerimages-standard.bin") == -1) {
      SDL_Log("ControllerImage_AddDataFromFile() failed! why='%s'", SDL_GetError());
  }
  ```
- Have a controller? Prepare to get images for it, probably when you open the gamepad:
  ```c
  // There's a version that accepts a device instance ID and not an `SDL_Gamepad *`, too.
  ControllerImage_GamepadDevice *imgdev = ControllerImage_CreateGamepadDevice(mySdlGamepad);
  if (!imgdev) {
      SDL_Log("ControllerImage_CreateGamepadDevice() failed! why='%s'", SDL_GetError());
  }
  ```
- Get image data for specific buttons or axes on the gamepad. They come in as
  SDL_Surfaces, so you can manipulate them, or upload them to SDL_Textures.
  You choose the size of the image; a game running at 4K might want a larger
  one than a game running at 720p, so it always looks crisp on the display
  without scaling. Lastly, you can specify a variant, so different images 
  can be loaded for the same button. This makes it easy to add alternate
  versions for things like button presses, or specific axis directions.   
  ```c
  SDL_Surface *axissurf = ControllerImage_CreateSurfaceForAxis(imgdev, SDL_GAMEPAD_AXIS_LEFTX, 100, 100, 0);
  if (!axissurf) {
      SDL_Log("Render axis failed! why='%s'", SDL_GetError());
  }
  SDL_Surface *buttonsurf = ControllerImage_CreateSurfaceForButton(imgdev, SDL_GAMEPAD_BUTTON_GUIDE, 100, 100, 0);
  if (!buttonsurf) {
      SDL_Log("Render button failed! why='%s'", SDL_GetError());
  }
  ```
- Done with this controller? Free up resources...
  ```c
  ControllerImage_DestroyGamepadDevice(imgdev);
  ```
- At app shutdown...
  ```c
  ControllerImage_Quit();  // safe even if ControllerImage_Init() failed!
  ```

There is also support for generic mouse and keyboard devices, with this same style of interface.

## How do I get the data file I need?

Compile the C file "src/make-controllerimage-data.c". It should compile
without any dependencies.

Run that with the "art" directory as its only command line argument.
It will produce a "controllerimage-standard.bin" file in the current working
directory. This is the data you pass to the library. It will also build other
themes that can be overlayed on top of the "standard" theme; these are
optional.

The library is designed to let you add to and replace existing data with
multiple files, so you can add more files that just fix things and add new
controllers without having to replace earlier data files completely in a
patch, and load them in order to get the same results, but a tool to generate
subsets of data hasn't been written yet.

## What if I want to make my own art?

No problem! Lots of games want to have controller images that match their
style.

Generally I would recommend you build out the most popular controllers, put
them in their own subdirectory under the "art" directory, and build it as
a second database. Ship both that file and the "standard" database with the
game, loading the "standard" one first, then your custom one second, so it
replaces pieces of the first database.

This way, the most common controllers will match your game's art style, your
artists didn't have to build a massive amount of art, and if someone comes
along with an obscure controller, they still see the right thing, just perhaps
without a perfect style match.

If you want to contribute your new art in the public domain, we will be happy
to include it with this project, so other people making games with the same
vibe can take advantage of it!

Regarding the format in the art folder; each SVG filename is split into two
parts, like this: `<buttonname>_<variantID>.svg`. `<buttonname>` is sourced
from SDL directly - using `SDL_GetGamepadButtonFromString`. The `<variantID>`
is relevant only to ControllerImage, and by default can load up to 
`CONTROLLERIMAGE_MAX_GAMEPAD_VARIANTS` (currently 8, with a minimum of 1). 
The "default" configuration for any given button is `0`, and if a variant doesn't 
exist, that will be loaded instead.

For keyboard keys, the names are also sourced from SDL, but this has some caveats.
First, names are checked using `SDL_GetScancodeFromName`. This covers most keys,
and will properly handle OS-specific key changes. So for example, when SDL detects
a Windows keyboard, `left_windows_0.svg` will be loaded for `SDL_SCANCODE_LGUI`,
and for Linux, `left_gui_0.svg` will be loaded instead.

Naturally, there are several keys that aren't permissable as filenames directly; 
so we have a special table at the top of `controllerimage.c` to mitigate this,
and keep all of our filenames human-readable. This is also true for mouse buttons,
which have no strings we can source from SDL.

