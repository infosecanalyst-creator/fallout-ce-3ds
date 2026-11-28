// In-game instruction manual (3DS). Section list on the touch screen, the
// text on the top screen. Runs as a modal loop, so the game is paused while
// it is open. The text comes from ctr_manual_text.h (tools/gen_manual.py).
#include "ctr_manual.h"

#include <3ds.h>
#include <citro2d.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include <string>
#include <vector>

#include "ctr_gfx.h"
#include "ctr_manual_text.h"
#include "click3ds.h"

namespace fallout {

bool ctr_exit_saves(void);   // ctr_input.cc (FO-04)
int ctr_exit_kind(void);     // ctr_input.cc: CTR_EXIT_* (what EXIT GAME can do on this screen)

#ifndef PORT_VERSION
#define PORT_VERSION "unknown"
#endif

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
const float kBottom = 224.0f; // the footer strip is below
const float kLeft = 8.0f;
const float kRight = 392.0f;
const float kKeyColumn = 112.0f;
const float kBulletIndent = 18.0f;

std::vector<ManualSection> gSections;
int gCurrent = 0;
float gScroll[64];
C2D_TextBuf gBuf = nullptr;
C2D_TextBuf gMeasureBuf = nullptr;
bool gLeftySaveFailed = false;   // the left-hand marker could not be written (FALLOUT-Q6)

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

const u32 kBg = C2D_Color32(8, 12, 40, 0xff);
const u32 kPanel = C2D_Color32(40, 56, 120, 0xff);
const u32 kGreen = C2D_Color32(240, 240, 240, 0xff);
const u32 kDim = C2D_Color32(150, 150, 165, 0xff);
const u32 kAmber = C2D_Color32(255, 204, 64, 0xff);
const u32 kLine = C2D_Color32(120, 150, 230, 0xff);

void drawTop()
{
    ManualSection& s = gSections[gCurrent];
    float scroll = gScroll[gCurrent];
    C2D_DrawRectSolid(0, 0, 0.4f, 400, 240, kBg);

    for (const ManualLine& l : s.lines) {
        float y = kTop + l.y - scroll;
        // FALLOUT-U4: whole lines only. A line that would be cut through by the header bar or
        // the footer strip is left out until it fits.
        if (y < kTop - 0.5f || y + kLineHeight > kBottom + 0.5f) {
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

    // Footer strip over the scrolled text: the house wording, the same in every port
    C2D_DrawRectSolid(0, kBottom, 0.6f, 400, 240 - kBottom, C2D_Color32(20, 30, 70, 0xff));
    const char* foot = gLeftySaveFailed ? "Left hand: could not save the setting (SD card locked or full?)"
                     : ctr_lefty() ? "Sticks: scroll   A/B/X/Y, L / R: section   D-Down / START: close"
                                   : "Sticks: scroll   D-Pad, L / R: section   B / START: close";
    float fs = 0.42f, fw = measure(foot) * fs / kTextScale;
    if (fw > 384) fs *= 384 / fw;   // shrink to fit, never cut off
    text(foot, 200, kBottom + 2, fs, kAmber, C2D_AlignCenter, 0.8f);
}

const int kColumns = 2;
// Rule 11: buttons 24 px tall, 4 px apart, 6 px from the screen edges. The
// last cell of the grid is Close (15 sections + Close = 8 rows, y 6..226).
const float kCellW = 152.0f;
const float kCellH = 24.0f;
const float kGridX = 6.0f;
const float kGridY = 6.0f;
const float kGap = 4.0f;

int rowsPerColumn()
{
    return ((int)gSections.size() + 1 + kColumns - 1) / kColumns;   // + the Close cell
}

// D-pad focus: a section (which opens as it is picked), n = LEFT HAND, n + 1 = CLOSE. The last
// cell of the grid is shared by those two (FALLOUT-U7: house manual has a Left hand toggle).
int gFocus = 0;
int gCellPressed = -1;        // pressed look for 150 ms after a tap or A
u64 gCellPressedUntil = 0;
void cellRect(int index, float* x, float* y, float* w)
{
    int n = (int)gSections.size();
    int rows = rowsPerColumn();
    int cell = index > n ? n : index;
    *x = kGridX + (cell / rows) * (kCellW + kGap);
    *y = kGridY + (cell % rows) * (kCellH + kGap);
    *w = kCellW;
    if (index >= n) {
        *w = (kCellW - kGap) / 2;
        if (index == n + 1) *x += *w + kGap;
    }
}

void drawBottom()
{
    const u32 edge = C2D_Color32(120, 150, 230, 0xff), press = C2D_Color32(220, 180, 60, 0xff);
    const u32 shade = C2D_Color32(20, 30, 70, 0xff);
    C2D_DrawRectSolid(0, 0, 0.4f, 320, 240, kBg);
    const int n = (int)gSections.size();
    const bool flash = osGetTime() < gCellPressedUntil;
    for (int i = 0; i <= n + 1; i++) {
        float x, y, w;
        cellRect(i, &x, &y, &w);
        const bool on = i == gCurrent;
        const bool pressed = flash && i == gCellPressed;
        if (i == gFocus) C2D_DrawRectSolid(x - 2, y - 2, 0.44f, w + 4, kCellH + 4, kAmber);   // gold focus ring
        C2D_DrawRectSolid(x, y, 0.45f, w, kCellH, edge);
        C2D_DrawRectSolid(x + 1, y + 1, 0.46f, w - 2, kCellH - 2, pressed ? press : kPanel);
        if (!pressed) C2D_DrawRectSolid(x + 1, y + kCellH - 2, 0.47f, w - 2, 1, shade);
        const char* label = i < n ? gSections[i].title.c_str() : (i == n ? (ctr_lefty() ? "LEFT: ON" : "LEFT: OFF") : "CLOSE");
        float sc = 0.42f, tw = measure(label) * sc / kTextScale;
        if (tw > w - 10) sc *= (w - 10) / tw;   // never cut off
        text(label, x + w / 2, y + 5 + (pressed ? 1 : 0), sc, pressed ? C2D_Color32(0, 0, 0, 0xff) : (on ? kAmber : kGreen), C2D_AlignCenter);
    }
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
    gFocus = gCurrent;
    layout(gSections[gCurrent]);
}

void pressCell(int i)
{
    click3ds::play();
    gCellPressed = i;
    gCellPressedUntil = osGetTime() + 150;
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

        // Touch: a tap (lifted within 6 px of where it went down) presses the section
        // button there; a touch that moves further drags the text (rule 13), with a little
        // momentum after the stylus is lifted.
        static int tx0, ty0, tyLast, tState;   // tState: 0 none, 1 maybe a tap, 2 dragging
        static float mom;
        float& scroll = gScroll[gCurrent];
        if (down & KEY_TOUCH) {
            touchPosition touch;
            hidTouchRead(&touch);
            tx0 = touch.px; ty0 = touch.py; tyLast = touch.py; tState = 1; mom = 0;
        } else if (tState && (held & KEY_TOUCH)) {
            touchPosition touch;
            hidTouchRead(&touch);
            if (tState == 1 && (abs(touch.px - tx0) > 6 || abs(touch.py - ty0) > 6)) tState = 2;
            if (tState == 2) { scroll -= touch.py - tyLast; mom = (float)(tyLast - touch.py); }
            tyLast = touch.py;
        } else if (tState) {
            const int was = tState;
            tState = 0;
            if (was == 1) {
                bool close = false;
                const int n = (int)gSections.size();
                for (int i = 0; i <= n + 1; i++) {
                    float x, y, w;
                    cellRect(i, &x, &y, &w);
                    // the 4-px gaps and 6-px margins count toward the nearest cell
                    if (tx0 >= x - 2 && tx0 < x + w + 2 && ty0 >= y - 2 && ty0 < y + kCellH + 2) {
                        pressCell(i);   // the house click and a pressed look
                        if (i == n + 1) close = true;
                        else if (i == n) { ctr_lefty_toggle(); gFocus = n; }
                        else select(i);
                        break;
                    }
                }
                if (close) {
                    break;
                }
            }
        } else if (mom > 0.5f || mom < -0.5f) {
            scroll += mom;
            mom *= 0.88f;
        }

        circlePosition cp;
        hidCircleRead(&cp);
        circlePosition cs;
        hidCstickRead(&cs);
        int dy = abs(cp.dy) > 20 ? cp.dy : (abs(cs.dy) > 20 ? cs.dy : 0);
        gScroll[gCurrent] -= dy / 20.0f;
        // D-pad moves between the section buttons (it does not scroll):
        // up/down within a column, left/right to the other column.
        // Below the last section sit LEFT HAND and CLOSE; A presses those two.
        {
            int n = (int)gSections.size();
            int rows = rowsPerColumn();
            int f = gFocus;
            if (down & KEY_DUP) f = f >= n ? n - 1 : f - 1;
            if (down & KEY_DDOWN) f = f >= n ? f : f + 1;
            if (down & KEY_DLEFT) f = f == n + 1 ? n : (f == n ? n - rows : f - rows);
            if (down & KEY_DRIGHT) f = f == n ? n + 1 : (f > n || f + rows > n ? f : f + rows);
            if (f != gFocus && f >= 0 && f <= n + 1) {
                if (f < n) select(f);
                else gFocus = f;
            }
            if (down & KEY_A) {
                if (gFocus == n) { pressCell(n); ctr_lefty_toggle(); }
                else if (gFocus == n + 1) { pressCell(n + 1); break; }
            }
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
static const char* const kLeftyPath = "sdmc:/3ds/fallout/.lefty";

static void leftyLoad()
{
    FILE* f = fopen(kLeftyPath, "r");
    gLefty = f != nullptr;
    if (f) fclose(f);
    gLeftyLoaded = true;
}

// FALLOUT-Q6: the marker write is checked. When the card refuses it the setting still holds
// for this session and the menu / manual say that it could not be saved.
static void leftySet(bool on)
{
    gLefty = on;
    if (on) {
        FILE* f = fopen(kLeftyPath, "w");
        gLeftySaveFailed = f == nullptr || fclose(f) != 0;
    } else {
        remove(kLeftyPath);
        FILE* f = fopen(kLeftyPath, "r");   // still there: the card refused
        gLeftySaveFailed = f != nullptr;
        if (f) fclose(f);
    }
}

void ctr_lefty_toggle()
{
    if (!gLeftyLoaded) leftyLoad();
    leftySet(!gLefty);
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

// ---------------------------------------------------------------------------
// House-style button pages (claude/3ds-look-and-feel.md): the START menu, the ACTIONS page
// and the YES / NO question share one loop. Every button works by touch (click, pressed
// look, acts on release) and by D-pad + A (gold ring); greyed buttons are inert and skipped;
// B or START backs out.
// ---------------------------------------------------------------------------
struct PageButton {
    float x, y, w, h;
    std::string label;
    bool usable;
    int code;
};
std::vector<PageButton> gPage;
int gPageSel = 0;
int gPagePressed = -1;
void (*gPageTop)() = nullptr;
// Text above the buttons on the touch screen (the question of a YES / NO page).
std::string gPageHead, gPageSub;

void pageDrawBottom()
{
    const u32 face = C2D_Color32(40, 56, 120, 0xff), edge = C2D_Color32(120, 150, 230, 0xff), press = C2D_Color32(220, 180, 60, 0xff);
    const u32 shade = C2D_Color32(20, 30, 70, 0xff), gold = C2D_Color32(255, 204, 64, 0xff), white = C2D_Color32(240, 240, 240, 0xff);
    C2D_DrawRectSolid(0, 0, 0.4f, 320, 240, kBg);
    if (!gPageHead.empty()) {
        float sc = 0.7f, w = measure(gPageHead) * sc / kTextScale;
        if (w > 304) sc *= 304 / w;
        text(gPageHead, 160, 22, sc, white, C2D_AlignCenter);
    }
    if (!gPageSub.empty()) {
        float sc = 0.5f, w = measure(gPageSub) * sc / kTextScale;
        if (w > 304) sc *= 304 / w;   // never cut off
        text(gPageSub, 160, 58, sc, C2D_Color32(150, 150, 165, 0xff), C2D_AlignCenter);
    }
    for (int i = 0; i < (int)gPage.size(); i++) {
        const PageButton& b = gPage[i];
        const bool on = i == gPageSel, pressed = i == gPagePressed;
        float sc = 0.5f, tw = measure(b.label) * sc / kTextScale;
        if (tw > b.w - 12) sc *= (b.w - 12) / tw;   // never cut off
        if (!b.usable) {   // greyed out: flat, dim, no focus ring
            C2D_DrawRectSolid(b.x, b.y, 0.45f, b.w, b.h, C2D_Color32(60, 66, 100, 0xff));
            C2D_DrawRectSolid(b.x + 1, b.y + 1, 0.46f, b.w - 2, b.h - 2, C2D_Color32(28, 34, 66, 0xff));
            text(b.label, b.x + b.w / 2, b.y + b.h / 2 - 7, sc, C2D_Color32(110, 114, 135, 0xff), C2D_AlignCenter);
            continue;
        }
        if (on) C2D_DrawRectSolid(b.x - 2, b.y - 2, 0.44f, b.w + 4, b.h + 4, gold);   // focus ring
        C2D_DrawRectSolid(b.x, b.y, 0.45f, b.w, b.h, edge);
        C2D_DrawRectSolid(b.x + 1, b.y + 1, 0.46f, b.w - 2, b.h - 2, pressed ? press : face);
        if (!pressed) C2D_DrawRectSolid(b.x + 1, b.y + b.h - 2, 0.47f, b.w - 2, 1, shade);
        text(b.label, b.x + b.w / 2, b.y + b.h / 2 - 7 + (pressed ? 1 : 0), sc,
            pressed ? C2D_Color32(0, 0, 0, 0xff) : (on ? gold : white), C2D_AlignCenter);
    }
}

// The nearest usable button in a direction, or the same one.
int pageMove(int from, int dx, int dy)
{
    const PageButton& f = gPage[from];
    const float cx = f.x + f.w / 2, cy = f.y + f.h / 2;
    int best = -1;
    float bestD = 1e9f;
    for (int i = 0; i < (int)gPage.size(); i++) {
        if (i == from || !gPage[i].usable) continue;
        const float x = gPage[i].x + gPage[i].w / 2, y = gPage[i].y + gPage[i].h / 2;
        const float along = dx ? (x - cx) * dx : (y - cy) * dy;
        const float across = dx ? fabsf(y - cy) : fabsf(x - cx);
        if (along < 2) continue;
        const float d = along * 2 + across * 5;
        if (d < bestD) { bestD = d; best = i; }
    }
    if (best >= 0) return best;
    // nothing further that way: wrap round to the far end (up from RESUME lands on EXIT GAME)
    float far = 0;
    for (int i = 0; i < (int)gPage.size(); i++) {
        if (i == from || !gPage[i].usable) continue;
        const float x = gPage[i].x + gPage[i].w / 2, y = gPage[i].y + gPage[i].h / 2;
        const float along = dx ? (x - cx) * dx : (y - cy) * dy;
        const float across = dx ? fabsf(y - cy) : fabsf(x - cx);
        if (across > 8 || along > -2) continue;   // same row or column only
        if (-along > far) { far = -along; best = i; }
    }
    return best >= 0 ? best : from;
}

// Runs the page in gPage until a button is chosen: returns its code, or `cancel` for B / START.
int pageRun(int cancel)
{
    if (gBuf == nullptr) {
        gBuf = C2D_TextBufNew(4096);
        gMeasureBuf = C2D_TextBufNew(512);
    }
    gPagePressed = -1;
    if (gPageSel < 0 || gPageSel >= (int)gPage.size() || !gPage[gPageSel].usable) {
        gPageSel = 0;
        while (gPageSel < (int)gPage.size() - 1 && !gPage[gPageSel].usable) gPageSel++;
    }
    int result = cancel;
    bool waitRelease = true;
    while (aptMainLoop()) {
        hidScanInput();
        u32 down = ctr_remap(hidKeysDown());
        u32 held = ctr_remap(hidKeysHeld());
        if (waitRelease) {
            if ((held & (KEY_START | KEY_SELECT | KEY_TOUCH | KEY_A | KEY_B | KEY_X | KEY_Y)) == 0) {
                waitRelease = false;
            }
        } else {
            if (down & (KEY_B | KEY_START)) {
                click3ds::play();
                result = cancel;
                break;
            }
            if (down & KEY_DUP) gPageSel = pageMove(gPageSel, 0, -1);
            if (down & KEY_DDOWN) gPageSel = pageMove(gPageSel, 0, 1);
            if (down & KEY_DLEFT) gPageSel = pageMove(gPageSel, -1, 0);
            if (down & KEY_DRIGHT) gPageSel = pageMove(gPageSel, 1, 0);
            if ((down & KEY_A) && gPage[gPageSel].usable) {
                click3ds::play();
                // show the pressed look for a moment (the same press a tap gives)
                gPagePressed = gPageSel;
                for (int f = 0; f < 6 && aptMainLoop(); f++) {
                    C2D_TextBufClear(gBuf);
                    ctr_gfx_manual_frame(gPageTop, pageDrawBottom);
                }
                result = gPage[gPageSel].code;
                break;
            }
            if (held & KEY_TOUCH) {
                touchPosition t;
                hidTouchRead(&t);
                const int before = gPagePressed;
                gPagePressed = -1;
                for (int i = 0; i < (int)gPage.size(); i++) {
                    const PageButton& b = gPage[i];
                    if (b.usable && t.px >= b.x && t.px < b.x + b.w && t.py >= b.y && t.py < b.y + b.h) {
                        gPagePressed = i;
                        gPageSel = i;
                    }
                }
                if (gPagePressed >= 0 && before < 0) click3ds::play();   // FO-10: the house click on press
            } else if (gPagePressed >= 0) {
                result = gPage[gPagePressed].code;
                break;
            }
        }
        C2D_TextBufClear(gBuf);
        ctr_gfx_manual_frame(gPageTop, pageDrawBottom);
    }
    // Don't let the closing press reach the game.
    gPagePressed = -1;
    while (aptMainLoop()) {
        hidScanInput();
        if (hidKeysHeld() == 0) break;
        C2D_TextBufClear(gBuf);
        ctr_gfx_manual_frame(gPageTop, pageDrawBottom);
    }
    return result;
}

void pageHeader(const char* right)
{
    const u32 face = C2D_Color32(40, 56, 120, 0xff), gold = C2D_Color32(255, 204, 64, 0xff), grey = C2D_Color32(150, 150, 165, 0xff);
    C2D_DrawRectSolid(0, 0, 0.4f, 400, 240, kBg);
    C2D_DrawRectSolid(0, 0, 0.41f, 400, 22, face);
    text("FALLOUT", 8, 4, 0.5f, gold);
    text(right, 392, 4, 0.5f, grey, C2D_AlignRight);
    C2D_DrawRectSolid(0, 224, 0.41f, 400, 16, C2D_Color32(20, 30, 70, 0xff));
    text(gLefty ? "ABXY: choose    D-Rt: select    D-Dn or START: close" : "D-pad: choose    A: select    B or START: close",
        200, 226, 0.42f, gold, C2D_AlignCenter);
}

// A line that is shrunk to the width it has, never cut off.
void fitText(const std::string& s, float x, float y, float scale, u32 color, float maxW)
{
    float w = measure(s) * scale / kTextScale;
    if (w > maxW) scale *= maxW / w;
    text(s, x, y, scale, color);
}

// ---- START menu -----------------------------------------------------------
const int kMenuCount = 8;
// screen order (house order, cross-port pass); the codes are the CTR_MENU_* values of ctr_manual.h
const int kMenuOrder[kMenuCount] = { CTR_MENU_RESUME, CTR_MENU_MANUAL, CTR_MENU_SAVE, CTR_MENU_LOAD, CTR_MENU_SETTINGS, CTR_MENU_ACTIONS, CTR_MENU_LEFTY, CTR_MENU_EXIT };
int gMenuWhere = 0;   // CTR_EXIT_* of ctr_exit_kind(): what EXIT GAME can do here

const char* menuLabel(int code)
{
    switch (code) {
    case CTR_MENU_RESUME: return "RESUME";
    case CTR_MENU_MANUAL: return "MANUAL";
    case CTR_MENU_SAVE: return "SAVE GAME";
    case CTR_MENU_LOAD: return "LOAD GAME";
    case CTR_MENU_ACTIONS: return "ACTIONS";
    case CTR_MENU_SETTINGS: return "SETTINGS";
    case CTR_MENU_LEFTY: return gLefty ? "LEFT HAND: ON" : "LEFT HAND: OFF";
    default: return "EXIT GAME";
    }
}

// FO-11: SAVE, ACTIONS and SETTINGS only make sense during play on the map; elsewhere (main
// menu, world map, over a window) they are greyed out and inert.
bool menuUsable(int code)
{
    if (code == CTR_MENU_SAVE || code == CTR_MENU_LOAD || code == CTR_MENU_SETTINGS || code == CTR_MENU_ACTIONS) return ctr_exit_saves();
    return true;
}

void menuDrawTop()
{
    const u32 gold = C2D_Color32(255, 204, 64, 0xff), white = C2D_Color32(240, 240, 240, 0xff), grey = C2D_Color32(150, 150, 165, 0xff);
    pageHeader("PAUSED");
    for (int i = 0; i < kMenuCount; i++) {
        const int code = kMenuOrder[i];
        const bool usable = menuUsable(code);
        const char* help = "";
        switch (code) {
        case CTR_MENU_RESUME: help = "back to the game"; break;
        case CTR_MENU_MANUAL: help = "how to play on the 3DS"; break;
        case CTR_MENU_SAVE: help = "save your game (slots 1 to 9)"; break;
        case CTR_MENU_LOAD: help = "load a saved game"; break;
        case CTR_MENU_ACTIONS: help = "every shortcut as a button"; break;
        case CTR_MENU_SETTINGS: help = "the game's Preferences"; break;
        case CTR_MENU_LEFTY: help = gLeftySaveFailed ? "could not save this (SD card?)" : "swap the D-pad and A/B/X/Y"; break;
        default:
            help = gMenuWhere == CTR_EXIT_SAVES ? "asks, saves to slot 10, quits"
                 : gMenuWhere == CTR_EXIT_CLOSES ? "asks, closes this window, saves"
                 : gMenuWhere == CTR_EXIT_NOGAME ? "asks, back to the Homebrew Menu"
                                                 : "asks; cannot save on this screen";
            break;
        }
        text(menuLabel(code), 16, 30 + i * 22, 0.5f, !usable ? grey : (i == gPageSel ? gold : white));   // eight rows end above the build line
        fitText(!usable ? "only during play on the map" : help, 170, 31 + i * 22, 0.45f, grey, 222);
    }
    text("Build " PORT_VERSION, 8, 208, 0.4f, grey);
}

// ---- ACTIONS --------------------------------------------------------------
struct ActionDef {
    int code;
    const char* label;
    const char* chord;       // the button shortcut, right-handed
    const char* chordLefty;  // the same with the D-pad and A/B/X/Y swapped
};
const ActionDef kActions[] = {
    { CTR_ACT_AUTOMAP, "AUTOMAP", "Select+X", "Select+Left" },
    { CTR_ACT_REST, "REST / WAIT", "Select+Y", "Select+Up" },
    { CTR_ACT_HEAL, "QUICK HEAL", "Select+A", "Select+Right" },
    { CTR_ACT_RELOAD, "RELOAD", "Select+B", "Select+Down" },
    { CTR_ACT_QUICKSAVE, "QUICK SAVE", "Start+X", "Start+Left" },
    { CTR_ACT_QUICKLOAD, "QUICK LOAD", "Start+Y", "Start+Up" },
    { CTR_ACT_HANDS, "SWAP HANDS", "D-pad Up", "Y" },
    { CTR_ACT_MODE, "ATTACK MODE", "D-pad Down", "B" },
    { CTR_ACT_SKILLDEX, "SKILLDEX", "D-pad Left", "X" },
    { CTR_ACT_CHAR, "CHARACTER", "D-pad Right", "A" },
    { CTR_ACT_ENDCOMBAT, "END COMBAT", "Start+ZR", "Start+ZR" },
    { CTR_ACT_NONE, "BACK", "B", "D-pad Down" },
};
const int kActionCount = sizeof(kActions) / sizeof(kActions[0]);

void actionsDrawTop()
{
    const u32 gold = C2D_Color32(255, 204, 64, 0xff), white = C2D_Color32(240, 240, 240, 0xff), grey = C2D_Color32(150, 150, 165, 0xff);
    pageHeader("ACTIONS");
    text("Each of these also has a button shortcut:", 16, 26, 0.45f, grey);
    for (int i = 0; i < kActionCount; i++) {
        // two columns of six, as on the touch screen
        const float x = 16 + (i / 6) * 192, y = 46 + (i % 6) * 26;
        text(kActions[i].label, x, y, 0.45f, i == gPageSel ? gold : white);
        text(gLefty ? kActions[i].chordLefty : kActions[i].chord, x, y + 12, 0.4f, grey);
    }
    text("Quick save keeps your own saves: it uses slots 7 to 9.", 16, 206, 0.4f, grey);
}

// ---- YES / NO -------------------------------------------------------------
std::string gAskTop[3];
const char* gAskTitle = "";

void askDrawTop()
{
    const u32 white = C2D_Color32(240, 240, 240, 0xff), grey = C2D_Color32(150, 150, 165, 0xff);
    pageHeader(gAskTitle);
    for (int i = 0; i < 3; i++) {
        if (!gAskTop[i].empty()) fitText(gAskTop[i], 16, 44 + i * 22, 0.5f, i == 0 ? white : grey, 368);
    }
}

} // namespace

int ctr_startmenu_run()
{
    gMenuWhere = ctr_exit_kind();
    gPageSel = 0;
    for (;;) {
        gPage.clear();
        gPageHead.clear();
        gPageSub.clear();
        gPageTop = menuDrawTop;
        // eight rows filling the touch screen: 6 px edges, 4 px gaps
        const float h = (240 - 12 - (kMenuCount - 1) * 4) / (float)kMenuCount;
        for (int i = 0; i < kMenuCount; i++) {
            gPage.push_back({ 6, 6 + i * (h + 4), 308, h, menuLabel(kMenuOrder[i]), menuUsable(kMenuOrder[i]), kMenuOrder[i] });
        }
        const int choice = pageRun(CTR_MENU_RESUME);
        if (choice != CTR_MENU_LEFTY) {
            return choice;
        }
        leftySet(!gLefty);   // flips the setting; the menu stays open
    }
}

int ctr_actions_run()
{
    gPage.clear();
    gPageHead.clear();
    gPageSub.clear();
    gPageTop = actionsDrawTop;
    gPageSel = 0;
    // two columns of six: 6 px edges, 4 px gaps
    const float w = 152, h = (240 - 12 - 5 * 4) / 6.0f;
    for (int i = 0; i < kActionCount; i++) {
        gPage.push_back({ 6 + (i / 6) * (w + 4), 6 + (i % 6) * (h + 4), w, h, kActions[i].label, true, kActions[i].code });
    }
    return pageRun(CTR_ACT_NONE);
}

bool ctr_ask_run(const char* title, const char* question, const char* sub, const char* top1, const char* top2, const char* top3,
    const char* yesLabel)
{
    gPage.clear();
    gPageHead = question;
    gPageSub = sub;
    gAskTitle = title;
    // the question is on the touch screen with its buttons; the top screen holds the detail
    gAskTop[0] = top1 ? top1 : "";
    gAskTop[1] = top2 ? top2 : "";
    gAskTop[2] = top3 ? top3 : "";
    gPageTop = askDrawTop;
    // two big buttons below the question; the ring starts on NO and B means NO
    gPage.push_back({ 6, 100, 152, 134, yesLabel, true, 1 });
    gPage.push_back({ 162, 100, 152, 134, "NO", true, 0 });
    gPageSel = 1;
    return pageRun(0) == 1;
}

} // namespace fallout
