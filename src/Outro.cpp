//============================================================================================
// Outro.cpp : Der Render-Outro
//============================================================================================
#include "Outro.h"

#include "global.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

void COutro::OnVideoDone() { Sim.Gamestate = GAMESTATE_BOOT; }
void COutro::OnVideoCancel() { Sim.Gamestate = GAMESTATE_BOOT; }