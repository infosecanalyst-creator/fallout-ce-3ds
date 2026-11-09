// In-game instruction manual (3DS). Section list on the touch screen, the
// text on the top screen. Runs as a modal loop, so the game is paused while
// it is open. The text comes from ctr_manual_text.h (tools/gen_manual.py).
#include "ctr_manual.h"

#include <3ds.h>
#include <citro2d.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <string>
#include <vector>

#include "ctr_gfx.h"
#include "ctr_manual_text.h"

namespace fallout {

namespace {

enum LineStyle {
    STYLE_HEADING,
    STYLE_TEXT,
    STYLE_BULLET_FIRST,
    STYLE_BULLET_MORE,
    STYLE_ROW_FIRST,
    STYLE_ROW_MORE,
};

struct ManualLine {
    int style;
    std::string key; // bullet mark or table key (first line only)
    std::string text;
    float y;
};

struct ManualSection {
    std::string title;
    std::vector<std::string> source;
    std::vector<ManualLine> lines; // wrapped on first view
    float height = 0;
    bool wrapped = false;
};

const float kTextScale = 0.47f;
const float kLineHeight = 15.0f;
const float kTop = 26.0f; // body starts under the header bar
const float kBottom = 240.0f;
const float kLeft = 8.0f;
const float kRight = 392.0f;
const float kKeyColumn = 112.0f;
const float kBulletIndent = 18.0f;

std::vector<ManualSection> gSections;
int gCurrent = 0;
float gScroll[64];
C2D_TextBuf gBuf = nullptr;
C2D_TextBuf gMeasureBuf = nullptr;

float measure(const std::string& s)
{
    if (s.empty()) {
        return 0;
    }
    C2D_Text t;
    C2D_TextBufClear(gMeasureBuf);
    C2D_TextParse(&t, gMeasureBuf, s.c_str());
    float w = 0, h = 0;
    C2D_TextGetDimensions(&t, kTextScale, kTextScale, &w, &h);
    return w;
}

std::vector<std::string> wrap(const std::string& text, float width)
{
    std::vector<std::string> out;
    std::string line;
    size_t i = 0;
    float spaceW = measure("a a") - measure("aa");
    float lineW = 0;
    while (i < text.size()) {
        size_t j = text.find(' ', i);
        if (j == std::string::npos) {
            j = text.size();
        }
        std::string word = text.substr(i, j - i);
        float w = measure(word);
        if (!line.empty() && lineW + spaceW + w > width) {
            out.push_back(line);
            line = word;
            lineW = w;
        } else {
            if (!line.empty()) {
                line += ' ';
                lineW += spaceW;
            }
            line += word;
            lineW += w;
        }
        i = j + 1;
    }
    if (!line.empty() || out.empty()) {
        out.push_back(line);
    }
    return out;
}

void load()
{
    if (!gSections.empty()) {
        return;
    }
    for (const char* raw : kManualLines) {
        std::string l = raw;
        if (l.compare(0, 2, "# ") == 0) {
            ManualSection s;
            s.title = l.substr(2);
            gSections.push_back(s);
        } else if (!gSections.empty()) {
            gSections.back().source.push_back(l);
        }
    }
    memset(gScroll, 0, sizeof(gScroll));
}

void layout(ManualSection& s)
{
    if (s.wrapped) {
        return;
    }
    float y = 0;
    bool lastBlank = true;
    for (const std::string& l : s.source) {
        if (l.empty()) {
            if (!lastBlank) {
                y += kLineHeight * 0.5f;
            }
            lastBlank = true;
            continue;
        }
        lastBlank = false;
        if (l.compare(0, 3, "## ") == 0) {
            if (y > 0) {
                y += kLineHeight * 0.4f;
            }
            s.lines.push_back({ STYLE_HEADING, "", l.substr(3), y });
            y += kLineHeight + 2;
        } else if (l.compare(0, 2, "| ") == 0) {
            std::string rest = l.substr(2);
            size_t bar = rest.find(" | ");
            std::string key = bar == std::string::npos ? rest : rest.substr(0, bar);
            std::string val = bar == std::string::npos ? "" : rest.substr(bar + 3);
            std::vector<std::string> parts = wrap(val, kRight - kKeyColumn);
            if (measure(key) > kKeyColumn - kLeft - 6) {
                // A key too wide for its column gets a line of its own, so it
                // never runs into the text beside it.
                s.lines.push_back({ STYLE_ROW_FIRST, key, "", y });
                y += kLineHeight;
                key = "";
            }
            for (size_t k = 0; k < parts.size(); k++) {
                s.lines.push_back({ k == 0 ? STYLE_ROW_FIRST : STYLE_ROW_MORE, k == 0 ? key : "", parts[k], y });
                y += kLineHeight;
            }
            y += 2;
        } else if (l.compare(0, 2, "- ") == 0 || (l.size() > 3 && l[0] >= '1' && l[0] <= '9' && l[1] == '.' && l[2] == ' ')) {
            bool numbered = l[0] != '-';
            std::string mark = numbered ? l.substr(0, 2) : "-";
            std::vector<std::string> parts = wrap(l.substr(numbered ? 3 : 2), kRight - kLeft - kBulletIndent);
            for (size_t k = 0; k < parts.size(); k++) {
                s.lines.push_back({ k == 0 ? STYLE_BULLET_FIRST : STYLE_BULLET_MORE, k == 0 ? mark : "", parts[k], y });
                y += kLineHeight;
            }
        } else {
            for (const std::string& part : wrap(l, kRight - kLeft)) {
                s.lines.push_back({ STYLE_TEXT, "", part, y });
                y += kLineHeight;
            }
        }
    }
    s.height = y;
    s.wrapped = true;
}

float maxScroll(const ManualSection& s)
{
    float m = s.height - (kBottom - kTop) + 6;
    return m > 0 ? m : 0;
}

void text(const std::string& str, float x, float y, float scale, u32 color, u32 flags = 0, float depth = 0.5f)
{
    if (str.empty()) {
        return;
    }
    C2D_Text t;
    C2D_TextParse(&t, gBuf, str.c_str());
    C2D_TextOptimize(&t);
    C2D_DrawText(&t, C2D_WithColor | flags, x, y, depth, scale, scale, color);
}

const u32 kBg = C2D_Color32(0x0b, 0x14, 0x0b, 0xff);
const u32 kPanel = C2D_Color32(0x16, 0x26, 0x16, 0xff);
const u32 kGreen = C2D_Color32(0x5c, 0xff, 0x5c, 0xff);
const u32 kDim = C2D_Color32(0x8c, 0xd8, 0x8c, 0xff);
const u32 kAmber = C2D_Color32(0xff, 0xc8, 0x40, 0xff);
const u32 kLine = C2D_Color32(0x3c, 0xb0, 0x3c, 0xff);

void drawTop()
{
    ManualSection& s = gSections[gCurrent];
    float scroll = gScroll[gCurrent];
    C2D_DrawRectSolid(0, 0, 0.4f, 400, 240, kBg);

    for (const ManualLine& l : s.lines) {
        float y = kTop + l.y - scroll;
        if (y < kTop - kLineHeight || y > kBottom) {
            continue;
        }
        switch (l.style) {
        case STYLE_HEADING:
            text(l.text, kLeft, y, 0.52f, kAmber);
            break;
        case STYLE_TEXT:
            text(l.text, kLeft, y, kTextScale, kDim);
            break;
        case STYLE_BULLET_FIRST:
        case STYLE_BULLET_MORE:
            text(l.key, kLeft + 4, y, kTextScale, kGreen);
            text(l.text, kLeft + kBulletIndent, y, kTextScale, kDim);
            break;
        case STYLE_ROW_FIRST:
        case STYLE_ROW_MORE:
            text(l.key, kLeft, y, kTextScale, kGreen);
            text(l.text, kKeyColumn, y, kTextScale, kDim);
            break;
        }
    }

    // Header bar over the scrolled text
    C2D_DrawRectSolid(0, 0, 0.6f, 400, 22, kPanel);
    C2D_DrawRectSolid(0, 22, 0.6f, 400, 1, kLine);
    text(s.title, kLeft, 2, 0.58f, kAmber, 0, 0.8f);
    char page[16];
    snprintf(page, sizeof(page), "%d / %d", gCurrent + 1, (int)gSections.size());
    text(page, 392, 5, 0.45f, kLine, C2D_AlignRight, 0.8f);

    // Scroll bar
    float ms = maxScroll(s);
    if (ms > 0) {
        float viewH = kBottom - kTop;
        float barH = viewH * viewH / (viewH + ms);
        float barY = kTop + (viewH - barH) * (scroll / ms);
        C2D_DrawRectSolid(396, kTop, 0.6f, 3, viewH, kPanel);
        C2D_DrawRectSolid(396, barY, 0.7f, 3, barH, kLine);
    }
}

const int kColumns = 2;
const float kCellW = 156.0f;
const float kCellH = 22.0f;
const float kGridX = 4.0f;
const float kGridY = 26.0f;
const float kCloseY = 214.0f;

int rowsPerColumn()
{
    return ((int)gSections.size() + kColumns - 1) / kColumns;
}

void cellRect(int index, float* x, float* y)
{
    int rows = rowsPerColumn();
    *x = kGridX + (index / rows) * (kCellW + 4);
    *y = kGridY + (index % rows) * (kCellH + 1);
}

void drawBottom()
{
    C2D_DrawRectSolid(0, 0, 0.4f, 320, 240, kBg);
    text(kManualTitle, 160, 4, 0.5f, kAmber, C2D_AlignCenter);
    for (int i = 0; i < (int)gSections.size(); i++) {
        float x, y;
        cellRect(i, &x, &y);
        bool on = i == gCurrent;
        C2D_DrawRectSolid(x, y, 0.45f, kCellW, kCellH, on ? kLine : kPanel);
        text(gSections[i].title, x + 6, y + 4, 0.42f, on ? C2D_Color32(0, 0, 0, 0xff) : kGreen);
    }
    C2D_DrawRectSolid(4, kCloseY, 0.45f, 312, 22, kPanel);
    text(ctr_lefty() ? "Close (D-Down)   ABXY, L/R: section   Sticks: scroll" : "Close (B)    D-Pad, L / R: section    Sticks: scroll", 160, kCloseY + 4, 0.42f, kDim, C2D_AlignCenter);
}

void frame()
{
    C2D_TextBufClear(gBuf);
    ctr_gfx_manual_frame(drawTop, drawBottom);
}

void select(int index)
{
    int n = (int)gSections.size();
    gCurrent = (index % n + n) % n;
    layout(gSections[gCurrent]);
}

} // namespace

void ctr_manual_run()
{
    load();
    if (gSections.empty()) {
        return;
    }
    if (gBuf == nullptr) {
        gBuf = C2D_TextBufNew(4096);
        gMeasureBuf = C2D_TextBufNew(512);
    }
    select(gCurrent);

    // Wait for the buttons that opened the manual to be released.
    while (aptMainLoop()) {
        hidScanInput();
        if ((hidKeysHeld() & (KEY_SELECT | KEY_START | KEY_TOUCH)) == 0) {
            break;
        }
        frame();
    }

    while (aptMainLoop()) {
        hidScanInput();
        u32 down = ctr_remap(hidKeysDown());
        u32 held = ctr_remap(hidKeysHeld());

        if (down & (KEY_B | KEY_START | KEY_SELECT)) {
            break;
        }
        if (down & KEY_L) {
            select(gCurrent - 1);
        }
        if (down & KEY_R) {
            select(gCurrent + 1);
        }

        if (down & KEY_TOUCH) {
            touchPosition touch;
            hidTouchRead(&touch);
            if (touch.py >= kCloseY) {
                break;
            }
            for (int i = 0; i < (int)gSections.size(); i++) {
                float x, y;
                cellRect(i, &x, &y);
                if (touch.px >= x && touch.px < x + kCellW && touch.py >= y && touch.py < y + kCellH) {
                    select(i);
                }
            }
        }

        float& scroll = gScroll[gCurrent];
        circlePosition cp;
        hidCircleRead(&cp);
        circlePosition cs;
        hidCstickRead(&cs);
        int dy = abs(cp.dy) > 20 ? cp.dy : (abs(cs.dy) > 20 ? cs.dy : 0);
        scroll -= dy / 20.0f;
        // D-pad moves between the section buttons (it does not scroll):
        // up/down within a column, left/right to the other column.
        {
            int n = (int)gSections.size();
            int rows = rowsPerColumn();
            int next = gCurrent;
            if (down & KEY_DUP) next = gCurrent - 1;
            if (down & KEY_DDOWN) next = gCurrent + 1;
            if (down & KEY_DLEFT) next = gCurrent - rows;
            if (down & KEY_DRIGHT) next = gCurrent + rows;
            if (next != gCurrent && next >= 0 && next < n) select(next);
        }
        float ms = maxScroll(gSections[gCurrent]);
        if (gScroll[gCurrent] < 0) gScroll[gCurrent] = 0;
        if (gScroll[gCurrent] > ms) gScroll[gCurrent] = ms;

        frame();
    }

    // Don't let the closing press reach the game.
    while (aptMainLoop()) {
        hidScanInput();
        if (hidKeysHeld() == 0) {
            break;
        }
        frame();
    }
}

static bool gLefty = false, gLeftyLoaded = false;
static const char* const kLeftyPath = "sdmc:/3ds/fallout2/.lefty";

static void leftyLoad()
{
    FILE* f = fopen(kLeftyPath, "r");
    gLefty = f != nullptr;
    if (f) fclose(f);
    gLeftyLoaded = true;
}

static void leftySet(bool on)
{
    gLefty = on;
    if (on) {
        if (FILE* f = fopen(kLeftyPath, "w")) fclose(f);
    } else {
        remove(kLeftyPath);
    }
}

bool ctr_lefty()
{
    if (!gLeftyLoaded) leftyLoad();
    return gLefty;
}

u32 ctr_remap(u32 m)
{
    static const u32 pr[4][2] = { { KEY_Y, KEY_DUP }, { KEY_A, KEY_DRIGHT }, { KEY_B, KEY_DDOWN }, { KEY_X, KEY_DLEFT } };
    if (!gLeftyLoaded) leftyLoad();
    if (!gLefty) return m;
    u32 r = m;
    for (int i = 0; i < 4; i++) r &= ~(pr[i][0] | pr[i][1]);
    for (int i = 0; i < 4; i++) {
        if (m & pr[i][0]) r |= pr[i][1];
        if (m & pr[i][1]) r |= pr[i][0];
    }
    return r;
}

namespace {

const char* const kMenuItems[4] = { "Manual", "Options", "Left hand: off", "Close" };
int gMenuSel = 0;
int gMenuPressed = -1;

void menuRect(int i, float* x, float* y, float* w, float* h)
{
    *x = 40;
    *w = 240;
    *h = 40;
    *y = 14 + i * 56;
}

void menuDrawTop()
{
    C2D_DrawRectSolid(0, 0, 0.4f, 400, 240, kBg);
    text("Menu", 200, 100, 0.8f, kAmber, C2D_AlignCenter);
    text("A / tap: choose    B or Start: close", 200, 140, 0.45f, kDim, C2D_AlignCenter);
}

void menuDrawBottom()
{
    C2D_DrawRectSolid(0, 0, 0.4f, 320, 240, kBg);
    for (int i = 0; i < 4; i++) {
        float x, y, w, h;
        menuRect(i, &x, &y, &w, &h);
        bool on = i == gMenuSel;
        bool pressed = i == gMenuPressed;
        float inset = pressed ? 3.0f : 0.0f; // visible press animation
        C2D_DrawRectSolid(x + inset, y + inset, 0.45f, w - 2 * inset, h - 2 * inset,
            pressed ? kLine : (on ? kLine : kPanel));
        text(i == 2 ? (gLefty ? "Left hand: on" : "Left hand: off") : kMenuItems[i], x + w / 2, y + 12, 0.55f,
            (on || pressed) ? C2D_Color32(0, 0, 0, 0xff) : kGreen, C2D_AlignCenter);
    }
}

} // namespace

int ctr_startmenu_run()
{
    if (gBuf == nullptr) {
        gBuf = C2D_TextBufNew(4096);
        gMeasureBuf = C2D_TextBufNew(512);
    }
    gMenuSel = 0;
    gMenuPressed = -1;
    int result = 2;
    bool waitRelease = true;
    while (aptMainLoop()) {
        hidScanInput();
        u32 down = ctr_remap(hidKeysDown());
        u32 held = ctr_remap(hidKeysHeld());
        if (waitRelease) {
            if ((held & (KEY_START | KEY_SELECT | KEY_TOUCH | KEY_A | KEY_B)) == 0) {
                waitRelease = false;
            }
        } else {
            if (down & (KEY_B | KEY_START)) {
                result = 2;
                break;
            }
            if (down & KEY_DUP) gMenuSel = (gMenuSel + 3) % 4;
            if (down & KEY_DDOWN) gMenuSel = (gMenuSel + 1) % 4;
            if (down & KEY_A) {
                if (gMenuSel == 2) {
                    leftySet(!gLefty);
                } else {
                    result = gMenuSel == 3 ? 2 : gMenuSel;
                    break;
                }
            }
            if (held & KEY_TOUCH) {
                touchPosition t;
                hidTouchRead(&t);
                gMenuPressed = -1;
                for (int i = 0; i < 4; i++) {
                    float x, y, w, h;
                    menuRect(i, &x, &y, &w, &h);
                    if (t.px >= x && t.px < x + w && t.py >= y && t.py < y + h) {
                        gMenuPressed = i;
                        gMenuSel = i;
                    }
                }
            } else if (gMenuPressed >= 0) {
                if (gMenuPressed == 2) {
                    leftySet(!gLefty);
                    gMenuPressed = -1;
                } else {
                    result = gMenuPressed == 3 ? 2 : gMenuPressed;
                    break;
                }
            }
        }
        C2D_TextBufClear(gBuf);
        ctr_gfx_manual_frame(menuDrawTop, menuDrawBottom);
    }
    // Don't let the closing press reach the game.
    while (aptMainLoop()) {
        hidScanInput();
        if (hidKeysHeld() == 0) break;
        C2D_TextBufClear(gBuf);
        ctr_gfx_manual_frame(menuDrawTop, menuDrawBottom);
    }
    return result;
}

} // namespace fallout
