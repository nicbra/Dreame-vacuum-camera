#include "Camera.hpp"
#include <iostream>
#include <cstring>

int main()
{
    // Max supported is 3264 x 2448
    const int width = 816;
    const int height = 612;
    const int count = 5;

    Camera camera(width, height);

    if (camera.ready()) {
        camera.save("/tmp/camstream", count);
    }
    printf("Closing\n");
    return 0;
}