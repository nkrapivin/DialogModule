cd "${0%/*}"
g++ -c -std=c++17 "DialogModule.part1.cpp" -fPIC -m32
g++ -c -ObjC "DialogModule.part2.mm" -fPIC -m32
g++ "DialogModule.part1.o" "DialogModule.part2.o" -o "DialogModule (x86)/DialogModule.dylib" -shared -framework Cocoa -fPIC -m32
