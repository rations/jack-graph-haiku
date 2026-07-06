# Builds on Haiku only (uses findpaths and the non-packaged JACK install).
CXX = g++
NONPACKAGED = /boot/system/non-packaged
# hmulti_audio.h (kernel audio driver contract) ships as a private development
# header; locate it the same way the JACK port's build does.
AUDIO_PRIVATE := $(shell findpaths -e B_FIND_PATH_DEVELOP_DIRECTORY headers/private/audio 2>/dev/null)
CXXFLAGS = -std=c++17 -Wall -Wextra -MMD -MP \
           -I$(NONPACKAGED)/include $(if $(AUDIO_PRIVATE),-I$(AUDIO_PRIVATE))
LDFLAGS = -L$(NONPACKAGED)/lib -ljack -lbe

SOURCES = src/main.cpp src/JackGraphApp.cpp src/MainWindow.cpp \
          src/GraphCanvasView.cpp src/Node.cpp src/Connection.cpp \
          src/ClientBox.cpp src/Config.cpp src/JackClient.cpp \
          src/JackServerControl.cpp src/SettingsWindow.cpp
OBJECTS = $(SOURCES:.cpp=.o)
DEPS    = $(OBJECTS:.o=.d)
TARGET = JackGraph

$(TARGET): $(OBJECTS)
	$(CXX) $(OBJECTS) -o $(TARGET) $(LDFLAGS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

-include $(DEPS)

clean:
	rm -f $(OBJECTS) $(DEPS) $(TARGET)

.PHONY: clean
