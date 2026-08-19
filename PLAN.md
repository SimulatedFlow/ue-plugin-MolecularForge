# MolecularForge — Bauplan

**Was das ist:** Ein Struktur-Loader und Renderer für Proteine und Moleküle in Unreal Engine 5.8.
Kern in einem Satz: *PDB-/mmCIF-Datei oder UniProt-ID rein → performantes, animierbares, Niagara-fähiges Molekül in der Szene raus.*

**Rechtsträger:** Freelancer Simulated Flow (Plugins/Bücher-Schiene), nicht die Simulated Flow UG.
**Ziel-Engine:** UE 5.8. **Zielplattform Verkauf:** Fab, sekundär Gumroad/Itch.

---

## 0. Stand — hier weitermachen

*Letzte Aktualisierung: 2026-08-19.*

**Alle fünf Phasen sind abgeschlossen.** Alle fünf Module bauen sauber gegen UE 5.8;
**53** Automationstests grün, dazu die Prüfung gegen echte Archivdaten und drei
Netzabruf-Tests gegen die echten Server.

**Das Fab-Paket liegt fertig in `Desktop\Fab-Upload-MolecularForge\`** — Archiv, Listing-
text, Attributionstext und das Bild-/Videomaterial. Hochgeladen ist nichts; der Absende-
knopf gehört Silvan.

**Zwei Festlegungen darin, die er ändern kann, wenn er anders entscheidet:**
- Die Fassung steht auf `VersionName 1.0.0` und `IsBetaVersion: false`, wie bei den
  bereits angenommenen Plugins. Vorher stand dort 0.1 und Beta.
- Das Impostor-Material `M_MF_AtomImpostor` geht **nicht** mit ins Paket. Es lässt sich
  nicht übersetzen und würde beim Käufer nur eine Fehlerzeile erzeugen; die Impostoren
  sind ohnehin abgeschaltet. Im Quellbestand bleibt es liegen, damit die angefangene
  Arbeit nicht verloren ist, und wird jetzt erst bei Bedarf und lautlos geladen.

**Das Bild steht, und es ist farbig.** GFP rendert im Schaulevel mit 1771 Atomen in
CPK-Farben — roter Sauerstoff, blauer Stickstoff, grauer Kohlenstoff. Damit ist die Kette
von der Elementtabelle über die Per-Instance-Daten bis ins Material nachweislich dicht.

**Alle fünf Darstellungen sind im Bild nachgewiesen** — Space-Filling, Ball-and-Stick,
Rückgrat, Cartoon und Oberfläche. Das Vergleichsbild liegt unter `Docs/Bilder/`.

**Der Abruf läuft gegen die echten Server.** RCSB und AlphaFold beide geprüft, samt
Zwischenspeicher. Damit ist jeder Teil des Plugins mindestens einmal wirklich ausgeführt
worden — bis auf die Impostoren.

**Die Impostor-Kugeln sind vorerst gescheitert** und stehen auf „aus". Das Material ist
gebaut, aber die Ausrichtung des Vierecks im Vertexshader liefert nichts Sichtbares —
Einzelheiten und der nächste konkrete Schritt stehen unter den offenen Punkten. Bis dahin
zeichnet das Plugin echte Kugelgeometrie, was für einige tausend Atome flüssig läuft, aber
nicht für hunderttausende.

**Die Mesoskala-Demo läuft.** `L_MF_Mesoscale` setzt 780 Moleküle in zwei Arten, sie
diffundieren, binden und werden gestaffelt gezeichnet — im Bild belegt, die Aufteilung
zusätzlich im Log (`0 nah, 329 mittel, 451 fern`). Damit ist Phase 4 nicht mehr nur
einheitengetestet, sondern einmal wirklich gelaufen.

**Die Mesoskala-Moleküle sind farbig** — blaue Proteine, orange Liganden, und im Bild sitzen
die kleinen sichtbar an den großen. Sie wirkten lange grau; die Ursache lag nicht im Plugin,
sondern in der Aufnahme. Siehe die Falle zu `HighResShot` weiter unten.

**Die drei Beispiel-Level sind aufgeräumt.** Alle haben jetzt dieselbe festgenagelte
Belichtung (`Tools/mf_szene.py`), das Schaulevel dieselbe Lichtstärke wie das
Vergleichslevel, und im Vergleichsbild sind zum ersten Mal alle fünf Darstellungen
vollständig im Bild — vorher liefen die äußeren beiden hinaus.

**Bilder und Video liegen unter `Docs/`:**
- `Bilder/01_Darstellungsarten.png` — alle fünf Darstellungen nebeneinander (1CRN)
- `Bilder/02_GFP_SpaceFilling.png` — GFP, 1771 Atome in CPK-Farben
- `Bilder/03_Mesoskala.png` — Population aus 780 Molekülen
- `Video/MolecularForge_Mesoskala.mp4` — 1920×1080, 30 Bilder/s, 5,3 s Diffusion und Bindung

**Kleinigkeit, die noch offen ist:** Die Gauß-Oberfläche wirkt bei Blobbiness −2,3 recht
knubbelig — jedes Atom zeichnet sich einzeln ab. Für Schaubilder wäre ein weicherer Wert
(etwa −1,5) gefälliger; die Voreinstellung sollte trotzdem beim physikalisch üblichen Wert
bleiben.

Damit gibt es fünf Darstellungen: Space-Filling, Ball-and-Stick, Rückgrat, Cartoon und
Oberfläche, alle über `AMolecularStructureActor` umschaltbar.

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

### Phase 5 — Editorarbeit (läuft)

Silvan hat den Editor freigegeben. Reihenfolge ist bindend.

- [x] **Echte Strukturdaten geprüft.** 1CRN, 1EMA und 4HHB aus dem RCSB liegen unter
      `Demo/`. `Tools/verify_real_structures.py` lädt sie und prüft: PDB und mmCIF liefern
      identische Ergebnisse, GFP wird als faltblattreich erkannt, Hämoglobin als reine
      Helix, der Eisen-Histidin-Abstand im Häm misst 1,98 Å. **Damit ist die Testlücke bei
      der Faltblatt-Erkennung geschlossen.**
- [x] **Erstes Bild.** `Tools/build_showcase_level.py` baut das Schaulevel
      (`/MolecularForge/Maps/L_MF_Showcase`), Kameraabstand aus der Strukturausdehnung
      gerechnet. GFP rendert mit 1771 Atomen.
- [x] **Material, erster Teil: CPK-Farben.** `Tools/build_materials.py` baut `M_MF_Atoms`
      (Farbe aus Per-Instance-Daten, für Kugeln und Stäbe) und `M_MF_VertexColor`
      (Farbe aus Vertices, für Band und Oberfläche). Beide sind als Eigenschaft an den
      Komponenten hinterlegt und werden beim Registrieren gesetzt. Nachgewiesen: roter
      Sauerstoff, blauer Stickstoff, grauer Kohlenstoff im Bild.
- [~] **Material, zweiter Teil: Impostor-Kugeln** — Material gebaut (`M_MF_AtomImpostor`),
      Umschalter an der Komponente vorhanden, **aber nicht einsatzbereit**. Voreinstellung
      steht deshalb auf echten Kugeln. Ausführlich unter „Offene Punkte".
- [x] **Alle fünf Darstellungen nachgewiesen.** `Tools/build_comparison_level.py` baut ein
      Level, das Space-Filling, Ball-and-Stick, Rückgrat, Cartoon und Oberfläche
      nebeneinander zeigt (Crambin, 1CRN). Ein Durchlauf statt fünf — und es ist zugleich
      das Bild, das im Listing am meisten erklärt. Liegt unter
      `Docs/Bilder/01_Darstellungsarten.png`.
      Zahlen aus dem Aufbau: 327 Kugeln, 337 Bindungen, 6.504 Dreiecke Band,
      70.740 Dreiecke Oberfläche. Das Cartoon-Band zeigt Helices, Faltblatt-Pfeile und
      Schleifen in den erwarteten Farben.
- [x] **Echter Abruf nachgewiesen** — und zwar nicht von Hand, sondern als drei
      Automationstests unter `Netzabruf.*` (bewusst außerhalb des Namens `MolecularForge`,
      damit die reguläre Suite ohne Internet grün bleibt; geprüft, dass sie dort nicht
      mitläuft). Latente Befehle sind hier der richtige Ort: der Editor tickt dabei auch
      das HTTP-Modul, ein Kommandozeilenskript täte das nicht.
      Aufruf: `-ExecCmds="Automation RunTests Netzabruf; Quit"`
      - RCSB: `https://files.rcsb.org/download/1CRN.cif` → 327 Atome ✅
      - AlphaFold: API aufgelöst zu `.../files/AF-P69905-F1-model_v6.cif` → 1077 Atome,
        als pLDDT erkannt, Attribution gesetzt ✅
      - Zweiter Abruf kommt aus dem Zwischenspeicher ✅
- [x] Mesoskala-Demo: Molekülpopulation mit Diffusion und Bindung
      (`L_MF_Mesoscale`, gebaut von `Tools/build_mesoscale_level.py`)
      780 Moleküle in zwei Arten, farbig getrennt, Staffelung im Log belegt
      (`0 nah, 329 mittel, 451 fern`).
- [x] **Beispiel-Level aufgeräumt, Screenshots, Video.** Belichtung in allen drei Levels
      festgenagelt und als gemeinsamer Baustein in `Tools/mf_szene.py`; Lichtstärke im
      Schaulevel an das Vergleichslevel angeglichen; Bildausschnitt des Vergleichslevels
      korrigiert, sodass alle fünf Darstellungen vollständig im Bild stehen. Drei Bilder
      unter `Docs/Bilder/`, ein Video unter `Docs/Video/`.
- [x] **Fab-Paket (TRC-konform), README, Attributionstext.** Inhalt in den Einpackordner
      `Content/MolecularForge/` verschoben (mit Referenzkorrektur, nicht im Dateisystem),
      `Config/FilterPlugin.ini` und `Resources/Icon128.png` angelegt, `.uplugin` auf den
      Hausstil der bereits angenommenen Plugins gebracht, `README.md`, `DESCRIPTION.md`
      und `ATTRIBUTION.md` geschrieben. Gepackt und geprüft von
      `Tools/build_fab_package.py`; Ergebnis liegt in
      `Desktop\Fab-Upload-MolecularForge\`.

**Wie gebaut und geprüft wird** (Befehle in `Tools/`):
```powershell
# Level bauen (headless)
UnrealEditor-Cmd.exe <uproject> -run=pythonscript -script="<Tools>\build_showcase_level.py" -unattended

# Bild aufnehmen (braucht Renderer, also nicht -nullrhi)
UnrealEditor.exe <uproject> /MolecularForge/Maps/L_MF_Showcase -game -windowed `
  -resx=1600 -resy=900 -nosplash -nosound -unattended -ExecCmds="HighResShot 1600x900"
# Ergebnis: Saved/Screenshots/WindowsEditor/
```

**Fallen, die Zeit gekostet haben und wiederkommen werden:**

*Materialien brauchen das Nutzungskennzeichen.* Ohne `used_with_instanced_static_meshes`
verweigert die Engine das Material auf instanzierten Meshes und nimmt stillschweigend das
graue Standardmaterial. Der Hinweis steht nur im Log, im Bild sieht man bloß farblose Kugeln.

*Ein Prozedural-Mesh hat erst dann einen Materialschlitz, wenn es einen Abschnitt hat.*
Wird das Material vorher gesetzt — etwa beim Registrieren der Komponente —, verpufft die
Zuweisung, und Band und Oberfläche rendern schwarz. Richtig ist: unmittelbar nach
`CreateMeshSection` setzen. Auch hier hat wieder eine Entweder-oder-Probe geholfen: dem
Material ein festes Eigenleuchten geben. Blieb es dunkel, wurde es gar nicht benutzt.

*Materialzuweisung im Komponentenkonstruktor überlebt das Speichern eines Levels nicht.*
Die Materialliste nennt danach zwar das richtige Material, gerendert wird trotzdem das der
Grundform. Richtig ist: das Material als `UPROPERTY` halten und in `OnRegister` setzen.
Gefunden wurde das mit einer Entweder-oder-Probe — Material auf knallrot, Bild ansehen.
Blieb es weiß, wurde das Material nicht benutzt; das war schneller als jede Vermutung.

*Level speichern im Kommandozeilenbetrieb.* `new_level(Zielpfad)` legt zwar eine leere
Karte am Ziel an, lässt den Editor danach aber auf einer namenlosen Welt (`/Temp/Untitled`)
stehen. Alle Speicherwege melden dann brav Erfolg oder Misserfolg und schreiben ins Leere.
Richtig ist: in die vorhandene Welt bauen und mit `save_map(world, Paketpfad)` wegschreiben.
Und: den Rückgabewert nicht glauben, sondern die Dateigröße vergleichen.

*Mass-Abfragen müssen im Konstruktor an den Prozessor gebunden werden* (`: MotionQuery(*this)`).
Ohne das läuft `ConfigureQueries` in eine Zusicherung — und zwar erst beim Weltstart, nicht
beim Übersetzen. Der Editor stürzt dann kommentarlos ab.

*Die automatische Belichtung macht jede Beleuchtungsarbeit zunichte.* In einer Szene, die
zu neun Zehnteln schwarz ist, regelt sie auf, bis das Motiv ausbrennt — und gleicht jede
Absenkung der Lichtstärke wieder aus. Im Mesoskala-Bild sahen die Moleküle deshalb weiß
aus, obwohl im Instanzdatensatz nachweislich Blau und Orange standen. Richtig ist ein
unbegrenztes Post-Process-Volumen mit `auto_exposure_min_brightness == max_brightness`;
die Helligkeit stellt man danach über `auto_exposure_bias` ein, nicht über die Lampen.
**Die anderen Level haben dieses Volumen noch nicht** — beim Aufräumen nachziehen.

*`HighResShot` verliert Instanzdaten, die erst zur Laufzeit gesetzt wurden.* Das ist die
teuerste Falle dieses Projekts gewesen — sie hat einen halben Abend Fehlersuche an einer
Stelle gekostet, an der nichts kaputt war. Die Mesoskala-Moleküle waren auf jedem
`HighResShot` grau. Nachweislich stimmte dabei alles: Material hing an der Komponente,
trug das Nutzungskennzeichen, übersetzte fehlerfrei, der Instanzpuffer hatte die richtige
Größe und enthielt die richtigen Farben. Ausgeschlossen wurden nacheinander das Material
selbst (auch ein reines Rot und das Engine-Gittermaterial blieben grau), die
Komponentenklasse, Konstruktor-Unterobjekt gegen Laufzeiterzeugung, Material vor gegen nach
dem Registrieren, einmaliger gegen bildweiser Instanzaufbau, Beweglichkeit und Schattenwurf.
Alles ohne Wirkung, weil alles davon in Ordnung war.

`HighResShot` rendert die Szene in einem eigenen Durchgang neu, und dieser Durchgang bekommt
die zur Laufzeit geschriebenen Per-Instance-Daten nicht mit. Instanzen, die im Editor
angelegt und mit dem Level gespeichert wurden, sind nicht betroffen — deshalb war das
Vergleichslevel im selben Lauf farbig und die Mesoskala nicht. Genau dieser Unterschied
hätte früher auffallen müssen.

**Richtig ist: `-dumpmovie` statt `HighResShot`**, sobald Instanzen zur Laufzeit entstehen.
```powershell
UnrealEditor.exe <uproject> /MolecularForge/Maps/L_MF_Mesoscale -game -windowed `
  -ResX=1920 -ResY=1080 -nosplash -nosound `
  -dumpmovie -benchmark -deterministic -fps=30 -benchmarkseconds=16
# schreibt MovieFrame%05d.png nach Saved/Screenshots/WindowsEditor/
```
Und die allgemeine Lehre: Wenn eine Probe ein Ergebnis liefert, das *unmöglich* sein kann —
ein rein rotes Material rendert grau —, dann ist die Annahme über das Messwerkzeug falsch
und nicht die über den Prüfling. Ab da hätte ich das Aufnahmeverfahren wechseln müssen,
statt weiter am Renderer zu drehen.

*Der Verzeichnisdienst ist im Kommandozeilenbetrieb noch nicht durch.* `list_assets` auf ein
Verzeichnis voller Assets liefert dann eine leere Liste, und ein Skript meldet zufrieden
„0 verschoben", obwohl drei Dateien dort liegen. Vorher
`AssetRegistryHelpers.get_asset_registry().scan_paths_synchronous([...], force_rescan=True)`.

*`-script=` verträgt kein `\r` im Pfad.* Ein Skript namens `restructure_content.py` unter
`...\Tools\` wird zu `...\Tools` abgeschnitten, weil die Zeichenfolge `\r` unterwegs als
Wagenrücklauf gelesen wird. Die Meldung lautet dann „Could not load Python file" mit einem
Pfad, der auf den Ordner zeigt. Abhilfe: den Pfad mit Schrägstrichen übergeben.

*Kameraabstand wird an der Hülle gemessen, nicht am Mittelpunkt.* Der erste Anlauf rechnete
den Abstand aus der Kantenlänge und stellte die Kamera damit bei 1895 Einheiten in einen
Raum, der bis 1800 reicht. Richtig ist die Umkugel: `Abstand = Radius / sin(halber
Bildwinkel)`, und zwar mit dem *senkrechten* Bildwinkel, denn der ist der kleinere.

### Offene Punkte


**Impostor-Kugeln: abgebrochen, nicht aufgegeben.** Das Material steht als
`M_MF_AtomImpostor` und ist vollständig verdrahtet — Kreisausschnitt, Kugelnormale,
Tiefenversatz, Farbe. Was fehlt, ist genau ein Stück: **im Vertexshader an den Versatz
des Vertex vom Mittelpunkt seiner Instanz zu kommen.** Ohne den lässt sich das Viereck
nicht zur Kamera drehen.

Probiert und je einmal gerendert:
- `WorldPosition` minus `ObjectPositionWS` → schwarz
- `PreSkinnedPosition` → schwarz (liefert bei Static Meshes offenbar null)
- `LocalPosition` → auch als Diagnosefarbe nichts sichtbar

Was dabei **bewiesen** wurde: mit abgeschalteter Ausrichtung rendern die Vierecke sauber
an den Atompositionen. Mesh, Instanzen, Per-Instance-Daten und Materialzuweisung sind also
alle in Ordnung — der Fehler sitzt ausschließlich in der Ausrichtung. Der Diagnoseschalter
`DIAGNOSTIC_FLAT_QUADS` in `build_materials.py` stellt diesen Zustand wieder her.

**Konkreter nächster Schritt:** nicht weiter mit eigenem HLSL raten, sondern das Material
einmal von Hand im Editor zusammenklicken und nachsehen, welchen Knoten Unreal selbst für
die Eckenlage benutzt — oder gleich eine mitgelieferte Billboard-Materialfunktion über
`MaterialExpressionMaterialFunctionCall` einbinden. Ein Durchlauf im geöffneten Editor
beantwortet das schneller als weitere Rateversuche über die Kommandozeile.

**Folge für das Fab-Listing:** solange das nicht steht, darf **„150.000 Atome in Echtzeit"
nicht behauptet werden.** Zurzeit hängt an jedem Atom echte Kugelgeometrie; die Grenze
liegt bei einigen zehntausend Atomen, nicht bei hunderttausenden. Was stimmt und gesagt
werden darf: mehrere tausend Atome laufen flüssig, und die Darstellung ist korrekt.

**XTC wird vorerst nicht gelesen.** Es ist das verbreitetere Trajektorienformat (GROMACS),
komprimiert aber mit einem eigenen Ganzzahlverfahren, dessen Umsetzung mehrere hundert
Zeilen kniffliger Bitschieberei braucht. Ein Fehler darin erzeugt keine Fehlermeldung,
sondern Koordinaten, die plausibel aussehen und falsch sind — bei einer Trajektorie fällt
das niemandem auf. DCD ist dagegen ein schlichtes Binärformat mit Längenmarken um jeden
Block, an denen sich beim Lesen fortlaufend prüfen lässt, ob man noch richtig liegt.
**Für das Listing heißt das: „DCD" schreiben, nicht „MD-Trajektorien" pauschal.** Wer XTC
braucht, kann mit `gmx trjconv` umwandeln — das ist ein Einzeiler und sollte so in der Doku
stehen. Nachrüsten lohnt erst, wenn es jemand verlangt.

**Entscheidung Niagara: Array-Parameter statt eigenem Data Interface.** Ein eigenes
Interface könnte ohne Umkopieren direkt auf die Strukturdaten zugreifen und wäre bei
ständig wechselnden Daten im Vorteil — etwa während einer MD-Trajektorie. Es braucht dafür
aber eigenen HLSL-Code für den GPU-Pfad, und der lässt sich nicht headless prüfen: ein
Fehler darin fällt erst im laufenden Effekt auf, und im Zweifel erst beim Kunden.
Array-Parameter kosten ein einmaliges Umkopieren beim Setzen, laufen auf CPU- und
GPU-Simulationen unverändert und sind vollständig testbar. Bei einer statischen Struktur
wird ohnehin einmal gesetzt und danach nur gelesen. **Nachzuholen, wenn die
MD-Trajektorien stehen** — dort wird pro Bild neu gesetzt, und erst dann zahlt sich das
eigene Interface aus. Silvan sollte wissen, dass „Custom Niagara DI" damit vorerst *nicht*
im Fab-Listing behauptet werden darf.

**Der Abruf ist geprüft — und die zweistufige AlphaFold-Auflösung hat sich bezahlt gemacht.**
Die API löste auf `AF-P69905-F1-model_v6.cif` auf. Als der Code entstand, war v4 aktuell.
Hätte ich die Datei-Adresse geraten statt sie zu erfragen, wäre das Plugin heute kaputt —
und zwar still, mit einem 404 statt einer Struktur. Die Entscheidung steht damit nicht mehr
nur als Begründung im Kopf, sondern als Messwert.

**Testlücke Faltblatt-Erkennung.** Helix ist gegen eine ideal gebaute Geometrie geprüft
(φ=−57°, ψ=−47° *ist* eine α-Helix — findet das Verfahren dort keine, liegt es am
Verfahren). Für Faltblätter fehlt das Gegenstück: zwei korrekt zueinander liegende Stränge
von Hand zu konstruieren ist fehleranfällig, und ein Fehlschlag wäre nicht zuordenbar —
falsche Testgeometrie oder falsche Erkennung. Der Code ist geschrieben und folgt den
Bridge-Definitionen der Arbeit, aber er ist nicht verifiziert. **Zu schließen, sobald eine
echte Demo-Struktur mitgeliefert wird** (kommt für Fab ohnehin): dann gegen eine Struktur
mit bekanntem Faltblattanteil prüfen.

**Frage an Silvan — Copyright-Zeile.** Die Header stehen inzwischen auf
`Copyright Simulated Flow`. Nach der bisherigen Aufteilung gehören Plugins zum Freelancer
Simulated Flow und nur Merch/Hardware zur UG. Ich habe nichts zurückgedreht. Wenn die
Aufteilung weiter gilt, muss das vor dem Fab-Listing einheitlich sein — die Zeile steht
in jeder Datei und im `.uplugin` unter `CreatedBy`.

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
| `MolecularForgeNiagara` | Runtime | Atompositionen/Farben/Radien/Bindungen als Niagara-Array-Parameter |
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
- [x] Sekundärstruktur: aus `HELIX`/`SHEET` bzw. `struct_conf` lesen, sonst nach
      Kabsch/Sander (DSSP) berechnen — Amid-H geschätzt, H-Brücken-Energie über
      Nachbarschaftsgitter parallel, Muster für Helix/Faltblatt/Turn. Prolin fällt als
      Donor aus. Ladeoption `SecondaryStructureSource` steuert Datei/Rechnen/beides.
      Die Faltblatt-Suche läuft über die gefundenen Brücken statt über alle Residuenpaare —
      sonst wäre sie quadratisch und bei einem Ribosom unbrauchbar.
- [x] Ball-and-Stick vollständig: `UMolecularBondsComponent` zeichnet Bindungen als
      instanzierte Halbzylinder, jede Hälfte in der Farbe ihres eigenen Atoms
- [x] Rückgrat-Spline (`MolBackboneSpline`): Catmull-Rom durch die Ankeratome, Querrichtung
      aus der Carbonylgruppe statt aus einem festen Hochvektor, Umdrehkorrektur gegen das
      Kippen im Faltblatt, Trennung an Lücken statt Interpolation über sie hinweg
- [x] Cartoon/Ribbon: `MolRibbonBuilder` zieht ein Superellipsen-Profil entlang der Spline —
      rund für Schleifen, flach für Helices, flach mit Pfeilspitze für Faltblätter.
      Profilmaße werden über die Sekundärstruktur-Grenzen geglättet, die Pfeilspitze erst
      danach aufgesetzt und bleibt deshalb scharf. Ausgabe als neutrale Arrays
      (`FMolMeshData`), getragen von `UMolecularCartoonComponent` (ProceduralMeshComponent).
- [x] `AMolecularStructureActor` schaltet Kugeln, Stäbe und Band über eine
      Darstellungsart um, statt drei Komponenten einzeln bedienen zu lassen
- [x] `MolecularForgeWeb`: AlphaFold-DB (`/api/prediction/{uniprot}`) + RCSB-Abruf, lokaler Cache.
      Kennungsprüfung als Positivliste, zweistufiger AlphaFold-Abruf über die API, Download und
      Parsen außerhalb des Spielthreads, Blueprint-Knoten „Struktur holen", Cache-Verwaltung.
      *Der Abruf selbst ist nicht automatisiert testbar — siehe offene Punkte.*
- [ ] Editor-Import-Factory: `.pdb`/`.cif` ins Content-Browser ziehen → Asset

### Phase 3 — Bewegung und Effekt
- [x] Moleküloberfläche: Gauß-Dichtegitter + Isoflächen-Extraktion, `UMolecularSurfaceComponent`.
      **Marching Tetraeder statt Marching Cubes** — die Würfelvariante braucht eine Tabelle mit
      256 Fällen, und eine falsche Zeile darin erzeugt Löcher, die erst im fertigen Bild
      auffallen. Die Tetraedervariante kommt ohne Tabelle aus und ist per Konstruktion dicht;
      der Preis sind etwa doppelt so viele Dreiecke. Falls das je stört, ist der Austausch
      lokal auf eine Funktion begrenzt.
      *Es ist ausdrücklich eine Gauß-Oberfläche, nicht die solvent-excluded surface nach
      Connolly — für Anschauung nicht zu unterscheiden, zum Ausmessen ungeeignet.*
- [x] Niagara-Anbindung: `MolecularForgeNiagara` übergibt Atompositionen, Farben, Radien und
      optional Bindungen als Array-Parameter — Auflösen, Falten, Docking-VFX.
      Filter und Färbung entsprechen exakt der Kugeldarstellung, damit Mesh und Partikel
      dasselbe Molekül gleich zeigen. Ausdünnen auf eine Obergrenze verteilt gleichmäßig
      über die Struktur statt vorne abzuschneiden.
      **Bewusst über Array-Parameter statt über ein eigenes Data Interface** — Begründung
      unter „Offene Punkte".
- [x] MD-Trajektorien abspielen: `UMolecularTrajectory` (flaches Array, Bild für Bild),
      DCD-Leser für beide Byte-Reihenfolgen, `UMolecularTrajectoryPlayer` mit Interpolation,
      Schleife, Schieberegler-Anbindung und Wiederherstellen des Ausgangszustands.
      Kugeln und Stäbe bekommen dafür ein schnelles Aktualisieren, das nur
      Instanztransformationen neu schreibt statt alles neu aufzubauen.
      **XTC ist bewusst nicht dabei** — Begründung unter „Offene Punkte".
- [x] Selektionssprache in PyMOL-Schreibweise: Klassen (`protein`, `water`, `ligand`,
      `backbone`, `sidechain`, …), Terme mit Argument (`chain`, `resi`, `resn`, `name`,
      `element`, `ss`, `b > 50`), `within X of …`, dazu `and`/`or`/`not` und Klammern.
      Fehler werden mit Grund und Zeichenposition gemeldet.
- [x] Messwerkzeuge: Abstand, Bindungswinkel, Torsionswinkel, Mittelpunkt, Massenschwerpunkt,
      Trägheitsradius, Hülle, RMSD (auch maskiert). Atommassen als eigene Tabelle.
- [x] Beides über Blueprint erreichbar (`UMolecularForgeLibrary`)

### Phase 4 — Maßstabssprung (MassAI)
- [x] `MolecularForgeMass`: Fragmente, Tag, Const-Shared-Parameter, `UMolMesoscaleTrait`
- [x] Brownsche Diffusion als Mass-Processor (`UMassProcessor_MolecularMotion`), mit
      Randbedingung und Detailstufe im selben Durchlauf — alle drei fassen dieselben Daten an
- [x] Bindungs-/Löseereignisse über Nachbarschaftsgitter (`UMassProcessor_MolecularBinding`),
      zweistufig: serielles Abbild + Gitter, dann parallele Entscheidung
- [x] `AMolecularMesoRenderer`: gestaffelte Darstellung — echte Atome für die nächsten
      Moleküle aus einem festen Vorrat, Instanzen für die mittlere Stufe, Kugeln für die ferne
- [ ] Zell-Innenraum-Szene: Organellen + Molekülpopulationen ⏸ *braucht Editor*
- [ ] Demo-Level „Partielle Zellreprogrammierung": Yamanaka-Faktoren binden an Chromatin
      ⏸ *braucht Editor*

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
