#pragma once
#include <SDL3/SDL.h>

class ATPokeyEmulator;

void ATInputSDL3_Init(ATPokeyEmulator *pokey);
void ATInputSDL3_HandleKeyDown(const SDL_KeyboardEvent& ev);
void ATInputSDL3_HandleKeyUp(const SDL_KeyboardEvent& ev);
