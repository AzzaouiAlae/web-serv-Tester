SRC := $(shell find . -name "*.cpp")
OBJ = $(SRC:%.cpp=%.o)

INCLUDES := $(shell find . -name "*.hpp" -exec dirname {} \; | sort -u | awk '{printf "-I%s ", $$1}')

CXX = c++
CXXFLAGS = $(INCLUDES)  -Wall -Wextra -Werror -g3
NAME = servTester.out

all : $(NAME)

$(NAME): $(OBJ)
	$(CXX) $(CXXFLAGS) $(OBJ) -o $(NAME)

clean :
	-rm $(OBJ)

fclean : clean
	-rm $(NAME)

re : fclean $(NAME)

.PHONY: all clean fclean re