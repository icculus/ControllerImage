#include <SDL3/SDL.h>
#include "controllerimage.h"

#define SDL_MAIN_USE_CALLBACKS 1
#include <SDL3/SDL_main.h>

#define MAX_FLOOD_TEXTURES 128

static SDL_Window *window = NULL;
static SDL_Renderer *renderer = NULL;

typedef struct TextureFloodItem
{
    SDL_GamepadButton button;
    int size;
    int velocity;
    int x;
    int y;
} TextureFloodItem;

static void (*IterateMode)(void) = NULL;

static int winw, winh;
static float mousex, mousey;
static SDL_Texture *ps_textures[MAX_FLOOD_TEXTURES];
static SDL_Texture *xbox_textures[MAX_FLOOD_TEXTURES];
static TextureFloodItem flood[MAX_FLOOD_TEXTURES];
static int do_flood = 0;
static int selected_controller = -1;

static SDL_Texture *press_x_to_doubt = NULL;
static SDL_Texture *press_f_to_pay_respects = NULL;

static SDL_Texture *miles_xbox = NULL;
static SDL_Texture *miles_ps5 = NULL;

static SDL_Texture *how_to_use0 = NULL;
static SDL_Texture *how_to_use1 = NULL;
static SDL_Texture *how_to_use2 = NULL;
static SDL_Texture *how_to_use3 = NULL;

static SDL_Texture *gamepad_front = NULL;
static SDL_Texture *gamepad_xbox_buttons[4];
static SDL_Texture *gamepad_ps_buttons[4];
static int gamepad_button_size = 0;

// I really want an SDL_random().  :(
static int random_seed = 0;
static int RandomNumber(void)
{
    // this is POSIX.1-2001's potentially bad suggestion, but we're not exactly doing cryptography here.
    random_seed = random_seed * 1103515245 + 12345;
    return (int) ((unsigned int) (random_seed / 65536) % 32768);
}

// between lo and hi (inclusive; it can return lo or hi itself, too!).
static int RandomNumberBetween(const int lo, const int hi)
{
    return (RandomNumber() % (hi + 1 - lo)) + lo;
}

static void InitFlood(void)
{
    SDL_zeroa(flood);

    for (int i = 0; i < MAX_FLOOD_TEXTURES; i++) {
        const int size = (i < 4) ? 20 : RandomNumberBetween(20, 512);
        const SDL_GamepadButton button = (SDL_GamepadButton) ((i < 4) ? (SDL_GAMEPAD_BUTTON_SOUTH + i) : RandomNumberBetween(SDL_GAMEPAD_BUTTON_SOUTH, SDL_GAMEPAD_BUTTON_NORTH));
        TextureFloodItem *item = &flood[i];
        item->button = button;
        item->size = size;
        item->velocity = RandomNumberBetween(3, 10);
        item->x = RandomNumberBetween(-(size / 2), winw + (size / 2));
        item->y = RandomNumberBetween(-size * 10, -size);
    }
}

static void LoadControllerImages(ControllerImage_Device *imgdev, SDL_Texture **textures)
{
    if (imgdev) {
        for (int i = 0; i < MAX_FLOOD_TEXTURES; i++) {
            SDL_DestroyTexture(textures[i]);
            textures[i] = NULL;
        }

        for (int i = 0; i < MAX_FLOOD_TEXTURES; i++) {
            TextureFloodItem *item = &flood[i];
            SDL_Surface *surf = ControllerImage_CreateSurfaceForButton(imgdev, item->button, item->size);
            if (surf) {
                textures[i] = SDL_CreateTextureFromSurface(renderer, surf);
                SDL_SetTextureScaleMode(textures[i], SDL_SCALEMODE_LINEAR);
                SDL_DestroySurface(surf);
            }
        }
        ControllerImage_DestroyDevice(imgdev);
    }
}

int SDL_AppInit(int argc, char *argv[])
{
    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD);

    winw = 1024;
    winh = 768;

    window = SDL_CreateWindow(argv[0], winw, winh, SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIDDEN);
    renderer = SDL_CreateRenderer(window, NULL, SDL_RENDERER_PRESENTVSYNC);

    ControllerImage_Init();

    SDL_version compiled;
    CONTROLLERIMAGE_VERSION(&compiled);
    SDL_Log("Compiled against ControllerImage %d.%d.%d\n", compiled.major, compiled.minor, compiled.patch);
    const SDL_version *linked = ControllerImage_LinkedVersion();
    SDL_Log("Linked against ControllerImage %d.%d.%d\n", linked->major, linked->minor, linked->patch);

    ControllerImage_AddDataFromFile("controllerimage-standard.bin");

    random_seed = (int) ((unsigned int) (SDL_GetPerformanceCounter() & 0xFFFFFFFF));

    InitFlood();
    LoadControllerImages(ControllerImage_CreateGamepadDeviceByIdString("xbox360"), xbox_textures);
    LoadControllerImages(ControllerImage_CreateGamepadDeviceByIdString("ps3"), ps_textures);

    SDL_Surface *surf;
    surf = SDL_LoadBMP("gamepad_front.bmp");
    gamepad_front = SDL_CreateTextureFromSurface(renderer, surf);
    SDL_SetTextureScaleMode(gamepad_front, SDL_SCALEMODE_LINEAR);
    SDL_DestroySurface(surf);

    surf = SDL_LoadBMP("press-x-to-doubt.bmp");
    press_x_to_doubt = SDL_CreateTextureFromSurface(renderer, surf);
    SDL_SetTextureScaleMode(gamepad_front, SDL_SCALEMODE_LINEAR);
    SDL_DestroySurface(surf);

    surf = SDL_LoadBMP("press-f-to-pay-respects.bmp");
    press_f_to_pay_respects = SDL_CreateTextureFromSurface(renderer, surf);
    SDL_SetTextureScaleMode(gamepad_front, SDL_SCALEMODE_LINEAR);
    SDL_DestroySurface(surf);

    surf = SDL_LoadBMP("miles-ps5.bmp");
    miles_ps5 = SDL_CreateTextureFromSurface(renderer, surf);
    SDL_SetTextureScaleMode(gamepad_front, SDL_SCALEMODE_LINEAR);
    SDL_DestroySurface(surf);

    surf = SDL_LoadBMP("miles-xbox.bmp");
    miles_xbox = SDL_CreateTextureFromSurface(renderer, surf);
    SDL_SetTextureScaleMode(gamepad_front, SDL_SCALEMODE_LINEAR);
    SDL_DestroySurface(surf);

    surf = SDL_LoadBMP("how_to_use0.bmp");
    how_to_use0 = SDL_CreateTextureFromSurface(renderer, surf);
    SDL_SetTextureScaleMode(gamepad_front, SDL_SCALEMODE_LINEAR);
    SDL_DestroySurface(surf);

    surf = SDL_LoadBMP("how_to_use1.bmp");
    how_to_use1 = SDL_CreateTextureFromSurface(renderer, surf);
    SDL_SetTextureScaleMode(gamepad_front, SDL_SCALEMODE_LINEAR);
    SDL_DestroySurface(surf);

    surf = SDL_LoadBMP("how_to_use2.bmp");
    how_to_use2 = SDL_CreateTextureFromSurface(renderer, surf);
    SDL_SetTextureScaleMode(gamepad_front, SDL_SCALEMODE_LINEAR);
    SDL_DestroySurface(surf);

    surf = SDL_LoadBMP("how_to_use3.bmp");
    how_to_use3 = SDL_CreateTextureFromSurface(renderer, surf);
    SDL_SetTextureScaleMode(gamepad_front, SDL_SCALEMODE_LINEAR);
    SDL_DestroySurface(surf);

    SDL_ShowWindow(window);

    return 0;
}

typedef void(*IterateFn)(void);

static void IterateSlide(SDL_Texture *texture)
{
    int w, h;
    SDL_QueryTexture(texture, NULL, NULL, &w, &h);
    const float scale = (((float) winw) / ((float) w));
    const float fh = ((float) h) * scale;
    const float fy = (((float) winh) - fh) / 2.0f;
    const SDL_FRect dstrect = { 0.0f, fy, (float) winw, fh };
    SDL_RenderTexture(renderer, texture, NULL, &dstrect);
    SDL_Delay(10);
}

static void IteratePressXToDoubt(void)
{
    IterateSlide(press_x_to_doubt);
}

static void IteratePressFToPayRespects(void)
{
    IterateSlide(press_f_to_pay_respects);
}

static void IterateMilesXbox(void)
{
    IterateSlide(miles_xbox);
}

static void IterateMilesPS5(void)
{
    IterateSlide(miles_ps5);
}

static void IterateHowToUse0(void)
{
    IterateSlide(how_to_use0);
}

static void IterateHowToUse1(void)
{
    IterateSlide(how_to_use1);
}

static void IterateHowToUse2(void)
{
    IterateSlide(how_to_use2);
}

static void IterateHowToUse3(void)
{
    IterateSlide(how_to_use3);
}

static void IterateGamepad(void)
{
    if (!gamepad_front) {
        return;
    }

    int w = 512, h = 317;

    const float scale = (((float) winw) / ((float) w));
    const float fh = ((float) h) * scale;
    const float fy = (((float) winh) - fh) / 2.0f;
    const SDL_FRect gamepad_dstrect = { 0.0f, fy, (float) winw, fh };
    SDL_RenderTexture(renderer, gamepad_front, NULL, &gamepad_dstrect);

    const float buttonw = 38 * scale;
    const float buttonh = 34 * scale;

    if (((int) buttonw) != gamepad_button_size) {
        ControllerImage_Device *imgdev_xbox = ControllerImage_CreateGamepadDeviceByIdString("xbox360");
        ControllerImage_Device *imgdev_ps = ControllerImage_CreateGamepadDeviceByIdString("ps3");
        for (int i = 0; i < 4; i++) {
            SDL_Surface *surf;
            SDL_DestroyTexture(gamepad_xbox_buttons[i]);
            surf = ControllerImage_CreateSurfaceForButton(imgdev_xbox, i + SDL_GAMEPAD_BUTTON_SOUTH, buttonw);
            gamepad_xbox_buttons[i] = SDL_CreateTextureFromSurface(renderer, surf);
            SDL_DestroySurface(surf);
            SDL_DestroyTexture(gamepad_ps_buttons[i]);
            surf = ControllerImage_CreateSurfaceForButton(imgdev_ps, i + SDL_GAMEPAD_BUTTON_SOUTH, buttonw);
            gamepad_ps_buttons[i] = SDL_CreateTextureFromSurface(renderer, surf);
            SDL_DestroySurface(surf);
        }

        ControllerImage_DestroyDevice(imgdev_ps);
        ControllerImage_DestroyDevice(imgdev_xbox);
    
        gamepad_button_size = (int) buttonw;
    }

    if (selected_controller != -1) {
        SDL_Texture **textures = (selected_controller == 0) ? gamepad_xbox_buttons : gamepad_ps_buttons;

        const SDL_FRect s_dstrect = { 394 * scale, fy + (171 * scale), buttonw, buttonh };
        SDL_RenderTexture(renderer, textures[SDL_GAMEPAD_BUTTON_SOUTH], NULL, &s_dstrect);
        const SDL_FRect e_dstrect = { 436 * scale, fy + (138 * scale), buttonw, buttonh };
        SDL_RenderTexture(renderer, textures[SDL_GAMEPAD_BUTTON_EAST], NULL, &e_dstrect);
        const SDL_FRect w_dstrect = { 352 * scale, fy + (141 * scale), buttonw, buttonh };
        SDL_RenderTexture(renderer, textures[SDL_GAMEPAD_BUTTON_WEST], NULL, &w_dstrect);
        const SDL_FRect n_dstrect = { 395 * scale, fy + (110 * scale), buttonw, buttonh };
        SDL_RenderTexture(renderer, textures[SDL_GAMEPAD_BUTTON_NORTH], NULL, &n_dstrect);
    }
}

static void IterateFlood(void)
{
    if (do_flood && (selected_controller != -1)) {
        SDL_Texture **textures = (selected_controller == 0) ? xbox_textures : ps_textures;
        for (int i = 0; i < MAX_FLOOD_TEXTURES; i++) {
            TextureFloodItem *item = &flood[i];
            SDL_Texture *texture = (do_flood == 1) ? textures[item->button + SDL_GAMEPAD_BUTTON_SOUTH] : textures[i];
            if (texture && (item->y < winh)) {
                const SDL_FRect dstrect = { item->x, item->y, item->size, item->size };

                SDL_RenderTexture(renderer, texture, NULL, &dstrect);
                item->y += item->velocity;
                if ((do_flood < 3) && (item->y > winh)) {
                    //item->velocity = RandomNumberBetween(3, 10);
                    item->x = RandomNumberBetween(-(item->size / 2), winw + (item->size / 2));
                    item->y = -item->size;
                }
            }
        }
    }
}

static void IterateNoOp(void) {}

static IterateFn iterate_funcs[] = {
    IterateNoOp,
    IterateGamepad,
    IteratePressXToDoubt,
    IterateGamepad,
    IterateMilesPS5,
    IterateMilesXbox,
    IteratePressFToPayRespects,
    IterateGamepad,
    IterateHowToUse0,
    IterateHowToUse1,
    IterateHowToUse2,
    IterateHowToUse3,
    IterateFlood,
};

static int current_iterate_func = 0;

int SDL_AppEvent(const SDL_Event *event)
{
    switch (event->type) {
        case SDL_EVENT_MOUSE_MOTION:
            mousex = event->motion.x;
            mousey = event->motion.y;
            break;

        case SDL_EVENT_QUIT:
            return 1;  // we're done.

        case SDL_EVENT_GAMEPAD_ADDED:
            //LoadControllerImages(ControllerImage_CreateGamepadDeviceByInstance(event->gdevice.which));
            break;

        case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
            winw = event->window.data1;
            winh = event->window.data2;
            for (int i = 0; i < MAX_FLOOD_TEXTURES; i++) {
                TextureFloodItem *item = &flood[i];
                item->x = RandomNumberBetween(-(item->size / 2), winw + (item->size / 2));
                item->y = RandomNumberBetween(-item->size * 10, -item->size);
            }
            break;

        case SDL_EVENT_KEY_DOWN:
            switch (event->key.keysym.sym) {
                case SDLK_RIGHT:
                    current_iterate_func++;
                    if (current_iterate_func >= SDL_arraysize(iterate_funcs)) {
                        current_iterate_func = 0;
                    }
                    IterateMode = iterate_funcs[current_iterate_func];
                    selected_controller = -1;
                    break;

                case SDLK_LEFT:
                    current_iterate_func--;
                    if (current_iterate_func < 0) {
                        current_iterate_func = SDL_arraysize(iterate_funcs) - 1;
                    }
                    IterateMode = iterate_funcs[current_iterate_func];
                    selected_controller = -1;
                    break;

                case SDLK_n:
                    selected_controller = -1;
                    break;

                case SDLK_x:
                    selected_controller = 0;
                    break;

                case SDLK_p:
                    selected_controller = 1;
                    break;

                case SDLK_f:
                    do_flood++;
                    if (do_flood == 3) {
                        for (int i = 0; i < MAX_FLOOD_TEXTURES; i++) {
                            TextureFloodItem *item = &flood[i];
                            item->velocity = 10;
                            if ((item->y + item->size) < 0) {
                                item->y = winh + 1;
                            }
                        }
                    } else if (do_flood > 3) {
                        do_flood = 0;
                        for (int i = 0; i < MAX_FLOOD_TEXTURES; i++) {
                            TextureFloodItem *item = &flood[i];
                            item->velocity = RandomNumberBetween(1, 10);
                            item->x = RandomNumberBetween(-(item->size / 2), winw + (item->size / 2));
                            item->y = RandomNumberBetween(-item->size * 10, -item->size);
                        }
                    }
                    break;

                default: break;
            }
            break;

        default: break;
    }

    return 0;
}

int SDL_AppIterate(void)
{
    SDL_SetRenderDrawColor(renderer, 127, 127, 127, 255);
    SDL_RenderClear(renderer);

    if (IterateMode) {
        IterateMode();
    }

    SDL_RenderPresent(renderer);
    return 0;
}

void SDL_AppQuit(void)
{
    for (int i = 0; i < MAX_FLOOD_TEXTURES; i++) {
        SDL_DestroyTexture(xbox_textures[i]);
        SDL_DestroyTexture(ps_textures[i]);
    }

    SDL_DestroyTexture(gamepad_front);
    for (int i = 0; i < 4; i++) {
        SDL_DestroyTexture(gamepad_xbox_buttons[i]);
        SDL_DestroyTexture(gamepad_ps_buttons[i]);
    }

    SDL_DestroyTexture(press_x_to_doubt);
    SDL_DestroyTexture(miles_ps5);
    SDL_DestroyTexture(miles_xbox);

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    ControllerImage_Quit();
}

