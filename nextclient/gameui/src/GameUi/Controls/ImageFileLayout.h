#pragma once

struct ImageFileBounds
{
    int x{};
    int y{};
    int wide{};
    int tall{};
};

ImageFileBounds ImageFile_Fit(int image_wide, int image_tall, int x, int y, int side);
