// win81.h - v0.9 additions, included from main.cpp (not a standalone header)
// Cortana, Skype, OneDrive, Task Manager, Desktop, Help+Tips, sound, installed games, Home menu manager
// Everything here is plain static code that shares main.cpp's helpers (rect, text, btn, openKbStr ...)

static int skyUnread();                                  // Skype section below
static void gotoApp(int app); static long long fileSizeOf(const std::string& p); static bool copyFile(const std::string& from, const std::string& to);
static void tmSample();                                  // Task Manager section below
static float gTmCpu = 0, gTmMemUsedMB = 0, gTmMemTotalMB = 0;
static float gFrameBusyMs = 0;                           // measured by the main loop: how long a frame's work took

// ---------------------------------------------------------------- small drawing helpers used by several apps
static void w9Rrect(int x, int y, int w, int h, Col c, int a = 255) { rect(x, y, w, h, c, a); }
static std::string w9Clean(std::string s) { for (auto& c : s) if (c == '\t' || c == '\n' || c == '\r') c = ' '; return s; }
static std::string w9Esc(const std::string& s) { std::string o; for (char c : s) { if (c == '\\') o += "\\\\"; else if (c == '\n') o += "\\n"; else if (c == '\t') o += "\\t"; else if (c == '\r') {} else o += c; } return o; }
static std::string w9Unesc(const std::string& s) { std::string o; for (size_t i = 0; i < s.size(); i++) { if (s[i] == '\\' && i + 1 < s.size()) { i++; o += s[i] == 'n' ? '\n' : (s[i] == 't' ? '\t' : s[i]); } else o += s[i]; } return o; }
static std::vector<std::string> w9Split(const std::string& l, char sep) { std::vector<std::string> f; size_t a = 0; while (true) { size_t b = l.find(sep, a); if (b == std::string::npos) { f.push_back(l.substr(a)); break; } f.push_back(l.substr(a, b - a)); a = b + 1; } return f; }
static void w9Toggle(int x, int y, bool on, Col accent) {
    rect(x, y, 90, 34, on ? accent : Col{200, 200, 205});
    rect(x + (on ? 52 : 4), y + 4, 34, 26, WHITE);
    text(on ? "On" : "Off", F16, x + (on ? 22 : 66), y + 6, on ? WHITE : Col{90, 90, 95}, 1);
}
static void w9Mic(int cx, int cy, Col c) {
    rect(cx - 6, cy - 16, 12, 20, c); disc(cx - 0, cy - 16, 6, c); disc(cx, cy + 4, 6, c);
    rect(cx - 12, cy - 4, 3, 12, c); rect(cx + 9, cy - 4, 3, 12, c); rect(cx - 12, cy + 8, 24, 3, c);
    rect(cx - 12, cy + 4, 3, 5, c); rect(cx + 9, cy + 4, 3, 5, c);
    rect(cx - 2, cy + 11, 4, 8, c); rect(cx - 9, cy + 18, 18, 3, c);
}
static void w9Burger(int cx, int cy, Col c) { for (int i = -1; i <= 1; i++) rect(cx - 12, cy + i * 8 - 2, 24, 4, c); }
static void w9Note(int cx, int cy, Col c) { disc(cx - 6, cy + 10, 6, c); rect(cx, cy - 14, 3, 24, c); quad(cx, cy - 14, cx + 12, cy - 8, cx + 12, cy - 2, cx, cy - 8, c); }
static void w9Glyph(int kind, int cx, int cy, Col c, Col bg) {
    switch (kind) {
        case 1: outline(cx - 14, cy - 12, 28, 26, 3, c); rect(cx - 14, cy - 12, 28, 8, c); rect(cx - 7, cy + 1, 5, 5, c); rect(cx + 2, cy + 1, 5, 5, c); rect(cx - 7, cy + 8, 5, 3, c); break;   // calendar
        case 2: disc(cx, cy, 10, c); for (int k = 0; k < 8; k++) { float a = k * 0.785398f; line(cx + (int)(sinf(a) * 14), cy - (int)(cosf(a) * 14), cx + (int)(sinf(a) * 20), cy - (int)(cosf(a) * 20), 3, c); } break; // sun
        case 3: ring(cx, cy, 17, 3, c, bg); line(cx, cy, cx, cy - 10, 3, c); line(cx, cy, cx + 8, cy + 3, 3, c); break;   // clock
        case 4: outline(cx - 16, cy - 9, 30, 18, 3, c); rect(cx + 14, cy - 4, 4, 8, c); rect(cx - 12, cy - 5, 18, 10, c); break; // battery
        case 5: for (int i = 0; i < 3; i++) rect(cx - 16, cy - 14 + i * 10, 32, 5, c); rect(cx - 16, cy + 14, 20, 3, c); break; // news
        case 6: disc(cx, cy, 18, c); text("S", F30, cx, cy - 20, bg, 1); break;                                              // skype
        case 7: rect(cx - 16, cy - 14, 32, 28, c); rect(cx - 16, cy - 14, 32, 5, bg, 90); break;                              // drive
        case 8: disc(cx - 8, cy + 2, 10, c); disc(cx + 8, cy + 4, 9, c); disc(cx, cy - 6, 11, c); rect(cx - 17, cy + 4, 34, 9, c); break; // cloud
        default: disc(cx, cy, 12, c); break;
    }
}

// ---------------------------------------------------------------- Cortana (rule-based assistant, works offline, Windows Phone 8.1 look)
struct CoMsg { bool me; std::string t; int card = 0; };
struct CoRem { long long due; std::string text; };
static std::vector<CoMsg> coMsgs; static std::vector<CoRem> coRems; static std::string coIn, coPending; static int coScroll = 0; static bool coLoaded = false; static float coPulse = 0;
static int coView = 0, coSum = 0, coSumMax = 0, coCardNext = 0; static bool coMenu = false; static Uint32 coListenT = 0, coThinkT = 0;
static const Col CO_BLUE = {27, 161, 226};
static long long coNow() { return wcDays(gNow.y, gNow.mo, gNow.d) * 86400LL + gNow.h * 3600 + gNow.mi * 60 + gNow.s; }
static void coSaveRem() { std::string s; for (auto& r : coRems) s += std::to_string(r.due) + "|" + r.text + "\n"; writeFile(DIR_ + "cortana.txt", s); }
static void coEnsure() {
    if (coLoaded) return; coLoaded = true;
    std::string d; if (!readFile(DIR_ + "cortana.txt", d)) return;
    size_t p = 0; while (p < d.size()) { size_t e = d.find('\n', p); if (e == std::string::npos) e = d.size(); std::string l = d.substr(p, e - p); p = e + 1; size_t b = l.find('|'); if (b != std::string::npos) coRems.push_back({atoll(l.substr(0, b).c_str()), l.substr(b + 1)}); }
}
static void coTick() {
    coEnsure(); if (coRems.empty()) return;
    long long n = coNow(); bool ch = false;
    for (size_t i = 0; i < coRems.size();) { if (coRems[i].due <= n) { notify(A_CORTANA, "Reminder", coRems[i].text); toast("Reminder: " + coRems[i].text); coRems.erase(coRems.begin() + i); ch = true; } else i++; }
    if (ch) coSaveRem();
}
static std::string coLow(std::string s) { for (auto& c : s) c = (char)tolower((unsigned char)c); return s; }
static bool coHas(const std::string& q, const char* w) { return q.find(w) != std::string::npos; }
static bool coStarts(const std::string& q, const char* w) { return q.compare(0, strlen(w), w) == 0; }
static std::string coAfter(const std::string& q, const char* w) { size_t p = q.find(w); if (p == std::string::npos) return ""; std::string r = q.substr(p + strlen(w)); while (!r.empty() && r[0] == ' ') r.erase(0, 1); while (!r.empty() && (r.back() == ' ' || r.back() == '?' || r.back() == '.')) r.pop_back(); return r; }
static void coLaunch(int a) { if (a == curApp) return; pendingApp = a; closeApp(false); }
static std::string coTimeStr() { char b[32]; if (g24) snprintf(b, sizeof b, "%02d:%02d", gNow.h, gNow.mi); else snprintf(b, sizeof b, "%d:%02d %s", gNow.h % 12 ? gNow.h % 12 : 12, gNow.mi, gNow.h < 12 ? "AM" : "PM"); return b; }
static std::string coIn_(long long secs) { char b[64]; if (secs >= 3600) snprintf(b, sizeof b, "%.3g hours", secs / 3600.0); else if (secs >= 60) snprintf(b, sizeof b, "%lld minutes", (secs + 30) / 60); else snprintf(b, sizeof b, "%lld seconds", secs); return b; }
static std::string coDayPart() { return gNow.h < 5 ? "Good evening" : (gNow.h < 12 ? "Good morning" : (gNow.h < 18 ? "Good afternoon" : "Good evening")); }

// unit conversion: "convert 5 km to miles", "10 pounds in kg", "72 f to c"
struct CoUnit { const char* n; int cat; double f; };    // cat: 0 length (m), 1 mass (kg), 2 volume (l), 3 temperature, 4 speed (m/s), 5 data (B)
static const CoUnit CO_UNITS[] = {
    {"km", 0, 1000}, {"kilometers", 0, 1000}, {"kilometres", 0, 1000}, {"kilometer", 0, 1000}, {"m", 0, 1}, {"meters", 0, 1}, {"metres", 0, 1}, {"meter", 0, 1}, {"cm", 0, 0.01}, {"centimeters", 0, 0.01}, {"mm", 0, 0.001},
    {"mi", 0, 1609.344}, {"miles", 0, 1609.344}, {"mile", 0, 1609.344}, {"ft", 0, 0.3048}, {"feet", 0, 0.3048}, {"foot", 0, 0.3048}, {"in", 0, 0.0254}, {"inches", 0, 0.0254}, {"inch", 0, 0.0254}, {"yd", 0, 0.9144}, {"yards", 0, 0.9144},
    {"kg", 1, 1}, {"kilograms", 1, 1}, {"kilos", 1, 1}, {"g", 1, 0.001}, {"grams", 1, 0.001}, {"lb", 1, 0.45359237}, {"lbs", 1, 0.45359237}, {"pounds", 1, 0.45359237}, {"pound", 1, 0.45359237}, {"oz", 1, 0.0283495}, {"ounces", 1, 0.0283495},
    {"l", 2, 1}, {"liters", 2, 1}, {"litres", 2, 1}, {"liter", 2, 1}, {"ml", 2, 0.001}, {"gal", 2, 3.78541}, {"gallons", 2, 3.78541}, {"cups", 2, 0.236588}, {"cup", 2, 0.236588},
    {"c", 3, 1}, {"celsius", 3, 1}, {"f", 3, 2}, {"fahrenheit", 3, 2}, {"k", 3, 3}, {"kelvin", 3, 3},
    {"kmh", 4, 0.277778}, {"km/h", 4, 0.277778}, {"mph", 4, 0.44704}, {"m/s", 4, 1},
    {"kb", 5, 1000}, {"mb", 5, 1e6}, {"gb", 5, 1e9}, {"tb", 5, 1e12},
};
static const CoUnit* coUnit(const std::string& n) { for (auto& u : CO_UNITS) if (n == u.n) return &u; return nullptr; }
static bool coConvert(const std::string& q, std::string& out) {
    size_t to = q.find(" to "); size_t tl = 4; if (to == std::string::npos) { to = q.find(" in "); }
    if (to == std::string::npos) return false;
    std::string a = q.substr(0, to), b = q.substr(to + tl);
    for (const char* pre : {"convert ", "what is ", "what's ", "how many ", "how much is "}) if (coStarts(a, pre)) { a.erase(0, strlen(pre)); break; }
    while (!b.empty() && (b.back() == ' ' || b.back() == '?' || b.back() == '.')) b.pop_back();
    size_t i = 0; while (i < a.size() && (isdigit((unsigned char)a[i]) || a[i] == '.' || a[i] == '-')) i++;
    if (i == 0) return false;
    double v = atof(a.substr(0, i).c_str()); std::string ua = a.substr(i); while (!ua.empty() && ua[0] == ' ') ua.erase(0, 1);
    const CoUnit* x = coUnit(ua); const CoUnit* y = coUnit(b);
    if (!x || !y || x->cat != y->cat) return false;
    double r;
    if (x->cat == 3) { double c = x->f == 1 ? v : (x->f == 2 ? (v - 32) * 5 / 9 : v - 273.15); r = y->f == 1 ? c : (y->f == 2 ? c * 9 / 5 + 32 : c + 273.15); }
    else r = v * x->f / y->f;
    char buf[96]; snprintf(buf, sizeof buf, "%.6g %s = %.6g %s", v, ua.c_str(), r, b.c_str()); out = buf; return true;
}
// alarms: "alarm for 7:30 am", "wake me up at 6 pm"
static bool coParseClock(const std::string& q, int& hh, int& mm) {
    size_t p = std::string::npos; for (const char* k : {" for ", " at "}) { size_t f = q.rfind(k); if (f != std::string::npos) { p = f + strlen(k); break; } }
    if (p == std::string::npos) return false;
    const char* s = q.c_str() + p; if (!isdigit((unsigned char)*s)) return false;
    hh = atoi(s); mm = 0; while (isdigit((unsigned char)*s)) s++;
    if (*s == ':' || *s == '.') { s++; mm = atoi(s); while (isdigit((unsigned char)*s)) s++; }
    std::string rest = s; if (coHas(rest, "pm") || coHas(rest, "p.m")) { if (hh < 12) hh += 12; } else if ((coHas(rest, "am") || coHas(rest, "a.m")) && hh == 12) hh = 0;
    return hh >= 0 && hh < 24 && mm >= 0 && mm < 60;
}
static const char* CO_FACTS[] = {"Honey never spoils. Archaeologists have eaten 3,000 year old honey.", "A day on Venus is longer than its year.", "Octopuses have three hearts.", "The first computer mouse was made of wood.", "Bananas are berries, but strawberries are not.", "The Nintendo Switch was released on 3 March 2017.", "There are more possible chess games than atoms in the observable universe.", "Windows 8.1 was released on 17 October 2013."};
static const char* CO_JOKES[] = {"Why do programmers prefer dark mode? Because light attracts bugs.", "I would tell you a UDP joke, but you might not get it.", "There are 10 kinds of people: those who understand binary and those who don't.", "A SQL query walks into a bar, sees two tables and asks: can I join you?", "Why did the Switch go to therapy? Too many unresolved docks.", "My Wi-Fi and I have a lot in common. We both drop out when it matters.", "I asked the Start screen for a day off. It said it had no tiles to spare."};

static std::string coReply(const std::string& raw) {
    std::string q = coLow(raw); coCardNext = 0;
    while (!q.empty() && (q.back() == '?' || q.back() == '!' || q.back() == '.' || q.back() == ' ')) q.pop_back();
    for (const char* pre : {"hey cortana ", "hey cortana, ", "hey cortana", "cortana, ", "cortana ", "please ", "can you ", "could you ", "i want to ", "i'd like to ", "i would like to "}) if (coStarts(q, pre)) q.erase(0, strlen(pre));
    while (!q.empty() && (q[0] == ' ' || q[0] == ',')) q.erase(0, 1);
    if (!coPending.empty() && (q == "yes" || q == "yeah" || q == "ok" || q == "sure" || q == "yep" || q == "please do")) { std::string s = coPending; coPending.clear(); gUrl = s; pendingApp = A_BROWSER; browserGo(); return "Searching the web for \"" + s + "\"."; }
    coPending.clear();
    if (q.empty()) return "I'm listening.";
    if (q == "hi" || q == "hello" || q == "hey" || coStarts(q, "good morning") || coStarts(q, "good evening") || coStarts(q, "good afternoon")) return coDayPart() + ", " + gUser + ".";
    if (coHas(q, "how are you") || coHas(q, "how's it going")) return "Running at full speed on a Tegra. How about you?";
    if (coHas(q, "who are you") || coHas(q, "your name") || coHas(q, "who made you")) return "I'm Cortana, your personal assistant in Windows 8.1 on this Switch.";
    if (coHas(q, "thank")) return "Anytime.";
    if (coHas(q, "i love you")) return "That's sweet. I'll keep the Start screen warm for you.";
    if (coHas(q, "what can you do") || q == "help" || coHas(q, "see more")) return "Try: open Skype, what's the weather tomorrow, remind me to call mum in 20 minutes, set an alarm for 7:30 am, set a timer for 5 minutes, convert 5 km to miles, 12*(3+4), define serendipity, search for pizza, take a note buy milk, flip a coin, volume up, how's my CPU, use the sunset theme, show my reminders, tell me a joke.";
    if (coHas(q, "joke")) return CO_JOKES[rand() % 7];
    if (coHas(q, "fun fact") || coHas(q, "tell me something") || q == "fact" || coHas(q, "trivia")) return CO_FACTS[rand() % 8];
    if (coHas(q, "flip a coin") || coHas(q, "toss a coin") || coHas(q, "heads or tails")) return (rand() & 1) ? "Heads." : "Tails.";
    if (coHas(q, "roll a die") || coHas(q, "roll a dice") || coHas(q, "roll the dice") || coHas(q, "roll dice")) return "You rolled a " + std::to_string(1 + rand() % 6) + ".";
    if (coHas(q, "random number") || coHas(q, "pick a number")) { int a = 1, b = 100; const char* s = q.c_str(); while (*s && !isdigit((unsigned char)*s)) s++; if (*s) { a = atoi(s); while (isdigit((unsigned char)*s)) s++; while (*s && !isdigit((unsigned char)*s)) s++; if (*s) b = atoi(s); else { b = a; a = 1; } } if (b < a) std::swap(a, b); return "How about " + std::to_string(a + rand() % (b - a + 1)) + "."; }
    if (coHas(q, "show my reminders") || coHas(q, "my reminders") || coHas(q, "list reminders") || q == "reminders") { coView = 1; coMenu = false; return coRems.empty() ? "You have no reminders." : "Here are your " + std::to_string(coRems.size()) + " reminders."; }
    if (coStarts(q, "take a note") || coStarts(q, "note ") || coStarts(q, "write down") || coStarts(q, "note that") || coStarts(q, "remember that")) {
        std::string n = coAfter(q, ": "); if (n.empty()) n = coAfter(q, "take a note "); if (n.empty()) n = coAfter(q, "write down "); if (n.empty()) n = coAfter(q, "note that "); if (n.empty()) n = coAfter(q, "remember that "); if (n.empty()) n = coAfter(q, "note ");
        if (n.empty()) return "What should I write down?";
        std::string old; readFile(DIR_ + "notes.txt", old); if (!old.empty() && old.back() != '\n') old += "\n"; writeFile(DIR_ + "notes.txt", old + n + "\n");
        if (running[A_NOTES]) { gNote.s = old + n + "\n"; }
        return "Noted: \"" + n + "\". It's in Notes.";
    }
    if (coHas(q, "phone mode")) return "Windows Phone mode has been retired. This shell is Windows 8.1 only now.";
    if ((coHas(q, "volume") || coHas(q, "louder") || coHas(q, "quieter") || coHas(q, "mute")) && !coHas(q, "music")) {
        if (coHas(q, "mute") || coHas(q, "off")) gSndVol = 0; else if (coHas(q, "up") || coHas(q, "louder") || coHas(q, "increase") || coHas(q, "raise")) gSndVol = std::min(10, gSndVol + 2); else if (coHas(q, "down") || coHas(q, "quieter") || coHas(q, "lower") || coHas(q, "decrease")) gSndVol = std::max(0, gSndVol - 2); else return "System sound volume is " + std::to_string(gSndVol * 10) + "%.";
        win9Save(); sndPlay(SND_NOTIFY); return "System sound volume is " + std::to_string(gSndVol * 10) + "%.";
    }
    if ((coHas(q, "alarm") || coHas(q, "wake me")) ) {
        int hh, mm; if (!coParseClock(q, hh, mm)) return "For what time? Try: set an alarm for 7:30 am.";
        long long n = coNow(), sod = n % 86400, due = n - sod + hh * 3600 + mm * 60; if (due <= n) due += 86400;
        coRems.push_back({due, "Alarm"}); coSaveRem();
        char b[64]; snprintf(b, sizeof b, "Alarm set for %02d:%02d, in %s.", hh, mm, coIn_(due - n).c_str()); return b;
    }
    if (coStarts(q, "remind me") || (coStarts(q, "set a ") && coHas(q, "timer")) || coStarts(q, "set a timer") || coStarts(q, "set timer") || coStarts(q, "timer for") || coStarts(q, "timer ")) {
        bool timer = !coStarts(q, "remind me"); long long secs = 0; std::string what;
        size_t ip = q.find(" in "); std::string num = timer ? q : (ip != std::string::npos ? q.substr(ip + 4) : "");
        double v = 0; const char* np = nullptr; for (size_t i = 0; i < num.size(); i++) if (isdigit((unsigned char)num[i])) { np = num.c_str() + i; break; }
        if (np) { v = atof(np); std::string rest = np; if (rest.find("hour") != std::string::npos || rest.find(" h") != std::string::npos) secs = (long long)(v * 3600); else if (rest.find("sec") != std::string::npos) secs = (long long)v; else secs = (long long)(v * 60); }
        if (secs <= 0) return timer ? "For how long? Try: set a timer for 5 minutes." : "When? Try: remind me to call mum in 20 minutes.";
        if (timer) what = "Timer finished"; else { what = coAfter(q, "remind me to "); if (what.empty()) what = coAfter(q, "remind me "); size_t k = what.rfind(" in "); if (k != std::string::npos) what = what.substr(0, k); if (what.empty()) what = "Reminder"; }
        coRems.push_back({coNow() + secs, what}); coSaveRem();
        return std::string(timer ? "Timer set for " : "OK, I'll remind you in ") + coIn_(secs) + ".";
    }
    { std::string cv; if (coHas(q, " to ") || coHas(q, " in ")) if (coConvert(q, cv)) return cv; }
    if (coStarts(q, "open ") || coStarts(q, "launch ") || coStarts(q, "start ") || coStarts(q, "go to ") || coStarts(q, "run ") || coStarts(q, "play ") || coStarts(q, "show me ")) {
        std::string n = coAfter(q, " "); if (coStarts(n, "my ")) n.erase(0, 3); if (coStarts(n, "the ")) n.erase(0, 4); int a = tFindApp(n);
        if (n == "music" || n == "songs") a = A_MUSIC; else if (n == "radio") a = A_RADIO; else if (n == "podcast" || n == "podcasts") a = A_PODCASTS; else if (n == "files" || n == "documents" || n == "file explorer" || n == "this pc") a = A_FILES; else if (n == "settings" || n == "pc settings" || n == "control panel") a = A_SETTINGS; else if (n == "desktop") a = A_DESKTOP; else if (n == "task manager" || n == "performance") a = A_TASKMGR; else if (n == "skype" || n == "messages") a = A_SKYPE; else if (n == "onedrive" || n == "skydrive" || n == "cloud") a = A_ONEDRIVE; else if (n == "help" || n == "tips") a = A_HELP;
        if (a >= 0 && appAvail(a)) { coLaunch(a); return std::string("Opening ") + ADEF[a].name + "."; }
        if (a >= 0) return std::string(ADEF[a].name) + " isn't installed yet. You can get it from the Store.";
        return "I couldn't find an app called \"" + n + "\".";
    }
    if (coStarts(q, "call ") || coStarts(q, "skype ") || coStarts(q, "message ") || coStarts(q, "text ")) { coLaunch(A_SKYPE); return "Opening Skype."; }
    if (coHas(q, "cpu") || coHas(q, "memory") || coHas(q, "ram") || coHas(q, "performance") || coHas(q, "how's my") || coHas(q, "how is my")) {
        tmSample(); coCardNext = 3; char b[160]; snprintf(b, sizeof b, "CPU load is about %.0f%%. The app is using %.0f MB of %.0f MB.", gTmCpu, gTmMemUsedMB, gTmMemTotalMB); return b;
    }
    if (coHas(q, "time")) return "It's " + coTimeStr() + ".";
    if (coHas(q, "date") || coHas(q, "what day") || coHas(q, "today")) { if (!coHas(q, "calendar") && !coHas(q, "agenda") && !coHas(q, "schedule")) { char b[96]; snprintf(b, sizeof b, "%s, %d %s %d.", WD[dow(gNow.y, gNow.mo, gNow.d)], gNow.d, MONTHS[gNow.mo - 1], gNow.y); return b; } }
    if (coHas(q, "weather") || coHas(q, "temperature") || coHas(q, "rain") || coHas(q, "forecast") || coHas(q, "umbrella") || coHas(q, "sunny") || coHas(q, "cold") || coHas(q, "hot outside")) {
        if (!gWx.have) { coLaunch(A_WEATHER); return "I don't have the weather yet. Opening Weather."; }
        char b[200]; auto tv = [&](float c) { return gTempF ? c * 9 / 5 + 32 : c; };
        if ((coHas(q, "tomorrow") || coHas(q, "forecast")) && gWx.days.size() >= 2) { coCardNext = 1; const WxDay& d = gWx.days[1]; snprintf(b, sizeof b, "Tomorrow: %s, high %.0f, low %.0f, %d%% chance of rain.", wxText(d.code), tv(d.hi), tv(d.lo), d.pop); return b; }
        coCardNext = 1; snprintf(b, sizeof b, "Right now in %s: %s, %.0f degrees, wind %.0f km/h.", gWxCity.empty() ? "your area" : gWxCity.c_str(), wxText(gWx.code), tv(gWx.temp), gWx.wind); return b;
    }
    if (coHas(q, "calendar") || coHas(q, "agenda") || coHas(q, "schedule") || coHas(q, "next event") || coHas(q, "what's on") || coHas(q, "whats on") || coHas(q, "meeting")) { std::string l = calTileLine(); return l.empty() ? "Nothing coming up on your calendar." : "Next up: " + l + "."; }
    if (coHas(q, "news") || coHas(q, "headline")) { if (!gNews.empty()) return "Top story: " + gNews[0].title; coLaunch(A_NEWS); return "Opening News."; }
    if (coHas(q, "battery")) return "Battery is at " + std::to_string((int)gBat) + "%.";
    if (coHas(q, "wifi") || coHas(q, "wi-fi") || coHas(q, "internet") || coHas(q, "network")) return gOffline ? "Offline mode is on." : (netUp() ? "You're online." : "You're not connected to Wi-Fi.");
    if (coHas(q, "storage") || coHas(q, "disk space") || coHas(q, "sd card")) { fmStorage(); return "SD card: " + fmSize(fmFree) + " free of " + fmSize(fmTotalB) + "."; }
    if (coHas(q, "offline mode") || coHas(q, "airplane")) { gOffline = coHas(q, " on") || coHas(q, "enable") || coHas(q, "turn on") ? true : (coHas(q, " off") || coHas(q, "disable") ? false : !gOffline); saveCfg(); return gOffline ? "Offline mode is on." : "Offline mode is off."; }
    if (q == "lock" || coHas(q, "lock the") || coHas(q, "lock screen") || coHas(q, "lock my")) { closeApp(false); uiState = 1; lockOff = 0; lockAnim = false; return "Locking."; }
    if (coHas(q, "shut down") || coHas(q, "shutdown") || coHas(q, "restart") || coHas(q, "power off")) { coLaunch(A_POWER); return "Opening Power."; }
    if (coHas(q, "theme")) {
        for (int i = 0; i < NSTH; i++) if (coHas(q, coLow(STHEMES[i].name).c_str())) { gThOwned |= 1u << i; thApply(i); return std::string("Switched to the ") + STHEMES[i].name + " theme."; }
        coLaunch(A_THEMES); return "Opening the Theme Store.";
    }
    if (coStarts(q, "define ") || coStarts(q, "what does ") || coStarts(q, "meaning of ") || coStarts(q, "what is the meaning of ")) {
        std::string w = coStarts(q, "define ") ? coAfter(q, "define ") : (coStarts(q, "what does ") ? coAfter(q, "what does ") : coAfter(q, "of "));
        size_t k = w.find(" mean"); if (k != std::string::npos) w = w.substr(0, k);
        if (!w.empty()) { dcWord = w; dcGo(); coLaunch(A_DICT); return "Looking up \"" + w + "\"."; }
    }
    if (coStarts(q, "search ") || coStarts(q, "google ") || coStarts(q, "look up ") || coStarts(q, "find ") || coStarts(q, "bing ")) {
        std::string w = coAfter(q, " "); if (coStarts(w, "for ")) w.erase(0, 4);
        if (!w.empty()) { gUrl = w; pendingApp = A_BROWSER; browserGo(); return "Searching the web for \"" + w + "\"."; }
    }
    { // maths
        std::string e = q; for (const char* pre : {"what is ", "what's ", "whats ", "calculate ", "how much is ", "compute "}) if (coStarts(e, pre)) { e.erase(0, strlen(pre)); break; }
        bool has = false, ok = true; for (char c : e) { if (isdigit((unsigned char)c)) has = true; if (!(isdigit((unsigned char)c) || strchr("+-*/^().% x", c) || isalpha((unsigned char)c))) ok = false; }
        for (size_t i = 0; i < e.size(); i++) if (e[i] == 'x' && i > 0 && isdigit((unsigned char)e[i - 1])) e[i] = '*';
        double v; if (has && ok && sciEval(e, v)) { char b[64]; snprintf(b, sizeof b, "%.10g", v); return std::string(e) + " = " + b; }
    }
    coPending = raw;
    return "I'm not sure about that one. Want me to search the web for it? Say yes, or try \"help\".";
}
static void coSend() {
    std::string q = coIn; coIn.clear(); while (!q.empty() && q.back() == ' ') q.pop_back(); if (q.empty()) return;
    coMsgs.push_back({true, q, 0}); coScroll = 0; coView = 0; coMenu = false;
    std::string r = coReply(q); coMsgs.push_back({false, r, coCardNext}); coCardNext = 0; coThinkT = SDL_GetTicks();
    if (coMsgs.size() > 80) coMsgs.erase(coMsgs.begin(), coMsgs.begin() + 2);
}
static void coSendEnter() { coSend(); openKbStr(&coIn, 120, coSendEnter); }
static void cortanaInit() { coEnsure(); coView = 0; coMenu = false; coSum = 0; coScroll = 0; }

// ---- layout shared by drawing and touch
struct CoLay { bool wide; int cx, ringY, ringR; SDL_Rect ask, mic, music, menu, sum, col; };
static CoLay coLayout(int cw, int ch) {
    CoLay L; L.wide = cw >= 900; bool chat = !coMsgs.empty() && coView == 0;
    int leftW = (L.wide && !chat && coView == 0) ? cw / 2 : cw;
    int colW = (chat || coView != 0) ? std::min(cw - 40, 760) : leftW - 80;
    L.col = SDL_Rect{(cw - colW) / 2 - (leftW < cw && !chat ? (cw - leftW) / 2 : 0), 0, colW, ch};
    if (leftW < cw && !chat) L.col.x = (leftW - colW) / 2;
    L.cx = L.col.x + colW / 2; L.ringR = chat ? 28 : 66; L.ringY = chat ? 52 : std::max(130, ch / 4 + 10);
    L.ask = SDL_Rect{L.col.x, ch - 78, colW, 56};
    L.mic = SDL_Rect{L.ask.x + L.ask.w - 56, L.ask.y, 56, 56};
    L.menu = SDL_Rect{cw - 72, 14, 56, 48}; L.music = SDL_Rect{cw - 140, 14, 56, 48};
    if (L.wide && !chat && coView == 0) L.sum = SDL_Rect{cw / 2 + 10, 84, cw / 2 - 50, ch - 84 - 20};
    else L.sum = SDL_Rect{L.col.x, 0, colW, 0};
    return L;
}
struct CoCard { int glyph; std::string t, s; int act; };
static std::vector<CoCard> coCards() {
    std::vector<CoCard> v;
    if (gCoInt & 1) { std::string l = calTileLine(); v.push_back({1, l.empty() ? "Nothing coming up" : l, "Next on your calendar", 1}); }
    if (gCoInt & 2) {
        if (gWx.have) { auto tv = [&](float c) { return gTempF ? c * 9 / 5 + 32 : c; }; char b[96]; snprintf(b, sizeof b, "%.0f\xC2\xB0 %s", tv(gWx.temp), wxText(gWx.code)); v.push_back({2, b, gWxCity.empty() ? "Weather" : gWxCity, 2}); }
        else v.push_back({2, "Weather", "Tap to load the forecast", 2});
    }
    if (gCoInt & 4) {
        if (!coRems.empty()) { size_t bi = 0; for (size_t i = 1; i < coRems.size(); i++) if (coRems[i].due < coRems[bi].due) bi = i; long long left = std::max(0LL, coRems[bi].due - coNow()); v.push_back({3, coRems[bi].text, "In " + coIn_(left), 3}); }
        else v.push_back({3, "No reminders", "Say \"remind me to ...\"", 3});
    }
    if (gCoInt & 8) { fmStorage(); v.push_back({4, "Battery " + std::to_string((int)gBat) + "%", fmTotalB > 0 ? fmSize(fmFree) + " free on the SD card" : "Power and storage", 4}); }
    if ((gCoInt & 16) && !gNews.empty()) v.push_back({5, gNews[0].title, "Top story", 5});
    if ((gCoInt & 32)) { int u = skyUnread(); v.push_back({6, u > 0 ? std::to_string(u) + (u == 1 ? " new Skype message" : " new Skype messages") : "Skype", u > 0 ? "Tap to read" : "You're all caught up", 6}); }
    return v;
}
static const int CO_CARD_H = 78;
static void coRing(float cx, float cy, float R, bool active) {
    float p = 0.5f + 0.5f * sinf(coPulse);
    ringA(cx, cy, R, R * 0.16f, CO_BLUE, 255);
    ringA(cx, cy, R * 0.78f + (active ? p * 3.0f : 0), R * 0.05f, CO_BLUE, 150);
    disc((int)cx, (int)cy, std::max(2, (int)(R * 0.12f)), Col{0, 0, 0}, 0);
    if (active) {
        float t = (SDL_GetTicks() % 1600) / 1600.0f;
        for (int k = 0; k < 3; k++) { float u = fmodf(t + k / 3.0f, 1.0f); ringA(cx, cy, R * (1.1f + u * 0.9f), 2.5f, CO_BLUE, (int)(200 * (1.0f - u))); }
    }
}
static void coDrawCard(int x, int y, int w, int glyph, const std::string& t, const std::string& s, bool boldTitle = true) {
    rect(x, y, 62, 62, CO_BLUE);
    w9Glyph(glyph, x + 31, y + 31, WHITE, CO_BLUE);
    text(fitTxt(t, boldTitle ? F30 : F22, w - 82), boldTitle ? F30 : F22, x + 80, y + (s.empty() ? 14 : 0), WHITE);
    if (!s.empty()) text(fitTxt(s, F16, w - 82), F16, x + 80, y + 40, Col{150, 160, 172});
}
static void drawCortanaAt(int cw, int ch) {
    coPulse += 0.045f; coEnsure();
    rect(0, 0, cw, ch, BLACK);
    CoLay L = coLayout(cw, ch); bool chat = !coMsgs.empty() && coView == 0;
    bool listening = coListenT && SDL_GetTicks() - coListenT < 4500; bool thinking = coThinkT && SDL_GetTicks() - coThinkT < 700;
    // top right: music note and menu
    w9Note(L.music.x + 28, L.music.y + 24, Col{190, 190, 196}); w9Burger(L.menu.x + 28, L.menu.y + 24, Col{190, 190, 196});
    if (coView == 1 || coView == 2) {
        text(coView == 1 ? "Reminders" : "Notebook", F44, 40, 22, CO_BLUE);
        text("Back", F22, cw - 170, 26, CO_BLUE, 2); rect(0, 0, 0, 0, BLACK);
        int y = 96;
        if (coView == 1) {
            if (coRems.empty()) text("You have no reminders. Try: remind me to stretch in 30 minutes.", F22, 40, y, Col{150, 160, 172});
            for (size_t i = 0; i < coRems.size() && y < ch - 90; i++) {
                rect(40, y, cw - 80, 70, Col{24, 24, 28});
                w9Glyph(3, 76, y + 35, CO_BLUE, Col{24, 24, 28});
                text(fitTxt(coRems[i].text, F22, cw - 340), F22, 116, y + 8, WHITE);
                text("in " + coIn_(std::max(0LL, coRems[i].due - coNow())), F16, 116, y + 40, Col{150, 160, 172});
                text("Delete", F22, cw - 60, y + 20, CO_BLUE, 2);
                y += 80;
            }
        } else {
            text("About me", F30, 40, y, WHITE); text(gUser, F22, 40, y + 42, Col{150, 160, 172});
            text("Interests (what Cortana shows in your summaries)", F22, 40, y + 94, WHITE);
            static const char* NI[6] = {"Calendar", "Weather", "Reminders", "Battery and storage", "News", "Skype"};
            for (int i = 0; i < 6; i++) { int yy = y + 140 + i * 56; text(NI[i], F22, 60, yy + 4, WHITE); w9Toggle(cw - 160, yy, (gCoInt >> i) & 1, CO_BLUE); }
        }
    } else if (!chat) {
        float R = (float)L.ringR; coRing((float)L.cx, (float)L.ringY, R, listening || thinking);
        std::string g = listening ? "Listening..." : ("Hi " + gUser + "! How can I help you?");
        auto lines = wrapText(g, F44, L.col.w);
        int gy = L.ringY + (int)R + 40;
        for (size_t i = 0; i < lines.size() && i < 3; i++) text(lines[i], F44, L.cx, gy + (int)i * 54, CO_BLUE, 1);
        int hy = gy + (int)std::min<size_t>(lines.size(), 3) * 54 + 24;
        if (listening) text("Voice recognition isn't available on Switch yet - type your question.", F16, L.cx, hy, Col{150, 160, 172}, 1);
        else { text("try what's the forecast?", F16, L.cx, hy, Col{150, 160, 172}, 1); text("see more", F16, L.cx, hy + 26, CO_BLUE, 1); }
        if (L.wide && gCoSumm) {
            std::vector<CoCard> cs = coCards();
            int top = L.sum.y, bot = L.sum.y + L.sum.h;
            std::string hd = std::string(coDayPart()) + ", " + gUser + ".";
            int total = 46 + (int)cs.size() * CO_CARD_H + 60; coSumMax = std::max(0, total - L.sum.h); coSum = std::max(0, std::min(coSum, coSumMax));
            SDL_Rect clip = L.sum; SDL_RenderSetClipRect(gR, &clip);
            int y = top - coSum;
            text("Here's what's coming up.", F30, L.sum.x, y, CO_BLUE); y += 46;
            for (auto& c : cs) { if (y + CO_CARD_H > top && y < bot) coDrawCard(L.sum.x, y, L.sum.w, c.glyph, c.t, c.s); y += CO_CARD_H; }
            text("Don't show me Summaries", F22, L.sum.x, y + 8, CO_BLUE);
            SDL_RenderSetClipRect(gR, nullptr);
            (void)hd;
        } else if (L.wide) {
            text("Summaries are off", F30, cw * 3 / 4, 200, Col{150, 160, 172}, 1);
            text("Show me Summaries", F22, cw * 3 / 4, 250, CO_BLUE, 1);
        }
    } else {
        coRing((float)(L.col.x + 36), 50, 24, listening || thinking); text("Cortana", F22, L.col.x + 76, 34, CO_BLUE);
        text("Home", F22, cw - 170, 26, CO_BLUE, 2);
        int top = 96, bot = L.ask.y - 14, w = L.col.w;
        SDL_Rect clip{0, top, cw, bot - top}; SDL_RenderSetClipRect(gR, &clip);
        int y = bot + coScroll;
        for (int i = (int)coMsgs.size() - 1; i >= 0; i--) {
            const CoMsg& m = coMsgs[i];
            int fnt = m.me ? F22 : F30; auto lines = wrapText(m.t, fnt, w - 20);
            int lh = m.me ? 30 : 40; int h = (int)lines.size() * lh + 14;
            int cardH = 0; if (!m.me && m.card) cardH = m.card == 1 ? (gWx.days.size() >= 3 ? 96 : 0) : (m.card == 3 ? 96 : 0);
            y -= h + cardH + 14;
            if (y + h + cardH >= top && y <= bot) {
                for (size_t k = 0; k < lines.size(); k++) text(lines[k], fnt, m.me ? L.col.x + w - 6 : L.col.x + 6, y + 6 + (int)k * lh, m.me ? Col{190, 190, 196} : CO_BLUE, m.me ? 2 : 0);
                if (cardH) {
                    int cy = y + h + 4;
                    if (m.card == 1) {
                        for (int d = 0; d < 3 && d < (int)gWx.days.size(); d++) { int cx2 = L.col.x + 6 + d * (w / 3); text(d == 0 ? "Today" : wxDayName(gWx.days[d].date, d), F16, cx2, cy, Col{150, 160, 172}); text(wxTemp(gWx.days[d].hi) + " / " + wxTemp(gWx.days[d].lo), F22, cx2, cy + 24, WHITE); text(fitTxt(wxText(gWx.days[d].code), F16, w / 3 - 12), F16, cx2, cy + 56, CO_BLUE); }
                    } else if (m.card == 3) {
                        float fr[2] = {gTmCpu / 100.0f, gTmMemTotalMB > 0 ? gTmMemUsedMB / gTmMemTotalMB : 0}; const char* nm[2] = {"CPU", "Memory"};
                        for (int k = 0; k < 2; k++) { int bx = L.col.x + 6 + k * (w / 2); text(nm[k], F16, bx, cy, Col{150, 160, 172}); rect(bx, cy + 30, w / 2 - 24, 14, Col{40, 40, 46}); rect(bx, cy + 30, (int)((w / 2 - 24) * clampf(fr[k], 0, 1)), 14, CO_BLUE); text(std::to_string((int)(fr[k] * 100)) + "%", F22, bx, cy + 50, WHITE); }
                    }
                }
            }
        }
        SDL_RenderSetClipRect(gR, nullptr);
    }
    // the "ask me anything" bar
    if (coView == 0) {
        rect(L.ask.x, L.ask.y, L.ask.w, L.ask.h, Col{224, 224, 226});
        rect(L.mic.x, L.mic.y, L.mic.w, L.mic.h, listening ? CO_BLUE : Col{190, 190, 194});
        w9Mic(L.mic.x + 28, L.mic.y + 26, listening ? WHITE : Col{60, 60, 66});
        std::string t = coIn; if (kbOpen && kbTarget == 99 && gKbStr == &coIn && (SDL_GetTicks() / 500) % 2 == 0) t += "|";
        if (coIn.empty() && !(kbOpen && kbTarget == 99 && gKbStr == &coIn)) text("ask me anything", F22, L.ask.x + 16, L.ask.y + 12, Col{110, 110, 116});
        else { while (t.size() > 2 && tw(t, F22) > L.ask.w - 100) t.erase(0, 1); text(t, F22, L.ask.x + 16, L.ask.y + 12, Col{30, 30, 34}); }
    }
    // the menu
    if (coMenu) {
        int mw = 300, mx = cw - mw;
        rect(mx, 0, mw, ch, Col{20, 20, 24}, 250);
        static const char* MI[4] = {"home", "reminders", "notebook", ""};
        for (int i = 0; i < 4; i++) { std::string l = i < 3 ? MI[i] : (gCoSumm ? "hide summaries" : "show summaries"); text(l, F30, mx + 30, 100 + i * 70, i == 0 && coView == 0 ? CO_BLUE : WHITE); }
        text("Cortana", F22, mx + 30, 30, CO_BLUE);
    }
}
static void drawCortana() { drawCortanaAt(W, kbOpen ? KBTOP : UIH); }
static void coOpenAsk() { openKbStr(&coIn, 120, coSendEnter); }
static void coGoHome() { coMsgs.clear(); coView = 0; coMenu = false; coScroll = 0; coSum = 0; }
static void cortanaTapAt(int x, int y, int cw, int ch) {
    CoLay L = coLayout(cw, ch); bool chat = !coMsgs.empty() && coView == 0;
    if (coMenu) {
        int mx = cw - 300;
        if (x < mx) { coMenu = false; return; }
        int i = (y - 90) / 70;
        if (y >= 90 && i >= 0 && i < 4) { if (i == 0) coGoHome(); else if (i == 1) { coView = 1; coMenu = false; } else if (i == 2) { coView = 2; coMenu = false; } else { gCoSumm = !gCoSumm; win9Save(); coMenu = false; } }
        return;
    }
    if (inR(x, y, L.menu.x, L.menu.y, L.menu.w, L.menu.h)) { coMenu = true; return; }
    if (inR(x, y, L.music.x, L.music.y, L.music.w, L.music.h)) { coLaunch(A_MUSIC); return; }
    if (coView == 1 || coView == 2) {
        if (inR(x, y, cw - 230, 14, 160, 50)) { coView = 0; return; }
        if (coView == 1) { int yy = 96; for (size_t i = 0; i < coRems.size() && yy < ch - 90; i++, yy += 80) if (inR(x, y, cw - 200, yy, 200, 70)) { coRems.erase(coRems.begin() + i); coSaveRem(); return; } }
        else { for (int i = 0; i < 6; i++) { int yy = 96 + 140 + i * 56; if (inR(x, y, cw - 180, yy - 8, 180, 50)) { gCoInt ^= 1 << i; win9Save(); return; } } }
        return;
    }
    if (chat && inR(x, y, cw - 230, 14, 160, 50)) { coGoHome(); return; }
    if (inR(x, y, L.mic.x, L.mic.y, L.mic.w, L.mic.h)) { coListenT = SDL_GetTicks(); coOpenAsk(); return; }
    if (inR(x, y, L.ask.x, L.ask.y, L.ask.w, L.ask.h)) { coOpenAsk(); return; }
    if (!chat) {
        int gy = L.ringY + L.ringR + 40; auto lines = wrapText("Hi " + gUser + "! How can I help you?", F44, L.col.w);
        int hy = gy + (int)std::min<size_t>(lines.size(), 3) * 54 + 24;
        if (inR(x, y, L.cx - 80, hy + 20, 160, 36)) { coIn = "help"; coSend(); return; }
        if (inR(x, y, L.cx - 150, hy - 6, 300, 30)) { coIn = "what's the forecast"; coSend(); return; }
        if (L.wide && !gCoSumm && inR(x, y, cw / 2 + 60, 240, cw / 2 - 120, 50)) { gCoSumm = true; win9Save(); return; }
        if (L.wide && gCoSumm) {
            std::vector<CoCard> cs = coCards(); int yy = L.sum.y - coSum + 46;
            for (auto& c : cs) { if (inR(x, y, L.sum.x, yy, L.sum.w, CO_CARD_H - 8) && y >= L.sum.y) { static const int APP[7] = {0, A_CALENDAR, A_WEATHER, -3, A_TASKMGR, A_NEWS, A_SKYPE}; if (c.act == 3) coView = 1; else coLaunch(APP[c.act]); return; } yy += CO_CARD_H; }
            if (inR(x, y, L.sum.x, yy + 4, 380, 40) && y >= L.sum.y) { gCoSumm = false; win9Save(); return; }
        }
    }
}
static void cortanaTap(int x, int y) { cortanaTapAt(x, y, W, kbOpen ? KBTOP : UIH); }
static void cortanaDrag(int, int dy) {
    bool chat = !coMsgs.empty() && coView == 0;
    if (chat) coScroll = std::max(0, coScroll + dy); else coSum = std::max(0, std::min(coSumMax, coSum - dy));
}

// ================================================================ settings for the v0.9 features (settings9.ini)
static void win9Save() {
    std::string b; auto kv = [&](const char* k, int v) { b += std::string(k) + "=" + std::to_string(v) + "\n"; };
    kv("v9", gV9); kv("flip", gFlipMode); kv("flipsecs", gFlipSecs); kv("tilenames", gTileNames); kv("sndscheme", gSndScheme); kv("sndvol", gSndVol);
    kv("sndstartup", gSndStartup); kv("sndnotify", gSndNotify); kv("sndtouch", gSndTouch); kv("bootdesk", gBootDesk); kv("cosumm", gCoSumm); kv("coint", gCoInt); kv("odauto", gOdAuto); kv("skyaway", gSkyAway);
    writeFile(DIR_ + "settings9.ini", b);
}
static void win9Load() {
    std::string s; if (!readFile(DIR_ + "settings9.ini", s)) return;
    for (auto& l : w9Split(s, '\n')) {
        size_t q = l.find('='); if (q == std::string::npos) continue;
        std::string k = l.substr(0, q); int n = atoi(l.c_str() + q + 1);
        if (k == "v9") gV9 = n != 0; else if (k == "flip") gFlipMode = std::max(0, std::min(2, n)); else if (k == "flipsecs") gFlipSecs = std::max(3, std::min(30, n));
        else if (k == "tilenames") gTileNames = n != 0; else if (k == "sndscheme") gSndScheme = std::max(0, std::min(3, n)); else if (k == "sndvol") gSndVol = std::max(0, std::min(10, n));
        else if (k == "sndstartup") gSndStartup = n != 0; else if (k == "sndnotify") gSndNotify = n != 0; else if (k == "sndtouch") gSndTouch = n != 0;
        else if (k == "bootdesk") gBootDesk = n != 0; else if (k == "cosumm") gCoSumm = n != 0; else if (k == "coint") gCoInt = n; else if (k == "odauto") gOdAuto = n != 0; else if (k == "skyaway") gSkyAway = n != 0;
    }
}

// ================================================================ sound themes: every sound is synthesised here (no audio files)
// scheme 0 Windows Default (soft bells and pads), 1 Chimes (glass), 2 Retro (square wave), 3 No sounds
static const int SND_RATE = 48000;
static std::vector<Sint16> gSndPcm[3][SND_COUNT];
struct SndMix { std::vector<float> m; explicit SndMix(float secs) : m((size_t)(secs * SND_RATE), 0.0f) {} };
// wave: 0 bell, 1 glass, 2 square, 3 pad
static void sndNote(SndMix& s, float t0, float dur, float f, float amp, int wave) {
    size_t a = (size_t)(t0 * SND_RATE), n = (size_t)(dur * SND_RATE);
    for (size_t i = 0; i < n && a + i < s.m.size(); i++) {
        float t = (float)i / SND_RATE, e, v;
        if (wave == 3) { float att = std::min(1.0f, t / 0.7f), rel = std::min(1.0f, (dur - t) / 0.9f); e = att * std::max(0.0f, rel); v = sinf(6.2831853f * f * t) + 0.5f * sinf(6.2831853f * f * 1.004f * t) + 0.25f * sinf(6.2831853f * f * 2.0f * t); v *= 0.55f; }
        else if (wave == 2) { float att = std::min(1.0f, t / 0.004f); e = att * std::max(0.0f, 1.0f - t / dur); float ph = fmodf(f * t, 1.0f); v = ph < 0.5f ? 0.6f : -0.6f; }
        else if (wave == 1) { e = std::min(1.0f, t / 0.003f) * expf(-t * 3.2f / dur * 2.0f); v = sinf(6.2831853f * f * t) + 0.45f * sinf(6.2831853f * f * 2.76f * t) + 0.2f * sinf(6.2831853f * f * 5.4f * t); v *= 0.6f; }
        else { e = std::min(1.0f, t / 0.005f) * expf(-t * 5.0f / dur * 1.2f); v = sinf(6.2831853f * f * t) + 0.3f * sinf(6.2831853f * f * 2.0f * t) + 0.1f * sinf(6.2831853f * f * 3.0f * t); v *= 0.7f; }
        s.m[a + i] += v * e * amp;
    }
}
static std::vector<Sint16> sndFinish(SndMix& s, float peak) {
    float mx = 0.0001f; for (float v : s.m) mx = std::max(mx, fabsf(v));
    float g = peak / mx; std::vector<Sint16> o(s.m.size() * 2);
    for (size_t i = 0; i < s.m.size(); i++) { float v = s.m[i] * g; Sint16 x = (Sint16)(v * 32000.0f); o[i * 2] = x; o[i * 2 + 1] = x; }
    return o;
}
static std::vector<Sint16> sndBuild(int scheme, int id) {
    static const float C4 = 261.63f, D4 = 293.66f, E4 = 329.63f, G4 = 392.0f, A4 = 440.0f, B4 = 493.88f, C5 = 523.25f, D5 = 587.33f, E5 = 659.25f, G5 = 783.99f, A5 = 880.0f, B5 = 987.77f, C6 = 1046.5f, E6 = 1318.5f, G6 = 1568.0f;
    int w = scheme == 0 ? 0 : (scheme == 1 ? 1 : 2);
    switch (id) {
        case SND_STARTUP: {
            SndMix s(3.6f);
            if (scheme == 0) { // a soft rising pad swell, then three bells settle on a chord
                sndNote(s, 0.0f, 3.5f, C4, 0.5f, 3); sndNote(s, 0.2f, 3.3f, G4, 0.45f, 3); sndNote(s, 0.4f, 3.1f, E5, 0.4f, 3); sndNote(s, 0.6f, 2.9f, C5, 0.35f, 3);
                sndNote(s, 1.5f, 1.8f, G5, 0.5f, 0); sndNote(s, 1.9f, 1.6f, E6, 0.45f, 0); sndNote(s, 2.3f, 1.3f, C6, 0.4f, 0);
            } else if (scheme == 1) { // glass arpeggio
                float n[7] = {C5, E5, G5, A5, C6, E6, G6}; for (int i = 0; i < 7; i++) sndNote(s, 0.25f + i * 0.28f, 1.6f, n[i], 0.5f, 1);
                sndNote(s, 2.4f, 1.1f, C6, 0.4f, 1);
            } else { // chiptune jingle
                float n[6] = {C5, E5, G5, C6, G5, C6}; float d[6] = {0.16f, 0.16f, 0.16f, 0.32f, 0.16f, 0.6f}; float t = 0.15f;
                for (int i = 0; i < 6; i++) { sndNote(s, t, d[i] * 1.2f, n[i], 0.5f, 2); t += d[i]; }
                sndNote(s, t, 0.9f, E5, 0.25f, 2); sndNote(s, t, 0.9f, G5, 0.25f, 2);
            }
            return sndFinish(s, 0.7f);
        }
        case SND_NOTIFY: {
            SndMix s(1.0f);
            if (scheme == 2) { sndNote(s, 0.0f, 0.12f, E5, 0.5f, 2); sndNote(s, 0.14f, 0.3f, B5, 0.5f, 2); }
            else { sndNote(s, 0.0f, 0.9f, scheme == 1 ? A5 : E5, 0.5f, w); sndNote(s, 0.16f, 0.8f, scheme == 1 ? E6 : B5, 0.45f, w); }
            return sndFinish(s, 0.6f);
        }
        case SND_UNLOCK: {
            SndMix s(0.7f);
            if (scheme == 2) { sndNote(s, 0.0f, 0.08f, G4, 0.5f, 2); sndNote(s, 0.08f, 0.08f, C5, 0.5f, 2); sndNote(s, 0.16f, 0.2f, G5, 0.5f, 2); }
            else { sndNote(s, 0.0f, 0.6f, G4, 0.4f, w); sndNote(s, 0.1f, 0.55f, C5, 0.45f, w); sndNote(s, 0.2f, 0.45f, E5, 0.4f, w); }
            return sndFinish(s, 0.5f);
        }
        case SND_ERROR: {
            SndMix s(0.6f);
            sndNote(s, 0.0f, 0.25f, scheme == 2 ? 220.0f : 196.0f, 0.5f, scheme == 1 ? 1 : (scheme == 2 ? 2 : 0)); sndNote(s, 0.2f, 0.4f, scheme == 2 ? 165.0f : 146.8f, 0.5f, scheme == 1 ? 1 : (scheme == 2 ? 2 : 0));
            return sndFinish(s, 0.55f);
        }
        case SND_CLICK: {
            SndMix s(0.05f); sndNote(s, 0.0f, 0.04f, scheme == 2 ? 1200.0f : 1800.0f, 0.5f, scheme == 2 ? 2 : 1);
            return sndFinish(s, 0.35f);
        }
        default: { // SND_RING: two short double rings
            SndMix s(1.6f);
            for (int k = 0; k < 2; k++) for (int j = 0; j < 2; j++) { sndNote(s, 0.0f + k * 0.8f + j * 0.18f, 0.16f, 523.25f, 0.4f, scheme == 2 ? 2 : 0); sndNote(s, 0.0f + k * 0.8f + j * 0.18f, 0.16f, 659.25f, 0.3f, scheme == 2 ? 2 : 0); }
            return sndFinish(s, 0.5f);
        }
    }
}
#ifdef WINNX_AUDIO
static Mix_Chunk* gSndChunk[3][SND_COUNT];
static bool gSndTried = false;
static void sndPlayScheme(int scheme, int id) {
    if (scheme < 0 || scheme > 2) return;
    if (!gSndTried) { gSndTried = true; musInit(); }
    if (!musAudioOk) return;
    if (gSndPcm[scheme][id].empty()) gSndPcm[scheme][id] = sndBuild(scheme, id);
    if (!gSndChunk[scheme][id]) gSndChunk[scheme][id] = Mix_QuickLoad_RAW((Uint8*)gSndPcm[scheme][id].data(), (Uint32)(gSndPcm[scheme][id].size() * 2));
    Mix_Chunk* c = gSndChunk[scheme][id]; if (!c) return;
    float v = gSndVol / 10.0f; Mix_VolumeChunk(c, (int)(MIX_MAX_VOLUME * v * v));
    Mix_PlayChannel(-1, c, 0);
}
#else
static void sndPlayScheme(int, int) {}
#endif
static void sndPlay(int id) {
    if (gSndScheme == 3 || gSndVol == 0) return;
    if (id == SND_STARTUP && !gSndStartup) return;
    if (id == SND_NOTIFY && !gSndNotify) return;
    if (id == SND_CLICK && !gSndTouch) return;
    sndPlayScheme(gSndScheme, id);
}
static void sndPreview(int scheme, int id) { if (gSndVol == 0) return; sndPlayScheme(scheme, id); }

#include "w81_apps.h"
#include "w81_desk.h"
