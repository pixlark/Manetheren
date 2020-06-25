NAME=raycast
SOURCE=raycast.cc
HEADERS=raycast.h
LIBS=-lSDL2 -lSDL2_ttf
OPTIONS=-g -Wall -Werror
DISABLED=-Wno-unused

all: raycast

$(NAME): $(SOURCE) $(HEADERS)
	clang++ $(OPTIONS) $(DISABLED) $(SOURCE) -o $(NAME) $(LIBS)
