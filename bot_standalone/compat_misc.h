#ifndef COMPAT_MISC_H_
#define COMPAT_MISC_H_

#include "compat_album.h"
#include "defines.h"

void DebugBreak();

//--------------------------------------------------------------------------------------------
// Aus Debug.cpp
//--------------------------------------------------------------------------------------------

void HercPrintf(SLONG, const char *Format, ...);
void HercPrintf(const char *Format, ...);
void HercPrintfRed(const char *Format, ...);
void HercPrintfGreen(const char *Format, ...);
#define hprintf HercPrintf
#define redprintf HercPrintfRed
#define greenprintf HercPrintfGreen

//--------------------------------------------------------------------------------------------
// Aus Misc.cpp
//--------------------------------------------------------------------------------------------

SLONG ReadLine(BUFFER_V<UBYTE> &Buffer, SLONG BufferStart, char *Line, SLONG LineLength);
CString KorrigiereUmlaute(CString &Originaltext);
__int64 StringToInt64(const CString &String);
CString FullFilename(const CString &Filename, const CString &PathString);
CString Insert1000erDots(SLONG Value);

//--------------------------------------------------------------------------------------------
// Berechnet, wieviel ein Flug kostet:
//--------------------------------------------------------------------------------------------
SLONG CalculateFlightKerosin(SLONG VonCity, SLONG NachCity, SLONG Verbrauch, SLONG Geschwindigkeit);
SLONG CalculateFlightCostNoTank(SLONG VonCity, SLONG NachCity, SLONG Verbrauch, SLONG Geschwindigkeit);

bool GlobalUse(SLONG FeatureId);

#endif // COMPAT_MISC_H_
