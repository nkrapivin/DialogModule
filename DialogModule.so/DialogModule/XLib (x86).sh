cd "${0%/*}"
g++ -c -std=c++17 "GameMaker.cpp" "XLib.cpp" "lodepng.cpp" -fPIC -m32
g++ "GameMaker.o" "XLib.o" "lodepng.o" -o "DialogModule (x86)/DialogModule.so" -shared -fPIC -m32
