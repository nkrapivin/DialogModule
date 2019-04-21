cd "${0%/*}"
g++ -c -std=c++17 "DialogModule.part1.cpp" -fPIC -m64
g++ -c -ObjC "DialogModule.part2.mm" -fPIC -m64
g++ "DialogModule.part1.o" "DialogModule.part2.o" -o "DialogModule (x64)/DialogModule.dylib" -shared -framework Cocoa -fPIC -m64
