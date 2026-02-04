#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <algorithm>
#include <cctype>
#include <ctime>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#ifdef __EMSCRIPTEN__
#include <emscripten/emscripten.h>
#endif

using namespace std;

static const int W = 800;
static const int H = 600;

static const int PADDLE_WIDTH = 140;
static const int PADDLE_HEIGHT = 12;

static const int LETTER_SIZE = 28;
static const int INITIAL_SPEED = 2;      // <- antes 5 (más lento al inicio)
static const float SPEED_ACCEL = 0.005f; // <- antes 0.01f (acelera más suave)

static const int BUTTON_WIDTH = 170;
static const int BUTTON_HEIGHT = 52;

static SDL_Color WHT = {255, 255, 255, 255};
static SDL_Color BLU = {0, 200, 255, 255};
static SDL_Color RED = {255, 0, 0, 255};
static SDL_Color BLK = {0, 0, 0, 255};
static SDL_Color GRN = {0, 255, 0, 255};
static SDL_Color YLW = {255, 220, 0, 255};

static SDL_Window* window = nullptr;
static SDL_Renderer* renderer = nullptr;
static TTF_Font* font = nullptr;

static string trim(const string& s) {
    size_t a = 0;
    while (a < s.size() && isspace(static_cast<unsigned char>(s[a]))) a++;
    size_t b = s.size();
    while (b > a && isspace(static_cast<unsigned char>(s[b - 1]))) b--;
    return s.substr(a, b - a);
}

// Envuelve texto por ANCHO EN PIXELES (no por cantidad de chars).
static vector<string> splitLinesWrapPixels(const string& text, int maxWidthPx) {
    vector<string> out;
    if (text.empty()) return out;

    // Fallback si no hay fuente cargada: usa un wrap "por caracteres".
    if (!font) {
        const size_t approxChars = (maxWidthPx > 0) ? (size_t)max(10, maxWidthPx / 10) : 60;
        istringstream iss(text);
        string word, line;
        while (iss >> word) {
            if (line.empty()) line = word;
            else if (line.size() + 1 + word.size() > approxChars) {
                out.push_back(line);
                line = word;
            } else {
                line += " " + word;
            }
        }
        if (!line.empty()) out.push_back(line);
        return out;
    }

    auto textWidth = [&](const string& s) -> int {
        int w = 0, h = 0;
        if (s.empty()) return 0;
        if (TTF_SizeUTF8(font, s.c_str(), &w, &h) != 0) return 0;
        return w;
    };

    istringstream iss(text);
    string word;
    string line;

    auto flushLine = [&]() {
        if (!line.empty()) out.push_back(line);
        line.clear();
    };

    while (iss >> word) {
        if (line.empty()) {
            // Si una sola "palabra" ya no entra, la partimos.
            if (textWidth(word) <= maxWidthPx) {
                line = word;
            } else {
                string chunk;
                for (size_t i = 0; i < word.size(); i++) {
                    chunk.push_back(word[i]);
                    if ((int)chunk.size() > 1 && textWidth(chunk) > maxWidthPx) {
                        chunk.pop_back();
                        if (!chunk.empty()) out.push_back(chunk);
                        chunk.clear();
                        chunk.push_back(word[i]);
                    }
                }
                if (!chunk.empty()) line = chunk;
            }
        } else {
            string candidate = line + " " + word;
            if (textWidth(candidate) <= maxWidthPx) {
                line = candidate;
            } else {
                flushLine();

                if (textWidth(word) <= maxWidthPx) {
                    line = word;
                } else {
                    string chunk;
                    for (size_t i = 0; i < word.size(); i++) {
                        chunk.push_back(word[i]);
                        if ((int)chunk.size() > 1 && textWidth(chunk) > maxWidthPx) {
                            chunk.pop_back();
                            if (!chunk.empty()) out.push_back(chunk);
                            chunk.clear();
                            chunk.push_back(word[i]);
                        }
                    }
                    if (!chunk.empty()) line = chunk;
                }
            }
        }
    }

    flushLine();
    return out;
}

static void drawText(const string& text, int x, int y, SDL_Color color) {
    if (!font) return;
    SDL_Surface* surface = TTF_RenderUTF8_Solid(font, text.c_str(), color);
    if (!surface) return;
    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
    SDL_Rect rect = {x, y, surface->w, surface->h};
    SDL_RenderCopy(renderer, texture, nullptr, &rect);
    SDL_DestroyTexture(texture);
    SDL_FreeSurface(surface);
}

static void drawButton(const string& label, SDL_Rect rect, SDL_Color bgColor, SDL_Color textColor) {
    SDL_SetRenderDrawColor(renderer, bgColor.r, bgColor.g, bgColor.b, bgColor.a);
    SDL_RenderFillRect(renderer, &rect);

    SDL_SetRenderDrawColor(renderer, WHT.r, WHT.g, WHT.b, WHT.a);
    SDL_RenderDrawRect(renderer, &rect);

    drawText(label, rect.x + 12, rect.y + 10, textColor);
}

struct Choice {
    char label = '?';
    string text;
    bool correct = false;
};

struct Question {
    string prompt;
    vector<Choice> choices;
};

static string readAllFile(const string& path) {
    ifstream in(path);
    if (!in) return {};
    ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

static vector<Question> parseGiftSimple(const string& content) {
    vector<Question> qs;

    string s = content;
    s.erase(remove(s.begin(), s.end(), '\r'), s.end());

    size_t pos = 0;
    while (true) {
        size_t open = s.find('{', pos);
        if (open == string::npos) break;
        size_t close = s.find('}', open);
        if (close == string::npos) break;

        size_t promptStart = s.rfind("\n", open);
        if (promptStart == string::npos) promptStart = 0; else promptStart += 1;

        size_t titleStart = s.rfind("::", open);
        if (titleStart != string::npos && titleStart >= promptStart) {
            size_t titleEnd = s.find("::", titleStart + 2);
            if (titleEnd != string::npos && titleEnd < open) {
                promptStart = titleEnd + 2;
            }
        }

        string prompt = trim(s.substr(promptStart, open - promptStart));
        string body = trim(s.substr(open + 1, close - (open + 1)));

        vector<pair<bool, string>> parsed;
        {
            bool curIsCorrect = false;
            string cur;

            auto flush = [&]() {
                string t = trim(cur);
                if (!t.empty()) parsed.push_back({curIsCorrect, t});
                cur.clear();
            };

            for (size_t i = 0; i < body.size(); i++) {
                char c = body[i];
                if (c == '=' || c == '~') {
                    flush();
                    curIsCorrect = (c == '=');
                } else {
                    cur.push_back(c);
                }
            }
            flush();
        }

        Question q;
        q.prompt = prompt;

        char label = 'A';
        for (auto& it : parsed) {
            if (label > 'Z') break;
            Choice ch;
            ch.label = label++;
            ch.correct = it.first;
            ch.text = it.second;
            q.choices.push_back(ch);
        }

        bool hasCorrect = any_of(q.choices.begin(), q.choices.end(),
                                [](const Choice& c) { return c.correct; });

        if (!q.prompt.empty() && q.choices.size() >= 2 && hasCorrect) {
            qs.push_back(q);
        }

        pos = close + 1;
    }

    return qs;
}

enum class GameState {
    SHOW_QUESTION,
    FALLING,
    GAME_OVER,
    GAME_WIN
};

struct FallingLetter {
    SDL_Rect rect{};
    char label = '?';
    bool correct = false;
};

static SDL_Rect paddle{};
static float fallSpeed = INITIAL_SPEED;
static GameState state = GameState::SHOW_QUESTION;

static vector<Question> questions;
static int currentQ = 0;
static vector<FallingLetter> falling;
static int correctCount = 0;

static SDL_Rect btnLeft{};
static SDL_Rect btnRight{};
static SDL_Rect btnContinue{};

static bool gameRunning = true;

static bool initSDL() {
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        cout << "Error SDL: " << SDL_GetError() << endl;
        return false;
    }

    window = SDL_CreateWindow("Quiz Catch (GIFT) - SDL2",
                              SDL_WINDOWPOS_CENTERED,
                              SDL_WINDOWPOS_CENTERED,
                              W, H,
                              SDL_WINDOW_SHOWN);
    if (!window) {
        cout << "Error ventana: " << SDL_GetError() << endl;
        return false;
    }

    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    if (!renderer) renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
    if (!renderer) {
        cout << "Error renderer: " << SDL_GetError() << endl;
        return false;
    }

    if (TTF_Init() < 0) {
        cout << "Error TTF: " << TTF_GetError() << endl;
        return false;
    }

    font = TTF_OpenFont("C:/Windows/Fonts/arial.ttf", 22);
    if (!font) font = TTF_OpenFont("arial.ttf", 22);

    srand((unsigned)time(nullptr));
    return true;
}

static void cleanup() {
    if (font) TTF_CloseFont(font);
    if (renderer) SDL_DestroyRenderer(renderer);
    if (window) SDL_DestroyWindow(window);
    TTF_Quit();
    SDL_Quit();
}

static void initGameUI() {
    paddle = {W / 2 - PADDLE_WIDTH / 2, H - 40, PADDLE_WIDTH, PADDLE_HEIGHT};

    int margin = 18;
    btnLeft = {margin, H - BUTTON_HEIGHT - margin, BUTTON_WIDTH, BUTTON_HEIGHT};
    btnRight = {W - BUTTON_WIDTH - margin, H - BUTTON_HEIGHT - margin, BUTTON_WIDTH, BUTTON_HEIGHT};

    btnContinue = {W / 2 - 90, H - BUTTON_HEIGHT - margin, 180, BUTTON_HEIGHT};
}

static int neededToWin() {
    int total = (int)questions.size();
    return (total / 2) + 1;
}

static void spawnLettersForCurrentQuestion() {
    falling.clear();
    if (currentQ < 0 || currentQ >= (int)questions.size()) return;

    const auto& q = questions[currentQ];
    int n = (int)q.choices.size();
    if (n <= 0) return;

    int topY = 150;
    int leftPad = 40;
    int rightPad = 40;
    int usable = W - leftPad - rightPad;

    for (int i = 0; i < n; i++) {
        float t = (n == 1) ? 0.5f : (float)i / (float)(n - 1);
        int x = leftPad + (int)(t * usable) - LETTER_SIZE / 2;

        FallingLetter fl;
        fl.label = q.choices[i].label;
        fl.correct = q.choices[i].correct;
        fl.rect = {x, topY, LETTER_SIZE, LETTER_SIZE};
        falling.push_back(fl);
    }

    fallSpeed = (float)INITIAL_SPEED;
}

static void handleAnswerCaught(const FallingLetter& caught) {
    if (caught.correct) correctCount++;

    currentQ++;

    if (currentQ >= (int)questions.size()) {
        state = (correctCount >= neededToWin()) ? GameState::GAME_WIN : GameState::GAME_OVER;
        return;
    }

    state = GameState::SHOW_QUESTION;
    falling.clear();
}

static bool pointInRect(int x, int y, const SDL_Rect& r) {
    return x >= r.x && x <= (r.x + r.w) && y >= r.y && y <= (r.y + r.h);
}

static bool rectsOverlap(const SDL_Rect& a, const SDL_Rect& b) {
    return (a.x < b.x + b.w) && (a.x + a.w > b.x) && (a.y < b.y + b.h) && (a.y + a.h > b.y);
}

static void handleEvents() {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        switch (event.type) {
            case SDL_QUIT:
                gameRunning = false;
                break;

            case SDL_MOUSEBUTTONDOWN:
                if (event.button.button == SDL_BUTTON_LEFT) {
                    int mx = event.button.x;
                    int my = event.button.y;

                    if (state == GameState::SHOW_QUESTION) {
                        if (pointInRect(mx, my, btnContinue)) {
                            state = GameState::FALLING;
                            spawnLettersForCurrentQuestion();
                        }
                    } else if (state == GameState::FALLING) {
                        if (pointInRect(mx, my, btnLeft)) {
                            if (paddle.x > 0) paddle.x -= 12;
                        } else if (pointInRect(mx, my, btnRight)) {
                            if (paddle.x < W - paddle.w) paddle.x += 12;
                        }
                    } else {
                        gameRunning = false;
                    }
                }
                break;

            case SDL_KEYDOWN:
                if (event.key.keysym.sym == SDLK_ESCAPE) {
                    gameRunning = false;
                    break;
                }

                if (state == GameState::SHOW_QUESTION) {
                    if (event.key.keysym.sym == SDLK_SPACE || event.key.keysym.sym == SDLK_RETURN) {
                        state = GameState::FALLING;
                        spawnLettersForCurrentQuestion();
                    }
                    break;
                }

                if (state == GameState::FALLING) {
                    switch (event.key.keysym.sym) {
                        case SDLK_LEFT:
                        case SDLK_a:
                            if (paddle.x > 0) paddle.x -= 12;
                            break;
                        case SDLK_RIGHT:
                        case SDLK_d:
                            if (paddle.x < W - paddle.w) paddle.x += 12;
                            break;
                        case SDLK_p:
                            state = GameState::SHOW_QUESTION;
                            falling.clear();
                            break;
                        default:
                            break;
                    }
                    break;
                }

                if (state == GameState::GAME_OVER || state == GameState::GAME_WIN) {
                    gameRunning = false;
                }
                break;
        }
    }
}

static void updateGame() {
    if (state != GameState::FALLING) return;

    for (auto& fl : falling) fl.rect.y += (int)fallSpeed;

    for (const auto& fl : falling) {
        if (rectsOverlap(fl.rect, paddle)) {
            handleAnswerCaught(fl);
            return;
        }
    }

    bool anyLost = false;
    for (const auto& fl : falling) {
        if (fl.rect.y > H) {
            anyLost = true;
            break;
        }
    }
    if (anyLost) {
        FallingLetter dummy;
        dummy.correct = false;
        handleAnswerCaught(dummy);
        return;
    }

    fallSpeed += SPEED_ACCEL; // <- antes 0.01f (más lenta la aceleración)
}

static void renderQuestionOverlay() {
    if (currentQ < 0 || currentQ >= (int)questions.size()) return;
    const auto& q = questions[currentQ];

    drawText("PAUSA: lee la pregunta. SPACE/ENTER o Continue para soltar letras.", 18, 14, YLW);

    {
        ostringstream ss;
        ss << "Pregunta " << (currentQ + 1) << "/" << questions.size()
           << " | Aciertos: " << correctCount
           << " | Para ganar: " << neededToWin();
        drawText(ss.str(), 18, 44, WHT);
    }

    const int leftX = 18;
    const int maxWidth = W - 2 * leftX;
    const int lineStep = 26;

    int y = 90;

    // Prompt con wrap por píxeles
    for (const auto& ln : splitLinesWrapPixels(q.prompt, maxWidth)) {
        drawText(ln, leftX, y, WHT);
        y += lineStep;
    }

    y += 10;

    // Opciones con wrap por píxeles (y con indent en líneas siguientes)
    for (const auto& c : q.choices) {
        const int indentFirst = 0;
        const int indentNext = 24;
        const string prefix = string(1, c.label) + ") ";

        auto wrapped = splitLinesWrapPixels(prefix + c.text, maxWidth);
        for (size_t i = 0; i < wrapped.size(); i++) {
            int x = leftX + ((i == 0) ? indentFirst : indentNext);
            drawText(wrapped[i], x, y, BLU);
            y += lineStep;
        }
    }

    drawButton("Continue (SPACE)", btnContinue, GRN, BLK);
}

static void renderFalling() {
    drawButton("<--", btnLeft, GRN, BLK);
    drawButton("-->", btnRight, GRN, BLK);

    {
        ostringstream ss;
        ss << "Pregunta " << (currentQ + 1) << "/" << questions.size()
           << " | Aciertos: " << correctCount
           << " | P: Pausa";
        drawText(ss.str(), 18, 14, WHT);
    }

    SDL_SetRenderDrawColor(renderer, WHT.r, WHT.g, WHT.b, WHT.a);
    SDL_RenderFillRect(renderer, &paddle);

    for (const auto& fl : falling) {
        SDL_Color box = fl.correct ? SDL_Color{0, 160, 80, 255} : SDL_Color{60, 120, 220, 255};
        SDL_SetRenderDrawColor(renderer, box.r, box.g, box.b, box.a);
        SDL_RenderFillRect(renderer, &fl.rect);

        SDL_SetRenderDrawColor(renderer, WHT.r, WHT.g, WHT.b, WHT.a);
        SDL_RenderDrawRect(renderer, &fl.rect);

        string s(1, fl.label);
        drawText(s, fl.rect.x + 8, fl.rect.y + 2, WHT);
    }
}

static void renderEndScreen(bool win) {
    string title = win ? "GANASTE!" : "FIN DEL JUEGO";
    SDL_Color col = win ? GRN : RED;

    drawText(title, W / 2 - 90, H / 2 - 80, col);

    ostringstream ss;
    ss << "Aciertos: " << correctCount << "/" << questions.size()
       << " | Necesarios: " << neededToWin();
    drawText(ss.str(), W / 2 - 170, H / 2 - 40, WHT);

    drawText("Presiona cualquier tecla o clic para salir.", W / 2 - 210, H / 2 + 10, WHT);
}

static void renderGame() {
    SDL_SetRenderDrawColor(renderer, BLK.r, BLK.g, BLK.b, BLK.a);
    SDL_RenderClear(renderer);

    if (questions.empty()) {
        drawText("No se cargaron preguntas. Asegura un archivo quiz.gift valido.", 18, 18, RED);
        drawText("Uso: QuizCatch.exe quiz.gift", 18, 50, WHT);
        SDL_RenderPresent(renderer);
        return;
    }

    if (state == GameState::SHOW_QUESTION) renderQuestionOverlay();
    else if (state == GameState::FALLING) renderFalling();
    else if (state == GameState::GAME_OVER) renderEndScreen(false);
    else if (state == GameState::GAME_WIN) renderEndScreen(true);

    SDL_RenderPresent(renderer);
}


// Add this new function for the loop
static void main_loop() {
    Uint32 frameStart = SDL_GetTicks();
    handleEvents();
    updateGame();
    renderGame();
    Uint32 frameTime = SDL_GetTicks() - frameStart;
    // No SDL_Delay needed in web; Emscripten handles framing
}

int main(int argc, char* argv[]) {
    SDL_SetMainReady();
    if (!initSDL()) return 1;

    // Change font path for web (preload "arial.ttf")
    font = TTF_OpenFont("arial.ttf", 22);  // Remove Windows path fallback if not needed

    string giftPath = "quiz.gift";  // Preload this file
    if (argc >= 2) giftPath = argv[1];
    questions = parseGiftSimple(readAllFile(giftPath));

    initGameUI();
    state = GameState::SHOW_QUESTION;
    currentQ = 0;
    correctCount = 0;

#ifdef __EMSCRIPTEN__
    emscripten_set_main_loop(main_loop, 0, 1);  // 0 = browser FPS, 1 = simulate infinite loop
#else
    while (gameRunning) {
        main_loop();
    }
#endif

    cleanup();
    return 0;
}

/*
int main(int argc, char* argv[]) {
    SDL_SetMainReady();

    if (!initSDL()) return 1;

    string giftPath = "quiz.gift";
    if (argc >= 2) giftPath = argv[1];

    questions = parseGiftSimple(readAllFile(giftPath));

    initGameUI();
    state = GameState::SHOW_QUESTION;
    currentQ = 0;
    correctCount = 0;

    while (gameRunning) {
        Uint32 frameStart = SDL_GetTicks();

        handleEvents();
        updateGame();
        renderGame();

        Uint32 frameTime = SDL_GetTicks() - frameStart;
        if (frameTime < 1000 / 60) SDL_Delay(1000 / 60 - frameTime);
    }

    cleanup();
    return 0;
}
*/