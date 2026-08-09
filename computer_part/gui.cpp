// Credit: https://github.com/ocornut/imgui/blob/master/examples/example_glfw_opengl3/main.cpp

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <glad/gl.h>

#include "imgui.h"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_opengl3.h"
#include <vector>
#include <string>
#include <chrono>
#include <algorithm>

using Timestamp = uint64_t;
const Timestamp currentTimestamp()
{
    const auto currentTime = std::chrono::steady_clock::now();
    return static_cast<Timestamp>(std::chrono::time_point_cast<std::chrono::milliseconds>(currentTime).time_since_epoch().count());
}

using std::string_literals::operator""s;

static const ImVec4 ColorWhite = (ImVec4)ImColor::HSV(0.5f, 0.0f, 1.0f);
static const ImVec4 ColorNew = (ImVec4)ImColor::HSV(90.0f / 360.0f, 1.0f, 0.5f);
static const ImVec4 ColorUpdate = (ImVec4)ImColor::HSV(180.0f / 360.0f, 1.0f, 0.5f);
static const ImVec4 ColorDelete = (ImVec4)ImColor::HSV(0.0f, 1.0f, 0.5f);
struct SinkInputDisplay
{
    // static ImVec4 ColorRemoved = (ImVec4)ImColor::HSV(0.5f, 0.0f, 1.0f);
    uint32_t id;
    // Will be swapped for const char* after integration with audio.c
    // const char* name;
    // const char* mediaName;
    // const char* applicationName;
    std::string name;
    std::string mediaName;
    std::string applicationName;
    float volume;
    bool corked;

    // Mostly display stuff below
    bool _new = true;
    bool _updated = false;
    bool _deleted = false;
    bool removeFromList = false;
    Timestamp createdTime;
    static const Timestamp newFade = 1000; // ms

    Timestamp updatedTime;
    static const Timestamp updateFade = 500; // ms

    Timestamp deletedTime;
    static const Timestamp delFade = 1000; // ms
    ImVec4 color;

    void UpdateAnimation(const Timestamp& currentTime)
    {
        // (ImVec4)ImColor::HSV(0.5f, 0.6f, 0.6f)
        if (_new && currentTime >= createdTime + newFade)
        {
            _new = false;
            color = ColorWhite;
            printf("Not new\n");
        }
        else if (_updated && currentTime >= updatedTime + delFade)
        {
            _updated = false;
            color = ColorWhite;
        }
        else if (_deleted && currentTime >= deletedTime + delFade)
        {
            _deleted = false;
            removeFromList = true;
        }

        if (_new)
        {
            float dt = (float)(currentTime - createdTime) / newFade;

            color = ImVec4(
                ColorNew.x * (1.0f - dt) + ColorWhite.x * dt,
                ColorNew.y * (1.0f - dt) + ColorWhite.y * dt,
                ColorNew.z * (1.0f - dt) + ColorWhite.z * dt,
                1.0f
            );
        }
        else if (_updated)
        {
            float dt = (float)(currentTime - updatedTime) / newFade;

            color = ImVec4(
                ColorUpdate.x * (1.0f - dt) + ColorWhite.x * dt,
                ColorUpdate.y * (1.0f - dt) + ColorWhite.y * dt,
                ColorUpdate.z * (1.0f - dt) + ColorWhite.z * dt,
                1.0f
            );
        }
        else if (_deleted)
        {
            color = ColorDelete;
        }
    }

    void setNew()
    {
        _new = true;
        createdTime = currentTimestamp();
    }

    void setUpdated()
    {
        _updated = true;
        updatedTime = currentTimestamp();
    }

    void setDeleted()
    {
        _deleted = true;
        deletedTime = currentTimestamp();

        if (_new || _updated)
        {
            const auto newTime = _new ? deletedTime - createdTime : 0;
            const auto updateTime = _updated ? deletedTime - updatedTime : 0;
            deletedTime += newTime + updateTime;
        }
    }
};

void GuiDebug(std::vector<SinkInputDisplay>& sinkInputs)
{
    static int id = 0;
    if (ImGui::Button("New sink"))
    {
            auto& elem = sinkInputs.emplace_back();
            elem.id = id++;
            elem.name = "Dum name"s + std::to_string(elem.id);
            elem.name = "Dum media"s + std::to_string(elem.id);
            elem.name = "Dum application"s + std::to_string(elem.id);
            elem.volume = 0.8f;
            elem.corked = elem.id % 2 == 1;
            elem.setNew();
    }
}

void ProcessUpdateFromAudio(std::vector<SinkInputDisplay>& sinkInputs)
{
    // if (sinkInputs.empty())
    // {
    //     for (int i = 0; i < 1; i++)
    //     {
    //         auto& elem = sinkInputs.emplace_back();
    //         elem.id = i;
    //         elem.name = "Dum name"s + std::to_string(i);
    //         elem.name = "Dum media"s + std::to_string(i);
    //         elem.name = "Dum application"s + std::to_string(i);
    //         elem.volume = 0.8f;
    //         elem.corked = i % 2 == 1;
    //         elem.setNew();
    //     }
    // }
}

void DisplaySinkInputs(std::vector<SinkInputDisplay>& sinkInputs)
{
    const Timestamp currentTime = currentTimestamp();
    GuiDebug(sinkInputs);

    for (SinkInputDisplay& sink : sinkInputs)
    {
        sink.UpdateAnimation(currentTime);

        ImGui::PushID(sink.id);
        ImGui::PushStyleColor(ImGuiCol_Separator, sink.color);
        ImGui::PushStyleColor(ImGuiCol_Text, sink.color);
        ImGui::SeparatorText(sink.name.c_str());
        // ImGui::PushID(i);
        // ImGui::Text("Name: %s", "Sink name", sink.id);
        ImGui::Text("Media: %s", sink.mediaName.c_str());
        ImGui::Text("Application: %s", sink.applicationName.c_str());
        ImGui::Checkbox("Corked", &sink.corked);
        ImGui::SliderFloat("Volume", &sink.volume, 0.0f, 1.0f, "%2f");
        // ImGui::Checkbox("new", &sink._new);
        if (ImGui::Button("Delete") && !sink._deleted)
        {
            sink.setDeleted();
        }
        ImGui::SameLine();
        if (ImGui::Button("Update"))// && !sink._updated)
        {
            sink.setUpdated();
        }
        ImGui::PopStyleColor(2);
        ImGui::PopID();
    }

    std::erase_if(sinkInputs, [](const auto& sink)
    {
        return sink.removeFromList;
    });
}

int main(void)
{
    std::vector<SinkInputDisplay> sinkInputs;

    GLFWwindow* window;

    /* Initialize the library */
    if (!glfwInit())
        return -1;

	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    /* Create a windowed mode window and its OpenGL context */float main_scale = ImGui_ImplGlfw_GetContentScaleForMonitor(glfwGetPrimaryMonitor()); // Valid on GLFW 3.3+ only
    window = glfwCreateWindow((int)(400 * main_scale), (int)(800 * main_scale), "Dear ImGui GLFW+OpenGL3 example", nullptr, nullptr);
    if (!window)
    {
        glfwTerminate();
        return -1;
    }

    /* Make the window's context current */
    glfwMakeContextCurrent(window);
    gladLoadGL(glfwGetProcAddress);


    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();
    //ImGui::StyleColorsLight();

    // Setup scaling
    ImGuiStyle& style = ImGui::GetStyle();
    style.ScaleAllSizes(main_scale);        // Bake a fixed style scale. (until we have a solution for dynamic style scaling, changing this requires resetting Style + calling this again)
    style.FontScaleDpi = main_scale;        // Set initial font scale. (in docking branch: using io.ConfigDpiScaleFonts=true automatically overrides this for every window depending on the current monitor)

    // Setup Platform/Renderer backends
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init(nullptr);
    
    bool show_demo_window = true;
    bool show_another_window = false;
    bool p_open = true;
    ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

    /* Loop until the user closes the window */
    while (!glfwWindowShouldClose(window))
    {
        /* Poll for and process events */
        glfwPollEvents();
        // if (glfwGetWindowAttrib(window, GLFW_ICONIFIED) != 0)
        // {
        //     ImGui_ImplGlfw_Sleep(10);
        //     continue;
        // }


        // Start the Dear ImGui frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        ProcessUpdateFromAudio(sinkInputs);

        if (show_demo_window)
            ImGui::ShowDemoWindow(&show_demo_window);

        // 2. Show a simple window that we create ourselves. We use a Begin/End pair to create a named window.
        {
            static float f = 0.0f;
            static int counter = 0;

            ImGui::Begin("Hello, world!");                          // Create a window called "Hello, world!" and append into it.

            ImGui::Text("This is some useful text.");               // Display some text (you can use a format strings too)
            ImGui::Checkbox("Demo Window", &show_demo_window);      // Edit bools storing our window open/close state
            ImGui::Checkbox("Another Window", &show_another_window);

            ImGui::SliderFloat("float", &f, 0.0f, 1.0f);            // Edit 1 float using a slider from 0.0f to 1.0f
            ImGui::ColorEdit3("clear color", (float*)&clear_color); // Edit 3 floats representing a color

            if (ImGui::Button("Button"))                            // Buttons return true when clicked (most widgets return true when edited/activated)
                counter++;
            ImGui::SameLine();
            ImGui::Text("counter = %d", counter);

            ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / io.Framerate, io.Framerate);
            ImGui::End();
        }

        // 3. Show another simple window.
        if (show_another_window)
        {
            ImGui::Begin("Another Window", &show_another_window);   // Pass a pointer to our bool variable (the window will have a closing button that will clear the bool when clicked)
            ImGui::Text("Hello from another window!");
            if (ImGui::Button("Close Me"))
                show_another_window = false;
            ImGui::End();
        }

        //
        //  Tu się zaczyna właściwy kod
        //
        // imgui_demo.cpp:10166
        // Na razie zamykalne, żeby w razie czegopodpatrzeć demo window.
        static bool use_work_area = true;
        static ImGuiWindowFlags flags = ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings;

        // We demonstrate using the full viewport area or the work area (without menu-bars, task-bars etc.)
        // Based on your use case you may want one or the other.
        const ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(use_work_area ? viewport->WorkPos : viewport->Pos);
        ImGui::SetNextWindowSize(use_work_area ? viewport->WorkSize : viewport->Size);
        // typedef struct {
        //     uint32_t initialized;
        //     uint32_t index;                      /**< Index of the sink input */
        //     char name[SMALL_STR_LEN];

        //     char mediaName[MEDIUM_STR_LEN];
        //     char appName[MEDIUM_STR_LEN];
        //     pa_cvolume volume;
        // } SinkInputInfo;
        if (ImGui::Begin("Example: Fullscreen window", &p_open, flags))
        {
            DisplaySinkInputs(sinkInputs);
            // for (int i = 0; i < 5; i++)
            // {
            //     ImGui::PushID(i);
            //     ImGui::SeparatorText("Sink input name");
            //     ImGui::Text("Name: %s%d", "Sink name", i);
            //     ImGui::Text("Media: %s%d", "Media name", i);
            //     ImGui::Text("Application: %s%d", "Application name", i);
            //     bool dumCorked = i % 2;
            //     ImGui::Checkbox("Corked", &dumCorked);
            //     float dumVolume = 0.8f;
            //     ImGui::SliderFloat("Volume", &dumVolume, 0.0f, 1.0f, "%2f");
            //     ImGui::PopID();
            // }
        }
        ImGui::End();
        //
        //  A tu się kończy
        //

        // Rendering
        ImGui::Render();
        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h);

        glViewport(0, 0, display_w, display_h);
        glClearColor(clear_color.x * clear_color.w, clear_color.y * clear_color.w, clear_color.z * clear_color.w, clear_color.w);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
    }

    // Cleanup
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(window);

    glfwTerminate();
    return 0;
}