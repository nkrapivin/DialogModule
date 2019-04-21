cd "${0%/*}"
g++ -c -std=c++17 "DialogModule.cpp" -fPIC -m64
g++ "DialogModule.o" -o "DialogModule (x64)/DialogModule.so" -shared -fPIC -m64
