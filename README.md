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
under the assumption it _probably_ represent most generic third-party devices.

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

To be written (literally, the code is yet to be written, stay tuned!).

