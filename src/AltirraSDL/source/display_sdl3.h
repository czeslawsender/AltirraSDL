#pragma once
#include <SDL3/SDL.h>
#include <vd2/VDDisplay/display.h>

class VDVideoDisplaySDL3;
VDVideoDisplaySDL3 *VDVideoDisplaySDL3_Create(SDL_Renderer *renderer, int w, int h);

// Call from main loop to present the latest received frame
void VDVideoDisplaySDL3_Present(VDVideoDisplaySDL3 *display);
