cd "${0%/*}"
g++ -c -std=c++17 "DialogModule.cpp" -fPIC -m32
g++ "DialogModule.o" -o "DialogModule (x86)/DialogModule.so" -shared -fPIC -m32
