// w82_rec.h - Sound Recorder: real microphone capture (libnx audin), WAV files, playback (included from w82.h)

struct RecItem { std::string name, path; long long bytes = 0; float secs = 0; };
static std::vector<RecItem> gRecs;
static bool recScanned = false;
static int recSel = -1, recScroll = 0;
static bool recOn = false;                 // recording right now
static std::string recMsg, recRenameBuf; static int recRenameIdx = -1;
static float recLvl = 0, recWave[96]; static int recWaveN = 0;
static float recTime = 0;                  // seconds recorded so far
static FILE* recFp = nullptr; static std::string recCurPath; static long long recDataBytes = 0;
static int recRate = 48000, recCh = 2;
static bool recAudInOk = false, recAudInTried = false;
static bool recPlaying = false, recPaused = false; static Uint32 recPlayT0 = 0; static float recPlayOff = 0, recPlayLen = 0;
static int recPlayIdx = -1;
static bool recSeeking = false;
#ifdef WINNX_AUDIO
static Mix_Music* recMus = nullptr;
#endif
#ifndef WNX_HOST
static const int REC_NB = 4; static const size_t REC_BUFSZ = 0x10000;
static AudioInBuffer recBuf[REC_NB]; static void* recBufMem[REC_NB];
#endif

static std::string recDir() { return DIR_ + "Recordings"; }
static void recWavHeader(FILE* f, long long dataBytes, int rate, int ch) {
    unsigned char h[44]; memset(h, 0, sizeof h);
    auto w32 = [&](int o, unsigned v) { h[o] = v & 255; h[o + 1] = (v >> 8) & 255; h[o + 2] = (v >> 16) & 255; h[o + 3] = (v >> 24) & 255; };
    auto w16 = [&](int o, unsigned v) { h[o] = v & 255; h[o + 1] = (v >> 8) & 255; };
    memcpy(h, "RIFF", 4); w32(4, (unsigned)(36 + dataBytes)); memcpy(h + 8, "WAVEfmt ", 8); w32(16, 16); w16(20, 1); w16(22, ch); w32(24, rate); w32(28, rate * ch * 2); w16(32, ch * 2); w16(34, 16);
    memcpy(h + 36, "data", 4); w32(40, (unsigned)dataBytes);
    fseek(f, 0, SEEK_SET); fwrite(h, 1, 44, f);
}
static bool recCopyFile(const std::string& from, const std::string& to) {
    FILE* a = fopen(from.c_str(), "rb"); if (!a) return false; FILE* b = fopen(to.c_str(), "wb"); if (!b) { fclose(a); return false; }
    std::vector<char> buf(1 << 16); size_t n; bool ok = true;
    while ((n = fread(buf.data(), 1, buf.size(), a)) > 0) if (fwrite(buf.data(), 1, n, b) != n) { ok = false; break; }
    fclose(a); fclose(b); return ok;
}
static long long recFileSize(const std::string& p) { struct stat st; return stat(p.c_str(), &st) == 0 ? (long long)st.st_size : 0; }
static void recScan() {
    recScanned = true; gRecs.clear();
    DIR* d = opendir(recDir().c_str()); if (!d) return;
    struct dirent* e;
    while ((e = readdir(d))) {
        std::string n = e->d_name; if (n.size() < 5 || lower(n.substr(n.size() - 4)) != ".wav") continue;
        RecItem r; r.name = n.substr(0, n.size() - 4); r.path = recDir() + "/" + n; r.bytes = recFileSize(r.path);
        int rate = 48000, ch = 2; FILE* f = fopen(r.path.c_str(), "rb");
        if (f) { unsigned char h[44]; if (fread(h, 1, 44, f) == 44) { ch = h[22] | (h[23] << 8); rate = h[24] | (h[25] << 8) | (h[26] << 16) | (h[27] << 24); } fclose(f); }
        if (rate <= 0 || ch <= 0) { rate = 48000; ch = 2; }
        r.secs = r.bytes > 44 ? (float)(r.bytes - 44) / (float)(rate * ch * 2) : 0;
        gRecs.push_back(r);
    }
    closedir(d);
    std::sort(gRecs.begin(), gRecs.end(), [](const RecItem& a, const RecItem& b) { return a.name > b.name; });
}
static std::string recFmt(float s) { int t = (int)s; char b[24]; if (t >= 3600) snprintf(b, sizeof b, "%d:%02d:%02d", t / 3600, (t / 60) % 60, t % 60); else snprintf(b, sizeof b, "%02d:%02d", t / 60, t % 60); return b; }
static std::string recSizeText(long long b) { char s[24]; if (b > 1048576) snprintf(s, sizeof s, "%.1f MB", b / 1048576.0); else snprintf(s, sizeof s, "%lld KB", b / 1024); return s; }

// ---- playback
static void recStopPlay() {
#ifdef WINNX_AUDIO
    if (musAudioOk && recMus) Mix_HaltMusic();
    if (recMus) { Mix_FreeMusic(recMus); recMus = nullptr; }
#endif
    recPlaying = false; recPaused = false; recPlayIdx = -1; recPlayOff = 0;
}
static float recPlayPos() { return recPlaying ? std::min(recPlayLen, recPlayOff + (recPaused ? 0.0f : (SDL_GetTicks() - recPlayT0) / 1000.0f)) : 0.0f; }
static void recPlay(int i) {
    if (i < 0 || i >= (int)gRecs.size()) return;
    recStopPlay();
#ifdef WINNX_AUDIO
    musStop(); musInit();
    if (!musAudioOk) { recMsg = "Audio output isn't available."; return; }
    recMus = Mix_LoadMUS(gRecs[i].path.c_str());
    if (!recMus) { recMsg = std::string("Couldn't play this file: ") + Mix_GetError(); return; }
    Mix_VolumeMusic((int)(MIX_MAX_VOLUME * std::max(0.3f, gSndVol / 10.0f)));
    if (Mix_PlayMusic(recMus, 1) != 0) { recMsg = "Couldn't start playback."; Mix_FreeMusic(recMus); recMus = nullptr; return; }
    recPlaying = true; recPaused = false; recPlayIdx = i; recPlayLen = gRecs[i].secs; recPlayOff = 0; recPlayT0 = SDL_GetTicks();
#else
#ifdef WNX_HOST
    recPlaying = true; recPaused = false; recPlayIdx = i; recPlayLen = gRecs[i].secs; recPlayOff = 0; recPlayT0 = SDL_GetTicks();
#else
    recMsg = "This build has no audio library, so playback isn't available.";
#endif
#endif
}
static void recTogglePause() {
    if (!recPlaying) return;
#ifdef WINNX_AUDIO
    if (recPaused) { Mix_ResumeMusic(); recPlayT0 = SDL_GetTicks(); recPaused = false; }
    else { recPlayOff = recPlayPos(); Mix_PauseMusic(); recPaused = true; }
#else
    if (recPaused) { recPlayT0 = SDL_GetTicks(); recPaused = false; } else { recPlayOff = recPlayPos(); recPaused = true; }
#endif
}
static void recSeek(float frac) {
    if (!recPlaying) return;
    float t = std::max(0.0f, std::min(recPlayLen - 0.3f, frac * recPlayLen));
#ifdef WINNX_AUDIO
    if (recMus && Mix_SetMusicPosition(t) != 0) return;
#endif
    recPlayOff = t; recPlayT0 = SDL_GetTicks();
}

// ---- capture
static bool recAudInStart() {
#ifdef WNX_HOST
    return true;
#else
    if (!recAudInTried) {
        recAudInTried = true;
        Result rc = audinInitialize();
        if (R_FAILED(rc)) { char b[160]; snprintf(b, sizeof b, "The microphone service isn't available (error 0x%X). It may be blocked in this launch mode.", rc); recMsg = b; return false; }
        recRate = (int)audinGetSampleRate(); recCh = (int)audinGetChannelCount(); if (recRate <= 0) recRate = 48000; if (recCh <= 0) recCh = 2;
        for (int i = 0; i < REC_NB; i++) {
            recBufMem[i] = aligned_alloc(0x1000, REC_BUFSZ); if (!recBufMem[i]) { recMsg = "Out of memory for the recorder."; audinExit(); return false; }
            memset(recBufMem[i], 0, REC_BUFSZ);
            recBuf[i].next = nullptr; recBuf[i].buffer = recBufMem[i]; recBuf[i].buffer_size = REC_BUFSZ; recBuf[i].data_size = REC_BUFSZ; recBuf[i].data_offset = 0;
        }
        recAudInOk = true;
    }
    if (!recAudInOk) { if (recMsg.empty()) recMsg = "The microphone isn't available."; return false; }
    for (int i = 0; i < REC_NB; i++) { recBuf[i].data_size = REC_BUFSZ; audinAppendAudioInBuffer(&recBuf[i]); }
    Result rc = audinStartAudioIn();
    if (R_FAILED(rc)) { char b[120]; snprintf(b, sizeof b, "Couldn't start the microphone (error 0x%X). Plug in a headset with a microphone.", rc); recMsg = b; return false; }
    return true;
#endif
}
static void recBegin() {
    if (recPlaying) recStopPlay();
    mkdir(recDir().c_str(), 0777);
    char nm[64]; snprintf(nm, sizeof nm, "Recording_%04d%02d%02d_%02d%02d%02d", gNow.y, gNow.mo, gNow.d, gNow.h, gNow.mi, gNow.s);
    recCurPath = recDir() + "/" + nm + ".wav";
    recMsg.clear();
    if (!recAudInStart()) return;
    recFp = fopen(recCurPath.c_str(), "wb");
    if (!recFp) { recMsg = "Couldn't create the file on the SD card."; return; }
    recWavHeader(recFp, 0, recRate, recCh); recDataBytes = 0; recTime = 0; recWaveN = 0; recLvl = 0; recOn = true;
}
static void recEnd() {
    if (!recOn) return;
    recOn = false;
#ifndef WNX_HOST
    audinStopAudioIn();
    AudioInBuffer* rel = nullptr; u32 cnt = 0; (void)rel; (void)cnt;
#endif
    if (recFp) { recWavHeader(recFp, recDataBytes, recRate, recCh); fclose(recFp); recFp = nullptr; }
    recScan();
    for (int i = 0; i < (int)gRecs.size(); i++) if (gRecs[i].path == recCurPath) recSel = i;
    toast("Recording saved");
}
static void recPush(const int16_t* s, size_t n) {   // n samples (all channels)
    if (!recFp || n == 0) return;
    fwrite(s, 2, n, recFp); recDataBytes += (long long)n * 2;
    double acc = 0; float pk = 0;
    for (size_t i = 0; i < n; i += 4) { float v = fabsf((float)s[i]) / 32768.0f; acc += v; pk = std::max(pk, v); }
    float lvl = std::min(1.0f, pk * 1.6f); recLvl = recLvl * 0.5f + lvl * 0.5f;
    (void)acc;
}
static void recUpdate(float dt) {
    if (recOn) {
        recTime += dt;
#ifdef WNX_HOST
        { static float ph = 0; int16_t tmp[800]; int ns = (int)(dt * recRate); ns = std::min(ns, 400); for (int i = 0; i < ns; i++) { float v = sinf(ph) * (0.3f + 0.25f * sinf(recTime * 3.0f)); ph += 0.05f; tmp[i * 2] = tmp[i * 2 + 1] = (int16_t)(v * 20000); } recPush(tmp, (size_t)ns * 2); }
#else
        AudioInBuffer* rel = nullptr; u32 cnt = 0;
        if (R_SUCCEEDED(audinGetReleasedAudioInBuffer(&rel, &cnt)) && cnt > 0 && rel) {
            for (u32 k = 0; k < cnt; k++) {
                AudioInBuffer* b = rel; if (!b) break;
                size_t bytes = (size_t)std::min<u64>(b->data_size, b->buffer_size);
                recPush((const int16_t*)b->buffer, bytes / 2);
                b->data_size = REC_BUFSZ; audinAppendAudioInBuffer(b);
                AudioInBuffer* nxt = nullptr; u32 c2 = 0; if (R_FAILED(audinGetReleasedAudioInBuffer(&nxt, &c2)) || c2 == 0) break; rel = nxt;
            }
        }
#endif
        recTime = std::max(recTime, (float)recDataBytes / (float)(recRate * recCh * 2));
        static float wt = 0; wt += dt; if (wt > 0.06f) { wt = 0; if (recWaveN < 96) recWave[recWaveN++] = recLvl; else { memmove(recWave, recWave + 1, 95 * sizeof(float)); recWave[95] = recLvl; } }
        if (recDataBytes > 400LL * 1024 * 1024) { recMsg = "Stopped: the recording got very large."; recEnd(); }
    } else recLvl *= 0.9f;
    if (recPlaying && !recPaused) {
#ifdef WINNX_AUDIO
        if (musAudioOk && recMus && !Mix_PlayingMusic()) recStopPlay();
#else
        if (recPlayPos() >= recPlayLen) recStopPlay();
#endif
    }
}

// ---- UI
static const Col REC_BG = {30, 30, 34}, REC_RED = {214, 40, 60};
static void recRenameDone() {
    std::string n; for (char c : recRenameBuf) if (isalnum((unsigned char)c) || c == ' ' || c == '_' || c == '-' || c == '.') n += c;
    while (!n.empty() && n.back() == ' ') n.pop_back();
    if (recRenameIdx < 0 || recRenameIdx >= (int)gRecs.size() || n.empty()) return;
    std::string np = recDir() + "/" + n + ".wav";
    if (np != gRecs[recRenameIdx].path && rename(gRecs[recRenameIdx].path.c_str(), np.c_str()) == 0) { recMsg = "Renamed."; if (recPlayIdx == recRenameIdx) recStopPlay(); recScan(); for (int i = 0; i < (int)gRecs.size(); i++) if (gRecs[i].path == np) recSel = i; }
    else recMsg = "Couldn't rename (name already used?).";
}
static void recInit() { recScan(); recSel = -1; recScroll = 0; recMsg.clear(); }
static const int RC_LX = 640, RC_ROW = 78, RC_TOP = 120;
static void drawRec() {
    if (!recScanned) recScan();
    rect(0, 0, W, UIH, REC_BG);
    text("Sound Recorder", F44, 50, 24, WHITE);
    // left: recorder
    int cx = 290, cy = 300;
    rect(RC_LX - 20, 100, 1, UIH - 120, WHITE, 40);
    if (recOn) { float p = 0.5f + 0.5f * sinf(SDL_GetTicks() / 250.0f); disc(cx, cy, 128 + (int)(recLvl * 40), REC_RED, 60); disc(cx, cy, 118, REC_RED, 90 + (int)(p * 60)); }
    disc(cx, cy, 104, recOn ? Col{240, 70, 80} : REC_RED);
    if (recOn) rect(cx - 30, cy - 30, 60, 60, WHITE);
    else { rect(cx - 12, cy - 44, 24, 52, WHITE); ring(cx, cy + 4, 34, 6, WHITE, REC_RED); rect(cx - 18, cy + 36, 36, 6, REC_RED); rect(cx - 3, cy + 36, 6, 18, WHITE); rect(cx - 20, cy + 52, 40, 5, WHITE); }
    text(recFmt(recOn ? recTime : 0), F72, cx, 440, WHITE, 1, recOn ? 255 : 150);
    // level meter / waveform
    int mx = 60, my = 540, mw = 460;
    rect(mx, my, mw, 70, Col{44, 44, 50});
    if (recOn) { for (int i = 0; i < recWaveN; i++) { int hh = 4 + (int)(recWave[i] * 60); rect(mx + 4 + i * (mw - 8) / 96, my + 35 - hh / 2, std::max(2, (mw - 8) / 96 - 1), hh, REC_RED); } }
    else text(recPlaying ? "Playing" : "Tap the red button to record", F16, mx + mw / 2, my + 24, WHITE, 1, 150);
    if (!recMsg.empty()) { int yy = 622; for (auto& l : wrapText(recMsg, F16, 520)) { text(l, F16, 50, yy, Col{255, 190, 90}); yy += 20; if (yy > UIH - 10) break; } }
    // right: list
    text("Recordings", F30, RC_LX, 40, WHITE);
    text(std::to_string((int)gRecs.size()) + " saved in switch/WinNX/Recordings", F16, RC_LX, 80, WHITE, 0, 150);
    SDL_Rect clip{RC_LX - 10, RC_TOP, W - RC_LX + 10, UIH - RC_TOP}; SDL_RenderSetClipRect(gR, &clip);
    int y = RC_TOP - recScroll;
    if (gRecs.empty()) text("Nothing recorded yet", F22, RC_LX, RC_TOP + 20, WHITE, 0, 150);
    for (int i = 0; i < (int)gRecs.size(); i++) {
        bool sel = i == recSel; int h = sel ? RC_ROW + 150 : RC_ROW;
        if (y + h >= RC_TOP && y < UIH) {
            rect(RC_LX, y, W - RC_LX - 24, h - 6, sel ? Col{60, 60, 70} : Col{44, 44, 50});
            text(fitTxt(gRecs[i].name, F22, 420), F22, RC_LX + 16, y + 10, WHITE);
            text(recFmt(gRecs[i].secs) + "   " + recSizeText(gRecs[i].bytes), F16, RC_LX + 16, y + 44, WHITE, 0, 160);
            if (recPlaying && recPlayIdx == i) disc(W - 60, y + 36, 10, Col{60, 200, 110});
            if (sel) {
                int by = y + RC_ROW - 4;
                bool pl = recPlaying && recPlayIdx == i;
                // scrub bar
                int sx = RC_LX + 16, sw = W - RC_LX - 24 - 32;
                rect(sx, by + 6, sw, 8, Col{90, 90, 100});
                float fr = pl && recPlayLen > 0 ? recPlayPos() / recPlayLen : 0;
                rect(sx, by + 6, (int)(sw * fr), 8, Col{60, 200, 110}); disc(sx + (int)(sw * fr), by + 10, 11, WHITE);
                text(recFmt(pl ? recPlayPos() : 0), F16, sx, by + 24, WHITE, 0, 170); text(recFmt(gRecs[i].secs), F16, sx + sw, by + 24, WHITE, 2, 170);
                btn(sx, by + 56, 150, 50, pl && !recPaused ? "Pause" : "Play", Col{40, 150, 90}, WHITE);
                btn(sx + 162, by + 56, 110, 50, "Rename", Col{70, 70, 82}, WHITE);
                btn(sx + 284, by + 56, 110, 50, "Delete", Col{170, 50, 60}, WHITE);
                btn(sx + 406, by + 56, 130 - 0, 50, "To OneDrive", Col{0, 110, 190}, WHITE);
            }
        }
        y += h;
    }
    SDL_RenderSetClipRect(gR, nullptr);
}
static int recListH() { int h = 0; for (int i = 0; i < (int)gRecs.size(); i++) h += (i == recSel) ? RC_ROW + 150 : RC_ROW; return h; }
static void recTap(int x, int y) {
    if (!recScanned) recScan();
    if (x < RC_LX - 20) {
        if ((x - 290) * (x - 290) + (y - 300) * (y - 300) < 110 * 110) { if (recOn) recEnd(); else recBegin(); }
        return;
    }
    if (y < RC_TOP) return;
    int yy = RC_TOP - recScroll;
    for (int i = 0; i < (int)gRecs.size(); i++) {
        bool sel = i == recSel; int h = sel ? RC_ROW + 150 : RC_ROW;
        if (y >= yy && y < yy + h - 6) {
            if (!sel) { recSel = i; return; }
            int by = yy + RC_ROW - 4, sx = RC_LX + 16, sw = W - RC_LX - 24 - 32;
            if (y < yy + RC_ROW - 10) { recSel = -1; return; }
            if (inR(x, y, sx - 10, by - 6, sw + 20, 36)) { if (recPlaying && recPlayIdx == i) recSeek((float)(x - sx) / sw); return; }
            if (inR(x, y, sx, by + 56, 150, 50)) { if (recPlaying && recPlayIdx == i) recTogglePause(); else recPlay(i); return; }
            if (inR(x, y, sx + 162, by + 56, 110, 50)) { recRenameIdx = i; recRenameBuf = gRecs[i].name; openKbStr(&recRenameBuf, 40, recRenameDone); return; }
            if (inR(x, y, sx + 284, by + 56, 110, 50)) { if (recPlayIdx == i) recStopPlay(); remove(gRecs[i].path.c_str()); recSel = -1; recScan(); toast("Recording deleted"); return; }
            if (inR(x, y, sx + 406, by + 56, 130, 50)) {
                std::string dst = DIR_ + "OneDrive/Shared"; mkdir((DIR_ + "OneDrive").c_str(), 0777); mkdir(dst.c_str(), 0777);
                recMsg = recCopyFile(gRecs[i].path, dst + "/" + gRecs[i].name + ".wav") ? "Copied to OneDrive > Shared." : "Couldn't copy the file.";
                return;
            }
            return;
        }
        yy += h;
    }
}
static void recDrag(int, int dy) { recScroll = std::max(0, std::min(std::max(0, recListH() - (UIH - RC_TOP) + 20), recScroll - dy)); }
static bool recBack() { if (recSel >= 0) { recSel = -1; return true; } return false; }
static bool recLive(const Tile& t, SDL_Rect r, bool& all) {
    if (!recOn) return false;
    (void)t;
    rect(r.x, r.y, r.w, r.h, Col{30, 30, 34});
    float p = 0.5f + 0.5f * sinf(SDL_GetTicks() / 250.0f);
    disc(r.x + r.w / 2, r.y + r.h / 2 - 12, 26, REC_RED, 120 + (int)(p * 135));
    text(recFmt(recTime), F22, r.x + r.w / 2, r.y + r.h - 42, WHITE, 1);
    all = true; return true;
}
