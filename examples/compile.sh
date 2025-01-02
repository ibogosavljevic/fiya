g++ -O0 fiya-time-measure.cpp -o fiya-time-measure
g++ -O3 fiya-heap-measure.cpp ../fiya-heap-overloads.cpp -o fiya-heap-measure
g++ -O3 -g -finstrument-functions -rdynamic -Wl,--export-dynamic fiya-cyg-time-measure.cpp ../fiya-cyg-overloads.cpp -o fiya-cyg-time-measure -ldl
