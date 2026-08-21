# Copyright 2026 Simulated Flow All Rights Reserved.
#
# Erzeugt Resources/Icon128.png.
#
# Das Symbol zeigt, was das Plugin macht: ein paar Atome mit Bindungen, in denselben
# CPK-Farben, die auch die Darstellung benutzt. Kein Schriftzug — bei 128 Pixeln ist Text
# in der Plugin-Liste ohnehin nicht zu lesen.
#
# Aufruf (ausserhalb von Unreal):
#   python Tools/make_icon.py

import math
import os

from PIL import Image, ImageDraw

ZIEL = os.path.join(os.path.dirname(os.path.abspath(__file__)),
                    "..", "Resources", "Icon128.png")

KANTE = 128
UEBERABTASTUNG = 8  # weiche Kanten ohne Weichzeichner

HINTERGRUND = (18, 22, 30, 255)
BINDUNG = (150, 158, 172, 255)

# Dieselben Farben wie in der Elementtabelle des Plugins.
KOHLENSTOFF = (58, 58, 58)
SAUERSTOFF = (240, 60, 40)
STICKSTOFF = (60, 110, 240)
WASSERSTOFF = (235, 235, 235)

# (x, y, Radius, Farbe) in Einheiten von 0..1. Ein kleines Molekuel, schraeg gestellt,
# damit es nicht wie ein Diagramm wirkt.
ATOME = [
    (0.30, 0.62, 0.155, SAUERSTOFF),
    (0.50, 0.50, 0.185, KOHLENSTOFF),
    (0.70, 0.38, 0.155, STICKSTOFF),
    (0.44, 0.24, 0.095, WASSERSTOFF),
    (0.62, 0.74, 0.095, WASSERSTOFF),
]

BINDUNGEN = [(0, 1), (1, 2), (1, 3), (1, 4)]


def zeichne():
    gross = KANTE * UEBERABTASTUNG
    bild = Image.new("RGBA", (gross, gross), HINTERGRUND)
    stift = ImageDraw.Draw(bild)

    def px(wert):
        return wert * gross

    # Bindungen zuerst, damit die Kugeln sie ueberdecken.
    for a, b in BINDUNGEN:
        ax, ay = px(ATOME[a][0]), px(ATOME[a][1])
        bx, by = px(ATOME[b][0]), px(ATOME[b][1])
        stift.line([(ax, ay), (bx, by)], fill=BINDUNG, width=int(px(0.045)))

    for x, y, radius, farbe in ATOME:
        mx, my, r = px(x), px(y), px(radius)

        # Ein billiger Lichtverlauf: mehrere Kreise, die zum Licht hin heller werden.
        stufen = 14
        for i in range(stufen):
            t = i / float(stufen - 1)
            schrumpf = r * (1.0 - t * 0.55)
            versatz = r * t * 0.34
            hell = 0.55 + 0.65 * t
            ton = tuple(min(255, int(k * hell)) for k in farbe) + (255,)
            stift.ellipse(
                [mx - schrumpf - versatz, my - schrumpf - versatz,
                 mx + schrumpf - versatz, my + schrumpf - versatz],
                fill=ton)

    return bild.resize((KANTE, KANTE), Image.LANCZOS)


def main():
    ziel = os.path.normpath(ZIEL)
    os.makedirs(os.path.dirname(ziel), exist_ok=True)
    zeichne().save(ziel, "PNG")
    print("Symbol geschrieben: %s (%d Byte)" % (ziel, os.path.getsize(ziel)))


main()
