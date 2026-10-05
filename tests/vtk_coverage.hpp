#pragma once
#include <fstream>
#include <initializer_list>
#include <stdexcept>
// Called only after the corresponding execution/assertion suite succeeds.
inline void vtk_coverage(const char *file, std::initializer_list<int> cases) {
    std::ofstream output(file);
    output << "{\"passed_cases\":[";
    bool first = true;
    for (int id : cases) {
        if (!first)
            output << ',';
        output << id;
        first = false;
    }
    output << "]}\n";
    output.flush();
    if (!output)
        throw std::runtime_error("Cannot write VTK execution coverage evidence");
}
