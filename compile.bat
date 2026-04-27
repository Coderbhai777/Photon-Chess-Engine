set GPP="C:\msys64\mingw64\bin\g++.exe"
%GPP% -O3 -std=c++17 engine/attacks.cpp engine/board.cpp engine/eval.cpp engine/main.cpp engine/movegen.cpp engine/search.cpp engine/tt.cpp engine/uci.cpp engine/zobrist.cpp -o build/photon.exe -I engine -static
