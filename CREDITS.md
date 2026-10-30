# Credits and third-party notices

## Artwork (GUI)

| Item | Origin |
|---|---|
| Original pedal image (`art_source/doctor_migrana_source.webp`) and every image derived from it (`assets/bg.jpg`, the lever and switch sprites in `assets/`, `docs/preview.jpg`) | **Created with Meta AI** (Meta Platforms, Inc.). *Images generated with Meta AI.* Distributed here for non-commercial, open-source use with credit to Meta AI (see `LICENSE-ARTWORK.md`). |
| **Doctor Migraña logo** (the engraved plate in the image) | **Not AI-generated. Property of GuitarFros – all rights reserved.** It is not covered by the code licence and may not be reused without permission. |
| Knob caps, shadows and the printed 0–10 scales | Rendered by code (`tools/knob_render.py`, `tools/make_assets.py`) – no AI. |
| Text baked into the images (labels, title, footer) | Rendered with **DejaVu Sans Condensed Bold** (free Bitstream Vera / DejaVu licence). |

The Meta AI images were edited and composited by GuitarFros: controls were removed and rebuilt, labels and the title
were replaced, and the layout was rearranged (all reproducible with `tools/make_assets.py`).

**Terms of use of the AI-generated images:** they remain subject to the terms of the service that generated them
(Meta AI) and to the law that applies in your country regarding AI-generated content. The AGPL licence of this
repository applies to the **source code only**. The artwork terms (including the all-rights-reserved Doctor Migraña logo) are in
[LICENSE-ARTWORK.md](LICENSE-ARTWORK.md). If you reuse or redistribute the artwork, check those terms yourself.

## Software

- **JUCE 8** – dual-licensed AGPLv3 / JUCE commercial licence. This project uses it under the AGPLv3.
  https://juce.com
- **Steinberg VST3 SDK** (bundled with JUCE) – used under its GPLv3 option. VST is a registered trademark of
  Steinberg Media Technologies GmbH.
- Build tooling: CMake, and GitHub Actions workflows (`.github/workflows/build.yml`).

## Technical references (no code copied)

The DSP models are original implementations written from the public circuit analyses by ElectroSmash:

- Tube Screamer – https://electrosmash.com/tube-screamer-analysis
- MXR Dyna Comp – https://electrosmash.com/mxr-dyna-comp-analysis
- Dallas Rangemaster – https://electrosmash.com/dallas-rangemaster

## Trademarks

Tube Screamer, MXR, Dyna Comp, Rangemaster, Meta AI, JUCE, VST and all other names are trademarks of their
respective owners. Their use here is only descriptive; this project is not affiliated with or endorsed by them.
