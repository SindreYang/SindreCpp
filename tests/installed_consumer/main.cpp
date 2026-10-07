#include <sindre/general.h>
#include <Eigen/Core>

#include <iostream>

int main() {
    Eigen::Vector3d value(1.0, 2.0, 3.0);
    const auto parsed = sindre::general::string::parse_int("42");
    if (!parsed || parsed.value() != 42 || value.sum() != 6.0)
        return 1;
    std::cout << sindre::general::library_abi() << '\n';
    return 0;
}
