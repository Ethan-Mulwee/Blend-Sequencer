#include <iostream>
#include <fstream>
#include <cstring>
#include <string>

int main() {
    std::ifstream blend("Cube.blend", std::ios::in | std::ios::binary);

    char* buffer = new char [17 + 1];

    blend.read(buffer, 17);
    buffer[17 + 1] = '\0';

    std::string str = buffer;

    std::cout << "header: " << str << "\n";
}