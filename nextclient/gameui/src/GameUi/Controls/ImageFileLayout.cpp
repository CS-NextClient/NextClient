#include "GameUi/Controls/ImageFileLayout.h"

#include <algorithm>

ImageFileBounds ImageFile_Fit(int image_wide, int image_tall, int x, int y, int side)
{
    if (image_wide <= 0 || image_tall <= 0 || side <= 0)
    {
        return ImageFileBounds{};
    }

    int wide = side;
    int tall = side;

    if (image_wide >= image_tall)
    {
        tall = std::max(1, side * image_tall / image_wide);
    }
    else
    {
        wide = std::max(1, side * image_wide / image_tall);
    }

    return ImageFileBounds{x + (side - wide) / 2, y + (side - tall) / 2, wide, tall};
}
