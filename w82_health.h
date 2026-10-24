// w82_health.h - Health & Fitness (Windows 8.1 Bing Health & Fitness look): tracker, guided workouts, calorie diary, tools, tips
// Not medical advice. Switch has no step sensor, so steps are typed in.

static int hlDay() { return gNow.y * 10000 + gNow.mo * 100 + gNow.d; }
struct HlDay { int day = 0, water = 0, steps = 0, sleep10 = 0, mood = 0, weight10 = 0; };
struct HlEntry { int day, food, qty; };
static std::vector<HlDay> hlDays; static std::vector<HlEntry> hlDiary;
static bool hlLoaded = false; static int hlGoalCal = 2000, hlGoalSteps = 8000, hlGoalWater = 8, hlHeight = 170, hlAge = 25, hlSex = 0, hlAct = 1, hlTipSel = -1;
static int hlTab = 0, hlScroll = 0, hlWo = -1, hlFoodSel = -1; static std::string hlQuery;
static bool hlDirty = false;

static const char* HL_TABS[5] = {"Tracker", "Fitness", "Diet", "Tools", "Tips"};
// ---- food database: name | kcal | protein | carbs | fat (one normal serving)
static const char* HL_FOODS[] = {
 "Apple|95|0|25|0","Banana|105|1|27|0","Orange|62|1|15|0","Dates (3)|200|2|54|0","Grapes (1 cup)|104|1|27|0","Watermelon (1 slice)|86|2|22|0","Strawberries (1 cup)|49|1|12|0","Fig (2)|74|1|19|0",
 "Boiled egg|78|6|1|5","Fried egg|90|6|0|7","Omelette (2 eggs)|190|13|1|14","Milk (1 glass)|120|8|12|5","Yoghurt (1 cup)|150|9|17|4","Cheese (1 slice)|110|7|0|9","Butter (1 tsp)|34|0|0|4","Olive oil (1 tbsp)|119|0|0|14",
 "Bread (1 slice)|80|3|15|1","Baguette (1/4)|180|6|36|1","Khobz (1 piece)|250|8|50|2","Rice (1 cup)|205|4|45|0","Pasta (1 cup)|220|8|43|1","Couscous (1 cup)|176|6|36|0","Semolina (1/2 cup)|300|10|60|1","Oats (1/2 cup)|150|5|27|3",
 "Chicken breast|165|31|0|4","Chicken thigh|210|26|0|11","Beef steak|270|26|0|18","Minced beef (100g)|250|26|0|17","Lamb (100g)|290|25|0|21","Tuna (1 can)|120|26|0|1","Sardines (1 can)|190|22|0|11","Grilled fish|180|30|0|6",
 "Lentils (1 cup)|230|18|40|1","Chickpeas (1 cup)|270|15|45|4","White beans (1 cup)|250|15|45|1","Falafel (3)|170|7|16|10","Hummus (2 tbsp)|70|2|6|5","Peanuts (handful)|170|7|5|14","Almonds (handful)|165|6|6|14","Walnuts (handful)|185|4|4|18",
 "Tomato|22|1|5|0","Cucumber|16|1|4|0","Carrot|25|1|6|0","Potato (boiled)|130|3|30|0","Chips (1 portion)|365|4|48|17","Onion|44|1|10|0","Lettuce (1 cup)|8|1|2|0","Salad (mixed)|60|2|8|3",
 "Pizza slice|285|12|36|10","Burger|480|25|40|25","Shawarma wrap|520|28|45|24","Tacos (2)|350|16|34|16","Sandwich|350|15|38|14","Kebab skewer|240|22|2|16","Chorba (1 bowl)|180|9|22|6","Chakchouka|220|10|14|13",
 "Tajine (1 plate)|420|30|30|20","Mhajeb (1)|230|6|34|8","Brik|260|11|20|15","Rechta (1 plate)|480|22|65|14","Makroud (1)|180|2|28|7","Baklava (1)|240|4|28|13","Cake slice|350|4|50|15","Ice cream (1 scoop)|140|2|16|7",
 "Chocolate bar|230|3|26|13","Biscuit|70|1|10|3","Cereal (1 bowl)|180|4|38|2","Honey (1 tbsp)|64|0|17|0","Sugar (1 tsp)|16|0|4|0","Jam (1 tbsp)|56|0|14|0","Nutella (1 tbsp)|100|1|11|6","Peanut butter (1 tbsp)|95|4|3|8",
 "Water|0|0|0|0","Tea (no sugar)|2|0|0|0","Coffee (black)|2|0|0|0","Orange juice (1 glass)|110|2|26|0","Cola (1 can)|140|0|39|0","Energy drink|110|0|28|0","Milkshake|350|9|55|10","Mint tea (sugar)|50|0|13|0",
 "Soup (1 bowl)|120|5|16|4","Lasagna|380|20|34|18","Fried rice|330|9|52|10","Spaghetti bolognese|420|20|55|14"};
static const int HL_NF = (int)(sizeof(HL_FOODS) / sizeof(HL_FOODS[0]));
struct HlF { std::string n; int k, p, c, f; };
static HlF hlFood(int i) { HlF r; std::string s = HL_FOODS[i]; std::vector<std::string> v; size_t a = 0; while (true) { size_t b = s.find('|', a); if (b == std::string::npos) { v.push_back(s.substr(a)); break; } v.push_back(s.substr(a, b - a)); a = b + 1; } r.n = v[0]; r.k = atoi(v[1].c_str()); r.p = atoi(v[2].c_str()); r.c = atoi(v[3].c_str()); r.f = atoi(v[4].c_str()); return r; }

// ---- workouts: name | minutes | kcal | exercises "name:seconds,..."
struct HlWo { const char* name; int kcal; const char* ex; Col col; };
static const HlWo HL_WO[] = {
 {"Morning wake-up", 70, "Jumping jacks:30,Arm circles:30,Hip rotations:30,Marching on the spot:40,Neck rolls:20,Toe touches:30", {240, 160, 30}},
 {"7-minute workout", 90, "Jumping jacks:30,Wall sit:30,Push-ups:30,Crunches:30,Step-ups on a chair:30,Squats:30,Chair dips:30,Plank:30,High knees:30,Lunges:30,Push-up and rotation:30,Side plank:30", {200, 60, 50}},
 {"Core blast", 80, "Plank:40,Crunches:40,Bicycle crunches:40,Leg raises:40,Russian twists:40,Mountain climbers:40,Side plank left:30,Side plank right:30,Superman hold:30", {160, 40, 120}},
 {"Upper body", 85, "Push-ups:40,Diamond push-ups:30,Chair dips:40,Pike push-ups:30,Plank shoulder taps:40,Arm circles:30,Superman hold:30", {40, 100, 190}},
 {"Legs and glutes", 90, "Squats:45,Lunges:45,Glute bridges:45,Wall sit:40,Calf raises:40,Jump squats:30,Side lunges:40,Donkey kicks:40", {30, 140, 90}},
 {"Cardio burn", 120, "High knees:40,Jumping jacks:40,Burpees:30,Mountain climbers:40,Skater hops:40,Butt kicks:40,Shadow boxing:60,Star jumps:30", {230, 90, 30}},
 {"Stretch and relax", 40, "Neck stretch:30,Shoulder stretch:30,Cat-cow:40,Child's pose:45,Hamstring stretch:40,Quad stretch left:30,Quad stretch right:30,Deep breathing:60", {110, 90, 190}},
 {"Beginner full body", 60, "Marching on the spot:40,Wall push-ups:30,Chair squats:30,Standing crunches:30,Glute bridges:30,Bird-dog:30,Plank on knees:20", {20, 150, 170}},
 {"Prayer-time stretch", 30, "Standing stretch:30,Back stretch:30,Hip opener:30,Seated twist:30,Neck roll:20,Deep breathing:40", {90, 130, 60}},
 {"Desk break", 30, "Seated twist:30,Shoulder rolls:30,Wrist stretch:20,Standing calf raises:30,Neck tilt:20,Stand and reach:30", {70, 110, 160}},
 {"HIIT 15", 150, "Burpees:40,Squat jumps:40,Mountain climbers:40,Push-ups:40,High knees:40,Plank jacks:40,Skater hops:40,Tuck jumps:30", {190, 30, 50}},
 {"Football fitness", 110, "Jogging on the spot:60,Lateral shuffles:40,Sprint starts:30,Lunges:40,Jump squats:30,Calf raises:30,Planks:40,Hamstring stretch:40", {40, 130, 60}}};
static const int HL_NW = (int)(sizeof(HL_WO) / sizeof(HL_WO[0]));
struct HlEx { std::string n; int secs; };
static std::vector<HlEx> hlExList(int w) { std::vector<HlEx> v; std::string s = HL_WO[w].ex; size_t a = 0; while (a < s.size()) { size_t b = s.find(',', a); if (b == std::string::npos) b = s.size(); std::string t = s.substr(a, b - a); size_t c = t.find(':'); if (c != std::string::npos) v.push_back({t.substr(0, c), atoi(t.c_str() + c + 1)}); a = b + 1; } return v; }
static int hlWoSecs(int w) { int t = 0; auto v = hlExList(w); for (auto& e : v) t += e.secs + 10; return t; }
// guided session
static bool hlRun = false, hlRest = false, hlPaused = false; static int hlIdx = 0; static float hlLeft = 0; static std::vector<HlEx> hlSess;
static void hlStartWo(int w) { hlSess = hlExList(w); hlIdx = 0; hlRest = true; hlLeft = 10; hlRun = true; hlPaused = false; sndPlay(SND_NOTIFY); }

// ---- tips
static const char* HL_TIPS[][2] = {
 {"Drink water through the day", "Most adults do well with around 2 litres of fluids a day, more in hot weather or when you exercise. Keep a glass near you and sip often. Thirst, a dry mouth and dark urine are signs you need more."},
 {"Sleep is part of fitness", "Aim for 7 to 9 hours. Keep a regular bedtime, dim the lights and put screens away 30 minutes before sleep. A cool, dark room helps. A short nap of 20 minutes can refresh you without ruining your night."},
 {"Move a little every hour", "Sitting for long stretches is hard on your back and energy. Stand up, stretch and walk for two minutes every hour. Even short walks after meals help you feel better."},
 {"Build a balanced plate", "Fill half your plate with vegetables and fruit, a quarter with protein such as fish, eggs, chicken or pulses, and a quarter with whole grains. Keep sweets and fried food for treats."},
 {"Protect your eyes", "Follow the 20-20-20 rule: every 20 minutes look at something 20 feet (6 metres) away for 20 seconds. Keep your screen at arm's length and use a lower brightness at night."},
 {"Warm up before you train", "Five minutes of easy movement raises your heart rate and loosens joints. Finish with slow stretching. Stop if anything hurts sharply, and rest between hard days."},
 {"Eat slowly", "It takes about 20 minutes for your body to feel full. Put your fork down between bites, chew well and stop when you are comfortably satisfied, not stuffed."},
 {"Care for your mind", "Take short breaks, talk to someone you trust and spend a little time outside each day. Slow breathing (in for 4, out for 6) can calm you in a minute. If you feel low for a long time, speak to a doctor or counsellor."},
 {"Sunshine and vitamin D", "Ten to twenty minutes of morning or late-afternoon sun on your arms helps your body make vitamin D. Use sunscreen for longer time outside and avoid the harshest midday hours."},
 {"Fasting safely", "If you fast, drink plenty of water at night, eat dates, soup and a balanced meal at iftar, and have a slow, protein-rich suhoor. Do light exercise after iftar. Ask your doctor first if you have a health condition."}};
static const int HL_NT = (int)(sizeof(HL_TIPS) / sizeof(HL_TIPS[0]));

// ---- persistence
static std::string hlFile() { return DIR_ + "health.txt"; }
static HlDay& hlToday() { int d = hlDay(); for (auto& x : hlDays) if (x.day == d) return x; HlDay n; n.day = d; hlDays.push_back(n); return hlDays.back(); }
static void hlSave() {
    std::string s = "G " + std::to_string(hlGoalCal) + " " + std::to_string(hlGoalSteps) + " " + std::to_string(hlGoalWater) + " " + std::to_string(hlHeight) + " " + std::to_string(hlAge) + " " + std::to_string(hlSex) + " " + std::to_string(hlAct) + "\n";
    for (auto& d : hlDays) s += "D " + std::to_string(d.day) + " " + std::to_string(d.water) + " " + std::to_string(d.steps) + " " + std::to_string(d.sleep10) + " " + std::to_string(d.mood) + " " + std::to_string(d.weight10) + "\n";
    for (auto& e : hlDiary) s += "F " + std::to_string(e.day) + " " + std::to_string(e.food) + " " + std::to_string(e.qty) + "\n";
    writeFile(hlFile(), s); hlDirty = false;
}
static void hlLoad() {
    hlLoaded = true; std::string d; if (!readFile(hlFile(), d)) return;
    size_t pos = 0; while (pos < d.size()) {
        size_t e = d.find('\n', pos); if (e == std::string::npos) e = d.size(); std::string l = d.substr(pos, e - pos); pos = e + 1;
        int a[8] = {0}; char c = l.empty() ? 0 : l[0];
        if (c == 'G' && sscanf(l.c_str(), "G %d %d %d %d %d %d %d", &a[0], &a[1], &a[2], &a[3], &a[4], &a[5], &a[6]) == 7) { hlGoalCal = a[0]; hlGoalSteps = a[1]; hlGoalWater = a[2]; hlHeight = a[3]; hlAge = a[4]; hlSex = a[5]; hlAct = a[6]; }
        else if (c == 'D' && sscanf(l.c_str(), "D %d %d %d %d %d %d", &a[0], &a[1], &a[2], &a[3], &a[4], &a[5]) == 6) { HlDay x; x.day = a[0]; x.water = a[1]; x.steps = a[2]; x.sleep10 = a[3]; x.mood = a[4]; x.weight10 = a[5]; hlDays.push_back(x); }
        else if (c == 'F' && sscanf(l.c_str(), "F %d %d %d", &a[0], &a[1], &a[2]) == 3 && a[1] >= 0 && a[1] < HL_NF) hlDiary.push_back({a[0], a[1], a[2]});
    }
}
static int hlTodayCal() { int t = 0, d = hlDay(); for (auto& e : hlDiary) if (e.day == d) t += hlFood(e.food).k * e.qty; return t; }
static float hlLastWeight() { for (int i = (int)hlDays.size() - 1; i >= 0; i--) if (hlDays[i].weight10 > 0) return hlDays[i].weight10 / 10.0f; return 0; }
static std::string hlStatus() {
    if (!hlLoaded) hlLoad(); HlDay& t = hlToday(); char b[160];
    snprintf(b, sizeof b, "Today: %d of %d glasses of water, %d steps, %d calories eaten.", t.water, hlGoalWater, t.steps, hlTodayCal()); return b;
}
static bool hlLive(const Tile&, SDL_Rect r, bool& all) {
    if (!hlLoaded) hlLoad(); HlDay& t = hlToday(); bool small = r.w < 150;
    int cx = r.x, cy = r.y; float pw = std::min(1.0f, t.water / (float)std::max(1, hlGoalWater));
    if (small) { text(std::to_string(t.water), F44, cx + r.w / 2, cy + 8, WHITE, 1); text("glasses", F16, cx + r.w / 2, cy + r.h - 24, WHITE, 1); all = true; return true; }
    text(std::to_string(t.steps), F44, cx + 14, cy + 8, WHITE); text("steps today", F16, cx + 16, cy + 62, WHITE, 0, 200);
    text(std::to_string(t.water) + "/" + std::to_string(hlGoalWater) + " water", F22, cx + 16, cy + 92, WHITE);
    rect(cx + 16, cy + r.h - 38, r.w - 32, 8, WHITE, 70); rect(cx + 16, cy + r.h - 38, (int)((r.w - 32) * pw), 8, WHITE);
    text("Health & Fitness", F16, cx + 10, cy + r.h - 26 + 4, WHITE, 0, 0); all = true; return true;
}

static void hlInit() { if (!hlLoaded) hlLoad(); hlScroll = 0; hlTab = 0; hlWo = -1; hlRun = false; hlTipSel = -1; hlFoodSel = -1; }
static void hlUpdate(float dt) {
    if (hlRun && !hlPaused) {
        hlLeft -= dt;
        if (hlLeft <= 0) {
            if (hlRest) { hlRest = false; hlLeft = (float)hlSess[hlIdx].secs; sndPlay(SND_NOTIFY); }
            else { hlIdx++; if (hlIdx >= (int)hlSess.size()) { hlRun = false; sndPlay(SND_UNLOCK); HlDay& t = hlToday(); t.steps += 0; toast("Workout complete. Well done!"); notify(A_HEALTH, "Workout complete", "Nice work today."); } else { hlRest = true; hlLeft = 10; sndPlay(SND_CLICK); } }
        }
    }
    static float acc = 0; acc += dt; if (hlDirty && acc > 3) { acc = 0; hlSave(); }
}
static bool hlBack() { if (hlRun) { hlRun = false; return true; } if (hlWo >= 0) { hlWo = -1; return true; } if (hlTipSel >= 0) { hlTipSel = -1; return true; } return false; }

// ---- drawing
static void hlMoodFace(int cx, int cy, int r, int m, Col c) {
    ring(cx, cy, r, 3, c, Col{0, 90, 62}); disc(cx - r / 3, cy - r / 4, 3, c); disc(cx + r / 3, cy - r / 4, 3, c);
    int my = cy + r / 2; if (m <= 1) { line(cx - r / 3, my, cx, my - 5, 3, c); line(cx, my - 5, cx + r / 3, my, 3, c); } else if (m == 2) line(cx - r / 3, my - 2, cx + r / 3, my - 2, 3, c); else { line(cx - r / 3, my - 5, cx, my, 3, c); line(cx, my, cx + r / 3, my - 5, 3, c); }
}
static void hlProgress(int x, int y, int w, float p, Col c) { rect(x, y, w, 10, WHITE, 50); rect(x, y, (int)(w * std::max(0.0f, std::min(1.0f, p))), 10, c); }
static void hlStepper(int x, int y, const std::string& label, const std::string& val) { text(label, F16, x, y, WHITE, 0, 190); btn(x, y + 22, 52, 44, "-", Col{0, 80, 56}, WHITE); text(val, F22, x + 130, y + 30, WHITE, 1); btn(x + 208, y + 22, 52, 44, "+", Col{0, 80, 56}, WHITE); }
static void hlDrawTracker() {
    HlDay& t = hlToday(); Col card{0, 90, 62}; char b[64];
    rect(70, 170, 560, 150, card); text("Water", F22, 86, 180, WHITE);
    for (int i = 0; i < 10; i++) { int x = 86 + i * 54, y = 220; bool on = i < t.water; rect(x, y, 40, 54, on ? Col{80, 190, 255} : Col{255, 255, 255}, on ? 255 : 50); if (i >= hlGoalWater) rect(x, y, 40, 54, BLACK, 60); }
    snprintf(b, sizeof b, "%d of %d glasses (about %.1f L)", t.water, hlGoalWater, t.water * 0.25f); text(b, F16, 86, 286, WHITE, 0, 210);
    rect(70, 332, 560, 150, card); text("Steps", F22, 86, 342, WHITE); text(std::to_string(t.steps), F44, 86, 372, WHITE);
    hlProgress(86, 440, 528, t.steps / (float)hlGoalSteps, Col{255, 200, 60}); btn(380, 350, 76, 46, "-1000", Col{0, 80, 56}, WHITE); btn(466, 350, 70, 46, "+500", Col{0, 80, 56}, WHITE); btn(546, 350, 70, 46, "+1000", Col{0, 80, 56}, WHITE);
    text("Goal " + std::to_string(hlGoalSteps) + " - typed in, the Switch has no step sensor", F16, 86, 454, WHITE, 0, 170);
    rect(70, 494, 270, 160, card); text("Sleep", F22, 86, 502, WHITE); snprintf(b, sizeof b, "%.1f h", t.sleep10 / 10.0f); text(b, F44, 86, 534, WHITE); btn(86, 596, 70, 44, "-", Col{0, 80, 56}, WHITE); btn(166, 596, 70, 44, "+", Col{0, 80, 56}, WHITE);
    rect(360, 494, 270, 160, card); text("Mood", F22, 376, 502, WHITE); for (int i = 0; i < 5; i++) { int cx = 392 + i * 50; hlMoodFace(cx, 570, 20, i, t.mood == i + 1 ? Col{255, 220, 60} : Col{255, 255, 255}); if (t.mood == i + 1) disc(cx, 612, 4, Col{255, 220, 60}); }
    rect(650, 170, 580, 484, card); text("Weight", F22, 666, 180, WHITE);
    float lw = hlLastWeight(); if (t.weight10 > 0) lw = t.weight10 / 10.0f; snprintf(b, sizeof b, lw > 0 ? "%.1f kg" : "-", lw); text(b, F44, 666, 210, WHITE);
    btn(1000, 196, 70, 46, "-", Col{0, 80, 56}, WHITE); btn(1080, 196, 70, 46, "+", Col{0, 80, 56}, WHITE); text("0.1 kg", F16, 1160, 210, WHITE, 0, 170);
    std::vector<float> ws; for (auto& d : hlDays) if (d.weight10 > 0) ws.push_back(d.weight10 / 10.0f); if (ws.size() > 30) ws.erase(ws.begin(), ws.end() - 30);
    int gx = 670, gy = 290, gw = 540, gh = 330; rect(gx, gy, gw, gh, BLACK, 50);
    if (ws.size() >= 2) { float mn = *std::min_element(ws.begin(), ws.end()) - 0.5f, mx = *std::max_element(ws.begin(), ws.end()) + 0.5f; for (size_t i = 1; i < ws.size(); i++) line(gx + 20 + (int)((i - 1) * (gw - 40) / (ws.size() - 1)), gy + gh - 20 - (int)((ws[i - 1] - mn) / (mx - mn) * (gh - 40)), gx + 20 + (int)(i * (gw - 40) / (ws.size() - 1)), gy + gh - 20 - (int)((ws[i] - mn) / (mx - mn) * (gh - 40)), 3, Col{255, 220, 60}); snprintf(b, sizeof b, "%.1f", mx); text(b, F16, gx + 6, gy + 4, WHITE, 0, 170); snprintf(b, sizeof b, "%.1f", mn); text(b, F16, gx + 6, gy + gh - 24, WHITE, 0, 170); }
    else text("Log your weight on two different days to see a graph.", F16, gx + gw / 2, gy + gh / 2 - 8, WHITE, 1, 170);
}
static void hlDrawFitness() {
    if (hlWo < 0) {
        SDL_Rect clip = {0, 168, W, UIH - 168}; SDL_RenderSetClipRect(gR, &clip); int rows = (HL_NW + 2) / 3; hlScroll = std::max(0, std::min(hlScroll, std::max(0, rows * 150 - (UIH - 180))));
        for (int i = 0; i < HL_NW; i++) { int x = 70 + (i % 3) * 390, y = 176 + (i / 3) * 150 - hlScroll; if (y + 140 < 168 || y > UIH) continue; rect(x, y, 376, 138, HL_WO[i].col); text(HL_WO[i].name, F30, x + 16, y + 12, WHITE); char b[64]; snprintf(b, sizeof b, "%d min  -  about %d kcal", (hlWoSecs(i) + 30) / 60, HL_WO[i].kcal); text(b, F16, x + 16, y + 62, WHITE, 0, 220); text(std::to_string(hlExList(i).size()) + " exercises", F16, x + 16, y + 90, WHITE, 0, 200); }
        SDL_RenderSetClipRect(gR, nullptr); return;
    }
    if (hlRun) {
        rect(0, 0, W, UIH, HL_WO[hlWo].col); auto& e = hlSess[std::min(hlIdx, (int)hlSess.size() - 1)];
        text(HL_WO[hlWo].name, F30, 70, 36, WHITE, 0, 200); text("Exercise " + std::to_string(hlIdx + 1) + " of " + std::to_string(hlSess.size()), F22, W - 70, 40, WHITE, 2, 200);
        text(hlRest ? "Get ready" : "Go!", F44, W / 2, 130, WHITE, 1, 220); text(e.n, F72, W / 2, 190, WHITE, 1);
        char b[16]; snprintf(b, sizeof b, "%d", (int)ceilf(std::max(0.0f, hlLeft))); text(b, F130, W / 2, 330, WHITE, 1);
        float tot = hlRest ? 10.0f : (float)e.secs; ring(W / 2, 400, 0, 0, WHITE, WHITE); hlProgress(240, 500, 800, 1.0f - hlLeft / tot, WHITE);
        if (hlIdx + 1 < (int)hlSess.size()) text("Next: " + hlSess[hlIdx + 1].n, F22, W / 2, 530, WHITE, 1, 200);
        btn(W / 2 - 330, 580, 200, 56, hlPaused ? "Resume" : "Pause", Col{0, 0, 0}, WHITE); btn(W / 2 - 100, 580, 200, 56, "Skip", Col{0, 0, 0}, WHITE); btn(W / 2 + 130, 580, 200, 56, "Stop", Col{150, 30, 30}, WHITE); return;
    }
    rect(70, 168, W - 140, UIH - 190, HL_WO[hlWo].col); text(HL_WO[hlWo].name, F44, 96, 180, WHITE); auto v = hlExList(hlWo); int y = 250;
    for (auto& e : v) { text(e.n, F22, 96, y, WHITE); text(std::to_string(e.secs) + " s", F22, 560, y, WHITE, 2, 210); y += 34; if (y > 600) break; }
    btn(660, 200, 240, 60, "Start workout", Col{0, 0, 0}, WHITE); btn(920, 200, 200, 60, "Back", Col{0, 0, 0}, WHITE);
    text("10 seconds of rest between exercises.", F16, 660, 280, WHITE, 0, 210); text("Warm up first and stop if anything hurts.", F16, 660, 306, WHITE, 0, 210);
}
static std::vector<int> hlFoodList() { std::vector<int> v; std::string q = lower(hlQuery); for (int i = 0; i < HL_NF; i++) if (q.empty() || lower(hlFood(i).n).find(q) != std::string::npos) v.push_back(i); return v; }
static void hlDrawDiet() {
    int d = hlDay(); int tot = hlTodayCal(); int P = 0, C = 0, Fa = 0;
    for (auto& e : hlDiary) if (e.day == d) { HlF f = hlFood(e.food); P += f.p * e.qty; C += f.c * e.qty; Fa += f.f * e.qty; }
    drawField(70, 168, 520, 46, &hlQuery, "Search foods", F22, Col{30, 30, 34}, Col{245, 245, 245}, Col{200, 200, 205});
    auto L = hlFoodList(); SDL_Rect clip = {0, 224, 620, UIH - 224}; SDL_RenderSetClipRect(gR, &clip); hlScroll = std::max(0, std::min(hlScroll, std::max(0, (int)L.size() * 48 - (UIH - 234))));
    for (size_t i = 0; i < L.size(); i++) { int y = 228 + (int)i * 48 - hlScroll; if (y + 44 < 224 || y > UIH) continue; HlF f = hlFood(L[i]); rect(70, y, 540, 44, WHITE, 36); text(fitTxt(f.n, F22, 370), F22, 82, y + 8, WHITE); text(std::to_string(f.k) + " kcal", F16, 560, y + 13, WHITE, 2, 210); text("+", F30, 596, y + 4, WHITE, 1); }
    SDL_RenderSetClipRect(gR, nullptr);
    rect(640, 168, 590, 486, Col{0, 90, 62}); char b[64]; text("Today", F22, 656, 178, WHITE); snprintf(b, sizeof b, "%d / %d kcal", tot, hlGoalCal); text(b, F44, 656, 208, tot > hlGoalCal ? Col{255, 140, 120} : WHITE);
    hlProgress(656, 266, 558, tot / (float)hlGoalCal, tot > hlGoalCal ? Col{255, 110, 90} : Col{120, 230, 160}); btn(1090, 180, 56, 40, "-", Col{0, 60, 42}, WHITE); btn(1154, 180, 56, 40, "+", Col{0, 60, 42}, WHITE);
    snprintf(b, sizeof b, "Protein %dg   Carbs %dg   Fat %dg", P, C, Fa); text(b, F16, 656, 286, WHITE, 0, 220);
    int y = 318; SDL_Rect c2 = {640, 310, 590, 344}; SDL_RenderSetClipRect(gR, &c2); int n = 0;
    for (int i = (int)hlDiary.size() - 1; i >= 0 && n < 7; i--) if (hlDiary[i].day == d) { HlF f = hlFood(hlDiary[i].food); text(fitTxt(std::to_string(hlDiary[i].qty) + " x " + f.n, F22, 430), F22, 656, y, WHITE); text(std::to_string(f.k * hlDiary[i].qty), F22, 1170, y, WHITE, 2, 220); text("x", F30, 1204, y - 4, WHITE, 1, 200); y += 40; n++; }
    if (!n) text("Tap a food on the left to add it.", F22, 656, 330, WHITE, 0, 190); SDL_RenderSetClipRect(gR, nullptr);
}
static void hlDrawTools() {
    char b[96]; Col card{0, 90, 62};
    rect(70, 168, 560, 240, card); text("Body mass index", F22, 86, 178, WHITE);
    float w = hlLastWeight(); float hm = hlHeight / 100.0f; float bmi = (w > 0 && hm > 0) ? w / (hm * hm) : 0;
    hlStepper(86, 214, "Height (cm)", std::to_string(hlHeight));
    if (bmi > 0) { const char* cat = bmi < 18.5f ? "Underweight range" : bmi < 25 ? "Healthy range" : bmi < 30 ? "Overweight range" : "Obese range"; snprintf(b, sizeof b, "BMI %.1f - %s", bmi, cat); text(b, F30, 86, 312, WHITE); text("Uses your latest weight from the Tracker. BMI is only a rough guide.", F16, 86, 356, WHITE, 0, 190); }
    else text("Log your weight in the Tracker to see your BMI.", F22, 86, 316, WHITE, 0, 200);
    rect(650, 168, 580, 240, card); text("Daily calories", F22, 666, 178, WHITE);
    hlStepper(666, 214, "Age", std::to_string(hlAge)); btn(1000, 236, 100, 44, hlSex ? "Female" : "Male", Col{0, 60, 42}, WHITE); static const char* AC[4] = {"Rest", "Light", "Active", "Very"}; btn(1110, 236, 100, 44, AC[hlAct], Col{0, 60, 42}, WHITE);
    if (w > 0) { double bmr = 10 * w + 6.25 * hlHeight - 5 * hlAge + (hlSex ? -161 : 5); static const double M[4] = {1.2, 1.375, 1.55, 1.725}; snprintf(b, sizeof b, "BMR %.0f kcal   Needs about %.0f kcal a day", bmr, bmr * M[hlAct]); text(b, F22, 666, 312, WHITE); text("Mifflin-St Jeor estimate. Tap Set goal to use it.", F16, 666, 350, WHITE, 0, 190); btn(1050, 346, 160, 40, "Set goal", Col{0, 60, 42}, WHITE); }
    rect(70, 420, 560, 234, card); text("Heart-rate zones", F22, 86, 430, WHITE); int mx = 220 - hlAge; static const char* ZN[4] = {"Warm-up", "Fat burn", "Cardio", "Peak"}; static const float Z[5] = {0.5f, 0.6f, 0.7f, 0.85f, 1.0f};
    for (int i = 0; i < 4; i++) { int y = 470 + i * 44; rect(86, y, 6, 34, Col{120, 230, 160 - i * 30}); text(ZN[i], F22, 104, y + 4, WHITE); snprintf(b, sizeof b, "%d - %d bpm", (int)(mx * Z[i]), (int)(mx * Z[i + 1])); text(b, F22, 600, y + 4, WHITE, 2, 230); }
    rect(650, 420, 580, 234, card); text("Hydration", F22, 666, 430, WHITE); if (w > 0) { snprintf(b, sizeof b, "About %.1f L a day for %.0f kg", w * 0.033f, w); text(b, F30, 666, 470, WHITE); snprintf(b, sizeof b, "That is roughly %d glasses of 250 ml.", (int)(w * 0.033f * 4 + 0.5f)); text(b, F22, 666, 520, WHITE, 0, 220); btn(666, 580, 200, 44, "Set water goal", Col{0, 60, 42}, WHITE); } else text("Log your weight first.", F22, 666, 470, WHITE, 0, 200);
}
static void hlDrawTips() {
    if (hlTipSel >= 0) { rect(70, 168, W - 140, UIH - 190, Col{0, 90, 62}); text(HL_TIPS[hlTipSel][0], F44, 96, 184, WHITE); int y = 260; for (auto& l : wrapText(HL_TIPS[hlTipSel][1], F30, W - 220)) { text(l, F30, 96, y, WHITE, 0, 235); y += 44; } text("General information, not medical advice. See a doctor for health concerns.", F16, 96, 600, WHITE, 0, 170); return; }
    for (int i = 0; i < HL_NT; i++) { int x = 70 + (i % 2) * 590, y = 172 + (i / 2) * 96; rect(x, y, 570, 86, Col{0, 90, 62}); text(fitTxt(HL_TIPS[i][0], F30, 530), F30, x + 16, y + 10, WHITE); text(fitTxt(HL_TIPS[i][1], F16, 530), F16, x + 16, y + 52, WHITE, 0, 180); }
}
static void drawHealth() {
    if (!hlLoaded) hlLoad(); if (hlRun) { hlDrawFitness(); return; }
    rect(0, 0, W, UIH, fdBg(A_HEALTH)); appTitle("Health & Fitness"); fdPivot(HL_TABS, 5, hlTab, 116);
    if (hlTab == 0) hlDrawTracker(); else if (hlTab == 1) hlDrawFitness(); else if (hlTab == 2) hlDrawDiet(); else if (hlTab == 3) hlDrawTools(); else hlDrawTips();
}
// ---- input
static void hlTap(int x, int y) {
    if (!hlLoaded) hlLoad(); HlDay& t = hlToday();
    if (hlRun) { if (inR(x, y, W / 2 - 330, 580, 200, 56)) hlPaused = !hlPaused; else if (inR(x, y, W / 2 - 100, 580, 200, 56)) { hlLeft = 0; } else if (inR(x, y, W / 2 + 130, 580, 200, 56)) hlRun = false; return; }
    int ti = fdPivotHit(HL_TABS, 5, x, y, 116); if (ti >= 0) { hlTab = ti; hlScroll = 0; hlWo = -1; hlTipSel = -1; return; }
    if (hlTab == 0) {
        for (int i = 0; i < 10; i++) if (inR(x, y, 86 + i * 54, 220, 40, 54)) { t.water = (t.water == i + 1) ? i : i + 1; hlDirty = true; sndPlay(SND_CLICK); return; }
        if (inR(x, y, 380, 350, 76, 46)) t.steps = std::max(0, t.steps - 1000); else if (inR(x, y, 466, 350, 70, 46)) t.steps += 500; else if (inR(x, y, 546, 350, 70, 46)) t.steps += 1000;
        else if (inR(x, y, 86, 596, 70, 44)) t.sleep10 = std::max(0, t.sleep10 - 5); else if (inR(x, y, 166, 596, 70, 44)) t.sleep10 = std::min(160, t.sleep10 + 5);
        else if (inR(x, y, 1000, 196, 70, 46)) { float w = t.weight10 > 0 ? t.weight10 : hlLastWeight() * 10; t.weight10 = std::max(200, (int)w - 1); } else if (inR(x, y, 1080, 196, 70, 46)) { float w = t.weight10 > 0 ? t.weight10 : (hlLastWeight() > 0 ? hlLastWeight() * 10 : 700); t.weight10 = std::min(3000, (int)w + 1); }
        else { for (int i = 0; i < 5; i++) if (inR(x, y, 372 + i * 50, 550, 40, 40)) t.mood = (t.mood == i + 1) ? 0 : i + 1; }
        hlDirty = true;
    } else if (hlTab == 1) {
        if (hlWo < 0) { if (y >= 176) for (int i = 0; i < HL_NW; i++) if (inR(x, y, 70 + (i % 3) * 390, 176 + (i / 3) * 150 - hlScroll, 376, 138)) { hlWo = i; return; } }
        else { if (inR(x, y, 660, 200, 240, 60)) hlStartWo(hlWo); else if (inR(x, y, 920, 200, 200, 60)) hlWo = -1; }
    } else if (hlTab == 2) {
        int d = hlDay();
        if (inR(x, y, 1090, 180, 56, 40)) { hlGoalCal = std::max(1000, hlGoalCal - 100); hlDirty = true; return; } if (inR(x, y, 1154, 180, 56, 40)) { hlGoalCal = std::min(5000, hlGoalCal + 100); hlDirty = true; return; }
        if (x < 620 && y >= 228) { auto L = hlFoodList(); for (size_t i = 0; i < L.size(); i++) if (inR(x, y, 70, 228 + (int)i * 48 - hlScroll, 540, 44)) { bool f = false; for (auto& e : hlDiary) if (e.day == d && e.food == L[i]) { e.qty++; f = true; break; } if (!f) hlDiary.push_back({d, L[i], 1}); hlDirty = true; sndPlay(SND_CLICK); return; } }
        else if (x >= 1180 && y >= 318) { int n = 0, yy = 318; for (int i = (int)hlDiary.size() - 1; i >= 0 && n < 7; i--) if (hlDiary[i].day == d) { if (inR(x, y, 1180, yy - 6, 50, 40)) { if (--hlDiary[i].qty <= 0) hlDiary.erase(hlDiary.begin() + i); hlDirty = true; return; } yy += 40; n++; } }
    } else if (hlTab == 3) {
        float w = hlLastWeight();
        if (inR(x, y, 86, 236, 52, 44)) hlHeight = std::max(100, hlHeight - 1); else if (inR(x, y, 294, 236, 52, 44)) hlHeight = std::min(230, hlHeight + 1);
        else if (inR(x, y, 666, 236, 52, 44)) hlAge = std::max(5, hlAge - 1); else if (inR(x, y, 874, 236, 52, 44)) hlAge = std::min(100, hlAge + 1);
        else if (inR(x, y, 1000, 236, 100, 44)) hlSex = !hlSex; else if (inR(x, y, 1110, 236, 100, 44)) hlAct = (hlAct + 1) % 4;
        else if (w > 0 && inR(x, y, 1050, 346, 160, 40)) { double bmr = 10 * w + 6.25 * hlHeight - 5 * hlAge + (hlSex ? -161 : 5); static const double M[4] = {1.2, 1.375, 1.55, 1.725}; hlGoalCal = (int)(bmr * M[hlAct] / 50) * 50; toast("Calorie goal set"); }
        else if (w > 0 && inR(x, y, 666, 580, 200, 44)) { hlGoalWater = std::max(4, std::min(10, (int)(w * 0.033f * 4 + 0.5f))); toast("Water goal set"); }
        hlDirty = true;
    } else {
        if (hlTipSel >= 0) { hlTipSel = -1; return; } for (int i = 0; i < HL_NT; i++) if (inR(x, y, 70 + (i % 2) * 590, 172 + (i / 2) * 96, 570, 86)) { hlTipSel = i; return; }
    }
}
static void hlDrag(int, int dy) { hlScroll = std::max(0, hlScroll - dy); }
