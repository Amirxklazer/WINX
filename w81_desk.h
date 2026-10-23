// w81_desk.h - Desktop (Windows 8.1 taskbar + Win+X menu) and Help+Tips (included from win81.h)

// gSetOpenPage declared in main.cpp
static void openSettingsPage(int p) { gSetOpenPage = p; setPage = p; setSub = 0; gotoApp(A_SETTINGS); }

// ================================================================ Desktop
static bool deskMenu = false, deskHold = false, deskMenuJust = false; static Uint32 deskHoldT = 0;
static const int DK_TB = 48;
static void deskWallpaper(int x, int y, int w, int h) {
    const Theme& th = THEMES[gTheme];
    Col a = mix(th.accent, WHITE, 0.12f), b = mix(th.bottom, BLACK, 0.25f);
    for (int i = 0; i < h; i += 4) rect(x, y + i, w, std::min(4, h - i), mix(a, b, (float)i / (float)h));
    float ox = x + w * 0.80f, oy = y + h * 0.05f;
    for (int k = 0; k < 6; k++) tri(ox, oy, x + w * (-0.1f + k * 0.2f), (float)(y + h), x + w * (0.02f + k * 0.2f), (float)(y + h), WHITE, 14);
    logoPanes(x + w * 0.78f, y + h * 0.42f, h * 0.46f, WHITE, 34);
}
struct DkIcon { const char* name; int app; };
static const DkIcon DK_ICONS[] = {{"Recycle Bin", -1}, {"This PC", A_FILES}, {"OneDrive", A_ONEDRIVE}, {"Skype", A_SKYPE}, {"Control Panel", A_SETTINGS}, {"Help+Tips", A_HELP}};
static const int NDK = 6;
static void deskOpen(int a) {
    if (a < 0) { toast("The Recycle Bin is empty"); return; }
    if (!appAvail(a)) { toast(std::string(ADEF[a].name) + " isn't installed - get it from the Store"); return; }
    gotoApp(a);
}
static const int DK_PIN[3] = {A_FILES, A_BROWSER, A_STORE};
static void deskTaskIcon(int x, int a, bool running_, bool cur) {
    rect(x, UIH - DK_TB, 52, DK_TB, cur ? Col{70, 84, 110} : (running_ ? Col{44, 54, 74} : Col{26, 34, 50}), 255);
    Col c = ADEF[a].col; rect(x + 9, UIH - DK_TB + 8, 34, 32, c);
    std::string s = ADEF[a].name; text(std::string(1, s[0]), F22, x + 26, UIH - DK_TB + 8, WHITE, 1);
    if (running_) rect(x + 6, UIH - 4, 40, 3, Col{130, 190, 255});
}
struct DkMenuItem { const char* label; int act; };
static const DkMenuItem DK_MENU[] = {{"Desktop", 1}, {"System", 2}, {"Device Manager", 3}, {"Power Options", 4}, {"Task Manager", 5}, {"Command Prompt", 6}, {"Windows PowerShell", 7}, {"Control Panel", 8}, {"File Explorer", 9}, {"Search", 10}, {"Run", 11}, {"Shut down", 12}};
static const int NDM = 12;
static void drawDesktop() {
    deskWallpaper(0, 0, W, UIH - DK_TB);
    // desktop icons
    for (int i = 0; i < NDK; i++) {
        int x = 24 + (i / 5) * 120, y = 24 + (i % 5) * 112; int a = DK_ICONS[i].app;
        Col c = a < 0 ? Col{110, 150, 190} : ADEF[a].col; rect(x + 14, y, 72, 72, c, 235);
        if (a < 0) { outline(x + 34, y + 20, 32, 38, 3, WHITE); rect(x + 30, y + 16, 40, 4, WHITE); rect(x + 42, y + 10, 16, 6, WHITE); line(x + 43, y + 28, x + 43, y + 50, 2, WHITE); line(x + 50, y + 28, x + 50, y + 50, 2, WHITE); line(x + 57, y + 28, x + 57, y + 50, 2, WHITE); }
        else drawIcon(a, x + 50, y + 36, c);
        text(DK_ICONS[i].name, F16, x + 51, y + 80, BLACK, 1, 120); text(DK_ICONS[i].name, F16, x + 50, y + 79, WHITE, 1);
    }
    // taskbar
    rect(0, UIH - DK_TB, W, DK_TB, Col{26, 34, 50}, 255); rect(0, UIH - DK_TB, W, 1, Col{70, 84, 110});
    rect(0, UIH - DK_TB, 60, DK_TB, deskMenu ? Col{70, 84, 110} : Col{36, 46, 66}); winLogo(30, UIH - DK_TB / 2, 24, WHITE);
    int x = 66; bool shown[A_COUNT] = {false};
    for (int p : DK_PIN) { if (!appAvail(p)) continue; shown[p] = true; deskTaskIcon(x, p, running[p], p == curApp); x += 54; }
    for (int a = 0; a < A_COUNT; a++) { if (!running[a] || shown[a] || a == A_DESKTOP) continue; shown[a] = true; deskTaskIcon(x, a, true, false); x += 54; if (x > W - 380) break; }
    // tray
    int tx = W - 14; rect(W - 10, UIH - DK_TB, 10, DK_TB, Col{60, 74, 100});
    text(coTimeStr(), F16, W - 20, UIH - DK_TB + 6, WHITE, 2);
    { char d[24]; snprintf(d, sizeof d, "%d/%d/%d", gNow.mo, gNow.d, gNow.y); text(d, F16, W - 20, UIH - DK_TB + 26, WHITE, 2, 210); }
    tx = W - 150; { int sx = tx - 30; rect(sx, UIH - 30, 8, 10, WHITE); tri((float)sx + 8, (float)UIH - 30, (float)sx + 20, (float)UIH - 36, (float)sx + 20, (float)UIH - 14, WHITE); if (gSndVol > 0) { line(sx + 24, UIH - 31, sx + 24, UIH - 19, 2, WHITE, 200); } }
    drawWifiIcon(tx - 74, UIH - 16, gWifiBars, WHITE, 255);
    text(std::to_string((int)gBat) + "%", F16, tx - 140, UIH - DK_TB + 14, WHITE, 2);
    // Win+X menu
    if (deskMenu) {
        int mh = NDM * 42 + 12, my = UIH - DK_TB - mh;
        rect(0, my, 300, mh, Col{34, 44, 62}, 250); outline(0, my, 300, mh, 1, Col{90, 106, 140});
        for (int i = 0; i < NDM; i++) { rect(0, my + 6 + i * 42, 300, 1, Col{50, 62, 86}); text(DK_MENU[i].label, F22, 24, my + 12 + i * 42, WHITE); }
    }
}
static void deskMenuAct(int act) {
    deskMenu = false;
    switch (act) {
        case 2: openSettingsPage(10); break;
        case 3: openSettingsPage(7); break;
        case 4: openSettingsPage(7); break;
        case 5: deskOpen(A_TASKMGR); break;
        case 6: deskOpen(A_TERM); break;
        case 7: deskOpen(A_PS); break;
        case 8: openSettingsPage(0); break;
        case 9: deskOpen(A_FILES); break;
        case 10: closeOverlays(); searchOpen = true; gQuery.clear(); openKb(6); break;
        case 11: deskOpen(A_TERM); break;
        case 12: deskOpen(A_POWER); break;
        default: break;
    }
}
static void desktopTap(int x, int y) {
    if (deskMenuJust) { deskMenuJust = false; return; }
    if (deskMenu) {
        int mh = NDM * 42 + 12, my = UIH - DK_TB - mh;
        if (x < 300 && y >= my && y < UIH - DK_TB) { int i = (y - my - 6) / 42; if (i >= 0 && i < NDM) deskMenuAct(DK_MENU[i].act); return; }
        deskMenu = false; if (y < UIH - DK_TB) return;
    }
    if (y >= UIH - DK_TB) {
        if (x < 60) { closeApp(false); return; }
        if (x >= W - 330 && x < W - 8) { if (x > W - 120) deskOpen(A_CALENDAR); else if (x < W - 190 && x > W - 250) toast(netStatusText()); else if (x >= W - 190 && x < W - 130) openSettingsPage(4); return; }
        int tx = 66; bool shown[A_COUNT] = {false};
        for (int p : DK_PIN) { if (!appAvail(p)) continue; shown[p] = true; if (inR(x, y, tx, UIH - DK_TB, 52, DK_TB)) { deskOpen(p); return; } tx += 54; }
        for (int a = 0; a < A_COUNT; a++) { if (!running[a] || shown[a] || a == A_DESKTOP) continue; shown[a] = true; if (inR(x, y, tx, UIH - DK_TB, 52, DK_TB)) { deskOpen(a); return; } tx += 54; if (tx > W - 380) break; }
        return;
    }
    for (int i = 0; i < NDK; i++) { int ix = 24 + (i / 5) * 120, iy = 24 + (i % 5) * 112; if (inR(x, y, ix, iy, 100, 104)) { deskOpen(DK_ICONS[i].app); return; } }
}
static void desktopTouch(int phase, int x, int y) {
    if (phase == 0) { deskHold = x < 60 && y >= UIH - DK_TB; deskHoldT = SDL_GetTicks(); }
    else if (phase == 1 && (x >= 60 || y < UIH - DK_TB)) deskHold = false;
    else if (phase == 2) deskHold = false;
}
static void desktopUpdate() { if (curApp == A_DESKTOP && deskHold && SDL_GetTicks() - deskHoldT > 500) { deskHold = false; deskMenu = true; deskMenuJust = true; } }
static bool desktopBack() { if (deskMenu) { deskMenu = false; return true; } return false; }

// ================================================================ Help+Tips
struct HelpTopic { const char* title; const char* body; };
static const HelpTopic HELP[] = {
    {"Get around", "Tap the right edge of the screen for the charms: Search, Alerts, Start, Devices and Settings. Swipe left from the right edge for notifications. You can also touch and hold the middle of the right edge.\n\nSwipe right from the left edge to switch apps. Drag an app from the switcher onto the right half of the screen to snap it next to the one you're using.\n\nSwipe up from the bottom edge to see all your apps. Tap the top edge of an app to show its title bar with the minimise and close buttons."},
    {"Buttons", "+ is the Windows key. It goes back to Start, or to the last app. Touch and hold the Windows button under the screen for Cortana.\n\n- opens the charms. ZL opens notifications. B goes back. X closes the app you're in. The D-pad moves around Start and A opens the selected tile."},
    {"Start screen", "Touch and hold a tile to edit it. From the bar at the bottom you can unpin it, resize it, give it a colour, or turn its live tile off.\n\nLive tiles flip or slide to show news from the app. Choose the animation, how often it happens and whether tiles show names in PC settings > Personalize > Tiles. Colours and themes are in PC settings and the Theme Store."},
    {"Desktop", "Open Desktop from Start. It has a taskbar with the Start button, pinned apps, the apps you have open, a clock and the sound and Wi-Fi icons.\n\nTap the Windows logo on the taskbar to go back to Start. Touch and hold it for the quick links menu: System, Task Manager, Command Prompt, PowerShell, Control Panel, Search and Shut down."},
    {"Cortana", "Tap the microphone or the bar that says ask me anything and type. Voice recognition isn't available on Switch yet, so the microphone just opens the keyboard.\n\nTry: remind me to call mum in 20 minutes, set an alarm for 7:30 am, convert 5 km to miles, what's the weather tomorrow, take a note buy milk, how's my CPU, open Skype. In the menu you can see your reminders and choose your interests."},
    {"Skype and OneDrive", "Skype keeps contacts and chats on this console. There is no Skype network any more, so conversations are with the Echo / Sound Test Service, Cortana, Skype itself and any contacts you add for your own notes.\n\nOneDrive is a folder on your SD card (switch/WinNX/OneDrive). It is not synced to the cloud. Word, Excel and Notes can be copied into it automatically from PC settings > OneDrive."},
    {"Games", "The Games app shows the games installed on your Switch with their icons, and your homebrew. Tap a game to launch it. The game starts when this app closes.\n\nPausing a running game and replacing the HOME menu need a system-level home menu, which a normal homebrew app can't be. See PC settings > Home menu."},
    {"Sound and themes", "PC settings > Sound has three sound schemes (Windows Default, Chimes, Retro) or none, a volume, and switches for the start-up sound, notification sounds and touch sounds. The Theme Store has more than thirty colour themes."},
    {"If something goes wrong", "Hold B and Y while this app starts to jump to the original homebrew menu. If the app crashes it shows a blue screen next time you start it.\n\nIf you installed a custom HOME menu and the console won't start properly, delete atmosphere/contents/0100000000001000 from the SD card on a computer, or use PC settings > Home menu > Restore the original HOME menu."},
};
static const int NHELP = (int)(sizeof(HELP) / sizeof(HELP[0]));
static int helpSel = 0, helpScroll = 0;
static void drawHelp() {
    rect(0, 0, W, UIH, Col{250, 250, 252});
    rect(0, 0, 400, UIH, Col{232, 120, 20});
    text("Help+Tips", F44, 36, 26, WHITE);
    for (int i = 0; i < NHELP; i++) { int y = 110 + i * 56; if (i == helpSel) rect(0, y, 400, 52, Col{200, 96, 10}); text(HELP[i].title, F22, 36, y + 10, WHITE); }
    int x = 450, w = W - x - 50;
    text(HELP[helpSel].title, F44, x, 26, Col{232, 120, 20});
    SDL_Rect clip{x - 10, 100, w + 20, UIH - 100}; SDL_RenderSetClipRect(gR, &clip);
    auto lines = wrapText(HELP[helpSel].body, F22, w); int y = 110 - helpScroll;
    int total = (int)lines.size() * 32; helpScroll = std::max(0, std::min(helpScroll, std::max(0, total - (UIH - 130))));
    for (auto& l : lines) { text(l, F22, x, y, Col{50, 52, 58}); y += 32; }
    SDL_RenderSetClipRect(gR, nullptr);
}
static void helpTap(int x, int y) { if (x < 400 && y >= 110) { int i = (y - 110) / 56; if (i >= 0 && i < NHELP) { helpSel = i; helpScroll = 0; } } }
static void helpDrag(int, int dy) { helpScroll = std::max(0, helpScroll - dy); }
