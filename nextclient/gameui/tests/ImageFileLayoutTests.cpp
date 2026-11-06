#include <gtest/gtest.h>

#include "GameUi/Controls/ImageFileLayout.h"

TEST(ImageFileLayout, WideImageFitsAndCentersVertically)
{
    ImageFileBounds bounds = ImageFile_Fit(512, 256, 10, 20, 100);
    EXPECT_EQ(bounds.x, 10);
    EXPECT_EQ(bounds.y, 45);
    EXPECT_EQ(bounds.wide, 100);
    EXPECT_EQ(bounds.tall, 50);
}

TEST(ImageFileLayout, TallImageFitsAndCentersHorizontally)
{
    ImageFileBounds bounds = ImageFile_Fit(256, 512, 10, 20, 100);
    EXPECT_EQ(bounds.x, 35);
    EXPECT_EQ(bounds.y, 20);
    EXPECT_EQ(bounds.wide, 50);
    EXPECT_EQ(bounds.tall, 100);
}

TEST(ImageFileLayout, SquareImageUsesFullBounds)
{
    ImageFileBounds bounds = ImageFile_Fit(512, 512, 10, 20, 100);
    EXPECT_EQ(bounds.x, 10);
    EXPECT_EQ(bounds.y, 20);
    EXPECT_EQ(bounds.wide, 100);
    EXPECT_EQ(bounds.tall, 100);
}
