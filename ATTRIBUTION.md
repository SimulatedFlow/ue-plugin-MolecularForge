# Attribution and data sources

MolecularForge ships no third-party code. It does ship a small number of experimentally
determined structures as demo data, and it can download further structures at runtime.
Those data have their own terms, and this file states them.

## Demo structures included with the plugin

The files under `Demo/` come from the RCSB Protein Data Bank (https://www.rcsb.org).
PDB coordinate data are released into the public domain; the wwPDB asks that the original
publications be cited when the data are used. The primary citations are:

| File | Entry | Structure | Primary citation |
|------|-------|-----------|------------------|
| `1CRN` | 1CRN | Crambin | Hendrickson, W.A., Teeter, M.M. (1981) *Nature* 290:107–113 |
| `1EMA` | 1EMA | Green fluorescent protein | Ormö, M. et al. (1996) *Science* 273:1392–1395 |
| `4HHB` | 4HHB | Human deoxyhaemoglobin | Fermi, G. et al. (1984) *J. Mol. Biol.* 175:159–174 |

Both the `.pdb` and the `.cif` form of each entry are included, because the plugin reads
both formats and the pair is what the format-comparison test checks against.

## Structures downloaded at runtime

The plugin can fetch structures from two archives. What you download is **not** covered by
the plugin's licence — it carries the licence of its source, and passing it on is your
responsibility, not ours.

**RCSB Protein Data Bank** — public domain, citation requested. The plugin sets

> Strukturdaten aus der RCSB Protein Data Bank (rcsb.org).

**AlphaFold Protein Structure Database** (DeepMind / EMBL-EBI) — **CC-BY 4.0**. Naming the
source is a licence condition here, not a courtesy. The plugin sets

> Strukturdaten aus der AlphaFold Protein Structure Database (DeepMind / EMBL-EBI),
> lizenziert unter CC-BY-4.0.

Both strings are available on every loaded structure as `Meta.Attribution` and are meant to
be shown wherever the structure is shown. If you ship a build that displays AlphaFold
models, that text — or an equivalent — has to be visible to your users.

Relevant citations:

- Jumper, J. et al. (2021) Highly accurate protein structure prediction with AlphaFold.
  *Nature* 596:583–589.
- Varadi, M. et al. (2022) AlphaFold Protein Structure Database. *Nucleic Acids Research*
  50:D439–D444.

## Algorithms

The secondary-structure assignment follows the method described in

- Kabsch, W., Sander, C. (1983) Dictionary of protein secondary structure. *Biopolymers*
  22:2577–2637.

This is an independent implementation from the published description. No DSSP source code
is used or included.

Element covalent and van-der-Waals radii follow the values commonly tabulated in the
chemical literature (Bondi 1964 and successors); CPK colouring follows the convention used
by the common molecular viewers.

## Narration in the presentation video

The spoken commentary in `MolecularForge_Mesoskala_kommentiert.mp4` is synthesised, not
recorded. It is listing material and is not part of the plugin.

- **Model:** Kokoro-82M, run locally through `kokoro-onnx`
- **Voice:** English default voice `af_bella`
- **Licence:** Apache-2.0 — free for commercial use

The script is in `Docs/Video_Kommentar.txt` and the build step in
`Tools/build_video_narration.py`, so the commentary can be regenerated whenever the plugin
changes rather than being re-recorded by hand.

## Engine content

The example levels use the standard Unreal Engine `BasicShapes` meshes and no other Epic
content. No marketplace or third-party assets are redistributed.
