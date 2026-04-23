CC = gcc
CFLAGS = -Wall -Wextra -std=c11
# Full dependencies needed for raylib on Ubuntu/WSL
LIBS = -lraylib -lGL -lm -lpthread -ldl -lrt -lX11 -lXrandr -lXi -lXcursor -lXinerama -lXext

all: cosmos_sim

cosmos_sim: cosmos_sim.c
	$(CC) $(CFLAGS) cosmos_sim.c -o cosmos_sim $(LIBS)

clean:
	rm -f cosmos_sim
