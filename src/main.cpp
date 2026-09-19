#include <cstdio>
#include <memory>
#include <string>
#include <vector>

#include "ClickerEngine.h"
#include "platform/PlatformInput.h"

#include "imgui.h"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_opengl3.h"

#include <GLFW/glfw3.h>

namespace {

void glfwErrorCallback(int error, const char* description) {
    std::fprintf(stderr, "GLFW error %d: %s\n", error, description);
}

// A compact dark theme with a single accent color, rounded controls.
void applyTheme() {
    ImGuiStyle& style = ImGui::GetStyle();
    ImVec4* colors = style.Colors;

    const ImVec4 accent(0.34f, 0.62f, 0.98f, 1.00f);
    const ImVec4 accentHover(0.44f, 0.70f, 1.00f, 1.00f);
    const ImVec4 accentActive(0.27f, 0.51f, 0.85f, 1.00f);

    colors[ImGuiCol_WindowBg]         = ImVec4(0.09f, 0.10f, 0.12f, 1.00f);
    colors[ImGuiCol_ChildBg]          = ImVec4(0.13f, 0.14f, 0.16f, 1.00f);
    colors[ImGuiCol_FrameBg]          = ImVec4(0.15f, 0.16f, 0.19f, 1.00f);
    colors[ImGuiCol_FrameBgHovered]   = ImVec4(0.19f, 0.21f, 0.25f, 1.00f);
    colors[ImGuiCol_FrameBgActive]    = ImVec4(0.21f, 0.23f, 0.27f, 1.00f);
    colors[ImGuiCol_Text]             = ImVec4(0.93f, 0.94f, 0.95f, 1.00f);
    colors[ImGuiCol_TextDisabled]     = ImVec4(0.52f, 0.55f, 0.59f, 1.00f);
    colors[ImGuiCol_Border]           = ImVec4(0.22f, 0.24f, 0.27f, 1.00f);
    colors[ImGuiCol_SliderGrab]       = accent;
    colors[ImGuiCol_SliderGrabActive] = accentActive;
    colors[ImGuiCol_Button]           = ImVec4(0.18f, 0.20f, 0.23f, 1.00f);
    colors[ImGuiCol_ButtonHovered]    = accentHover;
    colors[ImGuiCol_ButtonActive]     = accentActive;
    colors[ImGuiCol_CheckMark]        = accent;
    colors[ImGuiCol_TitleBg]          = ImVec4(0.07f, 0.08f, 0.09f, 1.00f);
    colors[ImGuiCol_TitleBgActive]    = ImVec4(0.07f, 0.08f, 0.09f, 1.00f);
    colors[ImGuiCol_PlotHistogram]    = accent;
    colors[ImGuiCol_Separator]        = ImVec4(0.22f, 0.24f, 0.27f, 1.00f);

    style.WindowRounding    = 10.0f;
    style.FrameRounding     = 6.0f;
    style.GrabRounding      = 6.0f;
    style.ChildRounding     = 8.0f;
    style.PopupRounding     = 6.0f;
    style.FramePadding      = ImVec2(10.0f, 6.0f);
    style.ItemSpacing       = ImVec2(10.0f, 10.0f);
    style.WindowPadding     = ImVec2(20.0f, 20.0f);
}

// Small uppercase caption used above each control group, e.g. "HOTKEY".
void sectionLabel(const char* text) {
    ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyle().Colors[ImGuiCol_TextDisabled]);
    ImGui::SetWindowFontScale(0.82f);
    ImGui::TextUnformatted(text);
    ImGui::SetWindowFontScale(1.0f);
    ImGui::PopStyleColor();
}

} // namespace

int main() {
    glfwSetErrorCallback(glfwErrorCallback);
    if (!glfwInit()) {
        std::fprintf(stderr, "Failed to initialize GLFW\n");
        return 1;
    }

#if defined(__APPLE__)
    const char* glslVersion = "#version 150";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#else
    const char* glslVersion = "#version 130";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
#endif

    GLFWwindow* window = glfwCreateWindow(440, 500, "EzAutoclicker", nullptr, nullptr);
    if (!window) {
        std::fprintf(stderr, "Failed to create GLFW window\n");
        glfwTerminate();
        return 1;
    }
    glfwSetWindowSizeLimits(window, 400, 420, GLFW_DONT_CARE, GLFW_DONT_CARE);
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1); // vsync: UI stays cheap, independent of click thread

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr; // no imgui.ini clutter for a small tool window
    ImGui::StyleColorsDark();
    applyTheme();

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init(glslVersion);

    std::unique_ptr<ezac::PlatformInput> input(ezac::PlatformInput::create());
    ezac::ClickerEngine engine(input.get());

    int cps = ezac::ClickerEngine::kDefaultCps;
    engine.setTargetCps(cps);

    const std::vector<ezac::KeyOption>& keyOptions = ezac::bindableKeyOptions();
    std::vector<bool> keyWasDown(keyOptions.size(), false);
    bool listeningForHotkey = false;
    int triggerKeyCode = engine.triggerKeyCode();

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        // While capturing a new hotkey, watch every bindable key globally
        // (via the same backend the click engine uses) for a fresh
        // press-down edge, so binding works the same way triggering does.
        if (listeningForHotkey) {
            for (size_t i = 0; i < keyOptions.size(); ++i) {
                const bool down = input->isKeyHeld(keyOptions[i].code);
                if (down && !keyWasDown[i]) {
                    triggerKeyCode = keyOptions[i].code;
                    engine.setTriggerKeyCode(triggerKeyCode);
                    listeningForHotkey = false;
                }
                keyWasDown[i] = down;
            }
            if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
                listeningForHotkey = false;
            }
        }

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(viewport->WorkPos);
        ImGui::SetNextWindowSize(viewport->WorkSize);
        ImGui::Begin("##main", nullptr,
                      ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                          ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse |
                          ImGuiWindowFlags_NoBringToFrontOnFocus);

        ImGui::SetWindowFontScale(1.3f);
        ImGui::TextUnformatted("EzAutoclicker");
        ImGui::SetWindowFontScale(1.0f);

        const bool active = engine.isActive();
        const std::string hotkeyName = ezac::keyDisplayName(triggerKeyCode);
        ImGui::TextDisabled("Hold %s to click - works in any window", hotkeyName.c_str());
        ImGui::Dummy(ImVec2(0, 10));

        // --- Status row ---------------------------------------------------
        {
            ImDrawList* draw = ImGui::GetWindowDrawList();
            ImVec2 p = ImGui::GetCursorScreenPos();
            const float dotR = 5.5f;
            const float lineH = ImGui::GetTextLineHeight();
            const ImU32 dotColor = active ? IM_COL32(88, 214, 141, 255) : IM_COL32(120, 122, 126, 255);
            draw->AddCircleFilled(ImVec2(p.x + dotR, p.y + lineH * 0.5f), dotR, dotColor);
            ImGui::Dummy(ImVec2(dotR * 2 + 10, 0));
            ImGui::SameLine();
            ImGui::TextColored(active ? ImVec4(0.62f, 0.92f, 0.75f, 1.0f) : ImVec4(0.78f, 0.79f, 0.81f, 1.0f),
                                "%s", active ? "Clicking" : "Idle");
        }

        ImGui::Dummy(ImVec2(0, 14));
        ImGui::Separator();
        ImGui::Dummy(ImVec2(0, 14));

        // --- Hotkey ---------------------------------------------------------
        sectionLabel("HOTKEY");
        ImGui::Dummy(ImVec2(0, 4));
        {
            ImGui::BeginChild("##keycap", ImVec2(160, 34), true, ImGuiWindowFlags_NoScrollbar);
            ImGui::SetCursorPos(ImVec2(12, 8));
            if (listeningForHotkey) {
                ImGui::TextColored(ImVec4(0.34f, 0.62f, 0.98f, 1.0f), "Press a key...");
            } else {
                ImGui::TextUnformatted(hotkeyName.c_str());
            }
            ImGui::EndChild();
        }
        ImGui::SameLine();
        if (!listeningForHotkey) {
            if (ImGui::Button("Change##hotkey", ImVec2(90, 34))) {
                listeningForHotkey = true;
                for (size_t i = 0; i < keyOptions.size(); ++i) {
                    keyWasDown[i] = input->isKeyHeld(keyOptions[i].code);
                }
            }
        } else {
            if (ImGui::Button("Cancel##hotkey", ImVec2(90, 34))) {
                listeningForHotkey = false;
            }
        }
        if (listeningForHotkey) {
            ImGui::TextDisabled("Press any listed key - Esc to cancel");
        }

        ImGui::Dummy(ImVec2(0, 16));

        // --- Click speed ------------------------------------------------
        sectionLabel("CLICK SPEED");
        ImGui::Dummy(ImVec2(0, 4));
        ImGui::SetNextItemWidth(-90);
        if (ImGui::SliderInt("##cps_slider", &cps, ezac::ClickerEngine::kMinCps,
                              ezac::ClickerEngine::kMaxCps, "%d CPS")) {
            engine.setTargetCps(cps);
        }
        ImGui::SameLine();
        ImGui::SetNextItemWidth(80);
        if (ImGui::InputInt("##cps_input", &cps, 0, 0)) {
            if (cps < ezac::ClickerEngine::kMinCps) cps = ezac::ClickerEngine::kMinCps;
            if (cps > ezac::ClickerEngine::kMaxCps) cps = ezac::ClickerEngine::kMaxCps;
            engine.setTargetCps(cps);
        }

        ImGui::Dummy(ImVec2(0, 16));

        // --- Live stats ---------------------------------------------------
        sectionLabel("STATS");
        ImGui::Dummy(ImVec2(0, 4));
        ImGui::BeginChild("##stats", ImVec2(0, 70), true, ImGuiWindowFlags_NoScrollbar);
        ImGui::Text("Live rate:    %6.1f CPS", engine.measuredCps());
        ImGui::Text("Total clicks: %lld", engine.totalClicks());
        ImGui::EndChild();

        const char* status = input->statusMessage();
        if (status && status[0] != '\0') {
            ImGui::Dummy(ImVec2(0, 12));
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.95f, 0.55f, 0.45f, 1.0f));
            ImGui::TextWrapped("%s", status);
            ImGui::PopStyleColor();
        }

        // --- Footer, pinned to the bottom of the window --------------------
        {
            const float footerH = ImGui::GetTextLineHeightWithSpacing() + 14.0f;
            const float remaining = ImGui::GetContentRegionAvail().y - footerH;
            if (remaining > 0.0f) ImGui::Dummy(ImVec2(0, remaining));
            ImGui::Separator();
            ImGui::Dummy(ImVec2(0, 4));
            ImGui::SetWindowFontScale(0.82f);
            ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyle().Colors[ImGuiCol_TextDisabled]);
            const char* footer = "Made by ukso";
            const float w = ImGui::CalcTextSize(footer).x;
            ImGui::SetCursorPosX(ImGui::GetWindowWidth() - w - ImGui::GetStyle().WindowPadding.x);
            ImGui::TextUnformatted(footer);
            ImGui::PopStyleColor();
            ImGui::SetWindowFontScale(1.0f);
        }

        ImGui::End();

        ImGui::Render();
        int displayW, displayH;
        glfwGetFramebufferSize(window, &displayW, &displayH);
        glViewport(0, 0, displayW, displayH);
        glClearColor(0.09f, 0.10f, 0.12f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(window);
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
