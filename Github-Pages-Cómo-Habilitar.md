# Ayuda-Memoria: Cómo habilitar GitHub Pages en un repositorio

Última actualización: Febrero 2026  
Autor: Germán (para mis futuros proyectos WebAssembly, HTML5, etc.)

## Pasos para activar GitHub Pages

1. **Subir los archivos necesarios al repositorio**
   - Asegurarse de que los archivos principales estén en la rama principal (`main`):
     - `index.html` (o `quiz.html`, `juego.html`, etc.)
     - Archivos JS, CSS, WASM, DATA, imágenes, fuentes, etc.
   - Subir con:
git add .
git commit -m "Agrega archivos para GitHub Pages"
git push origin main
text2. **Ve a la configuración del repositorio**
- Abre tu repo en GitHub: https://github.com/tu-usuario/tu-repo
- Clic en **Settings** (engranaje arriba a la derecha)

3. **Acceder a la sección Pages**
- En el menú lateral izquierdo → bajar hasta **Pages**  
(está en la sección "Code and automation")

4. **Configurar la fuente de publicación**
- En **Build and deployment** → **Source**:
- Seleccionar **Deploy from a branch**
- En **Branch**:
- Elegir la rama principal → **main** (o master si se usa esa)
- Folder: **/(root)**  ← casi siempre es root
- Clic en **Save**

5. **Esperar el despliegue**
- GitHub muestra un mensaje verde:
"Your site is live at https://tu-usuario.github.io/tu-repo/"
- Puede tardar **1 a 10 minutos** la primera vez
- Refrescar la página de Settings/Pages para ver el estado

6. **URL final**
- Si se tiene `index.html` en la raíz:  
https://tu-usuario.github.io/tu-repo/
- Si el archivo principal es otro (ej. `quiz.html`):  
https://tu-usuario.github.io/tu-repo/quiz.html

## Tips rápidos para proyectos WebAssembly / HTML5

- **Renombrar a index.html** (recomendado):
git mv quiz.html index.html
git commit -m "Renombra a index.html para acceso directo"
git push origin main
text- **Archivos grandes (.wasm, .data)**:
- Subir solo una vez (pueden ser pesados)
- Luego comentar en `.gitignore`:
#.wasm
#.data
#*.js
text- **Problemas comunes y soluciones**
- 404 → Esperar más, limpia caché (Ctrl+Shift+R), verificar que el archivo esté en root
- Pantalla en blanco → Revisar consola (F12), confirma que .js/.wasm/.data se cargan (200 OK en Network)
- Fuentes o archivos no cargan → Verificar nombres exactos en preload y código
- README se muestra en vez del juego → Renombrar a index.html

- **Actualizar el juego**
- Recompilar localmente → `git add` los archivos nuevos → push
- GitHub Pages redeploya automáticamente (tarda 1-5 min)