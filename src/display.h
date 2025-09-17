#pragma once
#ifndef WEATHER_ICONS_H
#define WEATHER_ICONS_H

#include <Arduino.h>   // for String, byte, uint8_t, int16_t, uint16_t
#include <math.h>      // for pow, sin, cos
#include <GxEPD2_BW.h>
#include <GxEPD2_3C.h>
#include <U8g2_for_Adafruit_GFX.h>
#include "setup_server.h"
#include "lang.h"   
#include "common.h"

extern GxEPD2_BW<GxEPD2_213_BN, GxEPD2_213_BN::HEIGHT> display;
extern U8G2_FOR_ADAFRUIT_GFX u8g2Fonts;

// Optional externs for constants used by the functions (define them in one .cpp)
// If you already define these elsewhere, keep these externs; otherwise remove.
extern bool LargeIcon; 
extern bool SmallIcon;
#define Large 7    // For best results use odd numbers
#define Small 3    // For best results use odd numbers
enum alignmentType {
    LEFT,
    RIGHT,
    CENTER
};

//------String write------
void drawString(int x, int y, String text, alignmentType alignment);
void drawStringMaxWidth(int x, int y, uint16_t max_w_px, const String& text, alignmentType align);

// -----Helper------
void Draw_Grid();

// ---- Battery ----
void DrawBattery(int x, int y);

// ---- Primitive icon parts ----
void addcloud(int x, int y, int scale, int linesize);
void addraindrop(int x, int y, int scale);
void addrain(int x, int y, int scale, bool IconSize);
void addsnow(int x, int y, int scale, bool IconSize);
void addtstorm(int x, int y, int scale);
void addsun(int x, int y, int scale, bool IconSize);
void addfog(int x, int y, int scale, int linesize, bool IconSize);
void addmoon(int x, int y, int scale, bool IconSize);
void addmoononly(int x, int y, int scale, bool IconSize);
void draw4Star(int16_t x, int16_t y, int16_t radius, uint16_t color);
void DrawPressureTrend(int x, int y, float pressure, String slope);
void arrow(int x, int y, int asize, float aangle, int pwidth, int plength);

// ---- Composite weather icons ----
void Sunny(int x, int y, bool IconSize, String IconName);          // 01
void MostlySunny(int x, int y, bool IconSize, String IconName);    // 02
void MostlyCloudy(int x, int y, bool IconSize, String IconName);   // 03
void Cloudy(int x, int y, bool IconSize, String IconName);         // 04
void Rain(int x, int y, bool IconSize, String IconName);           // 10
void ExpectRain(int x, int y, bool IconSize, String IconName);
void ChanceRain(int x, int y, bool IconSize, String IconName);     // 09
void Tstorms(int x, int y, bool IconSize, String IconName);        // 11
void Snow(int x, int y, bool IconSize, String IconName);           // 13
void Fog(int x, int y, bool IconSize, String IconName);            // 50
void Haze(int x, int y, bool IconSize, String IconName);
void CloudCover(int x, int y, int CCover);
void Visibility(int x, int y, String Visi);
void Nodata(int x, int y, bool IconSize, String IconName);

// ---- weather sections ----
void DrawWind(int x, int y, float angle, float windspeed);
void DrawSmallWind(int x, int y, float angle, float windspeed);
void DrawMoon(int x, int y, int dd, int mm, int yy);


#endif // WEATHER_ICONS_H
