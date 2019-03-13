CPP := g++


BUILD_DIR = build
CPPFLAGS = -std=c++17 -Wall -Wextra -Wshadow -Weffc++ -Wstrict-aliasing -pedantic -Werror $(INCLUDE_DIR)
DEP_DIR := $(BUILD_DIR)/dependency
DEPFLAGS = -MT $@ -MMD -MP -MF $(DEP_DIR)/$*.Td
LDIR = src/lib
SRC_DIR = src
CPP_LIBS = -lstdc++
COMPILE_CPP = $(CPP) $(DEPFLAGS) $(CPPFLAGS) $(CPP_LIBS) -c $<
POSTCOMPILE = @mv -f $(DEP_DIR)/$*.Td $(DEP_DIR)/$*.d && touch $@
INCLUDE_DIR = -Isrc/include \
              -Isrc/utilities
HEADERS = $(wildcard $(INCLUDE_DIR)/*.h)
DEPS = $(patsubst %,$ (INCLUDE_DIR)/%, $(HEADERS))

BIN_SRC = $(wildcard $(SRC_DIR)/servers/*.cpp) $(wildcard $(SRC_DIR)/clients/*.cpp)
SRC = $(wildcard $(SRC_DIR)/*/*.cpp)
CPPFLAGS += -DDEBUG -ggdb -O0
OBJ_DIR = $(BUILD_DIR)/obj
OBJ = $(filter-out $(OBJ_DIR)/clients/%.o $(OBJ_DIR)/servers/%.o, $(patsubst $(SRC_DIR)%.cpp, $(OBJ_DIR)%.o, $(SRC)))
BIN_DIR = $(BUILD_DIR)/bin
BIN = $(patsubst $(SRC_DIR)%.cpp, $(BIN_DIR)%, $(BIN_SRC))
DEP_DIR_SUBDIR = $(patsubst $(SRC_DIR)/%, $(DEP_DIR)/%, ${sort ${dir ${wildcard ${SRC_DIR}/*/ ${SRC_DIR}/*/*/}}})
BIN_DIR_SUBDIR = $(patsubst $(SRC_DIR)/%, $(BIN_DIR)/%, ${sort ${dir ${wildcard ${SRC_DIR}/*/ ${SRC_DIR}/*/*/}}})
OBJ_DIR_SUBDIR = $(patsubst $(SRC_DIR)/%, $(OBJ_DIR)/%, ${sort ${dir ${wildcard ${SRC_DIR}/*/ ${SRC_DIR}/*/*/}}})


.PHONY: clean release debug all

debug : $(BUILD_DIR) $(BIN)

$(BIN_DIR)/% : $(OBJ_DIR)/%.o $(OBJ)
	$(CPP) -o $@ $< $(OBJ) $(CPP_LIBS)
	
$(BUILD_DIR) :
	mkdir -p $(DEP_DIR_SUBDIR) $(BIN_DIR_SUBDIR) $(OBJ_DIR_SUBDIR)

$(OBJ_DIR)/%.o : $(SRC_DIR)/%.cpp $(DEP_DIR)/%.d
		$(COMPILE_CPP) -o $@
		$(POSTCOMPILE)

$(DEP_DIR)/%.d: ;
.PRECIOUS: $(DEP_DIR)/%.d

include $(wildcard $(patsubst %,$(DEP_DIR)/%.d,$(basename $(SRCS))))

clean:
	rm -rf build

