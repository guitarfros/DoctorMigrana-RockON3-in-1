# Como publicar este proxecto en GitHub (guía para GuitarFros)

1. **Crea un repositorio** en https://github.com/new (público; sen README, sen .gitignore nin licenza, porque xa están aquí).
   Para usar JUCE 8 de balde, o proxecto ten que ser de código aberto con licenza AGPL-3.0, así que debe ser **público**.

2. **O ficheiro `LICENSE`** (texto oficial da AGPL-3.0) **xa está incluído** na raíz do proxecto. Non hai que facer nada.
   (Se algún día o perdes: `curl -L -o LICENSE https://www.gnu.org/licenses/agpl-3.0.txt`.)

3. **Sube o código:**
   ```bash
   git init -b main
   git add .
   git commit -m "Doctor Migrana – RockON 3-in-1: first public version"
   git remote add origin https://github.com/TU_USUARIO/NOME_DO_REPOSITORIO.git
   git push -u origin main
   ```

4. **Mira a compilación automática:** pestana *Actions → Build*. Compila Linux, Windows e macOS (10–20 min).
   Se algunha falla, abre o rexistro desa execución e pégao onde me pidas axuda.

5. **Publica unha versión descargable (Release):**
   ```bash
   git tag v1.0.0
   git push origin v1.0.0
   ```
   Ao rematar a compilación, GitHub crea a *Release* `v1.0.0` cos zips de VST3 e standalone dos tres sistemas.

6. **Revisa antes de facer público:**
   - Que `CREDITS.md` e `LICENSE-ARTWORK.md` describen ben a arte: imaxes de Meta AI (agás o logo de Doctor Migraña, que é propiedade de GuitarFros). A arte NON está baixo a AGPL.
   - Se o logo non é de GuitarFros senón teu a título persoal, cambia "GuitarFros" polo teu nome nos avisos de logo (`README.md`, `README.gl.md`, `CREDITS.md`, `art_source/README.md`, o diálogo About en `Source/PluginEditor.cpp` e `.github/workflows/build.yml`).
   - Que queres deixar o teu correo ou unha páxina de contacto no README.
   - Que o repositorio non contén nada privado (revisa `git status` e os ficheiros de `art_source/`).
   - Se un día queres vender o plugin ou non abrir o código, necesitarás unha licenza comercial de JUCE.

7. Por último, no repositorio: *Settings → General → Features* para activar *Issues* se queres recibir avisos de erros.
