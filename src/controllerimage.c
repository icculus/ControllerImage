/*
 * ControllerImage; A simple way to obtain game controller images.
 *
 * Please see the file LICENSE.txt in the source's root directory.
 *
 *  This file written by Ryan C. Gordon.
 */

#include "controllerimage.h"

// nanosvg uses a bunch of C runtime stuff we can push through SDL to
// avoid the C runtime dependency...

#define NANOSVG_SKIP_STDC_HEADERS 1
#define NANOSVG_SKIP_STDIO 1
#define acosf SDL_acosf
#define atan2f SDL_atan2f
#define ceilf SDL_ceilf
#define cosf SDL_cosf
#define floorf SDL_floorf
#define fmodf SDL_fmodf
#define free SDL_free
#define sscanf SDL_sscanf
#define malloc SDL_malloc
#define memcpy SDL_memcpy
#define memset SDL_memset
#define pow SDL_pow
#define qsort SDL_qsort
#define realloc SDL_realloc
#define roundf SDL_roundf
#define sinf SDL_sinf
#define sqrt SDL_sqrt
#define sqrtf SDL_sqrtf
#define strchr SDL_strchr
#define strcmp SDL_strcmp
#define strlen SDL_strlen
#define strncmp SDL_strncmp
#define strncpy SDL_strlcpy
#define strstr SDL_strstr
#define strtol SDL_strtol
#define strtoll SDL_strtoll
#define tanf SDL_tanf
#define fabs SDL_fabs
#define fabsf SDL_fabsf
#define NANOSVG_IMPLEMENTATION
#include "nanosvg.h"
#define NANOSVGRAST_IMPLEMENTATION
#include "nanosvgrast.h"

static const char magic[8] = { 'C', 'T', 'I', 'M', 'G', '\r', '\n', '\0' };
static const SDL_GUID zeroguid;

typedef struct ControllerImage_Device
{
    // any of these might be NULL!
    NSVGimage *axes[SDL_GAMEPAD_AXIS_MAX];
    NSVGimage *buttons[SDL_GAMEPAD_BUTTON_MAX];
    const char *device_type;
    char *axes_svg[SDL_GAMEPAD_AXIS_MAX];
    char *buttons_svg[SDL_GAMEPAD_BUTTON_MAX];
    NSVGrasterizer *rasterizer;
} ControllerImage_Device;

typedef struct ControllerImage_Item
{
    const char *type;
    const char *svg;
} ControllerImage_Item;

typedef struct ControllerImage_DeviceInfo
{
    const char *type;
    const char *inherits;
    int num_items;
    ControllerImage_Item *items;
    int refcount;
} ControllerImage_DeviceInfo;


static SDL_PropertiesID DeviceGuidMap = 0;
static char **StringCache = NULL;
static int NumCachedStrings = 0;

const SDL_version *ControllerImage_LinkedVersion(void)
{
    static const SDL_version linked_version = {
        CONTROLLERIMAGE_MAJOR_VERSION,
        CONTROLLERIMAGE_MINOR_VERSION,
        CONTROLLERIMAGE_PATCHLEVEL
    };
    return &linked_version;
}

int ControllerImage_Init(void)
{
    DeviceGuidMap = SDL_CreateProperties();
    if (!DeviceGuidMap) {
        return -1;
    }
    return 0;
}

void ControllerImage_Quit(void)
{
    SDL_DestroyProperties(DeviceGuidMap);
    DeviceGuidMap = 0;
    for (int i = 0; i < NumCachedStrings; i++) {
        SDL_free(StringCache[i]);
    }
    SDL_free(StringCache);
    StringCache = NULL;
    NumCachedStrings = 0;
}

static SDL_bool readstr(const Uint8 **_ptr, size_t *_buflen, char **_str)
{
    const Uint8 *ptr = *_ptr;
    const size_t total = *_buflen;
    for (size_t i = 0; i < total; i++) {
        if (ptr[i] == '\0') {   // found end of string?
            i++;

            char *finalstr = NULL;
            for (int j = 0; j < NumCachedStrings; j++) {
                if (SDL_strcmp(StringCache[j], (const char *) ptr) == 0) {
                    finalstr = StringCache[j];
                    break;
                }
            }

            if (!finalstr) {
                void *expanded = SDL_realloc(StringCache, (NumCachedStrings + 1) * sizeof (char *));
                if (!expanded) {
                    return SDL_FALSE;
                }
                StringCache = (char **) expanded;
                finalstr = SDL_strdup((const char *) ptr);
                if (!finalstr) {
                    return SDL_FALSE;
                }

                StringCache[NumCachedStrings++] = finalstr;
            }

            *_str = finalstr;
            *_buflen -= i;
            *_ptr += i;
            return SDL_TRUE;
        }
    }

    SDL_SetError("Unexpected end of data");
    return SDL_FALSE;
}

static SDL_bool readui16(const Uint8 **_ptr, size_t *_buflen, Uint16 *_ui16)
{
    if (*_buflen < 2) {
        SDL_SetError("Unexpected end of data");
        return SDL_FALSE;
    }

    const Uint8 *ptr = *_ptr;
    *_ui16 = (((Uint16) ptr[0]) << 8) | ((Uint16) ptr[1]);
    *_ptr += 2;
    *_buflen -= 2;
    return SDL_TRUE;
}

static void SDLCALL CleanupDeviceInfo(void *userdata, void *value)
{
    // the same `ControllerImage_DeviceInfo*` is used for each matching
    // string/guid lookup in the hash table, so nuking it is actually a
    // refcount decrement until all references are gone.
    // the refcount isn't atomic, since it should only change on database
    // load and deinit...we just need to make sure we don't double-free.
    ControllerImage_DeviceInfo *info = (ControllerImage_DeviceInfo *) value;
    if (info->refcount == 1) {
        SDL_free(value);  // this is the ControllerImage_DeviceInfo and also its items in the same buffer. Strings live elsewhere and are reused.
    } else {
        info->refcount--;
    }
}

int ControllerImage_AddData(const void *buf, size_t buflen)
{
    const Uint8 *ptr = ((const Uint8 *) buf) + sizeof (magic);
    char **strings = NULL;
    Uint16 num_devices = 0;
    Uint16 num_strings = 0;
    Uint16 version = 0;

    if (!DeviceGuidMap) {
        return SDL_SetError("Not initialized");
    } else if (buflen < 20) {
        goto bogus_data;
    } else if (SDL_memcmp(magic, buf, sizeof (magic)) != 0) {
        goto bogus_data;
    } else if (!readui16(&ptr, &buflen, &version)) {
        return -1;
    } else if (version > CONTROLLERIMAGE_CURRENT_DATAVER) {
        return SDL_SetError("Unsupported data version; upgrade your copy of ControllerImage?");
    } else if (!readui16(&ptr, &buflen, &num_strings)) {
        return -1;
    } else if ((strings = (char **) SDL_calloc(num_strings, sizeof (char *))) == NULL) {
        return -1;
    }

    for (Uint16 i = 0; i < num_strings; i++) {
        if (!readstr(&ptr, &buflen, &strings[i])) {
            goto failed;
        }
    }

    if (!readui16(&ptr, &buflen, &num_devices)) {
        goto failed;
    }

    for (Uint16 i = 0; i < num_devices; i++) {
        Uint16 num_items = 0;
        Uint16 num_guids = 0;
        Uint16 devid = 0;
        Uint16 inherits = 0;
        if (!readui16(&ptr, &buflen, &devid)) {
            goto failed;
        } else if (devid >= num_strings) {
            goto bogus_data;
        } else if (!readui16(&ptr, &buflen, &inherits)) {
            goto failed;
        } else if (inherits && (inherits >= num_strings)) {
            goto bogus_data;
        } else if (!readui16(&ptr, &buflen, &num_items)) {
            goto failed;
        } else if ((version >= 2) && (!readui16(&ptr, &buflen, &num_guids))) {  // GUIDs didn't land until version 2 of the file format.
            goto failed;
        } else if (*(strings[devid]) == '\0') {
            goto bogus_data;  // can't have an empty string for the device ID.
        } else if (inherits && (*(strings[inherits]) == '\0')) {
            goto bogus_data;  // can't have an empty string for inherits.
        }

        ControllerImage_DeviceInfo *info = (ControllerImage_DeviceInfo *) SDL_calloc(1, sizeof (ControllerImage_DeviceInfo) + (sizeof (ControllerImage_Item) * num_items));
        if (!info) {
            goto failed;
        }

        info->type = strings[devid];
        info->inherits = inherits ? strings[inherits] : NULL;
        info->num_items = num_items;
        info->items = (ControllerImage_Item *) (info + 1);

        for (Uint16 j = 0; j < num_items; j++) {
            Uint16 itemtype = 0;
            Uint16 itemimage = 0;
            if (!readui16(&ptr, &buflen, &itemtype)) {
                SDL_free(info);
                goto failed;
            } else if (itemtype >= num_strings) {
                SDL_free(info);
                goto bogus_data;
            } else if (!readui16(&ptr, &buflen, &itemimage)) {
                SDL_free(info);
                goto failed;
            } else if (itemimage >= num_strings) {
                SDL_free(info);
                goto bogus_data;
            }

            info->items[j].type = strings[itemtype];
            info->items[j].svg = strings[itemimage];
        }

        if (SDL_SetPropertyWithCleanup(DeviceGuidMap, strings[devid], info, CleanupDeviceInfo, NULL) == -1) {
            SDL_free(info);
            goto failed;
        }

        info->refcount = 1;

        // now put the same `info` into the database for each associated GUID...
        for (Uint16 j = 0; j < num_guids; j++) {
            SDL_GUID guid;
            if (buflen < sizeof (guid.data)) {
                SDL_SetError("Unexpected end of data");
                goto failed;
            }

            SDL_memcpy(guid.data, ptr, sizeof (guid.data));
            ptr += sizeof (guid.data);
            buflen -= sizeof (guid.data);

            char guidstr[33];
            if (SDL_GUIDToString(guid, guidstr, sizeof (guidstr)) < 0) {
                SDL_assert(!"this probably shouldn't fail...");
                goto bogus_data;
            }

            // !!! FIXME: this does not work with overriding controllers with a second data set (so you load `standard` and then load `kenney` on top),
            // !!! FIXME:  so looking up a device by GUID will find the original images, not the overridden ones.
            // If this fails for some reason, go on without this guid. Do NOT SDL_free(info), it's already in the database, refcounted.
            if (SDL_SetPropertyWithCleanup(DeviceGuidMap, guidstr, info, CleanupDeviceInfo, NULL) == 0) {
                info->refcount++;
            }
        }
    }

    SDL_free(strings);  // the array! the actual strings are stored in StringCache!
    return 0;

bogus_data:
    SDL_SetError("Bogus data");

failed:
    SDL_free(strings);
    return -1;
}

int ControllerImage_AddDataFromRWops(SDL_RWops *rwops, SDL_bool freerw)
{
    Uint8 *buf = NULL;
    size_t buflen = 0;
    int retval = 0;

    if (!rwops) {
        return SDL_InvalidParamError("rwops");
    }

    buf = (Uint8 *) SDL_LoadFile_RW(rwops, &buflen, freerw);
    retval = buf ? ControllerImage_AddData(buf, buflen) : -1;
    SDL_free(buf);
    return retval;
}

int ControllerImage_AddDataFromFile(const char *fname)
{
    SDL_RWops *rwops = SDL_RWFromFile(fname, "rb");
    return rwops ? ControllerImage_AddDataFromRWops(rwops, SDL_TRUE) : -1;
}

static void CollectGamepadImages(ControllerImage_DeviceInfo *info, char **axes, char **buttons)
{
    if (!info) {
        return;
    } else if (info->inherits) {
        CollectGamepadImages((ControllerImage_DeviceInfo *) SDL_GetProperty(DeviceGuidMap, info->inherits, NULL), axes, buttons);
    }

    const ControllerImage_Item *leftxy = NULL;
    const ControllerImage_Item *rightxy = NULL;

    const int total = info->num_items;
    for (int i = 0; i < total; i++) {
        const ControllerImage_Item *item = &info->items[i];
        const char *typestr = item->type;

        // just save these for later as fallbacks...
        if (SDL_strcmp(typestr, "leftxy") == 0) {
            leftxy = item;
            continue;
        } else if (SDL_strcmp(typestr, "rightxy") == 0) {
            rightxy = item;
            continue;

        // SDL3 decided not to use n,s,e,w for controller config strings, so convert.
        } else if (SDL_strcmp(typestr, "n") == 0) {
            typestr = "y";
        } else if (SDL_strcmp(typestr, "s") == 0) {
            typestr = "a";
        } else if (SDL_strcmp(typestr, "w") == 0) {
            typestr = "x";
        } else if (SDL_strcmp(typestr, "e") == 0) {
            typestr = "b";
        }

        const int axis = (int) SDL_GetGamepadAxisFromString(typestr);
        if (axis != SDL_GAMEPAD_AXIS_INVALID) {
            SDL_assert(axis >= 0);
            if (axis < SDL_GAMEPAD_AXIS_MAX) {
                SDL_free(axes[axis]);  // in case we're overriding an earlier image.
                axes[axis] = SDL_strdup(item->svg);
            }
        } else {
            const int button = (int) SDL_GetGamepadButtonFromString(typestr);
            if (button != SDL_GAMEPAD_BUTTON_INVALID) {
                SDL_assert(button >= 0);
                if (button < SDL_GAMEPAD_BUTTON_MAX) {
                    SDL_free(buttons[button]);  // in case we're overriding an earlier image.
                    buttons[button] = SDL_strdup(item->svg);
                }
            }
        }

        // If there isn't a separate image for [left|right][x|y], see if there's a [left|right]xy fallback...
        if (leftxy) {
            if (!axes[SDL_GAMEPAD_AXIS_LEFTX]) {
                axes[SDL_GAMEPAD_AXIS_LEFTX] = SDL_strdup(leftxy->svg);
            }
            if (!axes[SDL_GAMEPAD_AXIS_LEFTY]) {
                axes[SDL_GAMEPAD_AXIS_LEFTY] = SDL_strdup(leftxy->svg);
            }
        }

        if (rightxy) {
            if (!axes[SDL_GAMEPAD_AXIS_RIGHTX]) {
                axes[SDL_GAMEPAD_AXIS_RIGHTX] = SDL_strdup(rightxy->svg);
            }
            if (!axes[SDL_GAMEPAD_AXIS_RIGHTY]) {
                axes[SDL_GAMEPAD_AXIS_RIGHTY] = SDL_strdup(rightxy->svg);
            }
        }
    }
}

static ControllerImage_Device *CreateGamepadDeviceFromInfo(ControllerImage_DeviceInfo *info)
{
    if (!info) {
        SDL_SetError("Couldn't find any usable images for this device! Maybe you didn't load anything?");
        return NULL;
    }

    ControllerImage_Device *device = SDL_calloc(1, sizeof (ControllerImage_Device));
    if (!device) {
        return NULL;
    }

    device->device_type = info->type;

    CollectGamepadImages(info, device->axes_svg, device->buttons_svg);

    device->rasterizer = nsvgCreateRasterizer();
    if (!device->rasterizer) {
        SDL_free(device);
        SDL_SetError("Failed to create SVG rasterizer");
        return NULL;
    }

    for (int i = 0; i < SDL_GAMEPAD_AXIS_MAX; i++) {
        if (device->axes_svg[i]) {
            char *cpy = SDL_strdup(device->axes_svg[i]);  // nsvgParse mangles the string!
            if (cpy) {
                device->axes[i] = nsvgParse(cpy, "px", 96.0f);
                SDL_free(cpy);
            }
        }
    }

    for (int i = 0; i < SDL_GAMEPAD_BUTTON_MAX; i++) {
        if (device->buttons_svg[i]) {
            char *cpy = SDL_strdup(device->buttons_svg[i]);  // nsvgParse mangles the string!
            if (cpy) {
                device->buttons[i] = nsvgParse(cpy, "px", 96.0f);
                SDL_free(cpy);
            }
        }
    }

    return device;
}

ControllerImage_Device *ControllerImage_CreateGamepadDeviceByIdString(const char *str)
{
    return CreateGamepadDeviceFromInfo((ControllerImage_DeviceInfo *) SDL_GetProperty(DeviceGuidMap, str, NULL));
}

ControllerImage_Device *ControllerImage_CreateGamepadDeviceByInstance(SDL_JoystickID jsid)
{
    const SDL_JoystickGUID guid = SDL_GetGamepadInstanceGUID(jsid);
    if (SDL_memcmp(&guid, &zeroguid, sizeof (SDL_GUID)) == 0) {
        return NULL;
    }

    char guidstr[33];
    SDL_GUIDToString(guid, guidstr, sizeof (guidstr));

    ControllerImage_DeviceInfo *info = (ControllerImage_DeviceInfo *) SDL_GetProperty(DeviceGuidMap, guidstr, NULL);

    if (!info) {
        guidstr[4] = guidstr[5] = guidstr[6] = guidstr[7] = '0';  // clear out the CRC, see if it matches...
        info = (ControllerImage_DeviceInfo *) SDL_GetProperty(DeviceGuidMap, guidstr, NULL);
    }

    if (!info) {
        // since these are the most common things, we might have a fallback specific to this device type...
        const char *typestr = SDL_GetGamepadStringForType(SDL_GetGamepadInstanceType(jsid));
        if (typestr) {
            info = (ControllerImage_DeviceInfo *) SDL_GetProperty(DeviceGuidMap, typestr, NULL);
        }
    }

    if (!info) {
        info = (ControllerImage_DeviceInfo *) SDL_GetProperty(DeviceGuidMap, "xbox360", NULL);  // if all else fails, this is probably most likely to match...
    }

    return CreateGamepadDeviceFromInfo(info);
}

const char *ControllerImage_GetDeviceType(ControllerImage_Device *device)
{
    if (!device) {
        SDL_InvalidParamError("device");
        return NULL;
    }
    return device->device_type;
}


ControllerImage_Device *ControllerImage_CreateGamepadDevice(SDL_Gamepad *gamepad)
{
    const SDL_JoystickID jsid = SDL_GetGamepadInstanceID(gamepad);
    return jsid ? ControllerImage_CreateGamepadDeviceByInstance(jsid) : NULL;
}

void ControllerImage_DestroyDevice(ControllerImage_Device *device)
{
    if (device) {
        nsvgDeleteRasterizer(device->rasterizer);
        for (int i = 0; i < SDL_GAMEPAD_AXIS_MAX; i++) {
	        nsvgDelete(device->axes[i]);
            SDL_free(device->axes_svg[i]);
        }
        for (int i = 0; i < SDL_GAMEPAD_BUTTON_MAX; i++) {
	        nsvgDelete(device->buttons[i]);
            SDL_free(device->buttons_svg[i]);
        }
    }
}

static SDL_Surface *RasterizeImage(NSVGrasterizer *rasterizer, NSVGimage *image, int size)
{
    SDL_assert(image != NULL);

    SDL_Surface *surface = SDL_CreateSurface(size, size, SDL_PIXELFORMAT_ABGR8888);
    if (!surface) {
        return NULL;
    }

    const float scale = (float)size / image->width;

    SDL_assert(rasterizer != NULL);
	nsvgRasterize(rasterizer, image, 0.0f, 0.0f, scale, (unsigned char *) surface->pixels, size, size, size * 4);
    return surface;
}

SDL_Surface *ControllerImage_CreateSurfaceForAxis(ControllerImage_Device *device, SDL_GamepadAxis axis, int size)
{
    const int iaxis = (int) axis;
    if ((iaxis < 0) || (iaxis >= SDL_GAMEPAD_AXIS_MAX)) {
        SDL_InvalidParamError("axis");
        return NULL;
    }
    NSVGimage *img = device->axes[iaxis];
    if (!img) {
        SDL_SetError("No image available");
        return NULL;
    }
    return RasterizeImage(device->rasterizer, img, size);
}

SDL_Surface *ControllerImage_CreateSurfaceForButton(ControllerImage_Device *device, SDL_GamepadButton button, int size)
{
    const int ibutton = (int) button;
    if ((ibutton < 0) || (ibutton >= SDL_GAMEPAD_BUTTON_MAX)) {
        SDL_InvalidParamError("button");
        return NULL;
    }
    NSVGimage *img = device->buttons[ibutton];
    if (!img) {
        SDL_SetError("No image available");
        return NULL;
    }
    return RasterizeImage(device->rasterizer, img, size);
}

const char *ControllerImage_GetSVGForAxis(ControllerImage_Device *device, SDL_GamepadAxis axis)
{
    const int iaxis = (int) axis;
    if ((iaxis < 0) || (iaxis >= SDL_GAMEPAD_AXIS_MAX)) {
        SDL_InvalidParamError("axis");
        return NULL;
    }
    const char *svg = device->axes_svg[iaxis];
    if (!svg) {
        SDL_SetError("No image available");  // !!! FIXME: default to some xbox thing?
    }
    return svg;
}

const char *ControllerImage_GetSVGForButton(ControllerImage_Device *device, SDL_GamepadButton button)
{
    const int ibutton = (int) button;
    if ((ibutton < 0) || (ibutton >= SDL_GAMEPAD_BUTTON_MAX)) {
        SDL_InvalidParamError("button");
        return NULL;
    }
    const char *svg = device->buttons_svg[ibutton];
    if (!svg) {
        SDL_SetError("No image available");  // !!! FIXME: default to some xbox thing?
    }
    return svg;
}

