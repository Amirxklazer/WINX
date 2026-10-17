// WinNX v0.3 - Windows 8.1-style tablet shell for Nintendo Switch (SDL2 + libnx)
// Touch-first. Black "bezel" strip at the bottom holds the Windows button (OLED friendly).
// Everything you write (Notes, Word, Excel) autosaves to sdmc:/switch/WinNX/ and reloads next time.
#include <switch.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <SDL2/SDL_image.h>
#include <dirent.h>
#include <sys/stat.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <cctype>
#include <string>
#include <vector>
#include <map>
#include <algorithm>
#include <ctime>

static const int W = 1280, H = 720, UIH = 668; // UIH = screen area above the black bezel strip
static const int KBH = 300, KBTOP = UIH - KBH;

struct Col { Uint8 r, g, b; };
static Col mix(Col a, Col b, float t) {
    return Col{ (Uint8)(a.r + (b.r - a.r) * t), (Uint8)(a.g + (b.g - a.g) * t), (Uint8)(a.b + (b.b - a.b) * t) };
}
static const Col WHITE = {255, 255, 255}, BLACK = {0, 0, 0}, DARK = {30, 30, 30};
static float clampf(float v, float a, float b) { return v < a ? a : (v > b ? b : v); }
static float ease(float t) { t = clampf(t, 0, 1); return 1.0f - (1 - t) * (1 - t) * (1 - t); }
static bool inR(int x, int y, int rx, int ry, int rw, int rh) { return x >= rx && x < rx + rw && y >= ry && y < ry + rh; }

// ---------------------------------------------------------------- globals
static SDL_Window* gWin = nullptr;
static SDL_Renderer* gR = nullptr;
static SDL_Texture* gTarget = nullptr;
static PadState gPad;
static bool gRun = true;

static const int F16 = 0, F22 = 1, F30 = 2, F44 = 3, F72 = 4, F130 = 5, FCOUNT = 6;
static const int FSZ[FCOUNT] = {16, 22, 30, 44, 72, 130};
static TTF_Font* gFont[FCOUNT];
struct TT { SDL_Texture* t; int w, h; };
static std::map<std::string, TT> gTC;

struct Theme { Col top, bottom, accent; };
static const Theme THEMES[4] = {
    {{58, 16, 140}, {28, 4, 88}, {122, 62, 224}},
    {{10, 72, 164}, {4, 28, 92}, {0, 120, 215}},
    {{0, 112, 112}, {0, 48, 60}, {0, 150, 140}},
    {{150, 20, 62}, {68, 4, 34}, {200, 40, 70}},
};
static int gTheme = 0;
static bool gBurn = true, g24 = true;
static int gDocFont = F22;
static bool gBold = false;
static int gEngine = 0; // CMrhoe search engine: 0 DuckDuckGo, 1 Google, 2 Bing

static std::string gUser = "User";
static u32 gBat = 0;
static const std::string DIR_ = "sdmc:/switch/WinNX/";

struct Now { int y = 2026, mo = 1, d = 1, h = 0, mi = 0, s = 0; };
static Now gNow;

// ---------------------------------------------------------------- drawing helpers
static void rect(int x, int y, int w, int h, Col c, int a = 255) {
    SDL_SetRenderDrawColor(gR, c.r, c.g, c.b, (Uint8)a);
    SDL_Rect r = {x, y, w, h};
    SDL_RenderFillRect(gR, &r);
}
static void disc(int cx, int cy, int r, Col c, int a = 255) {
    SDL_SetRenderDrawColor(gR, c.r, c.g, c.b, (Uint8)a);
    for (int dy = -r; dy <= r; dy++) {
        int dx = (int)sqrtf((float)(r * r - dy * dy));
        SDL_RenderDrawLine(gR, cx - dx, cy + dy, cx + dx, cy + dy);
    }
}
static void ring(int cx, int cy, int r, int t, Col fg, Col bg) { disc(cx, cy, r, fg); disc(cx, cy, r - t, bg); }
static void line(int x1, int y1, int x2, int y2, int t, Col c, int a = 255) {
    SDL_SetRenderDrawColor(gR, c.r, c.g, c.b, (Uint8)a);
    bool steep = abs(y2 - y1) > abs(x2 - x1);
    for (int i = -t / 2; i <= t / 2; i++) {
        if (steep) SDL_RenderDrawLine(gR, x1 + i, y1, x2 + i, y2);
        else SDL_RenderDrawLine(gR, x1, y1 + i, x2, y2 + i);
    }
}
static void tri(float x1, float y1, float x2, float y2, float x3, float y3, Col c, int a = 255) {
    SDL_Vertex v[3];
    SDL_Color sc = {c.r, c.g, c.b, (Uint8)a};
    v[0].position.x = x1; v[0].position.y = y1;
    v[1].position.x = x2; v[1].position.y = y2;
    v[2].position.x = x3; v[2].position.y = y3;
    for (int i = 0; i < 3; i++) { v[i].color = sc; v[i].tex_coord.x = 0; v[i].tex_coord.y = 0; }
    SDL_RenderGeometry(gR, nullptr, v, 3, nullptr, 0);
}
static void outline(int x, int y, int w, int h, int t, Col c) {
    rect(x, y, w, t, c); rect(x, y + h - t, w, t, c);
    rect(x, y, t, h, c); rect(x + w - t, y, t, h, c);
}
static void winLogo(int cx, int cy, int s, Col c, int a = 255) {
    int g = std::max(2, s / 12), p = (s - g) / 2;
    int x0 = cx - s / 2, y0 = cy - s / 2;
    rect(x0, y0, p, p, c, a);
    rect(x0 + p + g, y0, p, p, c, a);
    rect(x0, y0 + p + g, p, p, c, a);
    rect(x0 + p + g, y0 + p + g, p, p, c, a);
}
static int tw(const std::string& s, int f) {
    int w = 0, h = 0;
    if (gFont[f] && !s.empty()) TTF_SizeUTF8(gFont[f], s.c_str(), &w, &h);
    return w;
}
// white text is cached; colour/alpha applied through texture mods. align: 0 left, 1 centre, 2 right
static void text(const std::string& s, int f, int x, int y, Col c, int align = 0, int a = 255) {
    if (s.empty() || !gFont[f]) return;
    std::string key = std::to_string(f) + "|" + s;
    auto it = gTC.find(key);
    if (it == gTC.end()) {
        SDL_Color white = {255, 255, 255, 255};
        SDL_Surface* sf = TTF_RenderUTF8_Blended(gFont[f], s.c_str(), white);
        if (!sf) return;
        TT tt;
        tt.t = SDL_CreateTextureFromSurface(gR, sf);
        tt.w = sf->w; tt.h = sf->h;
        SDL_FreeSurface(sf);
        if (!tt.t) return;
        SDL_SetTextureBlendMode(tt.t, SDL_BLENDMODE_BLEND);
        it = gTC.emplace(key, tt).first;
    }
    TT& tt = it->second;
    int dx = x;
    if (align == 1) dx = x - tt.w / 2;
    else if (align == 2) dx = x - tt.w;
    SDL_SetTextureColorMod(tt.t, c.r, c.g, c.b);
    SDL_SetTextureAlphaMod(tt.t, (Uint8)a);
    SDL_Rect d = {dx, y, tt.w, tt.h};
    SDL_RenderCopy(gR, tt.t, nullptr, &d);
}
// styled text: style bits 1 bold, 2 italic, 4 underline (SDL_ttf synthesises them)
static int twS(const std::string& s, int f, int style) {
    int w = 0, h = 0;
    if (gFont[f] && !s.empty()) {
        TTF_SetFontStyle(gFont[f], style);
        TTF_SizeUTF8(gFont[f], s.c_str(), &w, &h);
        TTF_SetFontStyle(gFont[f], 0);
    }
    return w;
}
static void textS(const std::string& s, int f, int x, int y, Col c, int style, int align = 0, int a = 255) {
    if (style == 0) { text(s, f, x, y, c, align, a); return; }
    if (s.empty() || !gFont[f]) return;
    std::string key = "S" + std::to_string(style) + "|" + std::to_string(f) + "|" + s;
    auto it = gTC.find(key);
    if (it == gTC.end()) {
        SDL_Color white = {255, 255, 255, 255};
        TTF_SetFontStyle(gFont[f], style);
        SDL_Surface* sf = TTF_RenderUTF8_Blended(gFont[f], s.c_str(), white);
        TTF_SetFontStyle(gFont[f], 0);
        if (!sf) return;
        TT tt;
        tt.t = SDL_CreateTextureFromSurface(gR, sf);
        tt.w = sf->w; tt.h = sf->h;
        SDL_FreeSurface(sf);
        if (!tt.t) return;
        SDL_SetTextureBlendMode(tt.t, SDL_BLENDMODE_BLEND);
        it = gTC.emplace(key, tt).first;
    }
    TT& tt = it->second;
    int dx = x;
    if (align == 1) dx = x - tt.w / 2;
    else if (align == 2) dx = x - tt.w;
    SDL_SetTextureColorMod(tt.t, c.r, c.g, c.b);
    SDL_SetTextureAlphaMod(tt.t, (Uint8)a);
    SDL_Rect d = {dx, y, tt.w, tt.h};
    SDL_RenderCopy(gR, tt.t, nullptr, &d);
}
static void btn(int x, int y, int w, int h, const char* l, Col bg, Col fg) {
    rect(x, y, w, h, bg);
    text(l, F22, x + w / 2, y + h / 2 - 15, fg, 1);
}

// ---------------------------------------------------------------- time helpers
static const char* MONTHS[12] = {"January", "February", "March", "April", "May", "June", "July", "August", "September", "October", "November", "December"};
static const char* WD[7] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};
static const char* WD3[7] = {"Su", "Mo", "Tu", "We", "Th", "Fr", "Sa"};
static int dow(int y, int m, int d) {
    static const int t[12] = {0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4};
    if (m < 3) y -= 1;
    return (y + y / 4 - y / 100 + y / 400 + t[m - 1] + d) % 7;
}
static int daysIn(int y, int m) {
    static const int d[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    if (m == 2 && ((y % 4 == 0 && y % 100 != 0) || y % 400 == 0)) return 29;
    return d[m - 1];
}
static void updateNow() {
    u64 t = 0;
    if (R_SUCCEEDED(timeGetCurrentTime(TimeType_Default, &t))) {
        TimeCalendarTime ct;
        TimeCalendarAdditionalInfo ai;
        if (R_SUCCEEDED(timeToCalendarTimeWithMyRule(t, &ct, &ai))) {
            gNow.y = ct.year; gNow.mo = ct.month; gNow.d = ct.day;
            gNow.h = ct.hour; gNow.mi = ct.minute; gNow.s = ct.second;
            return;
        }
    }
    time_t tt = time(NULL);
    struct tm* lt = localtime(&tt);
    if (lt) {
        gNow.y = lt->tm_year + 1900; gNow.mo = lt->tm_mon + 1; gNow.d = lt->tm_mday;
        gNow.h = lt->tm_hour; gNow.mi = lt->tm_min; gNow.s = lt->tm_sec;
    }
}
static std::string timeStr(bool ampm = false) {
    char b[32];
    if (g24) snprintf(b, sizeof b, "%02d:%02d", gNow.h, gNow.mi);
    else {
        int h = gNow.h % 12; if (h == 0) h = 12;
        if (ampm) snprintf(b, sizeof b, "%d:%02d %s", h, gNow.mi, gNow.h >= 12 ? "PM" : "AM");
        else snprintf(b, sizeof b, "%d:%02d", h, gNow.mi);
    }
    return b;
}
static std::string dateStr() {
    char b[64];
    snprintf(b, sizeof b, "%s, %s %d", WD[dow(gNow.y, gNow.mo, gNow.d)], MONTHS[gNow.mo - 1], gNow.d);
    return b;
}

// ---------------------------------------------------------------- files
static bool readFile(const std::string& p, std::string& out) {
    FILE* f = fopen(p.c_str(), "rb");
    if (!f) return false;
    char buf[4096]; size_t n;
    out.clear();
    while ((n = fread(buf, 1, sizeof buf, f)) > 0) out.append(buf, n);
    fclose(f);
    return true;
}
static bool writeFile(const std::string& p, const std::string& d) {
    FILE* f = fopen(p.c_str(), "wb");
    if (!f) return false;
    fwrite(d.data(), 1, d.size(), f);
    fclose(f);
    return true;
}
static void backupFile(const std::string& p) { // keep the old copy as .bak before "New"
    std::string old;
    if (readFile(p, old) && !old.empty()) writeFile(p + ".bak", old);
}
static std::string lower(std::string s) { for (auto& c : s) c = (char)tolower((unsigned char)c); return s; }
static bool endsWith(const std::string& s, const std::string& e) { return s.size() >= e.size() && s.compare(s.size() - e.size(), e.size(), e) == 0; }

// ---------------------------------------------------------------- tiles / apps
enum { A_MAIL, A_MUSIC, A_PHOTOS, A_DESKTOP, A_CALENDAR, A_STORE, A_CALC, A_NOTES, A_WEATHER, A_CLOCK, A_SETTINGS, A_POWER, A_GAMES, A_WORD, A_EXCEL,
       A_BROWSER, A_PAINT, A_2048, A_MINES, A_SNAKE, A_TODO, A_FILES, A_TASKMGR, A_TIMER, A_COUNT };

// Apps that can be installed from the Store (all built in; "installing" adds a Start tile)
struct SApp { int app; const char* name; const char* cat; const char* d1; const char* d2; Col col; int w; const char* glyph; int stars; const char* rate; };
static const SApp STORE[] = {
    {A_BROWSER, "CMrhoe", "Internet", "Amir's browser: search the web or type an address.", "Bookmarks, history and three search engines (DuckDuckGo, Google, Bing). Pages open in the Switch web engine, so you need Wi-Fi.", {219, 68, 55}, 2, "C", 5, "2,431"},
    {A_WORD, "Word 2016", "Productivity", "Write letters and documents.", "Autosaves to your SD card. Bold, text size, word count.", {43, 87, 154}, 2, "W", 5, "9,812"},
    {A_PAINT, "Paint", "Art", "Draw with your finger.", "Brush, eraser, fill, line, rectangle and oval tools, 16 colours, undo. Save pictures you can open in Photos or put into Word.", {200, 70, 160}, 1, "P", 4, "5,320"},
    {A_EXCEL, "Excel 2016", "Productivity", "Spreadsheets with formulas.", "SUM, AVERAGE, MIN, MAX, COUNT and cell references like =A1+B2. Autosaves.", {33, 115, 70}, 2, "X", 5, "7,104"},
};
static const int NSTORE = (int)(sizeof(STORE) / sizeof(STORE[0]));
static bool gInst[A_COUNT];
static const char* CATS[5] = {"Games", "Productivity", "Internet", "Art", "Tools"};

struct Tile { int col, row, w, h; Col color; const char* label; int app; };
static const Tile BASE[] = {
    {0, 0, 2, 1, {30, 144, 224}, "Mail", A_MAIL},
    {0, 1, 1, 1, {222, 89, 26}, "Music", A_MUSIC},
    {1, 1, 1, 1, {0, 150, 160}, "Photos", A_PHOTOS},
    {0, 2, 1, 1, {236, 150, 10}, "Desktop", A_DESKTOP},
    {1, 2, 1, 1, {110, 70, 215}, "Calendar", A_CALENDAR},
    {2, 0, 2, 2, {16, 160, 50}, "Store", A_STORE},
    {2, 2, 1, 1, {60, 140, 70}, "Calculator", A_CALC},
    {3, 2, 1, 1, {200, 35, 75}, "Notes", A_NOTES},
    {4, 0, 2, 1, {50, 150, 230}, "Weather", A_WEATHER},
    {4, 1, 2, 2, {0, 110, 110}, "Clock", A_CLOCK},
    {6, 0, 1, 1, {90, 90, 100}, "Settings", A_SETTINGS},
    {7, 0, 1, 1, {200, 50, 50}, "Power", A_POWER},
    {6, 1, 2, 2, {130, 50, 160}, "Games", A_GAMES},
};
static std::vector<Tile> gTiles;
static const int U = 156, G = 8, LEFT = 90, TOP = 140;
static float scrollX = 0, scrollV = 0, maxScroll = 0;
static int sel = 0;

static int groupExtra(int col) { return 36 * (col / 2); }
static SDL_Rect tileBase(int i) {
    const Tile& t = gTiles[i];
    SDL_Rect r;
    r.x = LEFT + t.col * (U + G) + groupExtra(t.col);
    r.y = TOP + t.row * (U + G);
    r.w = t.w * U + (t.w - 1) * G;
    r.h = t.h * U + (t.h - 1) * G;
    return r;
}
static void rebuildTiles() {
    gTiles.assign(BASE, BASE + sizeof(BASE) / sizeof(BASE[0]));
    // installed Store apps are packed into extra groups (2 columns x 3 rows each) to the right
    std::vector<char> occ; // 6 cells per group
    for (int i = 0; i < NSTORE; i++) {
        const SApp& a = STORE[i];
        if (!gInst[a.app]) continue;
        bool placed = false;
        for (int g = 0; !placed; g++) {
            if ((int)occ.size() < 6 * (g + 1)) occ.resize(6 * (g + 1), 0);
            for (int r = 0; r < 3 && !placed; r++)
                for (int c = 0; c + a.w <= 2 && !placed; c++) {
                    bool ok = true;
                    for (int k = 0; k < a.w; k++) if (occ[6 * g + r * 2 + c + k]) ok = false;
                    if (!ok) continue;
                    for (int k = 0; k < a.w; k++) occ[6 * g + r * 2 + c + k] = 1;
                    gTiles.push_back(Tile{8 + 2 * g + c, r, a.w, 1, a.col, a.name, a.app});
                    placed = true;
                }
        }
    }
    int m = 0;
    for (int i = 0; i < (int)gTiles.size(); i++) { SDL_Rect r = tileBase(i); m = std::max(m, r.x + r.w); }
    maxScroll = std::max(0.0f, (float)(m + 90 - W));
    scrollX = clampf(scrollX, 0, maxScroll);
    if (sel >= (int)gTiles.size()) sel = 0;
}
static int tileIndexFor(int app) {
    for (int i = 0; i < (int)gTiles.size(); i++) if (gTiles[i].app == app) return i;
    return -1;
}
static std::string appName(int app) {
    for (auto& t : gTiles) if (t.app == app) return t.label;
    return "App";
}

// ---------------------------------------------------------------- state
static int curApp = -1, lastApp = -1, animTile = -1, lastTile = -1, appDir = 0, pendingTile = -1;
static bool gCloseReset = false;
static bool running[A_COUNT];
static float appT = 0, introT = 0, barT = 0, barP = 0;
static bool navMode = false;
static Uint32 lastInput = 0;
static float dimA = 0;

static Col appBg(int app) {
    switch (app) {
        case A_CALC: return Col{31, 31, 31};
        case A_CLOCK: return Col{0, 84, 88};
        case A_SETTINGS: return Col{38, 38, 38};
        case A_POWER: return Col{27, 27, 27};
        case A_DESKTOP: return Col{250, 170, 10};
        case A_PHOTOS: return Col{0, 0, 0};
        case A_WORD: return Col{43, 87, 154};
        case A_EXCEL: return Col{33, 115, 70};
        case A_STORE: return Col{0, 140, 0};
        case A_BROWSER: return Col{219, 68, 55};
        case A_PAINT: return Col{58, 58, 64};
        default: break;
    }
    for (auto& t : gTiles) if (t.app == app) return t.color;
    return Col{40, 40, 40};
}

// ---------------------------------------------------------------- settings persistence
static void saveCfg() {
    std::string b;
    b += (char)0xA8; b += (char)gTheme; b += (char)gBurn; b += (char)g24;
    b += (char)gDocFont; b += (char)gBold; b += (char)gEngine;
    for (int a = 0; a < A_COUNT; a++) b += (char)gInst[a];
    writeFile(DIR_ + "settings.cfg", b);
}
static void loadCfg() {
    std::string s;
    if (!readFile(DIR_ + "settings.cfg", s) || s.size() < 4) return;
    unsigned char m = (unsigned char)s[0];
    if (m == 0xA7) { // v0.2 format
        gTheme = (unsigned char)s[1] % 4; gBurn = s[2] != 0; g24 = s[3] != 0;
        if (s.size() >= 8) {
            gInst[A_WORD] = s[4] != 0; gInst[A_EXCEL] = s[5] != 0;
            gDocFont = std::min(F44, std::max(F16, (int)(unsigned char)s[6]));
            gBold = s[7] != 0;
        }
    } else if (m == 0xA8 && s.size() >= 7) {
        gTheme = (unsigned char)s[1] % 4; gBurn = s[2] != 0; g24 = s[3] != 0;
        gDocFont = std::min(F44, std::max(F16, (int)(unsigned char)s[4]));
        gBold = s[5] != 0; gEngine = (unsigned char)s[6] % 3;
        for (int a = 0; a < A_COUNT && 7 + a < (int)s.size(); a++) gInst[a] = s[7 + a] != 0;
    }
}
static void loadUser() {
    AccountUid uid;
    if (R_FAILED(accountGetPreselectedUser(&uid))) {
        if (R_FAILED(accountGetLastOpenedUser(&uid))) return;
    }
    AccountProfile prof;
    if (R_SUCCEEDED(accountGetProfile(&prof, uid))) {
        AccountProfileBase base;
        memset(&base, 0, sizeof base);
        if (R_SUCCEEDED(accountProfileGet(&prof, NULL, &base)) && base.nickname[0]) gUser = base.nickname;
        accountProfileClose(&prof);
    }
}

// ---------------------------------------------------------------- text editors (Notes, Word)
struct Editor {
    std::string s;
    int scroll = 0;
    bool dirty = false, follow = false;
    std::string cacheS; int cacheF = -1, cacheW = -1;
    std::vector<std::string> lines;
};
static Editor gNote, gDoc;

static std::vector<std::string> wrapText(const std::string& s, int f, int maxW) {
    std::vector<std::string> out;
    std::string line, word;
    auto flushWord = [&]() {
        if (word.empty()) return;
        std::string t = line.empty() ? word : line + " " + word;
        if (line.empty() || tw(t, f) <= maxW) line = t;
        else { out.push_back(line); line = word; }
        word.clear();
    };
    for (char c : s) {
        if (c == '\n') { flushWord(); out.push_back(line); line.clear(); }
        else if (c == ' ') flushWord();
        else {
            word += c;
            if (word.size() >= 8 && tw(word, f) > maxW) {
                std::string head = word.substr(0, word.size() - 1);
                if (!line.empty()) { out.push_back(line); line.clear(); }
                out.push_back(head);
                word = std::string(1, c);
            }
        }
    }
    flushWord();
    out.push_back(line);
    return out;
}
static const std::vector<std::string>& wrapCached(Editor& e, int f, int w) {
    if (e.cacheF != f || e.cacheW != w || e.cacheS != e.s) {
        e.lines = wrapText(e.s, f, w);
        e.cacheS = e.s; e.cacheF = f; e.cacheW = w;
    }
    return e.lines;
}
static void drawEditor(Editor& e, SDL_Rect a, int f, Col fg, bool bold, bool focused, const char* placeholder) {
    if (!gFont[f]) return;
    const std::vector<std::string>& L = wrapCached(e, f, a.w);
    int lh = TTF_FontLineSkip(gFont[f]);
    int total = (int)L.size() * lh;
    int maxS = std::max(0, total - a.h + lh / 2);
    if (e.follow) { e.scroll = maxS; e.follow = false; }
    e.scroll = std::max(0, std::min(e.scroll, maxS));
    SDL_RenderSetClipRect(gR, &a);
    if (e.s.empty() && placeholder) text(placeholder, f, a.x, a.y, fg, 0, 110);
    for (int i = 0; i < (int)L.size(); i++) {
        int y = a.y + i * lh - e.scroll;
        if (y + lh < a.y || y > a.y + a.h) continue;
        text(L[i], f, a.x, y, fg);
        if (bold) text(L[i], f, a.x + 1, y, fg);
    }
    if (focused && ((SDL_GetTicks() / 500) % 2 == 0)) {
        int y = a.y + ((int)L.size() - 1) * lh - e.scroll;
        rect(a.x + tw(L.back(), f) + 2, y + 3, 2, lh - 6, fg);
    }
    SDL_RenderSetClipRect(gR, nullptr);
}
static int wordCount(const std::string& s) {
    int n = 0; bool in = false;
    for (char c : s) {
        bool sp = (c == ' ' || c == '\n');
        if (!sp && !in) { n++; in = true; } else if (sp) in = false;
    }
    return n;
}

// ---------------------------------------------------------------- Excel engine
static const int XR = 60, XC = 10, XVC = 8, XROWH = 38, XTOP = 172, XHDRW = 50, XCOLW = 150;
static std::string xs[XR][XC];
static int xr = 0, xc = 0, xScrR = 0, xScrC = 0;
static std::string xEdit;
static bool xEditing = false, xDirty = false;

static bool cellVal(int r, int c, int depth, double& v, bool& err);
struct XP {
    std::string s; size_t p; int depth; bool ok;
    void ws() { while (p < s.size() && s[p] == ' ') p++; }
    bool ref(int& r, int& c) {
        size_t q = p;
        if (q < s.size() && isalpha((unsigned char)s[q])) {
            int cc = toupper((unsigned char)s[q]) - 'A';
            q++;
            if (cc < 0 || cc >= XC) return false;
            size_t st = q; int rr = 0;
            while (q < s.size() && isdigit((unsigned char)s[q])) { rr = rr * 10 + (s[q] - '0'); q++; }
            if (q == st || rr < 1 || rr > XR) return false;
            r = rr - 1; c = cc; p = q;
            return true;
        }
        return false;
    }
    double expr() {
        double v = term();
        for (;;) {
            ws();
            if (p < s.size() && (s[p] == '+' || s[p] == '-')) { char o = s[p++]; double t = term(); v = (o == '+') ? v + t : v - t; }
            else break;
        }
        return v;
    }
    double term() {
        double v = fac();
        for (;;) {
            ws();
            if (p < s.size() && (s[p] == '*' || s[p] == '/')) {
                char o = s[p++]; double t = fac();
                if (o == '*') v *= t; else { if (t == 0) ok = false; else v /= t; }
            } else break;
        }
        return v;
    }
    double fac() {
        ws();
        if (p >= s.size()) { ok = false; return 0; }
        char ch = s[p];
        if (ch == '-') { p++; return -fac(); }
        if (ch == '(') {
            p++; double v = expr(); ws();
            if (p < s.size() && s[p] == ')') p++; else ok = false;
            return v;
        }
        if (isdigit((unsigned char)ch) || ch == '.') {
            char* e; double v = strtod(s.c_str() + p, &e);
            p = (size_t)(e - s.c_str());
            return v;
        }
        if (isalpha((unsigned char)ch)) {
            size_t q = p; std::string name;
            while (q < s.size() && isalpha((unsigned char)s[q])) name += (char)toupper((unsigned char)s[q++]);
            if (q < s.size() && s[q] == '(') {
                p = q + 1; ws();
                int r0 = 0, c0 = 0, r1 = 0, c1 = 0;
                if (!ref(r0, c0)) { ok = false; return 0; }
                r1 = r0; c1 = c0; ws();
                if (p < s.size() && s[p] == ':') { p++; ws(); if (!ref(r1, c1)) { ok = false; return 0; } }
                ws();
                if (p < s.size() && s[p] == ')') p++; else { ok = false; return 0; }
                std::vector<double> vals;
                for (int r = std::min(r0, r1); r <= std::max(r0, r1); r++)
                    for (int c = std::min(c0, c1); c <= std::max(c0, c1); c++) {
                        double v = 0; bool er = false;
                        if (cellVal(r, c, depth + 1, v, er)) vals.push_back(v);
                        if (er) ok = false;
                    }
                double sum = 0, mn = 0, mx = 0;
                for (size_t i = 0; i < vals.size(); i++) { sum += vals[i]; if (i == 0 || vals[i] < mn) mn = vals[i]; if (i == 0 || vals[i] > mx) mx = vals[i]; }
                if (name == "SUM") return sum;
                if (name == "AVERAGE" || name == "AVG") { if (vals.empty()) { ok = false; return 0; } return sum / vals.size(); }
                if (name == "MIN") return mn;
                if (name == "MAX") return mx;
                if (name == "COUNT") return (double)vals.size();
                ok = false; return 0;
            }
            int r, c;
            if (ref(r, c)) {
                double v = 0; bool er = false;
                if (cellVal(r, c, depth + 1, v, er)) return v;
                if (er) ok = false;
                return 0;
            }
            ok = false; return 0;
        }
        ok = false; return 0;
    }
};
static bool cellVal(int r, int c, int depth, double& v, bool& err) {
    const std::string& raw = xs[r][c];
    if (raw.empty()) return false;
    if (raw[0] == '=') {
        if (depth > 16) { err = true; return false; }
        XP p; p.s = raw.substr(1); p.p = 0; p.depth = depth; p.ok = true;
        double x = p.expr(); p.ws();
        if (!p.ok || p.p != p.s.size()) { err = true; return false; }
        v = x; return true;
    }
    char* e; const char* st = raw.c_str();
    double x = strtod(st, &e);
    if (e != st && *e == '\0') { v = x; return true; }
    return false;
}
static std::string fmtNum(double v) { char b[40]; snprintf(b, sizeof b, "%.10g", v); return b; }
static std::string cellDisplay(int r, int c) {
    const std::string& raw = xs[r][c];
    if (raw.empty() || raw[0] != '=') return raw;
    double v = 0; bool er = false;
    if (cellVal(r, c, 0, v, er)) return fmtNum(v);
    return "#ERR";
}
static bool looksNumber(const std::string& s) {
    if (s.empty()) return false;
    char* e; const char* st = s.c_str();
    strtod(st, &e);
    return e != st && *e == '\0';
}
static std::string sheetToCsv() {
    int last = -1;
    for (int r = 0; r < XR; r++) for (int c = 0; c < XC; c++) if (!xs[r][c].empty()) last = r;
    std::string out;
    for (int r = 0; r <= last; r++) {
        for (int c = 0; c < XC; c++) {
            const std::string& v = xs[r][c];
            if (c) out += ',';
            if (v.find_first_of(",\"\n") != std::string::npos) {
                out += '"';
                for (char ch : v) { if (ch == '"') out += '"'; out += ch; }
                out += '"';
            } else out += v;
        }
        out += '\n';
    }
    return out;
}
static void csvToSheet(const std::string& d) {
    for (int r = 0; r < XR; r++) for (int c = 0; c < XC; c++) xs[r][c].clear();
    int r = 0, c = 0; bool q = false; std::string cur;
    auto put = [&]() { if (r < XR && c < XC) xs[r][c] = cur; cur.clear(); };
    for (size_t i = 0; i < d.size(); i++) {
        char ch = d[i];
        if (q) {
            if (ch == '"') { if (i + 1 < d.size() && d[i + 1] == '"') { cur += '"'; i++; } else q = false; }
            else cur += ch;
        } else if (ch == '"') q = true;
        else if (ch == ',') { put(); c++; }
        else if (ch == '\n') { put(); c = 0; r++; }
        else if (ch != '\r') cur += ch;
    }
    if (!cur.empty()) put();
}
static int xVisRows() { int bottom = 0; (void)bottom; return 1; }
static bool kbOpen = false;
static std::string gUrl, storeSearch;
static void browserGo();
static void wType(const std::string& t);
static void wBack();
static void appTouch(int phase, int x, int y);
static void appUpdate(float dt);
static int kbTarget = 0; // 1 notes, 2 word, 3 excel cell, 4 browser address, 5 store search
static bool kbShift = false, kbSym = false;
static int xRows() { int bottom = kbOpen ? KBTOP : UIH; return std::max(1, (bottom - XTOP) / XROWH); }
static void xEnsureVisible() {
    int rows = xRows();
    if (xr < xScrR) xScrR = xr;
    if (xr >= xScrR + rows) xScrR = xr - rows + 1;
    if (xc < xScrC) xScrC = xc;
    if (xc >= xScrC + XVC) xScrC = xc - XVC + 1;
    xScrR = std::max(0, std::min(xScrR, XR - 1));
    xScrC = std::max(0, std::min(xScrC, XC - XVC));
}
static void commitCell() {
    if (xEditing) { xs[xr][xc] = xEdit; xEditing = false; xDirty = true; }
}
static void startEdit() { if (!xEditing) { xEdit = xs[xr][xc]; xEditing = true; } }

// ---------------------------------------------------------------- on-screen keyboard
struct Key { SDL_Rect r; std::string label, act; };
static std::vector<Key> gKeys;
static void buildKeys() {
    gKeys.clear();
    const char* r1 = kbSym ? "1234567890" : "qwertyuiop";
    const char* r2 = kbSym ? "-/:;()$&@" : "asdfghjkl";
    const char* r3 = kbSym ? "%+*=#!?" : "zxcvbnm";
    auto add = [&](int x, int y, int w, const std::string& lab, const std::string& act) {
        Key k; k.r.x = x; k.r.y = y; k.r.w = w; k.r.h = 64; k.label = lab; k.act = act; gKeys.push_back(k);
    };
    int y0 = KBTOP + 10;
    for (int i = 0; i < 10; i++) { char c = r1[i]; if (kbShift && !kbSym) c = (char)toupper(c); std::string l(1, c); add(20 + i * 124, y0, 116, l, l); }
    for (int i = 0; i < 9; i++) { char c = r2[i]; if (kbShift && !kbSym) c = (char)toupper(c); std::string l(1, c); add(82 + i * 124, y0 + 72, 116, l, l); }
    add(20, y0 + 144, 160, "Shift", "SHIFT");
    for (int i = 0; i < 7; i++) { char c = r3[i]; if (kbShift && !kbSym) c = (char)toupper(c); std::string l(1, c); add(188 + i * 124, y0 + 144, 116, l, l); }
    add(1056, y0 + 144, 204, "Del", "BS");
    add(20, y0 + 216, 160, kbSym ? "abc" : "123", "SYM");
    add(188, y0 + 216, 100, ",", ",");
    add(296, y0 + 216, 520, "", " ");
    add(824, y0 + 216, 100, ".", ".");
    add(932, y0 + 216, 200, "Enter", "ENTER");
    add(1140, y0 + 216, 120, "Hide", "HIDE");
}
static int kbHit(int x, int y) {
    for (int i = 0; i < (int)gKeys.size(); i++) if (inR(x, y, gKeys[i].r.x, gKeys[i].r.y, gKeys[i].r.w, gKeys[i].r.h + 8)) return i;
    return -1;
}
static void kbType(const std::string& s) {
    if (kbTarget == 1) { gNote.s += s; gNote.dirty = true; gNote.follow = true; }
    else if (kbTarget == 2) wType(s);
    else if (kbTarget == 3) { if (!xEditing) { xEdit.clear(); xEditing = true; } xEdit += s; }
    else if (kbTarget == 4) gUrl += s;
    else if (kbTarget == 5) storeSearch += s;
}
static void kbBackspace() {
    if (kbTarget == 1) { if (!gNote.s.empty()) { gNote.s.pop_back(); gNote.dirty = true; gNote.follow = true; } }
    else if (kbTarget == 2) wBack();
    else if (kbTarget == 3) { startEdit(); if (!xEdit.empty()) xEdit.pop_back(); }
    else if (kbTarget == 4) { if (!gUrl.empty()) gUrl.pop_back(); }
    else if (kbTarget == 5) { if (!storeSearch.empty()) storeSearch.pop_back(); }
}
static void openKb(int target) { kbOpen = true; kbTarget = target; kbShift = false; kbSym = false; }
static void hideKb() { if (kbTarget == 3) commitCell(); kbOpen = false; }
static void kbAction(const std::string& a) {
    if (a == "SHIFT") kbShift = !kbShift;
    else if (a == "SYM") kbSym = !kbSym;
    else if (a == "BS") kbBackspace();
    else if (a == "ENTER") {
        if (kbTarget == 3) { commitCell(); xr = std::min(xr + 1, XR - 1); xEnsureVisible(); }
        else if (kbTarget == 4) browserGo();
        else if (kbTarget == 5) hideKb();
        else kbType("\n");
    }
    else if (a == "HIDE") hideKb();
    else { kbType(a); if (kbShift && !kbSym) kbShift = false; }
}
static void drawKb() {
    rect(0, KBTOP, W, KBH, Col{30, 30, 34});
    buildKeys();
    for (auto& k : gKeys) {
        bool special = k.act.size() > 1;
        Col bg = special ? Col{52, 52, 58} : Col{68, 68, 74};
        if (k.act == "SHIFT" && kbShift) bg = THEMES[gTheme].accent;
        if (k.act == "ENTER") bg = THEMES[gTheme].accent;
        rect(k.r.x, k.r.y, k.r.w, k.r.h, bg);
        text(k.label, special ? F22 : F30, k.r.x + k.r.w / 2, k.r.y + (special ? 18 : 14), WHITE, 1);
    }
}

// ---------------------------------------------------------------- photos and games
static std::vector<std::string> gPhotos;
static int phIdx = 0, phW = 0, phH = 0;
static SDL_Texture* phTex = nullptr;
static bool isImg(const std::string& n) {
    std::string l = lower(n);
    return endsWith(l, ".jpg") || endsWith(l, ".jpeg") || endsWith(l, ".png") || endsWith(l, ".bmp");
}
static void scanDir(const std::string& dir, int depth, bool nro, std::vector<std::string>& out) {
    DIR* d = opendir(dir.c_str());
    if (!d) return;
    struct dirent* e;
    while ((e = readdir(d)) != nullptr) {
        std::string n = e->d_name;
        if (n == "." || n == "..") continue;
        std::string p = dir + "/" + n;
        struct stat st;
        if (stat(p.c_str(), &st) != 0) continue;
        if (S_ISDIR(st.st_mode)) { if (depth > 0) scanDir(p, depth - 1, nro, out); }
        else if (nro ? (endsWith(lower(n), ".nro") && n != "WinNX.nro") : isImg(n)) out.push_back(p);
        if (out.size() > 3000) break;
    }
    closedir(d);
}
static void loadPhoto() {
    if (phTex) { SDL_DestroyTexture(phTex); phTex = nullptr; }
    if (gPhotos.empty()) return;
    SDL_Surface* sf = IMG_Load(gPhotos[phIdx].c_str());
    if (!sf) return;
    phW = sf->w; phH = sf->h;
    phTex = SDL_CreateTextureFromSurface(gR, sf);
    SDL_FreeSurface(sf);
}
static void photoNav(int d) {
    if (gPhotos.empty()) return;
    phIdx = (phIdx + d + (int)gPhotos.size()) % (int)gPhotos.size();
    loadPhoto();
}
struct Game { std::string name, path; };
static std::vector<Game> gGames;
static int gamesScroll = 0;
static void scanGames() {
    std::vector<std::string> files;
    scanDir("sdmc:/switch", 2, true, files);
    gGames.clear();
    for (auto& p : files) {
        size_t s = p.find_last_of('/');
        std::string n = p.substr(s + 1);
        n = n.substr(0, n.size() - 4);
        gGames.push_back(Game{n, p});
    }
    std::sort(gGames.begin(), gGames.end(), [](const Game& a, const Game& b) { return lower(a.name) < lower(b.name); });
}

// ---------------------------------------------------------------- calculator
static std::string cCur = "0";
static double cAcc = 0;
static int cOp = 0; // 1 + 2 - 3 x 4 /
static bool cFresh = true, cErr = false;
struct CBtn { const char* l; int c, r, cw, rh; };
static const CBtn CBTNS[] = {
    {"C", 0, 0, 1, 1}, {"DEL", 1, 0, 1, 1}, {"\xC3\xB7", 2, 0, 1, 1}, {"\xC3\x97", 3, 0, 1, 1},
    {"7", 0, 1, 1, 1}, {"8", 1, 1, 1, 1}, {"9", 2, 1, 1, 1}, {"-", 3, 1, 1, 1},
    {"4", 0, 2, 1, 1}, {"5", 1, 2, 1, 1}, {"6", 2, 2, 1, 1}, {"+", 3, 2, 1, 1},
    {"1", 0, 3, 1, 1}, {"2", 1, 3, 1, 1}, {"3", 2, 3, 1, 1}, {"=", 3, 3, 1, 2},
    {"0", 0, 4, 2, 1}, {".", 2, 4, 1, 1},
};
static const int NCB = (int)(sizeof(CBTNS) / sizeof(CBTNS[0]));
static SDL_Rect calcRect(const CBtn& b) {
    SDL_Rect r;
    r.x = 348 + b.c * 146; r.y = 170 + b.r * 94;
    r.w = b.cw * 140 + (b.cw - 1) * 6; r.h = b.rh * 88 + (b.rh - 1) * 6;
    return r;
}
static void calcDo() {
    double b = atof(cCur.c_str()), r = cAcc;
    switch (cOp) {
        case 1: r += b; break;
        case 2: r -= b; break;
        case 3: r *= b; break;
        case 4:
            if (b == 0) { cErr = true; cCur = "Error"; return; }
            r /= b; break;
        default: return;
    }
    cAcc = r;
    char buf[48];
    snprintf(buf, sizeof buf, "%.10g", r);
    cCur = buf;
    if (cCur.size() > 14) { snprintf(buf, sizeof buf, "%.5e", r); cCur = buf; }
}
static void calcPress(const std::string& b) {
    if (cErr) { cCur = "0"; cAcc = 0; cOp = 0; cFresh = true; cErr = false; }
    if (b.size() == 1 && b[0] >= '0' && b[0] <= '9') {
        if (cFresh || cCur == "0") { cCur = b; cFresh = false; }
        else if (cCur.size() < 14) cCur += b;
    } else if (b == ".") {
        if (cFresh) { cCur = "0."; cFresh = false; }
        else if (cCur.find('.') == std::string::npos) cCur += ".";
    } else if (b == "C") {
        cCur = "0"; cAcc = 0; cOp = 0; cFresh = true;
    } else if (b == "DEL") {
        if (!cFresh && cCur.size() > 1) cCur.pop_back(); else { cCur = "0"; cFresh = true; }
        if (cCur == "-") cCur = "0";
    } else if (b == "=") {
        if (cOp) { calcDo(); cOp = 0; }
        cFresh = true;
    } else {
        int n = b == "+" ? 1 : (b == "-" ? 2 : (b == "\xC3\x97" ? 3 : 4));
        if (cOp && !cFresh) calcDo(); else cAcc = atof(cCur.c_str());
        if (!cErr) { cOp = n; cFresh = true; }
    }
}

// ---------------------------------------------------------------- store
static float storeProg[A_COUNT];

// ---------------------------------------------------------------- store / browser state
static int storeView = 0, storeSel = 0, storeCat = -1, storeScroll = 0;
static Uint32 storeStart = 0;
static std::vector<std::string> gHist;
static std::string brMsg;
static Uint32 brMsgT = 0;
static void loadHist() {
    gHist.clear();
    std::string d;
    if (!readFile(DIR_ + "history.txt", d)) return;
    std::string line;
    for (char c : d) { if (c == '\n') { if (!line.empty()) gHist.push_back(line); line.clear(); } else line += c; }
    if (!line.empty()) gHist.push_back(line);
}
static void saveHist() {
    std::string d;
    for (auto& h : gHist) d += h + "\n";
    writeFile(DIR_ + "history.txt", d);
}


// ---------------------------------------------------------------- Paint (state + pixel engine)
static const int PCX = 20, PCY = 170, PCW = 1240, PCH = 488; // canvas rectangle on screen
static std::vector<Uint32> pPix, pBase;
static std::vector<std::vector<Uint32>> pUndo;
static SDL_Texture* pTex = nullptr;
static bool pTexDirty = true, pDirty = false, pDown = false;
static int pTool = 0, pSizeIdx = 2, pCol = 0, pSX = 0, pSY = 0, pLX = 0, pLY = 0;
static const int PSIZES[6] = {2, 4, 8, 14, 22, 32};
static std::string pMsg;
static Uint32 pMsgT = 0;
static const Col PAL[16] = {{0, 0, 0}, {90, 90, 90}, {170, 170, 170}, {255, 255, 255}, {200, 30, 30}, {240, 120, 20}, {250, 210, 30}, {130, 200, 40},
                            {20, 150, 60}, {20, 180, 190}, {40, 110, 220}, {40, 50, 150}, {130, 60, 190}, {230, 80, 170}, {130, 80, 40}, {250, 190, 150}};
static Uint32 packCol(Col c) { return 0xFF000000u | ((Uint32)c.r << 16) | ((Uint32)c.g << 8) | c.b; }
static void pSet(int x, int y, Uint32 v) { if (x >= 0 && y >= 0 && x < PCW && y < PCH) pPix[(size_t)y * PCW + x] = v; }
static void pDisc(int cx, int cy, int r, Uint32 v) {
    for (int dy = -r; dy <= r; dy++) {
        int w = (int)sqrtf((float)(r * r - dy * dy));
        for (int x = cx - w; x <= cx + w; x++) pSet(x, cy + dy, v);
    }
}
static void pLine(int x0, int y0, int x1, int y1, int r, Uint32 v) {
    int n = std::max(abs(x1 - x0), abs(y1 - y0));
    if (n == 0) { pDisc(x0, y0, r, v); return; }
    int step = r > 6 ? std::max(1, r / 4) : 1;
    for (int i = 0; i <= n; i += step) pDisc(x0 + (x1 - x0) * i / n, y0 + (y1 - y0) * i / n, r, v);
    pDisc(x1, y1, r, v);
}
static void pRectO(int x0, int y0, int x1, int y1, int r, Uint32 v) {
    pLine(x0, y0, x1, y0, r, v); pLine(x1, y0, x1, y1, r, v); pLine(x1, y1, x0, y1, r, v); pLine(x0, y1, x0, y0, r, v);
}
static void pOval(int x0, int y0, int x1, int y1, int r, Uint32 v) {
    float cx = (x0 + x1) / 2.0f, cy = (y0 + y1) / 2.0f, rx = fabsf((x1 - x0) / 2.0f), ry = fabsf((y1 - y0) / 2.0f);
    int px = (int)(cx + rx), py = (int)cy;
    for (int i = 1; i <= 96; i++) {
        float a = i * 6.2831853f / 96.0f;
        int nx = (int)(cx + cosf(a) * rx), ny = (int)(cy + sinf(a) * ry);
        pLine(px, py, nx, ny, r, v); px = nx; py = ny;
    }
}
static void pFill(int x, int y, Uint32 nv) {
    if (x < 0 || y < 0 || x >= PCW || y >= PCH) return;
    Uint32 old = pPix[(size_t)y * PCW + x];
    if (old == nv) return;
    std::vector<int> st; st.push_back(y * PCW + x);
    while (!st.empty()) {
        int i = st.back(); st.pop_back();
        int px = i % PCW, py = i / PCW;
        if (pPix[i] != old) continue;
        int l = px, r = px;
        while (l > 0 && pPix[(size_t)py * PCW + l - 1] == old) l--;
        while (r < PCW - 1 && pPix[(size_t)py * PCW + r + 1] == old) r++;
        for (int k = l; k <= r; k++) pPix[(size_t)py * PCW + k] = nv;
        for (int dy = -1; dy <= 1; dy += 2) {
            int ny = py + dy;
            if (ny < 0 || ny >= PCH) continue;
            bool run = false;
            for (int k = l; k <= r; k++) {
                bool m = pPix[(size_t)ny * PCW + k] == old;
                if (m && !run) st.push_back(ny * PCW + k);
                run = m;
            }
        }
    }
}
static void paintEnsure() { if (pPix.empty()) pPix.assign((size_t)PCW * PCH, 0xFFFFFFFFu); }
static void paintPush() {
    paintEnsure();
    pUndo.push_back(pPix);
    if (pUndo.size() > 8) pUndo.erase(pUndo.begin());
}
static void paintSaveFile(const std::string& path) {
    if (pPix.empty()) return;
    SDL_Surface* sf = SDL_CreateRGBSurfaceFrom(pPix.data(), PCW, PCH, 32, PCW * 4, 0x00FF0000, 0x0000FF00, 0x000000FF, 0xFF000000);
    if (sf) { SDL_SaveBMP(sf, path.c_str()); SDL_FreeSurface(sf); }
}
static void paintLoad() {
    pPix.assign((size_t)PCW * PCH, 0xFFFFFFFFu);
    pUndo.clear(); pTexDirty = true; pDirty = false; pDown = false;
    SDL_Surface* sf = IMG_Load((DIR_ + "paint.bmp").c_str());
    if (!sf) return;
    SDL_Surface* c = SDL_ConvertSurfaceFormat(sf, SDL_PIXELFORMAT_ARGB8888, 0);
    if (c) {
        if (c->w == PCW && c->h == PCH) for (int y = 0; y < PCH; y++) memcpy(&pPix[(size_t)y * PCW], (Uint8*)c->pixels + (size_t)y * c->pitch, PCW * 4);
        SDL_FreeSurface(c);
    }
    SDL_FreeSurface(sf);
}
static void paintFree() {
    if (pTex) { SDL_DestroyTexture(pTex); pTex = nullptr; }
    pPix.clear(); pBase.clear(); pUndo.clear();
}
static void paintExport() {
    mkdir((DIR_ + "photos").c_str(), 0777);
    char nm[64]; snprintf(nm, sizeof nm, "paint_%04d%02d%02d_%02d%02d%02d.bmp", gNow.y, gNow.mo, gNow.d, gNow.h, gNow.mi, gNow.s);
    paintSaveFile(DIR_ + "photos/" + nm);
    pMsg = std::string("Saved: switch/WinNX/photos/") + nm; pMsgT = SDL_GetTicks();
}

// ---------------------------------------------------------------- Word (document model)
struct WPara {
    std::string t;
    int style = 0;          // 0 normal, 1 heading 1, 2 heading 2, 3 divider line, 4 picture
    bool b = false, i = false, u = false;
    int align = 0;          // 0 left, 1 centre, 2 right
    int list = 0;           // 0 none, 1 bullets, 2 numbers
    std::string img;        // picture path (style 4)
    std::vector<std::string> lines; std::string cs; int cf = -1, cw = -1; // layout cache
};
static std::vector<WPara> wDoc;
static int wCur = 0, wScroll = 0, wTab = 0, wView = 0, wPicScroll = 0;
static bool wDirty = false, wFollow = false;
static std::vector<std::string> wPics;
struct WImg { SDL_Texture* t; int w, h; };
static std::map<std::string, WImg> wImgs;
static void wEnsure() { if (wDoc.empty()) { wDoc.push_back(WPara()); wCur = 0; } wCur = std::max(0, std::min(wCur, (int)wDoc.size() - 1)); }
static void wFreeImgs() { for (auto& p : wImgs) if (p.second.t) SDL_DestroyTexture(p.second.t); wImgs.clear(); }
static WImg* wImage(const std::string& path) {
    auto it = wImgs.find(path);
    if (it != wImgs.end()) return it->second.t ? &it->second : nullptr;
    if (wImgs.size() >= 8) wFreeImgs();
    WImg wi = {nullptr, 0, 0};
    SDL_Surface* sf = IMG_Load(path.c_str());
    if (sf) { wi.w = sf->w; wi.h = sf->h; wi.t = SDL_CreateTextureFromSurface(gR, sf); SDL_FreeSurface(sf); }
    WImg& slot = (wImgs[path] = wi);
    return slot.t ? &slot : nullptr;
}
static void wSave() {
    std::string d;
    for (auto& p : wDoc) {
        char hd[64];
        snprintf(hd, sizeof hd, "P|%d|%d|%d|%d|", p.style, (p.b ? 1 : 0) | (p.i ? 2 : 0) | (p.u ? 4 : 0), p.align, p.list);
        d += hd; d += p.img; d += "|"; d += p.t; d += "\n";
    }
    writeFile(DIR_ + "document1.wdoc", d);
}
static void wLoad() {
    wDoc.clear(); wCur = 0; wScroll = 0; wDirty = false; wFollow = true;
    std::string d;
    if (readFile(DIR_ + "document1.wdoc", d)) {
        size_t pos = 0;
        while (pos < d.size()) {
            size_t e = d.find('\n', pos);
            if (e == std::string::npos) e = d.size();
            std::string ln = d.substr(pos, e - pos); pos = e + 1;
            if (ln.size() < 2 || ln[0] != 'P' || ln[1] != '|') continue;
            std::vector<std::string> f; size_t a = 2;
            for (int k = 0; k < 5; k++) { size_t b = ln.find('|', a); if (b == std::string::npos) break; f.push_back(ln.substr(a, b - a)); a = b + 1; }
            if (f.size() < 5) continue;
            WPara p;
            p.style = atoi(f[0].c_str()); int fl = atoi(f[1].c_str());
            p.b = fl & 1; p.i = fl & 2; p.u = fl & 4;
            p.align = atoi(f[2].c_str()); p.list = atoi(f[3].c_str()); p.img = f[4];
            p.t = ln.substr(a);
            wDoc.push_back(p);
        }
    } else if (readFile(DIR_ + "document1.txt", d)) { // v0.2 plain-text document
        std::string cur;
        for (char c : d) { if (c == '\n') { WPara p; p.t = cur; wDoc.push_back(p); cur.clear(); } else cur += c; }
        WPara p; p.t = cur; wDoc.push_back(p);
    }
    wEnsure();
}
static int wWords() {
    int n = 0;
    for (auto& p : wDoc) if (p.style < 3) n += wordCount(p.t);
    return n;
}
static void wInsertAfter(const WPara& np) {
    wEnsure();
    wDoc.insert(wDoc.begin() + wCur + 1, np);
    wCur++; wDirty = true; wFollow = true;
}
static void wType(const std::string& t) {
    wEnsure();
    if (wDoc[wCur].style >= 3) { WPara np; wInsertAfter(np); }
    WPara& p = wDoc[wCur];
    if (t == "\n") {
        if (p.list && p.t.empty()) { p.list = 0; wDirty = true; return; } // Enter on an empty list item ends the list
        WPara np;
        np.align = p.align; np.list = p.list; np.b = p.b; np.i = p.i; np.u = p.u;
        if (p.style == 0) np.style = 0;
        wInsertAfter(np);
        return;
    }
    p.t += t; wDirty = true; wFollow = true;
}
static void wBack() {
    wEnsure();
    WPara& p = wDoc[wCur];
    if (p.style >= 3 || p.t.empty()) {
        if (p.list && p.style < 3) { p.list = 0; wDirty = true; return; }
        if (wDoc.size() > 1) { wDoc.erase(wDoc.begin() + wCur); wCur = std::max(0, wCur - 1); }
        else { p = WPara(); }
        wDirty = true; wFollow = true;
        return;
    }
    while (!p.t.empty() && ((unsigned char)p.t.back() & 0xC0) == 0x80) p.t.pop_back();
    if (!p.t.empty()) p.t.pop_back();
    wDirty = true; wFollow = true;
}
static void wScanPics() {
    wPics.clear();
    scanDir("sdmc:/switch/WinNX/photos", 2, false, wPics);
    scanDir("sdmc:/Pictures", 3, false, wPics);
    scanDir("sdmc:/Nintendo/Album", 6, false, wPics);
    std::sort(wPics.begin(), wPics.end(), [](const std::string& a, const std::string& b) { return a > b; });
    wPicScroll = 0;
}
// @@APPS_A@@
// ---------------------------------------------------------------- autosave + app lifecycle
static void saveAll() {
    if (gNote.dirty) { writeFile(DIR_ + "notes.txt", gNote.s); gNote.dirty = false; }
    if (wDirty) { wSave(); wDirty = false; }
    if (xDirty) { writeFile(DIR_ + "sheet1.csv", sheetToCsv()); xDirty = false; }
    if (pDirty && !pDown) { paintSaveFile(DIR_ + "paint.bmp"); pDirty = false; }
    // @@SAVE@@
}
static void appInit(int app) {
    std::string d;
    switch (app) {
        case A_NOTES: readFile(DIR_ + "notes.txt", gNote.s); gNote.scroll = 0; gNote.follow = true; gNote.dirty = false; break;
        case A_WORD: wLoad(); wTab = 0; wView = 0; break;
        case A_EXCEL:
            if (readFile(DIR_ + "sheet1.csv", d)) csvToSheet(d);
            xr = xc = xScrR = xScrC = 0; xEditing = false; xDirty = false; break;
        case A_PHOTOS:
            gPhotos.clear();
            scanDir("sdmc:/Nintendo/Album", 6, false, gPhotos);
            scanDir("sdmc:/switch/WinNX/photos", 2, false, gPhotos);
            scanDir("sdmc:/Pictures", 3, false, gPhotos);
            std::sort(gPhotos.begin(), gPhotos.end());
            phIdx = gPhotos.empty() ? 0 : (int)gPhotos.size() - 1;
            loadPhoto();
            break;
        case A_GAMES: scanGames(); gamesScroll = 0; break;
        case A_STORE: storeStart = SDL_GetTicks(); storeView = 0; storeCat = -1; storeScroll = 0; storeSearch.clear(); break;
        case A_BROWSER: loadHist(); brMsg.clear(); break;
        case A_PAINT: paintLoad(); break;
        // @@INIT@@
        default: break;
    }
}
static void appReset(int app) {
    saveAll();
    switch (app) {
        case A_NOTES: gNote.s.clear(); break;
        case A_WORD: wDoc.clear(); wFreeImgs(); break;
        case A_EXCEL: for (int r = 0; r < XR; r++) for (int c = 0; c < XC; c++) xs[r][c].clear(); break;
        case A_PHOTOS: if (phTex) { SDL_DestroyTexture(phTex); phTex = nullptr; } gPhotos.clear(); break;
        case A_GAMES: gGames.clear(); break;
        case A_STORE: storeView = 0; break;
        case A_PAINT: paintFree(); break;
        // @@RESET@@
        case A_CALC: cCur = "0"; cAcc = 0; cOp = 0; cFresh = true; cErr = false; break;
        default: break;
    }
    running[app] = false;
}

// ---------------------------------------------------------------- navigation
static void launch(int tile) {
    if (appDir != 0 || curApp >= 0 || tile < 0 || tile >= (int)gTiles.size()) return;
    animTile = tile; lastTile = tile;
    curApp = gTiles[tile].app; lastApp = curApp;
    if (!running[curApp]) { appInit(curApp); running[curApp] = true; }
    appDir = 1; appT = 0; barT = 0; barP = 0;
}
static void closeApp(bool reset) {
    if (appDir != 0 || curApp < 0) return;
    if (kbTarget == 3) commitCell();
    kbOpen = false;
    saveAll();
    appDir = -1; appT = 1; gCloseReset = reset;
}
static void winPress() {
    if (curApp >= 0) closeApp(false);
    else if (lastTile >= 0) launch(lastTile);
}
static SDL_Rect tileScreen(int i) {
    SDL_Rect r = tileBase(i);
    r.x -= (int)scrollX;
    return r;
}
static int hitTile(int x, int y) {
    if (y >= UIH) return -1;
    for (int i = 0; i < (int)gTiles.size(); i++) {
        SDL_Rect r = tileScreen(i);
        if (inR(x, y, r.x, r.y, r.w, r.h)) return i;
    }
    return -1;
}
static bool inWinBtn(int x, int y) { return abs(x - W / 2) <= 120 && y >= UIH - 6; }
static void showTile(int i) {
    SDL_Rect r = tileBase(i);
    if (r.x - scrollX < 40) scrollX = (float)(r.x - 40);
    if (r.x + r.w - scrollX > W - 40) scrollX = (float)(r.x + r.w - (W - 40));
    scrollX = clampf(scrollX, 0, maxScroll);
}
static void navigate(int dir) { // 0 left 1 right 2 up 3 down
    SDL_Rect c = tileBase(sel);
    float cx = c.x + c.w / 2.0f, cy = c.y + c.h / 2.0f;
    int best = -1; float bs = 1e9f;
    for (int i = 0; i < (int)gTiles.size(); i++) {
        if (i == sel) continue;
        SDL_Rect r = tileBase(i);
        float dx = r.x + r.w / 2.0f - cx, dy = r.y + r.h / 2.0f - cy, s;
        if (dir == 0 && dx < -10) s = -dx + 2 * fabsf(dy);
        else if (dir == 1 && dx > 10) s = dx + 2 * fabsf(dy);
        else if (dir == 2 && dy < -10) s = -dy + 2 * fabsf(dx);
        else if (dir == 3 && dy > 10) s = dy + 2 * fabsf(dx);
        else continue;
        if (s < bs) { bs = s; best = i; }
    }
    if (best >= 0) { sel = best; showTile(sel); }
}

// ---------------------------------------------------------------- icons
static void drawIcon(int app, int cx, int cy, Col bg) {
    switch (app) {
        case A_MAIL:
            rect(cx - 29, cy - 20, 58, 4, WHITE); rect(cx - 29, cy + 16, 58, 4, WHITE);
            rect(cx - 29, cy - 20, 4, 40, WHITE); rect(cx + 25, cy - 20, 4, 40, WHITE);
            line(cx - 29, cy - 18, cx, cy + 2, 4, WHITE); line(cx + 29, cy - 18, cx, cy + 2, 4, WHITE);
            break;
        case A_MUSIC:
            ring(cx, cy, 24, 6, WHITE, bg);
            rect(cx - 32, cy + 2, 64, 32, bg);
            rect(cx - 32, cy + 2, 14, 26, WHITE); rect(cx + 18, cy + 2, 14, 26, WHITE);
            break;
        case A_PHOTOS:
            outline(cx - 30, cy - 22, 60, 44, 4, WHITE);
            disc(cx + 14, cy - 8, 6, WHITE);
            line(cx - 26, cy + 16, cx - 6, cy - 4, 4, WHITE); line(cx - 6, cy - 4, cx + 8, cy + 10, 4, WHITE);
            break;
        case A_DESKTOP:
            outline(cx - 30, cy - 22, 60, 44, 4, WHITE);
            rect(cx - 26, cy - 18, 52, 10, WHITE);
            break;
        case A_STORE:
            ring(cx, cy - 8, 14, 5, WHITE, bg);
            rect(cx - 24, cy - 8, 48, 38, WHITE);
            winLogo(cx, cy + 12, 22, bg);
            break;
        case A_CALC:
            for (int r = 0; r < 3; r++) for (int c = 0; c < 3; c++) rect(cx - 27 + c * 20, cy - 27 + r * 20, 14, 14, WHITE);
            break;
        case A_NOTES:
            for (int i = 0; i < 4; i++) rect(cx - 28, cy - 24 + i * 14, i == 3 ? 36 : 56, 5, WHITE);
            break;
        case A_WEATHER:
            disc(cx, cy, 16, WHITE);
            for (int k = 0; k < 8; k++) {
                float a = k * 0.785398f;
                line(cx + (int)(sinf(a) * 23), cy - (int)(cosf(a) * 23), cx + (int)(sinf(a) * 33), cy - (int)(cosf(a) * 33), 4, WHITE);
            }
            break;
        case A_SETTINGS:
            ring(cx, cy, 20, 8, WHITE, bg);
            for (int k = 0; k < 8; k++) {
                float a = k * 0.785398f;
                disc(cx + (int)(sinf(a) * 23), cy - (int)(cosf(a) * 23), 5, WHITE);
            }
            disc(cx, cy, 9, bg);
            break;
        case A_POWER:
            ring(cx, cy + 2, 22, 6, WHITE, bg);
            rect(cx - 8, cy - 28, 16, 22, bg);
            line(cx, cy - 26, cx, cy + 2, 6, WHITE);
            break;
        case A_GAMES:
            rect(cx - 30, cy - 14, 60, 28, WHITE);
            disc(cx - 30, cy, 14, WHITE); disc(cx + 30, cy, 14, WHITE);
            rect(cx - 36, cy - 3, 16, 6, bg); rect(cx - 31, cy - 8, 6, 16, bg);
            disc(cx + 26, cy - 4, 3, bg); disc(cx + 34, cy + 4, 3, bg);
            break;
        case A_PAINT:
            line(cx - 26, cy + 24, cx + 20, cy - 22, 10, WHITE);
            disc(cx - 26, cy + 24, 8, WHITE); disc(cx + 24, cy - 26, 8, bg);
            disc(cx + 10, cy + 20, 8, WHITE); disc(cx + 26, cy + 8, 6, WHITE);
            break;
        case A_BROWSER:
            ring(cx, cy, 28, 5, WHITE, bg);
            rect(cx - 28, cy - 2, 56, 4, WHITE); rect(cx - 2, cy - 28, 4, 56, WHITE);
            rect(cx - 22, cy - 15, 44, 3, WHITE); rect(cx - 22, cy + 12, 44, 3, WHITE);
            break;
        case A_WORD:
            text("W", F72, cx, cy - 46, WHITE, 1);
            break;
        case A_EXCEL:
            text("X", F72, cx, cy - 46, WHITE, 1);
            break;
        default: break;
    }
}

// ---------------------------------------------------------------- start screen
static void drawTile(int i, float introOff, bool pressed, bool selected) {
    const Tile& t = gTiles[i];
    SDL_Rect r = tileScreen(i);
    r.x += (int)introOff;
    if (r.x > W || r.x + r.w < 0) return;
    if (pressed) { r.x += 5; r.y += 5; r.w -= 10; r.h -= 10; }
    rect(r.x, r.y, r.w, r.h, t.color);
    int cx = r.x + r.w / 2, cy = r.y + r.h / 2 - 12;
    std::string label = t.label;
    if (t.app == A_CALENDAR) {
        text(std::to_string(gNow.d), F72, r.x + 14, r.y + 8, WHITE);
        label = WD[dow(gNow.y, gNow.mo, gNow.d)];
    } else if (t.app == A_CLOCK) {
        int ccx = cx, ccy = r.y + r.h / 2 - 22;
        ring(ccx, ccy, 96, 5, WHITE, t.color);
        for (int k = 0; k < 12; k++) {
            float a = k * 0.523599f;
            line(ccx + (int)(sinf(a) * 78), ccy - (int)(cosf(a) * 78), ccx + (int)(sinf(a) * 88), ccy - (int)(cosf(a) * 88), 3, WHITE);
        }
        float ah = ((gNow.h % 12) + gNow.mi / 60.0f) * 0.523599f, am = gNow.mi * 0.104720f, as = gNow.s * 0.104720f;
        line(ccx, ccy, ccx + (int)(sinf(ah) * 46), ccy - (int)(cosf(ah) * 46), 6, WHITE);
        line(ccx, ccy, ccx + (int)(sinf(am) * 70), ccy - (int)(cosf(am) * 70), 4, WHITE);
        line(ccx, ccy, ccx + (int)(sinf(as) * 76), ccy - (int)(cosf(as) * 76), 2, WHITE, 200);
        disc(ccx, ccy, 6, WHITE);
        text(timeStr(true), F22, r.x + r.w - 12, r.y + r.h - 34, WHITE, 2);
    } else {
        drawIcon(t.app, cx, cy, t.color);
    }
    text(label, t.app == A_CALENDAR ? F22 : F16, r.x + 10, r.y + r.h - (t.app == A_CALENDAR ? 34 : 26), WHITE);
    if (running[t.app]) disc(r.x + r.w - 14, r.y + 14, 5, WHITE);
    if (selected) outline(r.x - 3, r.y - 3, r.w + 6, r.h + 6, 3, WHITE);
}

static void drawStart(bool tDown, bool tMoved, int pressTile) {
    const Theme& th = THEMES[gTheme];
    for (int y = 0; y < UIH; y += 6) rect(0, y, W, 6, mix(th.top, th.bottom, y / (float)UIH));
    text("Start", F72, 88, 30, WHITE);
    int ax = W - 100, ay = 40;
    rect(ax, ay, 48, 48, Col{70, 70, 90});
    disc(ax + 24, ay + 19, 9, Col{200, 200, 215});
    rect(ax + 10, ay + 32, 28, 16, Col{200, 200, 215});
    text(gUser, F22, W - 112, 42, WHITE, 2);
    char bb[16]; snprintf(bb, sizeof bb, "%u%%", (unsigned)gBat);
    text(bb, F16, W - 112, 72, WHITE, 2, 190);
    for (int i = 0; i < (int)gTiles.size(); i++) {
        float t = ease((introT - 0.05f * i) / 0.45f);
        float off = (1.0f - t) * 320.0f;
        drawTile(i, off, tDown && !tMoved && pressTile == i, navMode && sel == i);
    }
}

// ---------------------------------------------------------------- apps
static void toggle(int x, int y, bool on, Col accent) {
    rect(x, y, 100, 40, on ? accent : Col{90, 90, 90});
    rect(x + (on ? 62 : 4), y + 4, 34, 32, WHITE);
}
static void appTitle(const char* s) { text(s, F72, 70, 34, WHITE); }
static int areaBottom() { return kbOpen ? KBTOP : UIH; }

struct Chip { int app; SDL_Rect r; };
static std::vector<Chip> deskChips() {
    std::vector<Chip> v;
    int x = 140;
    for (int a = 0; a < A_COUNT; a++) {
        if (!running[a] || a == A_DESKTOP) continue;
        Chip c; c.app = a; c.r.x = x; c.r.y = UIH - 36; c.r.w = 130; c.r.h = 32;
        v.push_back(c); x += 136;
    }
    return v;
}


// ---------------------------------------------------------------- Store (Windows 8.1 look)
static const Col STG = {0, 138, 0};            // store green
static const Col GREYT = {80, 80, 80};
static void star(int cx, int cy, int r, Col c) {
    tri((float)cx, cy - (float)r, cx - r * 0.87f, cy + r * 0.5f, cx + r * 0.87f, cy + r * 0.5f, c);
    tri((float)cx, cy + (float)r, cx - r * 0.87f, cy - r * 0.5f, cx + r * 0.87f, cy - r * 0.5f, c);
}
static void stars(int x, int y, int n) {
    for (int i = 0; i < 5; i++) star(x + 8 + i * 18, y + 8, 8, i < n ? Col{70, 70, 70} : Col{200, 200, 200});
}
static void storeIcon(const SApp& a, int x, int y, int sz) {
    rect(x, y, sz, sz, a.col);
    std::string g = a.glyph;
    int f = sz >= 150 ? (g.size() > 2 ? F44 : F130) : (sz >= 90 ? (g.size() > 2 ? F30 : F72) : (g.size() > 2 ? F16 : F30));
    int fh = f == F130 ? 130 : f == F72 ? 72 : f == F44 ? 44 : f == F30 ? 30 : 16;
    text(g, f, x + sz / 2, y + sz / 2 - (int)(fh * 0.62f), WHITE, 1);
}
static void drawBag(int cx, int cy) {
    ring(cx, cy - 38, 30, 6, WHITE, STG);
    rect(cx - 52, cy - 38, 104, 92, WHITE);
    winLogo(cx, cy + 8, 46, STG);
}
static int storeFeat() { return (int)((SDL_GetTicks() / 6000) % std::max(1, NSTORE)); }
static std::vector<int> storeList() {
    std::vector<int> v;
    std::string q = lower(storeSearch);
    for (int i = 0; i < NSTORE; i++) {
        if (storeCat >= 0 && std::string(STORE[i].cat) != CATS[storeCat]) continue;
        if (!q.empty() && lower(STORE[i].name).find(q) == std::string::npos) continue;
        v.push_back(i);
    }
    return v;
}
static const int ST_LIST_X = 660, ST_LIST_Y = 206, ST_ROW = 84;
static SDL_Rect storeChip(int i) { // i = 0 "All", 1.. categories
    int x = ST_LIST_X;
    for (int k = 0; k < i; k++) x += tw(k == 0 ? "All" : CATS[k - 1], F16) + 28 + 6;
    SDL_Rect r = {x, 150, tw(i == 0 ? "All" : CATS[i - 1], F16) + 28, 38};
    return r;
}
static void storeAct(int idx) {
    const SApp& a = STORE[idx];
    if (storeProg[a.app] > 0) return;
    if (gInst[a.app]) { pendingTile = tileIndexFor(a.app); closeApp(false); }
    else storeProg[a.app] = 0.001f;
}
static void storeUninstall(int idx) {
    const SApp& a = STORE[idx];
    if (!gInst[a.app]) return;
    if (running[a.app]) appReset(a.app);
    gInst[a.app] = false; rebuildTiles(); saveCfg();
}
static void drawStoreBtn(int x, int y, int w, int idx, const Theme& th, bool primary) {
    const SApp& a = STORE[idx];
    if (storeProg[a.app] > 0) {
        rect(x, y + 16, w, 14, Col{210, 210, 210});
        rect(x, y + 16, (int)(w * storeProg[a.app]), 14, STG);
        text("Installing...", F16, x, y - 8, GREYT);
    } else if (primary) {
        btn(x, y, w, 52, gInst[a.app] ? "Open" : "Install", STG, WHITE);
    }
    (void)th;
}
static void drawStore(const Theme& th, int bottom) {
    float st = (SDL_GetTicks() - storeStart) / 1000.0f;
    if (st < 1.4f) { // splash screen
        rect(0, 0, W, UIH, Col{0, 140, 0});
        drawBag(W / 2, UIH / 2 - 40);
        for (int k = 0; k < 8; k++) {
            float ang = st * 6.0f - k * 0.38f;
            disc(W / 2 + (int)(sinf(ang) * 22), UIH / 2 + 110 - (int)(cosf(ang) * 22), 4 - k / 3, WHITE, 255 - k * 28);
        }
        return;
    }
    rect(0, 0, W, UIH, Col{243, 243, 243});
    tri(560, 0, 720, 0, 620, 92, Col{120, 190, 70});
    tri(560, 0, 620, 92, 520, 0, Col{40, 150, 40}, 160);
    // diagonal light wedge in the background
    tri(0, UIH, 340, UIH, 0, UIH - 250, Col{255, 255, 255}, 120);
    if (storeView == 1) { // ---------------- detail page
        const SApp& a = STORE[storeSel];
        ring(60, 70, 26, 3, GREYT, Col{243, 243, 243});
        line(70, 70, 50, 70, 3, GREYT); line(50, 70, 58, 62, 3, GREYT); line(50, 70, 58, 78, 3, GREYT);
        text("Store", F44, 118, 46, GREYT);
        storeIcon(a, 70, 150, 220);
        text(a.name, F44, 320, 146, Col{50, 50, 50});
        text(a.cat, F22, 320, 202, STG);
        stars(320, 238, a.stars);
        text(std::string("(") + a.rate + " ratings)", F16, 320 + 100, 240, GREYT);
        text("Free", F22, 320, 272, GREYT);
        drawStoreBtn(320, 318, 220, storeSel, th, true);
        if (storeProg[a.app] <= 0 && gInst[a.app]) btn(560, 318, 220, 52, "Uninstall", Col{110, 110, 110}, WHITE);
        text("Description", F30, 70, 410, Col{50, 50, 50});
        text(a.d1, F22, 70, 456, Col{60, 60, 60});
        int y = 494;
        for (auto& l : wrapText(a.d2, F22, 1100)) { text(l, F22, 70, y, GREYT); y += 30; }
        return;
    }
    // ---------------- home page
    text("Store", F72, 76, 24, Col{70, 70, 70});
    rect(900, 56, 310, 42, WHITE);
    outline(900, 56, 310, 42, 2, Col{150, 150, 150});
    if (storeSearch.empty() && !(kbOpen && kbTarget == 5)) text("Search for apps", F22, 912, 64, Col{150, 150, 150});
    else text(storeSearch + ((kbOpen && kbTarget == 5 && (SDL_GetTicks() / 500) % 2 == 0) ? "|" : ""), F22, 912, 64, DARK);
    ring(1190, 75, 9, 3, GREYT, WHITE); line(1196, 82, 1203, 90, 3, GREYT);

    std::vector<int> L = storeList();
    int f = storeFeat();
    const SApp& fa = STORE[f];
    // featured banner
    rect(70, 130, 560, 240, fa.col);
    tri(70, 370, 350, 130, 70, 130, Col{255, 255, 255}, 28);
    tri(630, 130, 630, 370, 430, 370, Col{0, 0, 0}, 40);
    text(fa.name, F44, 96, 150, WHITE);
    text(fa.cat, F22, 96, 204, WHITE, 0, 210);
    text(fa.glyph, std::string(fa.glyph).size() > 2 ? F72 : F130, 560, 160, WHITE, 2, 90);
    for (int i = 0; i < NSTORE; i++) rect(96 + i * 18, 346, 12, 5, WHITE, i == f ? 255 : 90);
    // info card under the banner
    rect(92, 384, 120, 120, WHITE);
    storeIcon(fa, 98, 390, 108);
    text(fa.name, F30, 232, 392, STG);
    text(fa.cat, F22, 232, 432, GREYT);
    stars(232, 466, fa.stars);
    int yy = 500;
    for (auto& l : wrapText(fa.d1, F16, 390)) { text(l, F16, 232, yy, GREYT); yy += 22; }
    drawStoreBtn(232, 580, 200, f, th, true);

    // right: category chips + list
    text(storeSearch.empty() ? "Top free apps" : "Results", F30, ST_LIST_X, 106, Col{70, 70, 70});
    for (int i = 0; i < 6; i++) {
        SDL_Rect r = storeChip(i);
        bool on = (storeCat == i - 1);
        rect(r.x, r.y, r.w, r.h, on ? STG : Col{222, 222, 222});
        text(i == 0 ? "All" : CATS[i - 1], F16, r.x + r.w / 2, r.y + 8, on ? WHITE : Col{60, 60, 60}, 1);
    }
    int n = (int)L.size();
    int maxS = std::max(0, n * ST_ROW - (bottom - ST_LIST_Y));
    storeScroll = std::max(0, std::min(storeScroll, maxS));
    SDL_Rect clip = {ST_LIST_X - 10, ST_LIST_Y, W - ST_LIST_X + 10, std::max(1, bottom - ST_LIST_Y)};
    SDL_RenderSetClipRect(gR, &clip);
    for (int i = 0; i < n; i++) {
        int y = ST_LIST_Y + i * ST_ROW - storeScroll;
        if (y + ST_ROW < ST_LIST_Y || y > bottom) continue;
        const SApp& a = STORE[L[i]];
        storeIcon(a, ST_LIST_X, y + 6, 64);
        text(a.name, F22, ST_LIST_X + 82, y + 4, Col{40, 40, 40});
        text("Free", F16, ST_LIST_X + 82, y + 36, GREYT);
        stars(ST_LIST_X + 130, y + 33, a.stars);
        text(a.cat, F16, ST_LIST_X + 82, y + 58, Col{140, 140, 140});
        if (storeProg[a.app] > 0) {
            rect(W - 190, y + 30, 150, 10, Col{210, 210, 210});
            rect(W - 190, y + 30, (int)(150 * storeProg[a.app]), 10, STG);
        } else if (gInst[a.app]) text("Installed", F16, W - 40, y + 26, STG, 2);
    }
    SDL_RenderSetClipRect(gR, nullptr);
    if (n == 0) text("No apps found", F30, ST_LIST_X, ST_LIST_Y + 20, GREYT);
}
static void storeTap(int x, int y) {
    if (SDL_GetTicks() - storeStart < 1400) return;
    if (storeView == 1) {
        const SApp& a = STORE[storeSel];
        if ((x - 60) * (x - 60) + (y - 70) * (y - 70) < 40 * 40) { storeView = 0; return; }
        if (inR(x, y, 320, 318, 220, 52)) storeAct(storeSel);
        else if (inR(x, y, 560, 318, 220, 52) && gInst[a.app] && storeProg[a.app] <= 0) storeUninstall(storeSel);
        return;
    }
    if (kbOpen && kbTarget == 5 && !inR(x, y, 900, 56, 310, 42)) hideKb();
    if (inR(x, y, 900, 56, 310, 42)) { openKb(5); return; }
    for (int i = 0; i < 6; i++) {
        SDL_Rect r = storeChip(i);
        if (inR(x, y, r.x, r.y, r.w, r.h)) { storeCat = i - 1; storeScroll = 0; return; }
    }
    int f = storeFeat();
    if (inR(x, y, 232, 580, 200, 52)) { storeAct(f); return; }
    if (inR(x, y, 70, 130, 560, 240) || inR(x, y, 92, 384, 520, 190)) { storeSel = f; storeView = 1; hideKb(); return; }
    if (x >= ST_LIST_X - 10 && y >= ST_LIST_Y && y < areaBottom()) {
        std::vector<int> L = storeList();
        int i = (y - ST_LIST_Y + storeScroll) / ST_ROW;
        if (i >= 0 && i < (int)L.size()) { storeSel = L[i]; storeView = 1; hideKb(); }
    }
}

// ---------------------------------------------------------------- CMrhoe (Amir's browser)
struct Bm { const char* name; const char* url; Col c; };
static const Bm BMS[8] = {
    {"DuckDuckGo", "https://duckduckgo.com", {222, 88, 51}},
    {"Google", "https://www.google.com", {66, 133, 244}},
    {"Wikipedia", "https://en.wikipedia.org", {90, 90, 100}},
    {"YouTube", "https://m.youtube.com", {204, 24, 30}},
    {"Reddit", "https://old.reddit.com", {255, 87, 34}},
    {"GitHub", "https://github.com", {36, 41, 46}},
    {"Nintendo", "https://www.nintendo.com", {230, 0, 18}},
    {"News", "https://news.google.com", {20, 130, 120}},
};
static const char* ENG_NAME[3] = {"DDG", "Google", "Bing"};
static const char* ENG_URL[3] = {"https://duckduckgo.com/?q=", "https://www.google.com/search?q=", "https://www.bing.com/search?q="};
static std::string urlEncode(const std::string& in) {
    std::string o; char b[8];
    for (unsigned char c : in) {
        if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') o += (char)c;
        else if (c == ' ') o += '+';
        else { snprintf(b, sizeof b, "%%%02X", c); o += b; }
    }
    return o;
}
static std::string buildUrl(std::string in) {
    while (!in.empty() && in.back() == ' ') in.pop_back();
    size_t st = in.find_first_not_of(' ');
    if (st == std::string::npos) return "";
    in = in.substr(st);
    std::string l = lower(in);
    if (l.rfind("http://", 0) == 0 || l.rfind("https://", 0) == 0) return in;
    if (in.find(' ') == std::string::npos && in.find('.') != std::string::npos) return "https://" + in;
    return std::string(ENG_URL[gEngine]) + urlEncode(in);
}
static void openUrl(const std::string& url) {
    gHist.erase(std::remove(gHist.begin(), gHist.end(), url), gHist.end());
    gHist.insert(gHist.begin(), url);
    if (gHist.size() > 12) gHist.resize(12);
    saveHist();
    saveAll();
    Result rc;
    if (appletGetAppletType() == AppletType_Application) {
        WebCommonConfig cfg;
        rc = webPageCreate(&cfg, url.c_str());
        if (R_SUCCEEDED(rc)) { webConfigSetWhitelist(&cfg, "^http*"); rc = webConfigShow(&cfg, NULL); }
    } else {
        WebWifiConfig cfg;
        Uuid u; memset(&u, 0, sizeof u);
        webWifiCreate(&cfg, NULL, url.c_str(), u, 0);
        WebWifiReturnValue out;
        rc = webWifiShow(&cfg, &out);
    }
    if (R_FAILED(rc)) {
        char b[96]; snprintf(b, sizeof b, "Couldn't open the page (error 0x%X). Check Wi-Fi.", (unsigned)rc);
        brMsg = b; brMsgT = SDL_GetTicks();
    }
}
static void browserGo() {
    std::string u = buildUrl(gUrl);
    if (u.empty()) return;
    hideKb();
    openUrl(u);
}
static void drawBrowser(int bottom) {
    (void)bottom;
    Col red = {219, 68, 55};
    rect(0, 0, W, UIH, Col{244, 244, 246});
    rect(0, 0, W, 90, red);
    ring(62, 50, 20, 4, WHITE, red);
    rect(42, 48, 40, 3, WHITE); rect(61, 30, 3, 40, WHITE);
    text("CMrhoe", F44, 96, 22, WHITE);
    text("Amir's browser", F16, 290, 44, WHITE, 0, 200);
    // address bar
    rect(70, 112, 920, 58, WHITE);
    outline(70, 112, 920, 58, 2, kbOpen && kbTarget == 4 ? red : Col{170, 170, 170});
    if (gUrl.empty() && !(kbOpen && kbTarget == 4)) text("Search or enter address", F30, 88, 124, Col{150, 150, 150});
    else {
        std::string t = gUrl;
        while (tw(t, F30) > 880 && t.size() > 1) t.erase(0, 1);
        text(t + ((kbOpen && kbTarget == 4 && (SDL_GetTicks() / 500) % 2 == 0) ? "|" : ""), F30, 88, 124, DARK);
    }
    btn(1000, 112, 90, 58, "Go", red, WHITE);
    btn(1100, 112, 110, 58, ENG_NAME[gEngine], Col{90, 90, 100}, WHITE);
    if (!brMsg.empty()) {
        if (SDL_GetTicks() - brMsgT > 6000) brMsg.clear();
        else text(brMsg, F22, 70, 180, Col{190, 40, 30});
    }
    text("Bookmarks", F30, 70, 214, Col{60, 60, 60});
    for (int i = 0; i < 8; i++) {
        int x = 70 + (i % 4) * 290, y = 260 + (i / 4) * 124;
        rect(x, y, 270, 108, BMS[i].c);
        text(BMS[i].name, F30, x + 16, y + 62, WHITE);
        text(std::string(BMS[i].name).substr(0, 1), F44, x + 16, y + 8, WHITE, 0, 200);
    }
    text("Recent", F30, 70, 512, Col{60, 60, 60});
    int n = std::min<int>(3, gHist.size());
    for (int i = 0; i < n; i++) {
        std::string h = gHist[i];
        while (tw(h, F22) > 1100 && h.size() > 4) h.pop_back();
        rect(70, 556 + i * 34, 1140, 30, WHITE);
        text(h, F22, 82, 558 + i * 34, Col{50, 90, 160});
    }
    if (n == 0) text("Pages you open will show up here.", F22, 70, 558, Col{130, 130, 130});
}
static void browserTap(int x, int y) {
    if (kbOpen && kbTarget == 4 && y < KBTOP && !inR(x, y, 70, 112, 920, 58)) {
        if (inR(x, y, 1000, 112, 90, 58)) { browserGo(); return; }
        hideKb();
    }
    if (inR(x, y, 70, 112, 920, 58)) { openKb(4); return; }
    if (inR(x, y, 1000, 112, 90, 58)) { browserGo(); return; }
    if (inR(x, y, 1100, 112, 110, 58)) { gEngine = (gEngine + 1) % 3; saveCfg(); return; }
    for (int i = 0; i < 8; i++) {
        int bx = 70 + (i % 4) * 290, by = 260 + (i / 4) * 124;
        if (inR(x, y, bx, by, 270, 108)) { openUrl(BMS[i].url); return; }
    }
    int n = std::min<int>(3, gHist.size());
    for (int i = 0; i < n; i++)
        if (inR(x, y, 70, 556 + i * 34, 1140, 30)) { std::string u = gHist[i]; openUrl(u); return; }
}


// ---------------------------------------------------------------- Paint (UI)
static void paintShape(int x0, int y0, int x1, int y1) {
    Uint32 v = packCol(PAL[pCol]);
    int r = std::max(1, PSIZES[pSizeIdx] / 2);
    if (pTool == 3) pLine(x0, y0, x1, y1, r, v);
    else if (pTool == 4) pRectO(x0, y0, x1, y1, r, v);
    else if (pTool == 5) pOval(x0, y0, x1, y1, r, v);
}
static void paintTouch(int ph, int x, int y) {
    paintEnsure();
    int cx = std::max(0, std::min(PCW - 1, x - PCX)), cy = std::max(0, std::min(PCH - 1, y - PCY));
    Uint32 v = packCol(PAL[pCol]), white = 0xFFFFFFFFu;
    int r = std::max(1, PSIZES[pSizeIdx] / 2);
    if (ph == 0) {
        if (!inR(x, y, PCX, PCY, PCW, PCH)) { pDown = false; return; }
        paintPush();
        pDown = true; pSX = pLX = cx; pSY = pLY = cy;
        if (pTool == 0) pDisc(cx, cy, r, v);
        else if (pTool == 1) pDisc(cx, cy, r + 3, white);
        else if (pTool == 2) { pFill(cx, cy, v); pDown = false; }
        else pBase = pPix;
        pTexDirty = pDirty = true;
    } else if (ph == 1 && pDown) {
        if (pTool == 0) pLine(pLX, pLY, cx, cy, r, v);
        else if (pTool == 1) pLine(pLX, pLY, cx, cy, r + 3, white);
        else if (pTool >= 3) { pPix = pBase; paintShape(pSX, pSY, cx, cy); }
        pLX = cx; pLY = cy; pTexDirty = pDirty = true;
    } else if (ph == 2 && pDown) {
        if (pTool >= 3) { pPix = pBase; paintShape(pSX, pSY, cx, cy); pTexDirty = pDirty = true; }
        pDown = false;
    }
}
static void paintTap(int x, int y) {
    paintEnsure();
    if (inR(x, y, PCX, PCY, PCW, PCH)) return;
    for (int i = 0; i < 6; i++) if (inR(x, y, 20 + i * 104, 48, 100, 56)) { pTool = i; return; }
    if (inR(x, y, 660, 48, 50, 56)) { pSizeIdx = std::max(0, pSizeIdx - 1); return; }
    if (inR(x, y, 774, 48, 50, 56)) { pSizeIdx = std::min(5, pSizeIdx + 1); return; }
    if (inR(x, y, 840, 48, 100, 56)) { if (!pUndo.empty()) { pPix = pUndo.back(); pUndo.pop_back(); pTexDirty = pDirty = true; } return; }
    if (inR(x, y, 950, 48, 100, 56)) { paintPush(); std::fill(pPix.begin(), pPix.end(), 0xFFFFFFFFu); pTexDirty = pDirty = true; return; }
    if (inR(x, y, 1060, 48, 100, 56)) { paintExport(); return; }
    for (int i = 0; i < 16; i++) if (inR(x, y, 20 + i * 77, 110, 72, 52)) { pCol = i; if (pTool == 1) pTool = 0; return; }
}
static void drawPaint() {
    paintEnsure();
    rect(0, 0, W, UIH, Col{58, 58, 64});
    static const char* TN[6] = {"Brush", "Eraser", "Fill", "Line", "Rect", "Oval"};
    for (int i = 0; i < 6; i++) btn(20 + i * 104, 48, 100, 56, TN[i], pTool == i ? WHITE : Col{86, 86, 94}, pTool == i ? Col{40, 40, 44} : WHITE);
    btn(660, 48, 50, 56, "-", Col{86, 86, 94}, WHITE);
    rect(714, 48, 56, 56, Col{40, 40, 44});
    text(std::to_string(PSIZES[pSizeIdx]), F22, 742, 62, WHITE, 1);
    btn(774, 48, 50, 56, "+", Col{86, 86, 94}, WHITE);
    btn(840, 48, 100, 56, "Undo", pUndo.empty() ? Col{70, 70, 76} : Col{86, 86, 94}, WHITE);
    btn(950, 48, 100, 56, "Clear", Col{150, 50, 55}, WHITE);
    btn(1060, 48, 100, 56, "Save", Col{200, 70, 160}, WHITE);
    for (int i = 0; i < 16; i++) {
        rect(20 + i * 77, 110, 72, 52, PAL[i]);
        if (i == pCol) { outline(20 + i * 77 - 3, 107, 78, 58, 3, WHITE); }
    }
    if (!pTex) pTex = SDL_CreateTexture(gR, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STATIC, PCW, PCH);
    if (pTex && pTexDirty) { SDL_UpdateTexture(pTex, nullptr, pPix.data(), PCW * 4); pTexDirty = false; }
    if (pTex) { SDL_Rect d = {PCX, PCY, PCW, PCH}; SDL_RenderCopy(gR, pTex, nullptr, &d); }
    outline(PCX - 2, PCY - 2, PCW + 4, PCH + 4, 2, Col{20, 20, 24});
    if (!pMsg.empty()) {
        if (SDL_GetTicks() - pMsgT > 3500) pMsg.clear();
        else { int w = tw(pMsg, F22) + 40; rect(W / 2 - w / 2, UIH - 70, w, 46, Col{20, 20, 24}, 230); text(pMsg, F22, W / 2, UIH - 59, WHITE, 1); }
    }
}

// ---------------------------------------------------------------- Word (UI)
struct WB { int id, x, w; const char* l; };
static std::vector<WB> wRibbon() {
    std::vector<WB> v;
    if (wTab == 0) {
        v = {{1, 20, 60, "B"}, {2, 86, 60, "I"}, {3, 152, 60, "U"}, {4, 236, 64, "A-"}, {5, 306, 64, "A+"},
             {6, 400, 100, "Normal"}, {7, 506, 100, "Title"}, {8, 612, 100, "Head"},
             {9, 740, 60, "Left"}, {10, 806, 60, "Mid"}, {11, 872, 60, "Right"}, {12, 960, 60, "\xE2\x80\xA2"}, {13, 1026, 60, "1."}};
    } else if (wTab == 1) {
        v = {{20, 20, 160, "Picture"}, {21, 190, 160, "Symbol"}, {22, 360, 160, "Date"}, {23, 530, 160, "Time"}, {24, 700, 160, "Divider"}};
    } else {
        v = {{30, 20, 150, "New"}, {31, 180, 150, "Save"}};
    }
    return v;
}
static const int WTOP = 154, WPX = 230, WPW = 820;
static std::vector<int> wY, wH;
static int wFontOf(const WPara& p) {
    if (p.style == 1) return std::min(F72, gDocFont + 2);
    if (p.style == 2) return std::min(F72, gDocFont + 1);
    return gDocFont;
}
static const std::vector<std::string>& wLines(WPara& p, int f, int w) {
    if (p.cf != f || p.cw != w || p.cs != p.t) {
        int st = (p.b || p.style == 1 ? 1 : 0) | (p.i ? 2 : 0);
        (void)st;
        p.lines = wrapText(p.t, f, w);
        p.cs = p.t; p.cf = f; p.cw = w;
    }
    return p.lines;
}
static void wApply(int id) {
    wEnsure();
    WPara& p = wDoc[wCur];
    bool textual = p.style < 3;
    switch (id) {
        case 1: p.b = !p.b; break;
        case 2: p.i = !p.i; break;
        case 3: p.u = !p.u; break;
        case 4: gDocFont = std::max(F16, gDocFont - 1); saveCfg(); break;
        case 5: gDocFont = std::min(F44, gDocFont + 1); saveCfg(); break;
        case 6: if (textual) p.style = 0; break;
        case 7: if (textual) p.style = (p.style == 1 ? 0 : 1); break;
        case 8: if (textual) p.style = (p.style == 2 ? 0 : 2); break;
        case 9: p.align = 0; break;
        case 10: p.align = 1; break;
        case 11: p.align = 2; break;
        case 12: if (textual) p.list = (p.list == 1 ? 0 : 1); break;
        case 13: if (textual) p.list = (p.list == 2 ? 0 : 2); break;
        case 20: wScanPics(); wView = 1; hideKb(); return;
        case 21: wView = 2; hideKb(); return;
        case 22: wType(dateStr()); openKb(2); break;
        case 23: wType(timeStr()); openKb(2); break;
        case 24: { WPara np; np.style = 3; wInsertAfter(np); WPara t; wInsertAfter(t); } break;
        case 30: backupFile(DIR_ + "document1.wdoc"); wDoc.clear(); wEnsure(); wScroll = 0; break;
        case 31: break;
        default: break;
    }
    wDirty = true; wFollow = true;
}
static const char* SYMS[40] = {"\xC2\xA9", "\xC2\xAE", "\xE2\x84\xA2", "\xC2\xB0", "\xC2\xB1", "\xC3\x97", "\xC3\xB7", "\xE2\x82\xAC", "\xC2\xA3", "\xC2\xA5",
    "\xC2\xA7", "\xC2\xB6", "\xE2\x80\xA2", "\xE2\x80\x93", "\xE2\x80\x94", "\xE2\x80\xA6", "\xC2\xAB", "\xC2\xBB", "\xC2\xBD", "\xC2\xBC",
    "\xC2\xBE", "\xC2\xB2", "\xC2\xB3", "\xC2\xB5", "\xCF\x80", "\xCE\xA9", "\xCE\xB1", "\xCE\xB2", "\xE2\x89\xA4", "\xE2\x89\xA5",
    "\xE2\x89\xA0", "\xE2\x88\x9E", "\xE2\x88\x9A", "\xE2\x86\x90", "\xE2\x86\x92", "\xE2\x86\x91", "\xE2\x86\x93", "\xE2\x9C\x93", "\xE2\x98\x85", "\xE2\x99\xA5"};
static void drawWord(int bottom) {
    Col blue = {43, 87, 154};
    rect(0, 0, W, UIH, Col{225, 226, 230});
    wEnsure();
    // ---- document area (drawn first so the ribbon overlaps cleanly)
    SDL_Rect clip = {0, WTOP, W, std::max(1, bottom - WTOP)};
    rect(190, WTOP, 900, bottom - WTOP, WHITE);
    wY.assign(wDoc.size(), 0); wH.assign(wDoc.size(), 0);
    int y = 0, num = 0;
    for (int i = 0; i < (int)wDoc.size(); i++) {
        WPara& p = wDoc[i];
        int f = wFontOf(p);
        int lh = gFont[f] ? TTF_FontLineSkip(gFont[f]) : 30;
        int indent = p.list ? 44 : 0;
        int h;
        if (p.style == 3) h = 24;
        else if (p.style == 4) {
            WImg* im = wImage(p.img);
            if (im) { float sc = std::min(std::min(1.0f, (float)WPW / im->w), 320.0f / im->h); h = (int)(im->h * sc) + 8; } else h = 50;
        } else {
            int n = (int)wLines(p, f, WPW - indent).size();
            h = std::max(1, n) * lh;
        }
        wY[i] = y; wH[i] = h; y += h + 8;
    }
    int total = y + 80;
    int view = bottom - WTOP - 20;
    int maxS = std::max(0, total - view);
    if (wFollow) {
        int top = wY[wCur], bot = wY[wCur] + wH[wCur];
        if (bot - wScroll > view - 20) wScroll = bot - view + 20;
        if (top - wScroll < 0) wScroll = top;
        wFollow = false;
    }
    wScroll = std::max(0, std::min(wScroll, maxS));
    SDL_RenderSetClipRect(gR, &clip);
    num = 0;
    for (int i = 0; i < (int)wDoc.size(); i++) {
        WPara& p = wDoc[i];
        int py = WTOP + 18 + wY[i] - wScroll;
        if (!(p.style < 3 && p.list == 2)) num = 0;
        if (py + wH[i] < WTOP || py > bottom) { if (p.list == 2 && p.style < 3) num++; continue; }
        bool cur = (i == wCur);
        if (cur) rect(WPX - 24, py - 2, 4, wH[i] + 4, Col{120, 160, 220});
        if (p.style == 3) { rect(WPX, py + 10, WPW, 3, Col{110, 110, 110}); continue; }
        if (p.style == 4) {
            WImg* im = wImage(p.img);
            if (im) {
                float sc = std::min(std::min(1.0f, (float)WPW / im->w), 320.0f / im->h);
                int dw = (int)(im->w * sc), dh = (int)(im->h * sc);
                int dx = WPX + (p.align == 1 ? (WPW - dw) / 2 : (p.align == 2 ? WPW - dw : 0));
                SDL_Rect d = {dx, py + 4, dw, dh};
                SDL_RenderCopy(gR, im->t, nullptr, &d);
                if (cur) outline(dx - 2, py + 2, dw + 4, dh + 4, 2, Col{120, 160, 220});
            } else text("[picture not found]", F22, WPX, py + 8, Col{170, 60, 60});
            continue;
        }
        int f = wFontOf(p);
        int lh = gFont[f] ? TTF_FontLineSkip(gFont[f]) : 30;
        int indent = p.list ? 44 : 0;
        int st = (p.b || p.style == 1 ? 1 : 0) | (p.i ? 2 : 0) | (p.u ? 4 : 0);
        Col ink = p.style == 1 ? blue : (p.style == 2 ? Col{70, 110, 170} : DARK);
        const std::vector<std::string>& L = wLines(p, f, WPW - indent);
        if (p.list == 1) disc(WPX + 14, py + lh / 2, 5, ink);
        if (p.list == 2) { num++; text(std::to_string(num) + ".", f, WPX + 34, py, ink, 2); }
        if (p.t.empty() && wDoc.size() == 1 && !p.list && p.style == 0) text("Tap here to start typing", f, WPX, py, Col{170, 170, 170});
        for (int k = 0; k < (int)L.size(); k++) {
            int ly = py + k * lh;
            int x = WPX + indent, al = 0;
            if (p.align == 1) { x = WPX + indent + (WPW - indent) / 2; al = 1; }
            else if (p.align == 2) { x = WPX + WPW; al = 2; }
            textS(L[k], f, x, ly, ink, st, al);
        }
        if (cur && kbOpen && kbTarget == 2 && ((SDL_GetTicks() / 500) % 2 == 0)) {
            const std::string& last = L.empty() ? std::string() : L.back();
            int lw = twS(last, f, st);
            int ly = py + (std::max(1, (int)L.size()) - 1) * lh;
            int cx = WPX + indent + lw + 2;
            if (p.align == 1) cx = WPX + indent + (WPW - indent) / 2 + lw / 2 + 2;
            else if (p.align == 2) cx = WPX + WPW + 2;
            rect(cx, ly + 3, 2, lh - 6, ink);
        }
    }
    SDL_RenderSetClipRect(gR, nullptr);
    // ---- title bar + tabs
    rect(0, 34, W, 56, blue);
    text("Word 2016", F30, 20, 46, WHITE);
    static const char* TABS[3] = {"Home", "Insert", "File"};
    for (int i = 0; i < 3; i++) {
        int x = 230 + i * 120;
        if (wTab == i) { rect(x, 44, 110, 46, Col{245, 245, 247}); text(TABS[i], F22, x + 55, 54, blue, 1); }
        else text(TABS[i], F22, x + 55, 54, WHITE, 1);
    }
    text(std::to_string(wWords()) + " words", F22, W - 20, 52, WHITE, 2, 210);
    // ---- ribbon
    rect(0, 90, W, 64, Col{245, 245, 247});
    rect(0, 153, W, 1, Col{190, 190, 195});
    WPara& cp = wDoc[wCur];
    for (auto& b : wRibbon()) {
        bool on = false;
        switch (b.id) {
            case 1: on = cp.b; break; case 2: on = cp.i; break; case 3: on = cp.u; break;
            case 6: on = cp.style == 0; break; case 7: on = cp.style == 1; break; case 8: on = cp.style == 2; break;
            case 9: on = cp.align == 0; break; case 10: on = cp.align == 1; break; case 11: on = cp.align == 2; break;
            case 12: on = cp.list == 1; break; case 13: on = cp.list == 2; break;
            default: break;
        }
        rect(b.x, 98, b.w, 48, on ? blue : Col{220, 222, 228});
        int st = b.id == 1 ? 1 : (b.id == 2 ? 2 : (b.id == 3 ? 4 : 0));
        textS(b.l, F22, b.x + b.w / 2, 108, on ? WHITE : DARK, st, 1);
    }
    if (wTab == 2) text("Autosaved to switch/WinNX/document1.wdoc", F16, 350, 112, Col{90, 90, 100});
    if (wTab == 1) text("Pictures come from Pictures, switch/WinNX/photos and the Album", F16, 890, 112, Col{90, 90, 100}, 0, 0);
    // ---- overlays
    if (wView == 1) {
        rect(0, 0, W, UIH, Col{0, 0, 0}, 170);
        rect(140, 50, 1000, 560, Col{245, 245, 247});
        text("Insert picture", F30, 170, 64, blue);
        btn(980, 58, 130, 44, "Cancel", Col{150, 150, 160}, WHITE);
        SDL_Rect pc = {140, 116, 1000, 480};
        SDL_RenderSetClipRect(gR, &pc);
        int n = (int)wPics.size();
        wPicScroll = std::max(0, std::min(wPicScroll, std::max(0, n * 50 - 470)));
        for (int i = 0; i < n; i++) {
            int ry = 116 + i * 50 - wPicScroll;
            if (ry + 46 < 116 || ry > 596) continue;
            rect(160, ry, 960, 44, i % 2 ? Col{235, 236, 240} : WHITE);
            std::string nm = wPics[i].substr(wPics[i].find_last_of('/') + 1);
            text(nm, F22, 176, ry + 8, DARK);
        }
        SDL_RenderSetClipRect(gR, nullptr);
        if (n == 0) text("No pictures found. Draw one in Paint and press Save.", F22, 170, 140, Col{90, 90, 100});
    } else if (wView == 2) {
        rect(0, 0, W, UIH, Col{0, 0, 0}, 170);
        rect(140, 50, 1000, 560, Col{245, 245, 247});
        text("Insert symbol", F30, 170, 64, blue);
        btn(980, 58, 130, 44, "Cancel", Col{150, 150, 160}, WHITE);
        for (int i = 0; i < 40; i++) {
            int sx = 170 + (i % 10) * 94, sy = 130 + (i / 10) * 112;
            rect(sx, sy, 88, 100, WHITE);
            text(SYMS[i], F44, sx + 44, sy + 24, DARK, 1);
        }
    }
}
static void wordTap(int x, int y) {
    wEnsure();
    if (wView == 1) {
        if (inR(x, y, 980, 58, 130, 44) || !inR(x, y, 140, 50, 1000, 560)) { wView = 0; return; }
        if (y >= 116 && y < 596) {
            int i = (y - 116 + wPicScroll) / 50;
            if (i >= 0 && i < (int)wPics.size()) {
                WPara np; np.style = 4; np.img = wPics[i]; np.align = 1;
                wInsertAfter(np); WPara t; wInsertAfter(t);
                wView = 0; openKb(2);
            }
        }
        return;
    }
    if (wView == 2) {
        if (inR(x, y, 980, 58, 130, 44) || !inR(x, y, 140, 50, 1000, 560)) { wView = 0; return; }
        for (int i = 0; i < 40; i++) {
            int sx = 170 + (i % 10) * 94, sy = 130 + (i / 10) * 112;
            if (inR(x, y, sx, sy, 88, 100)) { wType(SYMS[i]); wView = 0; openKb(2); return; }
        }
        return;
    }
    if (y >= 44 && y < 90) {
        for (int i = 0; i < 3; i++) if (inR(x, y, 230 + i * 120, 44, 110, 46)) { wTab = i; return; }
        return;
    }
    if (y >= 90 && y < 154) {
        for (auto& b : wRibbon()) if (inR(x, y, b.x, 98, b.w, 48)) { wApply(b.id); return; }
        return;
    }
    if (y >= WTOP) {
        int ry = y - WTOP - 18 + wScroll;
        int hit = -1;
        for (int i = 0; i < (int)wY.size(); i++) if (ry >= wY[i] - 4 && ry < wY[i] + wH[i] + 4) { hit = i; break; }
        if (hit < 0 && !wY.empty() && ry >= wY.back()) hit = (int)wY.size() - 1;
        if (hit >= 0) wCur = hit;
        openKb(2); wFollow = true;
    }
}
// @@APPS_B@@
static void appTouch(int phase, int x, int y) {
    switch (curApp) {
        case A_PAINT: paintTouch(phase, x, y); break;
        // @@TOUCH@@
        default: break;
    }
}
static void appUpdate(float dt) {
    (void)dt;
    // @@UPDATE@@
}
static void drawApp() {
    const Theme& th = THEMES[gTheme];
    Col bg = appBg(curApp);
    int bottom = areaBottom();
    switch (curApp) {
        case A_CALC: {
            rect(0, 0, W, UIH, bg);
            rect(348, 34, 584, 124, Col{42, 42, 42});
            if (cOp) {
                const char* sym[5] = {"", "+", "-", "\xC3\x97", "\xC3\xB7"};
                char b[48]; snprintf(b, sizeof b, "%.10g %s", cAcc, sym[cOp]);
                text(b, F22, 348 + 584 - 16, 40, WHITE, 2, 160);
            }
            text(cCur, cCur.size() > 11 ? F44 : F72, 348 + 584 - 16, 64, WHITE, 2);
            for (int i = 0; i < NCB; i++) {
                SDL_Rect r = calcRect(CBTNS[i]);
                std::string l = CBTNS[i].l;
                Col c = Col{58, 58, 58};
                if (l == "=") c = th.accent;
                else if (l == "C") c = Col{170, 50, 50};
                else if (!(l[0] >= '0' && l[0] <= '9') && l != ".") c = Col{86, 86, 86};
                rect(r.x, r.y, r.w, r.h, c);
                text(l, F44, r.x + r.w / 2, r.y + r.h / 2 - 28, WHITE, 1);
            }
            break;
        }
        case A_CLOCK: {
            rect(0, 0, W, UIH, bg);
            text(timeStr(), F130, W / 2, 130, WHITE, 1);
            char s[8]; snprintf(s, sizeof s, "%02d", gNow.s);
            text(s, F44, W / 2, 300, WHITE, 1, 200);
            if (!g24) text(gNow.h >= 12 ? "PM" : "AM", F30, W / 2, 370, WHITE, 1, 200);
            text(dateStr(), F44, W / 2, 440, WHITE, 1);
            break;
        }
        case A_CALENDAR: {
            rect(0, 0, W, UIH, bg);
            text(std::string(MONTHS[gNow.mo - 1]) + " " + std::to_string(gNow.y), F72, 70, 34, WHITE);
            for (int c = 0; c < 7; c++) text(WD3[c], F22, 150 + c * 140 + 70, 150, WHITE, 1, 200);
            int first = dow(gNow.y, gNow.mo, 1), dim = daysIn(gNow.y, gNow.mo);
            for (int d = 1; d <= dim; d++) {
                int idx = first + d - 1, col = idx % 7, row = idx / 7;
                int x = 150 + col * 140, y = 196 + row * 76;
                bool today = d == gNow.d;
                rect(x + 3, y + 3, 134, 70, WHITE, today ? 255 : 30);
                text(std::to_string(d), F30, x + 70, y + 18, today ? bg : WHITE, 1);
            }
            break;
        }
        case A_SETTINGS: {
            rect(0, 0, W, UIH, bg);
            appTitle("Settings");
            text("Background", F30, 70, 160, WHITE);
            for (int i = 0; i < 4; i++) {
                int x = 70 + i * 110;
                rect(x, 215, 90, 90, mix(THEMES[i].top, THEMES[i].bottom, 0.3f));
                if (i == gTheme) outline(x - 5, 210, 100, 100, 4, WHITE);
            }
            text("Burn-in protection", F30, 70, 340, WHITE);
            text("Shifts the screen slightly and dims it when idle (good for OLED)", F16, 70, 380, WHITE, 0, 170);
            toggle(1060, 345, gBurn, th.accent);
            text("24-hour clock", F30, 70, 460, WHITE);
            toggle(1060, 465, g24, th.accent);
            text("WinNX 0.2 - Windows 8.1-style shell for Nintendo Switch", F16, 70, 580, WHITE, 0, 130);
            text("Your files autosave to sdmc:/switch/WinNX/", F16, 70, 604, WHITE, 0, 130);
            break;
        }
        case A_POWER: {
            rect(0, 0, W, UIH, bg);
            appTitle("Power");
            text("Exit WinNX and return to the homebrew menu?", F30, 70, 190, WHITE);
            rect(70, 300, 260, 90, Col{209, 52, 56});
            text("Exit", F44, 200, 322, WHITE, 1);
            rect(350, 300, 260, 90, Col{80, 80, 80});
            text("Cancel", F44, 480, 322, WHITE, 1);
            break;
        }
        case A_DESKTOP: {
            for (int y = 0; y < UIH; y += 6) rect(0, y, W, 6, mix(Col{255, 190, 8}, Col{246, 160, 4}, y / (float)UIH));
            tri(0, 190, 260, 440, 0, 620, Col{232, 124, 6});
            tri(0, 190, 120, 330, 0, 420, Col{244, 150, 12});
            tri(520, 628, 575, 420, 600, 628, Col{222, 138, 10});
            tri(575, 420, 960, 628, 600, 628, Col{238, 228, 208});
            rect(36, 66, 36, 6, Col{207, 216, 220});
            rect(40, 74, 28, 36, Col{207, 216, 220});
            text("Recycle Bin", F16, 54, 118, WHITE, 1);
            rect(0, UIH - 40, W, 40, Col{28, 28, 28}, 238);
            disc(50, UIH - 20, 14, Col{27, 161, 226});
            text("e", F22, 50, UIH - 34, WHITE, 1);
            rect(88, UIH - 30, 30, 20, Col{250, 200, 60});
            rect(88, UIH - 34, 14, 6, Col{250, 200, 60});
            for (auto& c : deskChips()) {
                rect(c.r.x, c.r.y, c.r.w, c.r.h, Col{70, 70, 78});
                rect(c.r.x, c.r.y + c.r.h - 3, c.r.w, 3, appBg(c.app));
                text(appName(c.app), F16, c.r.x + c.r.w / 2, c.r.y + 5, WHITE, 1);
            }
            text(timeStr(), F16, W - 14, UIH - 38, WHITE, 2);
            char d[24]; snprintf(d, sizeof d, "%02d/%02d/%d", gNow.d, gNow.mo, gNow.y);
            text(d, F16, W - 14, UIH - 20, WHITE, 2, 200);
            break;
        }
        case A_NOTES: {
            rect(0, 0, W, UIH, bg);
            text("Notes", F44, 70, 40, WHITE);
            btn(1110, 42, 100, 42, "Clear", Col{150, 20, 55}, WHITE);
            rect(70, 100, 1140, bottom - 116, Col{252, 246, 205});
            drawEditor(gNote, SDL_Rect{94, 118, 1092, bottom - 116 - 36}, F30, Col{50, 40, 20}, false, kbOpen && kbTarget == 1, "Tap to start typing");
            break;
        }
        case A_WORD: drawWord(bottom); break;
        case A_EXCEL: {
            Col green = Col{33, 115, 70};
            rect(0, 0, W, UIH, WHITE);
            rect(0, 34, W, 50, green);
            text("Excel 2016", F30, 24, 42, WHITE);
            text("Book1 - autosaved", F22, 250, 46, WHITE, 0, 200);
            btn(1000, 40, 90, 38, "New", Col{60, 150, 100}, WHITE);
            btn(1100, 40, 90, 38, "Save", Col{60, 150, 100}, WHITE);
            rect(0, 90, W, 44, Col{245, 245, 245});
            rect(0, 133, W, 1, Col{200, 200, 200});
            text("fx", F22, 16, 98, green);
            std::string fb = xEditing ? xEdit : xs[xr][xc];
            if (xEditing && ((SDL_GetTicks() / 500) % 2 == 0)) fb += "|";
            text(fb, F22, 64, 98, DARK);
            rect(0, 140, W, 32, Col{232, 232, 232});
            int rows = xRows();
            for (int vc = 0; vc < XVC; vc++) {
                int c = xScrC + vc;
                if (c >= XC) break;
                std::string h(1, (char)('A' + c));
                text(h, F22, XHDRW + vc * XCOLW + XCOLW / 2, 144, Col{70, 70, 70}, 1);
                rect(XHDRW + vc * XCOLW, 140, 1, 32, Col{200, 200, 200});
            }
            for (int vr = 0; vr < rows; vr++) {
                int r = xScrR + vr;
                if (r >= XR) break;
                int y = XTOP + vr * XROWH;
                rect(0, y, XHDRW, XROWH, Col{232, 232, 232});
                text(std::to_string(r + 1), F22, XHDRW / 2, y + 6, Col{70, 70, 70}, 1);
                rect(0, y + XROWH - 1, W, 1, Col{215, 215, 215});
                for (int vc = 0; vc < XVC; vc++) {
                    int c = xScrC + vc;
                    if (c >= XC) break;
                    int x = XHDRW + vc * XCOLW;
                    rect(x, y, 1, XROWH, Col{215, 215, 215});
                    bool isSel = (r == xr && c == xc);
                    std::string d = (isSel && xEditing) ? xEdit : cellDisplay(r, c);
                    SDL_Rect cr = {x + 1, y + 1, XCOLW - 1, XROWH - 1};
                    SDL_RenderSetClipRect(gR, &cr);
                    if (isSel && xEditing) rect(cr.x, cr.y, cr.w, cr.h, WHITE);
                    if (looksNumber(d) && !(isSel && xEditing)) text(d, F22, x + XCOLW - 8, y + 6, DARK, 2);
                    else text(d, F22, x + 8, y + 6, DARK);
                    SDL_RenderSetClipRect(gR, nullptr);
                }
            }
            if (xr >= xScrR && xr < xScrR + rows && xc >= xScrC && xc < xScrC + XVC)
                outline(XHDRW + (xc - xScrC) * XCOLW, XTOP + (xr - xScrR) * XROWH, XCOLW + 1, XROWH + 1, 3, green);
            break;
        }
        case A_STORE: drawStore(th, bottom); break;
        case A_BROWSER: drawBrowser(bottom); break;
        case A_PAINT: drawPaint(); break;
        // @@DRAW@@
        case A_PHOTOS: {
            rect(0, 0, W, UIH, BLACK);
            if (phTex) {
                float s = std::min((float)W / phW, (float)UIH / phH);
                int dw = (int)(phW * s), dh = (int)(phH * s);
                SDL_Rect d = {(W - dw) / 2, (UIH - dh) / 2, dw, dh};
                SDL_RenderCopy(gR, phTex, nullptr, &d);
            } else if (gPhotos.empty()) {
                text("No photos found", F44, W / 2, 240, WHITE, 1);
                text("Looked in Nintendo/Album, Pictures and switch/WinNX/photos on your SD card.", F22, W / 2, 310, WHITE, 1, 170);
            } else text("Couldn't open this picture", F30, W / 2, 300, WHITE, 1);
            if (!gPhotos.empty()) {
                rect(0, UIH - 40, W, 40, BLACK, 150);
                size_t s = gPhotos[phIdx].find_last_of('/');
                text(std::to_string(phIdx + 1) + " / " + std::to_string(gPhotos.size()) + "   " + gPhotos[phIdx].substr(s + 1), F16, 16, UIH - 31, WHITE, 0, 220);
                text("Swipe or tap the sides", F16, W - 16, UIH - 31, WHITE, 2, 140);
            }
            break;
        }
        case A_GAMES: {
            rect(0, 0, W, UIH, bg);
            appTitle("Games");
            text("Tap to launch homebrew from /switch", F22, 76, 124, WHITE, 0, 190);
            int n = (int)gGames.size();
            int maxS = std::max(0, n * 70 - (UIH - 170));
            gamesScroll = std::max(0, std::min(gamesScroll, maxS));
            SDL_Rect clip = {0, 170, W, UIH - 170};
            SDL_RenderSetClipRect(gR, &clip);
            for (int i = 0; i < n; i++) {
                int y = 170 + i * 70 - gamesScroll;
                if (y + 64 < 170 || y > UIH) continue;
                rect(70, y, 1140, 64, WHITE, 28);
                text(gGames[i].name, F30, 90, y + 14, WHITE);
                text(gGames[i].path.substr(5), F16, 1190, y + 22, WHITE, 2, 150);
            }
            SDL_RenderSetClipRect(gR, nullptr);
            if (n == 0) text("No .nro files found in /switch", F30, 76, 200, WHITE, 0, 200);
            break;
        }
        default: {
            rect(0, 0, W, UIH, bg);
            appTitle(appName(curApp).c_str());
            const char* msg = "This app isn't available yet.";
            if (curApp == A_MAIL || curApp == A_WEATHER) msg = "Needs an internet connection. Coming in a later version.";
            if (curApp == A_MUSIC) msg = "Audio playback is coming in a later version.";
            text(msg, F30, 70, 190, WHITE);
            text("Press + or tap the Windows button to go back.", F22, 70, 240, WHITE, 0, 180);
            drawIcon(curApp, W - 200, 200, bg);
            break;
        }
    }
}

static void drawTopBar() {
    if (barP < 0.01f) return;
    int y = (int)(-44 + 44 * barP);
    rect(0, y, W, 44, Col{18, 18, 20}, 238);
    text(appName(curApp), F22, 20, y + 8, WHITE);
    rect(W - 150, y + 21, 40, 3, WHITE);
    rect(W - 90, y, 90, 44, Col{196, 43, 28});
    line(W - 58, y + 13, W - 32, y + 31, 3, WHITE);
    line(W - 58, y + 31, W - 32, y + 13, 3, WHITE);
}

static void appTap(int x, int y) {
    switch (curApp) {
        case A_CALC:
            for (int i = 0; i < NCB; i++) {
                SDL_Rect r = calcRect(CBTNS[i]);
                if (inR(x, y, r.x, r.y, r.w, r.h)) { calcPress(CBTNS[i].l); break; }
            }
            break;
        case A_SETTINGS:
            for (int i = 0; i < 4; i++) {
                int sx = 70 + i * 110;
                if (inR(x, y, sx, 215, 90, 90)) { gTheme = i; saveCfg(); }
            }
            if (y >= 320 && y < 430) { gBurn = !gBurn; saveCfg(); }
            else if (y >= 440 && y < 540) { g24 = !g24; saveCfg(); }
            break;
        case A_POWER:
            if (inR(x, y, 70, 300, 260, 90)) gRun = false;
            if (inR(x, y, 350, 300, 260, 90)) closeApp(false);
            break;
        case A_DESKTOP:
            for (auto& c : deskChips())
                if (inR(x, y, c.r.x, c.r.y, c.r.w, c.r.h)) { pendingTile = tileIndexFor(c.app); closeApp(false); return; }
            break;
        case A_NOTES:
            if (inR(x, y, 1110, 42, 100, 42)) { backupFile(DIR_ + "notes.txt"); gNote.s.clear(); gNote.dirty = true; }
            else if (inR(x, y, 70, 100, 1140, areaBottom() - 116)) openKb(1);
            break;
        case A_WORD: wordTap(x, y); break;
        case A_EXCEL:
            if (inR(x, y, 1000, 40, 90, 38)) {
                commitCell(); saveAll();
                backupFile(DIR_ + "sheet1.csv");
                for (int r = 0; r < XR; r++) for (int c = 0; c < XC; c++) xs[r][c].clear();
                xr = xc = xScrR = xScrC = 0; xDirty = true;
            } else if (inR(x, y, 1100, 40, 90, 38)) { commitCell(); xDirty = true; saveAll(); }
            else if (y >= 90 && y < 134) { openKb(3); startEdit(); }
            else if (x >= XHDRW && y >= XTOP) {
                int vc = (x - XHDRW) / XCOLW, vr = (y - XTOP) / XROWH;
                int r = xScrR + vr, c = xScrC + vc;
                if (vr < xRows() && vc < XVC && r < XR && c < XC) {
                    if (r == xr && c == xc) { openKb(3); startEdit(); }
                    else { commitCell(); xr = r; xc = c; xEnsureVisible(); }
                }
            }
            break;
        case A_STORE: storeTap(x, y); break;
        case A_BROWSER: browserTap(x, y); break;
        case A_PAINT: paintTap(x, y); break;
        // @@TAP@@
        case A_PHOTOS:
            if (x < W / 3) photoNav(-1);
            else if (x > 2 * W / 3) photoNav(1);
            break;
        case A_GAMES: {
            if (y < 170) break;
            int i = (y - 170 + gamesScroll) / 70;
            if (i >= 0 && i < (int)gGames.size() && ((y - 170 + gamesScroll) % 70) < 64) {
                saveAll();
                if (R_SUCCEEDED(envSetNextLoad(gGames[i].path.c_str(), gGames[i].path.c_str()))) gRun = false;
            }
            break;
        }
        default: break;
    }
}
static void appDrag(int dx, int dy) {
    static int accX = 0, accY = 0;
    switch (curApp) {
        case A_NOTES: gNote.scroll -= dy; break;
        case A_WORD: if (wView == 1) wPicScroll -= dy; else wScroll -= dy; break;
        case A_GAMES: gamesScroll -= dy; break;
        case A_STORE: if (storeView == 0) storeScroll -= dy; break;
        case A_EXCEL: {
            accX += dx; accY += dy;
            int sr = accY / XROWH, sc = accX / (XCOLW / 2);
            if (sr) { xScrR = std::max(0, std::min(XR - 1, xScrR - sr)); accY -= sr * XROWH; }
            if (sc) { xScrC = std::max(0, std::min(XC - XVC, xScrC - sc)); accX -= sc * (XCOLW / 2); }
            break;
        }
        default: accX = accY = 0; break;
    }
}

// ---------------------------------------------------------------- main
int main(int argc, char** argv) {
    (void)argc; (void)argv;
    plInitialize(PlServiceType_User);
    timeInitialize();
    psmInitialize();
    accountInitialize(AccountServiceType_Application);
    padConfigureInput(1, HidNpadStyleSet_NpadStandard);
    padInitializeDefault(&gPad);
    hidInitializeTouchScreen();
    mkdir("sdmc:/switch", 0777);
    mkdir("sdmc:/switch/WinNX", 0777);
    loadCfg();
    loadUser();
    rebuildTiles();

    SDL_Init(SDL_INIT_VIDEO);
    TTF_Init();
    IMG_Init(IMG_INIT_JPG | IMG_INIT_PNG);
    gWin = SDL_CreateWindow("WinNX", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, W, H, 0);
    gR = SDL_CreateRenderer(gWin, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC | SDL_RENDERER_TARGETTEXTURE);
    SDL_SetRenderDrawBlendMode(gR, SDL_BLENDMODE_BLEND);
    gTarget = SDL_CreateTexture(gR, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, W, H);

    PlFontData fd;
    if (R_SUCCEEDED(plGetSharedFontByType(&fd, PlSharedFontType_Standard))) {
        for (int i = 0; i < FCOUNT; i++) {
            SDL_RWops* rw = SDL_RWFromMem(fd.address, fd.size);
            gFont[i] = TTF_OpenFontRW(rw, 0, FSZ[i]);
        }
    }

    updateNow();
    lastInput = SDL_GetTicks();
    Uint32 prev = lastInput, lastClock = 0, lastBat = 0, lastSave = 0, kbHeldT = 0, kbRep = 0;
    bool wasDown = false, tMoved = false, tPressWin = false, tTop = false, tKb = false, tApp = false;
    int tDX = 0, tDY = 0, tLX = 0, tLY = 0, pressTile = -1, kbHeld = -1;
    static const int SH[5][2] = {{0, 0}, {2, 1}, {-2, 2}, {1, -2}, {-1, -1}};

    while (appletMainLoop() && gRun) {
        Uint32 now = SDL_GetTicks();
        float dt = std::min(0.05f, (now - prev) / 1000.0f);
        prev = now;
        introT += dt;

        SDL_Event ev;
        while (SDL_PollEvent(&ev)) if (ev.type == SDL_QUIT) gRun = false;

        if (now - lastClock > 250) { updateNow(); lastClock = now; }
        if (now - lastBat > 5000 || lastBat == 0) { psmGetBatteryChargePercentage(&gBat); lastBat = now; }
        if (now - lastSave > 1000) { saveAll(); lastSave = now; } // autosave

        // ---- buttons
        padUpdate(&gPad);
        u64 kd = padGetButtonsDown(&gPad);
        if (kd) { lastInput = now; dimA = 0; }
        if (kd & HidNpadButton_Plus) winPress();
        if (curApp < 0 && appDir == 0) {
            if (kd & HidNpadButton_AnyLeft) { navMode = true; navigate(0); }
            if (kd & HidNpadButton_AnyRight) { navMode = true; navigate(1); }
            if (kd & HidNpadButton_AnyUp) { navMode = true; navigate(2); }
            if (kd & HidNpadButton_AnyDown) { navMode = true; navigate(3); }
            if ((kd & HidNpadButton_A) && navMode) launch(sel);
        } else if (curApp >= 0 && appDir == 0) {
            if (kd & HidNpadButton_B) closeApp(false);
            if (kd & HidNpadButton_X) closeApp(true);
            if (curApp == A_PHOTOS) {
                if (kd & HidNpadButton_AnyLeft) photoNav(-1);
                if (kd & HidNpadButton_AnyRight) photoNav(1);
            }
        }

        // ---- touch
        HidTouchScreenState ts = {0};
        hidGetTouchScreenStates(&ts, 1);
        bool down = ts.count > 0;
        int tx = down ? (int)ts.touches[0].x : tLX, ty = down ? (int)ts.touches[0].y : tLY;
        bool appOpen = curApp >= 0 && appDir == 0;
        if (down && !wasDown) {
            lastInput = now; dimA = 0; navMode = false;
            tDX = tx; tDY = ty; tLX = tx; tLY = ty; tMoved = false; scrollV = 0;
            tTop = false; tKb = false; tApp = false; kbHeld = -1;
            tPressWin = inWinBtn(tx, ty);
            pressTile = (curApp < 0 && appDir == 0 && !tPressWin) ? hitTile(tx, ty) : -1;
            if (appOpen && !tPressWin) {
                if (ty < 30) { barT = 3.5f; tTop = true; }
                else if (barP > 0.5f && ty < 44) tTop = true;
                else if (kbOpen && ty >= KBTOP && ty < UIH) {
                    buildKeys();
                    int k = kbHit(tx, ty);
                    if (k >= 0) { kbHeld = k; kbHeldT = now; kbRep = now; kbAction(gKeys[k].act); }
                    tKb = true;
                } else { tApp = true; appTouch(0, tx, ty); }
            }
        } else if (down && wasDown) {
            lastInput = now;
            int dx = tx - tLX, dy = ty - tLY;
            if (!tMoved && (abs(tx - tDX) > 14 || abs(ty - tDY) > 14)) tMoved = true;
            if (tKb && kbHeld >= 0 && kbHeld < (int)gKeys.size() && gKeys[kbHeld].act == "BS" && now - kbHeldT > 450 && now - kbRep > 70) {
                kbAction("BS"); kbRep = now;
            }
            if (tApp && tMoved) appTouch(1, tx, ty);
            if (tMoved && !tKb && !tPressWin) {
                if (curApp < 0 && appDir == 0) {
                    scrollX = clampf(scrollX - dx, 0, maxScroll);
                    scrollV = (float)-dx;
                    pressTile = -1;
                } else if (appOpen && !tTop) appDrag(dx, dy);
            }
            tLX = tx; tLY = ty;
        } else if (!down && wasDown) {
            if (tApp) { appTouch(2, tLX, tLY); tApp = false; }
            if (tKb) { kbHeld = -1; }
            else if (tTop) {
                if (!tMoved && barP > 0.5f && tLY < 44 && appDir == 0) {
                    if (tLX >= W - 90) closeApp(true);
                    else if (tLX >= W - 180) closeApp(false);
                }
            } else if (!tMoved && appDir == 0) {
                if (inWinBtn(tLX, tLY)) winPress();
                else if (curApp < 0) { int t = hitTile(tLX, tLY); if (t >= 0) { sel = t; launch(t); } }
                else appTap(tLX, tLY);
            } else if (tMoved && appOpen && curApp == A_PHOTOS) {
                int dx = tLX - tDX;
                if (abs(dx) > 120) photoNav(dx < 0 ? 1 : -1);
            }
            tPressWin = false; pressTile = -1; tTop = false;
        }
        wasDown = down;

        // ---- updates
        if (!down && scrollV != 0) {
            scrollX += scrollV; scrollV *= 0.93f;
            if (fabsf(scrollV) < 0.3f) scrollV = 0;
            if (scrollX < 0 || scrollX > maxScroll) { scrollX = clampf(scrollX, 0, maxScroll); scrollV = 0; }
        }
        if (appDir != 0) {
            appT += appDir * dt * 3.6f;
            if (appT >= 1) { appT = 1; appDir = 0; }
            else if (appT <= 0) {
                appT = 0; appDir = 0;
                if (gCloseReset) appReset(curApp);
                gCloseReset = false;
                curApp = -1;
            }
        }
        if (appDir == 0 && curApp < 0 && pendingTile >= 0) { int t = pendingTile; pendingTile = -1; launch(t); }
        for (int a = 0; a < A_COUNT; a++) {
            if (storeProg[a] > 0) {
                storeProg[a] += dt * 0.7f;
                if (storeProg[a] >= 1) { storeProg[a] = 0; gInst[a] = true; rebuildTiles(); saveCfg(); }
            }
        }
        appUpdate(dt);
        if (barT > 0) barT -= dt;
        float barTarget = (barT > 0 && curApp >= 0 && appDir == 0) ? 1.0f : 0.0f;
        barP += (barTarget - barP) * std::min(1.0f, dt * 14.0f);
        if (gTC.size() > 300) { for (auto& p : gTC) SDL_DestroyTexture(p.second.t); gTC.clear(); }

        // ---- burn-in protection
        float idle = (now - lastInput) / 1000.0f;
        float target = 0;
        if (gBurn) target = idle > 300 ? 215.0f : (idle > 60 ? 135.0f : 0.0f);
        dimA += (target - dimA) * 0.06f;
        int sx = 0, sy = 0;
        if (gBurn) { int k = (int)((now / 30000) % 5); sx = SH[k][0]; sy = SH[k][1]; }

        // ---- render
        if (gTarget) SDL_SetRenderTarget(gR, gTarget);
        SDL_SetRenderDrawColor(gR, 0, 0, 0, 255);
        SDL_RenderClear(gR);
        drawStart(down, tMoved, pressTile);
        if (curApp >= 0) {
            if (appT < 1) {
                SDL_Rect a = tileScreen(animTile < 0 ? 0 : animTile);
                float e = ease(appT);
                int rx = (int)(a.x + (0 - a.x) * e), ry = (int)(a.y + (0 - a.y) * e);
                int rw = (int)(a.w + (W - a.w) * e), rh = (int)(a.h + (UIH - a.h) * e);
                rect(rx, ry, rw, rh, appBg(curApp));
            } else {
                drawApp();
                if (kbOpen) drawKb();
                drawTopBar();
            }
        }
        rect(0, UIH, W, H - UIH, BLACK);
        bool wb = down && tPressWin;
        winLogo(W / 2, UIH + (H - UIH) / 2, 26, WHITE, wb ? 255 : 105);
        if (dimA > 1) rect(0, 0, W, H, BLACK, (int)dimA);

        if (gTarget) {
            SDL_SetRenderTarget(gR, nullptr);
            SDL_SetRenderDrawColor(gR, 0, 0, 0, 255);
            SDL_RenderClear(gR);
            SDL_Rect dst = {sx, sy, W, H};
            SDL_RenderCopy(gR, gTarget, nullptr, &dst);
        }
        SDL_RenderPresent(gR);
    }

    commitCell();
    saveAll();
    saveCfg();
    if (phTex) SDL_DestroyTexture(phTex);
    for (auto& p : gTC) SDL_DestroyTexture(p.second.t);
    for (int i = 0; i < FCOUNT; i++) if (gFont[i]) TTF_CloseFont(gFont[i]);
    if (gTarget) SDL_DestroyTexture(gTarget);
    SDL_DestroyRenderer(gR);
    SDL_DestroyWindow(gWin);
    IMG_Quit();
    TTF_Quit();
    SDL_Quit();
    accountExit();
    psmExit();
    timeExit();
    plExit();
    return 0;
}
