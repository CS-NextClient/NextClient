#include <memory>
#include <stacktrace>
#include <GLFW/glfw3.h>
#include <cmrc/cmrc.hpp>
#include <easylogging++.h>
#include <taskcoro/TaskCoro.h>
#include <gui_app_core/imgui/imgui.h>
#include <gui_app_core/imgui/imgui_impl_glfw.h>
#include <gui_app_core/imgui/imgui_impl_opengl2.h>
#include <gui_app_core/GuiAppInterface.h>
#include <taskcoro/impl/TaskCoroImpl.h>

CMRC_DECLARE(gui_app_core_rc);

static const char* LOG_TAG = "[gui_app_core] ";
static const char* IMGUI_WINDOW_NAME = "main";

static void UnhandledExceptionHandler(const std::exception& ex)
{
    auto trace = std::stacktrace::current();

    LOG(ERROR) << LOG_TAG << "Unhandled exception: " << ex.what();

    for (const std::stacktrace_entry& entry : trace)
    {
        LOG(ERROR) << LOG_TAG << entry.description();
    }
}

static void InvokeOnUpdateSafe(const std::function<void(GuiAppState&)>& on_update, GuiAppState& state)
{
    try { on_update(state); }
    catch (const std::exception& ex) { UnhandledExceptionHandler(ex); }
}

static void InvokeOnExitSafe(const std::function<void()>& on_exit)
{
    try { on_exit(); }
    catch (const std::exception& ex) { UnhandledExceptionHandler(ex); }
}

void RunGuiAppImpl(
    const std::function<GuiAppStartUpInfo()>& on_start,
    const std::function<void(GuiAppState&)>& on_update,
    const std::function<void()>& on_exit)
{
    GuiAppStartUpInfo startup_info;

    try
    {
        startup_info = on_start();
    }
    catch (const std::exception& ex)
    {
        UnhandledExceptionHandler(ex);
        return;
    }

    GuiAppState gui_app_state;
    gui_app_state.should_exit = false;
    gui_app_state.windows_state_hidden = startup_info.window_state_hidden;

    std::string window_name = startup_info.window_name;
    int window_width = startup_info.window_width;
    int window_height = startup_info.window_height;

    glfwSetErrorCallback([](int error, const char* description)
    {
        LOG(ERROR) << LOG_TAG << "GLFW Error " << error << ": " << description;
    });

    if (!glfwInit())
    {
        InvokeOnExitSafe(on_exit);
        return;
    }

    // Create a window with graphics context
    glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
    glfwWindowHint(GLFW_DECORATED, GLFW_FALSE);

    GLFWwindow* window = glfwCreateWindow(window_width, window_height, window_name.c_str(), nullptr, nullptr);
    if (window == nullptr)
    {
        InvokeOnExitSafe(on_exit);
        return;
    }

    const GLFWvidmode* mode = glfwGetVideoMode(glfwGetPrimaryMonitor());
    glfwSetWindowPos(window, (mode->width - window_width) / 2, (mode->height - window_height) / 2);

    if (!startup_info.window_state_hidden)
    {
        glfwShowWindow(window);
    }

    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();

    // Setup Platform/Renderer backends
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL2_Init();

    // Load Fonts
    auto embedded_fs = cmrc::gui_app_core_rc::get_filesystem();
    auto font_file = embedded_fs.open("assets/ArialMT.ttf");
    auto font_data = std::string(font_file.begin(), font_file.end());

    auto font_cfg = new ImFontConfig();
    font_cfg->FontDataOwnedByAtlas = false;
    io.Fonts->AddFontFromMemoryTTF(font_data.data(), font_data.size(), 18.0f, font_cfg, io.Fonts->GetGlyphRangesCyrillic());

    // configure imgui
    io.IniFilename = nullptr;
    io.LogFilename = nullptr;

    // Our state
    ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

    // Main loop
    bool done = false;
    while (!done)
    {
        if (glfwWindowShouldClose(window))
        {
            InvokeOnExitSafe(on_exit);
            break;
        }

        // Poll and handle events (inputs, window resize, etc.)
        // You can read the io.WantCaptureMouse, io.WantCaptureKeyboard flags to tell if dear imgui wants to use your inputs.
        // - When io.WantCaptureMouse is true, do not dispatch mouse input data to your main application, or clear/overwrite your copy of the mouse data.
        // - When io.WantCaptureKeyboard is true, do not dispatch keyboard input data to your main application, or clear/overwrite your copy of the keyboard data.
        // Generally you may always pass all inputs to dear imgui, and hide them from your application based on those two flags.
        glfwPollEvents();

        // Start the Dear ImGui frame
        ImGui_ImplOpenGL2_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        {
            ImGui::Begin(IMGUI_WINDOW_NAME, nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove);
            ImGui::SetWindowPos(IMGUI_WINDOW_NAME, ImVec2(0.0f, 0.0f));
            ImGui::SetWindowSize(IMGUI_WINDOW_NAME, ImVec2(window_width, window_height));

            GuiAppState state_to_pass = gui_app_state;
            InvokeOnUpdateSafe(on_update, state_to_pass);

            if (state_to_pass.should_exit)
            {
                done = true;
                InvokeOnExitSafe(on_exit);
            }

            if (state_to_pass.windows_state_hidden != gui_app_state.windows_state_hidden)
            {
                gui_app_state.windows_state_hidden = state_to_pass.windows_state_hidden;

                if (state_to_pass.windows_state_hidden)
                {
                    glfwHideWindow(window);
                }
                else
                {
                    glfwShowWindow(window);
                }
            }

            ImGui::End();
        }

        // Rendering
        ImGui::Render();
        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(clear_color.x * clear_color.w, clear_color.y * clear_color.w, clear_color.z * clear_color.w, clear_color.w);
        glClear(GL_COLOR_BUFFER_BIT);

        ImGui_ImplOpenGL2_RenderDrawData(ImGui::GetDrawData());

        glfwMakeContextCurrent(window);
        glfwSwapBuffers(window);
    }

    // Cleanup
    ImGui_ImplOpenGL2_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(window);
    glfwTerminate();
}
