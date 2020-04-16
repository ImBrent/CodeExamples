CFLAGS = -D_POSIX_SOURCE
CC = cc

all: Controller Player

Controller: controller.c
			$(CC) $(CFLAGS) -o Controller controller.c

Player: player.c
			$(CC) $(CFLAGS) -o Player player.c
			
clean:
		rm -f Controller
		rm -f Player