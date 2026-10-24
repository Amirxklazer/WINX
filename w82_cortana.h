static void lockSave();
// w82_cortana.h - Cortana's extra brain: context, memory, personality, status of the new apps, time zones, countdowns, riddles, fuzzy app opening

static std::string coRiddleA; static int coScore = 0, coAsked = 0, coChats = 0; static std::string coLastTopic, coLastCity; static std::string coMemFile() { return DIR_ + "cortana_notes.txt"; }
static const char* CO_RIDDLES[][2] = {{"What has keys but can't open locks?", "keyboard"}, {"What gets wetter the more it dries?", "towel"}, {"What has hands but can't clap?", "clock"}, {"What goes up but never comes down?", "age"}, {"What has a neck but no head?", "bottle"}, {"The more you take, the more you leave behind. What am I?", "footsteps"}, {"What can travel around the world while staying in a corner?", "stamp"}, {"What has one eye but cannot see?", "needle"}};
static const char* CO_TRIVIA[][2] = {{"What is the capital of Algeria?", "algiers"}, {"How many players are on a football team on the pitch?", "11"}, {"What is the largest planet in our solar system?", "jupiter"}, {"What colour do you get by mixing blue and yellow?", "green"}, {"How many continents are there?", "7"}, {"What is the chemical symbol for water?", "h2o"}, {"Which animal is known as the king of the jungle?", "lion"}, {"What is 12 times 12?", "144"}};
static std::string coMood() { int h = gNow.h; if (coChats > 40) return "chatty"; if (h < 6) return "sleepy"; if (h < 11) return "cheerful"; if (h < 18) return "focused"; return "relaxed"; }
static std::string coPick(std::initializer_list<const char*> l) { int n = (int)l.size(); int i = (int)(SDL_GetTicks() % n); return *(l.begin() + i); }
static void coRemember(const std::string& s) { std::string d; readFile(coMemFile(), d); d += s + "\n"; if (d.size() > 8000) d.erase(0, d.size() - 8000); writeFile(coMemFile(), d); }
static std::string coRecall(const std::string& key) { std::string d; if (!readFile(coMemFile(), d)) return ""; std::string out; size_t p = 0; while (p < d.size()) { size_t e = d.find('\n', p); if (e == std::string::npos) e = d.size(); std::string l = d.substr(p, e - p); p = e + 1; if (key.empty() || coLow(l).find(key) != std::string::npos) out += (out.empty() ? "" : "; ") + l; } return out; }
static int coDaysUntil(int mo, int d) { int y = gNow.y; long long now = wcDays(gNow.y, gNow.mo, gNow.d), t = wcDays(y, mo, d); if (t < now) t = wcDays(y + 1, mo, d); return (int)(t - now); }
static int coFuzzyApp(const std::string& name) {   // fuzzy: substring either way, plus a few nicknames
    static const struct { const char* a; int app; } NICK[] = {{"explorer", A_FILES}, {"files", A_FILES}, {"pc", A_FILES}, {"mic", A_RECORDER}, {"record", A_RECORDER}, {"recipes", A_FOOD}, {"cook", A_FOOD}, {"fitness", A_HEALTH}, {"workout", A_HEALTH}, {"tamagotchi", A_PET}, {"city", A_CITY}, {"alarms", A_CLOCK}, {"alarm", A_CLOCK}, {"stopwatch", A_CLOCK}, {"timer", A_CLOCK}, {"browser", A_BROWSER}, {"internet", A_BROWSER}, {"chrome", A_BROWSER}, {"cmd", A_TERM}, {"command prompt", A_TERM}, {"calculator", A_CALC}, {"maths", A_CALC}, {"settings", A_SETTINGS}, {"pc settings", A_SETTINGS}, {"tips", A_HELP}};
    for (auto& n : NICK) if (name == n.a) return n.app;
    int best = -1; size_t bl = 0; for (int a = 0; a < A_COUNT; a++) { std::string an = coLow(ADEF[a].name); if (an.empty() || !appAvail(a)) continue; if (an.find(name) != std::string::npos || (name.find(an) != std::string::npos && an.size() >= 3)) { if (best < 0 || an.size() < bl) { best = a; bl = an.size(); } } }
    return best;
}
static bool coSmart(const std::string& q, const std::string& raw, std::string& o) {
    coChats++; (void)raw; std::string mood = coMood(); auto say = [&](const std::string& s) { o = s; return true; };
    // answers to the riddle / trivia we asked
    if (!coRiddleA.empty()) { std::string a = coRiddleA; if (q.find(a) != std::string::npos) { coRiddleA.clear(); coScore++; return say(coPick({"Correct!", "Yes! Nicely done.", "That's right."}) + " Score: " + std::to_string(coScore) + " of " + std::to_string(coAsked) + "."); } if (coHas(q, "give up") || coHas(q, "i don't know") || coHas(q, "idk") || coHas(q, "tell me")) { coRiddleA.clear(); return say("It was \"" + a + "\". Want another one?"); } if (q.size() < 22 && !coHas(q, "?") && !coStarts(q, "open") && !coStarts(q, "set")) { coRiddleA.clear(); return say("Not quite. The answer was \"" + a + "\"."); } coRiddleA.clear(); }
    if (coHas(q, "riddle")) { int i = rand() % 8; coRiddleA = CO_RIDDLES[i][1]; coAsked++; return say(CO_RIDDLES[i][0]); }
    if (coHas(q, "quiz me") || coHas(q, "trivia question") || coHas(q, "ask me a question") || coHas(q, "daily challenge") || coHas(q, "challenge")) { int i = rand() % 8; coRiddleA = CO_TRIVIA[i][1]; coAsked++; return say(CO_TRIVIA[i][0]); }
    if (coHas(q, "my score")) return say("You've answered " + std::to_string(coScore) + " of " + std::to_string(coAsked) + " correctly.");
    // status of the new apps
    if (coHas(q, "my pet") || coHas(q, "how is mochi") || coHas(q, "tamagotchi") || coHas(q, "pixel pet")) { coLastTopic = "pet"; return say(petStatus()); }
    if (coHas(q, "my city") || coHas(q, "metropolis")) { coLastTopic = "city"; return say(cityStatus()); }
    if ((coHas(q, "my health") || coHas(q, "my water") || coHas(q, "how many steps") || coHas(q, "my steps") || coHas(q, "calories today"))) { coLastTopic = "health"; return say(hlStatus()); }
    if (coHas(q, "what should i cook") || coHas(q, "what should i eat") || coHas(q, "dinner idea") || coHas(q, "recipe")) { coLaunch(A_FOOD); return say(foodStatus() + " Opening Food & Drink."); }
    if (coHas(q, "what's next on my alarm") || coHas(q, "next alarm") || coHas(q, "my alarms")) return say(clkNextAlarmText());
    // alarms and timers go to the Clock app
    if (coStarts(q, "set an alarm") || coStarts(q, "set alarm") || coStarts(q, "wake me") || coStarts(q, "alarm for")) { int hh, mm; if (coParseClock(q, hh, mm)) { clkAddAlarm(hh, mm, "Cortana"); char b[64]; snprintf(b, sizeof b, "Alarm set for %02d:%02d. You can edit it in Alarms & Clock.", hh, mm); return say(b); } }
    // time in another city
    if (coHas(q, "time in ") || coHas(q, "time is it in ") || (coHas(q, "what about ") && !coLastCity.empty())) {
        std::string c = coHas(q, "what about ") ? coAfter(q, "what about ") : (coHas(q, "time in ") ? coAfter(q, "time in ") : coAfter(q, "time is it in ")); while (!c.empty() && (c.back() == ' ' || c.back() == '?')) c.pop_back();
        if (!wcInit) wcLoad(); for (int i = 0; i < NWC; i++) if (coLow(WCITIES[i].n).find(c) != std::string::npos && c.size() >= 3) { int dd; float off; Now n = wcTime(i, dd, off); char b[96]; snprintf(b, sizeof b, "It's %02d:%02d in %s (%s).", n.h, n.mi, WCITIES[i].n, dd > 0 ? "tomorrow" : (dd < 0 ? "yesterday" : "today")); coLastCity = c; return say(b); }
        if (coHas(q, "time in ")) return say("I don't have " + c + " in my world clock list. You can add cities in Alarms & Clock.");
    }
    // countdowns
    if (coHas(q, "days until") || coHas(q, "days to ") || coHas(q, "how long until") || coHas(q, "countdown")) {
        struct H { const char* n; int m, d; } hs[] = {{"christmas", 12, 25}, {"new year", 1, 1}, {"valentine", 2, 14}, {"independence", 7, 5}, {"revolution", 11, 1}, {"halloween", 10, 31}, {"my birthday", 0, 0}};
        for (auto& h : hs) if (coHas(q, h.n) && h.m) { int d = coDaysUntil(h.m, h.d); return say(d == 0 ? std::string("That's today!") : std::to_string(d) + (d == 1 ? " day" : " days") + " until " + h.n + "."); }
        int m, d; if (sscanf(q.c_str() + (q.find("until") != std::string::npos ? q.find("until") : 0), "until %d/%d", &d, &m) == 2 && m >= 1 && m <= 12 && d >= 1 && d <= 31) return say(std::to_string(coDaysUntil(m, d)) + " days until " + std::to_string(d) + "/" + std::to_string(m) + ".");
    }
    if (coHas(q, "what day is it") || coHas(q, "what's the date") || coHas(q, "today's date") || coHas(q, "what is today")) return say("Today is " + dateStr() + ".");
    // memory
    if (coStarts(q, "remember that ") || coStarts(q, "remember ") || coStarts(q, "note that ")) { std::string s = coStarts(q, "remember that ") ? coAfter(q, "remember that ") : coStarts(q, "note that ") ? coAfter(q, "note that ") : coAfter(q, "remember "); if (!s.empty()) { coRemember(s); return say(coPick({"Got it. I'll remember that.", "Noted.", "Okay, I've made a note."})); } }
    if (coHas(q, "what did i tell you") || coHas(q, "what do you remember") || coHas(q, "what do you know about")) { std::string k = coHas(q, "about ") ? coAfter(q, "about ") : ""; std::string r = coRecall(k); return say(r.empty() ? "I haven't got any notes" + (k.empty() ? std::string(" yet.") : " about " + k + ".") : "You told me: " + r + "."); }
    if (coHas(q, "forget everything") || coHas(q, "clear my notes")) { writeFile(coMemFile(), ""); return say("Done. My notes are cleared."); }
    if (coStarts(q, "call me ") || coStarts(q, "my name is ")) { std::string n = coStarts(q, "call me ") ? coAfter(q, "call me ") : coAfter(q, "my name is "); if (!n.empty() && n.size() < 20) { n[0] = (char)toupper((unsigned char)n[0]); gUser = n; lockSave(); return say("Nice to meet you, " + n + "."); } }
    // personality
    if (coHas(q, "how are you") || coHas(q, "how do you feel") || coHas(q, "are you ok")) return say(mood == "sleepy" ? "A bit sleepy, honestly. It's early. You?" : mood == "chatty" ? "Great. We've been talking a lot today and I like it." : mood == "cheerful" ? "Bright and cheerful. How about you?" : mood == "focused" ? "Focused and ready. What do you need?" : "Relaxed. Good evening for a chat. You?");
    if (coHas(q, "you're awesome") || coHas(q, "you are awesome") || coHas(q, "good job") || coHas(q, "well done") || coHas(q, "you're smart") || coHas(q, "i like you")) return say(coPick({"Thank you, that made my ring glow.", "Aw, thanks. I try.", "You're kind. Let's do something fun."}));
    if (coHas(q, "stupid") || coHas(q, "useless") || coHas(q, "i hate you") || coHas(q, "shut up")) return say(coPick({"Sorry about that. Tell me what you needed and I'll try again.", "I'll do better. What went wrong?"}));
    if (coHas(q, "i'm bored") || coHas(q, "im bored") || coHas(q, "i am bored") || coHas(q, "entertain me")) { switch (rand() % 4) { case 0: coLaunch(A_PET); return say("Go and see your Pixel Pet. It misses you."); case 1: coLaunch(A_CITY); return say("Your city is waiting for a new building. Opening Metropolis."); case 2: { int i = rand() % 8; coRiddleA = CO_RIDDLES[i][1]; coAsked++; return say("Riddle time: " + std::string(CO_RIDDLES[i][0])); } default: return say(std::string(CO_JOKES[rand() % 7])); } }
    if (coHas(q, "good night") || coHas(q, "goodnight")) return say("Sleep well, " + gUser + ". I'll keep an eye on your alarms.");
    if (coHas(q, "what can you do") || coHas(q, "what do you do")) return say("I can set alarms and reminders, convert units, do maths, tell you the time in other cities, count days to holidays, remember notes, check your pet, city and health, play riddles and trivia, open any app, and search the web. I can't hear you yet, so type to me.");
    if (coHas(q, "are you real") || coHas(q, "are you alive") || coHas(q, "are you human")) return say("I'm a program living on your Switch. I can't hear or learn like the real thing, but I do my best to be good company.");
    // settings shortcuts
    if (coHas(q, "flip tiles") || coHas(q, "live tile animation")) { bool off = coHas(q, " off") || coHas(q, "turn off") || coHas(q, "disable"); gFlipMode = off ? 0 : (coHas(q, "slide") ? 2 : 1); win9Save(); return say(off ? "Live tile animation is off." : "Live tiles will " + std::string(gFlipMode == 2 ? "slide" : "flip") + "."); }
    if ((coHas(q, "volume") || coHas(q, "sound level")) && (coHas(q, "set") || coHas(q, "to "))) { const char* s = q.c_str(); while (*s && !isdigit((unsigned char)*s)) s++; if (*s) { gSndVol = std::max(0, std::min(10, atoi(s))); win9Save(); sndPreview(gSndScheme == 3 ? 0 : gSndScheme, SND_NOTIFY); return say("Volume set to " + std::to_string(gSndVol) + " out of 10."); } }
    if (coHas(q, "sound settings") || coHas(q, "open sound")) { openSettingsPage(4); return say("Opening Sound settings."); }
    if (coHas(q, "mute")) { gSndVol = 0; win9Save(); return say("Muted my sounds."); }
    // open an app by fuzzy name
    for (const char* v : {"open ", "launch ", "start ", "go to ", "show me ", "run "}) if (coStarts(q, v)) { std::string n = coAfter(q, v); while (!n.empty() && n.back() == ' ') n.pop_back(); if (coStarts(n, "the ")) n.erase(0, 4); if (coStarts(n, "my ")) n.erase(0, 3); int a = n.size() >= 2 ? coFuzzyApp(n) : -1; if (a >= 0) { coLaunch(a); return say(std::string("Opening ") + ADEF[a].name + "."); } }
    return false;
}
