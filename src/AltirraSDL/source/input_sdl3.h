#pragma once
#include <SDL3/SDL.h>

class ATPokeyEmulator;
class ATInputManager;
class ATGTIAEmulator;

void ATInputSDL3_Init(ATPokeyEmulator *pokey, ATInputManager *inputMgr, ATGTIAEmulator *gtia);
void ATInputSDL3_HandleKeyDown(const SDL_KeyboardEvent& ev);
void ATInputSDL3_HandleKeyUp(const SDL_KeyboardEvent& ev);
void ATInputSDL3_HandleTextInput(const char *text);
void ATInputSDL3_ReleaseAllKeys();
