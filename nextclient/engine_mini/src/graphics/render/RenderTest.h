#pragma once

namespace render
{
    // Phase A exit criterion: a shader drawn full screen quad behind r_test_quad that
    // keeps working across a video mode change. Exercises the whole foundation at once
    // (capability gate, shader cache, buffer, device reset), so a regression in any of
    // them is visible without a map-specific setup.
    void Test_Draw();
}
