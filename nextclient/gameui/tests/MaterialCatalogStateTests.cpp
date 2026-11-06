#include <gtest/gtest.h>

#include "GameUi/MaterialEditor/MaterialCatalogState.h"

using material_editor::MaterialCatalogState;
using material_editor::MaterialCatalogVisibleRows;

TEST(MaterialCatalogState, ExposesEveryMatchWithoutPaging)
{
    MaterialCatalogState state;
    std::vector<std::string> models;

    for (int index = 0; index < 300; index++)
    {
        models.push_back("models/test" + std::to_string(index) + ".mdl");
    }

    state.SetModels(models);
    ASSERT_EQ(state.get_indices().size(), 300);
    EXPECT_EQ(state.get_indices().front(), 0);
    EXPECT_EQ(state.get_indices().back(), 299);
}

TEST(MaterialCatalogState, FiltersAllModelsAndReportsWhetherTheFilterChanged)
{
    MaterialCatalogState state;
    std::vector<std::string> models(10, "models/V_RIFLE.mdl");
    models.push_back("models/w_pistol.mdl");
    state.SetModels(models);
    EXPECT_FALSE(state.SetFilter(""));
    EXPECT_TRUE(state.SetFilter("rifle"));
    EXPECT_FALSE(state.SetFilter("rifle"));
    EXPECT_EQ(state.get_match_count(), 10);
    EXPECT_EQ(state.get_indices().back(), 9);
    EXPECT_TRUE(state.SetFilter("pistol"));
    EXPECT_EQ(state.get_indices(), (std::vector<int>{10}));
    state.SetFilter("absent");
    EXPECT_TRUE(state.get_indices().empty());
}

TEST(MaterialCatalogState, RescanRetainsTheFilterAndEmptyListsHaveNoRows)
{
    MaterialCatalogState state;
    EXPECT_TRUE(state.GetVisibleRows(0, 300, 76).indices.empty());
    state.SetFilter("gun");
    state.SetModels({"models/gun.mdl", "models/knife.mdl"});
    EXPECT_EQ(state.get_indices(), (std::vector<int>{0}));
    state.SetModels({"models/knife.mdl"});
    EXPECT_TRUE(state.get_indices().empty());
}

TEST(MaterialCatalogState, VisibleRowsIncludePartialRowsAndClampToTheEnd)
{
    MaterialCatalogState state;
    state.SetModels(std::vector<std::string>(19, "models/gun.mdl"));
    MaterialCatalogVisibleRows rows = state.GetVisibleRows(0, 150, 76);
    EXPECT_EQ(rows.first_row, 0);
    EXPECT_EQ(rows.first_y, 0);
    EXPECT_EQ(std::vector<int>(rows.indices.begin(), rows.indices.end()), (std::vector<int>{0, 1}));
    rows = state.GetVisibleRows(75, 150, 76);
    EXPECT_EQ(rows.first_row, 0);
    EXPECT_EQ(rows.first_y, -75);
    EXPECT_EQ(std::vector<int>(rows.indices.begin(), rows.indices.end()), (std::vector<int>{0, 1, 2}));
    rows = state.GetVisibleRows(76, 150, 76);
    EXPECT_EQ(rows.first_row, 1);
    EXPECT_EQ(rows.first_y, 0);
    EXPECT_EQ(std::vector<int>(rows.indices.begin(), rows.indices.end()), (std::vector<int>{1, 2}));
    rows = state.GetVisibleRows(99999, 150, 76);
    EXPECT_EQ(rows.first_row, 17);
    EXPECT_EQ(rows.first_y, -2);
    EXPECT_EQ(std::vector<int>(rows.indices.begin(), rows.indices.end()), (std::vector<int>{17, 18}));
    EXPECT_TRUE(state.GetVisibleRows(0, 0, 76).indices.empty());
}
