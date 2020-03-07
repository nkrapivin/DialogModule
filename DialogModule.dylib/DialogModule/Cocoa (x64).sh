cd "${0%/*}"
g++ -c -std=c++17 "GameMaker.cpp" "EvaluateShell.cpp" "Cocoa.part1.cpp" -fPIC -m64
g++ -c -ObjC "Cocoa.part2.mm" -fPIC -m64
g++ "GameMaker.o" "EvaluateShell.o" "Cocoa.part1.o" "Cocoa.part2.o" -o "DialogModule (x64)/DialogModule.dylib" -shared -framework Cocoa -fPIC -m64
