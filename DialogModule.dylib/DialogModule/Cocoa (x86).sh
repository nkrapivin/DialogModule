cd "${0%/*}"
g++ -c -std=c++17 "GameMaker.cpp" "Cocoa.part1.cpp" -fPIC -m32
g++ -c -ObjC "Cocoa.part2.mm" "NSAlert+SynchronousSheet.mm" -fPIC -m32
g++ "GameMaker.o" "Cocoa.part1.o" "Cocoa.part2.o" "NSAlert+SynchronousSheet.o" -o "DialogModule (x86)/DialogModule.dylib" -shared -framework Cocoa -fPIC -m32
