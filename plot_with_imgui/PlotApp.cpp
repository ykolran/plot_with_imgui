#include "PlotApp.h"
#include "../imgui/imgui.h"
#include "../imgui/backends/imgui_impl_win32.h"
#include "../imgui/backends/imgui_impl_dx11.h"
#include "../implot/implot.h"
#include <string>
#include <set>
#include <map>
#include <vector>
#include <fstream>
#include <algorithm>
#include <sstream>
#include "Plot.h"


PlotApp::PlotApp()
{
	_pd3dDevice = nullptr;
	_pd3dDeviceContext = nullptr;
	_pSwapChain = nullptr;
	_ResizeWidth = 0;
	_ResizeHeight = 0;
	_mainRenderTargetView = nullptr;

    _show_demo_window_imgui = true;
    _show_demo_window_implot = true;
    _show_main_window = true;
    _currentFileIndex = 0;
    _lastSelectedField = -1;
}

// Forward declare message handler from imgui_impl_win32.cpp
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

// Win32 message handler
// You can read the io.WantCaptureMouse, io.WantCaptureKeyboard flags to tell if dear imgui wants to use your inputs.
// - When io.WantCaptureMouse is true, do not dispatch mouse input data to your main application, or clear/overwrite your copy of the mouse data.
// - When io.WantCaptureKeyboard is true, do not dispatch keyboard input data to your main application, or clear/overwrite your copy of the keyboard data.
// Generally you may always pass all inputs to dear imgui, and hide them from your application based on those two flags.
LRESULT WINAPI PlotApp::WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
        return true;

    switch (msg)
    {
    case WM_SIZE:
        if (wParam == SIZE_MINIMIZED)
            return 0;
        PlotApp::Instance()._ResizeWidth = (UINT)LOWORD(lParam); // Queue resize
        PlotApp::Instance()._ResizeHeight = (UINT)HIWORD(lParam);
        return 0;
    case WM_SYSCOMMAND:
        if ((wParam & 0xfff0) == SC_KEYMENU) // Disable ALT application menu
            return 0;
        break;
    case WM_DESTROY:
        ::PostQuitMessage(0);
        return 0;
    case WM_DPICHANGED:
        if (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_DpiEnableScaleViewports)
        {
            //const int dpi = HIWORD(wParam);
            //printf("WM_DPICHANGED to %d (%.0f%%)\n", dpi, (float)dpi / 96.0f * 100.0f);
            const RECT* suggested_rect = (RECT*)lParam;
            ::SetWindowPos(hWnd, nullptr, suggested_rect->left, suggested_rect->top, suggested_rect->right - suggested_rect->left, suggested_rect->bottom - suggested_rect->top, SWP_NOZORDER | SWP_NOACTIVATE);
        }
        break;
    }
    return ::DefWindowProcW(hWnd, msg, wParam, lParam);
}


std::string trimQuotes(const std::string& str) {
    size_t first = str.find_first_not_of('"');
    if (first == std::string::npos) {
        return ""; // No non-quote characters found
    }
    size_t last = str.find_last_not_of('"');
    return str.substr(first, last - first + 1);
}


File PlotApp::LoadCSV(const std::string& filename)
{
    File data;
    data.name = filename;
    std::ifstream file(filename);
    std::string line;

    while (std::getline(file, line)) {
        std::stringstream lineStream(line);
        std::string cell;
        std::vector<std::string> row;

        while (std::getline(lineStream, cell, ',')) {
            row.push_back(trimQuotes(cell));
        }

        data.content.push_back(row);
    }

    return data;
}


int PlotApp::MainLoop()
{
    // Create application window
    //ImGui_ImplWin32_EnableDpiAwareness();
    WNDCLASSEXW wc = { sizeof(wc), CS_CLASSDC, WndProc, 0L, 0L, GetModuleHandle(nullptr), nullptr, nullptr, nullptr, nullptr, L"ImGui Example", nullptr };
    ::RegisterClassExW(&wc);
    HWND hwnd = ::CreateWindowW(wc.lpszClassName, L"Dear ImGui DirectX11 Example", WS_OVERLAPPEDWINDOW, 100, 100, 1280, 800, nullptr, nullptr, wc.hInstance, nullptr);

    // Initialize Direct3D
    if (!CreateDeviceD3D(hwnd))
    {
        CleanupDeviceD3D();
        ::UnregisterClassW(wc.lpszClassName, wc.hInstance);
        return 1;
    }

    // Show the window
    ::ShowWindow(hwnd, SW_HIDE);
    ::UpdateWindow(hwnd);

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImPlot::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;         // Enable Docking
    io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;       // Enable Multi-Viewport / Platform Windows
    io.ConfigViewportsNoAutoMerge = true;
    io.ConfigViewportsNoDefaultParent = true;
    //io.ConfigViewportsNoTaskBarIcon = true;

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();
    //ImGui::StyleColorsLight();

    // When viewports are enabled we tweak WindowRounding/WindowBg so platform windows can look identical to regular ones.
    ImGuiStyle& style = ImGui::GetStyle();
    if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
    {
        style.WindowRounding = 0.0f;
        style.Colors[ImGuiCol_WindowBg].w = 1.0f;
    }

    // Setup Platform/Renderer backends
    ImGui_ImplWin32_Init(hwnd);
    ImGui_ImplDX11_Init(_pd3dDevice, _pd3dDeviceContext);

    // Load Fonts
    // - If no fonts are loaded, dear imgui will use the default font. You can also load multiple fonts and use ImGui::PushFont()/PopFont() to select them.
    // - AddFontFromFileTTF() will return the ImFont* so you can store it if you need to select the font among multiple.
    // - If the file cannot be loaded, the function will return a nullptr. Please handle those errors in your application (e.g. use an assertion, or display an error and quit).
    // - The fonts will be rasterized at a given size (w/ oversampling) and stored into a texture when calling ImFontAtlas::Build()/GetTexDataAsXXXX(), which ImGui_ImplXXXX_NewFrame below will call.
    // - Use '#define IMGUI_ENABLE_FREETYPE' in your imconfig file to use Freetype for higher quality font rendering.
    // - Read 'docs/FONTS.md' for more instructions and details.
    // - Remember that in C/C++ if you want to include a backslash \ in a string literal you need to write a double backslash \\ !
    //io.Fonts->AddFontDefault();
    //io.Fonts->AddFontFromFileTTF("c:\\Windows\\Fonts\\segoeui.ttf", 18.0f);
    //io.Fonts->AddFontFromFileTTF("../../misc/fonts/DroidSans.ttf", 16.0f);
    //io.Fonts->AddFontFromFileTTF("../../misc/fonts/Roboto-Medium.ttf", 16.0f);
    //io.Fonts->AddFontFromFileTTF("../../misc/fonts/Cousine-Regular.ttf", 15.0f);
    //ImFont* font = io.Fonts->AddFontFromFileTTF("c:\\Windows\\Fonts\\ArialUni.ttf", 18.0f, nullptr, io.Fonts->GetGlyphRangesJapanese());
    //IM_ASSERT(font != nullptr);

    // Our state
    ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

    ImPlot::GetInputMap().Select = ImGuiMouseButton_Left;
    ImPlot::GetInputMap().SelectCancel = ImGuiMouseButton_Right;
    ImPlot::GetInputMap().Pan = ImGuiMouseButton_Right;

    DragAcceptFiles(hwnd, true);
    // Main loop
    bool done = false;
    while (!done)
    {
        // Poll and handle messages (inputs, window resize, etc.)
        // See the WndProc() function below for our to dispatch events to the Win32 backend.
        MSG msg;
        while (::PeekMessage(&msg, nullptr, 0U, 0U, PM_REMOVE))
        {
            ::TranslateMessage(&msg);
            ::DispatchMessage(&msg);
            if (msg.message == WM_QUIT)
                done = true;
            if (msg.message == WM_DROPFILES)
            {
                HDROP hdrop = (HDROP)msg.wParam;
                UINT numFiles = DragQueryFile(hdrop, 0xFFFFFFFF, NULL, 0);
                for (UINT i = 0; i < numFiles; i++)
                {
                    char filename[256];
                    DragQueryFileA(hdrop, i, filename, 256);
                    _files.push_back(LoadCSV(filename));
                }

            }
        }
        if (done)
            break;

        // Handle window resize (we don't resize directly in the WM_SIZE handler)
        if (_ResizeWidth != 0 && _ResizeHeight != 0)
        {
            CleanupRenderTarget();
            _pSwapChain->ResizeBuffers(0, _ResizeWidth, _ResizeHeight, DXGI_FORMAT_UNKNOWN, 0);
            _ResizeWidth = _ResizeHeight = 0;
            CreateRenderTarget();
        }

        // Start the Dear ImGui frame
        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        if (_show_demo_window_implot)
            ImPlot::ShowDemoWindow(&_show_demo_window_implot);
        if (_show_demo_window_imgui)
            ImGui::ShowDemoWindow(&_show_demo_window_imgui);
        if (_show_main_window)
            ShowMainWindow();

        if (!_show_demo_window_imgui && !_show_demo_window_implot && !_show_main_window)
            PostMessage(hwnd, WM_CLOSE, 0, 0);

        // Rendering
        ImGui::Render();
        const float clear_color_with_alpha[4] = { clear_color.x * clear_color.w, clear_color.y * clear_color.w, clear_color.z * clear_color.w, clear_color.w };
        _pd3dDeviceContext->OMSetRenderTargets(1, &_mainRenderTargetView, nullptr);
        _pd3dDeviceContext->ClearRenderTargetView(_mainRenderTargetView, clear_color_with_alpha);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

        // Update and Render additional Platform Windows
        if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
        {
            ImGui::UpdatePlatformWindows();
            ImGui::RenderPlatformWindowsDefault();
        }

        _pSwapChain->Present(1, 0); // Present with vsync
        //g_pSwapChain->Present(0, 0); // Present without vsync
    }

    DragAcceptFiles(hwnd, false);
    // Cleanup
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

    CleanupDeviceD3D();
    ::DestroyWindow(hwnd);
    ::UnregisterClassW(wc.lpszClassName, wc.hInstance);

    return 0;
}

void PlotApp::ShowMainWindow()
{
    ImGui::Begin("DragFilesHere", &_show_main_window);
    DragAcceptFiles((HWND)ImGui::GetWindowViewport()->PlatformHandleRaw, true);

    for (auto& plot : _plots)
    {
        if (ImGui::Begin(plot.Name(), plot.IsOpen(), ImGuiWindowFlags_NoSavedSettings))
            plot.Draw();
        ImGui::End();
    }

    if (ImGui::Button("PLOT") && _currentFileIndex >= 0 && _currentFileIndex < _files.size() &&
        !_selectedFields.empty())
    {
        Plot plot;
        for (auto field : _selectedFields)
        {
            AddColToPlot(field, plot);
        }
        _plots.emplace_back(plot);
        ImGui::SetNextWindowSize(ImVec2(800, 600));
        if (ImGui::Begin(plot.Name(), plot.IsOpen(), ImGuiWindowFlags_NoSavedSettings))
            plot.Draw();
        ImGui::End();
    }

    _plots.erase(std::remove_if(_plots.begin(), _plots.end(), [](auto plot) {return !*plot.IsOpen(); }), _plots.end());
    ImGui::Spacing();

    CreateLists();

    ImGui::End();
}

void PlotApp::AddColToPlot(int idx, Plot& plot) 
{ 
    std::vector<double> ys;
    for (int row = 1; row < _files[_currentFileIndex].content.size(); row++)
        ys.push_back(atof(_files[_currentFileIndex].content[row][idx].c_str()));

    plot.AddCol(_files[_currentFileIndex].content[0][idx], ys);
}

void PlotApp::CreateLists()
{
    float width = ImGui::GetWindowWidth() - 3 * ImGui::GetStyle().ItemSpacing.x;
    ImGui::Text("File Names"); ImGui::SameLine(width / 3 + 2* ImGui::GetStyle().ItemSpacing.x);
    ImGui::Text("Fields");

    if (ImGui::BeginListBox("##FileNames", ImVec2(width / 3, ImGui::GetContentRegionAvail().y)))
    {
        for (size_t fileIdx = 0; fileIdx < _files.size(); fileIdx++)
        {
            bool selected = _currentFileIndex == fileIdx;
            ImGui::Selectable(_files[fileIdx].name.c_str(), &selected);
            if (selected && _currentFileIndex != fileIdx)
            {
                _currentFileIndex = fileIdx;
                _selectedFields.clear();
                _lastSelectedField = -1;
            }
        }
        ImGui::EndListBox();
    }
    ImGui::SameLine();
    if (ImGui::BeginListBox("##Fields", ImVec2(2 * width / 3, ImGui::GetContentRegionAvail().y)))
    {
        if (_currentFileIndex >= 0 && _currentFileIndex < _files.size())
        {
            for (int row = 0; row < _files[_currentFileIndex].content[0].size(); row++)
            {
                bool beforeSelectedField = _selectedFields.find(row) != _selectedFields.end();
                bool selectedField = beforeSelectedField;
                ImGui::Selectable(_files[_currentFileIndex].content[0][row].c_str(), &selectedField);
                if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None)) {
                    ImGui::SetDragDropPayload("ColDragAndDrop", &row, sizeof(int));
                    //ImPlot::ItemIcon(dnd[k].Color); ImGui::SameLine();
                    ImGui::TextUnformatted(_files[_currentFileIndex].content[0][row].c_str());
                    ImGui::EndDragDropSource();
                }

                if (beforeSelectedField != selectedField)
                {
                    if (!ImGui::IsKeyDown(ImGuiKey_ModCtrl))
                    {
                        _selectedFields.clear();
                    }

                    if (selectedField)
                    {
                        _selectedFields.insert(row);
                        if (ImGui::IsKeyDown(ImGuiKey_ModShift) && _lastSelectedField != -1)
                        {
                            for (int i = std::min(row, _lastSelectedField); i < std::max(row, _lastSelectedField); i++)
                                _selectedFields.insert(i);
                        }
                        _lastSelectedField = row;
                    }
                    else
                    {
                        _selectedFields.erase(row);
                        _lastSelectedField = -1;
                    }

                }
            }
        }
        ImGui::EndListBox();
    }
}

bool PlotApp::CreateDeviceD3D(HWND hWnd)
{
    // Setup swap chain
    DXGI_SWAP_CHAIN_DESC sd;
    ZeroMemory(&sd, sizeof(sd));
    sd.BufferCount = 2;
    sd.BufferDesc.Width = 0;
    sd.BufferDesc.Height = 0;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate.Numerator = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hWnd;
    sd.SampleDesc.Count = 1;
    sd.SampleDesc.Quality = 0;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    UINT createDeviceFlags = 0;
    //createDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
    D3D_FEATURE_LEVEL featureLevel;
    const D3D_FEATURE_LEVEL featureLevelArray[2] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0, };
    HRESULT res = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, createDeviceFlags, 
        featureLevelArray, 2, D3D11_SDK_VERSION, 
        &sd, &_pSwapChain, &_pd3dDevice, &featureLevel, &_pd3dDeviceContext);
    if (res == DXGI_ERROR_UNSUPPORTED) // Try high-performance WARP software driver if hardware is not available.
        res = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, createDeviceFlags, 
            featureLevelArray, 2, D3D11_SDK_VERSION, 
            &sd, &_pSwapChain, &_pd3dDevice, &featureLevel, &_pd3dDeviceContext);
    if (res != S_OK)
        return false;

    CreateRenderTarget();
    return true;
}

void PlotApp::CleanupDeviceD3D()
{
    CleanupRenderTarget();
    if (_pSwapChain) { _pSwapChain->Release(); _pSwapChain = nullptr; }
    if (_pd3dDeviceContext) { _pd3dDeviceContext->Release(); _pd3dDeviceContext = nullptr; }
    if (_pd3dDevice) { _pd3dDevice->Release(); _pd3dDevice = nullptr; }
}

void PlotApp::CreateRenderTarget()
{
    ID3D11Texture2D* pBackBuffer;
    _pSwapChain->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer));
    if (pBackBuffer)
    {
        _pd3dDevice->CreateRenderTargetView(pBackBuffer, nullptr, &_mainRenderTargetView);
        pBackBuffer->Release();
    }
}

void PlotApp::CleanupRenderTarget()
{
    if (_mainRenderTargetView) { _mainRenderTargetView->Release(); _mainRenderTargetView = nullptr; }
}


bool PlotApp::CaptureFramebuffer(int x, int y, int w, int h, unsigned int* pixels_rgba, void* user_data)
{
    ImGuiViewport* viewport = ImGui::GetWindowViewport();
    return ImGuiApp_ImplWin32DX11_CaptureFramebuffer(_mainRenderTargetView,
        _pd3dDevice, _pd3dDeviceContext, viewport,
        x, y, w, h, pixels_rgba, user_data);
}

