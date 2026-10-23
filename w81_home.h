// w81_home.h - installed Switch games, HOME-menu (QLaunch) manager and the new PC settings pages (included from main.cpp after setRow)

// ================================================================ installed games (ns service)
struct NsGame { u64 id = 0; std::string name; SDL_Texture* icon = nullptr; bool tried = false, ok = false; };
static std::vector<NsGame> gNsGames;
static bool gNsScanned = false;
static std::string gNsMsg;
static int gmTab = 0, gmSel = -1, gmLoadIdx = 0;

static std::string hex64(u64 v) { char b[24]; snprintf(b, sizeof b, "%016llX", (unsigned long long)v); return b; }
static void gmFreeIcons() { for (auto& g : gNsGames) if (g.icon) { SDL_DestroyTexture(g.icon); g.icon = nullptr; } }

static void gmScanNs() {
    gmFreeIcons(); gNsGames.clear(); gNsMsg.clear(); gNsScanned = true; gmLoadIdx = 0;
#ifdef WNX_HOST
    static const char* N[] = {"The Legend of Zelda: Tears of the Kingdom", "Super Mario Odyssey", "Mario Kart 8 Deluxe", "Animal Crossing: New Horizons", "Splatoon 3", "Metroid Dread", "Hades", "Celeste", "Stardew Valley", "Hollow Knight", "Pokemon Scarlet", "Fire Emblem: Three Houses", "Kirby and the Forgotten Land", "Super Smash Bros. Ultimate"};
    for (int i = 0; i < 14; i++) { NsGame g; g.id = 0x0100000000010000ULL + i * 0x1000; g.name = N[i]; g.tried = true; g.ok = true; gNsGames.push_back(g); }
#else
    Result rc = nsInitialize();
    if (R_FAILED(rc)) { char b[96]; snprintf(b, sizeof b, "Couldn't read the list of installed games (error 0x%X).", rc); gNsMsg = b; return; }
    std::vector<NsApplicationRecord> recs(512); s32 cnt = 0;
    rc = nsListApplicationRecord(recs.data(), (s32)recs.size(), 0, &cnt);
    if (R_FAILED(rc)) { char b[96]; snprintf(b, sizeof b, "Couldn't read the list of installed games (error 0x%X).", rc); gNsMsg = b; return; }
    for (s32 i = 0; i < cnt; i++) { NsGame g; g.id = recs[i].application_id; g.name = hex64(g.id); gNsGames.push_back(g); }
#endif
}
// one title per frame: name and icon from its control data
static void gmLoadOne() {
#ifndef WNX_HOST
    while (gmLoadIdx < (int)gNsGames.size() && gNsGames[gmLoadIdx].tried) gmLoadIdx++;
    if (gmLoadIdx >= (int)gNsGames.size()) return;
    NsGame& g = gNsGames[gmLoadIdx++]; g.tried = true;
    NsApplicationControlData* d = (NsApplicationControlData*)malloc(sizeof(NsApplicationControlData));
    if (!d) return;
    u64 got = 0;
    if (R_SUCCEEDED(nsGetApplicationControlData(NsApplicationControlSource_Storage, g.id, d, sizeof(NsApplicationControlData), &got)) && got > sizeof(NacpStruct)) {
        NacpLanguageEntry* le = nullptr;
        if (R_SUCCEEDED(nacpGetLanguageEntry(&d->nacp, &le)) && le && le->name[0]) g.name = le->name;
        else for (int i = 0; i < 16; i++) { const char* nm = (const char*)d + i * 0x300; if (nm[0]) { g.name = std::string(nm, strnlen(nm, 0x200)); break; } }  // NACP: 16 language entries of 0x300 bytes, name first
        size_t isz = (size_t)(got - sizeof(NacpStruct));
        if (isz > 100 && isz <= sizeof(d->icon)) {
            SDL_Surface* sf = IMG_Load_RW(SDL_RWFromConstMem(d->icon, (int)isz), 1);
            if (sf) { g.icon = SDL_CreateTextureFromSurface(gR, sf); SDL_FreeSurface(sf); }
        }
        g.ok = true;
    }
    free(d);
#endif
}
static void gmInit() { gmSel = -1; gmTab = 0; if (!gNsScanned) gmScanNs(); }
static void gmUpdate() { if (curApp == A_GAMES && gmTab == 0 && gNsScanned) { gmLoadOne(); } }
static void gmLaunch(int i) {
    if (i < 0 || i >= (int)gNsGames.size()) return;
#ifdef WNX_HOST
    toast("Launching isn't available in the preview");
#else
    saveAll();
    Result rc = appletRequestLaunchApplication(gNsGames[i].id, NULL);
    if (R_SUCCEEDED(rc)) { gRun = false; }
    else { char b[120]; snprintf(b, sizeof b, "Couldn't start the game (error 0x%X)", rc); toast(b); }
#endif
}

static const int GM_COLS = 6, GM_CW = 190, GM_CH = 214, GM_Y0 = 172;
static void drawGames(Col bg) {
    rect(0, 0, W, UIH, bg);
    appTitle("Games");
    static const char* TABS[2] = {"Games", "Homebrew"}; int tx = 76;
    for (int i = 0; i < 2; i++) {
        text(TABS[i], F30, tx, 112, WHITE, 0, i == gmTab ? 255 : 150);
        if (i == gmTab) rect(tx, 152, tw(TABS[i], F30), 3, WHITE);
        tx += tw(TABS[i], F30) + 50;
    }
    if (gmTab == 0) {
        int n = (int)gNsGames.size(); int rows = (n + GM_COLS - 1) / GM_COLS;
        int maxS = std::max(0, rows * GM_CH - (UIH - GM_Y0) + 10);
        gamesScroll = std::max(0, std::min(gamesScroll, maxS));
        SDL_Rect clip = {0, GM_Y0 - 8, W, UIH - GM_Y0 + 8}; SDL_RenderSetClipRect(gR, &clip);
        for (int i = 0; i < n; i++) {
            int x = 70 + (i % GM_COLS) * GM_CW, y = GM_Y0 + (i / GM_COLS) * GM_CH - gamesScroll;
            if (y + GM_CH < GM_Y0 - 8 || y > UIH) continue;
            NsGame& g = gNsGames[i];
            if (g.icon) { SDL_Rect d = {x, y, 164, 164}; SDL_RenderCopy(gR, g.icon, nullptr, &d); }
            else { rect(x, y, 164, 164, WHITE, 40); text(g.tried ? "?" : "...", F44, x + 82, y + 50, WHITE, 1, 170); }
            text(fitTxt(g.name, F16, 172), F16, x, y + 170, WHITE);
        }
        SDL_RenderSetClipRect(gR, nullptr);
        if (n == 0) {
            text(gNsMsg.empty() ? "No installed games found." : "Games can't be listed", F30, 76, 200, WHITE, 0, 220);
            int y = 250; for (auto& l : wrapText(gNsMsg, F22, 900)) { text(l, F22, 76, y, WHITE, 0, 190); y += 30; }
        }
        if (gmSel >= 0 && gmSel < n) {
            NsGame& g = gNsGames[gmSel];
            rect(0, 0, W, UIH, BLACK, 200);
            rect(180, 120, 920, 400, Col{40, 40, 46});
            if (g.icon) { SDL_Rect d = {210, 150, 256, 256}; SDL_RenderCopy(gR, g.icon, nullptr, &d); } else rect(210, 150, 256, 256, WHITE, 40);
            int y = 150; for (auto& l : wrapText(g.name, F44, 560)) { text(l, F44, 500, y, WHITE); y += 54; if (y > 300) break; }
            text("Title ID " + hex64(g.id), F16, 500, y + 6, WHITE, 0, 150);
            text("The game opens when this app closes.", F16, 500, y + 34, WHITE, 0, 150);
            btn(500, 440, 260, 56, "Launch", Col{0, 140, 70}, WHITE);
            btn(780, 440, 200, 56, "Cancel", Col{90, 90, 98}, WHITE);
        }
    } else {
        text("Tap to launch homebrew from /switch", F22, 76, 168, WHITE, 0, 190);
        int n = (int)gGames.size();
        int top = 206, maxS = std::max(0, n * 70 - (UIH - top));
        gamesScroll = std::max(0, std::min(gamesScroll, maxS));
        SDL_Rect clip = {0, top, W, UIH - top}; SDL_RenderSetClipRect(gR, &clip);
        for (int i = 0; i < n; i++) {
            int y = top + i * 70 - gamesScroll;
            if (y + 64 < top || y > UIH) continue;
            rect(70, y, 1140, 64, WHITE, 28);
            text(gGames[i].name, F30, 90, y + 14, WHITE);
            text(gGames[i].path.substr(5), F16, 1190, y + 22, WHITE, 2, 150);
        }
        SDL_RenderSetClipRect(gR, nullptr);
        if (n == 0) text("No .nro files found in /switch", F30, 76, 230, WHITE, 0, 200);
    }
}
static void gamesTap(int x, int y) {
    if (gmSel >= 0) {
        if (inR(x, y, 500, 440, 260, 56)) { int i = gmSel; gmSel = -1; gmLaunch(i); }
        else if (inR(x, y, 780, 440, 200, 56) || !inR(x, y, 180, 120, 920, 400)) gmSel = -1;
        return;
    }
    if (y >= 100 && y < 164) { int tx = 76; static const char* TABS[2] = {"Games", "Homebrew"}; for (int i = 0; i < 2; i++) { int w = tw(TABS[i], F30); if (inR(x, y, tx - 10, 100, w + 20, 64)) { if (gmTab != i) { gmTab = i; gamesScroll = 0; } return; } tx += w + 50; } return; }
    if (gmTab == 0) {
        if (y < GM_Y0) return;
        int c = (x - 70) / GM_CW, r = (y - GM_Y0 + gamesScroll) / GM_CH, i = r * GM_COLS + c;
        if (x >= 70 && c < GM_COLS && i >= 0 && i < (int)gNsGames.size() && ((x - 70) % GM_CW) < 164 && ((y - GM_Y0 + gamesScroll) % GM_CH) < 190) gmSel = i;
    } else {
        int top = 206; if (y < top) return;
        int i = (y - top + gamesScroll) / 70;
        if (i >= 0 && i < (int)gGames.size() && ((y - top + gamesScroll) % 70) < 64) {
            saveAll();
            if (R_SUCCEEDED(envSetNextLoad(gGames[i].path.c_str(), gGames[i].path.c_str()))) gRun = false;
        }
    }
}
static bool gamesBack() { if (gmSel >= 0) { gmSel = -1; return true; } return false; }

// ================================================================ HOME menu (QLaunch) manager
// Atmosphere replaces the HOME menu (title 0100000000001000) with whatever is in this folder.
static const char* QL_DIR = "sdmc:/atmosphere/contents/0100000000001000";
static std::string qlBackupDir() { return DIR_ + "qlaunch_override_backup"; }
static std::string qlPackageDir() { return DIR_ + "homemenu"; }
static bool dirExists9(const std::string& p) { struct stat st; return stat(p.c_str(), &st) == 0 && S_ISDIR(st.st_mode); }
static bool copyTree9(const std::string& from, const std::string& to, int depth = 0) {
    if (depth > 6) return false;
    mkdir(to.c_str(), 0777);
    DIR* d = opendir(from.c_str()); if (!d) return false;
    bool ok = true; struct dirent* e;
    while ((e = readdir(d))) {
        std::string n = e->d_name; if (n == "." || n == "..") continue;
        std::string a = from + "/" + n, b = to + "/" + n;
        if (dirExists9(a)) ok = copyTree9(a, b, depth + 1) && ok; else ok = copyFile(a, b) && ok;
    }
    closedir(d); return ok;
}
static bool qlActive, qlBackup, qlPackage, qlAtmos; static std::string qlMsg; static int qlConfirm = 0, qlWhich = 0;
static void qlRefresh() { qlActive = dirExists9(QL_DIR); qlBackup = dirExists9(qlBackupDir()); qlPackage = dirExists9(qlPackageDir()); qlAtmos = dirExists9("sdmc:/atmosphere"); }
static void qlRestoreOriginal() {
    if (!qlActive) { qlMsg = "The original HOME menu is already active."; return; }
    std::string dst = qlBackupDir(); int k = 1;
    while (dirExists9(dst)) dst = qlBackupDir() + "_" + std::to_string(k++);
    if (rename(QL_DIR, dst.c_str()) == 0) qlMsg = "Done. The original HOME menu is back. Restart the console to see it. Your custom one is saved in switch/WinNX.";
    else qlMsg = "Couldn't move the folder. On a computer, delete atmosphere/contents/0100000000001000 from the SD card.";
    qlRefresh();
}
static void qlReenable() {
    if (qlActive) { qlMsg = "A custom HOME menu is already installed."; return; }
    if (!qlBackup) { qlMsg = "There is no saved custom HOME menu to bring back."; return; }
    if (rename(qlBackupDir().c_str(), QL_DIR) == 0) qlMsg = "Done. Restart the console to use your custom HOME menu."; else qlMsg = "Couldn't move the folder back.";
    qlRefresh();
}
static void qlInstallPackage() {
    if (!qlPackage) { qlMsg = "Put your HOME menu package in switch/WinNX/homemenu on the SD card first."; return; }
    if (qlActive) { qlMsg = "A custom HOME menu is already installed. Restore the original first."; return; }
    mkdir("sdmc:/atmosphere/contents", 0777);
    if (copyTree9(qlPackageDir(), QL_DIR)) qlMsg = "Installed. Restart the console to use it. If the console won't start, restore the original here or delete atmosphere/contents/0100000000001000 on a computer.";
    else { qlMsg = "Copying failed part-way; removing the incomplete copy is safest. Use Restore the original HOME menu."; }
    qlRefresh();
}
static void drawHomePage(int X) {
    qlRefresh();
    text("Home menu", F44, X, 40, Col{70, 90, 120});
    Col g1{60, 60, 66}, g2{120, 120, 128};
    text("Windows 8.1 is a homebrew app. It can't replace the HOME menu itself or pause a", F16, X, 104, g2);
    text("running game: that needs a system-level home menu, which is a separate install.", F16, X, 126, g2);
    text(std::string("Atmosph\xC3\xA8re found: ") + (qlAtmos ? "Yes" : "No"), F22, X, 168, g1);
    text(std::string("Custom HOME menu installed: ") + (qlActive ? "Yes" : "No - the original is active"), F22, X, 202, g1);
    text(std::string("Saved custom HOME menu: ") + (qlBackup ? "Yes" : "None") + "     Package ready: " + (qlPackage ? "Yes" : "None"), F22, X, 236, g1);
    bool c1 = qlConfirm > 0 && qlWhich == 1, c2 = qlConfirm > 0 && qlWhich == 2;
    btn(X, 290, 520, 54, c1 ? "Tap again to confirm" : "Restore the original HOME menu", c1 ? Col{209, 52, 56} : SET_BLUE, WHITE);
    btn(X, 356, 520, 54, "Bring back my custom HOME menu", qlBackup && !qlActive ? Col{110, 110, 118} : Col{190, 190, 196}, WHITE);
    btn(X, 422, 520, 54, c2 ? "Tap again to confirm" : "Install package from switch/WinNX/homemenu", c2 ? Col{209, 52, 56} : (qlPackage && !qlActive ? Col{110, 110, 118} : Col{190, 190, 196}), WHITE);
    int my = 492; for (auto& l : wrapText(qlMsg, F16, 700)) { text(l, F16, X, my, Col{190, 110, 0}); my += 22; }
    text("Rescue: on a computer, delete atmosphere/contents/0100000000001000 on the SD card.", F16, X, 626, g2);
}
static void homePageTap(int X, int x, int y) {
    if (inR(x, y, X, 290, 520, 54)) { if (qlConfirm > 0 && qlWhich == 1) { qlConfirm = 0; qlRestoreOriginal(); } else { qlWhich = 1; qlConfirm = 4; qlMsg.clear(); } }
    else if (inR(x, y, X, 356, 520, 54)) qlReenable();
    else if (inR(x, y, X, 422, 520, 54)) { if (qlConfirm > 0 && qlWhich == 2) { qlConfirm = 0; qlInstallPackage(); } else { qlWhich = 2; qlConfirm = 4; qlMsg.clear(); } }
}

// ================================================================ new PC settings pages
static void drawSoundPage(int X) {
    text("Sound", F44, X, 40, Col{70, 90, 120});
    text("Sound scheme", F22, X, 108, Col{60, 60, 66});
    static const char* SN[4] = {"Windows Default", "Chimes", "Retro", "No sounds"};
    for (int i = 0; i < 4; i++) {
        int y = 144 + i * 52;
        rect(X, y, 360, 46, i == gSndScheme ? SET_BLUE : Col{232, 233, 237});
        text(SN[i], F22, X + 18, y + 8, i == gSndScheme ? WHITE : Col{60, 60, 66});
        if (i < 3) btn(X + 380, y, 120, 46, "Preview", Col{232, 233, 237}, Col{50, 50, 55});
    }
    text("Volume", F22, X, 372, Col{60, 60, 66});
    btn(X + 110, 366, 60, 40, "-", Col{232, 233, 237}, Col{50, 50, 55});
    rect(X + 186, 380, 300, 12, Col{225, 225, 230}); rect(X + 186, 380, gSndVol * 30, 12, SET_BLUE);
    btn(X + 502, 366, 60, 40, "+", Col{232, 233, 237}, Col{50, 50, 55});
    setRow(436, "Start-up sound", "Plays when Windows starts", gSndStartup);
    setRow(506, "Notification sounds", "A sound when an alert arrives", gSndNotify);
    setRow(576, "Touch sounds", "A soft click when you tap buttons", gSndTouch);
}
static void soundPageTap(int X, int x, int y) {
    for (int i = 0; i < 4; i++) { int yy = 144 + i * 52; if (inR(x, yy ? y : y, X, yy, 360, 46) && inR(x, y, X, yy, 360, 46)) { gSndScheme = i; win9Save(); if (i < 3) sndPreview(i, SND_STARTUP); return; } if (i < 3 && inR(x, y, X + 380, yy, 120, 46)) { sndPreview(i, SND_STARTUP); return; } }
    if (inR(x, y, X + 110, 366, 60, 40)) { gSndVol = std::max(0, gSndVol - 1); win9Save(); sndPreview(gSndScheme == 3 ? 0 : gSndScheme, SND_NOTIFY); }
    else if (inR(x, y, X + 502, 366, 60, 40)) { gSndVol = std::min(10, gSndVol + 1); win9Save(); sndPreview(gSndScheme == 3 ? 0 : gSndScheme, SND_NOTIFY); }
    else if (inR(x, y, 1100, 426, 160, 70)) { gSndStartup = !gSndStartup; win9Save(); }
    else if (inR(x, y, 1100, 496, 160, 70)) { gSndNotify = !gSndNotify; win9Save(); }
    else if (inR(x, y, 1100, 566, 160, 70)) { gSndTouch = !gSndTouch; win9Save(); }
}
static void drawTilesSub(int X) {
    text("Live tile animation", F22, X, 108, Col{60, 60, 66});
    static const char* AN[3] = {"Off", "Flip", "Slide"};
    for (int i = 0; i < 3; i++) { int x = X + i * 170; rect(x, 148, 156, 52, i == gFlipMode ? SET_BLUE : Col{232, 233, 237}); text(AN[i], F22, x + 78, 158, i == gFlipMode ? WHITE : Col{60, 60, 66}, 1); }
    text("Time between animations", F22, X, 226, Col{60, 60, 66});
    btn(X, 266, 60, 46, "-", Col{232, 233, 237}, Col{50, 50, 55});
    text(std::to_string(gFlipSecs) + " seconds", F22, X + 150, 274, Col{60, 60, 66}, 1);
    btn(X + 240, 266, 60, 46, "+", Col{232, 233, 237}, Col{50, 50, 55});
    setRow(346, "Show app names on tiles", "Turn off for a cleaner Start screen", gTileNames);
    setRow(426, "Start on the Desktop", "Go straight to the Desktop when Windows starts", gBootDesk);
    btn(X, 520, 360, 52, "Reset all tile colours", SET_BLUE, WHITE);
    text("Touch and hold a tile on Start, then choose Colour to change it.", F16, X, 586, Col{120, 120, 128});
}
static void tilesSubTap(int X, int x, int y) {
    for (int i = 0; i < 3; i++) if (inR(x, y, X + i * 170, 148, 156, 52)) { gFlipMode = i; win9Save(); return; }
    if (inR(x, y, X, 266, 60, 46)) { gFlipSecs = std::max(3, gFlipSecs - 1); win9Save(); }
    else if (inR(x, y, X + 240, 266, 60, 46)) { gFlipSecs = std::min(30, gFlipSecs + 1); win9Save(); }
    else if (inR(x, y, 1100, 336, 160, 70)) { gTileNames = !gTileNames; win9Save(); }
    else if (inR(x, y, 1100, 416, 160, 70)) { gBootDesk = !gBootDesk; win9Save(); }
    else if (inR(x, y, X, 520, 360, 52)) { for (auto& t : gTiles) t.tint = 0; saveLayout(); toast("Tile colours reset"); }
}
static void drawOneDrivePage(int X) {
    text("OneDrive", F44, X, 40, Col{70, 90, 120});
    text("Your OneDrive folder is switch/WinNX/OneDrive on the SD card.", F22, X, 118, Col{60, 60, 66});
    text("It is not synced to the cloud: it keeps your files on this console.", F16, X, 152, Col{120, 120, 128});
    setRow(206, "Back up my documents", "Word, Excel, Notes, To-do and Calendar are copied every two minutes", gOdAuto);
    btn(X, 316, 360, 52, "Open OneDrive", SET_BLUE, WHITE);
}
static void oneDrivePageTap(int X, int x, int y) {
    if (inR(x, y, 1100, 196, 160, 70)) { gOdAuto = !gOdAuto; win9Save(); }
    else if (inR(x, y, X, 316, 360, 52)) { gotoApp(A_ONEDRIVE); }
}
static void drawPcInfoPage(int X) {
    text("PC info", F44, X, 40, Col{70, 90, 120});
    text("Windows edition", F22, X, 112, Col{60, 60, 66});
    text("Windows 8.1", F44, X, 142, Col{0, 114, 198});
    text("Version 0.9 for Nintendo Switch", F16, X, 198, Col{120, 120, 128});
    tmSample();
    char b[96]; Col k{120, 120, 128}, v{50, 50, 55}; int y = 244;
    auto row = [&](const char* a, const std::string& s) { text(a, F16, X, y, k); text(s, F22, X + 260, y - 4, v); y += 40; };
    row("PC name", "Nintendo Switch");
    row("Owner", gUser);
    row("Processor", "NVIDIA Tegra X1, ARM Cortex-A57");
    row("System type", "64-bit, ARM-based");
    snprintf(b, sizeof b, "%.0f MB of %.0f MB in use", gTmMemUsedMB, gTmMemTotalMB); row("Memory for apps", b);
    snprintf(b, sizeof b, "%u%%", (unsigned)gBat); row("Battery", b);
    int up = (int)(SDL_GetTicks() / 1000); snprintf(b, sizeof b, "%d:%02d:%02d", up / 3600, (up / 60) % 60, up % 60); row("Up time", b);
    row("Network", netStatusText());
}
