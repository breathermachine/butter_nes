VPATH=../../nes
butter_nes: utils.o cpu_ppu.o cpu_sm.o cpu_apu.o cpu.o cpudecode.o SDL_graphics_system.o main.o
	g++ -lSDL2main -lSDL2 -o $@ $?
	echo "Build Done."
