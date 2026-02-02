CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -I.. -I../..
TARGET = tests

SRCS = test_main.cpp \
       test_parameter.cpp \
       test_menu_state.cpp \
       test_format_utils.cpp \
       test_cv_mapping.cpp \
       test_midi_processor.cpp \
       test_parameter_templates.cpp \
       test_encoder_controller.cpp \
       test_application_context.cpp \
       test_midi_dispatcher.cpp \
       test_display_queue.cpp

OBJS = $(SRCS:.cpp=.o)

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)

run: $(TARGET)
	./$(TARGET)

.PHONY: all clean run
