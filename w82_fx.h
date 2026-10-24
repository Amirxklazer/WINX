// w82_fx.h - Windows Phone style turnstile transitions (app open/close, Start tiles) and the Windows flag Easter egg

static SDL_Texture* gTurnA = nullptr; static SDL_Texture* gTurnB = nullptr; static SDL_Texture* gTileScratch = nullptr;
static SDL_Texture* fxMake(int w, int h) { SDL_Texture* t = SDL_CreateTexture(gR, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, w, h); if (t) SDL_SetTextureBlendMode(t, SDL_BLENDMODE_BLEND); return t; }
static bool fxReady() { if (!gTurnA) gTurnA = fxMake(W, UIH); if (!gTurnB) gTurnB = fxMake(W, UIH); return gTurnA && gTurnB; }
// Draws a texture (or its sub-rectangle) swung about its LEFT edge by theta radians (0 = flat, pi/2 = edge on) with perspective.
// (x, y, w, h) is where the flat image would be; depth k controls how strongly the far edge shrinks.
static void fxTurn(SDL_Texture* tex, SDL_Rect src, int x, int y, int w, int h, float theta, float k, int alpha = 255) {
    if (!tex) return; const int N = 14; SDL_Vertex v[(N + 1) * 2]; int idx[N * 6]; float s = sinf(theta), c = cosf(theta);
    for (int i = 0; i <= N; i++) {
        float u = (float)i / N, sc = 1.0f / (1.0f + k * u * s), px = x + u * w * c * sc, hh = h * sc, py = y + (h - hh) / 2;
        float tu = (float)(src.x + (int)(u * src.w)) / (float)0, tv0 = 0; (void)tu; (void)tv0;
        int tw_, th_; SDL_QueryTexture(tex, nullptr, nullptr, &tw_, &th_);
        float fu = (src.x + u * src.w) / tw_, ft = (float)src.y / th_, fb = (float)(src.y + src.h) / th_;
        SDL_Color col = {255, 255, 255, (Uint8)alpha};
        v[i * 2].position = {px, py}; v[i * 2].color = col; v[i * 2].tex_coord = {fu, ft};
        v[i * 2 + 1].position = {px, py + hh}; v[i * 2 + 1].color = col; v[i * 2 + 1].tex_coord = {fu, fb};
    }
    for (int i = 0; i < N; i++) { int a = i * 2, b = a + 1, c2 = a + 2, d = a + 3; int* p = idx + i * 6; p[0] = a; p[1] = b; p[2] = c2; p[3] = b; p[4] = d; p[5] = c2; }
    SDL_RenderGeometry(gR, tex, v, (N + 1) * 2, idx, N * 6);
}
// Start <-> app: the Start screen swings away, then the app swings in (and the reverse when closing). e = 0..1 (0 = Start only).
static void fxAppTurn(float e) {
    const float MAXT = 1.36f;                   // about 78 degrees
    if (e < 0.5f) { float p = ease(e / 0.5f); fxTurn(gTurnA, SDL_Rect{0, 0, W, UIH}, 0, 0, W, UIH, MAXT * p, 0.55f, (int)(255 * (1 - 0.35f * p))); }
    else { float p = ease((e - 0.5f) / 0.5f); fxTurn(gTurnB, SDL_Rect{0, 0, W, UIH}, 0, 0, W, UIH, MAXT * (1 - p), 0.55f, (int)(255 * (0.65f + 0.35f * p))); }
}
// A single Start tile swinging in (Windows Phone 8.1 style feather turnstile). t = 1 means finished.
static void drawTileRect(const Tile& t, SDL_Rect r, float pressP, bool selected, bool editing, bool shadow);
static void fxTileTurn(const Tile& t, SDL_Rect r, float tt) {
    if (r.w > 700 || r.h > 700) { drawTileRect(t, r, 0, false, false, false); return; }
    if (!gTileScratch) gTileScratch = fxMake(700, 700); if (!gTileScratch) { drawTileRect(t, r, 0, false, false, false); return; }
    SDL_Texture* prev = SDL_GetRenderTarget(gR); SDL_Rect oc; SDL_RenderGetClipRect(gR, &oc); SDL_RenderSetClipRect(gR, nullptr);
    SDL_SetRenderTarget(gR, gTileScratch); SDL_SetRenderDrawColor(gR, 0, 0, 0, 0); SDL_RenderClear(gR);
    drawTileRect(t, SDL_Rect{0, 0, r.w, r.h}, 0, false, false, false);
    SDL_SetRenderTarget(gR, prev); SDL_RenderSetClipRect(gR, oc.w > 0 ? &oc : nullptr);
    float th = (1.0f - tt) * 1.45f; fxTurn(gTileScratch, SDL_Rect{0, 0, r.w, r.h}, r.x, r.y, r.w, r.h, th, 0.35f, (int)(255 * (0.25f + 0.75f * tt)));
}

// ================================================================ Windows flag Easter egg (tap the Windows 8.1 edition in PC info 8 times)
static bool gEgg = false; static float gEggT = 0; static int gEggTaps = 0; static Uint32 gEggLast = 0;
static void eggTrigger() { gEgg = true; gEggT = 0; sndPlay(SND_STARTUP); }
static void eggCount() {   // call on every tap of the "Windows 8.1" text
    Uint32 n = SDL_GetTicks(); if (n - gEggLast > 2500) gEggTaps = 0; gEggLast = n; gEggTaps++;
    if (gEggTaps >= 8) { gEggTaps = 0; eggTrigger(); } else if (gEggTaps >= 4) toast(std::to_string(8 - gEggTaps) + " more...");
}
static void eggTap() { if (gEggT > 1.5f) gEgg = false; }
static void eggUpdate(float dt) { if (gEgg) { gEggT += dt; if (gEggT > 14) gEgg = false; } }
static void eggDraw() {
    if (!gEgg) return; float t = gEggT;
    float fadeIn = std::min(1.0f, t / 0.4f); rect(0, 0, W, UIH, WHITE, (int)(255 * fadeIn));
    static const float P[4][4][2] = {{{55.2f, 77.5f}, {107.2f, 67.0f}, {107.8f, 124.2f}, {55.5f, 124.5f}}, {{115.8f, 65.2f}, {200.5f, 49.5f}, {200.2f, 124.5f}, {115.8f, 124.2f}},
                                     {{55.8f, 131.8f}, {107.8f, 132.2f}, {107.2f, 188.8f}, {55.2f, 178.2f}}, {{115.8f, 132.2f}, {200.0f, 131.8f}, {200.5f, 206.2f}, {115.8f, 195.5f}}};
    float S = 2.9f, ox = W / 2 - 128 * S, oy = 30 - 49 * S; static const Col PC[4] = {{0, 188, 242}, {0, 160, 232}, {0, 166, 236}, {0, 132, 214}};
    // the thin vertical line first (as in the Windows 8 start-up animation), then the panes grow out of it
    float lineP = std::max(0.0f, std::min(1.0f, (t - 0.3f) / 0.8f)); if (t < 2.0f) rect(W / 2 - 2, UIH / 2 - (int)(250 * lineP) + 90, 4, (int)(250 * lineP), PC[0]);
    for (int k = 0; k < 4; k++) {
        float g = std::max(0.0f, std::min(1.0f, (t - 1.0f - k * 0.18f) / 0.9f)); g = 1 - (1 - g) * (1 - g); if (g <= 0) continue;
        const int N = 20; float amp = std::min(1.0f, std::max(0.0f, (t - 2.4f) / 1.5f)) * 7.0f; float ph = t * 3.2f;
        for (int i = 0; i < N; i++) {
            float u0 = (float)i / N, u1 = (float)(i + 1) / N; float xs[2], tp[2], bt[2];
            for (int e = 0; e < 2; e++) { float u = e ? u1 : u0; float xT = P[k][0][0] + (P[k][1][0] - P[k][0][0]) * u, yT = P[k][0][1] + (P[k][1][1] - P[k][0][1]) * u, xB = P[k][3][0] + (P[k][2][0] - P[k][3][0]) * u, yB = P[k][3][1] + (P[k][2][1] - P[k][3][1]) * u;
                float x = (xT + xB) / 2; float wave = amp * sinf((x - 55) * 0.055f - ph) * ((x - 55) / 150.0f); float cyc = 128; yT = cyc + (yT - cyc) * g; yB = cyc + (yB - cyc) * g; xs[e] = ox + x * S; tp[e] = oy + (yT + wave) * S; bt[e] = oy + (yB + wave) * S; }
            Col c = mix(PC[k], WHITE, 0.0f); float sh = 0.08f * sinf((u0 * 3.0f + k) - ph * 0.5f) * (amp / 7.0f); c = mix(c, sh > 0 ? WHITE : BLACK, fabsf(sh));
            quad(xs[0], tp[0], xs[1], tp[1], xs[1], bt[1], xs[0], bt[0], c, 255);
        }
    }
    float ta = std::max(0.0f, std::min(1.0f, (t - 3.0f) / 0.8f)); text("Windows 8.1", F72, W / 2, UIH - 130, Col{0, 114, 198}, 1, (int)(255 * ta));
    if (t > 1.5f) text("Tap to close", F16, W / 2, UIH - 40, Col{120, 130, 140}, 1, 180);
}
