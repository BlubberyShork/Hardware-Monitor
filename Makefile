
CXX := g++
CXXFLAGS := -Wall -Wextra -std=c++17 -D_CRT_SECURE_NO_WARNINGS -DUNICODE -D_UNICODE
LDFLAGS := -lole32 -lwbemuuid -lcomsuppw -luuid

TARGET := System_Info

SRCS := GraphicsProcessor.cpp \
        hardware_info.cpp \
        motherboard.cpp \
        processor.cpp \
        projutils.cpp \
        storagedevice.cpp \
        System_Info.cpp

OBJS := $(SRCS:.cpp=.o)

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $(OBJS) $(LDFLAGS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)
