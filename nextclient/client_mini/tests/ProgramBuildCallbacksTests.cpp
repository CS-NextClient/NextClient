#include <gtest/gtest.h>

#include <string>
#include <vector>

#include <ncl_gl/program.h>

#include "RadarGlContext.h"

namespace
{
    class ProgramBuildTest : public testing::Test
    {
    protected:
        RadarGlContext context;
        std::vector<ncl_gl::ProgramBuildStage> begins;
        std::vector<bool> successes;
        ncl_gl::ProgramBuildCallbacks callbacks{};

        void SetUp() override
        {
            std::string error;
            if (!context.Initialize(error))
            {
                GTEST_SKIP() << error;
            }
            callbacks.user = this;
            callbacks.Begin = [](void* user, ncl_gl::ProgramBuildStage stage) -> uint64_t {
                ProgramBuildTest* test = static_cast<ProgramBuildTest*>(user);
                test->begins.push_back(stage);
                return test->begins.size();
            };
            callbacks.End = [](void* user, uint64_t token, bool success) {
                EXPECT_NE(token, 0u);
                static_cast<ProgramBuildTest*>(user)->successes.push_back(success);
            };
        }
    };
} // namespace

TEST_F(ProgramBuildTest, FailedCompileAndLinkCloseScopes)
{
    EXPECT_EQ(ncl_gl::CompileShader(GL_VERTEX_SHADER, "invalid shader", nullptr, &callbacks), 0u);
    EXPECT_EQ(begins.size(), 1u);
    ASSERT_EQ(successes.size(), 1u);
    EXPECT_FALSE(successes[0]);
    const char* vertex = "#version 330 compatibility\nuniform float channel;\nvoid main(){ gl_Position=gl_Vertex*channel; }";
    const char* fragment = "#version 330 compatibility\nuniform vec4 channel;\nout vec4 color;\nvoid main(){ color=channel; }";
    EXPECT_EQ(ncl_gl::LinkProgram(vertex, fragment, nullptr, 0, nullptr, &callbacks), 0u);
    ASSERT_EQ(begins.size(), 4u);
    EXPECT_EQ(begins[3], ncl_gl::ProgramBuildStage::Link);
    ASSERT_EQ(successes.size(), 4u);
    EXPECT_FALSE(successes[3]);
}

TEST_F(ProgramBuildTest, NoCallbacksPreserveProgramBuildBehavior)
{
    const char* vertex = "#version 330 compatibility\nvoid main(){ gl_Position=gl_Vertex; }";
    const char* fragment = "#version 330 compatibility\nout vec4 color;\nvoid main(){ color=vec4(1); }";
    GLuint plain = ncl_gl::LinkProgram(vertex, fragment, nullptr, 0);
    GLuint observed = ncl_gl::LinkProgram(vertex, fragment, nullptr, 0, nullptr, &callbacks);
    EXPECT_NE(plain, 0u);
    EXPECT_NE(observed, 0u);
    EXPECT_EQ(begins.size(), 3u);
    EXPECT_EQ(successes, (std::vector<bool>{true, true, true}));
    glDeleteProgram(plain);
    glDeleteProgram(observed);
}
