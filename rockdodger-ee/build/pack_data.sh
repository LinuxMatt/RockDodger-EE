#!/bin/sh

OUTFILE=../src/data.h

cd ../data/
echo BUILDING $OUTFILE ...
xxd -i 20P_Betadance.png > $OUTFILE
xxd -i 4est_fulla3s.mod >> $OUTFILE
xxd -i boom.wav >> $OUTFILE
xxd -i booom.wav >> $OUTFILE
xxd -i bzboom.wav >> $OUTFILE
xxd -i cboom.wav >> $OUTFILE
xxd -i deadrock0.bmp >> $OUTFILE
xxd -i deadrock1.bmp >> $OUTFILE
xxd -i deadrock2.bmp >> $OUTFILE
xxd -i deadrock3.bmp >> $OUTFILE
xxd -i deadrock4.bmp >> $OUTFILE
xxd -i deadrock5.bmp >> $OUTFILE
xxd -i dodgers.png >> $OUTFILE
xxd -i game.png >> $OUTFILE
xxd -i gauge.png >> $OUTFILE
xxd -i getzznew.mod >> $OUTFILE
xxd -i greeblie0.bmp >> $OUTFILE
xxd -i laser.png >> $OUTFILE
xxd -i laser0.png >> $OUTFILE
xxd -i laser1.png >> $OUTFILE
xxd -i laserpowerup.png >> $OUTFILE
xxd -i magic.mod >> $OUTFILE
xxd -i over.png >> $OUTFILE
xxd -i paused.png >> $OUTFILE
xxd -i rock.png >> $OUTFILE
xxd -i rock0.bmp >> $OUTFILE
xxd -i rock1.bmp >> $OUTFILE
xxd -i rock2.bmp >> $OUTFILE
xxd -i rock3.bmp >> $OUTFILE
xxd -i rock4.bmp >> $OUTFILE
xxd -i rock5.bmp >> $OUTFILE
xxd -i shield.png >> $OUTFILE
xxd -i shield0.png >> $OUTFILE
xxd -i shield1.png >> $OUTFILE
xxd -i shieldpowerup.png >> $OUTFILE
xxd -i ship.bmp >> $OUTFILE
xxd -i ship2.bmp >> $OUTFILE
xxd -i ship_small.bmp >> $OUTFILE
xxd -i speedup.wav >> $OUTFILE
xxd -i url.png >> $OUTFILE

