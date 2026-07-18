/*
 * ControllerImage; A simple way to obtain game controller images.
 *
 * Please see the file LICENSE.txt in the source's root directory.
 *
 *  This file written by Ryan C. Gordon.
 */

/* WIKI CATEGORY: ControllerImage */

/**
 * # CategoryControllerImage
 *
 * This is a library that provides resolution-independent images for game
 * controller inputs, so games can show the correct visual markers for the
 * gamepad in the user's hand.
 *
 * This is intended, for example, to let you show a "Press `image` to
 * continue" message, where `image` might be a yellow 'Y' if the user has an
 * Xbox controller and a green triangle if they have a PlayStation controller.
 * It is intended to work with whatever looks like a gamepad to SDL3, but it
 * requires artwork, in SVG format, for whatever controller the user happens
 * to be using. The standard ones--Xbox, PlayStation, Switch--are supplied,
 * and if we can't offer anything better, we'll aim for a generic Xbox 360
 * controller, under the assumption it _probably_ represents most generic
 * third-party devices.
 *
 * ControllerImage depends on SDL3.
 *
 * Using the library is simple:
 *
 * - In your app near startup (preferably after SDL_Init), call
 *   ControllerImage_Init().
 * - Load in the controller and image data with
 *   ControllerImage_AddDataFromFile().
 * - Get image information for an SDL_Gamepad, probably once when you open the
 *   gamepad, with ControllerImage_CreateGamepadDevice().
 * - Get image data for specific buttons or axes on the gamepad. They come in
 *   as SDL_Surfaces, so you can manipulate them, or upload them to
 *   SDL_Textures. You choose the size of the image; a game running at 4K
 *   might want a larger one than a game running at 720p, so it always looks
 *   crisp on the display without scaling. Use
 *   ControllerImage_CreateSurfaceForAxis() and
 *   ControllerImage_CreateSurfaceForButton().
 * - Done with a gamepad? Free resources with ControllerImage_DestroyDevice().
 * - Done with the library? Call ControllerImage_Quit() to clean up.
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

/**
 * The current major version of ControllerImage headers.
 *
 * If this were ControllerImage version 3.2.1, this value would be 3.
 *
 * \since This macro is available since ControllerImage 1.0.0.
 */
#define CONTROLLERIMAGE_MAJOR_VERSION   0

/**
 * The current minor version of ControllerImage headers.
 *
 * If this were ControllerImage version 3.2.1, this value would be 2.
 *
 * \since This macro is available since ControllerImage 1.0.0.
 */
#define CONTROLLERIMAGE_MINOR_VERSION   0

/**
 * The current micro (or patchlevel) version of ControllerImage headers.
 *
 * If this were ControllerImage version 3.2.1, this value would be 1.
 *
 * \since This macro is available since ControllerImage 1.0.0.
 */
#define CONTROLLERIMAGE_MICRO_VERSION   2

/**
 * The current version number macro of the ControllerImage headers.
 *
 * \since This macro is available since ControllerImage 1.0.0.
 *
 * \sa ControllerImage_Version
 */
#define CONTROLLERIMAGE_VERSION SDL_VERSIONNUM(CONTROLLERIMAGE_MAJOR_VERSION, CONTROLLERIMAGE_MINOR_VERSION, CONTROLLERIMAGE_MICRO_VERSION)

/**
 * An opaque datatype representing a game controller's set of images.
 *
 * This collects the images of a single device. An app creates one of these
 * objects for each gamepad it wants to show iconography for. Generally these
 * live as long as a gamepad is opened, so images at new resolutions can be
 * generated as needed.
 *
 * \since This datatype is available since ControllerImage 1.0.0.
 *
 * \sa ControllerImage_CreateGamepadDevice
 * \sa ControllerImage_CreateGamepadDeviceByInstance
 * \sa ControllerImage_CreateGamepadDeviceByIdString
 */
typedef struct ControllerImage_Device ControllerImage_Device;

/**
 * Get the version of ControllerImage that is linked against your program.
 *
 * If you are linking to ControllerImage dynamically, then it is possible that
 * the current version will be different than the version you compiled
 * against. This function returns the current version, while
 * CONTROLLERIMAGE_VERSION is the version you compiled with.
 *
 * This function may be called safely at any time, even before
 * ControllerImage_Init().
 *
 * \returns the version of the linked library.
 *
 * \since This function is available since ControllerImage 1.0.0.
 *
 * \sa CONTROLLERIMAGE_VERSION
 */
extern SDL_DECLSPEC int SDLCALL ControllerImage_Version(void);

/**
 * Returns the current datafile format version this library understands.
 *
 * As the datafile format changes, this number bumps. This is the latest
 * version the library understands. The library will continue to understand
 * prior versions, but can't understand versions newer than this.
 *
 * Previous data versions:
 *
 * - 1: first public version
 * - 2: Added GUIDs lists to devices
 *
 * \since This function is available since ControllerImage 1.0.0.
 */
extern SDL_DECLSPEC int ControllerImage_MaxDatafileVersion(void);

/**
 * Initialize the ControllerImage library.
 *
 * This must be successfully called once before (almost) any other
 * ControllerImage function can be used.
 *
 * It is safe to call this multiple times; the library will only initialize
 * once, and won't deinitialize until ControllerImage_Quit() has been called a
 * matching number of times. Extra attempts to init report success.
 *
 * \returns true on success, false on error; call SDL_GetError() for details.
 *
 * \threadsafety This function is not thread safe.
 *
 * \since This function is available since ControllerImage 1.0.0.
 *
 * \sa ControllerImage_Quit
 */
extern SDL_DECLSPEC bool SDLCALL ControllerImage_Init(void);

/**
 * Deinitialize the ControllerImage library.
 *
 * This must be called when done with the library, probably at the end of your
 * program.
 *
 * It is safe to call this multiple times; the library will only deinitialize
 * once, when this function is called the same number of times as
 * ControllerImage_Init() was successfully called.
 *
 * Once you have successfully deinitialized the library, it is safe to call
 * ControllerImage_Init() to reinitialize it for further use.
 *
 * Any data added to the library through ControllerImage_AddData() and related
 * functions will be deallocated.
 *
 * This function does not automatically destroy any created
 * ControllerImage_Device objects that have been created. Please destroy them
 * before deinitializing the library. SDL_Surface objects generated by the
 * library are _also_ not destroyed here.
 *
 * Once the library deinitializes, constant strings returned by various
 * functions, like ControllerImage_GetDeviceType(),
 * ControllerImage_GetSVGForButton(), and ControllerImage_GetSVGForAxis(),
 * will be deallocated, and their pointers should not be referenced again.
 *
 * \threadsafety This function is not thread safe.
 *
 * \since This function is available since ControllerImage 1.0.0.
 *
 * \sa ControllerImage_Init
 */
extern SDL_DECLSPEC void SDLCALL ControllerImage_Quit(void);

/**
 * Add data to the ControllerImage database.
 *
 * The library needs a database of controller information to be useful. This
 * data is external to the library and must be provided by the app. See the
 * library's documentation on how to build the needed data file from the
 * provided public domain assets.
 *
 * This should be called successfully at least once before attempting to
 * create a ControllerImage_Device, as doing so will fail without data.
 *
 * It is legal to call this function multiple times. If data for the same
 * gamepad is added twice, the newer call replaces a previous call's data.
 * This allows an app to add a "standard" database with ControllerImage's
 * dataset for wide converage, and override the most popular controllers with
 * a second, custom dataset to match a game's style more closely.
 *
 * This function takes the data from a memory buffer. It must be in the format
 * that the make-controllerimage-data.c program produces. There are also
 * equivalent functions to load from a filename or an SDL_IOStream.
 *
 * \param buf a pointer to a buffer that holds database data.
 * \param buflen the number of bytes to store in buffer.
 * \returns true on success, false on error; call SDL_GetError() for details.
 *
 * \threadsafety This function is not thread safe.
 *
 * \since This function is available since ControllerImage 1.0.0.
 *
 * \sa ControllerImage_AddDataFromFile
 * \sa ControllerImage_AddDataFromIOStream
 */
extern SDL_DECLSPEC bool SDLCALL ControllerImage_AddData(const void *buf, size_t buflen);

/**
 * Add data to the ControllerImage database from a filesystem path.
 *
 * The library needs a database of controller information to be useful. This
 * data is external to the library and must be provided by the app. See the
 * library's documentation on how to build the needed data file from the
 * provided public domain assets.
 *
 * This should be called successfully at least once before attempting to
 * create a ControllerImage_Device, as doing so will fail without data.
 *
 * It is legal to call this function multiple times. If data for the same
 * gamepad is added twice, the newer call replaces a previous call's data.
 * This allows an app to add a "standard" database with ControllerImage's
 * dataset for wide converage, and override the most popular controllers with
 * a second, custom dataset to match a game's style more closely.
 *
 * This function takes the data from a filesystem path. It must be in the
 * format that the make-controllerimage-data.c program produces. There are
 * also equivalent functions to load from a memory buffer or an SDL_IOStream.
 *
 * \param fname a filesystem path from which to load database data.
 * \returns true on success, false on error; call SDL_GetError() for details.
 *
 * \threadsafety This function is not thread safe.
 *
 * \since This function is available since ControllerImage 1.0.0.
 *
 * \sa ControllerImage_AddData
 * \sa ControllerImage_AddDataFromIOStream
 */
extern SDL_DECLSPEC bool SDLCALL ControllerImage_AddDataFromFile(const char *fname);

/**
 * Add data to the ControllerImage database from an SDL_IOStream.
 *
 * The library needs a database of controller information to be useful. This
 * data is external to the library and must be provided by the app. See the
 * library's documentation on how to build the needed data file from the
 * provided public domain assets.
 *
 * This should be called successfully at least once before attempting to
 * create a ControllerImage_Device, as doing so will fail without data.
 *
 * It is legal to call this function multiple times. If data for the same
 * gamepad is added twice, the newer call replaces a previous call's data.
 * This allows an app to add a "standard" database with ControllerImage's
 * dataset for wide converage, and override the most popular controllers with
 * a second, custom dataset to match a game's style more closely.
 *
 * This function takes the data from an SDL_IOStream. It must be in the format
 * that the make-controllerimage-data.c program produces. There are also
 * equivalent functions to load from a memory buffer or a filesystem path.
 *
 * If `closeio` is true, this function will call `SDL_CloseIO(io)` before
 * returning, whether the function succeeded or not.
 *
 * \param io a stream to provide database data.
 * \param closeio if true, automatically close the stream when done.
 * \returns true on success, false on error; call SDL_GetError() for details.
 *
 * \threadsafety This function is not thread safe.
 *
 * \since This function is available since ControllerImage 1.0.0.
 *
 * \sa ControllerImage_AddData
 * \sa ControllerImage_AddDataFromFile
 */
extern SDL_DECLSPEC bool SDLCALL ControllerImage_AddDataFromIOStream(SDL_IOStream *io, bool closeio);

/**
 * Create an device object to obtain image data for a specific gamepad.
 *
 * Once a device object is created, it can be used to obtain image data,
 * either as SDL_Surface objects, or raw SVG image format strings.
 *
 * This function uses an SDL_Gamepad to look up data, which is convenient for
 * preparing image data for a controller that was just opened. One can also
 * use ControllerImage_CreateGamepadDeviceByInstance() for gamepads that are
 * not yet opened (or joysticks instead of gamepads), and
 * ControllerImage_CreateGamepadDeviceByIdString() for looking up by GUID or a
 * standard name string.
 *
 * When done with the returned device object, dispose of it with
 * ControllerImage_DestroyDevice().
 *
 * \param gamepad an opened gamepad to look up.
 * \returns a new device object on success, false on error; call
 *          SDL_GetError() for details.
 *
 * \threadsafety This function is not thread safe.
 *
 * \since This function is available since ControllerImage 1.0.0.
 *
 * \sa ControllerImage_CreateGamepadDeviceByIdString
 * \sa ControllerImage_CreateGamepadDeviceByInstance
 * \sa ControllerImage_DestroyDevice
 */
extern SDL_DECLSPEC ControllerImage_Device * SDLCALL ControllerImage_CreateGamepadDevice(SDL_Gamepad *gamepad);

/**
 * Create an device object to obtain image data for a specific SDL_JoystickID.
 *
 * Once a device object is created, it can be used to obtain image data,
 * either as SDL_Surface objects, or raw SVG image format strings.
 *
 * This function uses an SDL_JoystickID to look up data, which is convenient
 * for preparing image data for a controller that hasn't yet been opened, or
 * perhaps an SDL joystick that doesn't have a real gamepad mapping. One can
 * also use ControllerImage_CreateGamepadDevice() for gamepads that are
 * opened, and ControllerImage_CreateGamepadDeviceByIdString() for looking up
 * by GUID or a standard name string.
 *
 * When done with the returned device object, dispose of it with
 * ControllerImage_DestroyDevice().
 *
 * \param jsid an SDL joystick instance to look up.
 * \returns a new device object on success, false on error; call
 *          SDL_GetError() for details.
 *
 * \threadsafety This function is not thread safe.
 *
 * \since This function is available since ControllerImage 1.0.0.
 *
 * \sa ControllerImage_CreateGamepadDevice
 * \sa ControllerImage_CreateGamepadDeviceByIdString
 * \sa ControllerImage_DestroyDevice
 */
extern SDL_DECLSPEC ControllerImage_Device * SDLCALL ControllerImage_CreateGamepadDeviceByInstance(SDL_JoystickID jsid);

/**
 * Create an device object to obtain image data from an ID string.
 *
 * Once a device object is created, it can be used to obtain image data,
 * either as SDL_Surface objects, or raw SVG image format strings.
 *
 * This function uses a string to look up data. It can be a specific
 * joystick's GUID or an artset name (see the directory names in
 * art/standard/gamepad for a list). The standard strings can be useful if you
 * always want, generically, the "xbox360" image set or whatnot. One can also
 * use ControllerImage_CreateGamepadDevice() for gamepads that are opened, and
 * ControllerImage_CreateGamepadDeviceByInstance() for gamepads that are not
 * yet opened (or joysticks instead of gamepads).
 *
 * When done with the returned device object, dispose of it with
 * ControllerImage_DestroyDevice().
 *
 * \param str an SDL joystick GUID or a artset name to look up.
 * \returns a new device object on success, false on error; call
 *          SDL_GetError() for details.
 *
 * \threadsafety This function is not thread safe.
 *
 * \since This function is available since ControllerImage 1.0.0.
 *
 * \sa ControllerImage_CreateGamepadDevice
 * \sa ControllerImage_CreateGamepadDeviceByInstance
 * \sa ControllerImage_DestroyDevice
 */
extern SDL_DECLSPEC ControllerImage_Device * SDLCALL ControllerImage_CreateGamepadDeviceByIdString(const char *str);

/**
 * Dispose of a previously-created ControllerImage_Device object.
 *
 * Call this once done with a device. Resources are freed and the pointer
 * passed in here becomes invalid immediately.
 *
 * \param device the object to dispose of.
 *
 * \threadsafety This function is not thread safe.
 *
 * \since This function is available since ControllerImage 1.0.0.
 *
 * \sa ControllerImage_CreateGamepadDevice
 */
extern SDL_DECLSPEC void SDLCALL ControllerImage_DestroyDevice(ControllerImage_Device *device);


/**
 * Get the device type for a ControllerImage_Device object.
 *
 * Device types are short, ASCII strings that describe the controller. The
 * strings are derived from the artset name, not anything that SDL produces.
 *
 * Some examples strings this function might return are "xbox360", "ps5",
 * "joyconpair", "ouya".
 *
 * Generally speaking, this is _not_ intended to be used to identify
 * controllers; SDL3 has more robust facilities for this task, and this might
 * be giving a best guess to controller type anyhow. All this tells you is
 * what artset was chosen.
 *
 * \param device the device object to query.
 * \returns a NULL-terminated ASCII string, or NULL on error; call
 *          SDL_GetError() for details.
 *
 * \threadsafety This function is not thread safe.
 *
 * \since This function is available since ControllerImage 1.0.0.
 */
extern SDL_DECLSPEC const char * SDLCALL ControllerImage_GetDeviceType(ControllerImage_Device *device);

/**
 * Check if artwork is available for a given axis on a specific device.
 *
 * Not all devices have all axes, or perhaps an artset is incomplete. This
 * function reports if artwork for a specific axis is available.
 *
 * A NULL device or a bogus axis value will return false; make sure your
 * parameters are good to get useful information!
 *
 * \param device the device object to query.
 * \param axis the axis on the device to check for available artwork.
 * \returns true if artwork is available, false otherwise.
 *
 * \threadsafety This function is not thread safe.
 *
 * \since This function is available since ControllerImage 1.0.0.
 *
 * \sa ControllerImage_DeviceHasArtworkForButton
 */
extern SDL_DECLSPEC bool SDLCALL ControllerImage_DeviceHasArtworkForAxis(ControllerImage_Device *device, SDL_GamepadAxis axis);

/**
 * Check if artwork is available for a given button on a specific device.
 *
 * Not all devices have all buttons, or perhaps an artset is incomplete. This
 * function reports if artwork for a specific button is available.
 *
 * A NULL device or a bogus button value will return false; make sure your
 * parameters are good to get useful information!
 *
 * \param device the device object to query.
 * \param button the button on the device to check for available artwork.
 * \returns true if artwork is available, false otherwise.
 *
 * \threadsafety This function is not thread safe.
 *
 * \since This function is available since ControllerImage 1.0.0.
 *
 * \sa ControllerImage_DeviceHasArtworkForAxis
 */
extern SDL_DECLSPEC bool SDLCALL ControllerImage_DeviceHasArtworkForButton(ControllerImage_Device *device, SDL_GamepadButton button);

/**
 * Render one of a controller's axis images to an SDL_Surface.
 *
 * This creates a new surface with the art for a single axis. The artwork is
 * stored as scalable vector graphics, so it can be generated at any desired
 * size and look sharp.
 *
 * All artwork is generated as a square, so the requested size represents both
 * the width and height in pixels.
 *
 * Since this has to allocate and rasterize an image, it's not a fast call,
 * and should probably be done once, not every frame.
 *
 * This returns NULL on error, but also if there is no artwork available. For
 * a controller missing an axis, this is not necessarily an error. If the
 * distinction is important, consider calling
 * ControllerImage_DeviceHasArtworkForAxis().
 *
 * The returned SDL_Surface is owned by the caller, who should call
 * SDL_DestroySurface() to dispose of it when done with it.
 *
 * \param device the device object for which to generate an image.
 * \param axis the axis on the device for which to generate an image.
 * \param size the size, in pixels, that the generated SDL_Surface should be,
 *             This size is used for both the width and height.
 * \returns a new surface on success, or NULL on error; call SDL_GetError()
 *          for details.
 *
 * \threadsafety This function is not thread safe.
 *
 * \since This function is available since ControllerImage 1.0.0.
 *
 * \sa ControllerImage_GetSVGForAxis
 */
extern SDL_DECLSPEC SDL_Surface * SDLCALL ControllerImage_CreateSurfaceForAxis(ControllerImage_Device *device, SDL_GamepadAxis axis, int size);

/**
 * Render one of a controller's button images to an SDL_Surface.
 *
 * This creates a new surface with the art for a single button. The artwork is
 * stored as scalable vector graphics, so it can be generated at any desired
 * size and look sharp.
 *
 * All artwork is generated as a square, so the requested size represents both
 * the width and height in pixels.
 *
 * Since this has to allocate and rasterize an image, it's not a fast call,
 * and should probably be done once, not every frame.
 *
 * This returns NULL on error, but also if there is no artwork available. For
 * a controller missing a button, this is not necessarily an error. If the
 * distinction is important, consider calling
 * ControllerImage_DeviceHasArtworkForButton().
 *
 * The returned SDL_Surface is owned by the caller, who should call
 * SDL_DestroySurface() to dispose of it when done with it.
 *
 * \param device the device object for which to generate an image.
 * \param button the button on the device for which to generate an image.
 * \param size the size, in pixels, that the generated SDL_Surface should be,
 *             This size is used for both the width and height.
 * \returns a new surface on success, or NULL on error; call SDL_GetError()
 *          for details.
 *
 * \threadsafety This function is not thread safe.
 *
 * \since This function is available since ControllerImage 1.0.0.
 *
 * \sa ControllerImage_GetSVGForButton
 */
extern SDL_DECLSPEC SDL_Surface * SDLCALL ControllerImage_CreateSurfaceForButton(ControllerImage_Device *device, SDL_GamepadButton button, int size);

/**
 * Get the raw SVG data for one axis on a controller.
 *
 * This can be used if the caller intends to render its own images from
 * SVG-format data. Most apps will use ControllerImage_CreateSurfaceForAxis(),
 * instead, which will handle generating the pixels internally.
 *
 * This returns NULL on error, but also if there is no artwork available. For
 * a controller missing a button, this is not necessarily an error. If the
 * distinction is important, consider calling
 * ControllerImage_DeviceHasArtworkForButton().
 *
 * The returned string (SVG files are text-based XML files) is owned by
 * ControllerImage, not the caller, and should not be free'd. The pointer
 * remains valid until `device` is destroyed.
 *
 * \param device the device object for which to obtain SVG data.
 * \param axis the axis on the device for which to obtain SVG data.
 * \returns the raw SVG data for the image on success, or NULL on error; call
 *          SDL_GetError() for details.
 *
 * \threadsafety This function is not thread safe.
 *
 * \since This function is available since ControllerImage 1.0.0.
 *
 * \sa ControllerImage_CreateSurfaceForAxis
 */
extern SDL_DECLSPEC const char * SDLCALL ControllerImage_GetSVGForAxis(ControllerImage_Device *device, SDL_GamepadAxis axis);

/**
 * Get the raw SVG data for one button on a controller.
 *
 * This can be used if the caller intends to render its own images from
 * SVG-format data. Most apps will use
 * ControllerImage_CreateSurfaceForButton(), instead, which will handle
 * generating the pixels internally.
 *
 * This returns NULL on error, but also if there is no artwork available. For
 * a controller missing a button, this is not necessarily an error. If the
 * distinction is important, consider calling
 * ControllerImage_DeviceHasArtworkForButton().
 *
 * The returned string (SVG files are text-based XML files) is owned by
 * ControllerImage, not the caller, and should not be free'd. The pointer
 * remains valid until `device` is destroyed.
 *
 * \param device the device object for which to obtain SVG data.
 * \param button the button on the device for which to obtain SVG data.
 * \returns the raw SVG data for the image on success, or NULL on error; call
 *          SDL_GetError() for details.
 *
 * \threadsafety This function is not thread safe.
 *
 * \since This function is available since ControllerImage 1.0.0.
 *
 * \sa ControllerImage_CreateSurfaceForButton
 */
extern SDL_DECLSPEC const char * SDLCALL ControllerImage_GetSVGForButton(ControllerImage_Device *device, SDL_GamepadButton button);

/* Ends C function definitions when using C++ */
#ifdef __cplusplus
}
#endif
#include <SDL3/SDL_close_code.h>

#endif /* INCL_CONTROLLERIMAGE_H_ */

