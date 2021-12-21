BIN_DIR=bin
SRC_DIR=src
OBJ_DIR=obj
TEST_DIR=test
TEST_INPUT_DIR=test/input/
CC=g++
CC_FLAG= -Wall --std=c++17

# set optional flags

ifdef OPT
OPTIONAL_FLAGS+=-O3
endif

ifdef SYM
OPTIONAL_FLAGS+=-g
endif

CC_FLAG+= $(OPTIONAL_FLAGS)



# first convert all .c extension to .cpp, then everything into .o
_TMP = $(patsubst $(SRC_DIR)/%.cpp, $(SRC_DIR)/%.cpp,$(wildcard $(SRC_DIR)/*.c*))
OBJ = $(patsubst $(SRC_DIR)/%.cpp, $(OBJ_DIR)/%.o,$(_TMP))
OBJN = $(filter-out $(OBJ_DIR)/main.o,$(OBJ))

EXE=test

#-- MAIN BUILD TARGETS ----------------------------------------------------#

all: lib exe

lib: $(OBJN)

run: lib exe
	$(info )	
	$(info --------------------------------------------------------------------------------)
	@ ./$(BIN_DIR)/$(EXE) $(TEST_INPUT_DIR)/test02 
	@# ./$(BIN_DIR)/$(EXE)

debug:
	$(info )	
	$(info --------------------------------------------------------------------------------)
	@ gdb --args ./$(BIN_DIR)/$(EXE) $(TEST_INPUT_DIR)/test02 


exe: lib
	$(info )	
	$(info --------------------------------------------------------------------------------)
	$(info Compiling [$(OPTIONAL_FLAGS)] exe bin/test ... )	
	@time -f "Compiled [$(OPTIONAL_FLAGS)] exe in %e seconds" $(CC) $(CC_FLAG) $(SRC_DIR)/main.cpp $(OBJN) -o $(BIN_DIR)/$(EXE)

$(OBJN): $(OBJ_DIR)/%.o:$(SRC_DIR)/%.cpp 
	@time -f "Compiled [$(OPTIONAL_FLAGS)] $< in %e seconds" $(CC) $(FLAGS) -MP -MMD -c $< -o $@
	@rm -f $@.comptime

#-- MISC TARGETS ---------------------------------------------------------#

show:
	@echo CC CMD = $(CC) $(FLAGS) -MP -MMD -c
	@echo OBJN = $(OBJN) 
	@echo OBJ SIZE = `du -sh --exclude "*.d" obj/ | cut -f1 `
	@echo BIN SIZE = `du -sh bin/test | cut -f1 `

setup:
	mkdir -p $(BIN_DIR) $(OBJ_DIR) 

clean:
	rm -f bin/* obj/*
