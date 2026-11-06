#include <algorithm>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "GameUi/MaterialEditor/MaterialResourceCatalog.h"

namespace
{
    constexpr int kStudioModelId = ('T' << 24) | ('S' << 16) | ('D' << 8) | 'I';
    constexpr int kSequenceGroupId = ('Q' << 24) | ('S' << 16) | ('D' << 8) | 'I';

    class CatalogSource : public material_editor::ResourceSource
    {
    public:
        std::map<std::string, std::vector<material_editor::ResourceEntry>> entries;
        std::map<std::string, std::optional<material_editor::ModelHeader>> headers;
        std::set<std::string> files;
        std::vector<std::string> enumerations;
        std::vector<std::string> header_reads;
        std::vector<std::string> existence_checks;

        std::vector<material_editor::ResourceEntry> Enumerate(const std::string& wildcard) override
        {
            enumerations.push_back(wildcard);
            auto found = entries.find(wildcard);

            return found != entries.end() ? found->second : std::vector<material_editor::ResourceEntry>{};
        }

        std::optional<material_editor::ModelHeader> ReadModelHeader(const std::string& path) override
        {
            header_reads.push_back(path);
            auto found = headers.find(path);

            return found != headers.end() ? found->second : std::nullopt;
        }

        bool FileExists(const std::string& path) override
        {
            existence_checks.push_back(path);

            return files.contains(path);
        }
    };
} // namespace

TEST(MaterialResourceCatalog, ModelsWalkThreeChildLevelsAndDeduplicateDirectories)
{
    CatalogSource source;
    source.entries["models/*"] = {
        {"z.MDL", false},
        {"a.mdl", false},
        {"a.mdl", false},
        {"skip.png", false},
        {".", true},
        {"..", true},
        {"pack", true},
        {"PACK", true},
    };
    source.entries["models/pack/*"] = {{"level1.mdl", false}, {"inner", true}};
    source.entries["models/pack/inner/*"] = {{"level2.mdl", false}, {"last", true}};
    source.entries["models/pack/inner/last/*"] = {{"level3.mdl", false}, {"deeper", true}};
    source.entries["models/pack/inner/last/deeper/*"] = {{"level4.mdl", false}};
    material_editor::ResourceCatalog catalog(source);

    EXPECT_EQ(
        catalog.ScanModels(),
        (std::vector<std::string>{
            "models/a.mdl",
            "models/pack/inner/last/level3.mdl",
            "models/pack/inner/level2.mdl",
            "models/pack/level1.mdl",
            "models/z.MDL",
        })
    );
    EXPECT_EQ(
        source.enumerations,
        (std::vector<std::string>{
            "models/*",
            "models/pack/*",
            "models/pack/inner/*",
            "models/pack/inner/last/*",
        })
    );
    EXPECT_TRUE(source.header_reads.empty());
}

TEST(MaterialResourceCatalog, ModelPathsAreDeduplicatedIgnoringCase)
{
    CatalogSource source;
    source.entries["models/*"] = {{"RIFLE.MDL", false}, {"rifle.mdl", false}, {"a.mdl", false}};
    material_editor::ResourceCatalog catalog(source);

    const std::vector<std::string> models = catalog.ScanModels();

    ASSERT_EQ(models.size(), 2u);
    EXPECT_EQ(models.front(), "models/a.mdl");
    EXPECT_TRUE(models.back() == "models/RIFLE.MDL" || models.back() == "models/rifle.mdl");
}

TEST(MaterialResourceCatalog, RawModelLimitCountsDuplicatesBeforeCompanionFiltering)
{
    CatalogSource source;
    source.entries["models/*"] = std::vector<material_editor::ResourceEntry>(16383, {"rifle.mdl", false});
    source.entries["models/*"].push_back({"rifleT.mdl", false});
    source.entries["models/*"].push_back({"late.mdl", false});
    source.entries["models/*"].push_back({"pack", true});
    source.entries["models/pack/*"] = {{"later.mdl", false}};
    source.headers["models/rifleT.mdl"] = material_editor::ModelHeader{kStudioModelId, 0};
    material_editor::ResourceCatalog catalog(source);

    EXPECT_EQ(catalog.ScanModels(), (std::vector<std::string>{"models/rifle.mdl"}));
    EXPECT_EQ(source.enumerations, (std::vector<std::string>{"models/*", "models/pack/*"}));
    EXPECT_EQ(source.header_reads, (std::vector<std::string>{"models/rifleT.mdl"}));
}

TEST(MaterialResourceCatalog, PairedCompanionsUseHeaderIdentityAndGeometry)
{
    CatalogSource source;
    source.entries["models/*"] = {
        {"WEAPON.MDL", false},
        {"weaponT.mdl", false},
        {"weapon01.mdl", false},
        {"weapon02.mdl", false},
        {"weapon03.mdl", false},
        {"loader.mdl", false},
        {"loadert.mdl", false},
        {"tree.mdl", false},
        {"tree02.mdl", false},
    };
    source.headers["models/weaponT.mdl"] = material_editor::ModelHeader{kStudioModelId, 0};
    source.headers["models/weapon01.mdl"] = material_editor::ModelHeader{kSequenceGroupId, 1};
    source.headers["models/weapon02.mdl"] = material_editor::ModelHeader{0, 1};
    source.headers["models/weapon03.mdl"] = material_editor::ModelHeader{kStudioModelId, -1};
    source.headers["models/loadert.mdl"] = material_editor::ModelHeader{kStudioModelId, 0};
    source.headers["models/tree02.mdl"] = material_editor::ModelHeader{kStudioModelId, 1};
    material_editor::ResourceCatalog catalog(source);

    EXPECT_EQ(
        catalog.ScanModels(), (std::vector<std::string>{"models/loader.mdl", "models/tree.mdl", "models/tree02.mdl", "models/WEAPON.MDL"})
    );
    EXPECT_EQ(
        source.header_reads,
        (std::vector<std::string>{
            "models/loadert.mdl",
            "models/tree02.mdl",
            "models/weapon01.mdl",
            "models/weapon02.mdl",
            "models/weapon03.mdl",
            "models/weaponT.mdl"
        })
    );
}

TEST(MaterialResourceCatalog, OrdinaryAndUnpairedNamesDoNotRequireHeaderReads)
{
    CatalogSource source;
    source.entries["models/*"] = {{"soloT.mdl", false}, {"orphan01.mdl", false}, {"plain.mdl", false}, {"w_ak47.mdl", false}};
    source.headers["models/soloT.mdl"] = material_editor::ModelHeader{kStudioModelId, 0};
    source.headers["models/orphan01.mdl"] = material_editor::ModelHeader{kSequenceGroupId, 0};
    material_editor::ResourceCatalog catalog(source);

    EXPECT_EQ(
        catalog.ScanModels(), (std::vector<std::string>{"models/orphan01.mdl", "models/plain.mdl", "models/soloT.mdl", "models/w_ak47.mdl"})
    );
    EXPECT_TRUE(source.header_reads.empty());
}

TEST(MaterialResourceCatalog, ShortOrUnreadableCompanionHeadersRemainVisible)
{
    CatalogSource source;
    source.entries["models/*"] = {{"short.mdl", false}, {"shortT.mdl", false}, {"unreadable.mdl", false}, {"unreadable01.mdl", false}};
    source.headers["models/shortT.mdl"] = std::nullopt;
    material_editor::ResourceCatalog catalog(source);

    EXPECT_EQ(
        catalog.ScanModels(),
        (std::vector<std::string>{"models/short.mdl", "models/shortT.mdl", "models/unreadable.mdl", "models/unreadable01.mdl"})
    );
    EXPECT_EQ(source.header_reads, (std::vector<std::string>{"models/shortT.mdl", "models/unreadable01.mdl"}));
}

TEST(MaterialResourceCatalog, CompanionLookupUsesTheCompleteCatalog)
{
    CatalogSource source;
    source.entries["models/*"] = {{"chain.mdl", false}, {"chainT.mdl", false}, {"chainT0.mdl", false}, {"chainT01.mdl", false}};
    source.headers["models/chainT.mdl"] = material_editor::ModelHeader{kStudioModelId, 0};
    source.headers["models/chainT01.mdl"] = material_editor::ModelHeader{kSequenceGroupId, 0};
    material_editor::ResourceCatalog catalog(source);

    EXPECT_EQ(catalog.ScanModels(), (std::vector<std::string>{"models/chain.mdl", "models/chainT0.mdl"}));
    EXPECT_EQ(source.header_reads, (std::vector<std::string>{"models/chainT.mdl", "models/chainT01.mdl"}));
}

TEST(MaterialResourceCatalog, ImagesMergePngThenTgaAndKeepTheFirstPathSpelling)
{
    CatalogSource source;
    source.entries["materials/test/*.png"] = {{"z.png", false}, {"A.PNG", false}, {"ignored.jpg", false}, {"directory.png", true}};
    source.entries["materials/test/*.tga"] = {{"b.tga", false}, {"a.png", false}, {"B.TGA", false}};
    material_editor::ResourceCatalog catalog(source);

    EXPECT_EQ(
        catalog.ScanImages("materials/test"),
        (std::vector<std::string>{"materials/test/A.PNG", "materials/test/b.tga", "materials/test/z.png"})
    );
    EXPECT_EQ(source.enumerations, (std::vector<std::string>{"materials/test/*.png", "materials/test/*.tga"}));
}

TEST(MaterialResourceCatalog, ImageFoldersRequireDirectImagesAndStopAtTwoChildLevels)
{
    CatalogSource source;
    source.entries["materials/*"] = {
        {".", true},
        {"..", true},
        {"pack", true},
        {"PACK", true},
        {"empty", true},
        {"alpha", true},
        {"Zebra", true},
    };
    source.entries["materials/pack/*"] = {{"inner", true}};
    source.entries["materials/pack/inner/*.png"] = {{"image.png", false}};
    source.entries["materials/pack/inner/*"] = {{"deeper", true}};
    source.entries["materials/pack/inner/deeper/*.png"] = {{"hidden.png", false}};
    source.entries["materials/empty/*"] = {{"child", true}};
    source.entries["materials/empty/child/*.tga"] = {{"image.tga", false}};
    source.entries["materials/alpha/*.png"] = {{"image.png", false}};
    source.entries["materials/Zebra/*.tga"] = {{"image.tga", false}};
    material_editor::ResourceCatalog catalog(source);

    EXPECT_EQ(
        catalog.ScanImageFolders(),
        (std::vector<std::string>{"materials/Zebra", "materials/alpha", "materials/empty/child", "materials/pack/inner"})
    );
    EXPECT_EQ(std::count(source.enumerations.begin(), source.enumerations.end(), "materials/pack/*.png"), 1);
    EXPECT_EQ(std::count(source.enumerations.begin(), source.enumerations.end(), "materials/PACK/*.png"), 0);
    EXPECT_EQ(std::count(source.enumerations.begin(), source.enumerations.end(), "materials/pack/inner/*"), 0);
    EXPECT_EQ(std::count(source.enumerations.begin(), source.enumerations.end(), "materials/pack/inner/deeper/*.png"), 0);
}

TEST(MaterialResourceCatalog, ImageCollectionCanIncludeItsRootFolder)
{
    CatalogSource source;
    source.entries["materials/*.tga"] = {{"image.TGA", false}};
    material_editor::ResourceCatalog catalog(source);

    EXPECT_EQ(catalog.ScanImageFolders(), (std::vector<std::string>{"materials"}));
}

TEST(MaterialResourceCatalog, WeaponChoicesPreservePrefixOrderAndExcludeTheCurrentRelatedModel)
{
    CatalogSource source;
    source.files = {"models/Pack/v_Rifle.mdl", "models/Pack/p_Rifle.mdl", "models/Pack/w_Rifle.mdl"};
    material_editor::ResourceCatalog catalog(source);

    EXPECT_EQ(
        catalog.GetStandInModels("models/Pack/p_Rifle.mdl"),
        (std::vector<std::string>{"models/Pack/v_Rifle.mdl", "models/Pack/p_Rifle.mdl", "models/Pack/w_Rifle.mdl"})
    );
    EXPECT_EQ(
        catalog.GetRelatedModels("models/Pack/p_Rifle.mdl"),
        (std::vector<std::string>{"models/Pack/v_Rifle.mdl", "models/Pack/w_Rifle.mdl"})
    );
    EXPECT_EQ(
        source.existence_checks,
        (std::vector<std::string>{
            "models/Pack/v_Rifle.mdl",
            "models/Pack/p_Rifle.mdl",
            "models/Pack/w_Rifle.mdl",
            "models/Pack/v_Rifle.mdl",
            "models/Pack/w_Rifle.mdl"
        })
    );
}

TEST(MaterialResourceCatalog, MissingCurrentModelIsAppendedOnlyToStandInChoices)
{
    CatalogSource source;
    source.files = {"models/p_rifle.mdl", "models/w_rifle.mdl"};
    material_editor::ResourceCatalog catalog(source);

    EXPECT_EQ(
        catalog.GetStandInModels("models/v_rifle.mdl"),
        (std::vector<std::string>{"models/p_rifle.mdl", "models/w_rifle.mdl", "models/v_rifle.mdl"})
    );
    EXPECT_EQ(catalog.GetRelatedModels("models/v_rifle.mdl"), (std::vector<std::string>{"models/p_rifle.mdl", "models/w_rifle.mdl"}));
    EXPECT_EQ(catalog.GetStandInModels("models/custom.mdl"), (std::vector<std::string>{"models/custom.mdl"}));
    EXPECT_TRUE(catalog.GetRelatedModels("models/custom.mdl").empty());
}

TEST(MaterialResourceCatalog, UppercaseWeaponPrefixesAreNotReplaced)
{
    CatalogSource source;
    source.files = {"models/V_rifle.mdl", "models/p_rifle.mdl", "models/w_rifle.mdl"};
    material_editor::ResourceCatalog catalog(source);

    EXPECT_EQ(catalog.GetStandInModels("models/V_rifle.mdl"), (std::vector<std::string>{"models/V_rifle.mdl"}));
    EXPECT_TRUE(catalog.GetRelatedModels("models/V_rifle.mdl").empty());
    EXPECT_EQ(source.existence_checks, (std::vector<std::string>{"models/V_rifle.mdl"}));
}

TEST(MaterialResourceCatalog, WeaponChoicesAlsoSupportFilenamesWithoutDirectories)
{
    CatalogSource source;
    source.files = {"v_rifle.mdl", "p_rifle.mdl", "w_rifle.mdl"};
    material_editor::ResourceCatalog catalog(source);

    EXPECT_EQ(catalog.GetStandInModels("w_rifle.mdl"), (std::vector<std::string>{"v_rifle.mdl", "p_rifle.mdl", "w_rifle.mdl"}));
    EXPECT_EQ(catalog.GetRelatedModels("w_rifle.mdl"), (std::vector<std::string>{"v_rifle.mdl", "p_rifle.mdl"}));
    EXPECT_TRUE(catalog.GetStandInModels("").empty());
    EXPECT_TRUE(catalog.GetRelatedModels("").empty());
}

TEST(MaterialResourceCatalog, RescansReturnIndependentOwnedSnapshots)
{
    CatalogSource source;
    source.entries["models/*"] = {{"first.mdl", false}};
    source.entries["materials/test/*.png"] = {{"first.png", false}};
    material_editor::ResourceCatalog catalog(source);
    const std::vector<std::string> original_models = catalog.ScanModels();
    const std::vector<std::string> original_images = catalog.ScanImages("materials/test");
    source.entries["models/*"] = {{"second.mdl", false}};
    source.entries["materials/test/*.png"] = {{"second.png", false}};

    EXPECT_EQ(catalog.ScanModels(), (std::vector<std::string>{"models/second.mdl"}));
    EXPECT_EQ(catalog.ScanImages("materials/test"), (std::vector<std::string>{"materials/test/second.png"}));
    EXPECT_EQ(original_models, (std::vector<std::string>{"models/first.mdl"}));
    EXPECT_EQ(original_images, (std::vector<std::string>{"materials/test/first.png"}));
}
