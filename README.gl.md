# Doctor Migrana – RockON 3-in-1

Plugin de audio con forma de pedal de guitarra (**VST3** e **standalone**) que xunta tres pedais clásicos nunha caixa:
**booster → compresor → overdrive**, seguidos dun control de ton xeral e un volume master.

De **GuitarFros**. Para Linux, Windows e macOS.

![Previsualización da interface](docs/preview.jpg)
*Imaxe de maqueta da interface (os indicadores dos potes están debuxados en posicións de exemplo).*

> **Estado:** desenvolvido e probado en Ubuntu con Reaper. As versións de Windows e macOS xéranse con GitHub Actions
> e **aínda non se probaron en máquinas reais**. Benvidos os avisos de erros.

## Que fai

Cadea de sinal: **Boost → Compresor → Drive → Tone → Master**. Cada pedal ten o seu pedal de pé (footswitch) con LED.

| Pedal | Controis | Inspirado en |
|---|---|---|
| **BOOST** | BOOST (0–24 dB), RANGE (treble booster ↔ boost de rango completo) | Booster tipo Dallas Rangemaster, con recorte suave tipo xermanio cando se empurra |
| **COMP** | COMP (sensibilidade, ata 36 dB de ganancia), COMP LEVEL (saída ±12 dB), COMP LINK | Compresor OTA tipo MXR Dyna Comp (ataque rápido con "snap", relaxación lenta) |
| **DRIVE** | DRIVE, DRIVE TONE, DRIVE LEVEL (±12 dB), DRIVE LINK | Overdrive tipo Tube Screamer (paso alto de 720 Hz á entrada, recorte con díodos, circuíto de tono activo); limpo co DRIVE a 0 e con ganancia progresiva ata +41 dB |

Controis xerais:

- **TONE** – de 0 a 10. A posición 7 é neutra; por riba do 7 engade agudos extra (shelf de ata +9 dB).
- **MASTER** – o centro (as 12 en punto) son 0 dB; de −24 dB a +12 dB.
- **COMP LINK / DRIVE LINK** – activados, ao mover COMP (ou DRIVE) o seu pote LEVEL móvese en sentido contrario para manter máis ou menos o volume de saída, e ao revés.
- Interruptor **ON / BYPASS**, comparación **A/B** (con *copiar A→B / B→A*) e menú de **presets** (4 de fábrica + os teus).
- **Oversampling** (Off / 2x / 4x) para a etapa Drive, no menú da roda dentada (que tamén ten *About / Credits*). Engade unha pequena latencia, que se comunica ao host.
- Medidores LED de entrada e saída.

Os presets de usuario gárdanse como pequenos ficheiros `.dmpreset` en `DoctorMigrana/Presets`, dentro da carpeta de datos
de aplicacións do usuario (`~/.config` en Linux, `%APPDATA%` en Windows, `~/Library` en macOS).

## Instalación

Descarga o zip do teu sistema na páxina de **Releases** (ou nos artefactos de *Actions*) e copia:

| Sistema | Copia o cartafol `doctormigranaRockOn3-1.vst3` a | Despois |
|---|---|---|
| Linux | `~/.vst3/` | Reescanea os plugins no teu DAW |
| Windows | `C:\Program Files\Common Files\VST3\` | Reescanea os plugins no teu DAW |
| macOS | `~/Library/Audio/Plug-Ins/VST3/` | Reescanea os plugins no teu DAW |

O arquivo chámase `doctormigranaRockOn3-1.vst3`, pero o teu DAW amósao como **Doctor Migrana – RockON 3-in-1** (de *GuitarFros*).

As compilacións **non están asinadas**:
- **macOS:** quita a marca de corentena: `xattr -cr "~/Library/Audio/Plug-Ins/VST3/doctormigranaRockOn3-1.vst3"`
- **Windows:** SmartScreen pode avisar: *Máis información → Executar igualmente* (sobre todo co `.exe` standalone).

O standalone arranca coa entrada de audio silenciada (opción por defecto de JUCE): abre **Options → Audio/MIDI Settings**,
escolle a túa interface e quita o silencio da entrada. Usa auriculares para evitar realimentación.

## Compilar desde o código

Requisitos: CMake ≥ 3.22, un compilador C++17 e internet (CMake descarga JUCE 8.0.6 a primeira vez).

**Ubuntu / Debian**
```bash
sudo apt install build-essential cmake git pkg-config libasound2-dev libjack-jackd2-dev libfreetype-dev \
  libfontconfig1-dev libx11-dev libxcomposite-dev libxcursor-dev libxext-dev libxinerama-dev libxrandr-dev \
  libxrender-dev libglu1-mesa-dev mesa-common-dev
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel $(nproc)
```
O VST3 cópiase a `~/.vst3/` automaticamente (desactívase con `-DDM_COPY_AFTER_BUILD=OFF`). O resultado queda en
`build/DoctorMigrana_artefacts/Release/{VST3,Standalone}`.

**Windows** (Visual Studio 2022 coa carga de traballo *Desenvolvemento para escritorio con C++*)
```bat
cmake -B build
cmake --build build --config Release
```

**macOS** (ferramentas de liña de comandos de Xcode)
```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64" -DCMAKE_OSX_DEPLOYMENT_TARGET=11.0
cmake --build build --parallel
```

Se cambias o nome ou moves a carpeta do proxecto, borra `build/` e configura de novo (CMake garda rutas absolutas).

## Estrutura do repositorio

```
Source/        código do plugin (processor = son, editor = interface)
assets/        fondo da interface e sprites pequenos incrustados no plugin
art_source/    a imaxe orixinal de Meta AI da que sae o fondo
tools/         scripts que rexeneran assets/ a partir de art_source/ (renderizado de potes, textos, limpeza)
docs/          imaxe de previsualización
.github/       GitHub Actions: compila Linux/Windows/macOS e publica releases cos tags "v*"
```

## Créditos

- **Arte da interface: as imaxes foron creadas con Meta AI**, agás o **logo de Doctor Migraña**, que *non* está xerado por IA e é propiedade de **GuitarFros** (todos os dereitos reservados).
  As imaxes foron editadas e compostas por GuitarFros cos scripts de `tools/`; as tapas dos potes renderízanse por código
  (`tools/knob_render.py`). Mira [CREDITS.md](CREDITS.md) e [LICENSE-ARTWORK.md](LICENSE-ARTWORK.md) para os detalles e as condicións da arte.
- Feito con [JUCE](https://juce.com) (AGPLv3). A interface VST3 é o SDK VST3 de Steinberg que inclúe JUCE.
- O comportamento dos circuítos modelouse seguindo as análises públicas de ElectroSmash:
  [Tube Screamer](https://electrosmash.com/tube-screamer-analysis),
  [MXR Dyna Comp](https://electrosmash.com/mxr-dyna-comp-analysis) e
  [Dallas Rangemaster](https://electrosmash.com/dallas-rangemaster).

## Licenza

O **código fonte** está baixo a **GNU Affero General Public License v3.0 ou posterior** (mira `LICENSE`),
a licenza de código aberto que esixe JUCE 8 nos proxectos de código aberto.

A **arte** (`assets/`, `art_source/`, `docs/preview.jpg`) **non** está cuberta pola licenza do código. Rexese por
[LICENSE-ARTWORK.md](LICENSE-ARTWORK.md): as imaxes **crearonse con Meta AI** e distribúense aquí **para uso non comercial e
de código aberto, citando a Meta AI**; o logo de Doctor Migraña é propiedade de GuitarFros (todos os dereitos reservados).
Para usar unha compilación comercialmente, hai que substituír a arte.
Mira [LICENSE-ARTWORK.md](LICENSE-ARTWORK.md) e [CREDITS.md](CREDITS.md).

## Marcas rexistradas e aviso

VST é unha marca rexistrada de Steinberg Media Technologies GmbH. Tube Screamer, MXR, Dyna Comp, Rangemaster e calquera
outro nome de produto mencionado son marcas dos seus respectivos propietarios. Este proxecto é un traballo independente
*inspirado no* son destes circuítos; **non ten ningunha relación con eles nin está avalado por eles**.
O software ofrécese "tal cal", sen garantía de ningún tipo.
