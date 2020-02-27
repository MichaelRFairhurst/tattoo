tattoo.png: tattoo
	./tattoo tattoo.png

tattoo: tattoo.c
	gcc tattoo.c -lcairo -o tattoo -g
