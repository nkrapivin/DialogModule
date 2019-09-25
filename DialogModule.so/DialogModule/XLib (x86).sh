cd "${0%/*}"
g++ -c -std=c++17 "GameMaker.cpp" "XLib.cpp" -fPIC -m32
g++ "GameMaker.o" "XLib.o" -o "DialogModule (x86)/DialogModule.so" -shared -fPIC -m32
