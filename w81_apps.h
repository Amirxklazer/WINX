// w81_apps.h - Task Manager, Skype, OneDrive (included from win81.h)

static const Col W9_BLUE = {0, 114, 198};
static const Col W9_TXT = {40, 42, 48};
static const Col W9_GREY = {120, 124, 132};

// ================================================================ Task Manager (Windows 8.1 "more details" look)
static int tmTab = 0, tmSel = -1, tmPerf = 0, tmScroll = 0;
static float tmHCpu[60], tmHMem[60]; static int tmHN = 0; static Uint32 tmLastS = 0;
static float tmUsedMB() {
#ifndef WNX_HOST
    u64 used = 0; if (R_SUCCEEDED(svcGetInfo(&used, InfoType_UsedMemorySize, CUR_PROCESS_HANDLE, 0))) return (float)(used / 1048576.0);
    return 0;
#else
    return 180.0f + (SDL_GetTicks() % 1000) * 0.01f;
#endif
}
static void tmSample() {
    Uint32 now = SDL_GetTicks(); if (tmLastS && now - tmLastS < 1000) return; tmLastS = now;
    gTmCpu = clampf(gFrameBusyMs / 16.7f * 100.0f, 1, 100);
    gTmMemUsedMB = tmUsedMB();
#ifndef WNX_HOST
    u64 total = 0; if (R_SUCCEEDED(svcGetInfo(&total, InfoType_TotalMemorySize, CUR_PROCESS_HANDLE, 0))) gTmMemTotalMB = (float)(total / 1048576.0);
#else
    gTmMemTotalMB = 400;
#endif
    if (tmHN < 60) { tmHCpu[tmHN] = gTmCpu; tmHMem[tmHN] = gTmMemUsedMB; tmHN++; }
    else { memmove(tmHCpu, tmHCpu + 1, 59 * sizeof(float)); memmove(tmHMem, tmHMem + 1, 59 * sizeof(float)); tmHCpu[59] = gTmCpu; tmHMem[59] = gTmMemUsedMB; }
}
struct TmProc { std::string name; int app; float cpu, mem; bool isApp; };
static std::vector<TmProc> tmProcs() {
    std::vector<TmProc> v; float sum = 0;
    for (int a = 0; a < A_COUNT; a++) {
        if (!running[a]) continue;
        float mem = gAppMemMB[a] > 0 ? gAppMemMB[a] : 1.0f; sum += mem;
        v.push_back({ADEF[a].name, a, a == curApp ? std::max(0.0f, gTmCpu - 3.0f) : 0.0f, mem, true});
    }
    float other = std::max(8.0f, gTmMemUsedMB - sum);
    v.push_back({"Windows shell (Start screen)", -1, std::min(3.0f, gTmCpu), other * 0.62f, false});
    if (musAudioOk) v.push_back({"Audio engine", -2, 0.0f, std::min(30.0f, other * 0.12f), false});
    v.push_back({"Network service", -3, 0.0f, std::min(20.0f, other * 0.10f), false});
    v.push_back({"Reminder service", -4, 0.0f, std::min(14.0f, other * 0.08f), false});
    return v;
}
static Col tmHeat(float pct) { return mix(WHITE, Col{249, 203, 90}, clampf(pct / 50.0f, 0, 1)); }
static void tmGraph(int x, int y, int w, int h, const float* d, int n, float maxv, Col c) {
    rect(x, y, w, h, Col{241, 246, 250});
    for (int i = 1; i < 10; i++) rect(x, y + i * h / 10, w, 1, Col{217, 234, 244});
    for (int i = 1; i < 6; i++) rect(x + i * w / 6, y, 1, h, Col{217, 234, 244});
    SDL_Rect clip{x, y, w, h}; SDL_RenderSetClipRect(gR, &clip);
    for (int i = 0; i < n; i++) {
        int px = x + w - (n - 1 - i) * w / 60 - 1; int ph = (int)(clampf(d[i] / maxv, 0, 1) * (h - 2));
        rect(px - w / 60, y + h - ph, w / 60 + 1, ph, c, 60);
        if (i > 0) { int ppx = x + w - (n - i) * w / 60 - 1; int pph = (int)(clampf(d[i - 1] / maxv, 0, 1) * (h - 2)); line(ppx, y + h - pph, px, y + h - ph, 2, c); }
    }
    SDL_RenderSetClipRect(gR, nullptr);
    outline(x, y, w, h, 2, c);
}
static void drawTaskMgr() {
    tmSample();
    rect(0, 0, W, UIH, WHITE);
    rect(0, 0, W, 64, Col{243, 243, 245});
    text("Task Manager", F30, 30, 14, W9_TXT);
    static const char* TN[2] = {"Processes", "Performance"};
    int tx = 330; for (int i = 0; i < 2; i++) { text(TN[i], F22, tx, 20, i == tmTab ? W9_BLUE : W9_GREY); if (i == tmTab) rect(tx, 56, tw(TN[i], F22), 4, W9_BLUE); tx += tw(TN[i], F22) + 50; }
    text("Estimates: Switch gives apps no per-process counters", F16, W - 24, 24, W9_GREY, 2);
    if (tmTab == 0) {
        std::vector<TmProc> pr = tmProcs();
        static const int CX[4] = {800, 920, 1040, 1160};   // right edges of CPU, Memory, Disk, Network
        rect(0, 64, W, 78, Col{250, 250, 252});
        text("Name", F16, 60, 112, W9_GREY); text("Status", F16, 500, 112, W9_GREY);
        char b[48];
        snprintf(b, sizeof b, "%.0f%%", gTmCpu); text(b, F30, CX[0], 70, W9_TXT, 2); text("CPU", F16, CX[0], 112, W9_GREY, 2);
        snprintf(b, sizeof b, "%.0f%%", gTmMemTotalMB > 0 ? gTmMemUsedMB / gTmMemTotalMB * 100 : 0.0f); text(b, F30, CX[1], 70, W9_TXT, 2); text("Memory", F16, CX[1], 112, W9_GREY, 2);
        text("0%", F30, CX[2], 70, W9_TXT, 2); text("Disk", F16, CX[2], 112, W9_GREY, 2);
        text("0%", F30, CX[3], 70, W9_TXT, 2); text("Network", F16, CX[3], 112, W9_GREY, 2);
        int top = 144, bot = UIH - (tmSel != -99 ? 80 : 0);
        SDL_Rect clip{0, top, W, bot - top}; SDL_RenderSetClipRect(gR, &clip);
        int y = top - tmScroll, total = 0;
        auto group = [&](const char* nm, bool apps) {
            int cnt = 0; for (auto& p : pr) if (p.isApp == apps) cnt++;
            if (y + 44 > top && y < bot) text(std::string(nm) + " (" + std::to_string(cnt) + ")", F22, 40, y + 8, W9_BLUE);
            y += 44; total += 44;
            for (auto& p : pr) {
                if (p.isApp != apps) continue;
                if (y + 46 > top && y < bot) {
                    if (p.isApp && p.app == tmSel) rect(0, y, W, 46, Col{205, 232, 255});
                    Col ic = p.isApp ? appBg(p.app) : Col{110, 116, 128}; rect(60, y + 9, 28, 28, ic);
                    text(fitTxt(p.name, F22, 400), F22, 104, y + 8, W9_TXT);
                    text(p.isApp ? (p.app == curApp ? "Running" : "Suspended") : "", F16, 500, y + 14, W9_GREY);
                    rect(CX[0] - 118, y, 120, 46, tmHeat(p.cpu * 2)); snprintf(b, sizeof b, "%.0f%%", p.cpu); text(b, F22, CX[0], y + 8, W9_TXT, 2);
                    rect(CX[1] - 118, y, 120, 46, tmHeat(p.mem / 3)); snprintf(b, sizeof b, "%.1f MB", p.mem); text(b, F22, CX[1], y + 8, W9_TXT, 2);
                    rect(CX[2] - 118, y, 120, 46, WHITE); text("0 MB/s", F22, CX[2], y + 8, W9_TXT, 2);
                    rect(CX[3] - 118, y, 120, 46, WHITE); text("0 Mbps", F22, CX[3], y + 8, W9_TXT, 2);
                    rect(60, y + 45, W - 120, 1, Col{235, 235, 238});
                }
                y += 46; total += 46;
            }
        };
        group("Apps", true); group("Background processes", false);
        int mx = std::max(0, total - (bot - top)); tmScroll = std::max(0, std::min(tmScroll, mx));
        SDL_RenderSetClipRect(gR, nullptr);
        rect(0, UIH - 80, W, 80, Col{243, 243, 245});
        bool canEnd = tmSel >= 0 && running[tmSel];
        btn(W - 230, UIH - 66, 200, 52, "End task", canEnd ? W9_BLUE : Col{190, 190, 196}, WHITE);
        text(canEnd ? std::string("Selected: ") + ADEF[tmSel].name : "Tap an app to select it", F22, 40, UIH - 54, W9_GREY);
    } else {
        // left list
        const char* nm[4] = {"CPU", "Memory", "Disk (SD card)", "Wi-Fi"};
        char sub[4][64]; fmStorage();
        snprintf(sub[0], 64, "%.0f%%", gTmCpu); snprintf(sub[1], 64, "%.0f/%.0f MB", gTmMemUsedMB, gTmMemTotalMB);
        snprintf(sub[2], 64, "%.0f%% used", fmTotalB > 0 ? (1.0f - (float)fmFree / fmTotalB) * 100 : 0.0f); snprintf(sub[3], 64, "%s", netUp() ? "Connected" : "Not connected");
        for (int i = 0; i < 4; i++) {
            int y = 76 + i * 128; if (i == tmPerf) rect(0, y - 6, 410, 120, Col{229, 241, 251});
            float d[60]; int n = tmHN;
            if (i == 0) memcpy(d, tmHCpu, sizeof d); else if (i == 1) memcpy(d, tmHMem, sizeof d); else { for (int k = 0; k < 60; k++) d[k] = i == 2 ? (fmTotalB > 0 ? (1.0f - (float)fmFree / fmTotalB) * 100 : 0) : (netUp() ? 60.0f : 0.0f); n = 60; }
            tmGraph(24, y, 130, 104, d, n, i == 1 ? std::max(1.0f, gTmMemTotalMB) : 100.0f, W9_BLUE);
            text(nm[i], F22, 172, y + 8, W9_TXT); text(sub[i], F16, 172, y + 44, W9_GREY);
        }
        // detail
        int dx = 440, dw = W - dx - 40;
        text(nm[tmPerf], F44, dx, 76, W9_TXT);
        const char* what[4] = {"Frame-load estimate", "Memory used by Windows", "SD card usage", "Wi-Fi activity"};
        text(what[tmPerf], F16, W - 40, 106, W9_GREY, 2);
        float d[60]; int n = tmHN;
        if (tmPerf == 0) memcpy(d, tmHCpu, sizeof d); else if (tmPerf == 1) memcpy(d, tmHMem, sizeof d);
        else { for (int k = 0; k < 60; k++) d[k] = tmPerf == 2 ? (fmTotalB > 0 ? (1.0f - (float)fmFree / fmTotalB) * 100 : 0) : (netUp() ? 60.0f : 0.0f); n = 60; }
        tmGraph(dx, 150, dw, 300, d, n, tmPerf == 1 ? std::max(1.0f, gTmMemTotalMB) : 100.0f, W9_BLUE);
        text("60 seconds", F16, dx, 456, W9_GREY); text(tmPerf == 1 ? (std::to_string((int)gTmMemTotalMB) + " MB") : "100%", F16, dx + dw, 456, W9_GREY, 2);
        int y = 496; char b[96];
        auto stat = [&](int col, const char* k, const std::string& v) { int x = dx + col * 280; text(k, F16, x, y, W9_GREY); text(v, F30, x, y + 22, W9_TXT); };
        int up = (int)(SDL_GetTicks() / 1000);
        if (tmPerf == 0) { snprintf(b, sizeof b, "%.0f%%", gTmCpu); stat(0, "Utilization", b); snprintf(b, sizeof b, "%.1f ms", gFrameBusyMs); stat(1, "Frame time", b); int pc = 0; for (int a = 0; a < A_COUNT; a++) if (running[a]) pc++; stat(2, "Apps running", std::to_string(pc));
            snprintf(b, sizeof b, "%d:%02d:%02d", up / 3600, (up / 60) % 60, up % 60); stat(0, "Up time", b); y += 70; snprintf(b, sizeof b, "%.0f%%", gTmCpu); }
        else if (tmPerf == 1) { snprintf(b, sizeof b, "%.0f MB", gTmMemUsedMB); stat(0, "In use", b); snprintf(b, sizeof b, "%.0f MB", std::max(0.0f, gTmMemTotalMB - gTmMemUsedMB)); stat(1, "Available", b); snprintf(b, sizeof b, "%.0f MB", gTmMemTotalMB); stat(2, "Total for this app", b); }
        else if (tmPerf == 2) { stat(0, "Capacity", fmSize(fmTotalB)); stat(1, "Free", fmSize(fmFree)); stat(2, "Used", fmSize(std::max(0LL, fmTotalB - fmFree))); }
        else { stat(0, "Status", netUp() ? "Connected" : (gOffline ? "Offline mode" : "Not connected")); stat(1, "Signal bars", std::to_string(gWifiBars)); }
    }
}
static void taskMgrTap(int x, int y) {
    if (y < 64) { int tx = 330; for (int i = 0; i < 2; i++) { int w = tw(i ? "Performance" : "Processes", F22); if (inR(x, y, tx - 10, 0, w + 20, 64)) { tmTab = i; return; } tx += w + 50; } return; }
    if (tmTab == 0) {
        if (y >= UIH - 80) {
            if (inR(x, y, W - 230, UIH - 66, 200, 52) && tmSel >= 0 && running[tmSel]) { int a = tmSel; tmSel = -1; if (a == A_TASKMGR) { appReset(a); closeApp(false); } else appReset(a); }
            return;
        }
        if (y < 144) return;
        std::vector<TmProc> pr = tmProcs(); int yy = 144 - tmScroll + 44;
        for (auto& p : pr) { if (!p.isApp) continue; if (y >= yy && y < yy + 46) { tmSel = (tmSel == p.app) ? -1 : p.app; return; } yy += 46; }
    } else {
        for (int i = 0; i < 4; i++) if (inR(x, y, 0, 70 + i * 128, 410, 120)) tmPerf = i;
    }
}
static void taskMgrDrag(int, int dy) { if (tmTab == 0) tmScroll = std::max(0, tmScroll - dy); }

// ================================================================ Skype (Windows 8.1 look). Contacts and chats live on this console; there is no Skype network.
struct SkyMsg { bool me; std::string t, when; };
struct SkyContact { int kind; std::string name; int status; int col; std::string mood; int unread = 0; std::vector<SkyMsg> msgs; };
struct SkyPend { Uint32 due; int contact; std::string text; };
static std::vector<SkyContact> gSky; static std::vector<SkyPend> skyPend;
static int skySel = -1, skyScroll = 0, skyListScroll = 0, skyMy = 0, skyCall = 0, skyDownX = 0; static Uint32 skyCallT = 0;
static bool skyLoaded = false, skyEmoji = false, skyStatusMenu = false, skyMute = false, skyAdding = false;
static std::string skyIn, skyAddName;
static const Col SKY_BLUE = {0, 175, 240};
static const Col SKY_COLS[8] = {{0, 175, 240}, {226, 80, 80}, {80, 170, 90}, {240, 150, 30}, {150, 90, 200}, {20, 150, 160}, {200, 80, 150}, {90, 100, 120}};
static const char* SKY_ST[4] = {"Online", "Away", "Busy", "Invisible"};
static Col skyStCol(int st) { return st == 0 ? Col{60, 180, 75} : (st == 1 ? Col{240, 190, 40} : (st == 2 ? Col{220, 60, 60} : Col{150, 150, 156})); }
static int skyUnread() { int n = 0; for (auto& c : gSky) n += c.unread; return n; }
static void skySave() {
    std::string o = "S\t" + std::to_string(skyMy) + "\n"; int ci = 0;
    for (auto& c : gSky) {
        o += "C\t" + std::to_string(c.kind) + "\t" + w9Clean(c.name) + "\t" + std::to_string(c.status) + "\t" + std::to_string(c.col) + "\t" + w9Clean(c.mood) + "\t" + std::to_string(c.unread) + "\n";
        size_t st = c.msgs.size() > 200 ? c.msgs.size() - 200 : 0;
        for (size_t i = st; i < c.msgs.size(); i++) o += "M\t" + std::to_string(ci) + "\t" + (c.msgs[i].me ? "1" : "0") + "\t" + w9Clean(c.msgs[i].when) + "\t" + w9Esc(c.msgs[i].t) + "\n";
        ci++;
    }
    writeFile(DIR_ + "skype.txt", o);
}
static void skyPost(int ci, bool me, const std::string& t) { if (ci < 0 || ci >= (int)gSky.size()) return; gSky[ci].msgs.push_back({me, t, timeStr(true)}); gSkyDirty = true; }
static void skyDefaults() {
    gSky.clear();
    gSky.push_back({1, "Echo / Sound Test Service", 0, 2, "Call me to test your call screen", 0, {}});
    gSky.push_back({3, "Cortana", 0, 0, "Your personal assistant", 0, {}});
    gSky.push_back({2, "Skype", 0, 0, "Welcome to Skype", 1, {}});
    skyPost(2, false, "Welcome to Skype on Windows 8.1! Contacts and chats are kept on this console (there is no Skype network any more). Try the Echo / Sound Test Service or chat with Cortana.");
}
static void skyEnsure() {
    if (skyLoaded) return; skyLoaded = true;
    std::string d; if (!readFile(DIR_ + "skype.txt", d) || d.empty()) { skyDefaults(); skySave(); return; }
    for (auto& l : w9Split(d, '\n')) {
        std::vector<std::string> f = w9Split(l, '\t'); if (f.empty()) continue;
        if (f[0] == "S" && f.size() >= 2) skyMy = std::max(0, std::min(3, atoi(f[1].c_str())));
        else if (f[0] == "C" && f.size() >= 7) { SkyContact c; c.kind = atoi(f[1].c_str()); c.name = f[2]; c.status = atoi(f[3].c_str()); c.col = std::max(0, std::min(7, atoi(f[4].c_str()))); c.mood = f[5]; c.unread = atoi(f[6].c_str()); gSky.push_back(c); }
        else if (f[0] == "M" && f.size() >= 5) { int ci = atoi(f[1].c_str()); if (ci >= 0 && ci < (int)gSky.size()) gSky[ci].msgs.push_back({f[2] == "1", f[4].empty() ? "" : w9Unesc(f[4]), f[3]}); }
    }
    if (gSky.empty()) { skyDefaults(); skySave(); }
}
static std::string skyBotReply(int ci, const std::string& t) {
    SkyContact& c = gSky[ci];
    if (c.kind == 3) { std::string r = coReply(t); return r; }
    if (c.kind == 1) return "I'm the Echo / Sound Test Service. I only answer calls - tap the phone icon to try one.";
    if (c.kind == 2) return "This is an automatic message from Skype. Add contacts with the + button.";
    return "";
}
static void skySendEnter();
static void skySend() {
    std::string q = skyIn; skyIn.clear(); while (!q.empty() && q.back() == ' ') q.pop_back(); if (q.empty() || skySel < 0 || skySel >= (int)gSky.size()) return;
    skyPost(skySel, true, q); skyScroll = 0;
    if (gSky[skySel].kind != 0) skyPend.push_back({SDL_GetTicks() + 900 + (Uint32)(rand() % 800), skySel, q});
}
static void skySendEnter() { skySend(); openKbStr(&skyIn, 400, skySendEnter); }
static void skyAddEnter() {
    std::string n = skyAddName; skyAddName.clear(); skyAdding = false; hideKb();
    while (!n.empty() && n.back() == ' ') n.pop_back(); while (!n.empty() && n[0] == ' ') n.erase(0, 1);
    if (n.empty()) return;
    for (auto& c : gSky) if (c.name == n) { toast("Already in your contacts"); return; }
    SkyContact c; c.kind = 0; c.name = n; c.status = 3; c.col = (int)(gSky.size() % 8); c.mood = "Added just now"; gSky.push_back(c); skySel = (int)gSky.size() - 1; gSkyDirty = true;
}
static void skyInit() { skyEnsure(); skyCall = 0; skyStatusMenu = false; skyEmoji = false; skyAdding = false; if (skySel >= (int)gSky.size()) skySel = -1; }
static void skyUpdate() {
    if (skyPend.empty() && !skyCall) return;
    Uint32 now = SDL_GetTicks();
    for (size_t i = 0; i < skyPend.size();) {
        if (now >= skyPend[i].due) {
            SkyPend p = skyPend[i]; skyPend.erase(skyPend.begin() + i);
            if (p.contact >= 0 && p.contact < (int)gSky.size()) {
                std::string r = skyBotReply(p.contact, p.text);
                if (!r.empty()) { skyPost(p.contact, false, r); bool watching = curApp == A_SKYPE && skySel == p.contact; if (!watching) { gSky[p.contact].unread++; notify(A_SKYPE, gSky[p.contact].name, r); } skyScroll = 0; }
            }
        } else i++;
    }
    if (skyCall == 1 && now - skyCallT > 1800 && skySel >= 0 && skySel < (int)gSky.size()) {
        if (gSky[skySel].kind == 1) { skyCall = 2; skyCallT = now; sndPreview(gSndScheme == 3 ? 0 : gSndScheme, SND_NOTIFY); }
        else if (now - skyCallT > 6000) { skyCall = 3; skyCallT = now; }
    }
    if (skyCall == 3 && now - skyCallT > 2200) skyCall = 0;
}
static void skyAvatar(int cx, int cy, int r, const SkyContact& c, bool dot) {
    disc(cx, cy, r, SKY_COLS[c.col]);
    std::string in = c.kind == 1 ? "E" : (c.kind == 2 ? "S" : (c.name.empty() ? "?" : std::string(1, (char)toupper((unsigned char)c.name[0]))));
    text(in, r >= 40 ? F44 : F22, cx, cy - (r >= 40 ? 28 : 15), WHITE, 1);
    if (dot) { disc(cx + r * 7 / 10, cy + r * 7 / 10, r / 4 + 3, WHITE); disc(cx + r * 7 / 10, cy + r * 7 / 10, r / 4, skyStCol(c.kind ? 0 : c.status)); }
}
static void drawSkype() {
    skyEnsure();
    int bottom = areaBottom(); rect(0, 0, W, UIH, WHITE);
    // title bar
    rect(0, 0, W, 64, SKY_BLUE);
    w9Glyph(8, 40, 34, WHITE, SKY_BLUE); text("Skype", F30, 76, 14, WHITE);
    { SkyContact me; me.kind = 0; me.name = gUser; me.col = 0; me.status = skyMy; me.mood.clear();
      text(gUser, F22, W - 128, 8, WHITE, 2); text(SKY_ST[skyMy], F16, W - 128, 36, WHITE, 2, 220); skyAvatar(W - 64, 32, 24, me, true); }
    // contact list
    rect(0, 64, 380, bottom - 64, Col{245, 247, 249}); rect(380, 64, 1, bottom - 64, Col{220, 224, 228});
    text("Recent", F22, 24, 78, W9_TXT); rect(24, 108, 70, 3, SKY_BLUE);
    btn(300, 74, 60, 40, "+", SKY_BLUE, WHITE);
    SDL_Rect clip{0, 122, 380, bottom - 122}; SDL_RenderSetClipRect(gR, &clip);
    int n = (int)gSky.size(), maxS = std::max(0, n * 76 - (bottom - 122)); skyListScroll = std::max(0, std::min(skyListScroll, maxS));
    for (int i = 0; i < n; i++) {
        int y = 122 + i * 76 - skyListScroll; if (y + 76 < 122 || y > bottom) continue; SkyContact& c = gSky[i];
        if (i == skySel) rect(0, y, 380, 76, Col{214, 238, 251});
        skyAvatar(46, y + 38, 26, c, true);
        text(fitTxt(c.name, F22, 230), F22, 88, y + 10, W9_TXT);
        std::string pv = c.msgs.empty() ? c.mood : c.msgs.back().t; for (auto& ch : pv) if (ch == '\n') ch = ' ';
        text(fitTxt(pv, F16, 230), F16, 88, y + 42, W9_GREY);
        if (c.unread > 0) { disc(350, y + 24, 14, SKY_BLUE); text(std::to_string(std::min(99, c.unread)), F16, 350, y + 14, WHITE, 1); }
    }
    SDL_RenderSetClipRect(gR, nullptr);
    // right side
    int rx = 381, rw = W - rx;
    if (skySel < 0 || skySel >= n) {
        w9Glyph(8, rx + rw / 2, 250, Col{200, 232, 247}, WHITE); disc(rx + rw / 2, 250, 0, WHITE);
        text("Pick a conversation", F30, rx + rw / 2, 330, W9_GREY, 1); text("or add a contact with +", F22, rx + rw / 2, 376, W9_GREY, 1, 180);
    } else {
        SkyContact& c = gSky[skySel];
        skyAvatar(rx + 50, 100, 28, c, true);
        text(fitTxt(c.name, F30, rw - 380), F30, rx + 92, 72, W9_TXT);
        text(c.kind ? std::string("Online - ") + c.mood : std::string(SKY_ST[c.status]) + (c.status == 3 ? " (saved on this console)" : ""), F16, rx + 92, 112, W9_GREY);
        disc(W - 290, 98, 22, Col{60, 180, 75}); { Col g = WHITE; line(W - 298, 90, W - 292, 98, 5, g); line(W - 292, 98, W - 282, 106, 5, g); disc(W - 298, 90, 4, g); disc(W - 282, 106, 4, g); line(W - 298, 90, W - 282, 106, 3, Col{60, 180, 75}); disc(W - 291, 99, 3, Col{60, 180, 75}); }
        disc(W - 232, 98, 22, Col{150, 150, 156}); outline(W - 244, 91, 18, 14, 2, WHITE); tri(W - 226, 98, W - 217, 91, W - 217, 105, WHITE);
        text("Clear", F16, W - 190, 90, SKY_BLUE); if (c.kind == 0) text("Remove", F16, W - 120, 90, Col{200, 60, 60});
        rect(rx, 134, rw, 1, Col{225, 228, 232});
        int mt = 140, mb = bottom - (skyEmoji ? 126 : 78); SDL_Rect mc{rx, mt, rw, mb - mt}; SDL_RenderSetClipRect(gR, &mc);
        int y = mb + skyScroll; int bw = 560;
        if (!skyPend.empty()) { bool t = false; for (auto& p : skyPend) if (p.contact == skySel) t = true; if (t) { y -= 44; rect(rx + 20, y, 80, 36, Col{232, 234, 238}); for (int k = 0; k < 3; k++) disc(rx + 44 + k * 16, y + 18, (int)(3 + 1.5f * (0.5f + 0.5f * sinf(SDL_GetTicks() / 180.0f + k))), W9_GREY); y -= 8; } }
        for (int i = (int)c.msgs.size() - 1; i >= 0 && y > mt - 400; i--) {
            const SkyMsg& m = c.msgs[i]; auto lines = wrapText(m.t, F22, bw - 32);
            int tw0 = 0; for (auto& l : lines) tw0 = std::max(tw0, tw(l, F22)); int w = std::max(tw0 + 32, 110), h = (int)lines.size() * 30 + 38;
            y -= h + 10;
            if (y + h >= mt && y <= mb) {
                int x = m.me ? W - 30 - w : rx + 24;
                rect(x, y, w, h, m.me ? SKY_BLUE : Col{232, 234, 238});
                for (size_t k = 0; k < lines.size(); k++) text(lines[k], F22, x + 16, y + 6 + (int)k * 30, m.me ? WHITE : W9_TXT);
                text(m.when, F16, x + w - 12, y + h - 24, m.me ? WHITE : W9_GREY, 2, 190);
            }
        }
        SDL_RenderSetClipRect(gR, nullptr);
        // input
        int iy = bottom - 66; rect(rx, iy - 6, rw, 72, WHITE); rect(rx, iy - 6, rw, 1, Col{225, 228, 232});
        if (skyEmoji) { static const char* EM[8] = {":)", ":D", ";)", ":(", ":P", "<3", "(y)", ":O"}; for (int k = 0; k < 8; k++) { rect(rx + 70 + k * 74, iy - 52, 66, 40, Col{240, 242, 245}); text(EM[k], F22, rx + 103 + k * 74, iy - 46, W9_TXT, 1); } }
        rect(rx + 12, iy, 50, 56, Col{240, 242, 245}); disc(rx + 37, iy + 28, 14, Col{240, 190, 40}); disc(rx + 32, iy + 24, 2, W9_TXT); disc(rx + 42, iy + 24, 2, W9_TXT); line(rx + 30, iy + 33, rx + 44, iy + 33, 2, W9_TXT);
        drawField(rx + 70, iy, rw - 70 - 76, 56, &skyIn, "Type a message here", F22, W9_TXT, Col{250, 251, 252}, Col{200, 204, 210});
        rect(W - 64, iy, 52, 56, SKY_BLUE); tri(W - 52, iy + 14, W - 22, iy + 28, W - 52, iy + 42, WHITE);
        // call overlay
        if (skyCall) {
            rect(rx, 64, rw, bottom - 64, Col{0, 110, 160});
            skyAvatar(rx + rw / 2, 230, 70, c, false);
            text(c.name, F44, rx + rw / 2, 330, WHITE, 1);
            std::string st = skyCall == 1 ? "Calling..." : (skyCall == 2 ? "" : "No answer");
            if (skyCall == 2) { int s = (SDL_GetTicks() - skyCallT) / 1000; char b[32]; snprintf(b, sizeof b, "%02d:%02d", s / 60, s % 60); st = b; }
            text(st, F30, rx + rw / 2, 392, WHITE, 1, 220);
            if (skyCall == 2) text("Demo call: no microphone audio is sent anywhere.", F16, rx + rw / 2, 436, WHITE, 1, 170);
            disc(rx + rw / 2 - 90, 520, 34, skyMute ? Col{240, 190, 40} : Col{0, 150, 200}); w9Mic(rx + rw / 2 - 90, 520, WHITE);
            disc(rx + rw / 2 + 90, 520, 34, Col{220, 50, 50}); line(rx + rw / 2 + 76, 506, rx + rw / 2 + 104, 534, 4, WHITE); line(rx + rw / 2 + 76, 534, rx + rw / 2 + 104, 506, 4, WHITE);
        }
    }
    // status menu / add contact
    if (skyStatusMenu) { rect(W - 220, 64, 220, 4 * 52 + 8, WHITE); outline(W - 220, 64, 220, 4 * 52 + 8, 1, Col{200, 204, 210}); for (int i = 0; i < 4; i++) { disc(W - 190, 64 + 34 + i * 52, 9, skyStCol(i)); text(SKY_ST[i], F22, W - 168, 64 + 20 + i * 52, W9_TXT); } }
    if (skyAdding) { rect(0, 64, 380, 120, Col{255, 255, 255}); outline(0, 64, 380, 120, 1, Col{200, 204, 210}); text("Add a contact", F22, 20, 74, W9_TXT); drawField(20, 112, 250, 54, &skyAddName, "Name", F22, W9_TXT, Col{250, 251, 252}, Col{200, 204, 210}); btn(280, 112, 84, 54, "Add", SKY_BLUE, WHITE); }
}
static void skypeTap(int x, int y) {
    skyEnsure(); int bottom = areaBottom(); int n = (int)gSky.size();
    if (skyStatusMenu) { skyStatusMenu = false; if (x >= W - 220 && y >= 64) { int i = (y - 64) / 52; if (i >= 0 && i < 4) { skyMy = i; gSkyDirty = true; } return; } }
    if (y < 64) { if (x > W - 330) skyStatusMenu = true; return; }
    if (skyAdding) { if (inR(x, y, 280, 112, 84, 54)) { skyAddEnter(); return; } if (inR(x, y, 20, 112, 250, 54)) { openKbStr(&skyAddName, 30, skyAddEnter); return; } if (x < 380 && y < 184) return; skyAdding = false; hideKb(); }
    if (x < 380) {
        if (inR(x, y, 300, 74, 60, 40)) { skyAdding = true; skyAddName.clear(); openKbStr(&skyAddName, 30, skyAddEnter); return; }
        if (y >= 122) { int i = (y - 122 + skyListScroll) / 76; if (i >= 0 && i < n) { skySel = i; gSky[i].unread = 0; skyScroll = 0; skyCall = 0; gSkyDirty = true; } }
        return;
    }
    if (skySel < 0 || skySel >= n) return;
    SkyContact& c = gSky[skySel]; int rx = 381, rw = W - rx;
    if (skyCall) {
        if (inR(x, y, rx + rw / 2 + 56, 486, 68, 68)) { skyCall = 0; return; }
        if (inR(x, y, rx + rw / 2 - 124, 486, 68, 68)) { skyMute = !skyMute; return; }
        return;
    }
    if (inR(x, y, W - 316, 72, 52, 52)) {
        if (c.kind == 3) { toast("Cortana can't take calls, but she answers messages"); return; }
        skyCall = 1; skyCallT = SDL_GetTicks(); sndPlay(SND_RING); return;
    }
    if (inR(x, y, W - 258, 72, 52, 52)) { toast("Video calls need a camera - not available on Switch"); return; }
    if (inR(x, y, W - 200, 80, 60, 30)) { c.msgs.clear(); c.unread = 0; gSkyDirty = true; return; }
    if (c.kind == 0 && inR(x, y, W - 130, 80, 110, 30)) { gSky.erase(gSky.begin() + skySel); skySel = -1; skyPend.clear(); gSkyDirty = true; return; }
    int iy = bottom - 66;
    if (inR(x, y, rx + 12, iy, 50, 56)) { skyEmoji = !skyEmoji; return; }
    if (skyEmoji && y >= iy - 52 && y < iy - 12) { static const char* EM[8] = {":)", ":D", ";)", ":(", ":P", "<3", "(y)", ":O"}; int k = (x - rx - 70) / 74; if (k >= 0 && k < 8) skyIn += EM[k]; return; }
    if (inR(x, y, W - 64, iy, 52, 56)) { skySend(); return; }
    if (inR(x, y, rx + 70, iy, rw - 146, 56)) { openKbStr(&skyIn, 400, skySendEnter); return; }
}
static void skypeDrag(int dx, int dy) { (void)dx; if (skyDownX < 380) skyListScroll = std::max(0, skyListScroll - dy); else skyScroll = std::max(0, skyScroll + dy); }
static bool skypeBack() { if (skyCall) { skyCall = 0; return true; } if (skyAdding) { skyAdding = false; hideKb(); return true; } if (skySel >= 0 && kbOpen) { hideKb(); return true; } return false; }

// ================================================================ OneDrive (Windows 8.1 look). Files are kept in switch/WinNX/OneDrive on the SD card.
struct OdItem { std::string name; bool dir; long long size, mtime; };
static std::string odRoot() { return DIR_ + "OneDrive"; }
static std::string odRel;                     // current folder below the root ("" or "Documents/Reports")
static std::vector<OdItem> odItems; static std::vector<OdItem> odRecent;
static int odView = 0, odSel = -1, odScroll = 0, odPickMode = 0; static std::string odPick = "sdmc:/";  // odPickMode 1 = choosing a file from the SD card
static std::string odRename; static float odDelArm = 0; static long long odUsed = 0; static std::string odMsg; static Uint32 odMsgT = 0;
static std::string odPath() { return odRel.empty() ? odRoot() : odRoot() + "/" + odRel; }
static void odToast(const std::string& m) { odMsg = m; odMsgT = SDL_GetTicks(); }
static void odListDir(const std::string& dir, std::vector<OdItem>& out) {
    out.clear(); DIR* d = opendir(dir.c_str()); if (!d) return; struct dirent* e;
    while ((e = readdir(d)) != nullptr) {
        std::string n = e->d_name; if (n == "." || n == ".." || (n.size() > 4 && n.substr(n.size() - 4) == ".tmp")) continue;
        struct stat st; if (stat((dir + "/" + n).c_str(), &st) != 0) continue;
        out.push_back({n, S_ISDIR(st.st_mode) != 0, (long long)st.st_size, (long long)st.st_mtime});
        if (out.size() > 600) break;
    }
    closedir(d);
    std::sort(out.begin(), out.end(), [](const OdItem& a, const OdItem& b) { if (a.dir != b.dir) return a.dir; return lower(a.name) < lower(b.name); });
}
static void odScanTree(const std::string& dir, int depth, std::vector<std::pair<std::string, OdItem>>& all, long long& used) {
    std::vector<OdItem> v; odListDir(dir, v);
    for (auto& it : v) { if (it.dir) { if (depth > 0) odScanTree(dir + "/" + it.name, depth - 1, all, used); } else { used += it.size; all.push_back({dir + "/" + it.name, it}); } if (all.size() > 4000) return; }
}
static void odRefresh() {
    odListDir(odPath(), odItems); odSel = -1; odDelArm = 0;
    std::vector<std::pair<std::string, OdItem>> all; odUsed = 0; odScanTree(odRoot(), 5, all, odUsed);
    std::sort(all.begin(), all.end(), [](const std::pair<std::string, OdItem>& a, const std::pair<std::string, OdItem>& b) { return a.second.mtime > b.second.mtime; });
    odRecent.clear(); for (size_t i = 0; i < all.size() && i < 40; i++) { OdItem it = all[i].second; it.name = all[i].first.substr(odRoot().size() + 1); odRecent.push_back(it); }
}
static void odEnsureRoot() { mkdir(odRoot().c_str(), 0777); for (const char* d : {"Documents", "Pictures", "Shared"}) mkdir((odRoot() + "/" + d).c_str(), 0777); }
static void odInit() { odEnsureRoot(); odRel.clear(); odView = 0; odPickMode = 0; odScroll = 0; odRefresh(); }
static bool odChanged(const std::string& a, const std::string& b) { return fileSizeOf(a) != fileSizeOf(b); }
static int odBackupDocs() {
    odEnsureRoot(); int n = 0; std::string dst = odRoot() + "/Documents/";
    const char* files[] = {"document1.wdoc", "sheet1.csv", "sheet2.csv", "sheet3.csv", "notes.txt", "todo.txt", "calendar.txt"};
    for (const char* f : files) { std::string src = DIR_ + f; if (fileSizeOf(src) < 0) continue; if (odChanged(src, dst + f)) { if (copyFile(src, dst + f)) n++; } }
    return n;
}
static std::string odFmt(long long b) { char s[32]; if (b >= (1LL << 30)) snprintf(s, sizeof s, "%.1f GB", b / 1073741824.0); else if (b >= (1 << 20)) snprintf(s, sizeof s, "%.1f MB", b / 1048576.0); else snprintf(s, sizeof s, "%lld KB", (b + 1023) / 1024); return s; }
static std::string odExt(const std::string& n) { size_t p = n.rfind('.'); if (p == std::string::npos || p + 1 >= n.size()) return ""; std::string e = lower(n.substr(p + 1)); return e.size() > 4 ? e.substr(0, 4) : e; }
static void odTileDraw(int x, int y, int w, int h, const OdItem& it, bool sel) {
    bool img = !it.dir && isImg(it.name);
    Col c = it.dir ? Col{20, 90, 180} : (img ? Col{0, 150, 160} : (odExt(it.name) == "pdf" ? Col{200, 30, 40} : Col{86, 90, 100}));
    rect(x, y, w, h, c);
    if (it.dir) { rect(x + w / 2 - 28, y + 22, 24, 10, WHITE); rect(x + w / 2 - 28, y + 32, 56, 36, WHITE); }
    else if (img) { outline(x + w / 2 - 26, y + 22, 52, 42, 3, WHITE); tri(x + w / 2 - 22, y + 60, x + w / 2 - 6, y + 40, x + w / 2 + 8, y + 60, WHITE); disc(x + w / 2 + 12, y + 34, 5, WHITE); }
    else text(odExt(it.name).empty() ? "file" : odExt(it.name), F30, x + w / 2, y + 28, WHITE, 1);
    text(fitTxt(it.name, F16, w - 16), F16, x + 8, y + h - 28, WHITE);
    if (!it.dir) text(odFmt(it.size), F16, x + w - 8, y + 6, WHITE, 2, 170);
    if (sel) { outline(x - 3, y - 3, w + 6, h + 6, 3, Col{255, 255, 255}); disc(x + w - 18, y + 18, 13, WHITE); line(x + w - 25, y + 18, x + w - 19, y + 24, 3, c); line(x + w - 19, y + 24, x + w - 10, y + 12, 3, c); }
}
static const int OD_TW = 168, OD_TH = 120, OD_GAP = 12, OD_X = 60, OD_Y = 196;
static int odCols() { return std::max(1, (W - 2 * OD_X + OD_GAP) / (OD_TW + OD_GAP)); }
static std::vector<OdItem>& odShown() { static std::vector<OdItem> pk; if (odPickMode) { odListDir(odPick, pk); return pk; } return odItems; }
static void drawOneDrive() {
    int bottom = UIH; rect(0, 0, W, UIH, Col{246, 247, 249});
    text("OneDrive", F72, OD_X, 6, Col{20, 90, 180});
    // storage
    char b[96]; snprintf(b, sizeof b, "%s in OneDrive", odFmt(odUsed).c_str()); text(b, F22, W - 60, 28, W9_TXT, 2);
    fmStorage(); text(fmSize(fmFree) + " free on the SD card", F16, W - 60, 62, W9_GREY, 2); text("Saved on this console - not synced to the cloud", F16, W - 60, 84, W9_GREY, 2);
    // pivots
    static const char* PV[3] = {"files", "recent", "photos"}; int px = OD_X;
    for (int i = 0; i < 3; i++) { text(PV[i], F30, px, 112, i == odView ? W9_TXT : Col{150, 154, 160}); if (i == odView) rect(px, 150, tw(PV[i], F30), 3, Col{20, 90, 180}); px += tw(PV[i], F30) + 40; }
    if (odPickMode) { rect(0, 160, W, 34, Col{255, 244, 200}); text("Choose files to add to OneDrive > " + (odRel.empty() ? std::string("OneDrive") : odRel) + "    (now in " + odPick + ")", F16, OD_X, 168, W9_TXT); }
    else { std::string bc = "OneDrive"; if (!odRel.empty()) { for (char c : odRel) bc += c == '/' ? "  >  " : std::string(1, c); } text(bc, F22, OD_X, 160, W9_GREY); if (!odRel.empty()) text("< back", F22, W - 60, 160, Col{20, 90, 180}, 2); }
    int top = OD_Y, bot = bottom - 96; SDL_Rect clip{0, top - 4, W, bot - top + 4}; SDL_RenderSetClipRect(gR, &clip);
    if (odView == 0) {
        std::vector<OdItem>& v = odShown(); int cols = odCols(), rows = ((int)v.size() + cols - 1) / cols; int maxS = std::max(0, rows * (OD_TH + OD_GAP) - (bot - top)); odScroll = std::max(0, std::min(odScroll, maxS));
        for (int i = 0; i < (int)v.size(); i++) { int x = OD_X + (i % cols) * (OD_TW + OD_GAP), y = top + (i / cols) * (OD_TH + OD_GAP) - odScroll; if (y + OD_TH < top || y > bot) continue; odTileDraw(x, y, OD_TW, OD_TH, v[i], !odPickMode && i == odSel); }
        if (v.empty()) text(odPickMode ? "This folder is empty" : "This folder is empty. Use Add files or New folder below.", F22, OD_X, top + 20, W9_GREY);
    } else if (odView == 1) {
        int maxS = std::max(0, (int)odRecent.size() * 56 - (bot - top)); odScroll = std::max(0, std::min(odScroll, maxS));
        for (int i = 0; i < (int)odRecent.size(); i++) { int y = top + i * 56 - odScroll; if (y + 56 < top || y > bot) continue; rect(OD_X, y, W - 2 * OD_X, 50, i == odSel ? Col{214, 232, 250} : WHITE); rect(OD_X, y, 50, 50, isImg(odRecent[i].name) ? Col{0, 150, 160} : Col{86, 90, 100}); text(odExt(odRecent[i].name), F16, OD_X + 25, y + 14, WHITE, 1); text(fitTxt(odRecent[i].name, F22, W - 2 * OD_X - 300), F22, OD_X + 66, y + 10, W9_TXT); text(odFmt(odRecent[i].size), F16, W - OD_X - 12, y + 16, W9_GREY, 2); }
        if (odRecent.empty()) text("Nothing here yet", F22, OD_X, top + 20, W9_GREY);
    } else {
        int cols = odCols(), k = 0, maxS = 0; std::vector<int> idx; for (int i = 0; i < (int)odRecent.size(); i++) if (isImg(odRecent[i].name)) idx.push_back(i);
        maxS = std::max(0, ((int)idx.size() + cols - 1) / cols * (OD_TH + OD_GAP) - (bot - top)); odScroll = std::max(0, std::min(odScroll, maxS));
        for (int i : idx) { int x = OD_X + (k % cols) * (OD_TW + OD_GAP), y = top + (k / cols) * (OD_TH + OD_GAP) - odScroll; k++; if (y + OD_TH < top || y > bot) continue; OdItem it = odRecent[i]; size_t sl = it.name.rfind('/'); if (sl != std::string::npos) it.name = it.name.substr(sl + 1); odTileDraw(x, y, OD_TW, OD_TH, it, false); }
        if (idx.empty()) text("No pictures yet. Put pictures in the Pictures folder.", F22, OD_X, top + 20, W9_GREY);
    }
    SDL_RenderSetClipRect(gR, nullptr);
    // app bar
    rect(0, bottom - 96, W, 96, Col{236, 236, 240}); rect(0, bottom - 96, W, 2, Col{200, 200, 206});
    struct Bt { const char* l; bool on; }; std::vector<Bt> bt;
    if (odPickMode) bt = {{"Up", true}, {"Done", true}, {"", false}};
    else bt = {{"New folder", odView == 0}, {"Add files", odView == 0}, {"Back up now", true}, {"Open", odSel >= 0}, {"Rename", odSel >= 0 && odView == 0}, {odDelArm > 0 ? "Tap again" : "Delete", odSel >= 0 && odView == 0}};
    for (int i = 0; i < (int)bt.size(); i++) { if (!*bt[i].l) continue; int cx = W / 2 + (int)((i - (bt.size() - 1) / 2.0f) * 170); Col fg = bt[i].on ? Col{40, 40, 44} : Col{170, 170, 176}; ring(cx, bottom - 66, 24, 3, fg, Col{236, 236, 240}); text(*bt[i].l == 'N' ? "+" : (*bt[i].l == 'A' ? "+" : (*bt[i].l == 'B' ? "b" : (*bt[i].l == 'O' ? ">" : (*bt[i].l == 'R' ? "r" : (*bt[i].l == 'D' ? (odDelArm > 0 ? "!" : "x") : (*bt[i].l == 'U' ? "^" : "ok")))))), F22, cx, bottom - 80, fg, 1); text(bt[i].l, F16, cx, bottom - 32, fg, 1); }
    if (SDL_GetTicks() - odMsgT < 3000 && !odMsg.empty()) { int w = tw(odMsg, F22) + 40; rect((W - w) / 2, bottom - 156, w, 46, BLACK, 220); text(odMsg, F22, W / 2, bottom - 148, WHITE, 1); }
}
static void odOpenItem(const OdItem& it, const std::string& relPath) {
    std::string full = odRoot() + "/" + relPath;
    if (isImg(it.name)) { gPhotoOpenPath = full; gotoApp(A_PHOTOS); }
    else if (odExt(it.name) == "pdf") { gPdfOpenPath = full; gotoApp(A_PDF); }
    else odToast("No app on this console opens ." + odExt(it.name) + " files yet");
}
static void odRenameEnter() {
    hideKb(); std::string nn = odRename; odRename.clear(); while (!nn.empty() && nn.back() == ' ') nn.pop_back();
    if (nn.empty() || nn.find('/') != std::string::npos || odSel < 0 || odSel >= (int)odItems.size()) return;
    std::string a = odPath() + "/" + odItems[odSel].name, b = odPath() + "/" + nn;
    if (rename(a.c_str(), b.c_str()) == 0) { odToast("Renamed"); odRefresh(); } else odToast("Couldn't rename");
}
static void odNewFolderEnter() { hideKb(); std::string nn = odRename; odRename.clear(); while (!nn.empty() && nn.back() == ' ') nn.pop_back(); if (nn.empty() || nn.find('/') != std::string::npos) return; if (mkdir((odPath() + "/" + nn).c_str(), 0777) == 0) { odToast("Created " + nn); odRefresh(); } else odToast("Couldn't create the folder"); }
static void oneDriveTap(int x, int y) {
    int bottom = UIH;
    if (y >= bottom - 96) {
        std::vector<int> ids; if (odPickMode) ids = {1, 2}; else ids = {3, 4, 5, 6, 7, 8};
        int n = (int)ids.size(); int i = (int)floorf((x - W / 2) / 170.0f + (n - 1) / 2.0f + 0.5f); if (i < 0 || i >= n) return; int id = ids[i];
        if (odPickMode) { if (id == 1) { size_t p = odPick.find_last_of('/'); if (odPick.size() > 6 && p != std::string::npos) { odPick = odPick.substr(0, p); if (odPick == "sdmc:") odPick = "sdmc:/"; } } else { odPickMode = 0; odRefresh(); } return; }
        if (id == 3 && odView == 0) { odRename.clear(); openKbStr(&odRename, 40, odNewFolderEnter); }
        else if (id == 4 && odView == 0) { odPickMode = 1; odPick = "sdmc:/"; odScroll = 0; }
        else if (id == 5) { int n2 = odBackupDocs(); odToast(n2 ? "Backed up " + std::to_string(n2) + " documents" : "Everything is already backed up"); odRefresh(); }
        else if (id == 6 && odSel >= 0) { if (odView == 0 && odSel < (int)odItems.size()) { OdItem& it = odItems[odSel]; if (it.dir) { odRel += (odRel.empty() ? "" : "/") + it.name; odScroll = 0; odRefresh(); } else odOpenItem(it, (odRel.empty() ? "" : odRel + "/") + it.name); } else if (odView == 1 && odSel < (int)odRecent.size()) odOpenItem(odRecent[odSel], odRecent[odSel].name); }
        else if (id == 7 && odSel >= 0 && odView == 0 && odSel < (int)odItems.size()) { odRename = odItems[odSel].name; openKbStr(&odRename, 40, odRenameEnter); }
        else if (id == 8 && odSel >= 0 && odView == 0 && odSel < (int)odItems.size()) {
            if (odDelArm > 0) { std::string p = odPath() + "/" + odItems[odSel].name; if (odItems[odSel].dir ? rmdir(p.c_str()) == 0 : remove(p.c_str()) == 0) { odToast("Deleted"); odRefresh(); } else odToast(odItems[odSel].dir ? "Folder isn't empty" : "Couldn't delete"); odDelArm = 0; }
            else odDelArm = 3.5f;
        }
        return;
    }
    if (y >= 104 && y < 156 && !odPickMode) { int px = OD_X; for (int i = 0; i < 3; i++) { int w = tw(i == 0 ? "files" : (i == 1 ? "recent" : "photos"), F30); if (inR(x, y, px - 10, 104, w + 20, 52)) { odView = i; odScroll = 0; odSel = -1; return; } px += w + 40; } }
    if (!odPickMode && odView == 0 && !odRel.empty() && inR(x, y, W - 160, 150, 120, 40)) { size_t p = odRel.rfind('/'); odRel = p == std::string::npos ? "" : odRel.substr(0, p); odScroll = 0; odRefresh(); return; }
    if (y < OD_Y) return;
    if (odView == 0) {
        std::vector<OdItem>& v = odShown(); int cols = odCols();
        for (int i = 0; i < (int)v.size(); i++) {
            int tx = OD_X + (i % cols) * (OD_TW + OD_GAP), ty = OD_Y + (i / cols) * (OD_TH + OD_GAP) - odScroll;
            if (!inR(x, y, tx, ty, OD_TW, OD_TH)) continue;
            if (odPickMode) {
                std::string full = (odPick.back() == '/' ? odPick : odPick + "/") + v[i].name;
                if (v[i].dir) { odPick = full; odScroll = 0; }
                else if (v[i].size > 100LL * 1048576) odToast("That file is bigger than 100 MB");
                else { odEnsureRoot(); if (copyFile(full, odPath() + "/" + v[i].name)) odToast("Added " + v[i].name); else odToast("Couldn't copy " + v[i].name); }
            } else { if (odSel == i && v[i].dir) { odRel += (odRel.empty() ? "" : "/") + v[i].name; odScroll = 0; odRefresh(); } else if (odSel == i) odOpenItem(v[i], (odRel.empty() ? "" : odRel + "/") + v[i].name); else { odSel = i; odDelArm = 0; } }
            return;
        }
        odSel = -1;
    } else if (odView == 1) {
        int i = (y - OD_Y + odScroll) / 56; if (i >= 0 && i < (int)odRecent.size()) { if (odSel == i) odOpenItem(odRecent[i], odRecent[i].name); else odSel = i; }
    } else {
        int cols = odCols(), k = 0; for (int i = 0; i < (int)odRecent.size(); i++) { if (!isImg(odRecent[i].name)) continue; int tx = OD_X + (k % cols) * (OD_TW + OD_GAP), ty = OD_Y + (k / cols) * (OD_TH + OD_GAP) - odScroll; k++; if (inR(x, y, tx, ty, OD_TW, OD_TH)) { odOpenItem(odRecent[i], odRecent[i].name); return; } }
    }
}
static void oneDriveDrag(int, int dy) { odScroll = std::max(0, odScroll - dy); }
static bool oneDriveBack() {
    if (odPickMode) { odPickMode = 0; return true; }
    if (odView == 0 && !odRel.empty()) { size_t p = odRel.rfind('/'); odRel = p == std::string::npos ? "" : odRel.substr(0, p); odScroll = 0; odRefresh(); return true; }
    return false;
}
static void oneDriveUpdate(float dt) {
    if (odDelArm > 0) odDelArm -= dt;
    static float acc = 0; acc += dt; if (acc >= 120.0f) { acc = 0; if (gOdAuto && uiState == 2) odBackupDocs(); }
}
