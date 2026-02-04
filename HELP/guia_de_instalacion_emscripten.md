## Markdown# Guía: Instalación de Emscripten, Compilación y Ejecución de Quiz Catch en WebAssembly

## 1. Requisitos Previos
- Sistema Operativo: Windows, macOS o Linux
- Git instalado: https://git-scm.com/downloads
- Python 3 instalado (con "Add to PATH" en Windows): https://www.python.org/downloads
- Archivos del proyecto:
  - `quizcatch.cpp` (código fuente modificado)
  - `arial.ttf` → Copiar desde `C:\Windows\Fonts\Arial.ttf` o descargar una fuente equivalente y renombrar
  - `quiz.gift` → Tu archivo de preguntas en formato GIFT

## 2. Instalación de Emscripten (Windows - PowerShell/cmd)
1. Crear carpeta (ejemplo):
mkdir D:\Tools\emsdk
cd D:\Tools\emsdk
text2. Clona el repositorio:
git clone https://github.com/emscripten-core/emsdk.git .
text3. Instala la versión latest:
.\emsdk install latest
text4. Activa:
.\emsdk activate latest
.\emsdk_env.bat
text(Repite `.\emsdk_env.bat` cada vez que abras una terminal nueva)

## 3. Arreglos importantes para arial.ttf
- **Problema común**: `file_packager: error: $arial.ttf does not exist`
- **Solución**:
- Copia `arial.ttf` y `quiz.gift` **exactamente** a la carpeta donde está `quizcatch.cpp`
- Nombre debe ser **minúsculas**: `arial.ttf` (no `Arial.ttf`)
- Verificar con:
```text
dir arial.ttf
dir quiz.gift
```
## 4. Compilación
En la carpeta del proyecto:
em++ quizcatch.cpp -o quiz.html -std=c++11 -O2 
-s USE_SDL=2 -s USE_SDL_TTF=2 -s USE_FREETYPE=1 
--preload-file arial.ttf --preload-file quiz.gift
textOpcional (si hay problemas de memoria):
... -s ALLOW_MEMORY_GROWTH=1
textArchivos generados: `quiz.html`, `quiz.js`, `quiz.wasm`, `quiz.data`

## 5. Ejecución
1. Iniciar servidor local:
python -m http.server
text2. Abre en navegador:
http://localhost:8000/quiz.html

¡Listo! El juego debe funcionar con teclado/mouse.

# Cambios necesarios en el código fuente para WebAssembly + Teoría básica

## Cambios en el código (quizcatch.cpp)

1. Agregar inclusión condicional:
```cpp
#ifdef __EMSCRIPTEN__
#include <emscripten/emscripten.h>
#endif

Cambiar carga de fuente (eliminar path de Windows):

C++font = TTF_OpenFont("arial.ttf", 22);

Crear función main_loop:

C++static void main_loop() {
    Uint32 frameStart = SDL_GetTicks();
    handleEvents();
    updateGame();
    renderGame();
    // No SDL_Delay aquí
}

Modificar main():

C++#ifdef __EMSCRIPTEN__
    emscripten_set_main_loop(main_loop, 0, 1);
#else
    while (gameRunning) {
        main_loop();
    }
#endif
```


---

## Más información

- Para compilar en emscripten, consultar [Ayuda-Memoria-Compilación-En-Emscripten.md](Ayuda-Memoria-Compilación-En-Emscripten.md).
- PAra tepria de WebAssembly, consultar la sección de teoría en el archivo [teoria_webassembly.md](teoria_webassembly.md).