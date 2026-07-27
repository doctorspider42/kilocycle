# Licensing and third-party notices

## Kilocycle's own source

Everything under `Source/` and `tools/` is

> Copyright (C) 2026 Kilocycle Audio
>
> Licensed under the **GNU General Public License, version 3 or later**
> (see [LICENSE](LICENSE)).

## The combined work, and binaries built from it

Kilocycle links the JUCE framework, whose free licence is the **GNU Affero
General Public License version 3** — not the GPL. The two licences are
deliberately made to work together: GPLv3 section 13 and AGPLv3 section 13 each
grant explicit permission to combine a work under one with a work under the
other.

The practical consequence, spelled out because it is easy to get wrong:

- **Kilocycle's own source stays GPLv3-or-later.** You may reuse it in any GPLv3
  project.
- **The combined work — including every binary in a release — carries the AGPLv3
  network clause** (AGPLv3 section 13) in addition to the GPL terms. For a desktop
  audio plug-in this obligation is inert: it only bites if users interact with the
  program remotely over a network, which a VST3 in a DAW does not do. It is stated
  here anyway, because a redistributor is entitled to know.
- Anyone distributing a Kilocycle binary must offer the **corresponding source**
  for exactly that build. The tagged commit in this repository, together with the
  pinned JUCE submodule, *is* that corresponding source.

`LICENSE.AGPLv3` is included so redistributors have both texts to hand.

## Third-party components

| Component | Version | Licence |
|---|---|---|
| [JUCE](https://github.com/juce-framework/JUCE) | 8.0.15 (pinned as the `libs/JUCE` submodule) | AGPLv3 or commercial — we use AGPLv3 |
| VST3 SDK (the subset bundled inside JUCE) | as shipped with JUCE 8.0.15 | MIT, © 2025 Steinberg Media Technologies GmbH |
| GPLv3 / AGPLv3 licence texts | — | © 2007 Free Software Foundation |

JUCE in turn bundles a number of libraries under permissive licences (zlib,
HarfBuzz, FLAC, Ogg Vorbis, pnglib, jpeglib, SheenBidi and others). The
authoritative list, with links to each licence, is in
[`libs/JUCE/LICENSE.md`](libs/JUCE/LICENSE.md).

Note that the VST3 SDK Steinberg ships with JUCE 8 is **MIT licensed** as of 2025.
It imposes no copyleft obligation of its own — the only reason this project is
copyleft is JUCE. If Kilocycle were ever ported to a permissively licensed host
framework, the plug-in could be permissively licensed too; the DSP in
`Source/dsp/` was deliberately written as plain C++17 headers with no JUCE
includes so that port stays possible.

## Trademarks

VST is a trademark of Steinberg Media Technologies GmbH, registered in Europe and
other countries. Kilocycle uses the VST3 SDK under its MIT licence and does not
use, and is not licensed to use, Steinberg's VST logo. The valve type numbers on
the panel (ECC83, ECC88, EF86, 6SN7, 6V6, EL84) are generic industry part
designations; the models behind them are original caricatures, not measurements
of, or claims about, any manufacturer's product.
