# Ayuda-Memoria: Acceso rápido a Emscripten en Windows (PowerShell / cmd)

Problema frecuente:
Cada vez que se abre una nueva ventana de PowerShell o cmd, el comando `em++`, `emcc`, etc. no funciona porque el PATH no está configurado.
Se tiene que activar el entorno manualmente ejecutando `emsdk_env.bat` **en cada sesión nueva**.

## Solución recomendada: Crear un acceso directo "Emscripten Prompt"

### Paso 1: Encuentra la carpeta de Emscripten

- Ruta típica en el caso de uso del "2026-02-04": `D:\Tools\emsdk`
- Dentro debe haber un archivo llamado: `emsdk_env.bat`

### Paso 2: Crea el acceso directo especial

1. Abrir el Explorador de Archivos y ir a `D:\Tools\emsdk`
2. Buscar el archivo `emsdk_env.bat`
3. Clic derecho → **Crear acceso directo**
4. Renombrar el acceso directo a algo claro, por ejemplo:

   - `Emscripten Prompt`
   - `Emscripten Terminal`
   - `Compilar Wasm`
5. Clic derecho en el acceso directo → **Propiedades**
6. En la pestaña **Acceso directo** → campo **Destino**:

   - Cambiar a esto (ajustar la ruta si es diferente):

```
%comspec% /K "D:\Tools\emsdk\emsdk_env.bat"
```

- `%comspec%` abre cmd.exe o PowerShell (depende del sistema)
- `/K` mantiene la ventana abierta después de ejecutar el .bat
- Poner la ruta **exacta** a tu `emsdk_env.bat`

7. (Opcional) En **Inicio en** poner la ruta donde se suele trabajar:

```
D:\UCB\Programación_Superior\Cpp\Games
```

Así siempre abre directamente en esa carpeta de proyectos.

8. (Opcional) Cambiar el ícono:

- Clic en **Cambiar icono** → buscar uno bonito o usa el de Emscripten (si existe en la carpeta).

9. Aceptar y guardar.

### Paso 3: Cómo usarlo (flujo rápido diario)

1. Doble clic en el acceso directo "Emscripten Prompt"
   - Se abre una consola con el entorno ya activado (verás mensajes como "Setting environment variables..." y el PATH actualizado)
2. Desde ahí, navegar al proyecto:

```
cd D:\UCB\Programación_Superior\Cpp\Games\quiz-catch-game
```

O cualquier otro repo donde se esté trabajando.

3. Compila5 directamente:

```
em++ mi-proyecto.cpp -o index.html -std=c++11 -O2 -s USE_SDL=2 ...
```

4. Cuando se termine, simplemente cerra5 la ventana (no se necesita desactivar nada).

### Alternativa más rápida (si se usa siempre la misma carpeta de proyectos)

- Crea el acceso directo como arriba, pero poner en **Inicio en** la ruta exacta de la carpeta de juegos:

```
D:\UCB\Programación_Superior\Cpp\Games
```

- Así al abrir el acceso directo ya se está directamente en la carpeta correcta y solo se tiene que hacer `cd nombre-del-proyecto` o compilar de una.

### Otras opciones rápidas (si no se quiere acceso directo)

- Mantener siempre abierta **esa misma consola** donde se ejecuta `emsdk_env.bat` y usarla solo para compilar Emscripten.
- O agregar al final de tu `README.md` del proyecto:

Para compilar:

Doble clic en "Emscripten Prompt" (acceso directo en D:\Tools\emsdk)
cd a esta carpeta
em++ ...
