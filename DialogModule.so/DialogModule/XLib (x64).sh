cd "${0%/*}"
g++ -c -std=c++17 "GameMaker.cpp" "XLib.cpp" -fPIC -m64
g++ "GameMaker.o" "XLib.o" -o "DialogModule (x64)/DialogModule.so" -shared -fPIC -m64
