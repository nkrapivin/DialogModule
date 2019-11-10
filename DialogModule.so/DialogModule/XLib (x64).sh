cd "${0%/*}"
g++ -c -std=c++17 "GameMaker.cpp" "XLib.cpp" "lodepng.cpp" -fPIC -m64
g++ "GameMaker.o" "XLib.o" "lodepng.o" -o "DialogModule (x64)/DialogModule.so" -shared -fPIC -m64 -lprocps
