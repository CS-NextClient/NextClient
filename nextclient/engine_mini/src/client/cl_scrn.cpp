#include "../engine.h"

void SCR_UpdateScreen()
{
    eng()->SCR_UpdateScreen.InvokeChained();
}
