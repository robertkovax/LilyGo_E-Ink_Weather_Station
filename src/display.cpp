#include "display.h"

bool LargeIcon = true;
bool SmallIcon = false;

// #########################################################################################
void drawString(int x, int y, String text, alignmentType alignment)
{
  int16_t x1, y1; // the bounds of x,y and w and h of the variable 'text' in pixels.
  uint16_t w, h;
  display.setTextWrap(false);
  display.getTextBounds(text, x, y, &x1, &y1, &w, &h);
  if (alignment == RIGHT)
    x = x - w;
  if (alignment == CENTER)
    x = x - w / 2;
  u8g2Fonts.setCursor(x, y + h);
  u8g2Fonts.print(text);
}

// ########################################################################################
// Turn "\n" (two chars) into an actual newline, also handle \r and \t.
String decodeEscapes(const String &in) {
  String out; out.reserve(in.length());
  for (int i = 0; i < (int)in.length(); ++i) {
    char c = in[i];
    if (c == '\\' && i + 1 < (int)in.length()) {
      char n = in[i + 1];
      if (n == 'n')      { out += '\n'; ++i; continue; }
      if (n == 'r')      { out += '\r'; ++i; continue; }
      if (n == 't')      { out += '\t'; ++i; continue; }
      // leave unknown escapes as-is
    }
    out += c;
  }
  return out;
}

// ########################################################################################
//  Assumes `alignmentType { LEFT, CENTER, RIGHT }`
//  and u8g2Fonts is already configured with the font you want.
static uint16_t textPixelWidth(const String &s)
{
  // u8g2-for-TFT_eSPI exposes UTF-8 width
  return u8g2Fonts.getUTF8Width(s.c_str());
}
// ########################################################################################
void drawStringMaxWidth(int x, int y, uint16_t max_w_px, const String &text, alignmentType align)
{
  String lines[8]; // up to 8 lines; enlarge if needed
  int line_count = 0;

  auto push_line = [&](const String &s){
    if (line_count < 8) lines[line_count++] = s;
  };

  String current;
  int i = 0, n = text.length();

  while (i < n)
  {
    // Hard break on '\n'
    if (text[i] == '\n') {
      if (!current.isEmpty()) push_line(current);
      else push_line(""); // preserve blank line
      current = "";
      ++i;
      continue;
    }
    if (text[i] == '\r') { ++i; continue; } // ignore CR

    // Collapse any run of whitespace (excluding '\n' which is handled above)
    bool saw_space = false;
    while (i < n) {
      char c = text[i];
      if (c == '\n' || c == '\r') break;
      if (isspace((unsigned char)c)) { saw_space = true; ++i; }
      else break;
    }

    // Read next word (non-space, non-newline)
    String word = "";
    while (i < n) {
      char c = text[i];
      if (c == '\n' || c == '\r' || isspace((unsigned char)c)) break;
      word += c;
      ++i;
    }

    if (word.isEmpty()) {
      // No word found (e.g., trailing spaces); continue loop
      continue;
    }

    // Try to place the word (insert one space only if current not empty and we saw any whitespace)
    String trial = current.isEmpty() ? word : (saw_space ? current + " " + word : current + word);

    // If word must start a new line due to width
    if (!current.isEmpty() && textPixelWidth(trial) > max_w_px) {
      push_line(current);
      current = word;
      // If the word itself is longer than the line, hard-break it
      if (textPixelWidth(current) > max_w_px) {
        String chunk = "";
        for (int k = 0; k < (int)word.length(); ++k) {
          String tryChunk = chunk + word[k];
          if (textPixelWidth(tryChunk) > max_w_px) {
            push_line(chunk);
            chunk = String(word[k]);
          } else {
            chunk = tryChunk;
          }
        }
        current = chunk; // remainder on current line
      }
    } else {
      // Fits on current line as-is
      if (current.isEmpty()) {
        // New line or start of block
        if (textPixelWidth(word) > max_w_px) {
          // Hard-break long word on an empty line
          String chunk = "";
          for (int k = 0; k < (int)word.length(); ++k) {
            String tryChunk = chunk + word[k];
            if (textPixelWidth(tryChunk) > max_w_px) {
              push_line(chunk);
              chunk = String(word[k]);
            } else {
              chunk = tryChunk;
            }
          }
          current = chunk;
        } else {
          current = word;
        }
      } else {
        current = trial;
      }
    }
  }

  if (!current.isEmpty() && line_count < 8) push_line(current);

  // Metrics & drawing unchanged
  int16_t ascent = u8g2Fonts.getFontAscent();
  int16_t descent = u8g2Fonts.getFontDescent(); // usually negative
  int line_h = (int)((ascent - descent) * 1.2f);

  uint16_t block_w = 0;
  for (int j = 0; j < line_count; ++j) {
    uint16_t w = textPixelWidth(lines[j]);
    if (w > block_w) block_w = w;
  }

  int draw_x = x;
  if (align == CENTER)      draw_x = x - (int)block_w / 2;
  else if (align == RIGHT)  draw_x = x - (int)block_w;

  for (int j = 0; j < line_count; ++j) {
    u8g2Fonts.setCursor(draw_x, y + j * line_h);
    u8g2Fonts.print(lines[j]);
  }
}
// ############################################################################################
void DrawBattery(int x, int y)
{
  uint8_t percentage = 100;
  float voltage = analogRead(35) / 4096.0 * 7.46;
  if (voltage > 1)
  { // Only display if there is a valid reading
    // Serial.println("Voltage = " + String(voltage));
    percentage = 2836.9625 * pow(voltage, 4) - 43987.4889 * pow(voltage, 3) + 255233.8134 * pow(voltage, 2) - 656689.7123 * voltage + 632041.7303;
    if (voltage >= 4.20)
      percentage = 100;
    if (voltage <= 3.50)
      percentage = 0;
    display.drawRect(x + 15, y - 12, 19, 10, GxEPD_BLACK);
    display.fillRect(x + 34, y - 10, 2, 5, GxEPD_BLACK);
    display.fillRect(x + 17, y - 10, 15 * percentage / 100.0, 6, GxEPD_BLACK);
    drawString(x + 60, y - 11, String(percentage) + "%", RIGHT);
    // drawString(x + 13, y + 5,  String(voltage, 2) + "v", CENTER);
  }
}

// #########################################################################################
//  Symbols are drawn on a relative 10x10grid and 1 scale unit = 1 drawing unit
void addcloud(int x, int y, int scale, int linesize)
{
  // Draw cloud outer
  display.fillCircle(x - scale * 3, y, scale, GxEPD_BLACK);                              // Left most circle
  display.fillCircle(x + scale * 3, y, scale, GxEPD_BLACK);                              // Right most circle
  display.fillCircle(x - scale, y - scale, scale * 1.4, GxEPD_BLACK);                    // left middle upper circle
  display.fillCircle(x + scale * 1.5, y - scale * 1.3, scale * 1.75, GxEPD_BLACK);       // Right middle upper circle
  display.fillRect(x - scale * 3 - 1, y - scale, scale * 6, scale * 2 + 1, GxEPD_BLACK); // Upper and lower lines
  // Clear cloud inner
  display.fillCircle(x - scale * 3, y, scale - linesize, GxEPD_WHITE);                                                   // Clear left most circle
  display.fillCircle(x + scale * 3, y, scale - linesize, GxEPD_WHITE);                                                   // Clear right most circle
  display.fillCircle(x - scale, y - scale, scale * 1.4 - linesize, GxEPD_WHITE);                                         // left middle upper circle
  display.fillCircle(x + scale * 1.5, y - scale * 1.3, scale * 1.75 - linesize, GxEPD_WHITE);                            // Right middle upper circle
  display.fillRect(x - scale * 3 + 2, y - scale + linesize - 1, scale * 5.9, scale * 2 - linesize * 2 + 2, GxEPD_WHITE); // Upper and lower lines
}
// #########################################################################################
void addraindrop(int x, int y, int scale)
{
  display.fillCircle(x, y, scale / 2, GxEPD_BLACK);
  display.fillTriangle(x - scale / 2, y, x, y - scale * 1.2, x + scale / 2, y, GxEPD_BLACK);
  x = x + scale * 1.6;
  y = y + scale / 3;
  display.fillCircle(x, y, scale / 2, GxEPD_BLACK);
  display.fillTriangle(x - scale / 2, y, x, y - scale * 1.2, x + scale / 2, y, GxEPD_BLACK);
}
// #########################################################################################
void addrain(int x, int y, int scale, bool IconSize)
{
  if (IconSize == SmallIcon)
    scale *= 1.34;
  for (int d = 0; d < 4; d++)
  {
    addraindrop(x + scale * (7.8 - d * 1.95) - scale * 5.2, y + scale * 2.1 - scale / 6, scale / 1.6);
  }
}
// #########################################################################################
void addsnow(int x, int y, int scale, bool IconSize)
{
  int dxo, dyo, dxi, dyi;
  for (int flakes = 0; flakes < 5; flakes++)
  {
    for (int i = 0; i < 360; i = i + 45)
    {
      dxo = 0.5 * scale * cos((i - 90) * 3.14 / 180);
      dxi = dxo * 0.1;
      dyo = 0.5 * scale * sin((i - 90) * 3.14 / 180);
      dyi = dyo * 0.1;
      display.drawLine(dxo + x + flakes * 1.5 * scale - scale * 3, dyo + y + scale * 2, dxi + x + 0 + flakes * 1.5 * scale - scale * 3, dyi + y + scale * 2, GxEPD_BLACK);
    }
  }
}
// #########################################################################################
void addtstorm(int x, int y, int scale)
{
  y = y + scale / 2;
  for (int i = 0; i < 5; i++)
  {
    display.drawLine(x - scale * 4 + scale * i * 1.5 + 0, y + scale * 1.5, x - scale * 3.5 + scale * i * 1.5 + 0, y + scale, GxEPD_BLACK);
    if (scale != Small)
    {
      display.drawLine(x - scale * 4 + scale * i * 1.5 + 1, y + scale * 1.5, x - scale * 3.5 + scale * i * 1.5 + 1, y + scale, GxEPD_BLACK);
      display.drawLine(x - scale * 4 + scale * i * 1.5 + 2, y + scale * 1.5, x - scale * 3.5 + scale * i * 1.5 + 2, y + scale, GxEPD_BLACK);
    }
    display.drawLine(x - scale * 4 + scale * i * 1.5, y + scale * 1.5 + 0, x - scale * 3 + scale * i * 1.5 + 0, y + scale * 1.5 + 0, GxEPD_BLACK);
    if (scale != Small)
    {
      display.drawLine(x - scale * 4 + scale * i * 1.5, y + scale * 1.5 + 1, x - scale * 3 + scale * i * 1.5 + 0, y + scale * 1.5 + 1, GxEPD_BLACK);
      display.drawLine(x - scale * 4 + scale * i * 1.5, y + scale * 1.5 + 2, x - scale * 3 + scale * i * 1.5 + 0, y + scale * 1.5 + 2, GxEPD_BLACK);
    }
    display.drawLine(x - scale * 3.5 + scale * i * 1.4 + 0, y + scale * 2.5, x - scale * 3 + scale * i * 1.5 + 0, y + scale * 1.5, GxEPD_BLACK);
    if (scale != Small)
    {
      display.drawLine(x - scale * 3.5 + scale * i * 1.4 + 1, y + scale * 2.5, x - scale * 3 + scale * i * 1.5 + 1, y + scale * 1.5, GxEPD_BLACK);
      display.drawLine(x - scale * 3.5 + scale * i * 1.4 + 2, y + scale * 2.5, x - scale * 3 + scale * i * 1.5 + 2, y + scale * 1.5, GxEPD_BLACK);
    }
  }
}
// #########################################################################################
void addsun(int x, int y, int scale, bool IconSize)
{
  int linesize = 3;
  if (IconSize == SmallIcon)
    linesize = 1;
  display.fillRect(x - scale * 2, y, scale * 4, linesize, GxEPD_BLACK);
  display.fillRect(x, y - scale * 2, linesize, scale * 4, GxEPD_BLACK);
  display.drawLine(x - scale * 1.3, y - scale * 1.3, x + scale * 1.3, y + scale * 1.3, GxEPD_BLACK);
  display.drawLine(x - scale * 1.3, y + scale * 1.3, x + scale * 1.3, y - scale * 1.3, GxEPD_BLACK);
  if (IconSize == LargeIcon)
  {
    display.drawLine(1 + x - scale * 1.3, y - scale * 1.3, 1 + x + scale * 1.3, y + scale * 1.3, GxEPD_BLACK);
    display.drawLine(2 + x - scale * 1.3, y - scale * 1.3, 2 + x + scale * 1.3, y + scale * 1.3, GxEPD_BLACK);
    display.drawLine(3 + x - scale * 1.3, y - scale * 1.3, 3 + x + scale * 1.3, y + scale * 1.3, GxEPD_BLACK);
    display.drawLine(1 + x - scale * 1.3, y + scale * 1.3, 1 + x + scale * 1.3, y - scale * 1.3, GxEPD_BLACK);
    display.drawLine(2 + x - scale * 1.3, y + scale * 1.3, 2 + x + scale * 1.3, y - scale * 1.3, GxEPD_BLACK);
    display.drawLine(3 + x - scale * 1.3, y + scale * 1.3, 3 + x + scale * 1.3, y - scale * 1.3, GxEPD_BLACK);
  }
  display.fillCircle(x, y, scale * 1.3, GxEPD_WHITE);
  display.fillCircle(x, y, scale, GxEPD_BLACK);
  display.fillCircle(x, y, scale - linesize, GxEPD_WHITE);
}
// #########################################################################################
void addfog(int x, int y, int scale, int linesize, bool IconSize)
{
  if (IconSize == SmallIcon)
  {
    linesize = 1;
    y = y - 1;
  }
  else
    y = y - 3;
  for (int i = 0; i < 6; i++)
  {
    display.fillRect(x - scale * 3, y + scale * 1.5, scale * 6, linesize, GxEPD_BLACK);
    display.fillRect(x - scale * 3, y + scale * 2.0, scale * 6, linesize, GxEPD_BLACK);
    display.fillRect(x - scale * 3, y + scale * 2.6, scale * 6, linesize, GxEPD_BLACK);
  }
}
// #########################################################################################
void Sunny(int x, int y, bool IconSize, String IconName)
{ // 01
  int scale = 4;
  if (IconSize == LargeIcon)
  {
    scale = Large;
    y = y - 4; // Shift up large sun
  }
  else
    y = y + 2; // Shift down small sun icon

  if (IconName.endsWith("n"))
  {
    addmoononly(x + 10, y - 1, scale, IconSize);
    if (IconSize == LargeIcon)
    {
      draw4Star(x + 17, y - 10, 8, GxEPD_BLACK);
      draw4Star(x + 5, y - 18, 4, GxEPD_BLACK);
    }
    else
      draw4Star(x + 9, y - 12, 4, GxEPD_BLACK);
  }
  else if (IconSize == LargeIcon)
  {
    scale = scale * 1.6;
    addsun(x, y, scale, IconSize);
  }
  else
  {
    scale = scale * 1.4;
    addsun(x, y - 7, scale, IconSize);
  }
}
// #########################################################################################
void MostlySunny(int x, int y, bool IconSize, String IconName)
{ // 02
  int scale = Small, linesize = 3, offset = 0;
  if (IconSize == LargeIcon)
  {
    scale = Large;
    offset = 10;
  }
  if (scale == Small)
    linesize = 1;

  if (IconName.endsWith("n"))
  {
    addmoononly(x, y + (IconSize ? -8 : 0), scale, IconSize);
    addcloud(x, y + offset, scale, linesize);
  }
  else
  {
    addcloud(x, y + offset, scale, linesize);
    addsun(x - scale * 1.8, y - scale * 1.8 + offset, scale, IconSize);
  }
}
// #########################################################################################
void MostlyCloudy(int x, int y, bool IconSize, String IconName)
{ // 03
  int scale = Large, linesize = 3;
  if (IconSize == SmallIcon)
  {
    scale = Small;
    linesize = 1;
    if (IconName.endsWith("n"))
    {
      addmoononly(x, y - 4, scale, IconSize);
      addcloud(x, y - 5, scale, linesize);
      addcloud(x - 5, y, scale, linesize);
    }
    else
    {
      addsun(x - 10, y - 10, scale, IconSize);
      addcloud(x, y - 5, scale, linesize);
      addcloud(x - 5, y, scale, linesize);
    }
  }
  else
  {
    if (IconName.endsWith("n"))
    {
      addmoononly(x, y - 8, scale, IconSize);
      addcloud(x + 3, y, scale, linesize);
      addcloud(x, y + 10, scale, linesize);
    }
    else
    {
      addsun(x - scale * 1.8, y - scale * 1.8, scale, IconSize);
      addcloud(x + 3, y, scale, linesize);
      addcloud(x, y + 10, scale, linesize);
    }
  }
}
// #########################################################################################
void Cloudy(int x, int y, bool IconSize, String IconName)
{ // 04
  int scale = Large, linesize = 3;
  if (IconSize == SmallIcon)
  {
    scale = Small;
    // if (IconName.endsWith("n")) addmoon(x, y, scale, IconSize);
    linesize = 1;
    addcloud(x - 6, y - 7, scale, linesize); // Cloud top left
    addcloud(x, y - 5, scale, linesize);     // Cloud top right
    addcloud(x - 5, y, scale, linesize);     // Main cloud
  }
  else
  {
    y += 12;
    // if (IconName.endsWith("n")) addmoononly(x - 5, y - 15, scale, IconSize);
    addcloud(x - 11, y - 10, 5, linesize); // Cloud top left
    addcloud(x + 7, y - 21, 5, linesize);  // Cloud top right
    addcloud(x, y, scale, linesize);       // Main cloud
  }
}
// #########################################################################################
void Rain(int x, int y, bool IconSize, String IconName)
{ // 10
  int scale = Large, linesize = 3;
  if (IconSize == SmallIcon)
  {
    scale = Small;
    linesize = 1;
  }
  // if (IconName.endsWith("n")) addmoon(x, y, scale, IconSize);
  addcloud(x, y + (IconSize ? 2 : -4), scale, linesize);
  addrain(x, y + (IconSize ? 2 : -4), scale, IconSize);
}
// #########################################################################################
void ExpectRain(int x, int y, bool IconSize, String IconName)
{
  int scale = Large, linesize = 3;
  if (IconSize == SmallIcon)
  {
    scale = Small;
    linesize = 1;
  }
  if (IconName.endsWith("n"))
    addmoon(x, y, scale, IconSize);
  addsun(x - scale * 1.8, y - scale * 1.8, scale, IconSize);
  addcloud(x, y, scale, linesize);
  addrain(x, y, scale, IconSize);
}
// #########################################################################################
void ChanceRain(int x, int y, bool IconSize, String IconName)
{ // 09
  int scale = Large, linesize = 3;
  if (IconSize == SmallIcon)
  {
    scale = Small;
    linesize = 1;
  }
  if (IconName.endsWith("n"))
  {
    addmoononly(x - (IconSize ? 2 : 0), y - (IconSize ? 12 : 4), scale, IconSize);
  }
  else
  {
    addsun(x - scale * 1.8, y - scale * 1.8 - (IconSize ? 0 : 4), scale, IconSize);
  }
  addcloud(x, y + (IconSize ? 2 : -4), scale, linesize);
  addrain(x, y + (IconSize ? 2 : -4), scale, linesize);
}
// #########################################################################################
void Tstorms(int x, int y, bool IconSize, String IconName)
{ // 11
  int scale = Large, linesize = 3;
  if (IconSize == SmallIcon)
  {
    scale = Small;
    linesize = 1;
  }
  // if (IconName.endsWith("n")) addmoon(x, y, scale, IconSize);
  addcloud(x, y + (IconSize ? 2 : -4), scale, linesize);
  addtstorm(x, y + (IconSize ? 2 : -4), scale);
}
// #########################################################################################
void Snow(int x, int y, bool IconSize, String IconName)
{ // 13
  int scale = Large, linesize = 3;
  if (IconSize == SmallIcon)
  {
    scale = Small;
    linesize = 1;
  }
  // if (IconName.endsWith("n")) addmoon(x, y, scale, IconSize);
  addcloud(x, y + (IconSize ? 2 : -4), scale, linesize);
  addsnow(x, y + (IconSize ? 2 : -4), scale, IconSize);
}
// #########################################################################################
void Fog(int x, int y, bool IconSize, String IconName)
{ // 50
  int linesize = 3, scale = Large;
  if (IconSize == SmallIcon)
  {
    scale = Small;
    linesize = 1;
    y = y + 5;
  }
  // if (IconName.endsWith("n")) addmoon(x, y, scale, IconSize);
  addcloud(x, y + (IconSize ? -2 : -7), scale, linesize);
  addfog(x, y + (IconSize ? 1 : -4), scale, linesize, IconSize);
}
// #########################################################################################
void Haze(int x, int y, bool IconSize, String IconName)
{
  int linesize = 3, scale = Large;
  if (IconSize == SmallIcon)
  {
    scale = Small;
    linesize = 1;
  }
  // if (IconName.endsWith("n")) addmoon(x, y, scale, IconSize);
  addsun(x, y - 2, scale * 1.4, IconSize);
  addfog(x, y + 3 - (IconSize ? 12 : 0), scale * 1.4, linesize, IconSize);
}
// #########################################################################################
void CloudCover(int x, int y, int CCover)
{
  addcloud(x - 9, y - 3, Small * 0.6, 2); // Cloud top left
  addcloud(x + 3, y - 3, Small * 0.6, 2); // Cloud top right
  addcloud(x, y, Small * 0.6, 2);         // Main cloud
  u8g2Fonts.setFont(u8g2_font_helvB08_tf);
  drawString(x + 15, y - 5, String(CCover) + "%", LEFT);
}
// #########################################################################################
void Visibility(int x, int y, String Visi)
{
  y = y - 3; //
  float start_angle = 0.52, end_angle = 2.61;
  int r = 10;
  for (float i = start_angle; i < end_angle; i = i + 0.05)
  {
    display.drawPixel(x + r * cos(i), y - r / 2 + r * sin(i), GxEPD_BLACK);
    display.drawPixel(x + r * cos(i), 1 + y - r / 2 + r * sin(i), GxEPD_BLACK);
  }
  start_angle = 3.61;
  end_angle = 5.78;
  for (float i = start_angle; i < end_angle; i = i + 0.05)
  {
    display.drawPixel(x + r * cos(i), y + r / 2 + r * sin(i), GxEPD_BLACK);
    display.drawPixel(x + r * cos(i), 1 + y + r / 2 + r * sin(i), GxEPD_BLACK);
  }
  display.fillCircle(x, y, r / 4, GxEPD_BLACK);
  u8g2Fonts.setFont(u8g2_font_helvB08_tf);
  drawString(x + 12, y - 3, Visi, LEFT);
}
// #########################################################################################
void addmoon(int x, int y, int scale, bool IconSize)
{
  if (IconSize == LargeIcon)
  {
    x = x - 5;
    y = y + 5;
    display.fillCircle(x - 21, y - 23, scale, GxEPD_BLACK);
    display.fillCircle(x - 14, y - 23, scale * 1.7, GxEPD_WHITE);
  }
  else
  {
    display.fillCircle(x - 16, y - 11, scale, GxEPD_BLACK);
    display.fillCircle(x - 11, y - 11, scale * 1.7, GxEPD_WHITE);
  }
}
// #########################################################################################
void addmoononly(int x, int y, int scale, bool IconSize)
{
  if (IconSize == LargeIcon)
  {
    x = x + 10;
    y = y + 25;
    // display.drawCircle
    display.fillCircle(x - 20, y - 23, scale * 2.3, GxEPD_BLACK);
    display.fillCircle(x - 2, y - 25, scale * 3.5, GxEPD_WHITE);
  }
  else
  {
    display.fillCircle(x - 8, y - 6, scale * 1.8, GxEPD_BLACK);
    display.fillCircle(x - 3, y - 6, scale * 2, GxEPD_WHITE);
  }
}
// #########################################################################################
void draw4Star(int16_t x, int16_t y, int16_t radius, uint16_t color)
{
  float angle_step = 360.0 / 8.0;     // 8 points for a star shap
  int16_t r_in = round(radius * 0.3); // You can tweak this ratio for sharpness

  int16_t points_x[9]; // 8 + 1 to close the shape
  int16_t points_y[9];

  points_x[0] = x;
  points_y[0] = y + radius;
  points_x[1] = x + r_in;
  points_y[1] = y + r_in;
  points_x[2] = x + radius;
  points_y[2] = y;
  points_x[3] = x + r_in;
  points_y[3] = y - r_in;
  points_x[4] = x;
  points_y[4] = y - radius;
  points_x[5] = x - r_in;
  points_y[5] = y - r_in;
  points_x[6] = x - radius;
  points_y[6] = y;
  points_x[7] = x - r_in;
  points_y[7] = y + r_in;
  points_x[8] = points_x[0];
  points_y[8] = points_y[0];

  for (int i = 0; i < 8; i++)
  {
    display.fillTriangle(x, y, points_x[i], points_y[i], points_x[i + 1], points_y[i + 1], color);
  }
}
// #########################################################################################
void Nodata(int x, int y, bool IconSize, String IconName)
{
  if (IconSize == LargeIcon)
    u8g2Fonts.setFont(u8g2_font_helvB24_tf);
  else
    u8g2Fonts.setFont(u8g2_font_helvB10_tf);
  drawString(x - 3, y - 8, "?", CENTER);
  u8g2Fonts.setFont(u8g2_font_helvB08_tf);
}
// #########################################################################################
void DrawMoon(int x, int y, int dd, int mm, int yy)
{
  const int diameter = 28;        // tweak as you like
  const int number_of_lines = 90; // rendering resolution (radial scan lines)
  const double eps = 1e-6;        // avoid edge-case degeneracy

  // 0 = New, 0.5 = Full, 1.0 = New
  double Phase = NormalizedMoonPhase(dd, mm, yy);
  // rotate by 180° for the drawing logic
  Phase = fmod(Phase + 0.5, 1.0); // <<--- SHIFT HERE

  // Flip for southern hemisphere so waxing lights the LEFT, waning the RIGHT (as seen in south)
  if (LAT < 0)
  {
    Phase = 1.0 - Phase;
  }
  // Clamp away from exact edges to keep the limb math stable
  if (Phase <= 0.0)
    Phase = eps;
  if (Phase >= 1.0)
    Phase = 1.0 - eps;

  // --- Background disk (dark part) ---
  display.fillCircle(x + diameter - 1, y + diameter, diameter / 2 + 1, GxEPD_BLACK);

  // --- Scanline fill of illuminated part ---
  for (double Ypos = 0; Ypos <= number_of_lines / 2; Ypos++)
  {
    double Xpos = sqrt((number_of_lines / 2.0) * (number_of_lines / 2.0) - Ypos * Ypos);
    double Rpos = 2.0 * Xpos;
    double Xpos1, Xpos2;

    //   0..0.5  = waxing (light on RIGHT in north, LEFT in south)
    //   0.5..1  = waning
    if (Phase < 0.5)
    {
      Xpos1 = -Xpos;
      Xpos2 = Rpos - 2.0 * Phase * Rpos - Xpos;
    }
    else
    {
      Xpos1 = Xpos;
      Xpos2 = Xpos - 2.0 * Phase * Rpos + Rpos;
    }

    // Map unit coords to screen pixels
    double pW1x = (Xpos1 + number_of_lines) / number_of_lines * diameter + x;
    double pW1y = (number_of_lines - Ypos) / number_of_lines * diameter + y;
    double pW2x = (Xpos2 + number_of_lines) / number_of_lines * diameter + x;
    double pW2y = (number_of_lines - Ypos) / number_of_lines * diameter + y;
    double pW3x = (Xpos1 + number_of_lines) / number_of_lines * diameter + x;
    double pW3y = (Ypos + number_of_lines) / number_of_lines * diameter + y;
    double pW4x = (Xpos2 + number_of_lines) / number_of_lines * diameter + x;
    double pW4y = (Ypos + number_of_lines) / number_of_lines * diameter + y;

    display.drawLine(pW1x, pW1y, pW2x, pW2y, GxEPD_WHITE);
    display.drawLine(pW3x, pW3y, pW4x, pW4y, GxEPD_WHITE);
  }
  // Rim
  display.drawCircle(x + diameter - 1, y + diameter, diameter / 2, GxEPD_BLACK);
}

// #########################################################################################
//  Squeeze some wind info into a tiny space - just the speed, direction, and an arrow
//  No nice compass :-(
void DrawSmallWind(int x, int y, float angle, float windspeed)
{
  float Cradius = 0;
  float dx = Cradius * cos((angle - 90) * PI / 180) + x; // calculate X position
  float dy = Cradius * sin((angle - 90) * PI / 180) + y; // calculate Y position
  arrow(dx, dy, Cradius - 15, angle, 10, 20);            // Show wind direction as just an arrow
  u8g2Fonts.setFont(u8g2_font_helvB08_tf);
  drawString(x, y + 15, WindDegToDirection(angle), CENTER);
  drawString(x + 5, y + 25, String(windspeed * 3.6, 1), CENTER);
  drawString(x + 5, y + 35, String(String(Units) == "M" ? " km/h" : " mph"), CENTER);
}
// #########################################################################################
void DrawWind(int x, int y, float angle, float windspeed)
{
  float Cradius = 15;
  float dx = Cradius * cos((angle - 90) * PI / 180) + x; // calculate X position
  float dy = Cradius * sin((angle - 90) * PI / 180) + y; // calculate Y position
  arrow(x, y, Cradius - 3, angle, 10, 12);               // Show wind direction on outer circle
  display.drawCircle(x, y, Cradius + 2, GxEPD_BLACK);
  display.drawCircle(x, y, Cradius + 3, GxEPD_BLACK);
  for (int m = 0; m < 360; m = m + 45)
  {
    dx = Cradius * cos(m * PI / 180); // calculate X position
    dy = Cradius * sin(m * PI / 180); // calculate Y position
    display.drawLine(x + dx, y + dy, x + dx * 0.8, y + dy * 0.8, GxEPD_BLACK);
  }
  u8g2Fonts.setFont(u8g2_font_helvB10_tf);
  drawString(x - 7, y + Cradius + 10, WindDegToDirection(angle), CENTER);
  u8g2Fonts.setFont(u8g2_font_helvB08_tf);
  drawString(x, y - Cradius - 14, String(windspeed, 1) + (String(Units) == "M" ? " m/s" : " mph"), CENTER);
}
// #########################################################################################
void arrow(int x, int y, int asize, float aangle, int pwidth, int plength)
{
  // x,y is the centre poistion of the arrow and asize is the radius out from the x,y position
  // aangle is angle to draw the pointer at e.g. at 45° for NW
  // pwidth is the pointer width in pixels
  // plength is the pointer length in pixels
  float dx = (asize + 28) * cos((aangle - 90) * PI / 180) + x; // calculate X position
  float dy = (asize + 28) * sin((aangle - 90) * PI / 180) + y; // calculate Y position
  float x1 = 0;
  float y1 = plength;
  float x2 = pwidth / 2;
  float y2 = pwidth / 2;
  float x3 = -pwidth / 2;
  float y3 = pwidth / 2;
  float angle = aangle * PI / 180;
  float xx1 = x1 * cos(angle) - y1 * sin(angle) + dx;
  float yy1 = y1 * cos(angle) + x1 * sin(angle) + dy;
  float xx2 = x2 * cos(angle) - y2 * sin(angle) + dx;
  float yy2 = y2 * cos(angle) + x2 * sin(angle) + dy;
  float xx3 = x3 * cos(angle) - y3 * sin(angle) + dx;
  float yy3 = y3 * cos(angle) + x3 * sin(angle) + dy;
  display.fillTriangle(xx1, yy1, xx3, yy3, xx2, yy2, GxEPD_BLACK);
}
// #########################################################################################
void DrawPressureTrend(int x, int y, float pressure, String slope)
{
  drawString(x, y, String(pressure, (String(Units) == "M" ? 0 : 1)) + (String(Units) == "M" ? "hPa" : "in"), LEFT);
  x = x + 48 - (String(Units) == "M" ? 0 : 15);
  y = y + 3;
  if (slope == "+")
  {
    display.drawLine(x, y, x + 4, y - 4, GxEPD_BLACK);
    display.drawLine(x + 4, y - 4, x + 8, y, GxEPD_BLACK);
  }
  else if (slope == "0")
  {
    display.drawLine(x + 3, y - 4, x + 8, y, GxEPD_BLACK);
    display.drawLine(x + 3, y + 4, x + 8, y, GxEPD_BLACK);
  }
  else if (slope == "-")
  {
    display.drawLine(x, y, x + 4, y + 4, GxEPD_BLACK);
    display.drawLine(x + 4, y + 4, x + 8, y, GxEPD_BLACK);
  }
}
// #########################################################################################
// Help debug screen layout by drawing a grid of little crosses
void Draw_Grid(int sreen_width, int screen_height)
{
  int x, y;
  const int grid_step = 10;
  // Draw the screen border so we know how far we can push things out
  display.drawLine(0, 0, sreen_width - 1, 0, GxEPD_BLACK);                                 // across top
  display.drawLine(0, screen_height - 1, sreen_width - 1, screen_height - 1, GxEPD_BLACK); // across bottom
  display.drawLine(0, 0, 0, screen_height - 1, GxEPD_BLACK);                               // lhs
  display.drawLine(sreen_width - 1, 0, sreen_width - 1, screen_height - 1, GxEPD_BLACK);   // rhs

  for (x = grid_step; x < sreen_width; x += grid_step)
  {
    for (y = grid_step; y < screen_height; y += grid_step)
    {
      display.drawLine(x - 1, y, x + 1, y, GxEPD_BLACK); // Horizontal line
      display.drawLine(x, y - 1, x, y + 1, GxEPD_BLACK); // Vertical line
    }
  }
}