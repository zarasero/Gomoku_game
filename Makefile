NAME = Gomoku

CXX = c++
CXXFLAGS = -Wall -Wextra -Werror -std=c++20 -pthread -Iinclude

SRCS = \
	src/main.cpp \
	src/App.cpp \
	src/Board.cpp \
	src/Rules.cpp \
	src/GameState.cpp \
	src/AI.cpp \
	src/Renderer.cpp \
	src/Controller.cpp \
	src/Zobrist.cpp \
	src/Move.cpp

OBJS = $(SRCS:.cpp=.o)

LDFLAGS = -pthread -lsfml-graphics -lsfml-window -lsfml-system

all: $(NAME)

$(NAME): $(OBJS)
	$(CXX) $(CXXFLAGS) $(OBJS) -o $(NAME) $(LDFLAGS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS)

fclean: clean
	rm -f $(NAME)

re: fclean all

run: $(NAME)
	./$(NAME)

.PHONY: all clean fclean re run
