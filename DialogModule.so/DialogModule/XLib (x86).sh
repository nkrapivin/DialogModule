cd "${0%/*}"
g++ -c -std=c++17 "GameMaker.cpp" "XLib.cpp" "lodepng.cpp" -fPIC -m32                                                       # Linux/BSD
# g++ -c -std=c++17 "GameMaker.cpp" "XLib.cpp" "lodepng.cpp" -fPIC -m32 -I/opt/X11/include                                  # macOS
g++ "GameMaker.o" "XLib.o" "lodepng.o" -o "DialogModule (x86)/DialogModule.so" -shared -fPIC -m32 -lprocps                  # Linux
# g++ "GameMaker.o" "XLib.o" "lodepng.o" -o "DialogModule (x86)/DialogModule.dylib" -shared -fPIC -m32 -lX11 -L/opt/X11/lib # macOS
# g++ "GameMaker.o" "XLib.o" "lodepng.o" -o "DialogModule (x86)/DialogModule.so" -shared -fPIC -m32 -lutil                  # BSD
