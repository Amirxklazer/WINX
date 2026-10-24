// w82_city.h - Metropolis: an idle city builder. Your city keeps earning while the app is closed.

static const int CY_W = 30, CY_H = 20, CY_T = 48;
enum { CB_NONE, CB_ROAD, CB_HOUSE, CB_APT, CB_FARM, CB_SHOP, CB_FACT, CB_PARK, CB_POWER, CB_WATER, CB_SCHOOL, CB_HOSP, CB_LAND, CB_COUNT };
struct CyDef { const char* name; int cost; Col col; };
static const CyDef CY_DEF[CB_COUNT] = {
    {"", 0, {0, 0, 0}}, {"Road", 10, {90, 92, 100}}, {"House", 50, {220, 150, 90}}, {"Apartments", 220, {170, 120, 200}}, {"Farm", 90, {190, 170, 60}}, {"Shop", 140, {60, 160, 220}},
    {"Factory", 280, {130, 130, 140}}, {"Park", 70, {60, 170, 80}}, {"Power plant", 320, {240, 200, 40}}, {"Water tower", 160, {70, 140, 230}}, {"School", 230, {230, 100, 80}}, {"Hospital", 340, {240, 240, 245}}, {"Landmark", 1800, {250, 190, 40}}};
struct CyCell { unsigned char t = 0, lvl = 1; };
static CyCell cyG[CY_H][CY_W]; static unsigned char cyTer[CY_H][CY_W];   // terrain: 0 grass 1 water 2 trees
static std::string cyName = "New Metropolis"; static double cyMoney = 400, cyPop = 0; static long long cyLast = 0; static int cyQuest = 0, cyPlayed = 0;
static bool cyLoaded = false, cyDirty = false, cyAwayShow = false; static std::string cyAwayMsg;
static float cyCamX = 0, cyCamY = 0, cyIncome = 0, cyHappy = 50, cyCap = 0, cyJobs = 0, cyPowP = 0, cyPowU = 0, cyWatP = 0, cyWatU = 0, cyFoodP = 0, cyFoodU = 0, cyEvT = 0, cyAcc = 0, cyBoost = 0;
static int cySel = CB_HOUSE, cyPick = -1, cyPickX = 0, cyPickY = 0, cyPanel = 0, cyPalScroll = 0; static bool cyNaming = false; static std::string cyNameEdit;
struct CyEv { int kind = 0; std::string title, a, b; int costA = 0; } ; static CyEv cyEv; static bool cyEvOn = false;
static const char* CY_QN[] = {"Build 3 houses", "Reach 20 residents", "Build a farm and a shop", "Build a park", "Reach 60 residents", "Build a power plant", "Reach 150 residents", "Build a school and a hospital", "Reach 400 residents", "Build the Landmark"};
static const int CY_QR[] = {100, 150, 200, 150, 300, 250, 500, 600, 1500, 3000};
static const int CY_NQ = 10;
static int cyCount(int t) { int n = 0; for (int y = 0; y < CY_H; y++) for (int x = 0; x < CY_W; x++) if (cyG[y][x].t == t) n++; return n; }
static bool cyQuestDone(int q) {
    switch (q) { case 0: return cyCount(CB_HOUSE) >= 3; case 1: return cyPop >= 20; case 2: return cyCount(CB_FARM) >= 1 && cyCount(CB_SHOP) >= 1; case 3: return cyCount(CB_PARK) >= 1; case 4: return cyPop >= 60;
        case 5: return cyCount(CB_POWER) >= 1; case 6: return cyPop >= 150; case 7: return cyCount(CB_SCHOOL) >= 1 && cyCount(CB_HOSP) >= 1; case 8: return cyPop >= 400; case 9: return cyCount(CB_LAND) >= 1; } return false;
}
static void cyGenTerrain() {
    for (int y = 0; y < CY_H; y++) for (int x = 0; x < CY_W; x++) {
        int r = (int)(8 + 3 * sinf(x * 0.35f) + 1.5f * sinf(x * 0.9f)); unsigned char t = 0;
        if (y >= r && y <= r + 1) t = 1; unsigned h = (unsigned)(x * 73856093u ^ y * 19349663u); if (!t && (h % 100) < 9) t = 2; if (x >= 12 && x <= 17 && y >= 3 && y <= 6) t = 0;
        cyTer[y][x] = t; }
}
static void cyRecalc() {
    cyCap = cyJobs = cyPowP = cyPowU = cyWatP = cyWatU = cyFoodP = cyFoodU = 0; float park = 0, school = 0, hosp = 0, land = 0, fact = 0, plant = 0, shops = 0, factInc = 0, upkeep = 0;
    for (int y = 0; y < CY_H; y++) for (int x = 0; x < CY_W; x++) { const CyCell& c = cyG[y][x]; float l = (float)c.lvl; switch (c.t) {
        case CB_ROAD: upkeep += 0.02f; break;
        case CB_HOUSE: cyCap += 4 * l; cyPowU += 1; cyWatU += 1; upkeep += 0.05f; break;
        case CB_APT: cyCap += 16 * l; cyPowU += 3; cyWatU += 3; upkeep += 0.2f; break;
        case CB_FARM: cyFoodP += 8 * l; cyJobs += 2 * l; cyWatU += 1; upkeep += 0.05f; break;
        case CB_SHOP: cyJobs += 4 * l; cyPowU += 1; shops += l; upkeep += 0.1f; break;
        case CB_FACT: cyJobs += 10 * l; cyPowU += 4; fact += l; factInc += 4 * l; upkeep += 0.3f; break;
        case CB_PARK: park += l; break;
        case CB_POWER: cyPowP += 24 * l; plant += l; cyJobs += 3 * l; upkeep += 0.4f; break;
        case CB_WATER: cyWatP += 24 * l; upkeep += 0.2f; break;
        case CB_SCHOOL: school += l; cyJobs += 3 * l; cyPowU += 1; upkeep += 0.3f; break;
        case CB_HOSP: hosp += l; cyJobs += 4 * l; cyPowU += 2; upkeep += 0.4f; break;
        case CB_LAND: land += l; cyJobs += 6; upkeep += 0.5f; break; default: break; } }
    cyFoodU = (float)cyPop * 0.12f; float hp = 55 + park * 5 + school * 4 + hosp * 5 + land * 9 - fact * 4 - plant * 3;
    if (cyPowU > cyPowP) hp -= 12; if (cyWatU > cyWatP) hp -= 12; if (cyFoodU > cyFoodP + 0.01f && cyPop > 4) hp -= 12; if (cyPop > cyJobs * 1.6f + 6) hp -= 8;
    cyHappy = std::max(0.0f, std::min(100.0f, hp));
    float emp = std::min((float)cyPop, cyJobs); cyIncome = emp * 0.45f + shops * 2.2f * (cyHappy / 100.0f) + factInc + (float)cyPop * 0.05f - upkeep; if (cyPowU > cyPowP) cyIncome *= 0.7f; cyIncome *= (1.0f + cyBoost);
}
static void cyStep(double dt) {   // advance the simulation by dt seconds
    cyRecalc(); cyMoney = std::max(0.0, cyMoney + cyIncome * dt);
    double target = cyHappy > 30 ? cyCap : cyCap * 0.4; double rate = cyHappy > 30 ? 0.25 : 0.4; if (cyPop < target) cyPop = std::min(target, cyPop + rate * dt * (1 + cyHappy / 100.0)); else cyPop = std::max(target, cyPop - rate * dt);
}
static std::string cyFile() { return DIR_ + "city.txt"; }
static void cySave() {
    std::string s = "C\t" + cyName + "\t" + std::to_string((long long)cyMoney) + "\t" + std::to_string((long long)cyPop) + "\t" + std::to_string((long long)time(NULL)) + "\t" + std::to_string(cyQuest) + "\n";
    for (int y = 0; y < CY_H; y++) for (int x = 0; x < CY_W; x++) if (cyG[y][x].t) s += "T " + std::to_string(x) + " " + std::to_string(y) + " " + std::to_string(cyG[y][x].t) + " " + std::to_string(cyG[y][x].lvl) + "\n";
    writeFile(cyFile(), s); cyDirty = false;
}
static void cyEnsure() {
    if (cyLoaded) return; cyLoaded = true; cyGenTerrain(); std::string d; bool had = readFile(cyFile(), d);
    if (!had) { cyG[8 + 4][14].t = CB_ROAD; for (int x = 12; x <= 17; x++) cyG[5][x].t = CB_ROAD; cyG[4][13].t = CB_HOUSE; cyG[4][15].t = CB_HOUSE; cyLast = (long long)time(NULL); cyCamX = 12 * CY_T - 420; cyCamY = 2 * CY_T; cyG[12][14].t = 0; return; }
    size_t pos = 0; while (pos < d.size()) { size_t e = d.find('\n', pos); if (e == std::string::npos) e = d.size(); std::string l = d.substr(pos, e - pos); pos = e + 1;
        if (l.size() > 2 && l[0] == 'C') { std::vector<std::string> f; size_t a = 2; while (true) { size_t b = l.find('\t', a); if (b == std::string::npos) { f.push_back(l.substr(a)); break; } f.push_back(l.substr(a, b - a)); a = b + 1; } if (f.size() >= 5) { cyName = f[0]; cyMoney = atof(f[1].c_str()); cyPop = atof(f[2].c_str()); cyLast = atoll(f[3].c_str()); cyQuest = atoi(f[4].c_str()); } }
        else if (l[0] == 'T') { int x, y, t, lv; if (sscanf(l.c_str(), "T %d %d %d %d", &x, &y, &t, &lv) == 4 && x >= 0 && x < CY_W && y >= 0 && y < CY_H && t > 0 && t < CB_COUNT) { cyG[y][x].t = (unsigned char)t; cyG[y][x].lvl = (unsigned char)std::max(1, std::min(3, lv)); cyTer[y][x] = cyTer[y][x] == 2 ? 0 : cyTer[y][x]; } } }
    cyCamX = 12 * CY_T - 420; cyCamY = 2 * CY_T;
    long long now = (long long)time(NULL), el = std::max(0LL, std::min(now - cyLast, 8LL * 3600)); cyRecalc();
    if (el > 120) { double m0 = cyMoney, p0 = cyPop; double eff = 0.5; for (long long i = 0; i < el; i += 30) { double dt = std::min(30LL, el - i); double mb = cyMoney; cyStep(dt); cyMoney = mb + (cyMoney - mb) * eff; }
        char b[160]; snprintf(b, sizeof b, "You were away for %lld min. Your city earned $%d and grew by %d residents.", el / 60, (int)(cyMoney - m0), (int)(cyPop - p0)); cyAwayMsg = b; cyAwayShow = true; }
    cyLast = now;
}
static std::string cityStatus() { cyEnsure(); cyRecalc(); char b[160]; snprintf(b, sizeof b, "%s: %d residents, $%d, earning about $%.1f a second, %s.", cyName.c_str(), (int)cyPop, (int)cyMoney, cyIncome, cyHappy > 65 ? "everyone is happy" : cyHappy > 35 ? "people are okay" : "people are unhappy"); return b; }
static void cityInit() { cyEnsure(); cyPick = -1; cyPanel = 0; cyEvOn = cyEvOn; }
static bool cityBack() { if (cyEvOn) return true; if (cyAwayShow) { cyAwayShow = false; return true; } if (cyNaming) { cyNaming = false; return true; } if (cyPick >= 0) { cyPick = -1; return true; } if (cyPanel) { cyPanel = 0; return true; } return false; }
static void cyNewEvent() {
    int k = (int)(SDL_GetTicks() % 4); cyEv = CyEv(); cyEv.kind = k;
    if (k == 0) { cyEv.title = "A festival is planned!"; cyEv.a = "Sponsor it ($150)"; cyEv.b = "Skip it"; cyEv.costA = 150; }
    else if (k == 1) { cyEv.title = "A fire broke out in a building!"; cyEv.a = "Call the fire brigade ($120)"; cyEv.b = "Let it burn"; cyEv.costA = 120; }
    else if (k == 2) { cyEv.title = "A business boom! Investors are interested."; cyEv.a = "Welcome them (+$400)"; cyEv.b = "Politely decline (+happiness)"; }
    else { cyEv.title = "A storm cuts the power for a while."; cyEv.a = "Repair quickly ($100)"; cyEv.b = "Wait it out"; cyEv.costA = 100; }
    cyEvOn = true; notify(A_CITY, "Metropolis", cyEv.title);
}
static void cyResolve(bool a) {
    cyEvOn = false;
    if (cyEv.kind == 0) { if (a && cyMoney >= cyEv.costA) { cyMoney -= cyEv.costA; cyBoost = 0.5f; toast("The festival boosts your income!"); } }
    else if (cyEv.kind == 1) { std::vector<std::pair<int, int>> v; for (int y = 0; y < CY_H; y++) for (int x = 0; x < CY_W; x++) if (cyG[y][x].t >= CB_HOUSE) v.push_back({x, y}); if (!v.empty() && !(a && cyMoney >= cyEv.costA)) { auto p = v[SDL_GetTicks() % v.size()]; cyG[p.second][p.first].t = 0; toast("A building was lost to the fire"); cyDirty = true; } else if (a) cyMoney -= cyEv.costA; }
    else if (cyEv.kind == 2) { if (a) { cyMoney += 400; toast("+$400"); } else { cyPop += 2; toast("Citizens appreciate your choice"); } }
    else { if (a && cyMoney >= cyEv.costA) cyMoney -= cyEv.costA; else { cyMoney = std::max(0.0, cyMoney - 40); toast("The outage cost you $40"); } }
    cyDirty = true;
}
static void cityUpdate(float dt) {
    if (!cyLoaded) return; cyAcc += dt; if (cyBoost > 0) cyBoost = std::max(0.0f, cyBoost - dt * 0.004f);
    if (cyAcc >= 1.0f) { double s = cyAcc; cyAcc = 0; cyStep(s); cyLast = (long long)time(NULL);
        if (cyQuest < CY_NQ && cyQuestDone(cyQuest)) { cyMoney += CY_QR[cyQuest]; notify(A_CITY, "Quest complete", std::string(CY_QN[cyQuest]) + "  +$" + std::to_string(CY_QR[cyQuest])); toast("Quest complete! +$" + std::to_string(CY_QR[cyQuest])); cyQuest++; cyDirty = true; sndPlay(SND_UNLOCK); }
        cyEvT += (float)s; if (cyEvT > 240 && !cyEvOn && curApp == A_CITY) { cyEvT = 0; cyNewEvent(); }
        static float sv = 0; sv += (float)s; if (sv > 15) { sv = 0; cySave(); } }
}
static bool cityLive(const Tile&, SDL_Rect r, bool& all) {
    cyEnsure(); bool small = r.w < 150; int x = r.x, y = r.y; for (int i = 0; i < 6; i++) { int h = 20 + (i * 37 % 41); rect(x + 8 + i * (r.w - 16) / 6, y + r.h - 40 - h, (r.w - 16) / 6 - 4, h, WHITE, 90); }
    char b[48]; snprintf(b, sizeof b, "%d", (int)cyPop); if (small) { text(b, F30, x + r.w / 2, y + 8, WHITE, 1); all = true; return true; }
    text(b, F44, x + 14, y + 6, WHITE); text("residents", F16, x + 16, y + 58, WHITE, 0, 200); snprintf(b, sizeof b, "$%d", (int)cyMoney); text(b, F22, x + r.w - 14, y + 10, WHITE, 2); all = true; return true;
}
// ---- drawing
static void cyBuild(int bx, int by, int sz, int t, int lv, Col c) {
    switch (t) {
        case CB_ROAD: rect(bx, by, sz, sz, c); rect(bx + sz / 2 - 1, by + 6, 3, sz - 12, WHITE, 120); break;
        case CB_HOUSE: rect(bx + 8, by + 18, sz - 16, sz - 22, c); tri((float)bx + 4, (float)by + 20, (float)bx + sz / 2, (float)by + 4, (float)bx + sz - 4, (float)by + 20, Col{170, 60, 50}); rect(bx + sz / 2 - 4, by + sz - 20, 8, 14, Col{90, 50, 30}); break;
        case CB_APT: rect(bx + 6, by + 6, sz - 12, sz - 10, c); for (int i = 0; i < 3; i++) for (int j = 0; j < 2; j++) rect(bx + 12 + j * 14, by + 12 + i * 11, 8, 6, Col{250, 240, 170}); break;
        case CB_FARM: rect(bx + 3, by + 3, sz - 6, sz - 6, c); for (int i = 0; i < 4; i++) rect(bx + 6, by + 8 + i * 9, sz - 12, 3, Col{110, 140, 40}); break;
        case CB_SHOP: rect(bx + 6, by + 14, sz - 12, sz - 18, c); rect(bx + 4, by + 8, sz - 8, 8, Col{230, 60, 60}); rect(bx + sz / 2 - 5, by + sz - 18, 10, 12, WHITE); break;
        case CB_FACT: rect(bx + 4, by + 16, sz - 8, sz - 20, c); rect(bx + 10, by + 4, 8, 14, Col{90, 90, 100}); rect(bx + 24, by + 8, 8, 10, Col{90, 90, 100}); disc(bx + 14, by + 3, 4, Col{200, 200, 205}, 160); break;
        case CB_PARK: rect(bx + 2, by + 2, sz - 4, sz - 4, c); disc(bx + 14, by + 16, 8, Col{30, 120, 50}); disc(bx + 32, by + 28, 9, Col{30, 120, 50}); rect(bx + 12, by + 24, 4, 8, Col{100, 60, 30}); break;
        case CB_POWER: rect(bx + 6, by + 10, sz - 12, sz - 14, Col{100, 100, 108}); tri((float)bx + 22, (float)by + 8, (float)bx + 14, (float)by + 26, (float)bx + 24, (float)by + 24, c); tri((float)bx + 24, (float)by + 24, (float)bx + 20, (float)by + 38, (float)bx + 32, (float)by + 20, c); break;
        case CB_WATER: rect(bx + sz / 2 - 3, by + 18, 6, sz - 20, Col{90, 90, 100}); disc(bx + sz / 2, by + 14, 12, c); break;
        case CB_SCHOOL: rect(bx + 4, by + 14, sz - 8, sz - 18, c); tri((float)bx + 4, (float)by + 14, (float)bx + sz / 2, (float)by + 4, (float)bx + sz - 4, (float)by + 14, Col{120, 50, 40}); rect(bx + sz / 2 - 3, by + 24, 6, 12, WHITE); break;
        case CB_HOSP: rect(bx + 4, by + 6, sz - 8, sz - 10, c); rect(bx + sz / 2 - 3, by + 12, 6, 20, Col{220, 40, 50}); rect(bx + sz / 2 - 10, by + 19, 20, 6, Col{220, 40, 50}); break;
        case CB_LAND: tri((float)bx + sz / 2, (float)by + 2, (float)bx + 6, (float)by + sz - 4, (float)bx + sz - 6, (float)by + sz - 4, c); rect(bx + sz / 2 - 2, by + 10, 4, 20, Col{255, 240, 180}); break; default: break; }
    for (int i = 1; i < lv && t > CB_ROAD; i++) disc(bx + sz - 8 - (i - 1) * 9, by + 8, 3, Col{255, 220, 60});
}
static void drawCity() {
    cyEnsure(); cyRecalc();
    rect(0, 0, W, UIH, Col{80, 150, 90});
    int mapTop = 66, mapBot = UIH - 118; SDL_Rect clip = {0, mapTop, W, mapBot - mapTop}; SDL_RenderSetClipRect(gR, &clip);
    cyCamX = std::max(0.0f, std::min(cyCamX, (float)(CY_W * CY_T - W))); cyCamY = std::max(0.0f, std::min(cyCamY, (float)(CY_H * CY_T - (mapBot - mapTop))));
    int tx0 = (int)(cyCamX / CY_T), ty0 = (int)(cyCamY / CY_T), tx1 = std::min(CY_W, tx0 + W / CY_T + 2), ty1 = std::min(CY_H, ty0 + (mapBot - mapTop) / CY_T + 2);
    for (int y = ty0; y < ty1; y++) for (int x = tx0; x < tx1; x++) {
        int bx = x * CY_T - (int)cyCamX, by = mapTop + y * CY_T - (int)cyCamY; unsigned char tr = cyTer[y][x];
        rect(bx, by, CY_T, CY_T, ((x + y) & 1) ? Col{86, 158, 96} : Col{80, 150, 90});
        if (tr == 1) rect(bx, by, CY_T, CY_T, ((x + y) & 1) ? Col{70, 130, 210} : Col{64, 122, 200});
        else if (tr == 2 && !cyG[y][x].t) { disc(bx + 24, by + 22, 12, Col{36, 110, 56}); rect(bx + 22, by + 30, 4, 10, Col{90, 60, 30}); }
        if (cyG[y][x].t) { if (cyG[y][x].t != CB_ROAD) rect(bx, by, CY_T, CY_T, Col{140, 140, 146}, 60); cyBuild(bx, by, CY_T, cyG[y][x].t, cyG[y][x].lvl, CY_DEF[cyG[y][x].t].col); }
        if (x == cyPickX && y == cyPickY && cyPick >= 0) outline(bx, by, CY_T, CY_T, 3, WHITE);
    }
    int hr = gNow.h; float dark = hr >= 20 || hr < 5 ? 0.45f : (hr >= 18 || hr < 7 ? 0.2f : 0.0f); if (dark > 0) rect(0, mapTop, W, mapBot - mapTop, Col{10, 20, 60}, (int)(255 * dark));
    SDL_RenderSetClipRect(gR, nullptr);
    // HUD
    rect(0, 0, W, mapTop, Col{24, 36, 52}); text(cyName, F30, 20, 14, WHITE); char b[96];
    snprintf(b, sizeof b, "$%d", (int)cyMoney); text(b, F30, 360, 14, Col{255, 220, 90}); snprintf(b, sizeof b, "%+.1f/s", cyIncome); text(b, F16, 360, 46 - 6, WHITE, 0, 180);
    snprintf(b, sizeof b, "%d / %d", (int)cyPop, (int)cyCap); text(b, F22, 520, 8, WHITE); text("residents", F16, 520, 38, WHITE, 0, 170);
    Col hc = cyHappy > 65 ? Col{90, 220, 120} : cyHappy > 35 ? Col{255, 210, 70} : Col{240, 100, 90}; disc(690, 33, 17, hc); rect(682, 26, 4, 4, BLACK); rect(694, 26, 4, 4, BLACK); if (cyHappy > 50) line(682, 38, 690, 43, 2, BLACK); else line(682, 42, 698, 42, 2, BLACK);
    struct Ind { const char* n; bool ok; } ind[3] = {{"Power", cyPowU <= cyPowP}, {"Water", cyWatU <= cyWatP}, {"Food", cyFoodU <= cyFoodP + 0.01f}};
    for (int i = 0; i < 3; i++) { int x = 740 + i * 100; disc(x, 33, 8, ind[i].ok ? Col{90, 220, 120} : Col{240, 80, 70}); text(ind[i].n, F16, x + 14, 22, WHITE, 0, 210); }
    btn(1060, 12, 90, 42, "Goals", Col{60, 90, 130}, WHITE); btn(1160, 12, 100, 42, "Stats", Col{60, 90, 130}, WHITE);
    // palette
    rect(0, mapBot, W, 118, Col{24, 36, 52});
    for (int i = 1; i < CB_COUNT; i++) { int x = 12 + (i - 1) * 104; bool on = cySel == i; rect(x, mapBot + 8, 98, 102, on ? Col{70, 100, 150} : Col{40, 56, 78}); cyBuild(x + 25, mapBot + 12, 48, i, 1, CY_DEF[i].col); text(CY_DEF[i].name, F16, x + 49, mapBot + 66, WHITE, 1, on ? 255 : 200); snprintf(b, sizeof b, "$%d", CY_DEF[i].cost); text(b, F16, x + 49, mapBot + 86, cyMoney >= CY_DEF[i].cost ? Col{255, 220, 90} : Col{240, 120, 110}, 1); }
    // selection panel
    if (cyPick >= 0) { const CyCell& c = cyG[cyPickY][cyPickX]; if (c.t) { int px = 360, py = mapBot - 96; rect(px, py, 560, 88, Col{24, 36, 52}, 245); outline(px, py, 560, 88, 2, WHITE); snprintf(b, sizeof b, "%s  (level %d)", CY_DEF[c.t].name, c.lvl); text(b, F22, px + 16, py + 10, WHITE);
        int up = (int)(CY_DEF[c.t].cost * 1.5f * c.lvl); if (c.lvl < 3 && c.t > CB_ROAD) { snprintf(b, sizeof b, "Upgrade $%d", up); btn(px + 16, py + 42, 200, 38, b, cyMoney >= up ? Col{30, 150, 80} : Col{90, 90, 96}, WHITE); } btn(px + 230, py + 42, 160, 38, "Bulldoze", Col{170, 50, 50}, WHITE); btn(px + 404, py + 42, 140, 38, "Close", Col{70, 80, 100}, WHITE); } }
    if (cyPanel == 1 || cyPanel == 2) { rect(300, 90, 680, 440, Col{20, 30, 46}, 250); outline(300, 90, 680, 440, 2, WHITE);
        if (cyPanel == 1) { text("Goals", F30, 324, 100, WHITE); for (int i = 0; i < CY_NQ; i++) { int y = 150 + i * 36; bool d = i < cyQuest; text((d ? "[x] " : (i == cyQuest ? "[ ] " : "    ")) + std::string(CY_QN[i]), F22, 330, y, d ? Col{120, 220, 140} : (i == cyQuest ? WHITE : Col{150, 160, 175})); snprintf(b, sizeof b, "$%d", CY_QR[i]); text(b, F22, 950, y, Col{255, 220, 90}, 2, d ? 120 : 255); } }
        else { text("Statistics", F30, 324, 100, WHITE); snprintf(b, sizeof b, "Residents %d of %d", (int)cyPop, (int)cyCap); text(b, F22, 330, 160, WHITE); snprintf(b, sizeof b, "Jobs %d", (int)cyJobs); text(b, F22, 330, 196, WHITE); snprintf(b, sizeof b, "Happiness %d%%", (int)cyHappy); text(b, F22, 330, 232, WHITE); snprintf(b, sizeof b, "Power %d used of %d", (int)cyPowU, (int)cyPowP); text(b, F22, 330, 268, WHITE); snprintf(b, sizeof b, "Water %d used of %d", (int)cyWatU, (int)cyWatP); text(b, F22, 330, 304, WHITE); snprintf(b, sizeof b, "Food %.1f made, %.1f eaten per second", cyFoodP, cyFoodU); text(b, F22, 330, 340, WHITE); snprintf(b, sizeof b, "Income %+.1f per second", cyIncome); text(b, F22, 330, 376, WHITE); text("Tip: shortages make people unhappy and cut your income.", F16, 330, 420, WHITE, 0, 190); btn(330, 460, 220, 44, "Rename city", Col{60, 90, 130}, WHITE); }
        btn(860, 470, 100, 44, "Close", Col{70, 80, 100}, WHITE); }
    if (cyEvOn) { rect(0, 0, W, UIH, BLACK, 150); rect(340, 200, 600, 240, Col{30, 44, 66}); outline(340, 200, 600, 240, 2, WHITE); int y = 220; for (auto& l : wrapText(cyEv.title, F30, 560)) { text(l, F30, 360, y, WHITE); y += 40; } btn(360, 360, 270, 56, cyEv.a.c_str(), Col{30, 150, 80}, WHITE); btn(650, 360, 270, 56, cyEv.b.c_str(), Col{90, 100, 120}, WHITE); }
    if (cyAwayShow) { rect(0, 0, W, UIH, BLACK, 150); rect(340, 200, 600, 240, Col{30, 44, 66}); outline(340, 200, 600, 240, 2, WHITE); text("Welcome back", F44, 360, 214, WHITE); int y = 280; for (auto& l : wrapText(cyAwayMsg, F22, 560)) { text(l, F22, 360, y, WHITE, 0, 230); y += 30; } btn(500, 372, 280, 52, "Continue", Col{30, 150, 80}, WHITE); }
}
static void cyApplyName() { if (!cyNameEdit.empty()) { cyName = cyNameEdit; cyDirty = true; } hideKb(); }
static void cityTap(int x, int y) {
    cyEnsure(); int mapTop = 66, mapBot = UIH - 118;
    if (cyEvOn) { if (inR(x, y, 360, 360, 270, 56)) cyResolve(true); else if (inR(x, y, 650, 360, 270, 56)) cyResolve(false); return; }
    if (cyAwayShow) { if (inR(x, y, 500, 372, 280, 52)) cyAwayShow = false; return; }
    if (cyPanel) { if (inR(x, y, 860, 470, 100, 44)) cyPanel = 0; else if (cyPanel == 2 && inR(x, y, 330, 460, 220, 44)) { cyNameEdit = cyName; openKbStr(&cyNameEdit, 20, cyApplyName); } return; }
    if (y < mapTop) { if (inR(x, y, 1060, 12, 90, 42)) cyPanel = 1; else if (inR(x, y, 1160, 12, 100, 42)) cyPanel = 2; return; }
    if (y >= mapBot) { for (int i = 1; i < CB_COUNT; i++) if (inR(x, y, 12 + (i - 1) * 104, mapBot + 8, 98, 102)) { cySel = i; cyPick = -1; sndPlay(SND_CLICK); } return; }
    if (cyPick >= 0) { int px = 360, py = mapBot - 96; if (inR(x, y, px, py, 560, 88)) { CyCell& c = cyG[cyPickY][cyPickX];
            if (inR(x, y, px + 16, py + 42, 200, 38) && c.lvl < 3 && c.t > CB_ROAD) { int up = (int)(CY_DEF[c.t].cost * 1.5f * c.lvl); if (cyMoney >= up) { cyMoney -= up; c.lvl++; cyDirty = true; sndPlay(SND_UNLOCK); } else toast("Not enough money"); }
            else if (inR(x, y, px + 230, py + 42, 160, 38)) { cyMoney += CY_DEF[c.t].cost * 0.4; c = CyCell(); cyPick = -1; cyDirty = true; }
            else if (inR(x, y, px + 404, py + 42, 140, 38)) cyPick = -1; return; } }
    int gx = (int)((x + cyCamX) / CY_T), gy = (int)((y - mapTop + cyCamY) / CY_T); if (gx < 0 || gy < 0 || gx >= CY_W || gy >= CY_H) return;
    CyCell& c = cyG[gy][gx];
    if (c.t) { cyPick = 1; cyPickX = gx; cyPickY = gy; return; }
    if (cyTer[gy][gx] == 1) { toast("You can't build on water"); return; }
    if (cyMoney < CY_DEF[cySel].cost) { toast("Not enough money"); return; }
    if (cySel == CB_LAND && cyCount(CB_LAND) >= 1) { toast("You already have a Landmark"); return; }
    cyMoney -= CY_DEF[cySel].cost; c.t = (unsigned char)cySel; c.lvl = 1; cyTer[gy][gx] = 0; cyDirty = true; sndPlay(SND_CLICK);
}
static void cityDrag(int dx, int dy) { if (cyEvOn || cyAwayShow || cyPanel) return; cyCamX -= dx; cyCamY -= dy; }
static void cityTouch(int, int, int) {}
