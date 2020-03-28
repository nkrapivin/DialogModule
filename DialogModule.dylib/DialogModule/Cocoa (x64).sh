cd "${0%/*}"
clang++ -c -std=c++17 "GameMaker.cpp" "Cocoa.part1.cpp" -fPIC -m64
clang++ -c -ObjC "Cocoa.part2.mm" -fPIC -m64
clang++ "GameMaker.o" "Cocoa.part1.o" "Cocoa.part2.o" -o "DialogModule (x64)/DialogModule.dylib" -shared -framework Cocoa -fPIC -m64
