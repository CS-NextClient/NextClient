#include <gtest/gtest.h>

#include "GameUi/MaterialEditor/MaterialPreviewPointer.h"

using material_editor::MaterialPreviewPointer;

TEST(MaterialPreviewPointer, SmallMotionIsAClickAndCrossingFourPixelsStartsADrag)
{
    MaterialPreviewPointer pointer;
    pointer.Begin(10, 20);
    EXPECT_FALSE(pointer.Move(12, 22));
    EXPECT_TRUE(pointer.End(12, 22));
    pointer.Begin(10, 20);
    EXPECT_TRUE(pointer.Move(14, 20));
    EXPECT_TRUE(pointer.Move(10, 20));
    EXPECT_FALSE(pointer.End(10, 20));
}

TEST(MaterialPreviewPointer, ReleaseChecksMotionAndCancellationCannotSelect)
{
    MaterialPreviewPointer pointer;
    EXPECT_FALSE(pointer.End(0, 0));
    pointer.Begin(0, 0);
    EXPECT_FALSE(pointer.End(0, 8));
    pointer.Begin(10, 20);
    pointer.Cancel();
    EXPECT_FALSE(pointer.End(10, 20));
}
