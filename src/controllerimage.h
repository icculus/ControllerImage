/*
 * ControllerImage; A simple way to obtain game controller images.
 *
 * Please see the file LICENSE.txt in the source's root directory.
 *
 *  This file written by Ryan C. Gordon.
 */

#ifndef INCL_CONTROLLERIMAGE_H_
#define INCL_CONTROLLERIMAGE_H_

/* This library depends on SDL3. https://libsdl.org/ */
#include <SDL3/SDL.h>
#include <SDL3/SDL_version.h>
#include <SDL3/SDL_begin_code.h>

/* Set up for C function definitions, even when using C++ */
#ifdef __cplusplus
extern "C" {
#endif

/* Version checks... */

#define CONTROLLERIMAGE_MAJOR_VERSION   0
#define CONTROLLERIMAGE_MINOR_VERSION   0
#define CONTROLLERIMAGE_PATCHLEVEL      1

#define CONTROLLERIMAGE_VERSION(X) {                                                   \
    (X)->major = CONTROLLERIMAGE_MAJOR_VERSION; \
    (X)->minor = CONTROLLERIMAGE_MINOR_VERSION; \
    (X)->patch = CONTROLLERIMAGE_PATCHLEVEL; \
}

// "rightshoulder" at 13 chars is the longest supported button name thusfar
// keyboard scancode names could be longer, so this is bumped up
#define CONTROLLERIMAGE_MAX_FILENAME_LENGTH 64
#define CONTROLLERIMAGE_MAX_GAMEPAD_VARIANTS 8
#define CONTROLLERIMAGE_MAX_KEYBOARD_VARIANTS 8
#define CONTROLLERIMAGE_MAX_MOUSE_VARIANTS 8

extern DECLSPEC const SDL_Version * SDLCALL ControllerImage_LinkedVersion(void);

/* as the datafile format changes, this number bumps. This is the latest
   version the library understands. */
#define CONTROLLERIMAGE_CURRENT_DATAVER 3

/* Previous data versions:
    1: first public version
    2: Added GUIDs lists to devices
    3: Added variant IDs to .svg filenames
*/

// SDL only has a few mouse-relevant constants we can use.
// We want to be able to display a tad more than just buttons here.
enum CONTROLLERIMAGE_MOUSE_ICON_IDS {
    CONTROLLERIMAGE_MOUSE_NONE = 0,
    CONTROLLERIMAGE_MOUSE_LEFT = SDL_BUTTON_LEFT, // 1
    CONTROLLERIMAGE_MOUSE_MIDDLE = SDL_BUTTON_MIDDLE, // 2
    CONTROLLERIMAGE_MOUSE_RIGHT = SDL_BUTTON_RIGHT, // 3
    CONTROLLERIMAGE_MOUSE_X1 = SDL_BUTTON_X1, // 4
    CONTROLLERIMAGE_MOUSE_X2 = SDL_BUTTON_X2, // 5
    CONTROLLERIMAGE_MOUSE_MOVE,
    CONTROLLERIMAGE_MOUSE_SCROLL,
// Enumerated count
    CONTROLLERIMAGE_MOUSE_MAX,
};

typedef struct ControllerImage_MouseDevice ControllerImage_MouseDevice;
typedef struct ControllerImage_KeyboardDevice ControllerImage_KeyboardDevice;
typedef struct ControllerImage_GamepadDevice ControllerImage_GamepadDevice;

extern DECLSPEC int SDLCALL ControllerImage_Init(void);

extern DECLSPEC int SDLCALL ControllerImage_AddData(const void *buf, size_t buflen);
extern DECLSPEC int SDLCALL ControllerImage_AddDataFromIOStream(SDL_IOStream *iostrm, SDL_bool freeio);
extern DECLSPEC int SDLCALL ControllerImage_AddDataFromFile(const char *fname);

extern DECLSPEC ControllerImage_MouseDevice * SDLCALL ControllerImage_CreateMouseDevice();
extern DECLSPEC ControllerImage_KeyboardDevice * SDLCALL ControllerImage_CreateKeyboardDevice();
extern DECLSPEC ControllerImage_GamepadDevice * SDLCALL ControllerImage_CreateGamepadDevice(SDL_Gamepad *gamepad);
extern DECLSPEC ControllerImage_GamepadDevice * SDLCALL ControllerImage_CreateGamepadDeviceByInstance(SDL_JoystickID jsid);
extern DECLSPEC ControllerImage_GamepadDevice * SDLCALL ControllerImage_CreateGamepadDeviceByIdString(const char *str);

extern DECLSPEC void SDLCALL ControllerImage_DestroyMouseDevice(ControllerImage_MouseDevice *device);
extern DECLSPEC void SDLCALL ControllerImage_DestroyKeyboardDevice(ControllerImage_KeyboardDevice *device);
extern DECLSPEC void SDLCALL ControllerImage_DestroyGamepadDevice(ControllerImage_GamepadDevice *device);

extern DECLSPEC const char *ControllerImage_GetDeviceType(ControllerImage_GamepadDevice *device);
extern DECLSPEC SDL_Surface *ControllerImage_CreateSurfaceForAxis(ControllerImage_GamepadDevice *device, SDL_GamepadAxis axis, int size, int variantID);
extern DECLSPEC SDL_Surface *ControllerImage_CreateSurfaceForButton(ControllerImage_GamepadDevice *device, SDL_GamepadButton button, int size, int variantID);
extern DECLSPEC SDL_Surface *ControllerImage_CreateSurfaceForScancode(ControllerImage_KeyboardDevice *device, SDL_Scancode scancode, int size, int variantID);
extern DECLSPEC SDL_Surface *ControllerImage_CreateSurfaceForMouseIcon(ControllerImage_MouseDevice *device, int iconID, int size, int variantID);

extern DECLSPEC const char *ControllerImage_GetSVGForAxis(ControllerImage_GamepadDevice *device, SDL_GamepadAxis axis, int variantID);
extern DECLSPEC const char *ControllerImage_GetSVGForButton(ControllerImage_GamepadDevice *device, SDL_GamepadButton button, int variantID);
extern DECLSPEC const char *ControllerImage_GetSVGForScancode(ControllerImage_KeyboardDevice *device, SDL_Scancode scancode, int variantID);
extern DECLSPEC const char *ControllerImage_GetSVGForMouseIcon(ControllerImage_MouseDevice *device, int iconID, int variantID);

extern DECLSPEC SDL_bool ControllerImage_HasScancodeSVG(ControllerImage_KeyboardDevice *device, SDL_Scancode scancode, int variantID);
extern DECLSPEC SDL_bool ControllerImage_HasMouseIconSVG(ControllerImage_MouseDevice *device, int iconID, int variantID);

extern DECLSPEC void SDLCALL ControllerImage_Quit(void);

/* Ends C function definitions when using C++ */
#ifdef __cplusplus
}
#endif
#include <SDL3/SDL_close_code.h>

#endif /* INCL_CONTROLLERIMAGE_H_ */

