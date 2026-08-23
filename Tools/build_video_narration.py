# Copyright 2026 Silvan Teufel All Rights Reserved.
#
# Vertont den Kommentartext und legt ihn unter das Mesoskala-Video.
#
# Stimme: Kokoro-82M (kokoro-onnx), englische Standardstimme af_bella, Apache-2.0 und
# damit kommerziell frei verwendbar. Das Modell liegt lokal; es wird nichts hochgeladen
# und nichts nachgeladen.
#
# Der Kommentar ist laenger als der Videoausschnitt. Statt das Bild einzufrieren laeuft
# das Video in einer Schleife, bis der Ton zu Ende ist — bei einer diffundierenden
# Population faellt der Sprung am Schleifenende kaum auf, weil es keine feste Bildkomposition
# gibt, die sich zurueckstellt.
#
# Aufruf (ausserhalb von Unreal):
#   python Tools/build_video_narration.py

import os
import subprocess
import wave

import numpy as np
from kokoro_onnx import Kokoro

HIER = os.path.dirname(os.path.abspath(__file__))
PLUGIN = os.path.normpath(os.path.join(HIER, ".."))

MODELL = os.path.join(os.path.expanduser("~"), "AppData", "Local", "hermes",
                      "tts", "kokoro", "kokoro-v1.0.onnx")
STIMMEN = os.path.join(os.path.expanduser("~"), "AppData", "Local", "hermes",
                       "tts", "kokoro", "voices-v1.0.bin")

TEXTDATEI = os.path.join(PLUGIN, "Docs", "Video_Kommentar.txt")
STUMMES_VIDEO = os.path.join(PLUGIN, "Docs", "Video", "MolecularForge_Mesoskala.mp4")
TONSPUR = os.path.join(PLUGIN, "Docs", "Video", "MolecularForge_Kommentar.wav")
ZIEL = os.path.join(PLUGIN, "Docs", "Video", "MolecularForge_Mesoskala_kommentiert.mp4")
PENDEL = os.path.join(PLUGIN, "Docs", "Video", "_pendel.mp4")

STIMME = "af_bella"
TEMPO = 0.95          # etwas ruhiger als die Vorgabe; der Text ist dicht
PAUSE_SEKUNDEN = 0.45  # zwischen den Absaetzen


def absaetze():
    with open(TEXTDATEI, "r", encoding="utf-8") as f:
        roh = f.read()
    return [a.strip().replace("\n", " ") for a in roh.split("\n\n") if a.strip()]


def vertone():
    # Die Vertonung dauert Minuten. Aendert sich nur die Kodierung, waere ein zweiter Lauf
    # reine Wartezeit — deshalb wird eine vorhandene, aktuelle Tonspur wiederverwendet.
    if (os.path.isfile(TONSPUR)
            and os.path.getmtime(TONSPUR) > os.path.getmtime(TEXTDATEI)):
        with wave.open(TONSPUR, "rb") as w:
            dauer = w.getnframes() / float(w.getframerate())
        print("Vorhandene Tonspur wiederverwendet (%.1f s)." % dauer)
        return dauer

    kokoro = Kokoro(MODELL, STIMMEN)

    teile = []
    rate = None
    for i, absatz in enumerate(absaetze(), 1):
        proben, rate = kokoro.create(absatz, voice=STIMME, speed=TEMPO, lang="en-us")
        teile.append(np.asarray(proben, dtype=np.float32))
        teile.append(np.zeros(int(rate * PAUSE_SEKUNDEN), dtype=np.float32))
        print("  Absatz %2d: %5.1f s" % (i, len(proben) / float(rate)))

    ton = np.concatenate(teile)

    # Auf sechzehn Bit schreiben, mit etwas Luft nach oben gegen Uebersteuerung.
    spitze = float(np.max(np.abs(ton))) or 1.0
    ganz = np.int16(np.clip(ton / spitze * 0.92, -1.0, 1.0) * 32767)

    with wave.open(TONSPUR, "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(rate)
        w.writeframes(ganz.tobytes())

    dauer = len(ton) / float(rate)
    print("Tonspur: %s (%.1f s)" % (TONSPUR, dauer))
    return dauer


def baue_pendel():
    """
    Baut aus dem Ausschnitt einen Vor-und-Zurueck-Lauf.

    Ein schlichter Rundlauf springt am Ende hart auf den Anfang zurueck, und bei einem
    Kommentar von viereinhalb Minuten passiert das ueber fuenfzig Mal. Haengt man den
    rueckwaerts laufenden Ausschnitt an, schliesst sich die Schleife nahtlos: das letzte
    Bild des Rueckwaertsteils ist wieder das erste des Vorwaertsteils.

    Rueckwaerts laufende Diffusion sieht aus wie Diffusion — das ist der Grund, warum das
    hier zulaessig ist und bei einer gerichteten Bewegung nicht waere.
    """
    subprocess.run([
        "ffmpeg", "-y", "-i", STUMMES_VIDEO,
        "-filter_complex", "[0:v]split[a][b];[b]reverse[r];[a][r]concat=n=2:v=1[v]",
        "-map", "[v]", "-an",
        "-c:v", "libx264", "-preset", "slow", "-crf", "18", "-pix_fmt", "yuv420p",
        PENDEL,
    ], check=True, capture_output=True)
    return PENDEL


def mische(dauer):
    quelle = baue_pendel()

    # -stream_loop -1 wiederholt endlos, -shortest beendet, sobald der Ton zu Ende ist.
    #
    # Gemessene Groessen bei viereinhalb Minuten 1080p30: CRF 20 -> 230 MB, CRF 26 ->
    # 113 MB, CRF 30 -> siehe Ausgabe. Das Motiv ist schwarzer Hintergrund mit glatten,
    # einfarbigen Kugeln; dort kostet eine hoehere Stufe kaum sichtbare Qualitaet, aber
    # ein Listing-Video von ueber hundert Megabyte laedt niemand gern.
    befehl = [
        "ffmpeg", "-y",
        "-stream_loop", "-1", "-i", quelle,
        "-i", TONSPUR,
        "-map", "0:v:0", "-map", "1:a:0",
        "-c:v", "libx264", "-preset", "slow", "-crf", "30", "-pix_fmt", "yuv420p",
        "-g", "150",
        "-c:a", "aac", "-b:a", "128k",
        "-shortest", "-movflags", "+faststart",
        ZIEL,
    ]
    subprocess.run(befehl, check=True, capture_output=True)

    if os.path.isfile(PENDEL):
        os.remove(PENDEL)

    print("Video: %s (%.1f MB, %.0f s)" % (
        ZIEL, os.path.getsize(ZIEL) / (1024.0 * 1024.0), dauer))


def main():
    print("=" * 78)
    print("MolecularForge — Video vertonen (Kokoro-82M, %s)" % STIMME)
    print("=" * 78)

    for pfad in (MODELL, STIMMEN, TEXTDATEI, STUMMES_VIDEO):
        if not os.path.isfile(pfad):
            print("FEHLT: %s" % pfad)
            return 1

    dauer = vertone()
    mische(dauer)
    return 0


raise SystemExit(main())
