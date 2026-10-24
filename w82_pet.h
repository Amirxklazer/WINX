// w82_pet.h - Pixel Pet: a Tamagotchi-style companion that lives in real time (included from w82.h)
#include <ctime>

struct PetS {
    bool created = false;
    int stage = 0, species = 0, hue = 0, hat = 0;                // stage 0 egg 1 baby 2 child 3 teen 4 adult
    std::string name = "Mochi";
    float hunger = 80, happy = 80, energy = 90, clean = 90, health = 100;
    double eggLeft = 120, age = 0, careAcc = 0, careN = 0;
    long long last = 0, lastGift = 0, nGift[3] = {0, 0, 0};
    int coins = 30, earned = 0, pantry[5] = {0, 2, 1, 0, 1}, hatsOwned = 1, ach = 0, games = 0, washes = 0, meals = 0, streak = 0, giftDay = 0;
    bool asleep = false;
};
static PetS gPet;
static bool gPetLoaded = false, gPetDirty = false;
static int petView = 0, petTab = 0, petMg = 0;                    // view 0 room 1 shop 3 games 4 playing 5 stats
static bool petFeedOpen = false, petCreateName = false;
static float petSave = 0, petAcc = 0, petBubT = 0, petStroke = 0, petAchT = 0;
static std::string petBub = "", petNameEdit, petAchMsg;
static int petLastTouchX = 0, petLastTouchY = 0; static bool petStroking = false;
struct PetHeart { float x, y, t; };
static std::vector<PetHeart> petHearts;

static const Col PET_HUE[6] = {{245, 160, 80}, {120, 180, 255}, {255, 150, 195}, {140, 210, 120}, {200, 160, 255}, {250, 220, 90}};
static const char* PET_SP[5] = {"Cat", "Dog", "Blob", "Dragon", "Bunny"};
static const char* PET_NAMES[5] = {"Mochi", "Biscuit", "Bean", "Ember", "Clover"};
static const char* PET_STAGE[5] = {"Egg", "Baby", "Child", "Teen", "Adult"};
static const char* PET_FOOD[5] = {"Kibble", "Apple", "Burger", "Cake", "Fish"};
static const int PET_FP[5] = {0, 5, 12, 20, 15}, PET_FH[5] = {12, 15, 35, 20, 25}, PET_FJ[5] = {0, 3, 5, 25, 10};
static const char* PET_HAT[6] = {"No hat", "Cap", "Crown", "Party hat", "Bow", "Wizard hat"};
static const int PET_HP[6] = {0, 30, 120, 50, 25, 80};
static const char* PET_ACH[8] = {"First meal", "Hatched!", "Grew into a teen", "Fully grown", "Played 5 games", "5 baths", "3 day streak", "Collected 200 coins"};
static long long petNow() { return (long long)time(nullptr); }
static int petDayKey() { return gNow.y * 10000 + gNow.mo * 100 + gNow.d; }
static void petClampAll() {
    gPet.hunger = clampf(gPet.hunger, 0, 100); gPet.happy = clampf(gPet.happy, 0, 100); gPet.energy = clampf(gPet.energy, 0, 100);
    gPet.clean = clampf(gPet.clean, 0, 100); gPet.health = clampf(gPet.health, 5, 100);
}
static void petAch(int i) {
    if (gPet.ach & (1 << i)) return;
    gPet.ach |= 1 << i; gPet.coins += 15; gPetDirty = true; petAchMsg = std::string("Achievement: ") + PET_ACH[i] + "  (+15)"; petAchT = 4.0f; sndPlay(SND_NOTIFY);
}
static void petSaveNow() {
    PetS& p = gPet; char b[900];
    snprintf(b, sizeof b, "created=%d\nstage=%d\nspecies=%d\nhue=%d\nhat=%d\nname=%s\nhunger=%.2f\nhappy=%.2f\nenergy=%.2f\nclean=%.2f\nhealth=%.2f\negg=%.1f\nage=%.1f\ncareacc=%.1f\ncaren=%.1f\nlast=%lld\ncoins=%d\nearned=%d\np0=%d\np1=%d\np2=%d\np3=%d\np4=%d\nhats=%d\nach=%d\ngames=%d\nwashes=%d\nmeals=%d\nstreak=%d\ngiftday=%d\nasleep=%d\nnot0=%lld\nnot1=%lld\nnot2=%lld\n",
        p.created, p.stage, p.species, p.hue, p.hat, w9Clean(p.name).c_str(), p.hunger, p.happy, p.energy, p.clean, p.health, p.eggLeft, p.age, p.careAcc, p.careN, p.last, p.coins, p.earned,
        p.pantry[0], p.pantry[1], p.pantry[2], p.pantry[3], p.pantry[4], p.hatsOwned, p.ach, p.games, p.washes, p.meals, p.streak, p.giftDay, p.asleep, p.nGift[0], p.nGift[1], p.nGift[2]);
    writeFile(DIR_ + "pet.txt", b); gPetDirty = false;
}
static void petStep(double secs);
static void petLoad() {
    gPetLoaded = true; std::string d;
    if (!readFile(DIR_ + "pet.txt", d)) return;
    PetS& p = gPet;
    for (auto& l : w9Split(d, '\n')) {
        size_t q = l.find('='); if (q == std::string::npos) continue;
        std::string k = l.substr(0, q), v = l.substr(q + 1); double n = atof(v.c_str()); long long ll = atoll(v.c_str());
        if (k == "created") p.created = n != 0; else if (k == "stage") p.stage = std::max(0, std::min(4, (int)n)); else if (k == "species") p.species = std::max(0, std::min(4, (int)n));
        else if (k == "hue") p.hue = std::max(0, std::min(5, (int)n)); else if (k == "hat") p.hat = std::max(0, std::min(5, (int)n)); else if (k == "name") p.name = v.empty() ? "Mochi" : v;
        else if (k == "hunger") p.hunger = (float)n; else if (k == "happy") p.happy = (float)n; else if (k == "energy") p.energy = (float)n; else if (k == "clean") p.clean = (float)n; else if (k == "health") p.health = (float)n;
        else if (k == "egg") p.eggLeft = n; else if (k == "age") p.age = n; else if (k == "careacc") p.careAcc = n; else if (k == "caren") p.careN = n; else if (k == "last") p.last = ll;
        else if (k == "coins") p.coins = (int)n; else if (k == "earned") p.earned = (int)n; else if (k == "hats") p.hatsOwned = (int)n | 1; else if (k == "ach") p.ach = (int)n;
        else if (k == "games") p.games = (int)n; else if (k == "washes") p.washes = (int)n; else if (k == "meals") p.meals = (int)n; else if (k == "streak") p.streak = (int)n; else if (k == "giftday") p.giftDay = (int)n;
        else if (k == "asleep") p.asleep = n != 0; else if (k == "not0") p.nGift[0] = ll; else if (k == "not1") p.nGift[1] = ll; else if (k == "not2") p.nGift[2] = ll;
        else if (k.size() == 2 && k[0] == 'p' && k[1] >= '0' && k[1] <= '4') p.pantry[k[1] - '0'] = std::max(0, (int)n);
    }
    petClampAll();
    if (p.created && p.last > 0) { double el = (double)(petNow() - p.last); if (el > 0) petStep(std::min(el, 28800.0)); }
    p.last = petNow();
}
static void petEnsure() { if (!gPetLoaded) petLoad(); }
static void petHatch() {
    gPet.stage = 1; gPet.age = 0; gPet.hunger = 70; gPet.happy = 90; gPet.energy = 90; gPet.clean = 100; gPet.health = 100; petAch(1); gPetDirty = true;
    petBub = "Hello! I'm " + gPet.name + "!"; petBubT = 5;
    if (curApp != A_PET) notify(A_PET, gPet.name, "has hatched! Come and say hello.");
}
static void petStep(double secs) {
    PetS& p = gPet; if (!p.created || secs <= 0) return;
    if (p.stage == 0) { p.eggLeft -= secs; if (p.eggLeft <= 0) petHatch(); return; }
    float h = (float)(secs / 3600.0);
    p.hunger -= 8 * h; p.happy -= (p.asleep ? 1.0f : 5.0f) * h; p.clean -= 6 * h; p.energy += p.asleep ? 25 * h : -4 * h;
    if (p.asleep && p.energy >= 99) p.asleep = false;
    int low = (p.hunger < 15) + (p.clean < 15) + (p.happy < 10);
    p.health += (low ? -6.0f * low : 3.0f) * h;
    petClampAll();
    p.age += secs; p.careAcc += (double)((p.hunger + p.happy + p.energy + p.clean + p.health) / 5.0f) * secs; p.careN += secs;
    int st = p.age < 600 ? 1 : p.age < 3 * 3600 ? 2 : p.age < 12 * 3600 ? 3 : 4;
    if (st > p.stage) { p.stage = st; gPetDirty = true; petBub = std::string("I grew into a ") + PET_STAGE[st] + "!"; petBubT = 6; if (st >= 3) petAch(2); if (st == 4) petAch(3); sndPlay(SND_UNLOCK); }
}
static float petCare() { return gPet.careN > 1 ? (float)(gPet.careAcc / gPet.careN) : 80.0f; }
static int petMood() {   // 0 normal 1 happy 2 sad 3 sick 4 sleeping
    PetS& p = gPet; if (p.asleep) return 4; if (p.health < 30) return 3;
    if (p.hunger < 25 || p.happy < 25 || p.clean < 20) return 2; return (p.hunger + p.happy + p.energy + p.clean) / 4 > 70 ? 1 : 0;
}
static std::string petStatus() {
    petEnsure(); PetS& p = gPet;
    if (!p.created) return "You don't have a pet yet - open Pixel Pet and pick an egg.";
    if (p.stage == 0) return p.name + " is still an egg.";
    std::string s = p.name; std::vector<std::string> a;
    if (p.asleep) a.push_back("is sleeping"); if (p.health < 30) a.push_back("is not feeling well");
    if (p.hunger < 25) a.push_back("is hungry"); if (p.clean < 20) a.push_back("needs a bath"); if (p.happy < 25) a.push_back("is lonely");
    if (p.energy < 20 && !p.asleep) a.push_back("is very sleepy");
    if (a.empty()) return s + " is happy and doing great.";
    for (size_t i = 0; i < a.size(); i++) s += (i == 0 ? " " : (i + 1 == a.size() ? " and " : ", ")) + a[i];
    return s + ".";
}
static void petInit() { petEnsure(); petView = 0; petFeedOpen = false; petCreateName = false; petMg = 0; }

// ---------------------------------------------------------------- drawing the pet
static void petEllipse(int cx, int cy, int rx, int ry, Col c, int a = 255) {   // cheap ellipse from stacked discs
    if (rx <= 0 || ry <= 0) return;
    if (rx == ry) { disc(cx, cy, rx, c, a); return; }
    if (rx > ry) { int n = std::max(2, rx / std::max(1, ry) * 2); for (int i = 0; i <= n; i++) disc(cx - (rx - ry) + (rx - ry) * 2 * i / n, cy, ry, c, a); }
    else { int n = std::max(2, ry / std::max(1, rx) * 2); for (int i = 0; i <= n; i++) disc(cx, cy - (ry - rx) + (ry - rx) * 2 * i / n, rx, c, a); }
}
static void petHatDraw(int cx, int top, float R, int hat) {
    switch (hat) {
        case 1: rect(cx - (int)(R * 0.55f), top - (int)(R * 0.25f), (int)(R * 1.1f), (int)(R * 0.3f), Col{220, 50, 50}); rect(cx - (int)(R * 0.85f), top + 2, (int)(R * 0.8f), (int)(R * 0.12f), Col{180, 30, 30}); break;
        case 2: { Col g = {250, 200, 40}; rect(cx - (int)(R * 0.5f), top - (int)(R * 0.3f), (int)(R), (int)(R * 0.3f), g); for (int i = -1; i <= 1; i++) tri((float)(cx + i * R * 0.42f - R * 0.1f), (float)(top - R * 0.3f), (float)(cx + i * R * 0.42f + R * 0.1f), (float)(top - R * 0.3f), (float)(cx + i * R * 0.42f), (float)(top - R * 0.6f), g); disc(cx, top - (int)(R * 0.15f), std::max(2, (int)(R * 0.08f)), Col{220, 40, 80}); break; }
        case 3: tri((float)(cx - R * 0.4f), (float)(top + 4), (float)(cx + R * 0.4f), (float)(top + 4), (float)cx, (float)(top - R * 0.9f), Col{90, 160, 255}); disc(cx, top - (int)(R * 0.9f), std::max(3, (int)(R * 0.1f)), Col{255, 230, 80}); break;
        case 4: disc(cx - (int)(R * 0.3f), top, (int)(R * 0.2f), Col{235, 70, 130}); disc(cx + (int)(R * 0.3f), top, (int)(R * 0.2f), Col{235, 70, 130}); disc(cx, top, (int)(R * 0.1f), Col{200, 40, 100}); break;
        case 5: tri((float)(cx - R * 0.6f), (float)(top + 6), (float)(cx + R * 0.6f), (float)(top + 6), (float)(cx + R * 0.1f), (float)(top - R * 1.1f), Col{80, 60, 150}); rect(cx - (int)(R * 0.8f), top + 2, (int)(R * 1.6f), (int)(R * 0.14f), Col{60, 44, 120}); disc(cx + (int)(R * 0.05f), top - (int)(R * 0.5f), std::max(2, (int)(R * 0.07f)), Col{255, 230, 90}); break;
        default: break;
    }
}
// cx,cy = centre of body; R = body radius
static void petDraw(int cx, int cy, float R, const PetS& p, float t, int mood, int stage) {
    Col c = PET_HUE[p.hue], dk = mix(c, BLACK, 0.3f), lt = mix(c, WHITE, 0.55f);
    if (stage == 0) {   // egg
        float wob = sinf(t * (p.eggLeft < 40 ? 14.0f : 3.0f)) * (p.eggLeft < 40 ? 6.0f : 2.0f);
        int ex = cx + (int)wob; Col sh = mix(c, WHITE, 0.7f);
        disc(ex, cy + (int)(R * 0.15f), (int)(R * 0.95f), sh); disc(ex, cy - (int)(R * 0.3f), (int)(R * 0.85f), sh); disc(ex, cy - (int)(R * 0.7f), (int)(R * 0.6f), sh);
        disc(ex - (int)(R * 0.35f), cy, (int)(R * 0.2f), c); disc(ex + (int)(R * 0.3f), cy - (int)(R * 0.4f), (int)(R * 0.16f), c); disc(ex + (int)(R * 0.25f), cy + (int)(R * 0.45f), (int)(R * 0.22f), c); disc(ex - (int)(R * 0.1f), cy - (int)(R * 0.85f), (int)(R * 0.13f), c);
        if (p.eggLeft < 40) { line(ex - 20, cy - 20, ex, cy, 3, dk); line(ex, cy, ex + 14, cy - 18, 3, dk); line(ex, cy, ex - 6, cy + 24, 3, dk); }
        return;
    }
    float by = sinf(t * 3.0f) * 4.0f * (R / 60.0f) * (mood == 4 ? 0.3f : 1.0f); cy += (int)by;
    int sp = p.species; int ri = (int)R; int variant = stage == 4 ? (petCare() > 70 ? 2 : petCare() > 40 ? 1 : 0) : 1;
    // behind the body
    if (sp == 0) { for (int i = 0; i < 6; i++) { float a = i * 0.25f; disc(cx + ri + (int)(sinf(a + t * 2) * 6) + i * (ri / 8), cy + (int)(R * 0.3f) - i * (ri / 7), std::max(3, (int)(R * 0.13f)), dk); } }
    if (sp == 3) {
        float fl = sinf(t * 5.0f) * 0.25f;
        tri((float)(cx - R * 0.6f), (float)(cy - R * 0.2f), (float)(cx - R * 1.7f), (float)(cy - R * (0.9f + fl)), (float)(cx - R * 0.9f), (float)(cy + R * 0.3f), dk);
        tri((float)(cx + R * 0.6f), (float)(cy - R * 0.2f), (float)(cx + R * 1.7f), (float)(cy - R * (0.9f + fl)), (float)(cx + R * 0.9f), (float)(cy + R * 0.3f), dk);
        for (int i = 0; i < 5; i++) disc(cx + ri + i * (ri / 7), cy + (int)(R * 0.5f) + (i > 2 ? (i - 2) * 3 : 0), std::max(3, (int)(R * (0.2f - i * 0.03f))), c);
    }
    if (sp == 4) { disc(cx + (int)(R * 0.85f), cy + (int)(R * 0.55f), (int)(R * 0.22f), WHITE); }
    if (sp == 1) { for (int i = 0; i < 4; i++) disc(cx + ri - 4 + i * 5, cy + (int)(R * 0.4f) - i * (ri / 8) - (int)(sinf(t * 6 + i) * 3), std::max(3, (int)(R * 0.12f)), dk); }
    // ears
    if (sp == 0) { tri(cx - R * 0.78f, cy - R * 0.4f, cx - R * 0.5f, cy - R * 1.15f, cx - R * 0.1f, cy - R * 0.75f, c); tri(cx + R * 0.78f, cy - R * 0.4f, cx + R * 0.5f, cy - R * 1.15f, cx + R * 0.1f, cy - R * 0.75f, c);
                   tri(cx - R * 0.65f, cy - R * 0.55f, cx - R * 0.5f, cy - R * 0.95f, cx - R * 0.28f, cy - R * 0.72f, Col{255, 170, 190}); tri(cx + R * 0.65f, cy - R * 0.55f, cx + R * 0.5f, cy - R * 0.95f, cx + R * 0.28f, cy - R * 0.72f, Col{255, 170, 190}); }
    if (sp == 4) { for (int s = -1; s <= 1; s += 2) { int ex = cx + s * (int)(R * 0.35f); petEllipse(ex, cy - (int)(R * 1.35f), (int)(R * 0.17f), (int)(R * 0.6f), c); petEllipse(ex, cy - (int)(R * 1.3f), (int)(R * 0.09f), (int)(R * 0.45f), Col{255, 175, 195}); } }
    if (sp == 3) { tri(cx - R * 0.55f, cy - R * 0.7f, cx - R * 0.4f, cy - R * 1.2f, cx - R * 0.2f, cy - R * 0.85f, Col{245, 235, 200}); tri(cx + R * 0.55f, cy - R * 0.7f, cx + R * 0.4f, cy - R * 1.2f, cx + R * 0.2f, cy - R * 0.85f, Col{245, 235, 200}); }
    // body
    if (sp == 2) { float w = sinf(t * 3.0f) * R * 0.04f; petEllipse(cx, cy + (int)(R * 0.1f), (int)(R * (1.1f + w / R * 4)), (int)(R * (0.95f - w / R * 4)), c); disc(cx - (int)(R * 0.9f), cy + (int)(R * 0.8f), (int)(R * 0.2f), c); disc(cx + (int)(R * 0.95f), cy + (int)(R * 0.85f), (int)(R * 0.16f), c); }
    else { disc(cx, cy, ri, c); }
    // feet / belly
    disc(cx - (int)(R * 0.45f), cy + (int)(R * 0.9f), (int)(R * 0.28f), dk); disc(cx + (int)(R * 0.45f), cy + (int)(R * 0.9f), (int)(R * 0.28f), dk);
    petEllipse(cx, cy + (int)(R * 0.42f), (int)(R * 0.58f), (int)(R * 0.42f), lt);
    if (sp == 3) for (int i = 0; i < 3; i++) rect(cx - (int)(R * 0.4f), cy + (int)(R * 0.25f) + i * (int)(R * 0.16f), (int)(R * 0.8f), 3, mix(lt, c, 0.4f));
    if (sp == 1) { for (int s = -1; s <= 1; s += 2) { petEllipse(cx + s * (int)(R * 0.95f), cy - (int)(R * 0.1f), (int)(R * 0.22f), (int)(R * 0.5f), dk); } disc(cx, cy + (int)(R * 0.12f), (int)(R * 0.32f), lt); disc(cx, cy + (int)(R * 0.02f), (int)(R * 0.1f), Col{40, 30, 30}); }
    if (sp == 0) { for (int s = -1; s <= 1; s += 2) { line(cx + s * (int)(R * 0.35f), cy + (int)(R * 0.12f), cx + s * (int)(R * 1.0f), cy + (int)(R * 0.0f), 2, WHITE); line(cx + s * (int)(R * 0.35f), cy + (int)(R * 0.2f), cx + s * (int)(R * 1.0f), cy + (int)(R * 0.28f), 2, WHITE); } disc(cx, cy + (int)(R * 0.08f), std::max(2, (int)(R * 0.07f)), Col{255, 130, 150}); }
    if (sp == 4) disc(cx, cy + (int)(R * 0.08f), std::max(2, (int)(R * 0.07f)), Col{255, 130, 150});
    if (stage == 4 && variant == 2) { for (int i = 0; i < 4; i++) { float a = t * 1.5f + i * 1.57f; int sx = cx + (int)(cosf(a) * R * 1.35f), sy = cy - (int)(R * 0.2f) + (int)(sinf(a) * R * 0.5f); line(sx - 6, sy, sx + 6, sy, 2, Col{255, 240, 140}); line(sx, sy - 6, sx, sy + 6, 2, Col{255, 240, 140}); } }
    if (stage == 4 && variant == 0) { for (int i = 0; i < 3; i++) line(cx - (int)(R * 0.7f) + i * 4, cy - (int)(R * 0.9f), cx - (int)(R * 0.6f) + i * 4, cy - (int)(R * 0.7f), 2, dk); }
    if (mood == 3) { rect(cx - ri, cy - ri, 1, 1, c); disc(cx, cy, ri, Col{120, 200, 90}, 60); }
    // cheeks
    disc(cx - (int)(R * 0.55f), cy + (int)(R * 0.12f), std::max(3, (int)(R * 0.12f)), Col{255, 130, 150}, mood == 2 ? 60 : 140); disc(cx + (int)(R * 0.55f), cy + (int)(R * 0.12f), std::max(3, (int)(R * 0.12f)), Col{255, 130, 150}, mood == 2 ? 60 : 140);
    // eyes
    int ey = cy - (int)(R * 0.12f), ex = (int)(R * 0.32f); int er = std::max(4, (int)(R * (stage == 1 ? 0.2f : 0.15f)));
    bool blink = fmodf(t, 4.0f) < 0.15f;
    if (mood == 4 || blink) { for (int s = -1; s <= 1; s += 2) { line(cx + s * ex - er, ey, cx + s * ex, ey + er / 2, 3, Col{40, 30, 40}); line(cx + s * ex, ey + er / 2, cx + s * ex + er, ey, 3, Col{40, 30, 40}); } }
    else for (int s = -1; s <= 1; s += 2) {
        disc(cx + s * ex, ey, er, Col{40, 30, 40}); disc(cx + s * ex - er / 3, ey - er / 3, std::max(2, er / 3), WHITE);
        if (mood == 2 || mood == 3) line(cx + s * ex - er, ey - er - 2 + (s < 0 ? 4 : 0), cx + s * ex + er, ey - er - 2 + (s < 0 ? 0 : 4), 3, dk);
    }
    // mouth
    int my = cy + (int)(R * 0.3f); int mw = (int)(R * 0.2f);
    if (mood == 1) { for (int i = -mw; i < mw; i++) { int yy = my + (int)((float)(i * i) / mw * 0.5f) - 1; disc(cx + i, yy, 2, Col{60, 30, 40}); } }
    else if (mood == 2 || mood == 3) { for (int i = -mw; i < mw; i++) { int yy = my + mw / 2 - (int)((float)(i * i) / mw * 0.5f); disc(cx + i, yy, 2, Col{60, 30, 40}); } }
    else if (mood == 4) { petEllipse(cx, my + 2, std::max(3, mw / 2), std::max(2, mw / 3), Col{60, 30, 40}, 200); }
    else line(cx - mw / 2, my, cx + mw / 2, my, 3, Col{60, 30, 40});
    if (mood == 3) { disc(cx + (int)(R * 0.75f), cy - (int)(R * 0.5f), 5, Col{120, 190, 255}); }
    petHatDraw(cx, cy - ri + (sp == 4 ? 0 : 4), R * 0.9f, p.hat);
}
static float petScale(int stage) { static const float S[5] = {0.9f, 0.55f, 0.75f, 0.92f, 1.08f}; return S[stage]; }

// ---------------------------------------------------------------- mini games
struct PetItem { float x, y, vy; int type; };
static std::vector<PetItem> pcItems; static float pcBx = 640, pcT = 30, pcSpawn = 0; static int pcScore = 0, pcLives = 3; static bool pcOver = false;
static std::vector<int> psSeq; static int psShow = 0, psIn = 0, psPhase = 0, psFlash = -1; static float psT = 0, psFl = 0;
static int phCur = 7, phNext = 0, phStreak = 0, phPhase = 0; static float phT = 0;
static void petReward(int coins, int joy, int energy) {
    gPet.coins += coins; gPet.earned += coins; gPet.happy += joy; gPet.energy -= energy; gPet.games++; petClampAll(); gPetDirty = true;
    if (gPet.games >= 5) petAch(4); if (gPet.earned >= 200) petAch(7);
}
static void petStartGame(int g) {
    if (gPet.stage == 0) { toast("Wait until your egg hatches!"); return; }
    if (gPet.asleep) { toast(gPet.name + " is sleeping"); return; }
    if (gPet.energy < 10) { toast(gPet.name + " is too tired to play"); return; }
    petMg = g; petView = 4;
    if (g == 1) { pcItems.clear(); pcT = 30; pcScore = 0; pcLives = 3; pcOver = false; pcBx = 640; pcSpawn = 0; }
    else if (g == 2) { psSeq.clear(); psSeq.push_back(rand() % 4); psShow = 0; psIn = 0; psPhase = 0; psT = 0.6f; psFlash = -1; }
    else { phCur = 1 + rand() % 13; phNext = 1 + rand() % 13; phStreak = 0; phPhase = 0; }
}
static void petMgUpdate(float dt) {
    if (petView != 4) return;
    if (petMg == 1 && !pcOver) {
        pcT -= dt; pcSpawn -= dt;
        if (pcSpawn <= 0) { pcSpawn = 0.45f + (rand() % 40) / 100.0f; pcItems.push_back({(float)(120 + rand() % 1040), 80.0f, 230.0f + rand() % 160, (rand() % 5 == 0) ? 1 : 0}); }
        for (size_t i = 0; i < pcItems.size();) {
            PetItem& it = pcItems[i]; it.y += it.vy * dt; bool rm = false;
            if (it.y > 560 && it.y < 610 && fabsf(it.x - pcBx) < 80) { if (it.type == 0) { pcScore++; sndPlay(SND_CLICK); } else { pcLives--; pcScore = std::max(0, pcScore - 2); sndPlay(SND_ERROR); } rm = true; }
            else if (it.y > 680) rm = true;
            if (rm) pcItems.erase(pcItems.begin() + i); else i++;
        }
        if (pcT <= 0 || pcLives <= 0) { pcOver = true; petReward(pcScore, std::min(30, pcScore * 2), 10); }
    }
    if (petMg == 2) {
        if (psFl > 0) { psFl -= dt; if (psFl <= 0) psFlash = -1; }
        if (psPhase == 0) { psT -= dt; if (psT <= 0) { if (psShow < (int)psSeq.size()) { psFlash = psSeq[psShow]; psFl = 0.4f; sndPlay(SND_CLICK); psShow++; psT = 0.7f; } else { psPhase = 1; psIn = 0; } } }
    }
    if (petMg == 3 && phPhase == 1) { phT -= dt; if (phT <= 0) { phPhase = 0; phCur = phNext; phNext = 1 + rand() % 13; } }
}
static const Col PS_COL[4] = {{220, 60, 60}, {60, 190, 90}, {60, 120, 230}, {240, 200, 40}};
static void petMgDraw() {
    rect(0, 0, W, UIH, Col{30, 24, 60});
    text(petMg == 1 ? "Catch the treats" : petMg == 2 ? "Memory pads" : "Higher or lower", F44, 70, 24, WHITE);
    btn(W - 230, 30, 160, 48, "Quit", Col{150, 50, 70}, WHITE);
    if (petMg == 1) {
        char b[64]; snprintf(b, sizeof b, "Score %d   Time %d   Lives %d", pcScore, std::max(0, (int)pcT), pcLives); text(b, F22, W / 2, 90, WHITE, 1);
        for (auto& it : pcItems) { if (it.type == 0) { disc((int)it.x, (int)it.y, 18, Col{255, 190, 60}); disc((int)it.x - 5, (int)it.y - 5, 5, Col{255, 240, 180}); } else { disc((int)it.x, (int)it.y, 18, Col{60, 60, 70}); line((int)it.x - 8, (int)it.y - 8, (int)it.x + 8, (int)it.y + 8, 3, Col{230, 60, 60}); line((int)it.x + 8, (int)it.y - 8, (int)it.x - 8, (int)it.y + 8, 3, Col{230, 60, 60}); } }
        quad(pcBx - 80, 570, pcBx + 80, 570, pcBx + 60, 620, pcBx - 60, 620, Col{200, 140, 80}); rect((int)pcBx - 84, 566, 168, 8, Col{150, 100, 50});
        if (!pcOver) text("Drag to move the basket", F16, W / 2, 640, WHITE, 1, 160);
        if (pcOver) { rect(380, 220, 520, 230, Col{20, 16, 46}, 240); text("Game over", F44, W / 2, 240, WHITE, 1); snprintf(b, sizeof b, "You earned %d coins", pcScore); text(b, F30, W / 2, 310, Col{255, 220, 100}, 1); btn(W / 2 - 220, 380, 200, 52, "Play again", Col{0, 114, 198}, WHITE); btn(W / 2 + 20, 380, 200, 52, "Done", Col{90, 90, 100}, WHITE); }
    } else if (petMg == 2) {
        char b[64]; snprintf(b, sizeof b, "Level %d", (int)psSeq.size()); text(b, F30, W / 2, 90, WHITE, 1);
        text(psPhase == 0 ? "Watch..." : psPhase == 1 ? "Your turn" : "Wrong pad!", F22, W / 2, 130, WHITE, 1, 200);
        for (int i = 0; i < 4; i++) { int x = W / 2 - 230 + (i % 2) * 240, y = 170 + (i / 2) * 200; Col c = PS_COL[i]; rect(x, y, 220, 180, i == psFlash ? mix(c, WHITE, 0.55f) : mix(c, BLACK, 0.35f)); }
        if (psPhase == 2) { btn(W / 2 - 220, 590, 200, 52, "Try again", Col{0, 114, 198}, WHITE); btn(W / 2 + 20, 590, 200, 52, "Done", Col{90, 90, 100}, WHITE); }
    } else {
        char b[64]; snprintf(b, sizeof b, "Streak %d", phStreak); text(b, F30, W / 2, 90, WHITE, 1);
        static const char* CN[14] = {"", "A", "2", "3", "4", "5", "6", "7", "8", "9", "10", "J", "Q", "K"};
        rect(W / 2 - 280, 160, 200, 280, WHITE); text(CN[phCur], F130, W / 2 - 180, 230, Col{40, 40, 60}, 1);
        rect(W / 2 + 80, 160, 200, 280, phPhase == 1 ? WHITE : Col{80, 90, 200}); if (phPhase == 1) text(CN[phNext], F130, W / 2 + 180, 230, Col{40, 40, 60}, 1); else text("?", F130, W / 2 + 180, 230, WHITE, 1);
        if (phPhase == 0) { btn(W / 2 - 280, 480, 260, 60, "Higher", Col{40, 150, 80}, WHITE); btn(W / 2 + 20, 480, 260, 60, "Lower", Col{200, 70, 60}, WHITE); btn(W / 2 - 100, 560, 200, 50, "Cash out", Col{90, 90, 100}, WHITE); }
        else if (phPhase == 2) { text("Wrong! Streak lost.", F30, W / 2, 490, Col{255, 150, 150}, 1); btn(W / 2 - 220, 560, 200, 52, "Again", Col{0, 114, 198}, WHITE); btn(W / 2 + 20, 560, 200, 52, "Done", Col{90, 90, 100}, WHITE); }
    }
}
static void petMgTap(int x, int y) {
    if (inR(x, y, W - 230, 30, 160, 48)) { petView = 3; return; }
    if (petMg == 1 && pcOver) { if (inR(x, y, W / 2 - 220, 380, 200, 52)) petStartGame(1); else if (inR(x, y, W / 2 + 20, 380, 200, 52)) petView = 0; return; }
    if (petMg == 2) {
        if (psPhase == 2) { if (inR(x, y, W / 2 - 220, 590, 200, 52)) petStartGame(2); else if (inR(x, y, W / 2 + 20, 590, 200, 52)) petView = 0; return; }
        if (psPhase == 1) for (int i = 0; i < 4; i++) { int px = W / 2 - 230 + (i % 2) * 240, py = 170 + (i / 2) * 200; if (inR(x, y, px, py, 220, 180)) {
            psFlash = i; psFl = 0.25f; sndPlay(SND_CLICK);
            if (i == psSeq[psIn]) { psIn++; if (psIn >= (int)psSeq.size()) { psSeq.push_back(rand() % 4); psShow = 0; psPhase = 0; psT = 0.8f; } }
            else { psPhase = 2; sndPlay(SND_ERROR); petReward((int)psSeq.size() * 2 - 2 > 0 ? (int)psSeq.size() * 2 - 2 : 0, std::min(30, (int)psSeq.size() * 3), 8); }
            return; } }
    }
    if (petMg == 3) {
        if (phPhase == 0) {
            bool hi = inR(x, y, W / 2 - 280, 480, 260, 60), lo = inR(x, y, W / 2 + 20, 480, 260, 60);
            if (inR(x, y, W / 2 - 100, 560, 200, 50)) { petReward(phStreak * 3, std::min(30, phStreak * 5), 6); petView = 0; return; }
            if (hi || lo) { bool ok = hi ? phNext >= phCur : phNext <= phCur; phPhase = 1; phT = 1.0f; if (ok) { phStreak++; sndPlay(SND_UNLOCK); } else { phPhase = 2; sndPlay(SND_ERROR); petReward(0, 5, 6); phStreak = 0; } }
        } else if (phPhase == 2) { if (inR(x, y, W / 2 - 220, 560, 200, 52)) petStartGame(3); else if (inR(x, y, W / 2 + 20, 560, 200, 52)) petView = 0; }
    }
}

// ---------------------------------------------------------------- the room
static void petBar(int x, int y, const char* l, float v, Col c) {
    text(l, F16, x, y, WHITE, 0, 220); rect(x + 90, y + 4, 190, 14, Col{20, 20, 40}, 150); rect(x + 90, y + 4, (int)(190 * v / 100.0f), 14, v < 25 ? Col{230, 70, 70} : c);
}
static void petRoom(float t) {
    bool night = gNow.h >= 20 || gNow.h < 6; bool dusk = (gNow.h >= 18 && gNow.h < 20) || (gNow.h >= 6 && gNow.h < 8);
    Col sky = night ? Col{16, 20, 50} : dusk ? Col{240, 150, 110} : Col{130, 200, 245}, wall = night ? Col{50, 44, 86} : dusk ? Col{210, 170, 170} : Col{245, 225, 190}, floor = night ? Col{70, 50, 70} : Col{190, 140, 100};
    rect(0, 0, W, UIH, wall); rect(0, 470, W, UIH - 470, floor); rect(0, 468, W, 5, mix(floor, BLACK, 0.3f));
    // window
    rect(900, 90, 240, 210, Col{250, 250, 250}); rect(910, 100, 220, 190, sky);
    if (night) { disc(1060, 150, 28, Col{250, 245, 210}); disc(1072, 142, 24, sky); for (int i = 0; i < 9; i++) disc(930 + (i * 53) % 190, 115 + (i * 37) % 150, 2, WHITE, 150 + (i * 40) % 100); }
    else { disc(990, 160, 30, dusk ? Col{255, 190, 90} : Col{255, 235, 120}); disc(1060, 230, 22, WHITE, 230); disc(1085, 230, 28, WHITE, 230); disc(1040, 238, 22, WHITE, 230); }
    rect(1018, 100, 4, 190, Col{250, 250, 250}); rect(910, 192, 220, 4, Col{250, 250, 250});
    // rug and bowl
    petEllipse(640, 560, 260, 40, mix(PET_HUE[gPet.hue], BLACK, 0.2f)); petEllipse(640, 560, 220, 30, mix(PET_HUE[gPet.hue], WHITE, 0.3f));
    rect(1000, 520, 80, 24, Col{200, 80, 80}); rect(1010, 512, 60, 10, Col{120, 60, 40});
    // bed
    rect(120, 430, 220, 60, Col{120, 80, 140}); rect(120, 410, 40, 80, Col{90, 60, 110}); rect(150, 420, 180, 24, Col{240, 240, 255});
}
static void petDrawRoom() {
    PetS& p = gPet; float t = SDL_GetTicks() / 1000.0f; int mood = petMood();
    petRoom(t);
    text(p.name, F44, 70, 22, Col{40, 30, 60}); text(std::string(PET_STAGE[p.stage]) + " " + PET_SP[p.species], F16, 72, 76, Col{70, 60, 90});
    char b[40]; snprintf(b, sizeof b, "%d coins", p.coins); rect(W - 250, 26, 200, 44, Col{40, 30, 70}, 210); disc(W - 232, 48, 12, Col{250, 200, 40}); text(b, F22, W - 60, 34, WHITE, 2);
    if (p.created) {
        petBar(60, 120, "Hunger", p.hunger, Col{240, 150, 60}); petBar(60, 152, "Happy", p.happy, Col{250, 200, 60}); petBar(60, 184, "Energy", p.energy, Col{90, 190, 120}); petBar(60, 216, "Clean", p.clean, Col{90, 170, 240}); petBar(60, 248, "Health", p.health, Col{230, 80, 120});
    }
    float R = 70 * petScale(p.stage); int px = 640, py = 470 - (int)(R * 0.9f);
    if (p.asleep) { px = 230; py = 410; }
    petDraw(px, py, R, p, t, mood, p.stage);
    if (p.asleep) { for (int i = 0; i < 3; i++) text("z", F30 + (i > 1), px + 50 + i * 24, py - 70 - i * 28 - (int)(sinf(t * 2 + i) * 6), WHITE, 0, 230 - i * 50); }
    for (auto& h : petHearts) { int a = (int)(255 * clampf(h.t, 0, 1)); tri(h.x - 9, h.y - 3, h.x + 9, h.y - 3, h.x, h.y + 12, Col{255, 90, 130}, a); disc((int)h.x - 5, (int)h.y - 5, 6, Col{255, 90, 130}, a); disc((int)h.x + 5, (int)h.y - 5, 6, Col{255, 90, 130}, a); }
    // bubble
    std::string bub = petBubT > 0 ? petBub : "";
    if (bub.empty() && p.stage > 0 && !p.asleep) { static const char* H[] = {"Feed me, please!", "I'm so hungry...", "I need a bath!", "I don't feel well...", "Let's play!", "I'm lonely...", "Pet me!", "I love you!", "What a nice day!", "Hehe!"}; int k = (int)(t / 8.0f); bool hungry = p.hunger < 25, dirty = p.clean < 20, sick = p.health < 30, sad = p.happy < 25;
        bub = sick ? H[3] : hungry ? H[(k & 1) ? 0 : 1] : dirty ? H[2] : sad ? H[5] : mood == 1 ? H[7 + (k % 3)] : H[4 + (k % 2) * 2]; }
    if (!bub.empty() && p.stage > 0 && !p.asleep) { int bw = tw(bub, F22) + 40; int bx = px + 80, by2 = py - (int)(R * 1.6f) - 20; bx = std::min(bx, W - bw - 10); rect(bx, by2, bw, 50, WHITE); tri((float)bx + 20, (float)by2 + 50, (float)bx + 50, (float)by2 + 50, (float)bx + 6, (float)by2 + 74, WHITE); text(bub, F22, bx + 20, by2 + 10, Col{40, 30, 60}); }
    if (!p.created) return;
    if (p.stage == 0) { snprintf(b, sizeof b, "Hatching in about %d s", std::max(0, (int)p.eggLeft)); text(b, F22, 640, 560, Col{40, 30, 60}, 1); }
    // gift
    if (p.giftDay != petDayKey()) { btn(W - 250, 84, 200, 44, "Daily gift!", Col{220, 90, 150}, WHITE); }
    // actions
    static const char* AL[7] = {"Feed", "Play", "Sleep", "Wash", "Medicine", "Shop", "Stats"}; static const Col AC[7] = {{230, 130, 50}, {60, 150, 90}, {90, 80, 170}, {50, 150, 220}, {210, 60, 90}, {180, 120, 40}, {80, 90, 110}};
    for (int i = 0; i < 7; i++) btn(79 + i * 162, 592, 150, 58, i == 2 && p.asleep ? "Wake" : AL[i], AC[i], WHITE);
    if (petFeedOpen) {
        rect(0, 0, W, UIH, BLACK, 110); rect(340, 110, 600, 440, Col{250, 246, 240}); text("What would you like to feed?", F30, 640, 124, Col{40, 30, 60}, 1);
        for (int i = 0; i < 5; i++) { int y = 180 + i * 64; rect(360, y, 560, 56, i % 2 ? Col{245, 235, 225} : Col{238, 228, 214}); text(PET_FOOD[i], F22, 380, y + 14, Col{40, 30, 60});
            snprintf(b, sizeof b, "+%d hunger%s", PET_FH[i], PET_FJ[i] ? (std::string(", +") + std::to_string(PET_FJ[i]) + " joy").c_str() : ""); text(b, F16, 540, y + 20, Col{110, 100, 120});
            if (i == 0) text("always", F16, 900, y + 20, Col{110, 100, 120}, 2); else { snprintf(b, sizeof b, "x%d", p.pantry[i]); text(b, F22, 900, y + 14, p.pantry[i] ? Col{40, 30, 60} : Col{190, 60, 60}, 2); } }
        btn(540, 500, 200, 40, "Close", Col{90, 90, 100}, WHITE);
    }
    if (petAchT > 0) { rect(W / 2 - 280, 90, 560, 46, Col{40, 30, 70}, 235); text(petAchMsg, F22, W / 2, 98, Col{255, 220, 100}, 1); }
}
static void petDrawCreate() {
    PetS& p = gPet; float t = SDL_GetTicks() / 1000.0f;
    rect(0, 0, W, UIH, Col{245, 225, 235}); text("Choose your egg", F44, 70, 24, Col{70, 40, 90});
    for (int i = 0; i < 5; i++) { int x = 70 + i * 226; rect(x, 110, 210, 250, p.species == i ? Col{255, 255, 255} : Col{250, 235, 245}); if (p.species == i) outline(x, 110, 210, 250, 4, Col{200, 80, 140});
        PetS tmp = p; tmp.species = i; petDraw(x + 105, 220, 48, tmp, t + i, 1, 3); text(PET_SP[i], F22, x + 105, 308, Col{70, 40, 90}, 1); }
    text("Colour", F22, 70, 386, Col{70, 40, 90}); for (int i = 0; i < 6; i++) { rect(220 + i * 80, 380, 64, 44, PET_HUE[i]); if (p.hue == i) outline(216 + i * 80, 376, 72, 52, 4, Col{70, 40, 90}); }
    text("Name", F22, 70, 456, Col{70, 40, 90}); rect(220, 446, 420, 48, WHITE); text(p.name, F30, 236, 452, Col{40, 30, 60}); btn(660, 446, 160, 48, "Edit name", Col{140, 90, 170}, WHITE);
    btn(70, 540, 400, 70, "Adopt this egg", Col{200, 80, 140}, WHITE);
    text("It hatches in about two minutes, and keeps living while the app is closed.", F16, 500, 566, Col{110, 80, 130});
}
static void petDrawShop() {
    PetS& p = gPet; char b[64];
    rect(0, 0, W, UIH, Col{245, 240, 230}); text("Shop", F44, 70, 24, Col{70, 50, 30}); snprintf(b, sizeof b, "%d coins", p.coins); text(b, F30, W - 70, 34, Col{150, 100, 20}, 2);
    btn(70, 100, 160, 48, "Food", petTab == 0 ? Col{200, 120, 40} : Col{190, 180, 170}, WHITE); btn(244, 100, 160, 48, "Hats", petTab == 1 ? Col{200, 120, 40} : Col{190, 180, 170}, WHITE); btn(W - 230, 100, 160, 48, "Back", Col{90, 90, 100}, WHITE);
    if (petTab == 0) for (int i = 1; i < 5; i++) { int y = 170 + (i - 1) * 76; rect(70, y, 1140, 66, Col{255, 252, 246}); text(PET_FOOD[i], F30, 100, y + 14, Col{70, 50, 30}); snprintf(b, sizeof b, "+%d hunger  +%d joy  (you have %d)", PET_FH[i], PET_FJ[i], p.pantry[i]); text(b, F16, 340, y + 24, Col{130, 110, 90}); snprintf(b, sizeof b, "Buy  %d", PET_FP[i]); btn(1010, y + 8, 180, 50, b, p.coins >= PET_FP[i] ? Col{60, 150, 90} : Col{190, 190, 190}, WHITE); }
    else for (int i = 0; i < 6; i++) { int x = 70 + (i % 3) * 380, y = 170 + (i / 3) * 210; rect(x, y, 360, 190, Col{255, 252, 246}); PetS tmp = p; tmp.hat = i; tmp.stage = 3; petDraw(x + 180, y + 110, 40, tmp, 0.5f, 1, 3);
        text(PET_HAT[i], F22, x + 14, y + 8, Col{70, 50, 30}); bool own = p.hatsOwned & (1 << i); if (own) btn(x + 200, y + 140, 150, 40, p.hat == i ? "Wearing" : "Wear", p.hat == i ? Col{150, 150, 150} : Col{60, 120, 200}, WHITE); else { snprintf(b, sizeof b, "Buy %d", PET_HP[i]); btn(x + 200, y + 140, 150, 40, b, p.coins >= PET_HP[i] ? Col{60, 150, 90} : Col{190, 190, 190}, WHITE); } }
}
static void petDrawGames() {
    rect(0, 0, W, UIH, Col{30, 24, 60}); text("Play together", F44, 70, 24, WHITE); btn(W - 230, 30, 160, 48, "Back", Col{90, 90, 100}, WHITE);
    static const char* GN[3] = {"Catch the treats", "Memory pads", "Higher or lower"}; static const char* GD[3] = {"Drag the basket and catch treats. Avoid the black bombs.", "Repeat the pattern of lights. It gets longer each round.", "Guess whether the next card is higher or lower."}; static const Col GC[3] = {{230, 130, 50}, {60, 150, 190}, {160, 80, 190}};
    for (int i = 0; i < 3; i++) { int x = 70 + i * 390; rect(x, 130, 370, 360, GC[i]); text(GN[i], F30, x + 20, 150, WHITE); auto L = wrapText(GD[i], F22, 330); int y = 214; for (auto& l : L) { text(l, F22, x + 20, y, WHITE, 0, 220); y += 30; } btn(x + 20, 420, 330, 52, "Play", Col{20, 16, 46}, WHITE); }
    text("Games earn coins and make your pet happy, but tire it out.", F22, 70, 520, WHITE, 0, 190);
}
static void petDrawStats() {
    PetS& p = gPet; char b[96];
    rect(0, 0, W, UIH, Col{238, 240, 248}); text("About " + p.name, F44, 70, 24, Col{40, 40, 80}); btn(W - 230, 30, 160, 48, "Back", Col{90, 90, 100}, WHITE);
    snprintf(b, sizeof b, "%s %s", PET_STAGE[p.stage], PET_SP[p.species]); text(b, F30, 70, 110, Col{60, 60, 100});
    snprintf(b, sizeof b, "Age: %dh %dm   Care score: %d%%", (int)(p.age / 3600), (int)(p.age / 60) % 60, (int)petCare()); text(b, F22, 70, 156, Col{60, 60, 100});
    snprintf(b, sizeof b, "Meals %d   Baths %d   Games %d   Daily streak %d", p.meals, p.washes, p.games, p.streak); text(b, F22, 70, 192, Col{60, 60, 100});
    text("Achievements", F30, 70, 250, Col{40, 40, 80});
    for (int i = 0; i < 8; i++) { int x = 70 + (i % 2) * 570, y = 296 + (i / 2) * 70; bool on = p.ach & (1 << i); rect(x, y, 540, 60, on ? Col{255, 245, 200} : Col{225, 228, 238}); text(PET_ACH[i], F22, x + 70, y + 14, on ? Col{60, 50, 20} : Col{140, 140, 160}); disc(x + 32, y + 30, 18, on ? Col{250, 190, 40} : Col{200, 200, 215}); if (on) { line(x + 24, y + 30, x + 30, y + 37, 4, WHITE); line(x + 30, y + 37, x + 42, y + 22, 4, WHITE); } }
}
static void drawPet() {
    petEnsure();
    if (!gPet.created) { petDrawCreate(); return; }
    if (petView == 0) petDrawRoom(); else if (petView == 1) petDrawShop(); else if (petView == 3) petDrawGames(); else if (petView == 4) petMgDraw(); else petDrawStats();
}

// ---------------------------------------------------------------- input
static void petSay(const std::string& s) { petBub = s; petBubT = 3.5f; }
static void petSetName() { if (gPet.name.empty()) gPet.name = PET_NAMES[gPet.species]; if (gPet.name.size() > 14) gPet.name.resize(14); hideKb(); }
static void petTap(int x, int y) {
    petEnsure(); PetS& p = gPet;
    if (!p.created) {
        for (int i = 0; i < 5; i++) if (inR(x, y, 70 + i * 226, 110, 210, 250)) { bool def = p.name == PET_NAMES[p.species]; p.species = i; if (def) p.name = PET_NAMES[i]; }
        for (int i = 0; i < 6; i++) if (inR(x, y, 220 + i * 80, 380, 64, 44)) p.hue = i;
        if (inR(x, y, 660, 446, 160, 48)) { petNameEdit = p.name; openKbStr(&p.name, 14, petSetName); }
        if (inR(x, y, 70, 540, 400, 70)) { p.created = true; p.stage = 0; p.eggLeft = 120; p.last = petNow(); p.hunger = p.happy = p.energy = p.clean = p.health = 90; petSaveNow(); sndPlay(SND_UNLOCK); }
        return;
    }
    if (petView == 4) { petMgTap(x, y); return; }
    if (petView == 3) { if (inR(x, y, W - 230, 30, 160, 48)) petView = 0; for (int i = 0; i < 3; i++) if (inR(x, y, 90 + i * 390, 420, 330, 52)) petStartGame(i + 1); return; }
    if (petView == 5) { if (inR(x, y, W - 230, 30, 160, 48)) petView = 0; return; }
    if (petView == 1) {
        if (inR(x, y, W - 230, 100, 160, 48)) { petView = 0; return; }
        if (inR(x, y, 70, 100, 160, 48)) petTab = 0; else if (inR(x, y, 244, 100, 160, 48)) petTab = 1;
        if (petTab == 0) { for (int i = 1; i < 5; i++) if (inR(x, y, 1010, 178 + (i - 1) * 76, 180, 50)) { if (p.coins >= PET_FP[i]) { p.coins -= PET_FP[i]; p.pantry[i]++; sndPlay(SND_CLICK); gPetDirty = true; } else toast("Not enough coins"); } }
        else for (int i = 0; i < 6; i++) { int bx = 70 + (i % 3) * 380 + 200, by = 170 + (i / 3) * 210 + 140; if (!inR(x, y, bx, by, 150, 40)) continue;
            if (p.hatsOwned & (1 << i)) { p.hat = i; gPetDirty = true; } else if (p.coins >= PET_HP[i]) { p.coins -= PET_HP[i]; p.hatsOwned |= 1 << i; p.hat = i; gPetDirty = true; sndPlay(SND_UNLOCK); } else toast("Not enough coins"); }
        return;
    }
    // room
    if (petFeedOpen) {
        if (inR(x, y, 540, 500, 200, 40) || !inR(x, y, 340, 110, 600, 440)) { petFeedOpen = false; return; }
        for (int i = 0; i < 5; i++) if (inR(x, y, 360, 180 + i * 64, 560, 56)) {
            if (i > 0 && p.pantry[i] <= 0) { toast("Out of " + std::string(PET_FOOD[i]) + " - buy more in the Shop"); return; }
            if (p.hunger > 95) { petSay("I'm full!"); petFeedOpen = false; return; }
            if (i > 0) p.pantry[i]--; p.hunger += PET_FH[i]; p.happy += PET_FJ[i]; p.meals++; petClampAll(); petAch(0); petSay("Yum!"); sndPlay(SND_CLICK); gPetDirty = true; petFeedOpen = false;
            for (int k = 0; k < 3; k++) petHearts.push_back({640.0f + k * 30 - 30, 330.0f, 1.0f});
            return; }
        return;
    }
    if (p.giftDay != petDayKey() && inR(x, y, W - 250, 84, 200, 44)) {
        int yday = petDayKey() - p.giftDay; p.streak = (yday > 0 && yday < 3) ? p.streak + 1 : (p.giftDay == 0 ? 1 : 1); p.giftDay = petDayKey();
        int c = 20 + rand() % 31 + p.streak * 3; p.coins += c; p.earned += c; int f = 1 + rand() % 4; p.pantry[f]++; gPetDirty = true; sndPlay(SND_UNLOCK);
        toast("Daily gift: " + std::to_string(c) + " coins and a " + PET_FOOD[f] + "!"); if (p.streak >= 3) petAch(6); if (p.earned >= 200) petAch(7); return;
    }
    if (y >= 592 && y < 650) {
        int i = (x - 79) / 162; if (i < 0 || i > 6 || (x - 79) % 162 > 150) return;
        if (p.stage == 0 && i != 5 && i != 6) { toast("Your egg hasn't hatched yet"); return; }
        if (p.asleep && i != 2 && i != 5 && i != 6) { toast(p.name + " is sleeping. Tap Wake first."); return; }
        switch (i) {
            case 0: petFeedOpen = true; break;
            case 1: petView = 3; break;
            case 2: p.asleep = !p.asleep; petSay(p.asleep ? "Zzz..." : "Good morning!"); gPetDirty = true; break;
            case 3: if (p.clean > 90) petSay("I'm already clean!"); else { p.clean = 100; p.happy += 4; p.washes++; petClampAll(); petSay("Squeaky clean!"); sndPlay(SND_CLICK); if (p.washes >= 5) petAch(5); gPetDirty = true; } break;
            case 4: if (p.health > 85) petSay("I feel fine!"); else { p.health += 40; p.happy -= 3; petClampAll(); petSay("Ugh... thanks."); sndPlay(SND_CLICK); gPetDirty = true; } break;
            case 5: petView = 1; break;
            case 6: petView = 5; break;
        }
        return;
    }
    float R = 70 * petScale(p.stage); int py = 470 - (int)(R * 0.9f);
    if (p.stage > 0 && !p.asleep && inR(x, y, 640 - (int)(R * 1.2f), py - (int)(R * 1.3f), (int)(R * 2.4f), (int)(R * 2.6f))) { p.happy += 1; petClampAll(); petSay("Boop!"); petHearts.push_back({(float)x, (float)y - 20, 1.0f}); }
}
static void petDrag(int, int) {}
static void petTouch(int phase, int x, int y) {
    if (!gPet.created) return;
    if (petView == 4 && petMg == 1 && !pcOver) { pcBx = clampf((float)x, 90, W - 90); return; }
    if (petView != 0 || petFeedOpen || gPet.stage == 0 || gPet.asleep) return;
    float R = 70 * petScale(gPet.stage); int py = 470 - (int)(R * 0.9f); bool on = inR(x, y, 640 - (int)(R * 1.2f), py - (int)(R * 1.3f), (int)(R * 2.4f), (int)(R * 2.6f));
    if (phase == 0) { petStroking = on; petLastTouchX = x; petLastTouchY = y; petStroke = 0; }
    else if (phase == 1 && petStroking && on) {
        petStroke += (float)(abs(x - petLastTouchX) + abs(y - petLastTouchY)); petLastTouchX = x; petLastTouchY = y;
        if (petStroke > 90) { petStroke = 0; gPet.happy = std::min(100.0f, gPet.happy + 2.5f); petHearts.push_back({(float)x, (float)y - 30, 1.0f}); gPetDirty = true; if (petBubT <= 0) petSay("Purr... that feels nice"); }
    } else if (phase == 2) petStroking = false;
}
static bool petBack() {
    if (!gPet.created) return false;
    if (petFeedOpen) { petFeedOpen = false; return true; }
    if (petView == 4) { petView = 3; return true; }
    if (petView != 0) { petView = 0; return true; }
    if (gPetDirty) petSaveNow();
    return false;
}

// ---------------------------------------------------------------- always-on update (runs even when the app is closed)
static void petUpdate(float dt) {
    petEnsure(); PetS& p = gPet; if (!p.created) return;
    petAcc += dt;
    if (petBubT > 0) petBubT -= dt; if (petAchT > 0) petAchT -= dt;
    for (size_t i = 0; i < petHearts.size();) { petHearts[i].y -= 40 * dt; petHearts[i].t -= dt * 0.8f; if (petHearts[i].t <= 0) petHearts.erase(petHearts.begin() + i); else i++; }
    if (curApp == A_PET) petMgUpdate(dt);
    if (petAcc >= 1.0f) {
        long long now = petNow(); double el = (double)(now - p.last); if (el < 0) el = 0; if (el > 28800) el = 28800; p.last = now; petAcc = 0;
        petStep(el); gPetDirty = true;
        if (curApp != A_PET && p.stage > 0) {
            struct { bool c; const char* m; } N[3] = {{p.hunger < 25, "is hungry!"}, {p.health < 30, "doesn't feel well."}, {p.clean < 20, "needs a bath."}};
            for (int i = 0; i < 3; i++) if (N[i].c && now - p.nGift[i] > 7200) { p.nGift[i] = now; notify(A_PET, p.name, N[i].m); }
        }
    }
    petSave += dt; if (petSave > 20 && gPetDirty) { petSave = 0; petSaveNow(); }
}

// ---------------------------------------------------------------- live tile: the pet's face
static bool petLive(const Tile& t, SDL_Rect r, bool& all) {
    petEnsure(); if (!gPet.created) return false;
    PetS& p = gPet; float tm = SDL_GetTicks() / 1000.0f; int mood = petMood();
    bool small = r.w < 150; Col bg = mix(PET_HUE[p.hue], BLACK, 0.45f); rect(r.x, r.y, r.w, r.h, bg);
    float R = std::min(r.w, r.h) * (small ? 0.3f : 0.26f) * (p.stage == 0 ? 1.0f : petScale(p.stage) / 0.9f);
    int cx = r.x + (r.w >= 300 ? r.w / 4 : r.w / 2), cy = r.y + (int)(r.h * 0.5f);
    petDraw(cx, cy, R, p, tm, mood, p.stage);
    if (r.w >= 300) { text(p.name, F30, r.x + r.w / 2 - 10, r.y + 26, WHITE); auto L = wrapText(petStatus(), F16, r.w / 2 - 20); int y = r.y + 70; for (size_t i = 0; i < L.size() && i < 3; i++) { text(L[i], F16, r.x + r.w / 2 - 10, y, WHITE, 0, 220); y += 22; } }
    else if (!small) text(p.name, F16, r.x + 10, r.y + r.h - 26, WHITE);
    (void)t; all = true; return true;
}
