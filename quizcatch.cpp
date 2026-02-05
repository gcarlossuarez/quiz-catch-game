/*
========================================
 DIAGRAMA DE LA LÓGICA DEL ALGORITMO
========================================

1. Carga preguntas desde archivo (GIFT)
2. Inicializa el estado y la UI
3. Muestra pantalla de selección de modo:
    ┌───────────────────────────────┐
    │  [1] Estudio   [2] Juego      │
    └───────────────────────────────┘
    |                              |
    |-> Estudio: orden original, muestra respuesta correcta en verde
    |-> Juego: mezcla preguntas, NO muestra respuesta correcta
4. Para cada pregunta:
    a) Muestra pregunta y opciones
    b) Al continuar, caen letras (opciones)
    c) El jugador mueve la paleta y atrapa una letra
    d) Si es correcta, suma acierto
    e) Avanza a la siguiente pregunta
5. Al finalizar:
    - Si aciertos >= mínimo, gana
    - Si no, fin del juego

// Lógica principal:

main()
  └─> cargar preguntas
  └─> initGameUI()
  └─> state = MODE_SELECT
  └─> loop principal:
            └─> handleEvents()
            └─> updateGame()
            └─> renderGame()

// Selección de modo:
handleEvents()
  └─> Si elige JUEGO: mezcla preguntas
  └─> Si elige ESTUDIO: NO mezcla

// Caída de letras:
renderFalling()
  └─> Si modo ESTUDIO y es correcta: verde
  └─> Si modo JUEGO: todas igual
*/
/*
========================================
 OPCIONES DE COMPILACIÓN RECOMENDADAS
========================================

Para compilar este juego con Emscripten y SDL2, usa:

em++ quizcatch.cpp -o quiz.html -std=c++11 -O2 \
  -s USE_SDL=2 -s USE_SDL_TTF=2 -s USE_FREETYPE=1 \
  --preload-file arial.ttf --preload-file quiz.gift

Explicación de cada opción:
- em++                → Compilador C++ de Emscripten
- quizcatch.cpp       → Archivo fuente principal
- -o quiz.html        → Salida: HTML+JS+WASM
- -std=c++11          → Usa estándar C++11
- -O2                 → Optimización nivel 2
- -s USE_SDL=2        → Habilita SDL2 (gráficos, input)
- -s USE_SDL_TTF=2    → Habilita SDL_ttf (texto TrueType)
- -s USE_FREETYPE=1   → Habilita soporte de fuentes TTF
- --preload-file ...  → Incluye archivos necesarios en el paquete (fuente y preguntas)

NOTA: Si se compila para escritorio (no web), se debe enlazar SDL2 y SDL2_ttf según el sistema donde se ejecute.

*/

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

// ----------------------------------------
// Modo de funcionamiento: JUEGO o ESTUDIO
// ----------------------------------------
enum class PlayMode {
    GAME,
    STUDY
};

static PlayMode playMode = PlayMode::STUDY; // Por defecto modo estudio
// ---------------------------------------


// Opciones de la pregunta actual (mezcladas si corresponde)


// Elimina espacios en blanco al inicio y final de una cadena.
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

// Dibuja un texto en pantalla en la posición (x, y) con el color dado.
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

// Dibuja un botón con etiqueta, fondo y color de texto especificados.
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

// Lee todo el contenido de un archivo de texto y lo retorna como string.
static string readAllFile(const string& path) {
    ifstream in(path);
    if (!in) return {};
    ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

// Parsea preguntas en formato GIFT simple y devuelve un vector de Question.
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
    MODE_SELECT,   // Nueva pantalla de selección de modo
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
static GameState state = GameState::MODE_SELECT;
// Botones para elegir modo
static SDL_Rect btnModoJuego{};
static SDL_Rect btnModoEstudio{};

static vector<Question> questions;
static int currentQ = 0;
static vector<FallingLetter> falling;
static int correctCount = 0;

static SDL_Rect btnLeft{};
static SDL_Rect btnRight{};
static SDL_Rect btnContinue{};

static bool gameRunning = true;

// Inicializa SDL2, la ventana, el renderer y la fuente. Devuelve true si tuvo éxito.
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

// Libera recursos de SDL2 y cierra la aplicación.
static void cleanup() {
    if (font) TTF_CloseFont(font);
    if (renderer) SDL_DestroyRenderer(renderer);
    if (window) SDL_DestroyWindow(window);
    TTF_Quit();
    SDL_Quit();
}

// Inicializa la posición de la paleta y los botones de la UI.
static void initGameUI() {
    paddle = {W / 2 - PADDLE_WIDTH / 2, H - 40, PADDLE_WIDTH, PADDLE_HEIGHT};

    int margin = 18;
    btnLeft = {margin, H - BUTTON_HEIGHT - margin, BUTTON_WIDTH, BUTTON_HEIGHT};
    btnRight = {W - BUTTON_WIDTH - margin, H - BUTTON_HEIGHT - margin, BUTTON_WIDTH, BUTTON_HEIGHT};
    btnContinue = {W / 2 - 90, H - BUTTON_HEIGHT - margin, 180, BUTTON_HEIGHT};

    // Botones de selección de modo
    btnModoJuego = {W/2 - 200, H/2 - 40, 180, 60};
    btnModoEstudio = {W/2 + 20, H/2 - 40, 180, 60};
}

// Calcula la cantidad de aciertos necesarios para ganar.
static int neededToWin() {
    int total = (int)questions.size();
    return (total / 2) + 1;
}

// Genera las letras (opciones) que caen para la pregunta actual.
// Si el modo es JUEGO, mezcla aleatoriamente las opciones y reasigna las letras.
static void spawnLettersForCurrentQuestion() {
    falling.clear();
    if (currentQ < 0 || currentQ >= (int)questions.size()) return;

    // Usar las opciones en el orden original
    const auto& choices = questions[currentQ].choices;
    int n = (int)choices.size();
    if (n <= 0) return;

    int topY = 150;
    int leftPad = 40;
    int rightPad = 40;
    int usable = W - leftPad - rightPad;

    for (int i = 0; i < n; i++) {
        float t = (n == 1) ? 0.5f : (float)i / (float)(n - 1);
        int x = leftPad + (int)(t * usable) - LETTER_SIZE / 2;

        FallingLetter fl;
        fl.label = choices[i].label;
        fl.correct = choices[i].correct;
        fl.rect = {x, topY, LETTER_SIZE, LETTER_SIZE};
        falling.push_back(fl);
    }

    fallSpeed = (float)INITIAL_SPEED;
}

// Procesa la respuesta atrapada: suma acierto si es correcta y avanza de pregunta.
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

// Devuelve true si el punto (x, y) está dentro del rectángulo r.
static bool pointInRect(int x, int y, const SDL_Rect& r) {
    return x >= r.x && x <= (r.x + r.w) && y >= r.y && y <= (r.y + r.h);
}

// Devuelve true si los rectángulos a y b se superponen.
static bool rectsOverlap(const SDL_Rect& a, const SDL_Rect& b) {
    return (a.x < b.x + b.w) && (a.x + a.w > b.x) && (a.y < b.y + b.h) && (a.y + a.h > b.y);
}

// Maneja los eventos de entrada del usuario (mouse, teclado) y la lógica de selección de modo.
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

                    if (state == GameState::MODE_SELECT) {
                        if (pointInRect(mx, my, btnModoEstudio)) {
                            playMode = PlayMode::STUDY;
                            state = GameState::SHOW_QUESTION;
                        } else if (pointInRect(mx, my, btnModoJuego)) {
                            playMode = PlayMode::GAME;
                            // Mensaje por consola antes de mezclar
                            cout << "[MODO JUEGO] Mezclando preguntas aleatoriamente..." << endl;
                            std::random_shuffle(questions.begin(), questions.end());
                            state = GameState::SHOW_QUESTION;
                        }
                    } else if (state == GameState::SHOW_QUESTION) {
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

                if (state == GameState::MODE_SELECT) {
                    if (event.key.keysym.sym == SDLK_1) {
                        playMode = PlayMode::STUDY;
                        state = GameState::SHOW_QUESTION;
                    } else if (event.key.keysym.sym == SDLK_2) {
                        playMode = PlayMode::GAME;
                        // Mensaje por consola antes de mezclar
                        cout << "[MODO JUEGO] Mezclando preguntas aleatoriamente..." << endl;
                        std::random_shuffle(questions.begin(), questions.end());
                        state = GameState::SHOW_QUESTION;
                    }
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

// Renderiza la pantalla de selección de modo
// Dibuja la pantalla de selección de modo (juego o estudio).
static void renderModeSelect() {
    SDL_SetRenderDrawColor(renderer, BLK.r, BLK.g, BLK.b, BLK.a);
    SDL_RenderClear(renderer);
    drawText("Selecciona el modo de juego:", W/2 - 180, H/2 - 120, YLW);
    drawButton("MODO JUEGO", btnModoJuego, BLU, BLK);
    drawButton("MODO ESTUDIO", btnModoEstudio, GRN, BLK);
    drawText("(1) Juego   (2) Estudio", W/2 - 120, H/2 + 40, WHT);
    SDL_RenderPresent(renderer);
}


// Actualiza la lógica del juego en cada frame (movimiento de letras, colisiones, etc).
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

// Dibuja la superposición con la pregunta y las opciones antes de que caigan las letras.
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
    for (const auto& c : questions[currentQ].choices) {
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

// Dibuja las letras cayendo y la paleta. Colorea en verde la correcta solo en modo estudio.
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
        SDL_Color box = fl.correct && playMode == PlayMode::STUDY ? SDL_Color{0, 160, 80, 255} : SDL_Color{60, 120, 220, 255};
        SDL_SetRenderDrawColor(renderer, box.r, box.g, box.b, box.a);
        SDL_RenderFillRect(renderer, &fl.rect);

        SDL_SetRenderDrawColor(renderer, WHT.r, WHT.g, WHT.b, WHT.a);
        SDL_RenderDrawRect(renderer, &fl.rect);

        string s(1, fl.label);
        drawText(s, fl.rect.x + 8, fl.rect.y + 2, WHT);
    }
}

// Dibuja la pantalla final de victoria o derrota.
static void renderEndScreen(bool win) {
    string title = win ? "GANASTE!" : "FIN DEL JUEGO";
    SDL_Color col = win ? GRN : RED;

    drawText(title, W / 2 - 90, H / 2 - 80, col);

    ostringstream ss;
    ss << "Aciertos: " << correctCount << "/" << questions.size()
       << " | Necesarios: " << neededToWin();
    drawText(ss.str(), W / 2 - 170, H / 2 - 40, WHT);

    drawText("Recarga el juego para reiniciar.", W / 2 - 210, H / 2 + 10, WHT);
}

// Dibuja la pantalla principal según el estado actual del juego.
static void renderGame() {
    SDL_SetRenderDrawColor(renderer, BLK.r, BLK.g, BLK.b, BLK.a);
    SDL_RenderClear(renderer);

    if (state == GameState::MODE_SELECT) {
        renderModeSelect();
        return;
    }
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
// Loop principal: procesa eventos, actualiza lógica y renderiza.
static void main_loop() {
    Uint32 frameStart = SDL_GetTicks();
    handleEvents();
    updateGame();
    renderGame();
    Uint32 frameTime = SDL_GetTicks() - frameStart;
    // No SDL_Delay needed in web; Emscripten handles framing
}

// Función principal: inicializa, carga preguntas, entra al loop principal y limpia al salir.
int main(int argc, char* argv[]) {
    SDL_SetMainReady();
    if (!initSDL()) return 1;

    // Change font path for web (preload "arial.ttf")
    font = TTF_OpenFont("arial.ttf", 22);  // Remove Windows path fallback if not needed

    string giftPath = "quiz.gift";  // Preload this file
    if (argc >= 2) giftPath = argv[1];
    questions = parseGiftSimple(readAllFile(giftPath));

    initGameUI();
    state = GameState::MODE_SELECT; // Mostrar pantalla de selección de modo al inicio
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
    state = GameState::MODE_SELECT;
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

