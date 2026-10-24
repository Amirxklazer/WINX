// w82_clock.h - Alarms & Clock: alarms (ring anywhere), world clock, timer, stopwatch (included from w82.h)

struct ClkAlarm { int h = 7, m = 0; bool on = true; int days = 0; std::string label; };   // days: bit i = WD[i] (Sunday = bit 0); 0 = once
static std::vector<ClkAlarm> gAlarms;
static bool clkLoaded = false;
static int clkTab = 0, clkScroll = 0, clkEdit = -2;       // clkEdit: -2 none, -1 new alarm, >=0 editing that alarm
static ClkAlarm clkE;
static std::string clkLabelBuf;
static bool clkPick = false;                               // world tab: choosing cities

// ---- ringing state (shared by alarm and timer)
static bool clkRingOn = false; static std::string clkRingTitle, clkRingLabel; static Uint32 clkRingT0 = 0, clkRingLast = 0; static int clkRingKind = 0;   // 0 alarm, 1 timer
static long long clkSnoozeAt = -1;                         // absolute minute (since epoch days) when a snoozed alarm rings again
static int clkLastFiredKey = -1;

// ---- timer / stopwatch
static int clkTH = 0, clkTM = 5, clkTS = 0;                // timer setting
static bool clkTRun = false, clkTPaused = false; static Uint32 clkTEnd = 0; static float clkTLeft = 0, clkTTotal = 0;
static bool clkSwRun = false; static Uint32 clkSwT0 = 0; static float clkSwAcc = 0; static std::vector<float> clkLaps;

static std::string clkFile() { return DIR_ + "alarms.txt"; }
static long long clkAbsMin() { return wcDays(gNow.y, gNow.mo, gNow.d) * 1440LL + gNow.h * 60 + gNow.mi; }
static std::string clkClean(std::string s) { for (auto& c : s) if (c == '|' || c == '\n' || c == '\r') c = ' '; return s; }
static void clkSave() {
    std::string d;
    for (auto& a : gAlarms) d += std::to_string(a.h) + "|" + std::to_string(a.m) + "|" + (a.on ? "1" : "0") + "|" + std::to_string(a.days) + "|" + clkClean(a.label) + "\n";
    writeFile(clkFile(), d);
}
static void clkLoad() {
    if (clkLoaded) return;
    clkLoaded = true; gAlarms.clear();
    std::string d; if (!readFile(clkFile(), d)) return;
    size_t pos = 0;
    while (pos < d.size()) {
        size_t e = d.find('\n', pos); if (e == std::string::npos) e = d.size();
        std::string ln = d.substr(pos, e - pos); pos = e + 1;
        std::vector<std::string> f; size_t a = 0;
        while (true) { size_t b = ln.find('|', a); if (b == std::string::npos) { f.push_back(ln.substr(a)); break; } f.push_back(ln.substr(a, b - a)); a = b + 1; }
        if (f.size() < 5) continue;
        ClkAlarm al; al.h = std::max(0, std::min(23, atoi(f[0].c_str()))); al.m = std::max(0, std::min(59, atoi(f[1].c_str()))); al.on = f[2] != "0"; al.days = atoi(f[3].c_str()) & 127; al.label = f[4];
        gAlarms.push_back(al);
    }
}
static std::string clkHM(int h, int m, bool withAmPm = true) {
    char b[24];
    if (g24) snprintf(b, sizeof b, "%02d:%02d", h, m);
    else snprintf(b, sizeof b, withAmPm ? "%d:%02d %s" : "%d:%02d", h % 12 ? h % 12 : 12, m, h < 12 ? "AM" : "PM");
    return b;
}
static std::string clkDaysText(int days) {
    if (days == 0) return "Once";
    if (days == 127) return "Every day";
    if (days == 62) return "Weekdays";
    if (days == 65) return "Weekends";
    std::string s; for (int i = 0; i < 7; i++) if (days & (1 << i)) { if (!s.empty()) s += " "; s += WD3[i]; }
    return s;
}
// minutes from now until the next time this alarm would ring (for the "next alarm" text); -1 if never
static int clkMinsUntil(const ClkAlarm& a) {
    if (!a.on) return -1;
    int nowM = gNow.h * 60 + gNow.mi, at = a.h * 60 + a.m;
    int wd = dow(gNow.y, gNow.mo, gNow.d);
    for (int k = 0; k < 8; k++) {
        int dayOff = k; int d = (wd + k) % 7;
        if (a.days != 0 && !(a.days & (1 << d))) continue;
        int mins = dayOff * 1440 + at - nowM;
        if (mins > 0 || (mins == 0 && false)) return mins;
    }
    return -1;
}
static std::string clkNextAlarmText() {
    clkLoad();
    int best = -1, bi = -1;
    for (int i = 0; i < (int)gAlarms.size(); i++) { int m = clkMinsUntil(gAlarms[i]); if (m >= 0 && (best < 0 || m < best)) { best = m; bi = i; } }
    if (bi < 0) return "You have no alarms set.";
    std::string in;
    if (best >= 1440) in = std::to_string(best / 1440) + " day" + (best / 1440 > 1 ? "s " : " ") + std::to_string(best % 1440 / 60) + " h";
    else if (best >= 60) in = std::to_string(best / 60) + " h " + std::to_string(best % 60) + " min";
    else in = std::to_string(best) + " min";
    return "Your next alarm is at " + clkHM(gAlarms[bi].h, gAlarms[bi].m) + ", in " + in + ".";
}
static void clkAddAlarm(int h, int m, const std::string& label) {
    clkLoad();
    ClkAlarm a; a.h = std::max(0, std::min(23, h)); a.m = std::max(0, std::min(59, m)); a.on = true; a.days = 0; a.label = clkClean(label);
    gAlarms.push_back(a); clkSave();
}

// ---- ringing
static bool clkRinging() { return clkRingOn; }
static void clkStartRing(int kind, const std::string& title, const std::string& label) {
    clkRingOn = true; clkRingKind = kind; clkRingTitle = title; clkRingLabel = label; clkRingT0 = SDL_GetTicks(); clkRingLast = 0;
    lastInput = SDL_GetTicks();
    notify(A_CLOCK, title, label.empty() ? clkHM(gNow.h, gNow.mi) : label);
}
static void clkStopRing() { clkRingOn = false; }
static void clkSnooze() {
    if (clkRingKind == 0) { clkSnoozeAt = clkAbsMin() + 9; toast("Snoozed for 9 minutes"); }
    else { clkTRun = true; clkTPaused = false; clkTTotal = 60; clkTLeft = 60; clkTEnd = SDL_GetTicks() + 60000; toast("Timer: 1 more minute"); }
    clkStopRing();
}
static void clkDrawOverlay() {
    if (!clkRingOn) return;
    float t = (SDL_GetTicks() - clkRingT0) / 1000.0f;
    rect(0, 0, W, UIH, Col{0, 60, 70}, 245);
    float p = 0.5f + 0.5f * sinf(t * 6.0f);
    for (int i = 0; i < 3; i++) ring(W / 2, 220, 100 + i * 28 + (int)(p * 8), 3, Col{255, 255, 255}, Col{0, 60, 70});
    disc(W / 2, 220, 92, Col{0, 150, 160});
    // bell
    float sw = sinf(t * 14.0f) * 0.15f;
    int bx = W / 2, by = 215;
    tri((float)bx, (float)(by - 50), (float)(bx - 46 + sw * 120), (float)(by + 28), (float)(bx + 46 + sw * 120), (float)(by + 28), WHITE, 255);
    rect(bx - 56 + (int)(sw * 120), by + 26, 112, 10, WHITE);
    disc(bx + (int)(sw * 160), by + 48, 9, WHITE);
    text(clkRingTitle, F44, W / 2, 360, WHITE, 1);
    text(clkHM(gNow.h, gNow.mi), F72, W / 2, 410, WHITE, 1);
    if (!clkRingLabel.empty()) text(fitTxt(clkRingLabel, F30, 900), F30, W / 2, 500, WHITE, 1, 220);
    btn(W / 2 - 330, 560, 300, 70, clkRingKind == 0 ? "Snooze 9 min" : "+1 minute", Col{255, 255, 255}, Col{0, 80, 90});
    btn(W / 2 + 30, 560, 300, 70, "Dismiss", Col{0, 150, 160}, WHITE);
}
static bool clkOverlayTap(int x, int y) {
    if (!clkRingOn) return false;
    if (inR(x, y, W / 2 - 330, 560, 300, 70)) clkSnooze();
    else if (inR(x, y, W / 2 + 30, 560, 300, 70)) clkStopRing();
    return true;
}

// ---- per-frame work (runs while the app is closed too)
static void clkUpdate(float) {
    clkLoad();
    Uint32 now = SDL_GetTicks();
    // timer
    if (clkTRun && !clkTPaused) {
        int left = (int)(clkTEnd - now);
        if (clkTEnd <= now || left <= 0) { clkTRun = false; clkTLeft = 0; clkStartRing(1, "Timer", "Time's up"); }
        else clkTLeft = left / 1000.0f;
    }
    // alarms: check once per second
    static Uint32 lastChk = 0;
    if (now - lastChk > 500) {
        lastChk = now;
        int key = (gNow.d * 24 * 60 + gNow.h * 60 + gNow.mi);
        long long absM = clkAbsMin();
        if (!clkRingOn && key != clkLastFiredKey) {
            if (clkSnoozeAt >= 0 && absM >= clkSnoozeAt) { clkSnoozeAt = -1; clkLastFiredKey = key; clkStartRing(0, "Alarm", "Snoozed alarm"); }
            else {
                int wd = dow(gNow.y, gNow.mo, gNow.d);
                for (auto& a : gAlarms) {
                    if (!a.on || a.h != gNow.h || a.m != gNow.mi) continue;
                    if (a.days != 0 && !(a.days & (1 << wd))) continue;
                    clkLastFiredKey = key; if (a.days == 0) { a.on = false; clkSave(); }
                    clkStartRing(0, "Alarm", a.label.empty() ? "Alarm" : a.label); break;
                }
            }
        }
    }
    if (clkRingOn) {
        if (now - clkRingLast > 1800) { clkRingLast = now; sndPlay(SND_RING); }
        if (now - clkRingT0 > 120000) { clkStopRing(); notify(A_CLOCK, "Missed " + std::string(clkRingKind == 0 ? "alarm" : "timer"), clkRingLabel); }
    }
}

// ---- UI helpers
static Col clkPanel(Col bg, float t = 0.14f) { return mix(bg, WHITE, t); }
static void clkSwitch(int x, int y, bool on) {
    rect(x, y, 90, 34, on ? Col{255, 255, 255} : Col{255, 255, 255}, on ? 255 : 70);
    rect(x + (on ? 54 : 4), y + 4, 32, 26, on ? Col{0, 114, 198} : Col{255, 255, 255}, on ? 255 : 200);
    text(on ? "On" : "Off", F16, x + (on ? 20 : 66), y + 6, on ? Col{0, 80, 140} : WHITE, 1);
}
static std::string clkFmtT(float s, bool cs) {
    int total = (int)s; int h = total / 3600, m = (total / 60) % 60, sec = total % 60; char b[40];
    if (cs) { int c = (int)((s - total) * 100); if (h) snprintf(b, sizeof b, "%d:%02d:%02d.%02d", h, m, sec, c); else snprintf(b, sizeof b, "%02d:%02d.%02d", m, sec, c); }
    else { if (h) snprintf(b, sizeof b, "%d:%02d:%02d", h, m, sec); else snprintf(b, sizeof b, "%02d:%02d", m, sec); }
    return b;
}
static void clkArc(int cx, int cy, int r, float frac, int th, Col c) {
    int n = std::max(2, (int)(frac * 90));
    for (int i = 0; i < n; i++) {
        float a0 = -1.5707963f + 6.2831853f * frac * i / n, a1 = -1.5707963f + 6.2831853f * frac * (i + 1) / n;
        line(cx + (int)(cosf(a0) * r), cy + (int)(sinf(a0) * r), cx + (int)(cosf(a1) * r), cy + (int)(sinf(a1) * r), th, c);
    }
}
static void clkInit() { clkLoad(); clkScroll = 0; clkEdit = -2; clkPick = false; if (!wcInit) wcLoad(); }

static const int CK_TOP = 172;     // content starts here
static void clkDrawAlarms(Col bg) {
    SDL_Rect clip{0, CK_TOP, W, UIH - CK_TOP}; SDL_RenderSetClipRect(gR, &clip);
    if (gAlarms.empty()) {
        text("No alarms yet", F44, W / 2, 270, WHITE, 1, 220);
        text("Tap + to add one. Alarms ring even when this app is closed.", F22, W / 2, 330, WHITE, 1, 170);
    }
    int maxS = std::max(0, (int)gAlarms.size() * 100 - (UIH - CK_TOP - 120));
    clkScroll = std::max(0, std::min(clkScroll, maxS));
    for (int i = 0; i < (int)gAlarms.size(); i++) {
        int y = CK_TOP + 10 + i * 100 - clkScroll; if (y + 92 < CK_TOP || y > UIH) continue;
        const ClkAlarm& a = gAlarms[i];
        rect(40, y, W - 80, 90, clkPanel(bg), a.on ? 255 : 170);
        text(clkHM(a.h, a.m), F44, 66, y + 12, WHITE, 0, a.on ? 255 : 140);
        std::string sub = clkDaysText(a.days); if (!a.label.empty()) sub = a.label + "  -  " + sub;
        text(fitTxt(sub, F16, 640), F16, 380, y + 34, WHITE, 0, a.on ? 220 : 120);
        clkSwitch(W - 170, y + 28, a.on);
    }
    SDL_RenderSetClipRect(gR, nullptr);
    // floating plus
    disc(W - 80, UIH - 80, 38, Col{255, 255, 255}); rect(W - 98, UIH - 83, 36, 6, bg); rect(W - 83, UIH - 98, 6, 36, bg);
}
static void clkDrawEditor(Col bg) {
    rect(0, 0, W, UIH, BLACK, 150);
    int px = 190, py = 120, pw = 900, ph = 460;
    rect(px, py, pw, ph, mix(bg, BLACK, 0.2f)); outline(px, py, pw, ph, 2, Col{255, 255, 255});
    text(clkEdit == -1 ? "New alarm" : "Edit alarm", F30, px + 30, py + 18, WHITE);
    // hour / minute steppers
    for (int k = 0; k < 2; k++) {
        int cx = px + 150 + k * 190; int v = k ? clkE.m : clkE.h;
        btn(cx - 50, py + 70, 100, 44, "+", Col{255, 255, 255}, Col{0, 80, 90});
        char b[8]; snprintf(b, sizeof b, "%02d", v); text(b, F72, cx, py + 118, WHITE, 1);
        btn(cx - 50, py + 214, 100, 44, "-", Col{255, 255, 255}, Col{0, 80, 90});
    }
    text(":", F72, px + 245, py + 112, WHITE, 1);
    if (!g24) { text(clkE.h < 12 ? "AM" : "PM", F44, px + 520, py + 140, WHITE, 1); text("(tap the hour to flip)", F16, px + 520, py + 196, WHITE, 1, 150); }
    btn(px + 620, py + 70, 120, 44, "+5 min", Col{255, 255, 255}, Col{0, 80, 90});
    btn(px + 760, py + 70, 120, 44, "+1 h", Col{255, 255, 255}, Col{0, 80, 90});
    // days
    text("Repeat", F22, px + 30, py + 282, WHITE, 0, 200);
    for (int i = 0; i < 7; i++) {
        int x = px + 30 + i * 120; bool on = clkE.days & (1 << i);
        rect(x, py + 316, 110, 46, on ? Col{255, 255, 255} : Col{255, 255, 255}, on ? 255 : 60);
        text(WD3[i], F22, x + 55, py + 324, on ? Col{0, 80, 90} : WHITE, 1);
    }
    // label
    rect(px + 30, py + 378, 520, 50, Col{255, 255, 255}, 60);
    text(clkE.label.empty() ? "Label (tap to type)" : fitTxt(clkE.label, F22, 490), F22, px + 44, py + 388, WHITE, 0, clkE.label.empty() ? 140 : 255);
    btn(px + 570, py + 378, 100, 50, "Save", Col{255, 255, 255}, Col{0, 80, 90});
    btn(px + 680, py + 378, 100, 50, clkEdit >= 0 ? "Delete" : "Cancel", Col{200, 60, 60}, WHITE);
    if (clkEdit >= 0) btn(px + 790, py + 378, 90, 50, "Back", Col{255, 255, 255}, Col{0, 80, 90});
}
static void clkLabelDone() { clkE.label = clkLabelBuf; }
static bool clkAlarmsTap(int x, int y, Col /*bg*/) {
    if (clkEdit != -2) {
        int px = 190, py = 120, pw = 900, ph = 460;
        if (!inR(x, y, px, py, pw, ph)) { clkEdit = -2; return true; }
        for (int k = 0; k < 2; k++) {
            int cx = px + 150 + k * 190;
            if (inR(x, y, cx - 50, py + 70, 100, 44)) { if (k) clkE.m = (clkE.m + 1) % 60; else clkE.h = (clkE.h + 1) % 24; return true; }
            if (inR(x, y, cx - 50, py + 214, 100, 44)) { if (k) clkE.m = (clkE.m + 59) % 60; else clkE.h = (clkE.h + 23) % 24; return true; }
        }
        if (!g24 && inR(x, y, px + 60, py + 118, 180, 90)) { clkE.h = (clkE.h + 12) % 24; return true; }
        if (inR(x, y, px + 620, py + 70, 120, 44)) { clkE.m = (clkE.m + 5) % 60; return true; }
        if (inR(x, y, px + 760, py + 70, 120, 44)) { clkE.h = (clkE.h + 1) % 24; return true; }
        for (int i = 0; i < 7; i++) if (inR(x, y, px + 30 + i * 120, py + 316, 110, 46)) { clkE.days ^= (1 << i); return true; }
        if (inR(x, y, px + 30, py + 378, 520, 50)) { clkLabelBuf = clkE.label; openKbStr(&clkLabelBuf, 24, clkLabelDone); return true; }
        if (inR(x, y, px + 570, py + 378, 100, 50)) {
            clkE.on = true;
            if (clkEdit == -1) gAlarms.push_back(clkE); else if (clkEdit < (int)gAlarms.size()) gAlarms[clkEdit] = clkE;
            clkSave(); hideKb(); clkEdit = -2; toast(clkNextAlarmText()); return true;
        }
        if (inR(x, y, px + 680, py + 378, 100, 50)) { if (clkEdit >= 0 && clkEdit < (int)gAlarms.size()) { gAlarms.erase(gAlarms.begin() + clkEdit); clkSave(); } hideKb(); clkEdit = -2; return true; }
        if (clkEdit >= 0 && inR(x, y, px + 790, py + 378, 90, 50)) { hideKb(); clkEdit = -2; return true; }
        return true;
    }
    if (inR(x, y, W - 120, UIH - 120, 80, 80)) { clkE = ClkAlarm(); clkE.h = (gNow.h + (gNow.mi > 50 ? 1 : 0)) % 24; clkE.m = ((gNow.mi / 5) * 5 + 5) % 60; clkE.on = true; clkEdit = -1; return true; }
    if (y < CK_TOP) return false;
    for (int i = 0; i < (int)gAlarms.size(); i++) {
        int ry = CK_TOP + 10 + i * 100 - clkScroll; if (!inR(x, y, 40, ry, W - 80, 90)) continue;
        if (x >= W - 190) { gAlarms[i].on = !gAlarms[i].on; clkSave(); if (gAlarms[i].on) toast(clkNextAlarmText()); }
        else { clkE = gAlarms[i]; clkEdit = i; }
        return true;
    }
    return true;
}

// ---- world clock tab
static void clkDrawWorld(Col bg) {
    if (!wcInit) wcLoad();
    SDL_Rect clip{0, CK_TOP, W, UIH - CK_TOP}; SDL_RenderSetClipRect(gR, &clip);
    if (!wcPick) {
        int maxS = std::max(0, (int)wcSel.size() * 92 - (UIH - CK_TOP - 90)); wcScroll = std::max(0, std::min(wcScroll, maxS));
        if (wcSel.empty()) text("No cities. Tap + to add some.", F30, W / 2, 280, WHITE, 1, 200);
        for (int i = 0; i < (int)wcSel.size(); i++) {
            int y = CK_TOP + 8 + i * 92 - wcScroll; if (y + 84 < CK_TOP || y > UIH) continue; int c = wcSel[i];
            int dd; float off; Now n = wcTime(c, dd, off);
            rect(40, y, W - 80, 84, clkPanel(bg));
            text(WCITIES[c].n, F30, 66, y + 8, WHITE);
            char b[64]; snprintf(b, sizeof b, "UTC%+.4g   %s", off, dd > 0 ? "Tomorrow" : (dd < 0 ? "Yesterday" : "Today")); text(b, F16, 66, y + 50, WHITE, 0, 190);
            text(clkHM(n.h, n.mi), F44, W - 250, y + 12, WHITE, 2);
            text(c == wcHome ? "*" : "o", F30, W - 150, y + 18, c == wcHome ? Col{255, 210, 60} : Col{255, 255, 255}, 0, c == wcHome ? 255 : 120);
            text("x", F30, W - 90, y + 16, Col{255, 190, 190});
        }
    } else {
        int rows = (NWC + 2) / 3; int maxS = std::max(0, rows * 66 - (UIH - CK_TOP - 20)); wcScroll = std::max(0, std::min(wcScroll, maxS));
        for (int i = 0; i < NWC; i++) {
            int col = i % 3, rw = i / 3; int x = 50 + col * 400, y = CK_TOP + 8 + rw * 66 - wcScroll; if (y + 56 < CK_TOP || y > UIH) continue;
            bool has = std::find(wcSel.begin(), wcSel.end(), i) != wcSel.end();
            rect(x, y, 380, 56, has ? Col{40, 150, 90} : clkPanel(bg)); text(WCITIES[i].n, F22, x + 16, y + 12, WHITE);
        }
    }
    SDL_RenderSetClipRect(gR, nullptr);
    if (!wcPick) { disc(W - 80, UIH - 80, 38, Col{255, 255, 255}); rect(W - 98, UIH - 83, 36, 6, bg); rect(W - 83, UIH - 98, 6, 36, bg); }
    else btn(W - 240, UIH - 100, 190, 60, "Done", Col{255, 255, 255}, Col{0, 80, 90});
}
static bool clkWorldTap(int x, int y) {
    if (!wcInit) wcLoad();
    if (!wcPick && inR(x, y, W - 120, UIH - 120, 80, 80)) { wcPick = true; wcScroll = 0; return true; }
    if (wcPick && inR(x, y, W - 240, UIH - 100, 190, 60)) { wcPick = false; wcScroll = 0; wcSave(); return true; }
    if (y < CK_TOP) return false;
    if (!wcPick) {
        for (int i = 0; i < (int)wcSel.size(); i++) {
            int ry = CK_TOP + 8 + i * 92 - wcScroll; if (!inR(x, y, 40, ry, W - 80, 84)) continue;
            if (x >= W - 190 && x < W - 115) { wcHome = wcSel[i]; wcSave(); toast(std::string(WCITIES[wcHome].n) + " is now your time zone"); }
            else if (x >= W - 115) { wcSel.erase(wcSel.begin() + i); wcSave(); }
            return true;
        }
    } else {
        for (int i = 0; i < NWC; i++) {
            int col = i % 3, rw = i / 3; if (!inR(x, y, 50 + col * 400, CK_TOP + 8 + rw * 66 - wcScroll, 380, 56)) continue;
            auto it = std::find(wcSel.begin(), wcSel.end(), i);
            if (it == wcSel.end()) wcSel.push_back(i); else wcSel.erase(it);
            wcSave(); return true;
        }
    }
    return true;
}

// ---- timer tab
static void clkTimerStart() {
    int total = clkTH * 3600 + clkTM * 60 + clkTS; if (total <= 0) { toast("Set a time first"); return; }
    clkTTotal = (float)total; clkTLeft = (float)total; clkTEnd = SDL_GetTicks() + (Uint32)total * 1000; clkTRun = true; clkTPaused = false;
}
static void clkDrawTimer(Col bg) {
    bool active = clkTRun || clkTPaused;
    if (!active) {
        int vals[3] = {clkTH, clkTM, clkTS}; static const char* NM[3] = {"hours", "minutes", "seconds"};
        for (int k = 0; k < 3; k++) {
            int cx = 330 + k * 310;
            btn(cx - 60, 196, 120, 46, "+", Col{255, 255, 255}, Col{0, 80, 90});
            char b[8]; snprintf(b, sizeof b, "%02d", vals[k]); text(b, F130, cx, 244, WHITE, 1);
            text(NM[k], F22, cx, 380, WHITE, 1, 180);
            btn(cx - 60, 412, 120, 46, "-", Col{255, 255, 255}, Col{0, 80, 90});
            if (k < 2) text(":", F130, cx + 155, 236, WHITE, 1, 160);
        }
        static const int PR[6] = {1, 3, 5, 10, 15, 30};
        for (int i = 0; i < 6; i++) { char b[16]; snprintf(b, sizeof b, "%d min", PR[i]); btn(120 + i * 180, 490, 160, 50, b, clkPanel(bg, 0.22f), WHITE); }
        btn(W / 2 - 130, 570, 260, 66, "Start", Col{255, 255, 255}, Col{0, 80, 90});
    } else {
        int cx = W / 2, cy = 360;
        ring(cx, cy, 170, 8, clkPanel(bg, 0.25f), bg);
        float frac = clkTTotal > 0 ? std::max(0.0f, std::min(1.0f, clkTLeft / clkTTotal)) : 0;
        if (frac > 0.003f) clkArc(cx, cy, 166, frac, 8, WHITE);
        text(clkFmtT(clkTPaused ? clkTLeft : clkTLeft + 0.99f, false), F72, cx, cy - 50, WHITE, 1);
        text(clkTPaused ? "Paused" : "Running", F22, cx, cy + 40, WHITE, 1, 190);
        btn(W / 2 - 290, 570, 260, 66, clkTPaused ? "Resume" : "Pause", Col{255, 255, 255}, Col{0, 80, 90});
        btn(W / 2 + 30, 570, 260, 66, "Cancel", Col{200, 60, 60}, WHITE);
    }
}
static bool clkTimerTap(int x, int y) {
    bool active = clkTRun || clkTPaused;
    if (!active) {
        int* v[3] = {&clkTH, &clkTM, &clkTS}; int mx[3] = {23, 59, 59};
        for (int k = 0; k < 3; k++) {
            int cx = 330 + k * 310;
            if (inR(x, y, cx - 60, 196, 120, 46)) { *v[k] = (*v[k] + 1) % (mx[k] + 1); return true; }
            if (inR(x, y, cx - 60, 412, 120, 46)) { *v[k] = (*v[k] + mx[k]) % (mx[k] + 1); return true; }
        }
        static const int PR[6] = {1, 3, 5, 10, 15, 30};
        for (int i = 0; i < 6; i++) if (inR(x, y, 120 + i * 180, 490, 160, 50)) { clkTH = 0; clkTM = PR[i]; clkTS = 0; return true; }
        if (inR(x, y, W / 2 - 130, 570, 260, 66)) { clkTimerStart(); return true; }
    } else {
        if (inR(x, y, W / 2 - 290, 570, 260, 66)) {
            if (clkTPaused) { clkTEnd = SDL_GetTicks() + (Uint32)(clkTLeft * 1000); clkTPaused = false; clkTRun = true; }
            else { clkTPaused = true; clkTRun = false; }
            return true;
        }
        if (inR(x, y, W / 2 + 30, 570, 260, 66)) { clkTRun = false; clkTPaused = false; return true; }
    }
    return true;
}

// ---- stopwatch tab
static float clkSwNow() { return clkSwAcc + (clkSwRun ? (SDL_GetTicks() - clkSwT0) / 1000.0f : 0.0f); }
static void clkDrawStopwatch(Col bg) {
    text(clkFmtT(clkSwNow(), true), F130, W / 2, 190, WHITE, 1);
    btn(W / 2 - 390, 350, 250, 64, clkSwRun ? "Stop" : "Start", Col{255, 255, 255}, Col{0, 80, 90});
    btn(W / 2 - 125, 350, 250, 64, "Lap", clkSwRun ? clkPanel(bg, 0.28f) : clkPanel(bg, 0.12f), WHITE);
    btn(W / 2 + 140, 350, 250, 64, "Reset", clkPanel(bg, 0.28f), WHITE);
    SDL_Rect clip{0, 430, W, UIH - 430}; SDL_RenderSetClipRect(gR, &clip);
    int n = (int)clkLaps.size();
    for (int i = 0; i < n && i < 6; i++) {
        int idx = n - 1 - i; float lap = clkLaps[idx] - (idx > 0 ? clkLaps[idx - 1] : 0); int y = 436 + i * 38;
        rect(300, y, W - 600, 34, clkPanel(bg, i % 2 ? 0.1f : 0.16f));
        text("Lap " + std::to_string(idx + 1), F16, 320, y + 6, WHITE, 0, 200); text(clkFmtT(lap, true), F22, W / 2, y + 2, WHITE, 1); text(clkFmtT(clkLaps[idx], true), F16, W - 320, y + 6, WHITE, 2, 200);
    }
    SDL_RenderSetClipRect(gR, nullptr);
}
static bool clkSwTap(int x, int y) {
    if (inR(x, y, W / 2 - 390, 350, 250, 64)) { if (clkSwRun) { clkSwAcc = clkSwNow(); clkSwRun = false; } else { clkSwT0 = SDL_GetTicks(); clkSwRun = true; } return true; }
    if (inR(x, y, W / 2 - 125, 350, 250, 64)) { if (clkSwRun) clkLaps.push_back(clkSwNow()); return true; }
    if (inR(x, y, W / 2 + 140, 350, 250, 64)) { clkSwRun = false; clkSwAcc = 0; clkLaps.clear(); return true; }
    return true;
}

// ---- the app
static void drawClock(Col bg) {
    clkLoad();
    rect(0, 0, W, UIH, bg);
    text(clkHM(gNow.h, gNow.mi), F72, 50, 28, WHITE);
    { char s[8]; snprintf(s, sizeof s, ":%02d", gNow.s); text(s, F30, 50 + tw(clkHM(gNow.h, gNow.mi), F72) + 8, 62, WHITE, 0, 190); }
    text(dateStr(), F22, 54, 112, WHITE, 0, 200);
    static const char* TABS[4] = {"Alarms", "World clock", "Timer", "Stopwatch"};
    int tx = 520;
    for (int i = 0; i < 4; i++) {
        text(TABS[i], F30, tx, 56, WHITE, 0, i == clkTab ? 255 : 150);
        if (i == clkTab) rect(tx, 98, tw(TABS[i], F30), 3, WHITE);
        tx += tw(TABS[i], F30) + 34;
    }
    text(clkNextAlarmText(), F16, W - 40, 112, WHITE, 2, 170);
    rect(0, CK_TOP - 12, W, 1, WHITE, 40);
    switch (clkTab) {
        case 0: clkDrawAlarms(bg); if (clkEdit != -2) clkDrawEditor(bg); break;
        case 1: clkDrawWorld(bg); break;
        case 2: clkDrawTimer(bg); break;
        default: clkDrawStopwatch(bg); break;
    }
}
static void clkTap(int x, int y) {
    if (clkEdit != -2) { clkAlarmsTap(x, y, Col{0, 0, 0}); return; }
    if (y >= 40 && y < 112) {
        static const char* TABS[4] = {"Alarms", "World clock", "Timer", "Stopwatch"};
        int tx = 520;
        for (int i = 0; i < 4; i++) { int w = tw(TABS[i], F30); if (inR(x, y, tx - 12, 36, w + 24, 76)) { if (clkTab != i) { clkTab = i; clkScroll = 0; wcScroll = 0; wcPick = false; } return; } tx += w + 34; }
        return;
    }
    switch (clkTab) {
        case 0: clkAlarmsTap(x, y, Col{0, 0, 0}); break;
        case 1: clkWorldTap(x, y); break;
        case 2: clkTimerTap(x, y); break;
        default: clkSwTap(x, y); break;
    }
}
static void clkDrag(int, int dy) {
    if (clkEdit != -2) return;
    if (clkTab == 0) clkScroll = std::max(0, clkScroll - dy);
    else if (clkTab == 1) wcScroll = std::max(0, wcScroll - dy);
}
static bool clkBack() {
    if (clkEdit != -2) { hideKb(); clkEdit = -2; return true; }
    if (clkTab == 1 && wcPick) { wcPick = false; wcSave(); return true; }
    return false;
}
static bool clkLive(const Tile&, SDL_Rect, bool&) { return false; }
