# MolecularForge — Bauplan

**Was das ist:** Ein Struktur-Loader und Renderer für Proteine und Moleküle in Unreal Engine 5.8.
Kern in einem Satz: *PDB-/mmCIF-Datei oder UniProt-ID rein → performantes, animierbares, Niagara-fähiges Molekül in der Szene raus.*

**Rechtsträger:** Freelancer Simulated Flow (Plugins/Bücher-Schiene), nicht die Simulated Flow UG.
**Ziel-Engine:** UE 5.8. **Zielplattform Verkauf:** Fab, sekundär Gumroad/Itch.

---

## 0. Stand — hier weitermachen

*Letzte Aktualisierung: 2026-08-19.*

**Phase 1 fertig. Phase 2 begonnen: mmCIF-Parser steht.** Beide Module bauen sauber gegen
UE 5.8; **sieben** Automationstests laufen grün (`MolecularForge.*`).

Der schärfste davon ist `Parser.CifStimmtMitPdbUeberein`: derselbe Strukturausschnitt in
beiden Formaten, und die Ergebnisse werden Atom für Atom verglichen. Wenn ein dritter Leser
dazukommt (BinaryCIF, MMTF), gehört er in dasselbe Muster.

Bauen und Testen ohne Editor-GUI:
```powershell
& "C:\Program Files\Epic Games\UE_5.8\Engine\Build\BatchFiles\Build.bat" `
  ExamplePluginProjectEditor Win64 Development `
  -Project="F:\Unreal Projects\ExamplePluginProject\ExamplePluginProject.uproject" -WaitMutex

& "C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" `
  "F:\Unreal Projects\ExamplePluginProject\ExamplePluginProject.uproject" `
  -ExecCmds="Automation RunTests MolecularForge; Quit" -unattended -nopause -nullrhi -nosplash -stdout
```
Vorher prüfen, ob `UnrealEditor.exe` läuft — der Testlauf sperrt sonst gegen die offene Sitzung.

**Als Nächstes:** Sekundärstruktur-Berechnung (DSSP-artig über Wasserstoffbrücken-Energie),
dann `MolecularForgeWeb` (AlphaFold-DB + RCSB), dann die Mesh-Erzeugung über Geometry
Scripting. Alles ohne Editor machbar.

Warum die Berechnung nötig ist, obwohl beide Parser Sekundärstruktur lesen können:
AlphaFold-Dateien enthalten keine. Ohne eigene Berechnung bliebe ausgerechnet die Quelle
mit 200 Millionen Strukturen durchgehend als „Coil" eingefärbt.

**Was auf den Editor wartet** (nicht ohne GUI zu erledigen, deshalb bewusst zurückgestellt):
Impostor-Material, Beispiel-Level, Screenshots, Video, Fab-Paket.

---

## 1. Warum dieses Plugin

- **Zwei Käufergruppen statt einer.** Sci-Fi-/Medical-Gamedevs (kaufen für Optik) *und* echte Visualisierungsarbeit
  (Pharma-Studios, Unis, VR-Lehre — kaufen für Funktion). Bisherige Plugins bedienen nur die erste Gruppe.
- **Unendlicher Gratis-Content.** ~220.000 PDB-Einträge + ~200 Mio. AlphaFold-Strukturen. Marketing-Material
  erzeugt sich selbst und sieht jedes Mal anders aus.
- **Technisch nicht in einer Woche kopierbar.** 150.000+ Atome (Ribosom) bei 60 fps geht nur über
  Impostor-Spheres und Instancing. Genau das ist der Unterschied zwischen „Asset" und „Werkzeug".

---

## 2. Modulschnitt

| Modul | Typ | Inhalt |
|---|---|---|
| `MolecularForgeRuntime` | Runtime | Datenmodell, PDB-/mmCIF-Parser, Bindungsableitung, Sekundärstruktur, Elementtabelle |
| `MolecularForgeRender` | Runtime | Impostor-Spheres, Ball-and-Stick, Backbone-Tube/Ribbon, Solvent-Surface, Färbeschemata |
| `MolecularForgeWeb` | Runtime | AlphaFold-DB- und RCSB-Abruf, lokaler Cache |
| `MolecularForgeMass` | Runtime | **Mesoskopische Ebene**: viele Molekül-Instanzen als Mass-Entities (Diffusion, LOD, Repräsentation) |
| `MolecularForgeNiagara` | Runtime | Niagara Data Interface: Atompositionen/Elemente/Ketten als Partikelquelle |
| `MolecularForgeEditor` | Editor | Import-Factory (`.pdb`/`.cif` per Drag&Drop), Asset-Typ, Thumbnail, „Struktur holen"-Dialog |

---

## 3. Wo MassAI eingesetzt wird — und wo bewusst nicht

Das ist eine Architekturentscheidung, keine Geschmacksfrage. Sie wird hier festgehalten, damit sie nicht
in jeder Phase neu aufgeworfen wird.

**Nicht** für einzelne Atome. Ein Atom in einer statischen Struktur hat kein Verhalten, keine Entscheidung,
keinen eigenen Zustand — es ist eine Position plus zwei Zahlen. Es als `FMassEntityHandle` mit Archetyp,
Fragmenten und Chunk-Verwaltung zu führen, kostet pro Atom ein Vielfaches dessen, was das Atom an Daten
überhaupt trägt. Bei 150.000 Atomen ist das messbar schlechter als ein flaches SoA-Array. Parser,
Bindungsableitung und Mesh-Erzeugung laufen deshalb über `ParallelFor` und `UE::Tasks` direkt auf
SoA-Arrays — echtes Multithreading, nur ohne ECS-Overhead.

**Ja** für die mesoskopische Ebene (Phase 4). Dort ist jede Entity *ein ganzes Molekül*, das sich bewegt,
rotiert, diffundiert, an andere bindet und je nach Kameraabstand anders dargestellt werden muss. Das ist
exakt der Fall, für den Mass gebaut wurde:

- `FMassMoleculeInstanceFragment` — welche `UMolecularStructure`, welcher Konformationszustand
- `FTransformFragment` + `FMassVelocityFragment` — Position/Drift (Engine-Fragmente wiederverwenden)
- `FMassRepresentationFragment` + LOD — nah: volle Atomdarstellung; mittel: Backbone; fern: ein Impostor-Blob
- `UMassBrownianMotionProcessor` — Diffusion, `ParallelForEachEntityChunk`
- `UMassMolecularBindingProcessor` — Bindungsereignisse über Nachbarschaftsgitter

Das Datenmodell aus Phase 1 wird deshalb schon jetzt POD-freundlich und SoA gehalten, damit Phase 4
ohne Umbau darauf aufsetzen kann.

---

## 4. Phasen

Jede Phase ist für sich lauffähig und screenshot-tauglich. Reihenfolge ist bindend.

### Phase 1 — Statik: Laden und Darstellen ✅
- [x] Elementtabelle (118 Elemente: CPK-Farbe, Van-der-Waals-Radius, kovalenter Radius)
- [x] Datenmodell `UMolecularStructure` in SoA (Atome/Residuen/Ketten/Bindungen/Metadaten)
- [x] PDB-Parser (fixe Spalten), dreistufig: sequenzielle Klassifizierung → `ParallelFor` über
      die Atomzeilen → sequenzielles Verdichten und Gruppieren
- [x] Bindungsableitung über Uniform-Grid + kovalente Radien, parallel; freie Ionen ausgenommen
- [x] `UMolecularAtomsComponent`: Instanced-Rendering der Atome (zunächst Engine-Sphere),
      Farbe und Radius bereits als Per-Instance Custom Data
- [x] `AMolecularStructureActor` als Ein-Klick-Weg vom Dateipfad zum Bild
- [x] Färbeschemata: Element (CPK), Kette, Sekundärstruktur, B-Faktor/pLDDT, Hydrophobizität, einfarbig
- [x] Blueprint-API (`UMolecularForgeLibrary`) zum Laden aus Datei und aus Text
- [x] Automationstests: Parser-Grundlagen, Ladeoptionen, AlphaFold-Erkennung, Elementtabelle

Offen aus Phase 1, weil ohne Editor nicht prüfbar: die Darstellung wurde noch nie gerendert.
Der Code ist getestet, das Bild nicht.

### Phase 2 — Qualität und Web
- [ ] Impostor-Sphere-Material (Pixel Depth Offset) statt Engine-Sphere — der Performance-Sprung ⏸ *braucht Editor*
- [x] mmCIF-Parser (Textformat) — Tokenizer mit Anführungszeichen, `;`-Textfeldern, Kommentaren,
      Nullwerten; `auth_*` wird gegenüber `label_*` bevorzugt; nur Zeilenanfänge werden gemerkt,
      die Zeilen selbst laufen parallel
- [x] Gemeinsamer Assembler für beide Formate + Formaterkennung (`ParseStructureFile`)
- [x] Ketten-ID von `uint8` auf `FName` verbreitert — mmCIF erlaubt mehrstellige Bezeichner,
      und in Ribosomen sind `AA` und `AB` verschiedene Ketten
- [ ] Sekundärstruktur: aus `HELIX`/`SHEET` bzw. `struct_conf` lesen ✅, sonst DSSP-artig
      berechnen — **noch offen**, AlphaFold-Dateien liefern keine
- [ ] Ball-and-Stick, Backbone-Tube, Cartoon/Ribbon über `UDynamicMesh` (Geometry Scripting)
- [ ] `MolecularForgeWeb`: AlphaFold-DB (`/api/prediction/{uniprot}`) + RCSB-Abruf, lokaler Cache
- [ ] Editor-Import-Factory: `.pdb`/`.cif` ins Content-Browser ziehen → Asset

### Phase 3 — Bewegung und Effekt
- [ ] Solvent-Surface: Gauß-Dichtegitter + Marching Cubes, GPU wo möglich
- [ ] Niagara Data Interface (Atompositionen/Elemente als Partikelquelle) — Auflösen, Falten, Docking-VFX
- [ ] MD-Trajektorien (XTC/DCD) als Positionsanimation abspielen
- [ ] Messwerkzeuge (Abstand/Winkel), Selektionssprache (`chain A and resi 1-50`)

### Phase 4 — Maßstabssprung (MassAI)
- [ ] `MolecularForgeMass`: Fragmente, Traits, LOD, Repräsentation
- [ ] Brownsche Diffusion als Mass-Processor
- [ ] Bindungs-/Kollisionsereignisse über Nachbarschaftsgitter
- [ ] Zell-Innenraum-Szene: Organellen + Molekülpopulationen
- [ ] Demo-Level „Partielle Zellreprogrammierung": Yamanaka-Faktoren binden an Chromatin

### Phase 5 — Fab-Reife
- [ ] Beispiel-Level (Showcase + Minimal-Setup)
- [ ] Screenshots + Video-Material
- [ ] Doku (README, Quickstart, API-Referenz)
- [ ] Fab-TRC-Konformität: `FilterPlugin.ini`, `Content/MolecularForge/`-Struktur, Paketbau
- [ ] Attributionstext im Plugin-UI und im Listing

---

## 5. Lizenzlage — vorab geklärt

| Quelle | Lage | Konsequenz |
|---|---|---|
| **AlphaFold DB** | CC-BY-4.0, kommerziell erlaubt | Namensnennung **Pflicht** — Attributionstext ins Plugin-UI *und* ins Fab-Listing |
| **RCSB PDB** | Archivdaten frei nutzbar | Nicht hämmern: lokaler Cache ist Pflicht, nicht Kür |
| **AlphaFold-3-Code/-Gewichte** | Nicht-kommerziell restringiert | **Nicht anfassen.** Wir holen nur vorberechnete Ergebnisse aus der DB — damit sauber |
| Mitgelieferte Demo-Strukturen | Nur 2–3 kleine, mit Attribution | Fab-Paketgröße; alles Weitere wird zur Laufzeit geholt |

---

## 6. Grenzen, die bewusst gezogen sind

- **Keine Vorhersage.** Das Plugin faltet keine Proteine und rechnet keine Inferenz. Es lädt und zeigt.
- **Keine Simulationsphysik in Phase 1–3.** MD-Trajektorien werden *abgespielt*, nicht gerechnet.
- **Keine wissenschaftliche Zertifizierung.** Darstellung ist für Visualisierung gedacht, nicht für
  regulatorische oder diagnostische Zwecke. Das steht so auch im Listing.
