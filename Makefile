NAME=raycast
SOURCE=raycast.cc
LIBS=-lSDL2
OPTIONS=-g -Wall -Werror
DISABLED=-Wno-unused

all: raycast

$(NAME): $(SOURCE)
	clang++ $(OPTIONS) $(DISABLED) $(SOURCE) -o $(NAME) $(LIBS)
