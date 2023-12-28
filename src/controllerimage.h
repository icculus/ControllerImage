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

extern DECLSPEC const SDL_version * SDLCALL ControllerImage_LinkedVersion(void);

/* as the datafile format changes, this number bumps. This is the latest
   version the library understands. */
#define CONTROLLERIMAGE_CURRENT_DATAVER 2

/* Previous data versions:
    1: first public version
    2: Added GUIDs lists to devices
*/

typedef struct ControllerImage_Device ControllerImage_Device;

extern DECLSPEC int SDLCALL ControllerImage_Init(void);

extern DECLSPEC int SDLCALL ControllerImage_AddData(const void *buf, size_t buflen);
extern DECLSPEC int SDLCALL ControllerImage_AddDataFromRWops(SDL_RWops *rwops, SDL_bool freerw);
extern DECLSPEC int SDLCALL ControllerImage_AddDataFromFile(const char *fname);

extern DECLSPEC ControllerImage_Device * SDLCALL ControllerImage_CreateGamepadDevice(SDL_Gamepad *gamepad);
extern DECLSPEC ControllerImage_Device * SDLCALL ControllerImage_CreateGamepadDeviceByInstance(SDL_JoystickID jsid);
extern DECLSPEC ControllerImage_Device * SDLCALL ControllerImage_CreateGamepadDeviceByIdString(const char *str);

extern DECLSPEC void SDLCALL ControllerImage_DestroyDevice(ControllerImage_Device *device);

extern DECLSPEC const char *ControllerImage_GetDeviceType(ControllerImage_Device *device);
extern DECLSPEC SDL_Surface *ControllerImage_CreateSurfaceForAxis(ControllerImage_Device *device, SDL_GamepadAxis axis, int size);
extern DECLSPEC SDL_Surface *ControllerImage_CreateSurfaceForButton(ControllerImage_Device *device, SDL_GamepadButton button, int size);

extern DECLSPEC const char *ControllerImage_GetSVGForAxis(ControllerImage_Device *device, SDL_GamepadAxis axis);
extern DECLSPEC const char *ControllerImage_GetSVGForButton(ControllerImage_Device *device, SDL_GamepadButton button);

extern DECLSPEC void SDLCALL ControllerImage_Quit(void);

/* Ends C function definitions when using C++ */
#ifdef __cplusplus
}
#endif
#include <SDL3/SDL_close_code.h>

#endif /* INCL_CONTROLLERIMAGE_H_ */

