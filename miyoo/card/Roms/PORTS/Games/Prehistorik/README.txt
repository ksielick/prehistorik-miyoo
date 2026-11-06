prehistorik-miyoo - Prehistorik (1991) on the Miyoo Mini Plus (OnionOS)
=======================================================================

A modified build of Olduvai, a from-scratch engine for Prehistorik by
Krzysztof Sokolowski (https://github.com/ksokolowski/olduvai, GPL-3.0), ported
to the Miyoo Mini Plus: https://github.com/ksielick/prehistorik-miyoo
It is not made or endorsed by the Olduvai project; report problems with this
port there, not to Olduvai. It ships no game content: you need your own copy
of the game.

Game files
----------
Put these into  Roms/PORTS/Games/Prehistorik/game/ :

    FILESA.CUR  FILESB.CUR  FILESA.VGA  FILESB.VGA
    and EITHER  HISTORIK.EXE  OR  PREH.SQZ  (GOG's Prehistorik 1+2 has PREH.SQZ)

If the DOS game is already on the card as Roms/DOS/Prehistorik (1991).zip (for
DOSBox), the first launch takes the files from that zip by itself; the zip is
left as it is.

Starting
--------
Ports -> Platform -> Prehistorik (1991).

Buttons
-------
    D-pad        move
    A            jump, confirm in menus
    B            attack, back in menus
    START        pause menu (Options, Quit)
    MENU         pause menu
    SELECT + R1  quicksave        SELECT + L1  quickload

Quit with START -> Quit -> Exit Game -> Yes.

Style
-----
It starts in Classic DOS: the 1991 game, with smooth 60 Hz pans and fades between
screens ("transitions": "classic" in conf/olduvai/play.json brings back the
original steps). Options -> Style switches to Enhanced HD (smooth motion at 60
fps, upscaled graphics). The choice is kept.

Files here
----------
    conf/olduvai/play.json   settings (delete it to go back to the defaults)
    conf/olduvai/saves/      the quicksave
    log.txt                  the last run's log (quote its first lines in a bug report)
    playlog.on               create this empty file to log frame times to playlog/
