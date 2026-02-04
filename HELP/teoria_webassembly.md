
# Teoría: ¿Qué es WebAssembly y cómo funciona en el navegador?

WebAssembly (Wasm) es un formato binario de bajo nivel que permite ejecutar código compilado (por ejemplo, C++ o Rust) en el navegador a una velocidad cercana a la nativa.

WebAssembly no reemplaza JavaScript, sino que lo complementa: el archivo `.wasm` realiza los cálculos pesados, mientras que el archivo `.js` se encarga de la manipulación del DOM, eventos y otras tareas.

## ¿Cómo funciona?

1. Emscripten convierte el código C++ en archivos `.wasm`, `.js` y código de enlace (glue code).
2. El navegador descarga estos archivos.
3. El motor del navegador (V8, SpiderMonkey, JavaScriptCore) compila el archivo Wasm a código máquina nativo.
4. SDL2 se emula sobre Canvas/WebGL, permitiendo renderizar gráficos sin necesidad de instalar software adicional.

## Ventajas

- Ofrece un rendimiento muy alto, cercano al de un ejecutable nativo.
- Es seguro, ya que se ejecuta en un entorno aislado (sandbox) del navegador.
- Es portátil y funciona en cualquier navegador moderno.

## Limitaciones

- El acceso a archivos solo es posible mediante preload o fetch.
- No soporta hilos nativos, a menos que se utilice la opción `-s USE_PTHREADS=1`.
- La depuración puede ser más compleja, aunque se pueden usar source maps.
