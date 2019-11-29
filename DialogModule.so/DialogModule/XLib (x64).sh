cd "${0%/*}"
g++ -c -std=c++17 "GameMaker.cpp" "XLib.cpp" "lodepng.cpp" -fPIC -m64                                                       # Linux/BSD
# g++ -c -std=c++17 "GameMaker.cpp" "XLib.cpp" "lodepng.cpp" -fPIC -m64 -I/opt/X11/include                                  # macOS
g++ "GameMaker.o" "XLib.o" "lodepng.o" -o "DialogModule (x64)/DialogModule.so" -shared -fPIC -m64 -lprocps                  # Linux
# g++ "GameMaker.o" "XLib.o" "lodepng.o" -o "DialogModule (x64)/DialogModule.dylib" -shared -fPIC -m64 -lX11 -L/opt/X11/lib # macOS
# g++ "GameMaker.o" "XLib.o" "lodepng.o" -o "DialogModule (x64)/DialogModule.so" -shared -fPIC -m64 -lutil                  # BSD
