// w82_calc.h - multi-mode calculator: Scientific (2nd functions, memory, history), Graphing, Programmer, Statistics, Date calculation
// Included from the end of w82.h. The Standard layout stays in main.cpp; this file adds the mode menu and the other modes.

static const char* CALC_MODES[6] = {"Standard", "Scientific", "Graphing", "Programmer", "Statistics", "Date calculation"};
static bool gCalcMenu = false, gCalcLoaded = false, gCalc2nd = false, gCalcHist = false;
static int gCalcBtnY = 16, gHistScroll = 0;
static double gCalcMem = 0; static bool gCalcMemSet = false;
static std::vector<std::pair<std::string, std::string>> gCalcHistV;
static double gSciX = 0;

static const Col CK_BG = {31, 31, 31}, CK_NUM = {58, 58, 58}, CK_OP = {86, 86, 86}, CK_FN = {44, 52, 72}, CK_DISP = {42, 42, 42}, CK_DIS = {44, 44, 44};

// ---------------------------------------------------------------- evaluator extras (hooked from sciPrimary in main.cpp)
static bool sciIdCh(char ch) { return (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z'); }
static bool sciExt(SciP& p, double& out) {
    const char* s = p.s + p.i;
    if (s[0] == 'x' && !sciIdCh(s[1])) { p.i++; out = gSciX; return true; }
    if (strncmp(s, "phi", 3) == 0 && !sciIdCh(s[3])) { p.i += 3; out = 1.6180339887498949; return true; }
    if (strncmp(s, "rand", 4) == 0 && !sciIdCh(s[4])) { p.i += 4; out = (double)(rand() % 1000000) / 1000000.0; return true; }
    if ((strncmp(s, "nPr(", 4) == 0) || (strncmp(s, "nCr(", 4) == 0)) {
        bool perm = s[1] == 'P'; p.i += 4;
        double a = sciExpr(p); sciWs(p);
        double b = 0; if (p.s[p.i] == ',') { p.i++; b = sciExpr(p); sciWs(p); } else p.err = true;
        if (p.s[p.i] == ')') p.i++;
        if (a < 0 || b < 0 || b > a || a != floor(a) || b != floor(b)) { out = NAN; return true; }
        double r = 1; if (perm) { for (int i = 0; i < (int)b; i++) r *= (a - i); } else { double k = std::min(b, a - b); for (int i = 1; i <= (int)k; i++) r = r * (a - k + i) / i; }
        out = r; return true;
    }
    static const char* F1[] = {"asinh", "acosh", "atanh", "sinh", "cosh", "tanh", "log2", "exp", "floor", "ceil", "round", "sgn", nullptr};
    for (int k = 0; F1[k]; k++) {
        size_t n = strlen(F1[k]);
        if (strncmp(s, F1[k], n) == 0 && s[n] == '(') {
            p.i += n + 1; double v = sciExpr(p); sciWs(p); if (p.s[p.i] == ')') p.i++;
            switch (k) {
                case 0: out = asinh(v); break; case 1: out = acosh(v); break; case 2: out = atanh(v); break;
                case 3: out = sinh(v); break; case 4: out = cosh(v); break; case 5: out = tanh(v); break;
                case 6: out = log2(v); break; case 7: out = exp(v); break; case 8: out = floor(v); break;
                case 9: out = ceil(v); break; case 10: out = floor(v + 0.5); break; default: out = (v > 0) - (v < 0); break;
            }
            return true;
        }
    }
    return false;
}

// ---------------------------------------------------------------- persistence
static std::string gFn[3] = {"sin(x)", "x^2/4", ""};
static std::vector<double> gStat;
static void calcSave() {
    std::string b = "mode=" + std::to_string(gCalcMode) + "\nmem=" + sciFmt(gCalcMem) + "\nmemset=" + std::to_string(gCalcMemSet ? 1 : 0) + "\n";
    for (int i = 0; i < 3; i++) b += "f" + std::to_string(i) + "=" + gFn[i] + "\n";
    for (size_t i = 0; i < gCalcHistV.size() && i < 40; i++) b += "h=" + gCalcHistV[i].first + "\t" + gCalcHistV[i].second + "\n";
    std::string st; for (double d : gStat) st += sciFmt(d) + ",";
    b += "stat=" + st + "\n";
    writeFile(DIR_ + "calc.txt", b);
}
static void calcEnsure() {
    if (gCalcLoaded) return;
    gCalcLoaded = true; gCalcMode = gSci ? 1 : 0;
    std::string d; if (!readFile(DIR_ + "calc.txt", d)) return;
    size_t pos = 0;
    while (pos < d.size()) {
        size_t e = d.find('\n', pos); if (e == std::string::npos) e = d.size();
        std::string ln = d.substr(pos, e - pos); pos = e + 1;
        size_t q = ln.find('='); if (q == std::string::npos) continue;
        std::string k = ln.substr(0, q), v = ln.substr(q + 1);
        if (k == "mode") gCalcMode = std::max(0, std::min(5, atoi(v.c_str())));
        else if (k == "mem") gCalcMem = atof(v.c_str());
        else if (k == "memset") gCalcMemSet = v == "1";
        else if (k.size() == 2 && k[0] == 'f' && k[1] >= '0' && k[1] <= '2') gFn[k[1] - '0'] = v;
        else if (k == "h") { size_t t = v.find('\t'); if (t != std::string::npos) gCalcHistV.push_back({v.substr(0, t), v.substr(t + 1)}); }
        else if (k == "stat") { gStat.clear(); size_t a = 0; while (a < v.size()) { size_t c = v.find(',', a); if (c == std::string::npos) c = v.size(); if (c > a) gStat.push_back(atof(v.substr(a, c - a).c_str())); a = c + 1; } }
    }
    gSci = gCalcMode == 1;
}
static bool gCalcDegSaved = true;
static void calcSetMode(int m) { if (m == 2 && gCalcMode != 2) { gCalcDegSaved = gSciDeg; gSciDeg = false; } else if (m != 2 && gCalcMode == 2) gSciDeg = gCalcDegSaved; gCalcMode = m; gSci = m == 1; gCalcMenu = false; gCalcHist = false; calcSave(); }

// ---------------------------------------------------------------- shared widgets
static void cbtn(int x, int y, int w, int h, const std::string& l, Col c, int f = F30, Col fg = WHITE) {
    rect(x, y, w, h, c);
    text(l, f, x + w / 2, y + h / 2 - (f == F44 ? 28 : (f == F30 ? 18 : (f == F22 ? 15 : 10))), fg, 1);
}
static void calcModeBtn(int y) {
    gCalcBtnY = y;
    rect(SCI_TX, y, 250, 40, Col{70, 70, 76});
    for (int i = 0; i < 3; i++) rect(SCI_TX + 12, y + 12 + i * 8, 20, 3, WHITE);
    text(CALC_MODES[gCalcMode], F22, SCI_TX + 44, y + 6, WHITE);
}
static void calcMenuDraw() {
    if (!gCalcMenu) return;
    int y0 = gCalcBtnY + 44;
    rect(SCI_TX - 4, y0 - 4, 308, 6 * 50 + 8, Col{24, 24, 28}, 250);
    for (int i = 0; i < 6; i++) {
        rect(SCI_TX, y0 + i * 50, 300, 46, i == gCalcMode ? THEMES[gTheme].accent : Col{58, 58, 64});
        text(CALC_MODES[i], F22, SCI_TX + 18, y0 + i * 50 + 9, WHITE);
    }
}
static bool inGrid(int x, int y, int x0, int y0, int cw, int ch, int gap, int cols, int rows, int& c, int& r) {
    if (x < x0 || y < y0) return false;
    c = (x - x0) / (cw + gap); r = (y - y0) / (ch + gap);
    if (c >= cols || r >= rows) return false;
    return ((x - x0) % (cw + gap)) < cw && ((y - y0) % (ch + gap)) < ch;
}

// ================================================================ Scientific
struct SciK { const char* l; const char* ins; };
// 4 function columns x 5 rows, first set and 2nd set
static const SciK SK1[20] = {
    {"sin", "sin("}, {"cos", "cos("}, {"tan", "tan("}, {"pi", "pi"},
    {"asin", "asin("}, {"acos", "acos("}, {"atan", "atan("}, {"e", "e"},
    {"ln", "ln("}, {"log", "log("}, {"sqrt", "sqrt("}, {"x\xC2\xB2", "^2"},
    {"x^y", "^"}, {"n!", "!"}, {"1/x", "^(-1)"}, {"|x|", "abs("},
    {"10^x", "10^("}, {"e^x", "exp("}, {"mod", " mod "}, {"Ans", "Ans"}};
static const SciK SK2[20] = {
    {"sinh", "sinh("}, {"cosh", "cosh("}, {"tanh", "tanh("}, {"phi", "phi"},
    {"asinh", "asinh("}, {"acosh", "acosh("}, {"atanh", "atanh("}, {"c", "299792458"},
    {"log2", "log2("}, {"cbrt", "cbrt("}, {"nPr", "nPr("}, {"nCr", "nCr("},
    {"floor", "floor("}, {"ceil", "ceil("}, {"round", "round("}, {"g", "9.80665"},
    {"2^x", "2^("}, {"x\xC2\xB3", "^3"}, {"rand", "rand"}, {"Ans", "Ans"}};
static const SciK SKR[20] = {  // right-hand 4 columns
    {"C", "C"}, {"DEL", "DEL"}, {"(", "("}, {")", ")"},
    {"7", "7"}, {"8", "8"}, {"9", "9"}, {"\xC3\xB7", "/"},
    {"4", "4"}, {"5", "5"}, {"6", "6"}, {"\xC3\x97", "*"},
    {"1", "1"}, {"2", "2"}, {"3", "3"}, {"-", "-"},
    {"0", "0"}, {".", "."}, {"=", "="}, {"+", "+"}};
static const SciK SKM[8] = {{"MC", "MC"}, {"MR", "MR"}, {"M+", "M+"}, {"M-", "M-"}, {"MS", "MS"}, {"EE", "*10^("}, {"%", "%"}, {",", ","}};
static const int SC_GX = 56, SC_GY = 210, SC_CW = 140, SC_CH = 80, SC_GAP = 6;

static double sciCurVal() { double v; if (sciEval(gSciExpr, v)) return v; return gSciAns; }
static void sciDel() {
    static const char* TK[] = {"asinh(", "acosh(", "atanh(", "asin(", "acos(", "atan(", "sinh(", "cosh(", "tanh(", "sqrt(", "floor(", "round(", "cbrt(", "ceil(", "log2(", "nPr(", "nCr(", "exp(", "sin(", "cos(", "tan(", "abs(", "log(", "sgn(", "ln(", "rand", "phi", "Ans", "pi", "^(-1)", " mod ", nullptr};
    if (gSciExpr.empty()) return;
    for (int i = 0; TK[i]; i++) { size_t n = strlen(TK[i]); if (gSciExpr.size() >= n && gSciExpr.compare(gSciExpr.size() - n, n, TK[i]) == 0) { gSciExpr.erase(gSciExpr.size() - n); gSciErr = false; gSciDone = false; return; } }
    gSciExpr.pop_back(); gSciErr = false; gSciDone = false;
}
static void sciPress2(const std::string& ins) {
    if (ins == "DEL") { sciDel(); return; }
    if (ins == "MC") { gCalcMemSet = false; gCalcMem = 0; calcSave(); return; }
    if (ins == "MR") { if (gCalcMemSet) { if (gSciDone) gSciExpr.clear(); gSciDone = false; gSciExpr += sciFmt(gCalcMem); } return; }
    if (ins == "MS") { gCalcMem = sciCurVal(); gCalcMemSet = true; calcSave(); return; }
    if (ins == "M+") { gCalcMem += sciCurVal(); gCalcMemSet = true; calcSave(); return; }
    if (ins == "M-") { gCalcMem -= sciCurVal(); gCalcMemSet = true; calcSave(); return; }
    if (ins == "=") {
        std::string ex = gSciExpr; sciPress("=");
        if (gSciDone && !gSciErr && !ex.empty()) { gCalcHistV.insert(gCalcHistV.begin(), {ex, gSciExpr}); if (gCalcHistV.size() > 40) gCalcHistV.pop_back(); calcSave(); }
        return;
    }
    bool op = ins == "/" || ins == "*" || ins == "^" || ins == "!" || ins == "%" || ins == "^2" || ins == "^3" || ins == "^(-1)" || ins == "+" || ins == "-" || ins == " mod " || ins == ",";
    if (ins == "*10^(") op = true;
    if (!op && ins != "C" && ins != "(" && ins != ")" && gSciDone) { /* typing after a result starts fresh (handled in sciPress) */ }
    sciPress(ins);
}
static void drawSciMode(const Theme& th) {
    rect(0, 0, W, UIH, CK_BG);
    calcModeBtn(12);
    rect(320, 12, 100, 40, gCalc2nd ? th.accent : Col{70, 70, 76}); text("2nd", F22, 370, 18, WHITE, 1);
    rect(430, 12, 100, 40, Col{70, 70, 76}); text(gSciDeg ? "DEG" : "RAD", F22, 480, 18, WHITE, 1);
    rect(540, 12, 100, 40, gCalcHist ? th.accent : Col{70, 70, 76}); text("Hist", F22, 590, 18, WHITE, 1);
    if (gCalcMemSet) text("M = " + sciFmt(gCalcMem), F22, 1224, 18, Col{200, 200, 210}, 2);
    rect(56, 58, 1168, 100, CK_DISP);
    std::string ex = sciShow(gSciExpr);
    while (ex.size() > 3 && tw(ex, F30) > 1120) ex.erase(0, 1);
    text(ex.empty() ? "0" : ex, F30, 1208, 64, WHITE, 2, ex.empty() ? 140 : 255);
    double v; std::string pre;
    if (gSciErr) pre = "Error"; else if (!gSciDone && sciEval(gSciExpr, v)) pre = "= " + sciFmt(v);
    text(pre, F44, 1208, 102, gSciErr ? Col{255, 120, 120} : WHITE, 2, 200);
    for (int i = 0; i < 8; i++) cbtn(SC_GX + i * (SC_CW + SC_GAP), 164, SC_CW, 38, SKM[i].l, i < 5 && gCalcMemSet && i < 2 ? Col{60, 70, 100} : CK_FN, F22);
    const SciK* A = gCalc2nd ? SK2 : SK1;
    for (int r = 0; r < 5; r++) for (int c = 0; c < 4; c++) {
        const SciK& k = A[r * 4 + c]; std::string l = k.l;
        cbtn(SC_GX + c * (SC_CW + SC_GAP), SC_GY + r * (SC_CH + SC_GAP), SC_CW, SC_CH, l, CK_FN, l.size() > 3 ? F22 : F30);
    }
    for (int r = 0; r < 5; r++) for (int c = 0; c < 4; c++) {
        const SciK& k = SKR[r * 4 + c]; std::string l = k.l;
        Col col = CK_NUM; if (l == "=") col = th.accent; else if (l == "C") col = Col{170, 50, 50}; else if (!(l[0] >= '0' && l[0] <= '9') && l != ".") col = CK_OP;
        cbtn(SC_GX + (c + 4) * (SC_CW + SC_GAP), SC_GY + r * (SC_CH + SC_GAP), SC_CW, SC_CH, l, col, l.size() > 3 ? F22 : F44);
    }
    if (gCalcHist) {
        rect(640, 58, 584, 596, Col{22, 22, 26}, 252);
        text("History", F30, 664, 66, WHITE);
        cbtn(1096, 62, 110, 40, "Clear", Col{150, 40, 40}, F22);
        SDL_Rect clip{640, 110, 584, 544}; SDL_RenderSetClipRect(gR, &clip);
        int n = (int)gCalcHistV.size(); gHistScroll = std::max(0, std::min(gHistScroll, std::max(0, n * 76 - 544)));
        for (int i = 0; i < n; i++) {
            int y = 116 + i * 76 - gHistScroll; if (y + 70 < 110 || y > 654) continue;
            rect(656, y, 552, 70, Col{40, 40, 46});
            text(fitTxt(sciShow(gCalcHistV[i].first), F16, 520), F16, 1196, y + 6, Col{170, 170, 180}, 2);
            text(fitTxt("= " + gCalcHistV[i].second, F30, 520), F30, 1196, y + 28, WHITE, 2);
        }
        SDL_RenderSetClipRect(gR, nullptr);
        if (n == 0) text("Nothing here yet. Results appear after =.", F16, 664, 130, Col{150, 150, 160});
    }
}
static bool tapSciMode(int x, int y) {
    if (gCalcHist) {
        if (inR(x, y, 1096, 62, 110, 40)) { gCalcHistV.clear(); calcSave(); return true; }
        if (x >= 640 && y >= 110) {
            int i = (y - 116 + gHistScroll) / 76;
            if (i >= 0 && i < (int)gCalcHistV.size() && ((y - 116 + gHistScroll) % 76) < 70) { gSciExpr = gCalcHistV[i].first; gSciDone = false; gSciErr = false; gCalcHist = false; }
            return true;
        }
        if (inR(x, y, 540, 12, 100, 40)) { gCalcHist = false; return true; }
        if (x >= 640 && y >= 58) return true;
    }
    if (inR(x, y, 320, 12, 100, 40)) { gCalc2nd = !gCalc2nd; return true; }
    if (inR(x, y, 430, 12, 100, 40)) { gSciDeg = !gSciDeg; saveCfg(); return true; }
    if (inR(x, y, 540, 12, 100, 40)) { gCalcHist = !gCalcHist; gHistScroll = 0; return true; }
    int c, r;
    for (int i = 0; i < 8; i++) if (inR(x, y, SC_GX + i * (SC_CW + SC_GAP), 164, SC_CW, 38)) { sciPress2(SKM[i].ins); return true; }
    if (inGrid(x, y, SC_GX, SC_GY, SC_CW, SC_CH, SC_GAP, 8, 5, c, r)) {
        if (c < 4) sciPress2((gCalc2nd ? SK2 : SK1)[r * 4 + c].ins); else sciPress2(SKR[r * 4 + (c - 4)].ins);
        return true;
    }
    return true;
}

// ================================================================ Graphing
static double gGx = 0, gGy = 0, gGs = 60; static bool gGTrace = false; static double gGTx = 0; static int gGSel = 0;
static const Col GCOL[3] = {{255, 110, 110}, {110, 220, 140}, {120, 170, 255}};
static const int GR_X = 440, GR_Y = 62, GR_W = 784, GR_H = 590;
static bool graphEval(int i, double x, double& y) { if (gFn[i].empty()) return false; gSciX = x; return sciEval(gFn[i], y); }
static void graphNiceStep(double& step) { double raw = 44.0 / gGs; double p = pow(10.0, floor(log10(raw))); double m = raw / p; step = (m <= 1 ? 1 : m <= 2 ? 2 : m <= 5 ? 5 : 10) * p; }
static void drawGraphMode(const Theme& th) {
    rect(0, 0, W, UIH, CK_BG);
    calcModeBtn(12);
    for (int i = 0; i < 3; i++) {
        int y = 62 + i * 70;
        rect(56, y, 364, 62, i == gGSel ? Col{58, 62, 74} : CK_DISP);
        rect(56, y, 8, 62, GCOL[i]);
        text("y" + std::to_string(i + 1) + " =", F16, 76, y + 4, Col{170, 170, 180});
        double t; bool bad = !gFn[i].empty() && !graphEval(i, 1.0, t) && !graphEval(i, 0.5, t) && !graphEval(i, 2.0, t);
        text(gFn[i].empty() ? "tap to type" : fitTxt(gFn[i], F30, 320), F30, 76, y + 24, WHITE, 0, gFn[i].empty() ? 120 : 255);
        if (bad) text("!", F30, 408, y + 18, Col{255, 120, 120}, 2);
    }
    cbtn(56, 280, 84, 52, "+", CK_OP, F44); cbtn(146, 280, 84, 52, "-", CK_OP, F44); cbtn(236, 280, 184, 52, "Reset view", CK_OP, F22);
    cbtn(56, 338, 182, 52, gGTrace ? "Trace on" : "Trace", gGTrace ? th.accent : CK_OP, F22); cbtn(244, 338, 176, 52, gSciDeg ? "Degrees" : "Radians", CK_OP, F22);
    static const char* PR[6] = {"sin(x)", "x^2", "1/x", "sqrt(x)", "abs(x)", "ln(x)"};
    text("Quick functions", F16, 56, 400, Col{150, 150, 160});
    for (int i = 0; i < 6; i++) cbtn(56 + (i % 3) * 122, 424 + (i / 3) * 58, 116, 52, PR[i], CK_FN, F22);
    cbtn(56, 544, 364, 52, "Clear selected", Col{110, 50, 50}, F22);
    // plot
    rect(GR_X, GR_Y, GR_W, GR_H, Col{18, 18, 22});
    SDL_Rect clip{GR_X, GR_Y, GR_W, GR_H}; SDL_RenderSetClipRect(gR, &clip);
    double cx = GR_X + GR_W / 2.0, cy = GR_Y + GR_H / 2.0;
    auto px = [&](double mx) { return cx + (mx - gGx) * gGs; };
    auto py = [&](double my) { return cy - (my - gGy) * gGs; };
    double step; graphNiceStep(step);
    double x0 = gGx - (GR_W / 2.0) / gGs, x1 = gGx + (GR_W / 2.0) / gGs, y0 = gGy - (GR_H / 2.0) / gGs, y1 = gGy + (GR_H / 2.0) / gGs;
    for (double gx = ceil(x0 / step) * step; gx <= x1; gx += step) { int X = (int)px(gx); bool ax = fabs(gx) < step * 0.01; rect(X, GR_Y, ax ? 2 : 1, GR_H, ax ? Col{150, 150, 160} : Col{44, 44, 52}); if (!ax) { char b[24]; snprintf(b, sizeof b, "%g", gx); text(b, F16, X + 3, std::min(GR_Y + GR_H - 22, std::max(GR_Y + 2, (int)py(0) + 2)), Col{120, 120, 130}); } }
    for (double gy = ceil(y0 / step) * step; gy <= y1; gy += step) { int Y = (int)py(gy); bool ax = fabs(gy) < step * 0.01; rect(GR_X, Y, GR_W, ax ? 2 : 1, ax ? Col{150, 150, 160} : Col{44, 44, 52}); if (!ax) { char b[24]; snprintf(b, sizeof b, "%g", gy); text(b, F16, std::max(GR_X + 2, std::min(GR_X + GR_W - 60, (int)px(0) + 4)), Y + 2, Col{120, 120, 130}); } }
    for (int f = 0; f < 3; f++) {
        if (gFn[f].empty()) continue;
        bool havePrev = false; double ppx = 0, ppy = 0;
        for (int sx = 0; sx <= GR_W; sx += 2) {
            double mx = gGx + (sx - GR_W / 2.0) / gGs, my;
            if (!graphEval(f, mx, my)) { havePrev = false; continue; }
            double X = GR_X + sx, Y = py(my);
            if (Y < GR_Y - 4000 || Y > GR_Y + GR_H + 4000) { havePrev = false; continue; }
            if (havePrev && fabs(Y - ppy) < GR_H * 1.5) line((int)ppx, (int)ppy, (int)X, (int)Y, 3, GCOL[f]);
            ppx = X; ppy = Y; havePrev = true;
        }
    }
    if (gGTrace) {
        int X = (int)px(gGTx); rect(X, GR_Y, 1, GR_H, Col{200, 200, 210}, 160);
        int ty = GR_Y + 6;
        char b[96]; snprintf(b, sizeof b, "x = %s", sciFmt(gGTx).c_str()); text(b, F22, GR_X + 12, ty, WHITE); ty += 28;
        for (int f = 0; f < 3; f++) { double my; if (!gFn[f].empty() && graphEval(f, gGTx, my)) { disc(X, (int)py(my), 6, GCOL[f]); snprintf(b, sizeof b, "y%d = %s", f + 1, sciFmt(my).c_str()); text(b, F22, GR_X + 12, ty, GCOL[f]); ty += 28; } }
    }
    SDL_RenderSetClipRect(gR, nullptr);
    outline(GR_X, GR_Y, GR_W, GR_H, 1, Col{90, 90, 100});
}
static std::string* gGEditStr = nullptr;
static void graphEdited() { calcSave(); }
static bool tapGraphMode(int x, int y) {
    for (int i = 0; i < 3; i++) if (inR(x, y, 56, 62 + i * 70, 364, 62)) { gGSel = i; openKbStr(&gFn[i], 60, graphEdited); return true; }
    if (inR(x, y, 56, 280, 84, 52)) { gGs = std::min(4000.0, gGs * 1.3); return true; }
    if (inR(x, y, 146, 280, 84, 52)) { gGs = std::max(2.0, gGs / 1.3); return true; }
    if (inR(x, y, 236, 280, 184, 52)) { gGx = 0; gGy = 0; gGs = 60; return true; }
    if (inR(x, y, 56, 338, 182, 52)) { gGTrace = !gGTrace; return true; }
    if (inR(x, y, 244, 338, 176, 52)) { gSciDeg = !gSciDeg; saveCfg(); return true; }
    static const char* PR[6] = {"sin(x)", "x^2", "1/x", "sqrt(x)", "abs(x)", "ln(x)"};
    for (int i = 0; i < 6; i++) if (inR(x, y, 56 + (i % 3) * 122, 424 + (i / 3) * 58, 116, 52)) { gFn[gGSel] = PR[i]; calcSave(); return true; }
    if (inR(x, y, 56, 544, 364, 52)) { gFn[gGSel].clear(); calcSave(); return true; }
    if (inR(x, y, GR_X, GR_Y, GR_W, GR_H) && gGTrace) { gGTx = gGx + (x - (GR_X + GR_W / 2.0)) / gGs; return true; }
    return true;
}

// ================================================================ Programmer
static int pgBase = 10, pgBits = 64, pgOp = 0; static unsigned long long pgVal = 0, pgAcc = 0; static bool pgFresh = true;
static unsigned long long pgMask() { return pgBits >= 64 ? ~0ULL : ((1ULL << pgBits) - 1); }
static long long pgSigned(unsigned long long v) { v &= pgMask(); if (pgBits < 64 && (v >> (pgBits - 1)) & 1) return (long long)(v | ~pgMask()); return (long long)v; }
static std::string pgStr(unsigned long long v, int base, bool group) {
    v &= pgMask(); std::string s;
    if (base == 10) return std::to_string(pgSigned(v));
    if (v == 0) s = "0";
    while (v) { int d = (int)(v % base); s.insert(s.begin(), "0123456789ABCDEF"[d]); v /= base; }
    if (group) { int gsz = base == 2 ? 4 : (base == 16 ? 4 : 3); std::string o; for (size_t i = 0; i < s.size(); i++) { if (i && (s.size() - i) % gsz == 0) o += ' '; o += s[i]; } return o; }
    return s;
}
static void pgApply() {
    unsigned long long a = pgAcc & pgMask(), b = pgVal & pgMask(), r = b;
    long long sa = pgSigned(a), sb = pgSigned(b);
    switch (pgOp) {
        case 1: r = a + b; break; case 2: r = a - b; break; case 3: r = (unsigned long long)(sa * sb); break;
        case 4: r = sb ? (unsigned long long)(sa / sb) : 0; break; case 5: r = sb ? (unsigned long long)(sa % sb) : 0; break;
        case 6: r = a & b; break; case 7: r = a | b; break; case 8: r = a ^ b; break; case 9: r = ~(a & b); break; case 10: r = ~(a | b); break;
        case 11: r = b >= 64 ? 0 : (a << b); break; case 12: r = b >= 64 ? 0 : (a >> b); break;
        case 13: { unsigned k = (unsigned)(b % pgBits); r = k ? ((a << k) | (a >> (pgBits - k))) : a; break; }
        case 14: { unsigned k = (unsigned)(b % pgBits); r = k ? ((a >> k) | (a << (pgBits - k))) : a; break; }
        default: r = b; break;
    }
    pgVal = r & pgMask();
}
struct PgK { const char* l; int kind; int val; };   // kind 0 digit, 1 binary op, 2 special
static const PgK PGK[40] = {
    {"A", 0, 10}, {"B", 0, 11}, {"7", 0, 7}, {"8", 0, 8}, {"9", 0, 9}, {"AND", 1, 6}, {"OR", 1, 7}, {"XOR", 1, 8},
    {"C", 0, 12}, {"D", 0, 13}, {"4", 0, 4}, {"5", 0, 5}, {"6", 0, 6}, {"NOT", 2, 1}, {"<<", 1, 11}, {">>", 1, 12},
    {"E", 0, 14}, {"F", 0, 15}, {"1", 0, 1}, {"2", 0, 2}, {"3", 0, 3}, {"+", 1, 1}, {"-", 1, 2}, {"\xC3\x97", 1, 3},
    {"NAND", 1, 9}, {"NOR", 1, 10}, {"0", 0, 0}, {"00", 2, 4}, {"FF", 2, 5}, {"\xC3\xB7", 1, 4}, {"mod", 1, 5}, {"+/-", 2, 2},
    {"RoL", 1, 13}, {"RoR", 1, 14}, {"", 2, 0}, {"", 2, 0}, {"", 2, 0}, {"AC", 2, 6}, {"DEL", 2, 3}, {"=", 2, 7}};
static const int PG_X = 56, PG_Y = 296, PG_CW = 140, PG_CH = 64, PG_GAP = 6;
static void pgPress(const PgK& k) {
    if (k.kind == 0) {
        if (k.val >= pgBase) return;
        if (pgFresh) { pgVal = 0; pgFresh = false; }
        pgVal = (pgVal * pgBase + k.val) & pgMask();
    } else if (k.kind == 1) {
        if (pgOp && !pgFresh) pgApply();
        pgAcc = pgVal; pgOp = k.val; pgFresh = true;
    } else switch (k.val) {
        case 1: pgVal = ~pgVal & pgMask(); pgFresh = true; break;
        case 2: pgVal = (0 - pgVal) & pgMask(); pgFresh = true; break;
        case 3: pgVal /= pgBase; break;
        case 4: if (pgBase == 16 || pgBase == 2 || pgBase == 8 || pgBase == 10) { pgVal = (pgVal * pgBase * pgBase) & pgMask(); pgFresh = false; } break;
        case 5: if (pgBase == 16) { pgVal = ((pgVal << 8) | 0xFF) & pgMask(); pgFresh = false; } else pgVal = 0xFF & pgMask(); break;
        case 6: pgVal = 0; pgAcc = 0; pgOp = 0; pgFresh = true; break;
        case 7: if (pgOp) pgApply(); pgOp = 0; pgFresh = true; break;
        default: break;
    }
}
static void drawProgMode(const Theme& th) {
    rect(0, 0, W, UIH, CK_BG);
    calcModeBtn(12);
    static const int BASES[4] = {16, 10, 8, 2}; static const char* BN[4] = {"HEX", "DEC", "OCT", "BIN"};
    for (int i = 0; i < 4; i++) {
        int y = 62 + i * 48;
        rect(56, y, 540, 44, BASES[i] == pgBase ? Col{58, 62, 74} : CK_DISP);
        if (BASES[i] == pgBase) rect(56, y, 6, 44, th.accent);
        text(BN[i], F16, 76, y + 12, Col{170, 170, 180});
        std::string s = pgStr(pgVal, BASES[i], true); while (s.size() > 2 && tw(s, F22) > 420) s.erase(0, 1);
        text(s, F22, 580, y + 8, WHITE, 2);
    }
    static const char* WS[4] = {"QWORD", "DWORD", "WORD", "BYTE"}; static const int WB[4] = {64, 32, 16, 8};
    for (int i = 0; i < 4; i++) cbtn(56 + i * 136, 256, 130, 34, WS[i], WB[i] == pgBits ? th.accent : CK_OP, F16);
    // bit toggles (16 per row, high bits first)
    int rows = pgBits / 16 > 0 ? pgBits / 16 : 1; int perRow = pgBits < 16 ? pgBits : 16;
    for (int r = 0; r < rows; r++) for (int c = 0; c < perRow; c++) {
        int bit = pgBits - 1 - (r * perRow + c);
        int x = 630 + c * 37, y = 66 + r * 56;
        bool on = (pgVal >> bit) & 1ULL;
        rect(x, y + 14, 33, 36, on ? th.accent : CK_NUM);
        text(on ? "1" : "0", F22, x + 16, y + 20, WHITE, 1);
        if (c % 4 == 0) text(std::to_string(bit), F16, x + 1, y - 2, Col{120, 120, 130});
    }
    for (int r = 0; r < 5; r++) for (int c = 0; c < 8; c++) {
        const PgK& k = PGK[r * 8 + c]; if (!k.l[0]) continue;
        Col col = k.kind == 0 ? CK_NUM : CK_OP; if (!strcmp(k.l, "=")) col = th.accent; else if (!strcmp(k.l, "AC")) col = Col{170, 50, 50};
        bool dis = k.kind == 0 && k.val >= pgBase;
        cbtn(PG_X + c * (PG_CW + PG_GAP), PG_Y + r * (PG_CH + PG_GAP), PG_CW, PG_CH, k.l, dis ? CK_DIS : col, strlen(k.l) > 3 ? F22 : F30, dis ? Col{100, 100, 100} : WHITE);
    }
}
static bool tapProgMode(int x, int y) {
    static const int BASES[4] = {16, 10, 8, 2};
    for (int i = 0; i < 4; i++) if (inR(x, y, 56, 62 + i * 48, 540, 44)) { pgBase = BASES[i]; return true; }
    static const int WB[4] = {64, 32, 16, 8};
    for (int i = 0; i < 4; i++) if (inR(x, y, 56 + i * 136, 256, 130, 34)) { pgBits = WB[i]; pgVal &= pgMask(); return true; }
    int rows = pgBits / 16 > 0 ? pgBits / 16 : 1; int perRow = pgBits < 16 ? pgBits : 16;
    for (int r = 0; r < rows; r++) for (int c = 0; c < perRow; c++) { int bit = pgBits - 1 - (r * perRow + c); if (inR(x, y, 630 + c * 37, 66 + r * 56 + 14, 33, 36)) { pgVal ^= (1ULL << bit); pgFresh = false; return true; } }
    int c, r; if (inGrid(x, y, PG_X, PG_Y, PG_CW, PG_CH, PG_GAP, 8, 5, c, r)) { const PgK& k = PGK[r * 8 + c]; if (k.l[0]) pgPress(k); }
    return true;
}

// ================================================================ Statistics
static std::string stEntry; static int stScroll = 0;
static const char* ST_KEYS[15] = {"7", "8", "9", "4", "5", "6", "1", "2", "3", "0", ".", "+/-", "DEL", "Add", "Clear"};
static void stAdd() { if (stEntry.empty() || stEntry == "-" || stEntry == ".") return; gStat.push_back(atof(stEntry.c_str())); stEntry.clear(); calcSave(); }
static void statPress(const std::string& k) {
    if (k == "Add") stAdd();
    else if (k == "Clear") { gStat.clear(); stEntry.clear(); calcSave(); }
    else if (k == "DEL") { if (!stEntry.empty()) stEntry.pop_back(); }
    else if (k == "+/-") { if (!stEntry.empty() && stEntry[0] == '-') stEntry.erase(0, 1); else stEntry = "-" + stEntry; }
    else if (k == ".") { if (stEntry.find('.') == std::string::npos && stEntry.size() < 14) stEntry += stEntry.empty() || stEntry == "-" ? "0." : "."; }
    else if (stEntry.size() < 14) stEntry += k;
}
static void drawStatMode(const Theme& th) {
    rect(0, 0, W, UIH, CK_BG);
    calcModeBtn(12);
    rect(56, 62, 372, 66, CK_DISP); text(stEntry.empty() ? "enter a number" : stEntry, F44, 418, 74, WHITE, 2, stEntry.empty() ? 110 : 255);
    for (int i = 0; i < 15; i++) {
        int c = i % 3, r = i / 3; std::string l = ST_KEYS[i];
        Col col = l == "Add" ? th.accent : (l == "Clear" ? Col{150, 40, 40} : (l[0] >= '0' && l[0] <= '9') || l == "." ? CK_NUM : CK_OP);
        cbtn(56 + c * 126, 136 + r * 72, 120, 66, l, col, l.size() > 2 ? F22 : F30);
    }
    text("Data (" + std::to_string(gStat.size()) + ") - tap to remove", F16, 450, 62, Col{150, 150, 160});
    SDL_Rect clip{440, 86, 210, 566}; SDL_RenderSetClipRect(gR, &clip);
    int n = (int)gStat.size(); stScroll = std::max(0, std::min(stScroll, std::max(0, n * 38 - 566)));
    for (int i = 0; i < n; i++) { int y = 90 + i * 38 - stScroll; if (y + 34 < 86 || y > 654) continue; rect(444, y, 200, 34, CK_DISP); text(std::to_string(i + 1) + ".", F16, 454, y + 8, Col{130, 130, 140}); text(fitTxt(sciFmt(gStat[i]), F22, 150), F22, 636, y + 4, WHITE, 2); }
    SDL_RenderSetClipRect(gR, nullptr);
    // results
    double sum = 0, mn = 0, mx = 0, mean = 0, med = 0, var_p = 0, var_s = 0; std::string modeS = "none";
    if (n > 0) {
        std::vector<double> s = gStat; std::sort(s.begin(), s.end());
        for (double d : s) sum += d; mean = sum / n; mn = s[0]; mx = s[n - 1]; med = n % 2 ? s[n / 2] : (s[n / 2 - 1] + s[n / 2]) / 2;
        for (double d : s) var_p += (d - mean) * (d - mean); var_s = n > 1 ? var_p / (n - 1) : 0; var_p /= n;
        int best = 1, cnt = 1; double bv = s[0]; for (int i = 1; i <= n; i++) { if (i < n && s[i] == s[i - 1]) cnt++; else { if (cnt > best) { best = cnt; bv = s[i - 1]; } cnt = 1; } }
        if (best > 1) modeS = sciFmt(bv);
    }
    struct R { const char* l; std::string v; } rows[12] = {{"Count", std::to_string(n)}, {"Sum", sciFmt(sum)}, {"Mean", sciFmt(mean)}, {"Median", sciFmt(med)}, {"Mode", modeS}, {"Minimum", sciFmt(mn)}, {"Maximum", sciFmt(mx)}, {"Range", sciFmt(mx - mn)}, {"Variance (population)", sciFmt(var_p)}, {"Variance (sample)", sciFmt(var_s)}, {"Std dev (population)", sciFmt(sqrt(var_p))}, {"Std dev (sample)", sciFmt(sqrt(var_s))}};
    for (int i = 0; i < 12; i++) { int y = 62 + (i % 6) * 34, x = 680 + (i / 6) * 280; text(rows[i].l, F16, x, y, Col{150, 150, 160}); text(fitTxt(rows[i].v, F22, 270), F22, x, y + 17, WHITE); }
    // histogram
    rect(680, 278, 544, 372, CK_DISP); text("Histogram", F16, 692, 284, Col{150, 150, 160});
    if (n > 0) {
        int bins = std::min(10, std::max(3, (int)ceil(sqrt((double)n)) + 1)); std::vector<int> cnt(bins, 0); double span = mx - mn;
        for (double d : gStat) { int b = span <= 0 ? 0 : (int)((d - mn) / span * bins); if (b >= bins) b = bins - 1; cnt[b]++; }
        int top = *std::max_element(cnt.begin(), cnt.end()); int bw = 520 / bins;
        for (int i = 0; i < bins; i++) {
            int h = top ? (int)(250.0 * cnt[i] / top) : 0; int x = 692 + i * bw;
            rect(x + 2, 600 - h, bw - 4, h, th.accent); if (cnt[i]) text(std::to_string(cnt[i]), F16, x + bw / 2, 600 - h - 22, WHITE, 1);
            char b[24]; snprintf(b, sizeof b, "%.3g", mn + span * i / bins); if (bins <= 8 || i % 2 == 0) text(b, F16, x + bw / 2, 608, Col{130, 130, 140}, 1);
        }
    }
}
static bool tapStatMode(int x, int y) {
    int c, r; if (inGrid(x, y, 56, 136, 120, 66, 6, 3, 5, c, r)) { statPress(ST_KEYS[r * 3 + c]); return true; }
    if (x >= 440 && x < 650 && y >= 86) { int i = (y - 90 + stScroll) / 38; if (i >= 0 && i < (int)gStat.size()) { gStat.erase(gStat.begin() + i); calcSave(); } }
    return true;
}

// ================================================================ Date calculation
struct DDate { int y, m, d; };
static DDate gDt1 = {2026, 1, 1}, gDt2 = {2026, 12, 25}; static int gDtTab = 0, gDtAdd = 30; static DDate gDtBase = {2026, 1, 1}; static bool gDtInit = false, gDtSub = false;
static int dtDim(int y, int m) { static const int D[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31}; return m == 2 && ((y % 4 == 0 && y % 100) || y % 400 == 0) ? 29 : D[m - 1]; }
static void dtFix(DDate& d) { d.y = std::max(1601, std::min(9999, d.y)); d.m = std::max(1, std::min(12, d.m)); d.d = std::max(1, std::min(dtDim(d.y, d.m), d.d)); }
static long long dtDays(const DDate& d) { return wcDays(d.y, d.m, d.d); }
static DDate dtFrom(long long z) { DDate d; wcCivil(z, d.y, d.m, d.d); return d; }
static std::string dtStr(const DDate& d) { char b[64]; snprintf(b, sizeof b, "%s, %d %s %d", WD[dow(d.y, d.m, d.d)], d.d, MONTHS[d.m - 1], d.y); return b; }
static void dtPicker(int x, int y, const char* title, DDate& d) {
    text(title, F22, x, y, Col{170, 170, 180});
    const char* labs[3] = {"Day", "Month", "Year"}; int vals[3] = {d.d, d.m, d.y}; int ws[3] = {110, 150, 150};
    int cx = x;
    for (int i = 0; i < 3; i++) {
        cbtn(cx, y + 36, ws[i], 44, "+", CK_OP, F30);
        rect(cx, y + 84, ws[i], 60, CK_DISP); text(i == 1 ? std::string(MONTHS[d.m - 1]).substr(0, 3) : std::to_string(vals[i]), F44, cx + ws[i] / 2, y + 90, WHITE, 1);
        cbtn(cx, y + 148, ws[i], 44, "-", CK_OP, F30);
        text(labs[i], F16, cx + ws[i] / 2, y + 196, Col{130, 130, 140}, 1);
        cx += ws[i] + 14;
    }
}
static bool dtPickTap(int x, int y, int px, int py, DDate& d) {
    int ws[3] = {110, 150, 150}; int cx = px;
    for (int i = 0; i < 3; i++) {
        int dir = inR(x, y, cx, py + 36, ws[i], 44) ? 1 : (inR(x, y, cx, py + 148, ws[i], 44) ? -1 : 0);
        if (dir) { if (i == 0) { d.d += dir; if (d.d > dtDim(d.y, d.m)) d.d = 1; if (d.d < 1) d.d = dtDim(d.y, d.m); } else if (i == 1) { d.m += dir; if (d.m > 12) d.m = 1; if (d.m < 1) d.m = 12; } else d.y += dir; dtFix(d); return true; }
        cx += ws[i] + 14;
    }
    return false;
}
static void dtInit() { if (gDtInit) return; gDtInit = true; gDt1 = {gNow.y, gNow.mo, gNow.d}; gDt2 = gDt1; gDt2.m = 12; gDt2.d = 25; dtFix(gDt2); gDtBase = gDt1; }
static void drawDateMode(const Theme& th) {
    dtInit();
    rect(0, 0, W, UIH, CK_BG);
    calcModeBtn(12);
    cbtn(330, 12, 320, 40, "Difference between dates", gDtTab == 0 ? th.accent : Col{70, 70, 76}, F22);
    cbtn(660, 12, 300, 40, "Add or subtract days", gDtTab == 1 ? th.accent : Col{70, 70, 76}, F22);
    if (gDtTab == 0) {
        dtPicker(56, 80, "From", gDt1); dtPicker(640, 80, "To", gDt2);
        cbtn(56, 300, 150, 40, "Today", CK_OP, F22); cbtn(640, 300, 150, 40, "Today", CK_OP, F22);
        long long a = dtDays(gDt1), b = dtDays(gDt2); DDate lo = a <= b ? gDt1 : gDt2, hi = a <= b ? gDt2 : gDt1;
        long long dd = llabs(b - a);
        int yy = hi.y - lo.y, mm = hi.m - lo.m, ddd = hi.d - lo.d;
        if (ddd < 0) { mm--; int pm = hi.m - 1 < 1 ? 12 : hi.m - 1; int py = hi.m - 1 < 1 ? hi.y - 1 : hi.y; ddd += dtDim(py, pm); }
        if (mm < 0) { yy--; mm += 12; }
        rect(56, 360, 1168, 290, CK_DISP);
        text("Difference", F22, 80, 376, Col{170, 170, 180});
        if (dd == 0) text("Same date", F72, 80, 410, WHITE);
        else text(std::to_string(dd) + (dd == 1 ? " day" : " days"), F72, 80, 410, WHITE);
        std::string br; if (yy) br += std::to_string(yy) + (yy == 1 ? " year, " : " years, "); if (mm) br += std::to_string(mm) + (mm == 1 ? " month, " : " months, "); br += std::to_string(ddd) + (ddd == 1 ? " day" : " days");
        text(br, F30, 80, 510, WHITE);
        text(std::to_string(dd / 7) + " weeks and " + std::to_string(dd % 7) + " days", F22, 80, 560, Col{200, 200, 210});
        text(dtStr(gDt1) + "   to   " + dtStr(gDt2), F16, 80, 604, Col{150, 150, 160});
    } else {
        dtPicker(56, 80, "From", gDtBase);
        cbtn(56, 300, 150, 40, "Today", CK_OP, F22);
        cbtn(640, 90, 250, 56, gDtSub ? "Subtract" : "Add", th.accent, F30); cbtn(900, 90, 200, 56, "Switch", CK_OP, F22);
        rect(640, 160, 460, 80, CK_DISP); text(std::to_string(gDtAdd) + (gDtAdd == 1 ? " day" : " days"), F44, 870, 172, WHITE, 1);
        static const int ST[8] = {1, 7, 30, 365, -1, -7, -30, -365};
        for (int i = 0; i < 8; i++) { char b[16]; snprintf(b, sizeof b, "%+d", ST[i]); cbtn(640 + (i % 4) * 116, 250 + (i / 4) * 56, 110, 50, b, CK_OP, F22); }
        rect(56, 360, 1168, 290, CK_DISP);
        long long z = dtDays(gDtBase) + (gDtSub ? -gDtAdd : gDtAdd); DDate r = dtFrom(z);
        text("Result", F22, 80, 376, Col{170, 170, 180});
        text(dtStr(r), F44, 80, 420, WHITE);
        text("Starting from " + dtStr(gDtBase), F16, 80, 490, Col{150, 150, 160});
    }
}
static bool tapDateMode(int x, int y) {
    dtInit();
    if (inR(x, y, 330, 12, 320, 40)) { gDtTab = 0; return true; }
    if (inR(x, y, 660, 12, 300, 40)) { gDtTab = 1; return true; }
    DDate today = {gNow.y, gNow.mo, gNow.d};
    if (gDtTab == 0) {
        if (dtPickTap(x, y, 56, 80, gDt1) || dtPickTap(x, y, 640, 80, gDt2)) return true;
        if (inR(x, y, 56, 300, 150, 40)) gDt1 = today; else if (inR(x, y, 640, 300, 150, 40)) gDt2 = today;
    } else {
        if (dtPickTap(x, y, 56, 80, gDtBase)) return true;
        if (inR(x, y, 56, 300, 150, 40)) gDtBase = today;
        else if (inR(x, y, 640, 90, 250, 56) || inR(x, y, 900, 90, 200, 56)) gDtSub = !gDtSub;
        static const int ST[8] = {1, 7, 30, 365, -1, -7, -30, -365};
        for (int i = 0; i < 8; i++) if (inR(x, y, 640 + (i % 4) * 116, 250 + (i / 4) * 56, 110, 50)) { gDtAdd = std::max(0, std::min(100000, gDtAdd + ST[i])); return true; }
    }
    return true;
}

// ================================================================ entry points used by main.cpp
static void calcDrawMode(const Theme& th) {
    calcEnsure();
    switch (gCalcMode) {
        case 1: drawSciMode(th); break; case 2: drawGraphMode(th); break; case 3: drawProgMode(th); break;
        case 4: drawStatMode(th); break; case 5: drawDateMode(th); break; default: break;
    }
    calcMenuDraw();
}
static bool calcTapMode(int x, int y) {
    calcEnsure();
    if (gCalcMenu) {
        int y0 = gCalcBtnY + 44;
        for (int i = 0; i < 6; i++) if (inR(x, y, SCI_TX, y0 + i * 50, 300, 46)) { calcSetMode(i); return true; }
        gCalcMenu = false; return true;
    }
    if (inR(x, y, SCI_TX, gCalcBtnY, 250, 40)) { gCalcMenu = true; return true; }
    switch (gCalcMode) {
        case 1: return tapSciMode(x, y); case 2: return tapGraphMode(x, y); case 3: return tapProgMode(x, y);
        case 4: return tapStatMode(x, y); case 5: return tapDateMode(x, y); default: return false;
    }
}
static void calcDragMode(int dx, int dy) {
    if (gCalcMode == 2) { gGx -= dx / gGs; gGy += dy / gGs; }
    else if (gCalcMode == 1 && gCalcHist) gHistScroll = std::max(0, gHistScroll - dy);
    else if (gCalcMode == 4) stScroll = std::max(0, stScroll - dy);
}
static bool calcBack() {
    if (gCalcMenu) { gCalcMenu = false; return true; }
    if (gCalcHist) { gCalcHist = false; return true; }
    return false;
}
