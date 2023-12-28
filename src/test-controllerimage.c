#define SDL_MAIN_USE_CALLBACKS 1
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include "controllerimage.h"

#define PROP_IMGDEV "test-controllerimage.imgdev"
#define PROP_RENDER_SIZE "test-controllerimage.render_size"
#define PROP_TEXBUTTON_FMT "test-controllerimage.texure_button_%d"
#define PROP_TEXAXIS_FMT "test-controllerimage.texure_axis_%d"

static SDL_Window *window = NULL;
static SDL_Renderer *renderer = NULL;
static SDL_Texture *gamepad_front_texture = NULL;
static SDL_Gamepad *current_gamepad = NULL;
static SDL_PropertiesID artset_properties = 0;
static int winw = 1280;
static int winh = 720;

static int panic(const char *title, const char *msg)
{
    char *cpy = SDL_strdup(msg);  // in case this is from SDL_GetError() and something changes it.
    if (cpy) {
        msg = cpy;
    }
    SDL_Log("%s", title);
    SDL_Log("%s", msg);
    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, title, msg, window);
    SDL_free(cpy);
    return -1;
}

static void SDLCALL cleanup_imgdev(void *userdata, void *value)
{
    //SDL_Log("Destroying ControllerImage_Device %p", value);
    ControllerImage_DestroyDevice((ControllerImage_Device *) value);
}

static void SDLCALL cleanup_texture(void *userdata, void *value)
{
    //SDL_Log("Destroying Texture %p", value);
    SDL_DestroyTexture((SDL_Texture *) value);
}


int SDL_AppInit(int argc, char *argv[])
{
    const char *title = argv[0] ? argv[0] : "test-controllerimage";
    SDL_Surface *surf = NULL;

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD) < 0) {
        return panic("SDL_Init failed!", SDL_GetError());
    } else if (ControllerImage_Init() < 0) {
        return panic("ControllerImage_Init failed!", SDL_GetError());
    } else if (ControllerImage_AddDataFromFile("controllerimage-standard.bin") < 0) {
        return panic("ControllerImage_AddDataFromFile failed!", SDL_GetError());
    } else if ((window = SDL_CreateWindow(title, winw, winh, SDL_WINDOW_RESIZABLE|SDL_WINDOW_HIDDEN)) == NULL) {
        return panic("SDL_CreateWindow failed!", SDL_GetError());
    } else if ((renderer = SDL_CreateRenderer(window, NULL, 0)) == NULL) {
        return panic("SDL_CreateRenderer failed!", SDL_GetError());
    } else if ((surf = SDL_LoadBMP("gamepad_front.bmp")) == NULL) {
        return panic("Failed to load gamepad_front.bmp!", SDL_GetError());
    } else if ((gamepad_front_texture = SDL_CreateTextureFromSurface(renderer, surf)) == NULL) {
        panic("Failed to create gamepad texture!", SDL_GetError());
        SDL_DestroySurface(surf);
        return -1;
    }

    SDL_DestroySurface(surf);

    SDL_SetTextureScaleMode(gamepad_front_texture, SDL_SCALEMODE_LINEAR);

    if (argc > 1) {
        artset_properties = SDL_CreateProperties();
        if (!artset_properties) {
            return panic("SDL_CreateProperties failed!", SDL_GetError());
        }
        ControllerImage_Device *imgdev = ControllerImage_CreateGamepadDeviceByIdString(argv[1]);
        if (!imgdev) {
            return panic("ControllerImage_CreateGamepadDeviceByIdString failed!", SDL_GetError());
        }
        SDL_SetPropertyWithCleanup(artset_properties, PROP_IMGDEV, imgdev, cleanup_imgdev, NULL);
    }

    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);
    SDL_RenderPresent(renderer);
    SDL_ShowWindow(window);
    SDL_RenderClear(renderer);
    SDL_RenderPresent(renderer);

    return 0;  // we're ready, keep going.
}

int SDL_AppEvent(const SDL_Event *event)
{
    char numstr[64];
    SDL_JoystickID which;
    SDL_Gamepad *gamepad;

    switch (event->type) {
        case SDL_EVENT_GAMEPAD_ADDED:
            if (artset_properties) {
                break;  // ignore hardware if we're looking at specific art.
            }
            which = event->gdevice.which;
            SDL_snprintf(numstr, sizeof (numstr), "%u", (unsigned int) which);
            gamepad = SDL_OpenGamepad(which);
            if (gamepad) {
                SDL_PropertiesID props = SDL_GetGamepadProperties(gamepad);
                if (!props) {
                    SDL_CloseGamepad(gamepad);
                } else {
                    ControllerImage_Device *imgdev = ControllerImage_CreateGamepadDevice(gamepad);
                    if (imgdev) {
                        const SDL_JoystickGUID guid = SDL_GetGamepadInstanceGUID(which);
                        char guidstr[64];
                        guidstr[0] = '\0';
                        SDL_GUIDToString((SDL_GUID) guid, guidstr, sizeof (guidstr));
                        SDL_Log("Adding gamepad %s ('%s', guid %s)", numstr, SDL_GetGamepadInstanceName(which), guidstr);
                        SDL_Log("ControllerImage device type: %s", ControllerImage_GetDeviceType(imgdev));
                        SDL_SetPropertyWithCleanup(props, PROP_IMGDEV, imgdev, cleanup_imgdev, NULL);
                        current_gamepad = gamepad;
                    }
                }
            } else {
                SDL_Log("ERROR: SDL_OpenGamepad failed: %s", SDL_GetError());
            }
            break;

        case SDL_EVENT_GAMEPAD_REMOVED:
            if (artset_properties) {
                break;  // ignore hardware if we're looking at specific art.
            }
            which = event->gdevice.which;
            SDL_Log("Removing gamepad %u", (unsigned int) which);
            gamepad = SDL_GetGamepadFromInstanceID(which);
            if (gamepad) {
                if (gamepad == current_gamepad) {
                    current_gamepad = NULL;
                }
                SDL_CloseGamepad(gamepad);
            }
            break;

        case SDL_EVENT_GAMEPAD_AXIS_MOTION:
        case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
        case SDL_EVENT_GAMEPAD_BUTTON_UP:
        case SDL_EVENT_GAMEPAD_TOUCHPAD_DOWN:
        case SDL_EVENT_GAMEPAD_TOUCHPAD_MOTION:
        case SDL_EVENT_GAMEPAD_TOUCHPAD_UP:
            if (artset_properties) {
                break;  // ignore hardware if we're looking at specific art.
            }
            // these actually use different parts of the event union, but the `which` field lines up in all of them.
            which = event->gdevice.which;
            current_gamepad = SDL_GetGamepadFromInstanceID(which);
            break;

        case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
            winw = event->window.data1;
            winh = event->window.data2;
            break;

        case SDL_EVENT_QUIT:
            return 1;  // stop app.

        case SDL_EVENT_KEY_DOWN:
            if (event->key.keysym.sym == SDLK_ESCAPE) {
                return 1;  // stop app.
            }
            break;
    }

    return 0;  // keep going.
}

static void update_gamepad_textures(SDL_PropertiesID gamepad_props, const int newsize)
{
    SDL_assert(gamepad_props != 0);
    const int render_size = (int) SDL_GetFloatProperty(gamepad_props, PROP_RENDER_SIZE, -1.0f);
    const float epsilon = 1e-6;

    if (SDL_fabs(newsize - render_size) < epsilon) {
        return;  // already good to go.
    }

    ControllerImage_Device *imgdev = (ControllerImage_Device *) SDL_GetProperty(gamepad_props, PROP_IMGDEV, NULL);
    SDL_assert(imgdev != NULL);

    char propname[64];

    for (SDL_GamepadButton i = SDL_GAMEPAD_BUTTON_SOUTH; i < SDL_GAMEPAD_BUTTON_MAX; i++) {
        int thissize = newsize;
        if ((i >= SDL_GAMEPAD_BUTTON_DPAD_UP) && (i <= SDL_GAMEPAD_BUTTON_DPAD_RIGHT)) {
            thissize *= 2;
        } else if ((i == SDL_GAMEPAD_BUTTON_LEFT_STICK) || (i == SDL_GAMEPAD_BUTTON_RIGHT_STICK)) {
            thissize *= 2;
        } else if (i == SDL_GAMEPAD_BUTTON_TOUCHPAD) {
            thissize *= 3;
        }
        SDL_Surface *surf = ControllerImage_CreateSurfaceForButton(imgdev, i, thissize);
        SDL_Texture *tex = surf ? SDL_CreateTextureFromSurface(renderer, surf) : NULL;
        SDL_DestroySurface(surf);
        SDL_snprintf(propname, sizeof (propname), PROP_TEXBUTTON_FMT, (int) i);
        SDL_SetPropertyWithCleanup(gamepad_props, propname, tex, cleanup_texture, NULL);
    }

    for (SDL_GamepadAxis i = SDL_GAMEPAD_AXIS_LEFTX; i < SDL_GAMEPAD_AXIS_MAX; i++) {
        int thissize = newsize;
        if ((i != SDL_GAMEPAD_AXIS_LEFT_TRIGGER) && (i != SDL_GAMEPAD_AXIS_RIGHT_TRIGGER)) {
            thissize *= 2;
        }
        SDL_Surface *surf = ControllerImage_CreateSurfaceForAxis(imgdev, i, thissize);
        SDL_Texture *tex = surf ? SDL_CreateTextureFromSurface(renderer, surf) : NULL;
        SDL_DestroySurface(surf);
        SDL_snprintf(propname, sizeof (propname), PROP_TEXAXIS_FMT, (int) i);
        SDL_SetPropertyWithCleanup(gamepad_props, propname, tex, cleanup_texture, NULL);
    }

    SDL_SetNumberProperty(gamepad_props, PROP_RENDER_SIZE, (Sint64) (int) newsize);
}

static void render_button(SDL_Renderer *renderer, SDL_PropertiesID gamepad_props, SDL_GamepadButton button, float x, float y, float size)
{
    SDL_assert(gamepad_props != 0);
    char propname[64];
    SDL_snprintf(propname, sizeof (propname), PROP_TEXBUTTON_FMT, (int) button);
    SDL_Texture *texture = (SDL_Texture *) SDL_GetProperty(gamepad_props, propname, NULL);
    if (texture) {
        const SDL_FRect dstrect = { x, y, size, size };
        SDL_RenderTexture(renderer, texture, NULL, &dstrect);
    }
}

static void render_axis(SDL_Renderer *renderer, SDL_PropertiesID gamepad_props, SDL_GamepadAxis axis, float x, float y, float size)
{
    SDL_assert(gamepad_props != 0);
    char propname[64];
    SDL_snprintf(propname, sizeof (propname), PROP_TEXAXIS_FMT, (int) axis);
    SDL_Texture *texture = (SDL_Texture *) SDL_GetProperty(gamepad_props, propname, NULL);
    if (texture) {
        const SDL_FRect dstrect = { x, y, size, size };
        SDL_RenderTexture(renderer, texture, NULL, &dstrect);
    }
}

int SDL_AppIterate(void)
{
    SDL_RenderClear(renderer);

    // original size of the bitmap. It's a hack.
    const int w = 512;
    const int h = 317;

    const float scale = (((float) winw) / ((float) w));
    const float fh = ((float) h) * scale;
    const float fy = (((float) winh) - fh) / 2.0f;
    const SDL_FRect gamepad_dstrect = { 0.0f, fy, (float) winw, fh };
    SDL_RenderTexture(renderer, gamepad_front_texture, NULL, &gamepad_dstrect);

    const float button_size = 38 * scale;

    SDL_PropertiesID gamepad_props = 0;
    if (artset_properties) {
        gamepad_props = artset_properties;
    } else if (current_gamepad) {
        gamepad_props = SDL_GetGamepadProperties(current_gamepad);
    }

    if (gamepad_props) {
        static const SDL_GamepadButton dpad_order[5] = {
            SDL_GAMEPAD_BUTTON_DPAD_UP,
            SDL_GAMEPAD_BUTTON_DPAD_RIGHT,
            SDL_GAMEPAD_BUTTON_DPAD_DOWN,
            SDL_GAMEPAD_BUTTON_DPAD_LEFT,
            SDL_GAMEPAD_BUTTON_DPAD_UP
        };

        static const SDL_GamepadAxis axisleft_order[5] = {
            SDL_GAMEPAD_AXIS_LEFTY,
            SDL_GAMEPAD_AXIS_LEFTX,
            SDL_GAMEPAD_AXIS_LEFTY,
            SDL_GAMEPAD_AXIS_LEFTX,
            SDL_GAMEPAD_AXIS_INVALID
        };

        static const SDL_GamepadAxis axisright_order[5] = {
            SDL_GAMEPAD_AXIS_RIGHTY,
            SDL_GAMEPAD_AXIS_RIGHTX,
            SDL_GAMEPAD_AXIS_RIGHTY,
            SDL_GAMEPAD_AXIS_RIGHTX,
            SDL_GAMEPAD_AXIS_INVALID
        };

        const Uint64 now = SDL_GetTicks();
        const int total_looptime = 3000;
        const int looptime = (int) (now % ((Uint64) total_looptime));
        const int section = (int) (((float) looptime) / (total_looptime / 5.0f));
        update_gamepad_textures(gamepad_props, button_size);
        render_button(renderer, gamepad_props, SDL_GAMEPAD_BUTTON_NORTH, 403 * scale, fy + (110 * scale), button_size);
        render_button(renderer, gamepad_props, SDL_GAMEPAD_BUTTON_SOUTH, 403 * scale, fy + (167 * scale), button_size);
        render_button(renderer, gamepad_props, SDL_GAMEPAD_BUTTON_WEST, 370 * scale, fy + (138 * scale), button_size);
        render_button(renderer, gamepad_props, SDL_GAMEPAD_BUTTON_EAST, 435 * scale, fy + (138 * scale), button_size);
        render_button(renderer, gamepad_props, SDL_GAMEPAD_BUTTON_BACK, 155 * scale, fy + (125 * scale), button_size);
        render_button(renderer, gamepad_props, SDL_GAMEPAD_BUTTON_GUIDE, 237 * scale, fy + (215 * scale), button_size);
        render_button(renderer, gamepad_props, SDL_GAMEPAD_BUTTON_TOUCHPAD, 200 * scale, fy + (105 * scale), button_size * 3);
        render_button(renderer, gamepad_props, SDL_GAMEPAD_BUTTON_START, 319 * scale, fy + (125 * scale), button_size);
        render_button(renderer, gamepad_props, dpad_order[section], 140 * scale, fy + (200 * scale), button_size * 2);
        render_button(renderer, gamepad_props, SDL_GAMEPAD_BUTTON_LEFT_SHOULDER, 65 * scale, fy + (40 * scale), button_size);
        render_button(renderer, gamepad_props, SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER, 410 * scale, fy + (40 * scale), button_size);
        if (section == 4) {
            render_button(renderer, gamepad_props, SDL_GAMEPAD_BUTTON_LEFT_STICK, 62 * scale, fy + (133 * scale), button_size * 2);
            render_button(renderer, gamepad_props, SDL_GAMEPAD_BUTTON_RIGHT_STICK, 292 * scale, fy + (207 * scale), button_size * 2);
        }

        render_axis(renderer, gamepad_props, axisleft_order[section], 62 * scale, fy + (133 * scale), button_size * 2);
        render_axis(renderer, gamepad_props, axisright_order[section], 292 * scale, fy + (207 * scale), button_size * 2);
        render_axis(renderer, gamepad_props, SDL_GAMEPAD_AXIS_LEFT_TRIGGER, 100 * scale, fy + (10 * scale), button_size);
        render_axis(renderer, gamepad_props, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER, 381 * scale, fy + (10 * scale), button_size);

#if 0  // !!! FIXME:
    SDL_GAMEPAD_BUTTON_MISC1,           /* Additional button (e.g. Xbox Series X share button, PS5 microphone button, Nintendo Switch Pro capture button, Amazon Luna microphone button) */
    SDL_GAMEPAD_BUTTON_RIGHT_PADDLE1,   /* Upper or primary paddle, under your right hand (e.g. Xbox Elite paddle P1) */
    SDL_GAMEPAD_BUTTON_LEFT_PADDLE1,    /* Upper or primary paddle, under your left hand (e.g. Xbox Elite paddle P3) */
    SDL_GAMEPAD_BUTTON_RIGHT_PADDLE2,   /* Lower or secondary paddle, under your right hand (e.g. Xbox Elite paddle P2) */
    SDL_GAMEPAD_BUTTON_LEFT_PADDLE2,    /* Lower or secondary paddle, under your left hand (e.g. Xbox Elite paddle P4) */
#endif
    }

    SDL_RenderPresent(renderer);

    return 0;  // keep going.
}

void SDL_AppQuit(void)
{
    SDL_DestroyProperties(artset_properties);
    SDL_DestroyTexture(gamepad_front_texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    ControllerImage_Quit();
}

