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

typedef struct ControllerImage_SpecialScancodeName
{
    const char* name;
    const SDL_Scancode scancode;
} ControllerImage_SpecialScancodeName;

static const ControllerImage_SpecialScancodeName special_scancode_names[] = {
    { "apostrophe", SDL_SCANCODE_APOSTROPHE },
    { "backslash", SDL_SCANCODE_BACKSLASH },
    { "comma", SDL_SCANCODE_COMMA },
    { "equals", SDL_SCANCODE_EQUALS },
    { "keypad asterisk", SDL_SCANCODE_KP_MULTIPLY },
    { "keypad enter", SDL_SCANCODE_KP_ENTER },
    { "keypad minus", SDL_SCANCODE_KP_MINUS },
    { "keypad plus", SDL_SCANCODE_KP_PLUS },
    { "leftbracket", SDL_SCANCODE_LEFTBRACKET },
    { "rightbracket", SDL_SCANCODE_RIGHTBRACKET },
    { "period", SDL_SCANCODE_PERIOD },
    { "semicolon", SDL_SCANCODE_SEMICOLON },
    { "slash", SDL_SCANCODE_SLASH },
    { "tilde", SDL_SCANCODE_GRAVE },
    { "unknown", SDL_SCANCODE_UNKNOWN }, // Unknown is used for scancodes with no matching image
};

typedef struct ControllerImage_MouseIconName
{
    const char* name;
    const int iconID;
} ControllerImage_MouseIconName;

static const ControllerImage_MouseIconName mouse_icon_names[] = {
    { "none", CONTROLLERIMAGE_MOUSE_NONE },
    { "left", CONTROLLERIMAGE_MOUSE_LEFT },
    { "middle", CONTROLLERIMAGE_MOUSE_MIDDLE },
    { "right", CONTROLLERIMAGE_MOUSE_RIGHT },
    { "x1", CONTROLLERIMAGE_MOUSE_X1 },
    { "x2", CONTROLLERIMAGE_MOUSE_X2 },
    { "move", CONTROLLERIMAGE_MOUSE_MOVE },
    { "scroll", CONTROLLERIMAGE_MOUSE_SCROLL },
};

typedef struct ControllerImage_MouseDevice
{
    // any of these might be NULL!
    NSVGimage *icons[CONTROLLERIMAGE_MOUSE_MAX][CONTROLLERIMAGE_MAX_MOUSE_VARIANTS];
    char *icons_svg[CONTROLLERIMAGE_MOUSE_MAX][CONTROLLERIMAGE_MAX_MOUSE_VARIANTS];
    NSVGrasterizer *rasterizer;
} ControllerImage_MouseDevice;

typedef struct ControllerImage_KeyboardDevice
{
    // any of these might be NULL!
    NSVGimage *keys[SDL_NUM_SCANCODES][CONTROLLERIMAGE_MAX_KEYBOARD_VARIANTS];
    char *keys_svg[SDL_NUM_SCANCODES][CONTROLLERIMAGE_MAX_KEYBOARD_VARIANTS];
    NSVGrasterizer *rasterizer;
} ControllerImage_KeyboardDevice;

typedef struct ControllerImage_GamepadDevice
{
    // any of these might be NULL!
    NSVGimage *axes[SDL_GAMEPAD_AXIS_MAX][CONTROLLERIMAGE_MAX_GAMEPAD_VARIANTS];
    NSVGimage *buttons[SDL_GAMEPAD_BUTTON_MAX][CONTROLLERIMAGE_MAX_GAMEPAD_VARIANTS];
    const char *device_type;
    char *axes_svg[SDL_GAMEPAD_AXIS_MAX][CONTROLLERIMAGE_MAX_GAMEPAD_VARIANTS];
    char *buttons_svg[SDL_GAMEPAD_BUTTON_MAX][CONTROLLERIMAGE_MAX_GAMEPAD_VARIANTS];
    NSVGrasterizer *rasterizer;
} ControllerImage_GamepadDevice;

typedef struct ControllerImage_Item
{
    const char *type;
    const char *svg;
} ControllerImage_Item;

typedef struct ControllerImage_GamepadDeviceInfo
{
    const char *type;
    const char *inherits;
    int num_items;
    ControllerImage_Item *items;
    int refcount;
} ControllerImage_GamepadDeviceInfo;

static SDL_PropertiesID DeviceGuidMap = 0;
static char **StringCache = NULL;
static int NumCachedStrings = 0;

const SDL_Version *ControllerImage_LinkedVersion(void)
{
    static const SDL_Version linked_version = {
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
    // the same `ControllerImage_GamepadDeviceInfo*` is used for each matching
    // string/guid lookup in the hash table, so nuking it is actually a
    // refcount decrement until all references are gone.
    // the refcount isn't atomic, since it should only change on database
    // load and deinit...we just need to make sure we don't double-free.
    ControllerImage_GamepadDeviceInfo *info = (ControllerImage_GamepadDeviceInfo *) value;
    if (info->refcount == 1) {
        SDL_free(value);  // this is the ControllerImage_GamepadDeviceInfo and also its items in the same buffer. Strings live elsewhere and are reused.
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

        ControllerImage_GamepadDeviceInfo *info = (ControllerImage_GamepadDeviceInfo *) SDL_calloc(1, sizeof (ControllerImage_GamepadDeviceInfo) + (sizeof (ControllerImage_Item) * num_items));
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

int ControllerImage_AddDataFromIOStream(SDL_IOStream *iostrm, SDL_bool freeio)
{
    Uint8 *buf = NULL;
    size_t buflen = 0;
    int retval = 0;

    if (!iostrm) {
        return SDL_InvalidParamError("iostrm");
    }

    buf = (Uint8 *) SDL_LoadFile_IO(iostrm, &buflen, freeio);
    retval = buf ? ControllerImage_AddData(buf, buflen) : -1;
    SDL_free(buf);
    return retval;
}

int ControllerImage_AddDataFromFile(const char *fname)
{
    SDL_IOStream *iostrm = SDL_IOFromFile(fname, "rb");
    return iostrm ? ControllerImage_AddDataFromIOStream(iostrm, SDL_TRUE) : -1;
}

static void CollectGamepadImages(ControllerImage_GamepadDeviceInfo *info, ControllerImage_GamepadDevice *device)
{
    if (!info) {
        return;
    } else if (info->inherits) {
        CollectGamepadImages((ControllerImage_GamepadDeviceInfo *) SDL_GetProperty(DeviceGuidMap, info->inherits, NULL), device);
    }

    const ControllerImage_Item *leftxy = NULL;
    const ControllerImage_Item *rightxy = NULL;

    char typestrbuffer[CONTROLLERIMAGE_MAX_FILENAME_LENGTH];

    const int total = info->num_items;
    for (int i = 0; i < total; i++) {
        const ControllerImage_Item *item = &info->items[i];
        const char *typestr = item->type;

        // Split the root name and variantID
        const char *split = SDL_strrchr(typestr, '_');
        if (!split) {
            // bad data
            SDL_SetError("svg file name missing variant underscore");
            continue;
        }

        if (SDL_strlen(split) <= 1) {
            // bad data
            SDL_SetError("svg file name has no variant ID");
            continue;
        }
        
        // This could probably be better.
        int typelen = (int)(split - typestr);
        
        if (typelen >= CONTROLLERIMAGE_MAX_FILENAME_LENGTH) {
            // bad data
            SDL_SetError("svg file name is too long");
            continue;
        }
        
        for (int j = 0; j < typelen; j++) {
            typestrbuffer[j] = typestr[j];
        }
        typestrbuffer[typelen] = '\0';
        
        split += 1;

        int variantID = SDL_atoi(split);
        
        if ((variantID < 0) || (variantID >= CONTROLLERIMAGE_MAX_GAMEPAD_VARIANTS))  {
            // bad data
            SDL_SetError("svg file name invalid variant ID");
            continue;
        }

        // Our variantID is valid.

        // just save these for later as fallbacks...
        if (SDL_strcmp(typestrbuffer, "leftxy") == 0) {
            leftxy = item;
            continue;
        } else if (SDL_strcmp(typestrbuffer, "rightxy") == 0) {
            rightxy = item;
            continue;

        // SDL3 decided not to use n,s,e,w for controller config strings, so convert.
        } else if (SDL_strcmp(typestrbuffer, "n") == 0) {
            *typestrbuffer = 'y';
        } else if (SDL_strcmp(typestrbuffer, "s") == 0) {
            *typestrbuffer = 'a';
        } else if (SDL_strcmp(typestrbuffer, "w") == 0) {
            *typestrbuffer = 'x';
        } else if (SDL_strcmp(typestrbuffer, "e") == 0) {
            *typestrbuffer = 'b';
        }

        const int axis = (int) SDL_GetGamepadAxisFromString(typestrbuffer);
        if (axis != SDL_GAMEPAD_AXIS_INVALID) {
            SDL_assert(axis >= 0);
            if (axis < SDL_GAMEPAD_AXIS_MAX) {
                SDL_free(device->axes_svg[axis][variantID]);  // in case we're overriding an earlier image.
                device->axes_svg[axis][variantID] = SDL_strdup(item->svg);
            }
        } else {
            const int button = (int) SDL_GetGamepadButtonFromString(typestrbuffer);
            if (button != SDL_GAMEPAD_BUTTON_INVALID) {
                SDL_assert(button >= 0);
                if (button < SDL_GAMEPAD_BUTTON_MAX) {
                    SDL_free(device->buttons_svg[button][variantID]);  // in case we're overriding an earlier image.
                    device->buttons_svg[button][variantID] = SDL_strdup(item->svg);
                }
            }
        }

        // If there isn't a separate image for [left|right][x|y], see if there's a [left|right]xy fallback...
        if (leftxy) {
            if (!device->axes_svg[SDL_GAMEPAD_AXIS_LEFTX][0]) {
                device->axes_svg[SDL_GAMEPAD_AXIS_LEFTX][0] = SDL_strdup(leftxy->svg);
            }
            if (!device->axes_svg[SDL_GAMEPAD_AXIS_LEFTY][0]) {
                device->axes_svg[SDL_GAMEPAD_AXIS_LEFTY][0] = SDL_strdup(leftxy->svg);
            }
        }

        if (rightxy) {
            if (!device->axes_svg[SDL_GAMEPAD_AXIS_RIGHTX][0]) {
                device->axes_svg[SDL_GAMEPAD_AXIS_RIGHTX][0] = SDL_strdup(rightxy->svg);
            }
            if (!device->axes_svg[SDL_GAMEPAD_AXIS_RIGHTY][0]) {
                device->axes_svg[SDL_GAMEPAD_AXIS_RIGHTY][0] = SDL_strdup(rightxy->svg);
            }
        }
    }
}

static int GetScancodeFromSpecialName(const char *name)
{
    // SDL_SCANCODE_UNKNOWN is actually a valid return value here, since we want to use that "slot" for a default image.
    // As a result, we return -1 for a failure to find a suitable value.
    int i;

    if (!name || !*name) {
        SDL_InvalidParamError("special name");
        return -1;
    }

    for (i = 0; i < SDL_arraysize(special_scancode_names); ++i) {
        if (SDL_strcasecmp(name, special_scancode_names[i].name) == 0) {
            return (int)special_scancode_names[i].scancode;
        }
    }

    SDL_InvalidParamError("special name");
    return -1;
}

static void CollectKeyboardImages(ControllerImage_GamepadDeviceInfo *info, ControllerImage_KeyboardDevice *device)
{
    if (!info) {
        return;
    } else if (info->inherits) {
        // TODO inheritance requires more-specific keyboard handling
        CollectKeyboardImages((ControllerImage_GamepadDeviceInfo *) SDL_GetProperty(DeviceGuidMap, info->inherits, NULL), device);
    }

    char typestrbuffer[CONTROLLERIMAGE_MAX_FILENAME_LENGTH];

    const int total = info->num_items;
    for (int i = 0; i < total; i++) {
        const ControllerImage_Item *item = &info->items[i];
        const char *typestr = item->type;

        // Split the root name and variantID
        const char *split = SDL_strrchr(typestr, '_');
        if (!split) {
            // bad data
            SDL_SetError("svg file name missing variant underscore");
            continue;
        }

        if (SDL_strlen(split) <= 1) {
            // bad data
            SDL_SetError("svg file name has no variant ID");
            continue;
        }
        
        // This could probably be better.
        int typelen = (int)(split - typestr);
        
        if (typelen >= CONTROLLERIMAGE_MAX_FILENAME_LENGTH) {
            // bad data
            SDL_SetError("svg file name is too long");
            continue;
        }
        
        // As we copy the string, underscores become spaces
        // This lets us properly compare when using SDL_GetScancodeFromName
        for (int j = 0; j < typelen; j++) {
            if(typestr[j] == '_') {
                typestrbuffer[j] = ' ';
            } else {
                typestrbuffer[j] = typestr[j];
            }
        }
        typestrbuffer[typelen] = '\0';
        
        split += 1;

        int variantID = SDL_atoi(split);
        
        if ((variantID < 0) || (variantID >= CONTROLLERIMAGE_MAX_KEYBOARD_VARIANTS))  {
            // bad data
            SDL_SetError("svg file name invalid keyboard variant ID");
            continue;
        }

        // Our variantID is valid.
        // Try to get the scancode from the default SDL scancode names
        int scancode = (int) SDL_GetScancodeFromName(typestrbuffer);

        if (scancode == SDL_SCANCODE_UNKNOWN) {
            // We couldn't match a scancode using SDL_GetScancodeFromName.
            // In all likelihood, this is because this file name is once of the special ones,
            // which have different names to avoid filesystem shenanigans.
            scancode = GetScancodeFromSpecialName(typestrbuffer);
            if(scancode == -1) continue;
        }

        SDL_assert(scancode >= 0);
        if (scancode < SDL_NUM_SCANCODES) {
            SDL_free(device->keys_svg[scancode][variantID]);  // in case we're overriding an earlier image.
            device->keys_svg[scancode][variantID] = SDL_strdup(item->svg);
            
            // When loading certain scancodes, we'll also load them to "duplicate" keys
            int altscancode = SDL_SCANCODE_UNKNOWN;
            
            switch(scancode) {
                case SDL_SCANCODE_LCTRL:
                    altscancode = SDL_SCANCODE_RCTRL;
                    break;
                case SDL_SCANCODE_LALT:
                    altscancode = SDL_SCANCODE_RALT;
                    break;
                case SDL_SCANCODE_LSHIFT:
                    altscancode = SDL_SCANCODE_RSHIFT;
                    break;
                case SDL_SCANCODE_LGUI:
                    altscancode = SDL_SCANCODE_RGUI;
                    break;
                default:
                    break;
            }
            
            // If we do have a matching duplicate key, we won't overwrite any existing art.
            if(altscancode != SDL_SCANCODE_UNKNOWN) {
                if( !device->keys_svg[altscancode][variantID] ) {
                    SDL_free(device->keys_svg[altscancode][variantID]);  // in case we're overriding an earlier image.
                    device->keys_svg[altscancode][variantID] = SDL_strdup(item->svg);
                }
            }
        }
    }
}

static int GetMouseIDFromName(const char *name)
{
    // We return -1 if we fail to find a suitable value.
    int i;

    if (!name || !*name) {
        SDL_InvalidParamError("mouse id name");
        return -1;
    }

    for (i = 0; i < SDL_arraysize(mouse_icon_names); ++i) {
        if (SDL_strcasecmp(name, mouse_icon_names[i].name) == 0) {
            return mouse_icon_names[i].iconID;
        }
    }

    SDL_InvalidParamError("mouse id name");
    return -1;
}

static void CollectMouseImages(ControllerImage_GamepadDeviceInfo *info, ControllerImage_MouseDevice *device)
{
    if (!info) {
        return;
    } else if (info->inherits) {
        // TODO inheritance requires more-specific mouse handling
        CollectMouseImages((ControllerImage_GamepadDeviceInfo *) SDL_GetProperty(DeviceGuidMap, info->inherits, NULL), device);
    }

    char typestrbuffer[CONTROLLERIMAGE_MAX_FILENAME_LENGTH];

    const int total = info->num_items;
    for (int i = 0; i < total; i++) {
        const ControllerImage_Item *item = &info->items[i];
        const char *typestr = item->type;

        // Split the root name and variantID
        const char *split = SDL_strrchr(typestr, '_');
        if (!split) {
            // bad data
            SDL_SetError("svg file name missing variant underscore");
            continue;
        }

        if (SDL_strlen(split) <= 1) {
            // bad data
            SDL_SetError("svg file name has no variant ID");
            continue;
        }
        
        // This could probably be better.
        int typelen = (int)(split - typestr);
        
        if (typelen >= CONTROLLERIMAGE_MAX_FILENAME_LENGTH) {
            // bad data
            SDL_SetError("svg file name is too long");
            continue;
        }

        for (int j = 0; j < typelen; j++) {
            typestrbuffer[j] = typestr[j];
        }
        typestrbuffer[typelen] = '\0';
        
        split += 1;

        int variantID = SDL_atoi(split);
        
        if ((variantID < 0) || (variantID >= CONTROLLERIMAGE_MAX_KEYBOARD_VARIANTS))  {
            // bad data
            SDL_SetError("svg file name invalid keyboard variant ID");
            continue;
        }

        // Our variantID is valid.
        // Try to get the mouse icon from the table
        int iconID = GetMouseIDFromName(typestrbuffer);
        if(iconID == -1) continue;

        SDL_assert(iconID >= 0);
        if (iconID < CONTROLLERIMAGE_MOUSE_MAX) {
            SDL_free(device->icons_svg[iconID][variantID]);  // in case we're overriding an earlier image.
            device->icons_svg[iconID][variantID] = SDL_strdup(item->svg);
        }
    }
}

static ControllerImage_GamepadDevice *CreateGamepadDeviceFromInfo(ControllerImage_GamepadDeviceInfo *info)
{
    if (!info) {
        SDL_SetError("Couldn't find any usable images for this device! Maybe you didn't load anything?");
        return NULL;
    }

    ControllerImage_GamepadDevice *device = SDL_calloc(1, sizeof (ControllerImage_GamepadDevice));
    if (!device) {
        return NULL;
    }

    device->device_type = info->type;

    CollectGamepadImages(info, device);

    device->rasterizer = nsvgCreateRasterizer();
    if (!device->rasterizer) {
        SDL_free(device);
        SDL_SetError("Failed to create SVG rasterizer");
        return NULL;
    }

    for (int i = 0; i < SDL_GAMEPAD_AXIS_MAX; i++) {
        for (int j = 0; j < CONTROLLERIMAGE_MAX_GAMEPAD_VARIANTS; j++) {
            if (device->axes_svg[i][j]) {
                char *cpy = SDL_strdup(device->axes_svg[i][j]);  // nsvgParse mangles the string!
                if (cpy) {
                    device->axes[i][j] = nsvgParse(cpy, "px", 96.0f);
                    SDL_free(cpy);
                }
            }
        }
    }

    for (int i = 0; i < SDL_GAMEPAD_BUTTON_MAX; i++) {
        for (int j = 0; j < CONTROLLERIMAGE_MAX_GAMEPAD_VARIANTS; j++) {
            if (device->buttons_svg[i][j]) {
                char *cpy = SDL_strdup(device->buttons_svg[i][j]);  // nsvgParse mangles the string!
                if (cpy) {
                    device->buttons[i][j] = nsvgParse(cpy, "px", 96.0f);
                    SDL_free(cpy);
                }
            }
        }
    }

    return device;
}

ControllerImage_GamepadDevice *ControllerImage_CreateGamepadDeviceByIdString(const char *str)
{
    return CreateGamepadDeviceFromInfo((ControllerImage_GamepadDeviceInfo *) SDL_GetProperty(DeviceGuidMap, str, NULL));
}

ControllerImage_GamepadDevice *ControllerImage_CreateGamepadDeviceByInstance(SDL_JoystickID jsid)
{
    const SDL_JoystickGUID guid = SDL_GetGamepadInstanceGUID(jsid);
    if (SDL_memcmp(&guid, &zeroguid, sizeof (SDL_GUID)) == 0) {
        return NULL;
    }

    char guidstr[33];
    SDL_GUIDToString(guid, guidstr, sizeof (guidstr));

    ControllerImage_GamepadDeviceInfo *info = (ControllerImage_GamepadDeviceInfo *) SDL_GetProperty(DeviceGuidMap, guidstr, NULL);

    if (!info) {
        guidstr[4] = guidstr[5] = guidstr[6] = guidstr[7] = '0';  // clear out the CRC, see if it matches...
        info = (ControllerImage_GamepadDeviceInfo *) SDL_GetProperty(DeviceGuidMap, guidstr, NULL);
    }

    if (!info) {
        // since these are the most common things, we might have a fallback specific to this device type...
        const char *typestr = SDL_GetGamepadStringForType(SDL_GetGamepadInstanceType(jsid));
        if (typestr) {
            info = (ControllerImage_GamepadDeviceInfo *) SDL_GetProperty(DeviceGuidMap, typestr, NULL);
        }
    }

    if (!info) {
        info = (ControllerImage_GamepadDeviceInfo *) SDL_GetProperty(DeviceGuidMap, "xbox360", NULL);  // if all else fails, this is probably most likely to match...
    }

    return CreateGamepadDeviceFromInfo(info);
}

const char *ControllerImage_GetDeviceType(ControllerImage_GamepadDevice *device)
{
    if (!device) {
        SDL_InvalidParamError("device");
        return NULL;
    }
    return device->device_type;
}

ControllerImage_GamepadDevice *ControllerImage_CreateGamepadDevice(SDL_Gamepad *gamepad)
{
    const SDL_JoystickID jsid = SDL_GetGamepadInstanceID(gamepad);
    return jsid ? ControllerImage_CreateGamepadDeviceByInstance(jsid) : NULL;
}

ControllerImage_MouseDevice *ControllerImage_CreateMouseDevice()
{
    ControllerImage_MouseDevice *device = SDL_calloc(1, sizeof (ControllerImage_MouseDevice));
    if (!device) {
        return NULL;
    }

    ControllerImage_GamepadDeviceInfo *info = (ControllerImage_GamepadDeviceInfo *) SDL_GetProperty(DeviceGuidMap, "mouse", NULL);

    CollectMouseImages(info, device);

    device->rasterizer = nsvgCreateRasterizer();
    if (!device->rasterizer) {
        SDL_free(device);
        SDL_SetError("Failed to create SVG rasterizer");
        return NULL;
    }

    for (int i = 0; i < CONTROLLERIMAGE_MOUSE_MAX; i++) {
        for (int j = 0; j < CONTROLLERIMAGE_MAX_MOUSE_VARIANTS; j++) {
            if (device->icons_svg[i][j]) {
                char *cpy = SDL_strdup(device->icons_svg[i][j]);  // nsvgParse mangles the string!
                if (cpy) {
                    device->icons[i][j] = nsvgParse(cpy, "px", 96.0f);
                    SDL_free(cpy);
                }
            }
        }
    }

    return device;
}

ControllerImage_KeyboardDevice *ControllerImage_CreateKeyboardDevice()
{
    ControllerImage_KeyboardDevice *device = SDL_calloc(1, sizeof (ControllerImage_KeyboardDevice));
    if (!device) {
        return NULL;
    }

    ControllerImage_GamepadDeviceInfo *info = (ControllerImage_GamepadDeviceInfo *) SDL_GetProperty(DeviceGuidMap, "keyboard", NULL);

    CollectKeyboardImages(info, device);

    device->rasterizer = nsvgCreateRasterizer();
    if (!device->rasterizer) {
        SDL_free(device);
        SDL_SetError("Failed to create SVG rasterizer");
        return NULL;
    }

    for (int i = 0; i < SDL_NUM_SCANCODES; i++) {
        for (int j = 0; j < CONTROLLERIMAGE_MAX_KEYBOARD_VARIANTS; j++) {
            if (device->keys_svg[i][j]) {
                char *cpy = SDL_strdup(device->keys_svg[i][j]);  // nsvgParse mangles the string!
                if (cpy) {
                    device->keys[i][j] = nsvgParse(cpy, "px", 96.0f);
                    SDL_free(cpy);
                }
            }
        }
    }

    return device;
}

void ControllerImage_DestroyMouseDevice(ControllerImage_MouseDevice *device)
{
    if (device) {
        nsvgDeleteRasterizer(device->rasterizer);
        for (int i = 0; i < CONTROLLERIMAGE_MOUSE_MAX; i++) {
            for (int j = 0; j < CONTROLLERIMAGE_MAX_MOUSE_VARIANTS; j++) {
                nsvgDelete(device->icons[i][j]);
                SDL_free(device->icons_svg[i][j]);
            }
        }
    }
}

void ControllerImage_DestroyKeyboardDevice(ControllerImage_KeyboardDevice *device)
{
    if (device) {
        nsvgDeleteRasterizer(device->rasterizer);
        for (int i = 0; i < SDL_NUM_SCANCODES; i++) {
            for (int j = 0; j < CONTROLLERIMAGE_MAX_KEYBOARD_VARIANTS; j++) {
                nsvgDelete(device->keys[i][j]);
                SDL_free(device->keys_svg[i][j]);
            }
        }
    }
}

void ControllerImage_DestroyGamepadDevice(ControllerImage_GamepadDevice *device)
{
    if (device) {
        nsvgDeleteRasterizer(device->rasterizer);
        for (int i = 0; i < SDL_GAMEPAD_AXIS_MAX; i++) {
            for (int j = 0; j < CONTROLLERIMAGE_MAX_GAMEPAD_VARIANTS; j++) {
                nsvgDelete(device->axes[i][j]);
                SDL_free(device->axes_svg[i][j]);
            }
        }
        for (int i = 0; i < SDL_GAMEPAD_BUTTON_MAX; i++) {
            for (int j = 0; j < CONTROLLERIMAGE_MAX_GAMEPAD_VARIANTS; j++) {
                nsvgDelete(device->buttons[i][j]);
                SDL_free(device->buttons_svg[i][j]);
            }
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

SDL_Surface *ControllerImage_CreateSurfaceForAxis(ControllerImage_GamepadDevice *device, SDL_GamepadAxis axis, int size, int variantID)
{
    const int iaxis = (int) axis;
    if ((iaxis < 0) || (iaxis >= SDL_GAMEPAD_AXIS_MAX)) {
        SDL_InvalidParamError("axis");
        return NULL;
    }
    if ((variantID < 0) || (variantID >= CONTROLLERIMAGE_MAX_GAMEPAD_VARIANTS)) {
        SDL_InvalidParamError("axis variantID");
        return NULL;
    }
    
    NSVGimage *img = device->axes[iaxis][variantID];
    NSVGimage *defaultimg = device->axes[iaxis][0];
    if (!img) {
        if (!defaultimg) {
            SDL_SetError("No image available");
            return NULL;
        } else {
            // We don't have a variant but we do have a default image (variant 0)
            img = defaultimg;
        }
    }
    return RasterizeImage(device->rasterizer, img, size);
}

SDL_Surface *ControllerImage_CreateSurfaceForButton(ControllerImage_GamepadDevice *device, SDL_GamepadButton button, int size, int variantID)
{
    const int ibutton = (int) button;
    if ((ibutton < 0) || (ibutton >= SDL_GAMEPAD_BUTTON_MAX)) {
        SDL_InvalidParamError("button");
        return NULL;
    }
    if ((variantID < 0) || (variantID >= CONTROLLERIMAGE_MAX_GAMEPAD_VARIANTS)) {
        SDL_InvalidParamError("button variantID");
        return NULL;
    }

    NSVGimage *img = device->buttons[ibutton][variantID];
    NSVGimage *defaultimg = device->buttons[ibutton][0];
    if (!img) {
        if (!defaultimg) {
            SDL_SetError("No image available");
            return NULL;
        } else {
            // We don't have a variant but we do have a default image (variant 0)
            img = defaultimg;
        }
    }
    return RasterizeImage(device->rasterizer, img, size);
}

SDL_Surface *ControllerImage_CreateSurfaceForScancode(ControllerImage_KeyboardDevice *device, SDL_Scancode scancode, int size, int variantID)
{
    const int iscancode = (int) scancode;
    if ((iscancode < SDL_SCANCODE_UNKNOWN) || (iscancode >= SDL_NUM_SCANCODES)) {
        SDL_InvalidParamError("scancode");
        return NULL;
    }
    if ((variantID < 0) || (variantID >= CONTROLLERIMAGE_MAX_KEYBOARD_VARIANTS)) {
        SDL_InvalidParamError("scancode variantID");
        return NULL;
    }

    NSVGimage *img = device->keys[iscancode][variantID];
    NSVGimage *defaultimg = device->keys[iscancode][0];

    // If we don't have a matching variant...
    if (!img) {
        // And we don't have a matching standard image for this scancode...
        if (!defaultimg) {
            // We'll try to load the matching unknown variant (so we can at least show a key being pressed)
            NSVGimage *unknownimg = device->keys[SDL_SCANCODE_UNKNOWN][variantID];
            NSVGimage *unknowndefaultimg = device->keys[SDL_SCANCODE_UNKNOWN][0];
            
            // If we have no matching unknown variant...
            if(!unknownimg) {
                // Then we'll use the standard unknown image. (which we should always have, but...)
                if(!unknowndefaultimg) {
                    SDL_SetError("No image available");
                    return NULL;
                } else {
                    img = unknowndefaultimg;
                }
            } else {
                img = unknownimg;
            }
        } else {
            // We don't have a variant but we do have a default image (variant 0)
            img = defaultimg;
        }
    }
    return RasterizeImage(device->rasterizer, img, size);
}

SDL_Surface *ControllerImage_CreateSurfaceForMouseIcon(ControllerImage_MouseDevice *device, int iconID, int size, int variantID)
{
    if ((iconID < CONTROLLERIMAGE_MOUSE_NONE) || (iconID >= CONTROLLERIMAGE_MOUSE_MAX)) {
        SDL_InvalidParamError("mouse icon");
        return NULL;
    }
    if ((variantID < 0) || (variantID >= CONTROLLERIMAGE_MAX_MOUSE_VARIANTS)) {
        SDL_InvalidParamError("mouse icon variantID");
        return NULL;
    }

    NSVGimage *img = device->icons[iconID][variantID];
    NSVGimage *defaultimg = device->icons[iconID][0];

    // If we don't have a matching variant...
    if (!img) {
        // And we don't have a matching standard image for this mouse icon...
        if (!defaultimg) {
            // We'll try to load the matching none variant (so we can at least show a mouse icon)
            NSVGimage *noneimg = device->icons[CONTROLLERIMAGE_MOUSE_NONE][variantID];
            NSVGimage *nonedefaultimg = device->icons[CONTROLLERIMAGE_MOUSE_NONE][0];
            
            // If we have no matching none variant...
            if(!noneimg) {
                // Then we'll use the standard none image. (which we should always have, but...)
                if(!nonedefaultimg) {
                    SDL_SetError("No image available");
                    return NULL;
                } else {
                    img = nonedefaultimg;
                }
            } else {
                img = noneimg;
            }
        } else {
            // We don't have a variant but we do have a default image (variant 0)
            img = defaultimg;
        }
    }
    return RasterizeImage(device->rasterizer, img, size);
}

const char *ControllerImage_GetSVGForAxis(ControllerImage_GamepadDevice *device, SDL_GamepadAxis axis, int variantID)
{
    const int iaxis = (int) axis;
    if ((iaxis < 0) || (iaxis >= SDL_GAMEPAD_AXIS_MAX)) {
        SDL_InvalidParamError("axis");
        return NULL;
    }
    if ((variantID < 0) || (variantID >= CONTROLLERIMAGE_MAX_GAMEPAD_VARIANTS)) {
        SDL_InvalidParamError("axis variantID");
        return NULL;
    }
    const char *svg = device->axes_svg[iaxis][variantID];
    const char *defaultsvg = device->axes_svg[iaxis][0];
    if (!svg) {
        if (!defaultsvg) {
            SDL_SetError("No image available");  // !!! FIXME: default to some xbox thing?
            return NULL;
        } else {
            // We don't have a variant but we do have a default svg (variant 0)
            svg = defaultsvg;
        }
    }
    return svg;
}

const char *ControllerImage_GetSVGForButton(ControllerImage_GamepadDevice *device, SDL_GamepadButton button, int variantID)
{
    const int ibutton = (int) button;
    if ((ibutton < 0) || (ibutton >= SDL_GAMEPAD_BUTTON_MAX)) {
        SDL_InvalidParamError("button");
        return NULL;
    }
    if ((variantID < 0) || (variantID >= CONTROLLERIMAGE_MAX_GAMEPAD_VARIANTS)) {
        SDL_InvalidParamError("button variantID");
        return NULL;
    }
    const char *svg = device->buttons_svg[ibutton][variantID];
    const char *defaultsvg = device->buttons_svg[ibutton][0];
    if (!svg) {
        if (!defaultsvg) {
            SDL_SetError("No image available");  // !!! FIXME: default to some xbox thing?
            return NULL;
        } else {
            // We don't have a variant but we do have a default svg (variant 0)
            svg = defaultsvg;
        }
    }
    return svg;
}

const char *ControllerImage_GetSVGForScancode(ControllerImage_KeyboardDevice *device, SDL_Scancode scancode, int variantID)
{
    const int iscancode = (int) scancode;
    if ((iscancode < SDL_SCANCODE_UNKNOWN) || (iscancode >= SDL_NUM_SCANCODES)) {
        SDL_InvalidParamError("scancode");
        return NULL;
    }
    if ((variantID < 0) || (variantID >= CONTROLLERIMAGE_MAX_KEYBOARD_VARIANTS)) {
        SDL_InvalidParamError("scancode variantID");
        return NULL;
    }

    const char *svg = device->keys_svg[iscancode][variantID];
    const char *defaultsvg = device->keys_svg[iscancode][0];

    // If we don't have a matching variant...
    if (!svg) {
        // And we don't have a matching standard image for this scancode...
        if (!defaultsvg) {
            // We'll try to load the matching unknown variant (so we can at least show a key being pressed)
            const char *unknownsvg = device->keys_svg[SDL_SCANCODE_UNKNOWN][variantID];
            const char *unknowndefaultsvg = device->keys_svg[SDL_SCANCODE_UNKNOWN][0];
            
            // If we have no matching unknown variant...
            if(!unknownsvg) {
                // Then we'll use the standard unknown image. (which we should always have, but...)
                if(!unknowndefaultsvg) {
                    SDL_SetError("No image available");
                    return NULL;
                } else {
                    svg = unknowndefaultsvg;
                }
            } else {
                svg = unknownsvg;
            }
        } else {
            // We don't have a variant but we do have a default image (variant 0)
            svg = defaultsvg;
        }
    }
    return svg;
}

const char *ControllerImage_GetSVGForMouseIcon(ControllerImage_MouseDevice *device, int iconID, int variantID)
{
    if ((iconID < CONTROLLERIMAGE_MOUSE_NONE) || (iconID >= CONTROLLERIMAGE_MOUSE_MAX)) {
        SDL_InvalidParamError("mouse icon");
        return NULL;
    }
    if ((variantID < 0) || (variantID >= CONTROLLERIMAGE_MAX_MOUSE_VARIANTS)) {
        SDL_InvalidParamError("mouse icon variantID");
        return NULL;
    }

    const char *svg = device->icons_svg[iconID][variantID];
    const char *defaultsvg = device->icons_svg[iconID][0];

    // If we don't have a matching variant...
    if (!svg) {
        // And we don't have a matching standard image for this scancode...
        if (!defaultsvg) {
            // We'll try to load the matching none variant (so we can at least show a key being pressed)
            const char *nonesvg = device->icons_svg[CONTROLLERIMAGE_MOUSE_NONE][variantID];
            const char *nonedefaultsvg = device->icons_svg[CONTROLLERIMAGE_MOUSE_NONE][0];
            
            // If we have no matching none variant...
            if(!nonesvg) {
                // Then we'll use the standard none image. (which we should always have, but...)
                if(!nonedefaultsvg) {
                    SDL_SetError("No image available");
                    return NULL;
                } else {
                    svg = nonedefaultsvg;
                }
            } else {
                svg = nonesvg;
            }
        } else {
            // We don't have a variant but we do have a default image (variant 0)
            svg = defaultsvg;
        }
    }
    return svg;
}

SDL_bool ControllerImage_HasScancodeSVG(ControllerImage_KeyboardDevice *device, SDL_Scancode scancode, int variantID)
{
    const int iscancode = (int) scancode;
    if ((iscancode < SDL_SCANCODE_UNKNOWN) || (iscancode >= SDL_NUM_SCANCODES)) {
        SDL_InvalidParamError("scancode");
        return SDL_FALSE;
    }
    if ((variantID < 0) || (variantID >= CONTROLLERIMAGE_MAX_KEYBOARD_VARIANTS)) {
        SDL_InvalidParamError("scancode variantID");
        return SDL_FALSE;
    }

    if (!device->keys_svg[iscancode][variantID]) return SDL_FALSE;
    return SDL_TRUE;
}

SDL_bool ControllerImage_HasMouseIconSVG(ControllerImage_MouseDevice *device, int iconID, int variantID)
{
    if ((iconID < CONTROLLERIMAGE_MOUSE_NONE) || (iconID >= CONTROLLERIMAGE_MOUSE_MAX)) {
        SDL_InvalidParamError("mouse icon");
        return SDL_FALSE;
    }
    if ((variantID < 0) || (variantID >= CONTROLLERIMAGE_MAX_MOUSE_VARIANTS)) {
        SDL_InvalidParamError("mouse icon variantID");
        return SDL_FALSE;
    }

    if (!device->icons_svg[iconID][variantID]) return SDL_FALSE;
    return SDL_TRUE;
}