#include <algorithm>
#include <array>

#include <gtest/gtest.h>

#include "GameUi/ColorPickerDialog.h"
#include "GameUi/MaterialEditor/MaterialEditDialogState.h"

using material_editor::MaterialEditDialogState;

TEST(MaterialEditDialogState, NewTargetStartsWithACopiedCleanSnapshot)
{
    MaterialEditDialogState state;
    char initial[] = "original";
    state.Begin(initial);
    initial[0] = 'x';
    state.MarkMaterialChanged("original");

    EXPECT_FALSE(state.is_material_dirty());
}

TEST(MaterialEditDialogState, ReturningToTheSavedMaterialClearsDirtyState)
{
    MaterialEditDialogState state;
    state.Begin("original");
    state.MarkMaterialChanged("edited");
    EXPECT_TRUE(state.is_material_dirty());

    state.MarkMaterialChanged("original");
    EXPECT_FALSE(state.is_material_dirty());
}

TEST(MaterialEditDialogState, ComparisonDoesNotChangeMaterialDirtyState)
{
    MaterialEditDialogState state;
    state.Begin("original");
    state.SetComparison(true);
    EXPECT_TRUE(state.is_comparing());
    EXPECT_FALSE(state.is_material_dirty());
    state.MarkMaterialChanged("edited");
    state.SetComparison(false);
    EXPECT_TRUE(state.is_material_dirty());
}

TEST(MaterialEditDialogState, SavingReplacesTheMaterialSnapshot)
{
    MaterialEditDialogState state;
    state.Begin("original");
    state.MarkMaterialChanged("edited");
    state.MarkMaterialSaved("edited");
    EXPECT_FALSE(state.is_material_dirty());

    state.MarkMaterialChanged("original");
    EXPECT_TRUE(state.is_material_dirty());
}

TEST(MaterialEditDialogState, EndingTheSessionReleasesItsWorkingState)
{
    MaterialEditDialogState state;
    state.Begin("original");
    state.MarkMaterialChanged("edited");
    state.SetComparison(true);
    state.End();

    EXPECT_FALSE(state.is_material_dirty());
    EXPECT_FALSE(state.is_comparing());
}

namespace
{
    class ColorPickerListener : public CColorPickerDialog::IListener
    {
    public:
        std::array<float, 4> color{};
        int changes{};

        void OnColorPicked(const char*, const float rgba[4]) override
        {
            std::copy_n(rgba, color.size(), color.begin());
            changes++;
        }
    };
}

TEST(ColorPickerListener, CancelRestoresTheOriginalForListenersWithoutAPreviewSnapshot)
{
    ColorPickerListener listener;
    const float original[] = {0.1f, 0.2f, 0.3f, 0.4f};
    listener.OnColorPickEnded("color", original, false);

    EXPECT_EQ(listener.changes, 1);
    EXPECT_EQ(listener.color, (std::array<float, 4>{0.1f, 0.2f, 0.3f, 0.4f}));
}

TEST(ColorPickerListener, AcceptRetainsThePublishedColor)
{
    ColorPickerListener listener;
    const float original[] = {0.1f, 0.2f, 0.3f, 0.4f};
    const float preview[] = {0.5f, 0.6f, 0.7f, 0.8f};
    listener.OnColorPicked("color", preview);
    listener.OnColorPickEnded("color", original, true);

    EXPECT_EQ(listener.changes, 1);
    EXPECT_EQ(listener.color, (std::array<float, 4>{0.5f, 0.6f, 0.7f, 0.8f}));
}
