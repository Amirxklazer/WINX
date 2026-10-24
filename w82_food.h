// w82_food.h - Food & Drink (Windows 8.1 Bing Food & Drink look): offline recipes, pantry search, shopping list, cook mode
// Shared small UI helpers (fd*) are also used by w82_health.h

// ---------------------------------------------------------------- shared helpers
static Col fdBg(int app) { return mix(ADEF[app].col, BLACK, 0.22f); }
static void fdPivot(const char* const* tabs, int n, int sel, int y) {
    int x = 70;
    for (int i = 0; i < n; i++) {
        int w = tw(tabs[i], F30);
        text(tabs[i], F30, x, y, WHITE, 0, i == sel ? 255 : 140);
        if (i == sel) rect(x, y + 40, w, 3, WHITE);
        x += w + 44;
    }
}
static int fdPivotHit(const char* const* tabs, int n, int px, int py, int y) {
    if (py < y - 8 || py > y + 52) return -1;
    int x = 70;
    for (int i = 0; i < n; i++) { int w = tw(tabs[i], F30); if (px >= x - 12 && px < x + w + 12) return i; x += w + 44; }
    return -1;
}
static std::string fdNum(float v) {   // 1.5 -> "1 1/2", quarters
    if (v >= 10.0f) { char b[24]; snprintf(b, sizeof b, "%d", (int)(v + 0.5f)); return b; }
    int q = (int)(v * 4 + 0.5f); if (q <= 0) q = 1;
    int whole = q / 4, fr = q % 4;
    std::string s; if (whole > 0 || fr == 0) s = std::to_string(whole);
    if (fr) { if (!s.empty()) s += " "; s += fr == 1 ? "1/4" : (fr == 2 ? "1/2" : "3/4"); }
    return s;
}
static int fdDateKey() { return gNow.y * 10000 + gNow.mo * 100 + gNow.d; }
static std::string fdMMSS(int s) { char b[16]; snprintf(b, sizeof b, "%d:%02d", s / 60, s % 60); return b; }
static void fdCircleBtn(int cx, int cy, int r, const char* label, Col bg, Col fg) { disc(cx, cy, r, bg); text(label, F30, cx, cy - 20, fg, 1); }

// ---------------------------------------------------------------- recipes (ingredients separated by ';', steps by '|', optional "@seconds" timer on a step)
struct FdRecipe { const char* name; int cat, prep, cook, serves; const char* blurb; const char* ing; const char* steps; };
static const FdRecipe FD_R[] = {
    // Breakfast (0)
    {"Chakchouka", 0, 10, 25, 2, "Eggs poached in a spicy pepper and tomato sauce, a North-African favourite.",
     "2 tbsp olive oil;1 onion, sliced;2 bell peppers, sliced;2 garlic cloves;4 tomatoes, chopped;1 tsp paprika;1/2 tsp cumin;1/2 tsp chilli flakes;eggs;salt to taste",
     "Heat the oil and soften the onion and peppers for 8 minutes.@480|Add garlic, tomatoes and the spices. Simmer until thick and saucy.@900|Make small wells in the sauce and crack an egg into each.|Cover and cook until the whites are set.@300|Serve hot with fresh bread."},
    {"Mhajeb", 0, 40, 20, 4, "Thin Algerian crepes folded around a spiced tomato and pepper filling.",
     "300 g fine semolina;100 g flour;1 tsp salt;200 ml warm water;2 tbsp oil;3 tomatoes, grated;1 onion, chopped;1 bell pepper, diced;2 garlic cloves;1 tsp paprika;1/2 tsp harissa",
     "Mix semolina, flour and salt with warm water into a soft dough. Rest it.@900|Cook the onion, pepper and garlic in oil, add tomatoes and spices, and reduce to a thick paste.@600|Stretch a ball of dough very thin on an oiled surface.|Spoon on filling, fold into a square.|Cook each square on a hot pan until golden on both sides.@180"},
    {"Fluffy Pancakes", 0, 10, 15, 4, "Tall, soft pancakes with a golden crust.",
     "200 g flour;2 tbsp sugar;2 tsp baking powder;1/4 tsp salt;250 ml milk;eggs;30 g melted butter;butter for the pan",
     "Whisk flour, sugar, baking powder and salt.|Mix milk, eggs and melted butter, then pour into the dry mix. Stir until just combined.|Rest the batter.@300|Cook ladlefuls on a buttered pan until bubbles form, flip and cook the other side.@180|Serve with honey or fruit."},
    {"Overnight Oats", 0, 5, 0, 1, "No cooking: prepare it tonight and eat it cold in the morning.",
     "50 g oats;120 ml milk;2 tbsp yogurt;1 tbsp honey;1 banana, sliced;1 tbsp chia seeds (optional)",
     "Stir oats, milk, yogurt, honey and chia in a jar.|Cover and chill overnight.|Top with banana and eat cold."},
    {"Scrambled Eggs on Toast", 0, 3, 5, 2, "Creamy scrambled eggs, ready in minutes.",
     "eggs;20 g butter;2 tbsp milk;salt and pepper;2 slices bread",
     "Beat the eggs with milk, salt and pepper.|Melt butter in a pan over low heat. Pour in the eggs.|Stir slowly until just set but still creamy.@120|Serve on hot buttered toast."},
    {"French Toast", 0, 5, 10, 2, "Eggy bread with cinnamon, crisp outside and soft inside.",
     "4 slices bread;eggs;100 ml milk;1 tbsp sugar;1/2 tsp cinnamon;butter for the pan;honey to serve",
     "Whisk eggs, milk, sugar and cinnamon in a shallow dish.|Soak each slice for 20 seconds per side.|Fry in butter until golden on both sides.@240|Serve with honey."},
    // Main dishes (1)
    {"Couscous with Chicken", 1, 30, 60, 4, "The Friday classic: steamed couscous with chicken, chickpeas and vegetables in a light broth.",
     "400 g couscous;500 g chicken pieces;1 onion, chopped;2 carrots, cut;2 zucchini, cut;1 turnip, cut;200 g chickpeas;2 tomatoes;2 tbsp tomato paste;2 tbsp oil;1 tsp ras el hanout;1 tsp turmeric;salt and pepper;1 tbsp butter",
     "Brown the chicken and onion in oil with the spices.|Add tomato paste, tomatoes, carrots, turnip, chickpeas and 1.5 litres of water. Simmer.@2400|Add the zucchini for the last 15 minutes.@900|Meanwhile moisten the couscous with salted water, rub with oil and steam it twice, raking with a fork.@1200|Pile the couscous on a dish, add butter, and serve with the chicken and broth."},
    {"Chorba Frik", 1, 20, 50, 4, "Algerian green wheat soup with lamb and fresh coriander, served at Ramadan.",
     "300 g lamb, cubed;100 g frik (green wheat);1 onion, chopped;2 tbsp tomato paste;2 tbsp chickpeas;1 tbsp oil;1 tsp cinnamon;1 tsp paprika;1 bunch fresh coriander;1 lemon;salt and pepper",
     "Brown the lamb and onion in oil with the spices.|Stir in the tomato paste and cover with 1.5 litres of water. Simmer until the meat softens.@1800|Add the frik and chickpeas and cook.@1200|Stir in chopped coriander and serve with lemon wedges."},
    {"Tajine Zitoune", 1, 20, 60, 4, "Chicken tagine with green olives and preserved lemon.",
     "4 chicken thighs;1 onion, grated;2 garlic cloves;100 g green olives;1 preserved lemon;1 tsp ginger;1 tsp turmeric;pinch saffron;2 tbsp oil;1 bunch parsley;salt and pepper",
     "Rub the chicken with onion, garlic, ginger, turmeric, saffron and oil. Marinate if you have time.|Cook the chicken in a covered pot over low heat with a splash of water.@2400|Add olives and sliced preserved lemon.@900|Finish with chopped parsley."},
    {"Spaghetti Bolognese", 1, 15, 40, 4, "A slow-simmered meat sauce over spaghetti.",
     "400 g spaghetti;300 g minced beef;1 onion, chopped;2 carrots, diced;2 garlic cloves;400 g chopped tomatoes;2 tbsp tomato paste;1 tsp oregano;2 tbsp olive oil;salt and pepper;grated cheese to serve",
     "Soften onion and carrot in oil, add garlic.@300|Add the beef and brown it, breaking it up.@300|Add tomatoes, tomato paste, oregano, salt and pepper. Simmer gently.@1800|Boil the spaghetti until al dente and drain.@600|Toss with the sauce and top with cheese."},
    {"Chicken Fried Rice", 1, 15, 15, 3, "A quick way to use leftover rice.",
     "300 g cooked rice, cold;200 g chicken, diced;eggs;1 carrot, diced;100 g peas;2 spring onions;2 tbsp soy sauce;2 tbsp oil;2 garlic cloves",
     "Fry the chicken in hot oil until cooked.@300|Add garlic, carrot and peas for 2 minutes.@120|Push aside, scramble the eggs in the pan.|Add the rice and soy sauce and stir-fry until hot.@180|Top with spring onions."},
    {"Lemon Grilled Chicken", 1, 10, 20, 4, "Juicy chicken breast marinated in lemon, garlic and herbs.",
     "4 chicken breasts;2 lemons, juiced;3 garlic cloves;3 tbsp olive oil;1 tsp oregano;1 tsp paprika;salt and pepper",
     "Mix lemon juice, garlic, oil and spices. Coat the chicken and marinate for at least 30 minutes.@1800|Grill or pan-fry on medium heat until cooked through.@900|Rest for a few minutes, then slice."},
    {"Chicken Shawarma Wraps", 1, 20, 15, 4, "Spiced chicken, garlic yogurt and salad in warm flatbread.",
     "500 g chicken thighs, sliced;4 flatbreads;1 tsp cumin;1 tsp paprika;1/2 tsp cinnamon;3 tbsp yogurt;2 garlic cloves;1 tomato;1 cucumber;2 tbsp oil;lemon juice",
     "Marinate the chicken with spices, half the yogurt, oil and lemon.@600|Fry in a hot pan until browned.@600|Mix the remaining yogurt with crushed garlic.|Fill the flatbreads with chicken, sliced tomato and cucumber and the garlic yogurt."},
    {"Baked Salmon", 1, 10, 18, 2, "Oven salmon with lemon and herbs.",
     "2 salmon fillets;1 lemon;2 tbsp olive oil;2 garlic cloves;1 tsp dill;salt and pepper",
     "Heat the oven to 200 C.|Place the fillets on a tray, brush with oil, garlic, dill, salt and pepper.|Top with lemon slices and bake.@1080|Serve with rice or salad."},
    // Vegetarian (2)
    {"Red Lentil Soup", 2, 10, 30, 4, "Warm, filling and cheap: red lentils with cumin and lemon.",
     "200 g red lentils;1 onion, chopped;2 carrots, chopped;2 garlic cloves;1 tsp cumin;1 tsp paprika;1 litre vegetable stock;2 tbsp olive oil;1 lemon;salt and pepper",
     "Soften onion, carrot and garlic in oil.@420|Add spices, lentils and stock. Simmer until the lentils collapse.@1500|Blend for a smooth soup if you like.|Season and add lemon juice."},
    {"Greek Salad", 2, 10, 0, 2, "Crisp vegetables with feta and olives.",
     "4 tomatoes;1 cucumber;1 red onion;100 g feta;12 olives;3 tbsp olive oil;1 tsp oregano;1 tbsp vinegar",
     "Chop the tomatoes and cucumber into chunks and slice the onion.|Add olives and feta on top.|Dress with oil, vinegar and oregano."},
    {"Falafel", 2, 20, 15, 4, "Crunchy chickpea fritters with herbs and spices.",
     "250 g dried chickpeas, soaked overnight;1 onion;3 garlic cloves;1 bunch parsley;1 bunch coriander;2 tsp cumin;1 tsp coriander seeds;1 tsp baking powder;oil for frying;salt",
     "Blitz chickpeas, onion, garlic and herbs into a coarse paste.|Stir in spices, salt and baking powder. Chill.@1200|Shape into small balls.|Deep fry until deep golden.@240|Serve in bread with salad and tahini."},
    {"Hummus", 2, 10, 0, 4, "Smooth chickpea dip.",
     "400 g cooked chickpeas;3 tbsp tahini;2 lemons, juiced;1 garlic clove;3 tbsp olive oil;1/2 tsp cumin;salt;ice water",
     "Blend tahini and lemon juice until pale.|Add chickpeas, garlic, cumin and salt and blend.|Add ice water a spoon at a time for a creamy texture.|Serve with olive oil and paprika."},
    {"Zaalouk", 2, 15, 30, 4, "Smoky Maghreb eggplant and tomato salad.",
     "2 eggplants;4 tomatoes;3 garlic cloves;2 tbsp olive oil;1 tsp paprika;1 tsp cumin;1 bunch coriander;lemon juice;salt",
     "Roast or boil the eggplants until soft, then peel and mash.@1200|Cook the tomatoes with garlic and spices until thick.@900|Add the eggplant and cook until jammy.@600|Finish with coriander and lemon."},
    {"Vegetable Stir-fry", 2, 10, 10, 2, "A rainbow of vegetables in a savoury sauce.",
     "1 broccoli head;2 carrots;1 bell pepper;100 g mushrooms;2 garlic cloves;2 tbsp soy sauce;1 tbsp honey;1 tsp ginger;2 tbsp oil",
     "Cut all vegetables into bite-size pieces.|Stir-fry carrots and broccoli in hot oil.@240|Add pepper, mushrooms, garlic and ginger.@180|Add soy sauce and honey and toss for a minute.@60"},
    {"Margherita Pizza", 2, 90, 12, 2, "Simple tomato, mozzarella and basil on homemade dough.",
     "300 g flour;1 tsp yeast;1 tsp salt;200 ml warm water;1 tbsp olive oil;150 g tomato sauce;150 g mozzarella;basil leaves",
     "Mix flour, yeast, salt, water and oil into a dough and knead.|Let it rise until doubled.@3600|Heat the oven to its highest setting.|Stretch the dough, spread the sauce and add cheese.|Bake until the crust is golden.@720|Top with basil."},
    // Desserts (3)
    {"Makroud", 3, 40, 20, 8, "Semolina diamonds filled with dates, fried and dipped in honey.",
     "500 g semolina;200 g butter, melted;1 tsp salt;200 ml orange blossom water;300 g date paste;1 tsp cinnamon;oil for frying;300 g honey",
     "Mix semolina, butter and salt, then moisten with orange blossom water into a firm dough. Rest.@1200|Roll into a log, press a groove along the middle and fill it with date paste mixed with cinnamon.|Close the dough around the filling and cut into diamonds.|Fry until golden.@600|Dip in warm honey and drain."},
    {"Chocolate Brownies", 3, 15, 25, 9, "Fudgy brownies with a crackly top.",
     "150 g dark chocolate;120 g butter;150 g sugar;eggs;80 g flour;30 g cocoa powder;pinch of salt",
     "Heat the oven to 180 C.|Melt chocolate and butter together and cool slightly.|Whisk in sugar and eggs, then fold in flour, cocoa and salt.|Pour into a lined tin and bake.@1500|Cool before cutting into squares."},
    {"Rice Pudding", 3, 5, 40, 4, "Creamy stovetop rice pudding with vanilla.",
     "100 g short grain rice;750 ml milk;3 tbsp sugar;1 tsp vanilla;1 cinnamon stick;cinnamon to dust",
     "Put rice, milk, sugar and the cinnamon stick in a pan.|Simmer on low heat, stirring often, until thick.@2400|Stir in vanilla and serve warm or cold with cinnamon."},
    {"Chocolate Chip Cookies", 3, 15, 12, 12, "Chewy in the middle, crisp at the edges.",
     "200 g flour;100 g butter, soft;100 g brown sugar;50 g sugar;eggs;1 tsp vanilla;1/2 tsp baking soda;150 g chocolate chips;pinch of salt",
     "Heat the oven to 180 C.|Cream butter and both sugars, then beat in the egg and vanilla.|Mix in flour, baking soda and salt, then the chocolate chips.|Drop spoonfuls on a tray and bake.@660|Cool on the tray for 5 minutes."},
    {"Basbousa", 3, 15, 35, 9, "Coconut semolina cake soaked in syrup.",
     "200 g semolina;100 g sugar;100 g coconut;120 g butter, melted;150 g yogurt;1 tsp baking powder;200 g sugar syrup;almonds",
     "Mix semolina, sugar, coconut and baking powder.|Stir in butter and yogurt.|Spread in a tin, cut into diamonds and put an almond on each.|Bake until golden.@2100|Pour cool syrup over the hot cake."},
    {"Fruit Salad", 3, 10, 0, 4, "Fresh fruit with lemon and mint.",
     "2 oranges;2 apples;1 banana;100 g strawberries;1 handful grapes;1 lemon, juiced;mint leaves;1 tbsp honey",
     "Chop all the fruit into bite-size pieces.|Toss with lemon juice and honey.|Chill and add torn mint before serving."},
    // Drinks (4)
    {"Mint Lemonade", 4, 10, 0, 4, "Cold, sharp and sweet.",
     "4 lemons, juiced;100 g sugar;1 litre cold water;1 handful mint;ice",
     "Dissolve the sugar in a little warm water.|Add lemon juice, cold water and bruised mint.|Chill and serve over ice."},
    {"Strawberry Smoothie", 4, 5, 0, 2, "A thick fruity smoothie.",
     "200 g strawberries;1 banana;150 ml milk;100 g yogurt;1 tbsp honey;ice",
     "Put everything in a blender.|Blend until smooth and pour into glasses."},
    {"Mango Lassi", 4, 5, 0, 2, "Creamy yogurt drink with mango.",
     "1 mango;250 g yogurt;100 ml milk;1 tbsp sugar;pinch of cardamom;ice",
     "Peel and chop the mango.|Blend all the ingredients until smooth.|Serve cold."},
    {"Virgin Mojito", 4, 5, 0, 2, "Lime, mint and fizz without alcohol.",
     "2 limes;1 handful mint;2 tbsp sugar;sparkling water;ice",
     "Muddle the mint, sugar and lime wedges in a glass.|Fill with ice and top with sparkling water.|Stir gently."},
    {"Banana Milkshake", 4, 5, 0, 2, "Classic and simple.",
     "2 bananas;300 ml cold milk;2 scoops ice cream;1 tsp honey",
     "Blend everything until smooth.|Serve immediately."},
    // Coffee & tea (5)
    {"Algerian Mint Tea", 5, 5, 5, 4, "Green tea with fresh mint and plenty of sugar, poured from a height.",
     "2 tsp green tea;1 handful fresh mint;4 tbsp sugar;750 ml boiling water",
     "Rinse the tea leaves quickly with a little hot water and pour it away.|Add mint, sugar and boiling water to the pot. Steep.@240|Pour into a glass and back into the pot a few times to mix.|Serve in small glasses."},
    {"Cafe Latte", 5, 3, 3, 1, "Espresso with plenty of steamed milk.",
     "1 shot espresso;200 ml milk;sugar to taste",
     "Brew the espresso into a cup.|Heat and froth the milk.|Pour the milk over the espresso."},
    {"Iced Coffee", 5, 5, 0, 1, "Cold brewed coffee over ice.",
     "2 shots strong coffee, cooled;150 ml milk;1 tbsp sugar;ice",
     "Stir sugar into the coffee.|Fill a glass with ice, add coffee and milk.|Stir and serve."},
    {"Hot Chocolate", 5, 3, 7, 2, "Thick and rich.",
     "500 ml milk;60 g dark chocolate;1 tbsp sugar;pinch of salt;1/2 tsp vanilla",
     "Heat the milk gently.|Whisk in chocolate, sugar and salt until melted.@300|Add vanilla and pour into mugs."},
    {"Cardamom Coffee", 5, 3, 7, 2, "Strong Arabic-style coffee with cardamom.",
     "2 tbsp finely ground coffee;2 cups water;2 cardamom pods, crushed;sugar to taste",
     "Bring the water and cardamom to a boil.|Add the coffee and sugar and let it rise once. Take off the heat.@60|Repeat twice more and pour carefully.@120"},
};
static const int FD_N = (int)(sizeof(FD_R) / sizeof(FD_R[0]));
static const char* FD_CATN[6] = {"Breakfast", "Main dishes", "Vegetarian", "Desserts", "Drinks", "Coffee & tea"};
static const Col FD_CATC[6] = {{232, 150, 30}, {196, 60, 40}, {70, 150, 50}, {190, 60, 130}, {30, 140, 200}, {130, 84, 54}};
static const char* FD_PANTRY[36] = {"eggs", "tomato", "onion", "garlic", "chicken", "beef", "rice", "pasta", "potato", "lentils", "chickpeas", "flour", "sugar", "milk", "butter", "yogurt", "cheese", "lemon", "mint", "olive oil",
                                    "pepper", "carrot", "eggplant", "semolina", "dates", "banana", "strawberr", "oats", "bread", "chocolate", "honey", "coffee", "tea", "cumin", "mushroom", "salmon"};

// ---------------------------------------------------------------- state
struct FdShop { std::string t; bool done = false; };
static int fdTab = 0, fdCat = -1, fdScroll = 0, fdSel = -1, fdServ = 0, fdStep = -1, fdTimerStep = -1;
static std::vector<int> fdFav;
static std::vector<FdShop> fdShop;
static bool fdPantry[36]; static bool fdLoaded = false;
static std::string fdQuery, fdAddText;
static float fdTimerLeft = 0; static bool fdTimerOn = false;
static const char* FD_TABS[5] = {"Featured", "Recipes", "Favourites", "Cook with", "Shopping list"};

static void fdSave() {
    std::string d;
    for (int f : fdFav) d += "F\t" + std::string(FD_R[f].name) + "\n";
    for (auto& s : fdShop) d += std::string("S\t") + (s.done ? "1" : "0") + "\t" + s.t + "\n";
    writeFile(DIR_ + "food.txt", d);
}
static int fdFindName(const std::string& n) { for (int i = 0; i < FD_N; i++) if (n == FD_R[i].name) return i; return -1; }
static void fdLoad() {
    fdLoaded = true; fdFav.clear(); fdShop.clear();
    std::string d; if (!readFile(DIR_ + "food.txt", d)) return;
    size_t p = 0;
    while (p < d.size()) {
        size_t e = d.find('\n', p); if (e == std::string::npos) e = d.size();
        std::string ln = d.substr(p, e - p); p = e + 1;
        if (ln.size() > 2 && ln[0] == 'F' && ln[1] == '\t') { int i = fdFindName(ln.substr(2)); if (i >= 0) fdFav.push_back(i); }
        else if (ln.size() > 4 && ln[0] == 'S' && ln[1] == '\t') { FdShop s; s.done = ln[2] == '1'; s.t = ln.substr(4); if (!s.t.empty()) fdShop.push_back(s); }
    }
}
static bool fdIsFav(int r) { return std::find(fdFav.begin(), fdFav.end(), r) != fdFav.end(); }
static void fdToggleFav(int r) { auto it = std::find(fdFav.begin(), fdFav.end(), r); if (it == fdFav.end()) fdFav.push_back(r); else fdFav.erase(it); fdSave(); }
static int fdOfDay() { return (int)((fdDateKey() * 2654435761u) >> 8) % FD_N; }
static std::vector<std::string> fdSplit(const char* s, char sep) {
    std::vector<std::string> v; std::string cur;
    for (const char* p = s; *p; p++) { if (*p == sep) { v.push_back(cur); cur.clear(); } else cur += *p; }
    if (!cur.empty()) v.push_back(cur);
    return v;
}
// scales the leading number of an ingredient line ("2 tbsp oil" -> "4 tbsp oil")
static std::string fdScaleIng(const std::string& s, float k) {
    if (s.empty() || !(isdigit((unsigned char)s[0]))) return s;
    size_t e = 0; while (e < s.size() && (isdigit((unsigned char)s[e]) || s[e] == '/' || s[e] == '.')) e++;
    std::string n = s.substr(0, e); float v;
    size_t sl = n.find('/'); if (sl != std::string::npos) v = (float)atof(n.substr(0, sl).c_str()) / std::max(1.0f, (float)atof(n.substr(sl + 1).c_str())); else v = (float)atof(n.c_str());
    return fdNum(v * k) + s.substr(e);
}
static std::string fdIngNoQty(const std::string& s) {
    size_t i = 0; while (i < s.size() && (isdigit((unsigned char)s[i]) || s[i] == '/' || s[i] == '.')) i++;
    while (i < s.size() && s[i] == ' ') i++;
    return s.substr(i);
}
static int fdStepSecs(const std::string& st, std::string& txt) {
    size_t a = st.rfind('@'); if (a == std::string::npos) { txt = st; return 0; }
    txt = st.substr(0, a); return atoi(st.c_str() + a + 1);
}
static bool fdMatches(int r, const std::string& q) {
    if (q.empty()) return true;
    std::string ql = lower(q);
    if (lower(FD_R[r].name).find(ql) != std::string::npos) return true;
    return lower(FD_R[r].ing).find(ql) != std::string::npos;
}
static int fdPantryScore(int r, int& total) {
    auto ing = fdSplit(FD_R[r].ing, ';'); total = (int)ing.size(); int m = 0;
    for (auto& s : ing) { std::string l = lower(s); for (int k = 0; k < 36; k++) if (fdPantry[k] && l.find(FD_PANTRY[k]) != std::string::npos) { m++; break; } }
    return m;
}

// ---------------------------------------------------------------- procedural dish art
static Col fdArtBase(int r) {
    static const Col P[10] = {{214, 120, 60}, {190, 70, 60}, {110, 160, 70}, {200, 150, 70}, {220, 90, 110}, {90, 150, 190}, {160, 110, 70}, {230, 170, 60}, {120, 90, 150}, {80, 160, 140}};
    return P[(r * 7 + FD_R[r].cat * 3) % 10];
}
static void fdArt(int x, int y, int w, int h, int r) {
    Col base = fdArtBase(r), bot = mix(base, BLACK, 0.45f);
    for (int k = 0; k < h; k += 4) rect(x, y + k, w, std::min(4, h - k), mix(base, bot, (float)k / (float)h));
    int cx = x + w / 2, cy = y + h / 2, sz = std::min(w, h);
    unsigned s = (unsigned)(r * 2654435761u + 12345);
    auto rnd = [&]() { s = s * 1664525u + 1013904223u; return (int)((s >> 16) & 0x7fff); };
    int cat = FD_R[r].cat;
    SDL_Rect clip = {x, y, w, h}; SDL_Rect old; SDL_RenderGetClipRect(gR, &old); SDL_RenderSetClipRect(gR, &clip);
    if (cat == 4) {   // glass with a drink
        int gw = sz * 36 / 100, gh = sz * 62 / 100; Col liq = mix(base, WHITE, 0.25f);
        quad((float)(cx - gw), (float)(cy - gh / 2), (float)(cx + gw), (float)(cy - gh / 2), (float)(cx + gw * 7 / 10), (float)(cy + gh / 2), (float)(cx - gw * 7 / 10), (float)(cy + gh / 2), WHITE, 70);
        quad((float)(cx - gw * 9 / 10), (float)(cy - gh / 5), (float)(cx + gw * 9 / 10), (float)(cy - gh / 5), (float)(cx + gw * 66 / 100), (float)(cy + gh / 2 - 4), (float)(cx - gw * 66 / 100), (float)(cy + gh / 2 - 4), liq, 235);
        line(cx + gw / 3, cy - gh / 2 - 22, cx + gw / 2 + 8, cy + gh / 8, 6, mix(base, WHITE, 0.7f));
        for (int i = 0; i < 4; i++) disc(cx - gw / 2 + (rnd() % std::max(1, gw)), cy - gh / 8 + (rnd() % std::max(1, gh / 3)), 4 + rnd() % 4, WHITE, 120);
    } else if (cat == 5) {   // cup with steam
        int cw = sz * 30 / 100, ch = sz * 30 / 100;
        rect(cx - cw, cy - ch / 4, cw * 2, ch, WHITE); disc(cx, cy + ch * 3 / 4, cw, WHITE); rect(cx - cw, cy - ch / 4, cw * 2, 12, mix(base, BLACK, 0.55f));
        ring(cx + cw + 6, cy + ch / 4, ch / 3, 7, WHITE, base);
        for (int i = 0; i < 3; i++) line(cx - cw / 2 + i * cw / 2, cy - ch, cx - cw / 2 + i * cw / 2 + 8, cy - ch / 2, 4, WHITE, 120);
        rect(cx - cw - 20, cy + ch + 8, cw * 2 + 40, 8, WHITE, 200);
    } else {   // plate with food
        int pr = sz * 40 / 100;
        disc(cx + 6, cy + 8, pr, BLACK, 60); disc(cx, cy, pr, Col{240, 240, 240}); disc(cx, cy, pr * 84 / 100, Col{252, 252, 252});
        Col f1 = mix(base, WHITE, 0.15f); disc(cx, cy, pr * 62 / 100, f1);
        for (int i = 0; i < 9; i++) { int a = rnd() % 360; float rr = (rnd() % 100) / 100.0f * pr * 0.5f; Col c = (i % 3 == 0) ? Col{70, 150, 60} : (i % 3 == 1 ? mix(base, BLACK, 0.3f) : Col{250, 210, 90}); disc(cx + (int)(cosf(a * 0.01745f) * rr), cy + (int)(sinf(a * 0.01745f) * rr), 6 + rnd() % 10, c); }
        if (cat == 3) { for (int i = 0; i < 6; i++) disc(cx - pr / 3 + rnd() % std::max(1, pr * 2 / 3), cy - pr / 3 + rnd() % std::max(1, pr * 2 / 3), 4, Col{255, 240, 220}); }
    }
    SDL_RenderSetClipRect(gR, old.w > 0 ? &old : nullptr);
}

// ---------------------------------------------------------------- layout constants shared by draw and tap
static const int FD_GX = 50, FD_GW = 282, FD_GH = 196, FD_GGAP = 20, FD_GY = 232;
static std::vector<int> fdGridList() {
    std::vector<int> v;
    for (int i = 0; i < FD_N; i++) {
        if (fdTab == 2 && !fdIsFav(i)) continue;
        if (fdTab == 1 && fdCat >= 0 && FD_R[i].cat != fdCat) continue;
        if (fdTab == 1 && !fdMatches(i, fdQuery)) continue;
        v.push_back(i);
    }
    return v;
}
static std::vector<std::pair<int, std::pair<int, int>>> fdPantryList() {   // (recipe, (matched,total)) best first
    std::vector<std::pair<int, std::pair<int, int>>> v;
    for (int i = 0; i < FD_N; i++) { int t; int m = fdPantryScore(i, t); if (m > 0) v.push_back({i, {m, t}}); }
    std::sort(v.begin(), v.end(), [](const auto& a, const auto& b) { float ra = (float)a.second.first / a.second.second, rb = (float)b.second.first / b.second.second; if (ra != rb) return ra > rb; return a.second.first > b.second.first; });
    return v;
}
static void fdOpen(int r) { fdSel = r; fdServ = FD_R[r].serves; fdScroll = 0; fdStep = -1; }
static void fdAddToShop(int r) {
    auto ing = fdSplit(FD_R[r].ing, ';'); float k = (float)fdServ / (float)FD_R[r].serves; int added = 0;
    for (auto& s : ing) {
        std::string line = fdScaleIng(s, k); bool dup = false;
        for (auto& x : fdShop) if (lower(fdIngNoQty(x.t)) == lower(fdIngNoQty(line))) { dup = true; break; }
        if (!dup) { FdShop it; it.t = line; fdShop.push_back(it); added++; }
    }
    fdSave(); toast(added ? std::to_string(added) + " items added to your shopping list" : "Everything is already on your list");
}

// ---------------------------------------------------------------- drawing
static void fdCard(int x, int y, int w, int h, int r) {
    fdArt(x, y, w, h - 52, r);
    rect(x, y + h - 52, w, 52, Col{250, 250, 250});
    text(fitTxt(FD_R[r].name, F22, w - 20), F22, x + 10, y + h - 46, Col{40, 40, 44});
    char b[48]; snprintf(b, sizeof b, "%d min", FD_R[r].prep + FD_R[r].cook); text(b, F16, x + 10, y + h - 22, Col{110, 110, 118});
    if (fdIsFav(r)) text("*", F30, x + w - 24, y + h - 48, Col{240, 170, 20}, 1);
}
static void fdDrawDetail() {
    const FdRecipe& R = FD_R[fdSel]; Col bg = fdBg(A_FOOD);
    rect(0, 0, W, UIH, bg);
    text(fitTxt(R.name, F44, 1100), F44, 70, 22, WHITE);
    fdArt(70, 86, 380, 220, fdSel);
    char b[96]; snprintf(b, sizeof b, "Prep %d min   Cook %d min   %s", R.prep, R.cook, FD_CATN[R.cat]); text(b, F16, 70, 314, WHITE, 0, 210);
    text("Servings", F22, 70, 348, WHITE);
    fdCircleBtn(250, 362, 24, "-", Col{255, 255, 255}, bg); text(std::to_string(fdServ), F30, 306, 346, WHITE, 1); fdCircleBtn(362, 362, 24, "+", Col{255, 255, 255}, bg);
    btn(70, 410, 380, 52, "Start cooking", Col{20, 150, 70}, WHITE);
    btn(70, 472, 380, 52, "Add to shopping list", Col{255, 255, 255}, bg);
    btn(70, 534, 380, 52, fdIsFav(fdSel) ? "Remove from favourites" : "Add to favourites", Col{255, 255, 255}, bg);
    SDL_Rect clip = {480, 80, W - 480, UIH - 80}; SDL_RenderSetClipRect(gR, &clip);
    int y = 90 - fdScroll; float k = (float)fdServ / (float)R.serves;
    text("Ingredients", F30, 500, y, WHITE); y += 46;
    for (auto& s : fdSplit(R.ing, ';')) { disc(510, y + 14, 4, WHITE); text(fitTxt(fdScaleIng(s, k), F22, 700), F22, 530, y, WHITE); y += 32; }
    y += 14; text("Method", F30, 500, y, WHITE); y += 46;
    int n = 1;
    for (auto& s : fdSplit(R.steps, '|')) {
        std::string t; int secs = fdStepSecs(s, t);
        disc(512, y + 16, 16, WHITE); text(std::to_string(n), F22, 512, y + 4, bg, 1);
        auto L = wrapText(t, F22, 680);
        for (size_t i = 0; i < L.size(); i++) { text(L[i], F22, 546, y + (int)i * 30, WHITE); }
        int hh = (int)L.size() * 30; if (secs) { text("Timer: " + fdMMSS(secs), F16, 546, y + hh, WHITE, 0, 170); hh += 22; }
        y += hh + 14; n++;
    }
    fdScroll = std::max(0, std::min(fdScroll, std::max(0, y + fdScroll - (UIH - 20))));
    SDL_RenderSetClipRect(gR, nullptr);
}
static void fdDrawCook() {
    const FdRecipe& R = FD_R[fdSel]; auto steps = fdSplit(R.steps, '|'); int n = (int)steps.size();
    rect(0, 0, W, UIH, Col{20, 22, 26});
    text(R.name, F30, 70, 30, WHITE, 0, 200);
    text("Step " + std::to_string(fdStep + 1) + " of " + std::to_string(n), F44, 70, 70, Col{255, 160, 60});
    btn(W - 190, 30, 120, 50, "Close", Col{70, 70, 78}, WHITE);
    std::string t; int secs = fdStepSecs(steps[fdStep], t);
    int y = 160; for (auto& l : wrapText(t, F44, 1130)) { text(l, F44, 70, y, WHITE); y += 56; if (y > 420) break; }
    if (secs) {
        bool mine = fdTimerOn && fdTimerStep == fdStep; int left = mine ? (int)ceilf(fdTimerLeft) : secs;
        btn(W / 2 - 160, 500, 320, 64, mine ? ("Stop  " + fdMMSS(left)).c_str() : ("Start timer  " + fdMMSS(secs)).c_str(), mine ? Col{200, 60, 50} : Col{20, 150, 70}, WHITE);
    }
    rect(70, 596, W - 140, 6, Col{60, 62, 70}); rect(70, 596, (W - 140) * (fdStep + 1) / n, 6, Col{255, 160, 60});
    btn(70, 604, 200, 52, "Previous", fdStep > 0 ? Col{70, 70, 78} : Col{40, 40, 46}, WHITE);
    btn(W - 270, 604, 200, 52, fdStep + 1 < n ? "Next" : "Done", Col{20, 150, 70}, WHITE);
}
static void drawFood() {
    if (!fdLoaded) fdLoad();
    if (fdSel >= 0 && fdStep >= 0) { fdDrawCook(); return; }
    if (fdSel >= 0) { fdDrawDetail(); return; }
    Col bg = fdBg(A_FOOD);
    rect(0, 0, W, UIH, bg);
    appTitle("Food & Drink");
    fdPivot(FD_TABS, 5, fdTab, 116);
    if (fdTab == 0) {
        int r = fdOfDay();
        fdArt(70, 176, 560, 290, r); rect(70, 410, 560, 56, BLACK, 140);
        text("Recipe of the day", F16, 86, 414, WHITE, 0, 210); text(fitTxt(FD_R[r].name, F30, 530), F30, 86, 428, WHITE);
        auto L = wrapText(FD_R[r].blurb, F22, 560); for (size_t i = 0; i < L.size() && i < 2; i++) text(L[i], F22, 70, 480 + (int)i * 30, WHITE, 0, 220);
        char b[64]; snprintf(b, sizeof b, "%d min  -  serves %d", FD_R[r].prep + FD_R[r].cook, FD_R[r].serves); text(b, F16, 70, 548, WHITE, 0, 190);
        text("Browse", F30, 670, 176, WHITE);
        for (int i = 0; i < 6; i++) { int x = 670 + (i % 3) * 190, y = 222 + (i / 3) * 130; rect(x, y, 180, 120, FD_CATC[i]); text(FD_CATN[i], F22, x + 12, y + 8, WHITE); int cnt = 0; for (int q = 0; q < FD_N; q++) if (FD_R[q].cat == i) cnt++; text(std::to_string(cnt) + " recipes", F16, x + 12, y + 92, WHITE, 0, 210); }
        rect(670, 492, 275, 120, Col{232, 120, 20}); text("Cook with what", F22, 684, 502, WHITE); text("you have", F22, 684, 530, WHITE);
        rect(955, 492, 275, 120, Col{0, 138, 0}); text("Shopping list", F22, 969, 502, WHITE); text(std::to_string(fdShop.size()) + " items", F22, 969, 530, WHITE, 0, 220);
    } else if (fdTab == 1 || fdTab == 2) {
        int x = 70;
        if (fdTab == 1) {
            for (int i = -1; i < 6; i++) { const char* nm = i < 0 ? "All" : FD_CATN[i]; int w = tw(nm, F16) + 28; rect(x, 176, w, 44, i == fdCat ? WHITE : Col{255, 255, 255}, i == fdCat ? 255 : 50); text(nm, F16, x + w / 2, 188, i == fdCat ? bg : WHITE, 1); x += w + 8; if (x > 900) break; }
            drawField(x + 4 > 860 ? 860 : x + 4, 176, 1230 - (x + 4 > 860 ? 860 : x + 4), 44, &fdQuery, "Search recipes or ingredients", F16, Col{30, 30, 34}, Col{245, 245, 245}, Col{200, 200, 205});
        } else text("Your favourite recipes. Tap the star in a recipe to add it here.", F22, 70, 180, WHITE, 0, 200);
        auto L = fdGridList(); int rows = ((int)L.size() + 3) / 4;
        SDL_Rect clip = {0, FD_GY, W, UIH - FD_GY}; SDL_RenderSetClipRect(gR, &clip);
        fdScroll = std::max(0, std::min(fdScroll, std::max(0, rows * (FD_GH + FD_GGAP) - (UIH - FD_GY) + 10)));
        for (size_t i = 0; i < L.size(); i++) { int cx = FD_GX + (int)(i % 4) * (FD_GW + FD_GGAP), cy = FD_GY + (int)(i / 4) * (FD_GH + FD_GGAP) - fdScroll; if (cy + FD_GH < FD_GY || cy > UIH) continue; fdCard(cx, cy, FD_GW, FD_GH, L[i]); }
        SDL_RenderSetClipRect(gR, nullptr);
        if (L.empty()) text(fdTab == 2 ? "No favourites yet." : "No recipes match your search.", F30, 70, 250, WHITE, 0, 200);
    } else if (fdTab == 3) {
        text("Tap what is in your kitchen", F22, 70, 176, WHITE, 0, 210);
        for (int i = 0; i < 36; i++) { int x = 70 + (i % 4) * 180, y = 216 + (i / 4) * 49; rect(x, y, 170, 42, fdPantry[i] ? WHITE : Col{255, 255, 255}, fdPantry[i] ? 255 : 45); std::string nm = FD_PANTRY[i]; if (nm == "strawberr") nm = "strawberry"; text(nm, F16, x + 85, y + 10, fdPantry[i] ? bg : WHITE, 1); }
        text("You can make", F30, 830, 176, WHITE);
        auto L = fdPantryList(); bool any = false; for (int i = 0; i < 36; i++) any = any || fdPantry[i];
        for (size_t i = 0; i < L.size() && i < 8; i++) { int y = 222 + (int)i * 54; rect(830, y, 400, 48, WHITE, 40); text(fitTxt(FD_R[L[i].first].name, F22, 290), F22, 842, y + 9, WHITE); char b[24]; snprintf(b, sizeof b, "%d/%d", L[i].second.first, L[i].second.second); text(b, F22, 1218, y + 9, WHITE, 2, 210); }
        if (!any) text("Pick some ingredients.", F22, 830, 230, WHITE, 0, 180);
        btn(70, 616 > 650 ? 616 : 622, 170, 36, "Clear", Col{70, 70, 78}, WHITE);
    } else {
        drawField(70, 176, 700, 48, &fdAddText, "Add an item", F22, Col{30, 30, 34}, Col{245, 245, 245}, Col{200, 200, 205});
        btn(790, 176, 150, 48, "Add", Col{20, 150, 70}, WHITE); btn(960, 176, 130, 48, "Clear done", Col{70, 70, 78}, WHITE); btn(1100, 176, 130, 48, "Clear all", Col{150, 40, 40}, WHITE);
        SDL_Rect clip = {0, 238, W, UIH - 238}; SDL_RenderSetClipRect(gR, &clip);
        fdScroll = std::max(0, std::min(fdScroll, std::max(0, (int)fdShop.size() * 56 - (UIH - 250))));
        for (size_t i = 0; i < fdShop.size(); i++) {
            int y = 244 + (int)i * 56 - fdScroll; if (y + 52 < 238 || y > UIH) continue;
            rect(70, y, 1160, 50, WHITE, 38); rect(84, y + 9, 32, 32, WHITE, fdShop[i].done ? 255 : 70); if (fdShop[i].done) { line(89, y + 26, 98, y + 36, 4, bg); line(98, y + 36, 112, y + 14, 4, bg); }
            text(fitTxt(fdShop[i].t, F22, 980), F22, 132, y + 10, WHITE, 0, fdShop[i].done ? 130 : 255); text("x", F30, 1204, y + 6, WHITE, 1, 200);
        }
        SDL_RenderSetClipRect(gR, nullptr);
        if (fdShop.empty()) text("Your shopping list is empty. Add ingredients from any recipe.", F22, 70, 260, WHITE, 0, 200);
    }
}

// ---------------------------------------------------------------- input
static void fdAddItem() { if (!fdAddText.empty()) { FdShop s; s.t = fdAddText; fdShop.push_back(s); fdAddText.clear(); fdSave(); } }
static void foodInit() { if (!fdLoaded) fdLoad(); fdSel = -1; fdStep = -1; fdScroll = 0; }
static void foodTap(int x, int y) {
    if (!fdLoaded) fdLoad();
    if (fdSel >= 0 && fdStep >= 0) {   // cook mode
        auto steps = fdSplit(FD_R[fdSel].steps, '|'); int n = (int)steps.size(); std::string t; int secs = fdStepSecs(steps[fdStep], t);
        if (inR(x, y, W - 190, 30, 120, 50)) { fdStep = -1; return; }
        if (inR(x, y, 70, 604, 200, 52) && fdStep > 0) fdStep--;
        else if (inR(x, y, W - 270, 604, 200, 52)) { if (fdStep + 1 < n) fdStep++; else { fdStep = -1; sndPlay(SND_NOTIFY); toast("Enjoy your meal!"); } }
        else if (secs && inR(x, y, W / 2 - 160, 500, 320, 64)) { if (fdTimerOn && fdTimerStep == fdStep) fdTimerOn = false; else { fdTimerOn = true; fdTimerStep = fdStep; fdTimerLeft = (float)secs; } }
        return;
    }
    if (fdSel >= 0) {
        if (inR(x, y, 226, 338, 48, 48)) fdServ = std::max(1, fdServ - 1);
        else if (inR(x, y, 338, 338, 48, 48)) fdServ = std::min(24, fdServ + 1);
        else if (inR(x, y, 70, 410, 380, 52)) fdStep = 0;
        else if (inR(x, y, 70, 472, 380, 52)) fdAddToShop(fdSel);
        else if (inR(x, y, 70, 534, 380, 52)) fdToggleFav(fdSel);
        return;
    }
    int p = fdPivotHit(FD_TABS, 5, x, y, 116); if (p >= 0) { if (p != fdTab) { fdTab = p; fdScroll = 0; } return; }
    if (fdTab == 0) {
        if (inR(x, y, 70, 176, 560, 290)) fdOpen(fdOfDay());
        for (int i = 0; i < 6; i++) if (inR(x, y, 670 + (i % 3) * 190, 222 + (i / 3) * 130, 180, 120)) { fdTab = 1; fdCat = i; fdScroll = 0; fdQuery.clear(); return; }
        if (inR(x, y, 670, 492, 275, 120)) { fdTab = 3; fdScroll = 0; } else if (inR(x, y, 955, 492, 275, 120)) { fdTab = 4; fdScroll = 0; }
    } else if (fdTab == 1 || fdTab == 2) {
        if (fdTab == 1 && y >= 176 && y < 222) {
            int cx = 70;
            for (int i = -1; i < 6; i++) { const char* nm = i < 0 ? "All" : FD_CATN[i]; int w = tw(nm, F16) + 28; if (inR(x, y, cx, 176, w, 44)) { fdCat = i; fdScroll = 0; return; } cx += w + 8; if (cx > 900) break; }
            int fx = cx + 4 > 860 ? 860 : cx + 4; if (x >= fx) openKbStr(&fdQuery, 40);
            return;
        }
        if (y < FD_GY) return;
        auto L = fdGridList();
        for (size_t i = 0; i < L.size(); i++) { int cx = FD_GX + (int)(i % 4) * (FD_GW + FD_GGAP), cy = FD_GY + (int)(i / 4) * (FD_GH + FD_GGAP) - fdScroll; if (inR(x, y, cx, cy, FD_GW, FD_GH)) { fdOpen(L[i]); return; } }
    } else if (fdTab == 3) {
        for (int i = 0; i < 36; i++) if (inR(x, y, 70 + (i % 4) * 180, 216 + (i / 4) * 49, 170, 42)) { fdPantry[i] = !fdPantry[i]; return; }
        if (inR(x, y, 70, 622, 170, 36)) { for (int i = 0; i < 36; i++) fdPantry[i] = false; return; }
        auto L = fdPantryList(); for (size_t i = 0; i < L.size() && i < 8; i++) if (inR(x, y, 830, 222 + (int)i * 54, 400, 48)) { fdOpen(L[i].first); return; }
    } else {
        if (inR(x, y, 70, 176, 700, 48)) { openKbStr(&fdAddText, 60, fdAddItem); return; }
        if (inR(x, y, 790, 176, 150, 48)) { fdAddItem(); return; }
        if (inR(x, y, 960, 176, 130, 48)) { fdShop.erase(std::remove_if(fdShop.begin(), fdShop.end(), [](const FdShop& s) { return s.done; }), fdShop.end()); fdSave(); return; }
        if (inR(x, y, 1100, 176, 130, 48)) { fdShop.clear(); fdSave(); return; }
        for (size_t i = 0; i < fdShop.size(); i++) { int yy = 244 + (int)i * 56 - fdScroll; if (y < 238 || !inR(x, y, 70, yy, 1160, 50)) continue; if (x > 1170) fdShop.erase(fdShop.begin() + i); else fdShop[i].done = !fdShop[i].done; fdSave(); return; }
    }
}
static void foodDrag(int, int dy) { fdScroll = std::max(0, fdScroll - dy); }
static bool foodBack() {
    if (fdSel >= 0 && fdStep >= 0) { fdStep = -1; return true; }
    if (fdSel >= 0) { fdSel = -1; fdScroll = 0; return true; }
    if (fdTab != 0) { fdTab = 0; fdScroll = 0; return true; }
    return false;
}
static void foodUpdate(float dt) {
    if (fdTimerOn) {
        fdTimerLeft -= dt;
        if (fdTimerLeft <= 0) { fdTimerOn = false; sndPlay(SND_RING); notify(A_FOOD, "Timer done", fdSel >= 0 ? std::string(FD_R[fdSel].name) + ": step " + std::to_string(fdTimerStep + 1) + " is ready" : "Your cooking timer finished"); toast("Cooking timer finished"); }
    }
}
static bool foodLive(const Tile& t, SDL_Rect r, bool& all) {
    int rc = fdOfDay();
    if (r.w < 150) { fdArt(r.x, r.y, r.w, r.h, rc); all = true; return true; }
    fdArt(r.x, r.y, r.w, r.h, rc);
    rect(r.x, r.y + r.h - 54, r.w, 54, BLACK, 140);
    text("Recipe of the day", F16, r.x + 10, r.y + r.h - 50, WHITE, 0, 200); text(fitTxt(FD_R[rc].name, F22, r.w - 20), F22, r.x + 10, r.y + r.h - 30, WHITE);
    (void)t; all = true; return true;
}
static std::string foodStatus() {
    if (!fdLoaded) fdLoad();
    int rc = fdOfDay(); std::string s = std::string("Today's recipe idea is ") + FD_R[rc].name + " (" + std::to_string(FD_R[rc].prep + FD_R[rc].cook) + " min).";
    int left = 0; for (auto& i : fdShop) if (!i.done) left++;
    if (left) s += " You have " + std::to_string(left) + (left == 1 ? " item" : " items") + " on your shopping list.";
    return s;
}
