// w82_ui.h - the Windows 8 icon set (app-bar glyphs, arrows, charm icons) drawn with primitives, plus round app-bar buttons and a gallery
enum { UG_PLAY, UG_STOP, UG_PAUSE, UG_SKIPA, UG_SKIPB, UG_FWD, UG_REW, UG_PREV, UG_NEXT, UG_OUT, UG_DOWN, UG_NO, UG_ADD, UG_REMOVE, UG_SETTINGS, UG_VIEW, UG_SHARE, UG_DEVICES, UG_SEARCH,
       UG_FAV, UG_FULL, UG_BACKWIN, UG_REFRESH, UG_SYNC, UG_YES, UG_KEYBOARD, UG_MAIL, UG_PHOTO, UG_MAPS, UG_TARGET, UG_LIST, UG_BARS, UG_LOCATION, UG_PAGE, UG_APP, UG_SOUND, UG_QUIET, UG_MUTE,
       UG_VOLUME, UG_UNAVAIL, UG_CLOUD, UG_BACK, UG_COUNT };
static const char* UG_NAMES[UG_COUNT] = {"Play", "Stop", "Pause", "Skip ahead", "Skip back", "Forward", "Back", "Previous", "Next", "Out", "Down", "No", "Add", "Remove", "Settings", "View", "Share", "Devices", "Search",
    "Favorite", "Full screen", "Back to window", "Refresh", "Sync", "Yes", "Keyboard", "Mail", "Photo", "Maps", "Target", "List", "Three bars", "Location", "Page", "App", "Sound", "Quiet sound", "Mute",
    "Volume", "Unavailable", "Skydrive", "Back"};
static void uiArc(int cx, int cy, int r, float a0, float a1, int th, Col c) { int n = 18; for (int i = 0; i < n; i++) { float a = a0 + (a1 - a0) * i / n, b = a0 + (a1 - a0) * (i + 1) / n; line(cx + (int)(sinf(a) * r), cy - (int)(cosf(a) * r), cx + (int)(sinf(b) * r), cy - (int)(cosf(b) * r), th, c); } }
static void uiArrow(int cx, int cy, int s, int dir, int th, Col c) {   // dir 0 right,1 down-right,2 down,3 down-left,4 left,5 up-left,6 up,7 up-right
    float a = dir * 0.785398f, dx = cosf(a), dy = sinf(a), px = -dy, py = dx; float L = (float)s;
    int x0 = cx - (int)(dx * L), y0 = cy - (int)(dy * L), x1 = cx + (int)(dx * L), y1 = cy + (int)(dy * L);
    line(x0, y0, x1, y1, th, c); line(x1, y1, x1 - (int)((dx - px) * L * 0.55f), y1 - (int)((dy - py) * L * 0.55f), th, c); line(x1, y1, x1 - (int)((dx + px) * L * 0.55f), y1 - (int)((dy + py) * L * 0.55f), th, c);
}
// draws glyph id centred at cx,cy, s = half size in pixels (14 = a 28px glyph)
static void uiGlyph(int id, int cx, int cy, int s, Col c, Col bg = Col{0, 0, 0}) {
    int th = std::max(2, s / 5), u = s;
    switch (id) {
        case UG_PLAY: tri((float)cx - u * 0.5f, (float)cy - u * 0.75f, (float)cx - u * 0.5f, (float)cy + u * 0.75f, (float)cx + u * 0.8f, (float)cy, c); break;
        case UG_STOP: rect(cx - u * 0.7f, cy - u * 0.7f, (int)(u * 1.4f), (int)(u * 1.4f), c); break;
        case UG_PAUSE: rect(cx - u * 0.65f, cy - u * 0.75f, (int)(u * 0.5f), (int)(u * 1.5f), c); rect(cx + u * 0.15f, cy - u * 0.75f, (int)(u * 0.5f), (int)(u * 1.5f), c); break;
        case UG_SKIPA: tri((float)cx - u * 0.7f, (float)cy - u * 0.7f, (float)cx - u * 0.7f, (float)cy + u * 0.7f, (float)cx + u * 0.4f, (float)cy, c); rect(cx + u * 0.5f, cy - u * 0.7f, std::max(2, (int)(u * 0.3f)), (int)(u * 1.4f), c); break;
        case UG_SKIPB: tri((float)cx + u * 0.7f, (float)cy - u * 0.7f, (float)cx + u * 0.7f, (float)cy + u * 0.7f, (float)cx - u * 0.4f, (float)cy, c); rect(cx - u * 0.8f, cy - u * 0.7f, std::max(2, (int)(u * 0.3f)), (int)(u * 1.4f), c); break;
        case UG_FWD: tri((float)cx - u * 0.9f, (float)cy - u * 0.65f, (float)cx - u * 0.9f, (float)cy + u * 0.65f, (float)cx, (float)cy, c); tri((float)cx, (float)cy - u * 0.65f, (float)cx, (float)cy + u * 0.65f, (float)cx + u * 0.9f, (float)cy, c); break;
        case UG_REW: tri((float)cx + u * 0.9f, (float)cy - u * 0.65f, (float)cx + u * 0.9f, (float)cy + u * 0.65f, (float)cx, (float)cy, c); tri((float)cx, (float)cy - u * 0.65f, (float)cx, (float)cy + u * 0.65f, (float)cx - u * 0.9f, (float)cy, c); break;
        case UG_PREV: uiArrow(cx, cy, (int)(u * 0.8f), 4, th, c); break;
        case UG_NEXT: uiArrow(cx, cy, (int)(u * 0.8f), 0, th, c); break;
        case UG_OUT: uiArrow(cx, cy, (int)(u * 0.8f), 6, th, c); break;
        case UG_DOWN: uiArrow(cx, cy, (int)(u * 0.8f), 2, th, c); break;
        case UG_BACK: uiArrow(cx, cy, (int)(u * 0.8f), 4, th, c); break;
        case UG_NO: line(cx - u * 0.6f, cy - u * 0.6f, cx + u * 0.6f, cy + u * 0.6f, th, c); line(cx + u * 0.6f, cy - u * 0.6f, cx - u * 0.6f, cy + u * 0.6f, th, c); break;
        case UG_ADD: line(cx - u * 0.75f, cy, cx + u * 0.75f, cy, th, c); line(cx, cy - u * 0.75f, cx, cy + u * 0.75f, th, c); break;
        case UG_REMOVE: line(cx - u * 0.75f, cy, cx + u * 0.75f, cy, th, c); break;
        case UG_SETTINGS: ring(cx, cy, (int)(u * 0.6f), th, c, bg); for (int k = 0; k < 8; k++) { float a = k * 0.785398f; line(cx + (int)(sinf(a) * u * 0.75f), cy - (int)(cosf(a) * u * 0.75f), cx + (int)(sinf(a) * u * 1.0f), cy - (int)(cosf(a) * u * 1.0f), th + 1, c); } break;
        case UG_VIEW: ring(cx, cy, (int)(u * 0.95f), th, c, bg); disc(cx, cy, (int)(u * 0.4f), c); uiArc(cx, cy, (int)(u * 0.6f), 3.6f, 5.7f, th, bg); break;
        case UG_SHARE: disc(cx - u * 0.6f, cy, (int)(u * 0.28f), c); disc(cx + u * 0.5f, cy - u * 0.55f, (int)(u * 0.28f), c); disc(cx + u * 0.5f, cy + u * 0.55f, (int)(u * 0.28f), c); line(cx - u * 0.6f, cy, cx + u * 0.5f, cy - u * 0.55f, th - 1, c); line(cx - u * 0.6f, cy, cx + u * 0.5f, cy + u * 0.55f, th - 1, c); break;
        case UG_DEVICES: outline(cx - u * 0.9f, cy - u * 0.5f, (int)(u * 1.3f), (int)(u * 0.9f), th, c); outline(cx - u * 0.1f, cy - u * 0.05f, (int)(u * 1.1f), (int)(u * 0.75f), th, c); break;
        case UG_SEARCH: ring(cx - u * 0.2f, cy - u * 0.2f, (int)(u * 0.6f), th, c, bg); line(cx + u * 0.25f, cy + u * 0.25f, cx + u * 0.8f, cy + u * 0.8f, th + 1, c); break;
        case UG_FAV: { float R = u * 1.0f, r2 = u * 0.42f; float px[10], py[10]; for (int i = 0; i < 10; i++) { float a = i * 0.628318f; float rr = (i & 1) ? r2 : R; px[i] = cx + sinf(a) * rr; py[i] = cy - cosf(a) * rr * 1.0f + u * 0.08f; } for (int i = 0; i < 10; i++) tri((float)cx, (float)cy, px[i], py[i], px[(i + 1) % 10], py[(i + 1) % 10], c); break; }
        case UG_FULL: for (int sx = -1; sx <= 1; sx += 2) for (int sy = -1; sy <= 1; sy += 2) { int x = cx + sx * (int)(u * 0.75f), y = cy + sy * (int)(u * 0.75f); line(x, y, x - sx * (int)(u * 0.5f), y - sy * (int)(u * 0.5f), th, c); line(x, y, x - sx * (int)(u * 0.5f), y, th, c); line(x, y, x, y - sy * (int)(u * 0.5f), th, c); } break;
        case UG_BACKWIN: for (int sx = -1; sx <= 1; sx += 2) for (int sy = -1; sy <= 1; sy += 2) { int x = cx + sx * (int)(u * 0.2f), y = cy + sy * (int)(u * 0.2f); line(x, y, x + sx * (int)(u * 0.55f), y + sy * (int)(u * 0.55f), th, c); line(x, y, x + sx * (int)(u * 0.55f), y, th, c); line(x, y, x, y + sy * (int)(u * 0.55f), th, c); } break;
        case UG_REFRESH: uiArc(cx, cy, (int)(u * 0.8f), 0.6f, 5.6f, th, c); tri((float)cx + u * 0.15f, (float)cy - u * 1.15f, (float)cx + u * 0.15f, (float)cy - u * 0.45f, (float)cx + u * 0.85f, (float)cy - u * 0.8f, c); break;
        case UG_SYNC: uiArc(cx, cy, (int)(u * 0.8f), 0.4f, 5.4f, th, c); break;
        case UG_YES: line(cx - u * 0.75f, cy + u * 0.05f, cx - u * 0.25f, cy + u * 0.6f, th + 1, c); line(cx - u * 0.25f, cy + u * 0.6f, cx + u * 0.8f, cy - u * 0.65f, th + 1, c); break;
        case UG_KEYBOARD: outline(cx - u * 1.0f, cy - u * 0.55f, (int)(u * 2.0f), (int)(u * 1.1f), th - 1, c); for (int i = 0; i < 4; i++) rect(cx - u * 0.75f + i * u * 0.45f, cy - u * 0.3f, 2, 2, c); for (int i = 0; i < 4; i++) rect(cx - u * 0.75f + i * u * 0.45f, cy - u * 0.05f, 2, 2, c); rect(cx - u * 0.5f, cy + u * 0.2f, (int)(u * 1.0f), 2, c); break;
        case UG_MAIL: rect(cx - u * 0.95f, cy - u * 0.65f, (int)(u * 1.9f), (int)(u * 1.3f), c); line(cx - u * 0.95f, cy - u * 0.6f, cx, cy + u * 0.1f, th - 1, bg); line(cx + u * 0.95f, cy - u * 0.6f, cx, cy + u * 0.1f, th - 1, bg); break;
        case UG_PHOTO: rect(cx - u * 0.95f, cy - u * 0.55f, (int)(u * 1.9f), (int)(u * 1.3f), c); rect(cx - u * 0.35f, cy - u * 0.8f, (int)(u * 0.7f), (int)(u * 0.3f), c); disc(cx, cy + u * 0.1f, (int)(u * 0.45f), bg); disc(cx, cy + u * 0.1f, (int)(u * 0.28f), c); break;
        case UG_MAPS: for (int i = 0; i < 3; i++) { quad(cx - u + i * u * 0.67f, cy - u * 0.7f, cx - u * 0.38f + i * u * 0.67f, cy - u * 0.7f + (i == 1 ? u * 0.15f : 0), cx - u * 0.38f + i * u * 0.67f, cy + u * 0.7f, cx - u + i * u * 0.67f, cy + u * 0.7f - (i == 1 ? u * 0.15f : 0), c, 255); } break;
        case UG_TARGET: ring(cx, cy, (int)(u * 0.95f), th - 1, c, bg); ring(cx, cy, (int)(u * 0.6f), th - 1, c, bg); disc(cx, cy, (int)(u * 0.25f), c); break;
        case UG_LIST: for (int i = 0; i < 3; i++) { disc(cx - u * 0.75f, cy - u * 0.65f + i * u * 0.65f, (int)(u * 0.14f) + 1, c); rect(cx - u * 0.4f, cy - u * 0.7f + i * u * 0.65f, (int)(u * 1.3f), std::max(2, (int)(u * 0.2f)), c); } break;
        case UG_BARS: for (int i = 0; i < 4; i++) rect(cx - u * 0.8f + i * u * 0.45f, cy + u * 0.8f - (i + 1) * u * 0.42f, std::max(2, (int)(u * 0.3f)), (int)((i + 1) * u * 0.42f), c); break;
        case UG_LOCATION: disc(cx, cy - u * 0.25f, (int)(u * 0.65f), c); tri((float)cx - u * 0.55f, (float)cy, (float)cx + u * 0.55f, (float)cy, (float)cx, (float)cy + u * 0.95f, c); disc(cx, cy - u * 0.25f, (int)(u * 0.25f), bg); break;
        case UG_PAGE: outline(cx - u * 0.6f, cy - u * 0.9f, (int)(u * 1.2f), (int)(u * 1.8f), th - 1, c); line(cx + u * 0.15f, cy - u * 0.9f, cx + u * 0.6f, cy - u * 0.45f, th - 1, c); break;
        case UG_APP: for (int k = 0; k < 8; k++) { float a = k * 0.785398f; line(cx, cy, cx + (int)(sinf(a) * u), cy - (int)(cosf(a) * u), th, c); } disc(cx, cy, (int)(u * 0.55f), c); break;
        case UG_SOUND: tri((float)cx - u * 0.1f, (float)cy - u * 0.7f, (float)cx - u * 0.1f, (float)cy + u * 0.7f, (float)cx - u * 0.9f, (float)cy, c); rect(cx - u * 0.9f, cy - u * 0.3f, (int)(u * 0.5f), (int)(u * 0.6f), c); break;
        case UG_QUIET: uiGlyph(UG_SOUND, cx - 3, cy, s, c, bg); uiArc(cx + u * 0.2f, cy, (int)(u * 0.5f), 1.0f, 2.1f, th - 1, c); break;
        case UG_MUTE: uiGlyph(UG_SOUND, cx - u * 0.2f, cy, s, c, bg); line(cx + u * 0.3f, cy - u * 0.4f, cx + u * 0.9f, cy + u * 0.4f, th - 1, c); line(cx + u * 0.9f, cy - u * 0.4f, cx + u * 0.3f, cy + u * 0.4f, th - 1, c); break;
        case UG_VOLUME: uiGlyph(UG_SOUND, cx - u * 0.35f, cy, s, c, bg); uiArc(cx + u * 0.2f, cy, (int)(u * 0.5f), 1.0f, 2.1f, th - 1, c); uiArc(cx + u * 0.2f, cy, (int)(u * 0.9f), 1.0f, 2.1f, th - 1, c); break;
        case UG_UNAVAIL: disc(cx, cy, (int)(u * 0.3f), c); for (int k = 0; k < 8; k++) { float a = k * 0.785398f; line(cx + (int)(sinf(a) * u * 0.55f), cy - (int)(cosf(a) * u * 0.55f), cx + (int)(sinf(a) * u * 0.95f), cy - (int)(cosf(a) * u * 0.95f), th - 1, c); } break;
        case UG_CLOUD: disc(cx - u * 0.45f, cy + u * 0.2f, (int)(u * 0.4f), c); disc(cx + u * 0.05f, cy - u * 0.1f, (int)(u * 0.55f), c); disc(cx + u * 0.6f, cy + u * 0.2f, (int)(u * 0.4f), c); rect(cx - u * 0.45f, cy + u * 0.2f, (int)(u * 1.05f), (int)(u * 0.4f), c); break;
        default: break; }
}
// round Windows 8 app-bar button: circle outline with a glyph, optional label under it
static void uiRoundBtn(int cx, int cy, int id, const char* label, Col fg, Col ring_bg, bool pressed = false, int r = 28) {
    if (pressed) disc(cx, cy, r, fg); ring(cx, cy, r, 3, pressed ? ring_bg : fg, pressed ? fg : ring_bg);
    uiGlyph(id, cx, cy, r / 2 + 1, pressed ? ring_bg : fg, pressed ? fg : ring_bg);
    if (label && *label) text(label, F16, cx, cy + r + 8, fg, 1);
}
static void drawIconGallery(int x0, int y0, int w) {
    int cols = 6, cw = w / cols; Col fg = Col{50, 52, 58}, bgc = Col{250, 250, 252};
    for (int i = 0; i < UG_COUNT; i++) { int cx = x0 + (i % cols) * cw + cw / 2, cy = y0 + 34 + (i / cols) * 84; uiRoundBtn(cx, cy, i, "", fg, bgc, false, 24); text(UG_NAMES[i], F16, cx, cy + 28, Col{110, 110, 120}, 1); }
    int ay = y0 + 34 + ((UG_COUNT + cols - 1) / cols) * 84 + 8; static const Col AC[8] = {{90, 90, 96}, {40, 150, 230}, {110, 190, 60}, {240, 90, 30}, {240, 110, 150}, {250, 150, 30}, {150, 50, 160}, {170, 180, 190}};
    for (int i = 0; i < 8; i++) { int cx = x0 + (i) * (w / 8) + w / 16; ring(cx, ay + 26, 24, 4, AC[i], bgc); uiArrow(cx, ay + 26, 12, i, 3, AC[i]); }
}
