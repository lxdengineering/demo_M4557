#ifndef __ESI_M4557_H__
#define __ESI_M4557_H__

#include <stdint.h>

// ESI M4557 demo application
void esi_M4557();

// esiGetXY - Returns true or false depending on whether a touch is
//            currently active. If active, x,y coords of the touch,
//            are also returned as a calibrated x,y pair in "LCD units" (128x64).
bool esiGetXY(int16_t *x, int16_t *y);

#endif
