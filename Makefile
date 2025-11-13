CXX = g++
flags = -std=c++11 -Isofa/20231011/c/src
libs = -Lsofa/20231011/c/src -lsofa_c -lglfw -lGL -lGLEW -ldl -lX11 -lXi -lXcursor
 
CXXFLAGS = -Iimgui -Iimgui/backends -Wall -std=c++17
LIBS = -Lsofa/20231011/c/src -lsofa_c -lglfw -lGL -ldl -lpthread
SRCS = main.cpp imgui/imgui.cpp imgui/imgui_demo.cpp imgui/imgui_draw.cpp imgui/imgui_tables.cpp imgui/imgui_widgets.cpp imgui/backends/imgui_impl_glfw.cpp imgui/backends/imgui_impl_opengl3.cpp ImGuiDatePicker/ImGuiDatePicker.cpp

sun_angle: $(SRCS)
	$(CXX) $(CXXFLAGS) $(SRCS) $(LIBS) -o main
	clear
	./main

main: main.cpp
	$(CXX) $(flags) main.cpp $(libs) -o main
	clear
	./main

