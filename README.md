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
  if (ControllerImage_AddDataFromFile("controllerimages.bin") == -1) {
      SDL_Log("ControllerImage_AddDataFromFile() failed! why='%s'", SDL_GetError());
  }
  ```
- Have a controller? Prepare to get images for it, probably when you open the gamepad:
  ```c
  // There's a version that accepts a device instance ID and not an `SDL_Gamepad *`, too.
  ControllerImage_Device *imgdev = ControllerImage_CreateGamepadDevice(mySdlGamepad);
  if (!imgdev) {
      SDL_Log("ControllerImage_CreateGamepadDevice() failed! why='%s'", SDL_GetError());
  }
  ```
- Get image data for specific buttons or axes on the gamepad. They come in as
  SDL_Surfaces, so you can manipulate them, or upload them to SDL_Textures.
  You choose the size of the image; a game running at 4K might want a larger
  one than a game running at 720p, so it always looks crisp on the display
  without scaling.
  ```c
  SDL_Surface *axissurf = ControllerImage_CreateSurfaceForAxis(imgdev, SDL_GAMEPAD_AXIS_LEFTX, 100, 100);
  if (!axissurf) {
      SDL_Log("Render axis failed! why='%s'", SDL_GetError());
  }
  SDL_Surface *buttonsurf = ControllerImage_CreateSurfaceForButton(imgdev, SDL_GAMEPAD_BUTTON_GUIDE, 100, 100);
  if (!buttonsurf) {
      SDL_Log("Render button failed! why='%s'", SDL_GetError());
  }
  ```
- Done with this controller? Free up resources...
  ```c
  ControllerImage_DestroyDevice(imgdev);
  ```
- At app shutdown...
  ```c
  ControllerImage_Quit();  // safe even if ControllerImage_Init() failed!
  ```

## How do I get the data file I need?

Compile the C file "src/make-controllerimage-data.c". It should compile
without any dependencies.

Run that with the "art" directory as its only command line argument.
It will produce a "controllerimage-standard.bin" file in the current working
directory. This is the data you pass to the library.

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

