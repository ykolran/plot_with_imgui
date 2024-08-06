#include "Plot.h"
#include "PlotApp.h"
#include <Windows.h>
#include <iostream>
#include <algorithm>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#define __STDC_LIB_EXT1__
#include "../stb/stb_image_write.h"

#ifdef max
#undef max
#undef min
#endif

int Plot::Counter = 0;

const char* MarkerNameGetter(void* user_data, int idx)
{
    return ImPlot::GetMarkerName(idx);
}

void Plot::Draw()
{
    _cursorPos = ImGui::GetCursorPos();
    _extents = ImGui::GetContentRegionAvail();

    if (ImPlot::BeginPlot("My Title##Plot", _extents))
    {
        if (!_initialized)
        {
            ImPlot::SetupAxes("Time", "Diff [Kg]", ImPlotAxisFlags_EditableLabel, ImPlotAxisFlags_EditableLabel);
            _initialized = true;
        }

        if (ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows))
            HandleKeyPressed();

        for (auto& vec : _annotations)
            ImPlot::Annotation(vec.first.x, vec.first.y, vec.second, ImVec2(10, 10), true);

        for (auto& col : _columns)
        {
            if (ImPlot::BeginLegendPopup(col.label_id.c_str()))
            {
                ImGui::ColorEdit3("Color", &col.color.x);
                if (!col.histogram)
                {
                    ImGui::Checkbox("Line", &col.line);
                    if (col.line) {
                        ImGui::SliderFloat("Thickness", &col.thickness, 0, 5);
                    }
                    ImGui::Combo("Marker Type", &col.marker, &MarkerNameGetter, nullptr, ImPlotMarker_COUNT);
                }
                else
                {
                    ImGui::Checkbox("Cumulative", &col.cumulative);
                    ImGui::Checkbox("Density", &col.density);
                    ImGui::Checkbox("Remove Outliers", &col.no_outliers);
                    ImGui::SliderInt("Bins", &col.bins, 2, static_cast<int>(col.ys.size() / 2));
                }
                if (col.marker != ImPlotMarker_None || col.histogram)
                    ImGui::SliderFloat("Fill", &col.alpha, 0, 1, "%.2f");
                if (!col.histogram)
                {
                    if (ImGui::Button("Histogram"))
                    {
                        Plot histogram;
                        histogram.AddCol(col.label_id, col.ys, col.color, true);
                        PlotApp::Instance().AddPlot(histogram);
                    }
                }
                ImPlot::EndLegendPopup();
            }
            else if (ImPlot::IsLegendEntryHovered(col.label_id.c_str()))
            {
                if (ImGui::IsMouseClicked(ImGuiMouseButton_Left))
                    col.show = !col.show;
            }

            ImPlot::PushStyleVar(ImPlotStyleVar_FillAlpha, col.alpha);
            ImPlot::SetNextMarkerStyle(col.marker);
            ImPlot::SetNextLineStyle(col.color, col.thickness);
            ImPlot::HideNextItem(!col.show, ImGuiCond_Always);

            if (col.histogram)
            {
                ImPlot::SetNextFillStyle(col.color, col.alpha);
                ImPlotHistogramFlags flags = 0;
                if (col.cumulative) flags |= ImPlotHistogramFlags_Cumulative;
                if (col.density) flags |= ImPlotHistogramFlags_Density;
                if (col.no_outliers) flags |= ImPlotHistogramFlags_NoOutliers;
                ImPlot::PlotHistogram(col.label_id.c_str(), col.ys.data(), static_cast<int>(col.ys.size()), col.bins, 1.0,
                    ImPlotRange(), flags);
            }
            else if (!col.line)
                ImPlot::PlotScatter(col.label_id.c_str(), col.ys.data(), static_cast<int>(col.ys.size()));
            else
                ImPlot::PlotLine(col.label_id.c_str(), col.ys.data(), static_cast<int>(col.ys.size()));

        }

        if (ImGui::BeginDragDropTarget()) {
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ColDragAndDrop")) {
                int i = *(int*)payload->Data; 
                PlotApp::Instance().AddColToPlot(i, *this);
            }
            ImGui::EndDragDropTarget();
        }

        ImPlot::EndPlot();
    }
}

void Plot::HandleKeyPressed()
{
    if (ImGui::IsKeyPressed(ImGuiKey_D))
    {
        AddDataTip();
    }

    if (ImGui::IsKeyPressed(ImGuiKey_I))
    {
        for (auto& col : _columns)
            col.show = !col.show;
    }

    if (ImGui::IsKeyPressed(ImGuiKey_P))
    {
        static const int max_width = 5000;
        static const int max_height = 5000;
        static unsigned int pixels_rgba[max_width * max_height];
        
        int width = std::min((int)_extents.x, max_width);
        int height = std::min((int)_extents.y, max_height);
        if (PlotApp::Instance().CaptureFramebuffer((int)_cursorPos.x, (int)_cursorPos.y, width, height, pixels_rgba, nullptr))
        {
            CopyToClipboard(pixels_rgba, width, height, "c:\\temp\\test.png");
            SaveTextureToPNG("c:\\temp\\test.png", pixels_rgba, width, height);
        }
    }
}

void Plot::AddDataTip()
{
    auto mousePos = ImPlot::GetPlotMousePos();
    double dist = std::numeric_limits<double>::max();
    ImPlotPoint point;
    ImVec4 color;
    for (int col = 0; col < _columns.size(); col++)
    {
        for (int i = 0; i < _columns[col].ys.size(); i++)
        {
            double currDist = (i - mousePos.x) * (i - mousePos.x) + (_columns[col].ys[i] - mousePos.y) * (_columns[col].ys[i] - mousePos.y);
            if (currDist < dist)
            {
                dist = currDist;
                point.x = i;
                point.y = _columns[col].ys[i];
                color = ImPlot::GetColormapColor(col);
            }
        }
    }

    _annotations.push_back(std::make_pair(point, color));
}

// Helper function to convert BGRA to RGBA
void Plot::ConvertBGRAtoRGBA(void* data, int width, int height) {
    unsigned char* pixels = static_cast<unsigned char*>(data);
    for (int i = 0; i < width * height; ++i) {
        std::swap(pixels[i * 4 + 0], pixels[i * 4 + 2]); // Swap blue and red channels
    }
}

void Plot::SaveTextureToPNG(const std::string& filename, const void* data, int width, int height) {
    stbi_write_png(filename.c_str(), width, height, 4, data, width * 4);
}

// Helper function to write HTML to the clipboard
void WriteHTMLToClipboard(const std::string& html) {
    // Open the clipboard
    if (!OpenClipboard(nullptr)) {
        std::cerr << "Unable to open clipboard" << std::endl;
        return;
    }

    // Create the HTML clipboard format
    UINT cfHtml = RegisterClipboardFormatA("HTML Format");
    if (cfHtml == 0) {
        std::cerr << "Unable to register HTML clipboard format" << std::endl;
        CloseClipboard();
        return;
    }

    // Calculate the total size for the global memory block
    size_t size = html.length() + 1; // +1 for the null terminator

    // Allocate global memory
    HGLOBAL hGlobal = GlobalAlloc(GHND, size);
    if (!hGlobal) {
        std::cerr << "Unable to allocate global memory" << std::endl;
        CloseClipboard();
        return;
    }

    // Lock the global memory and copy the HTML content
    char* pGlobal = static_cast<char*>(GlobalLock(hGlobal));
    if (pGlobal)
    {
        memcpy(pGlobal, html.c_str(), size);
        GlobalUnlock(hGlobal);


        // Set the HTML content to the clipboard
        if (!SetClipboardData(cfHtml, hGlobal)) {
            std::cerr << "Unable to set clipboard data" << std::endl;
            GlobalFree(hGlobal);
            CloseClipboard();
            return;
        }
    }

    CloseClipboard();
    std::cout << "HTML content successfully written to the clipboard" << std::endl;
}


std::string base64_encode(unsigned char const* bytes_to_encode, unsigned int len)
{
    static const std::string base64_chars =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz"
        "0123456789+/";
    std::string ret;
    int i = 0;
    int j = 0;
    unsigned char char_array_3[3];
    unsigned char char_array_4[4];

    while (len--) {
        char_array_3[i++] = *(bytes_to_encode++);
        if (i == 3) {
            char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
            char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
            char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
            char_array_4[3] = char_array_3[2] & 0x3f;

            for (i = 0; (i < 4); i++)
                ret += base64_chars[char_array_4[i]];
            i = 0;
        }
    }

    if (i) {
        for (j = i; j < 3; j++)
            char_array_3[j] = '\0';

        char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
        char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
        char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
        char_array_4[3] = char_array_3[2] & 0x3f;

        for (j = 0; (j < i + 1); j++)
            ret += base64_chars[char_array_4[j]];

        while ((i++ < 3))
            ret += '=';
    }

    return ret;
}

// Helper function to write RTF to the clipboard
void WriteRTFToClipboard(const std::string& rtf) {
    if (!OpenClipboard(nullptr)) {
        std::cerr << "Unable to open clipboard" << std::endl;
        return;
    }

    UINT cfRtf = RegisterClipboardFormatA("Rich Text Format");
    if (cfRtf == 0) {
        std::cerr << "Unable to register RTF clipboard format" << std::endl;
        CloseClipboard();
        return;
    }

    size_t size = rtf.length() + 1;
    HGLOBAL hGlobal = GlobalAlloc(GHND, size);
    if (!hGlobal) {
        std::cerr << "Unable to allocate global memory" << std::endl;
        CloseClipboard();
        return;
    }

    char* pGlobal = static_cast<char*>(GlobalLock(hGlobal));
    if (pGlobal)
    {
        memcpy(pGlobal, rtf.c_str(), size);
        GlobalUnlock(hGlobal);

        if (!SetClipboardData(cfRtf, hGlobal)) {
            std::cerr << "Unable to set clipboard data" << std::endl;
            GlobalFree(hGlobal);
            CloseClipboard();
            return;
        }
    }
    CloseClipboard();
    std::cout << "RTF content successfully written to the clipboard" << std::endl;
}

void Plot::CopyToClipboard(void* data, int width, int height, std::string filePath) 
{
    int len;
    unsigned char* png = stbi_write_png_to_mem(static_cast<unsigned char*>(data), width * 4, width, height, 4, &len);
    std::replace(filePath.begin(), filePath.end(), '\\', '/');

    std::string html = CreateHTML(png, len, filePath);
    WriteHTMLToClipboard(html);

//    std::string rtf = CreateRTF(filePath, width, height, len, png);
//    WriteRTFToClipboard(rtf);

    WriteDIBToClipboard(width, height, data);
}

void Plot::WriteDIBToClipboard(int width, int height, void* data)
{
    // Calculate the size of the bitmap info header
    int headerSize = sizeof(BITMAPINFOHEADER);
    int dataSize = width * height * 4;

    // Create a global memory object for the DIB
    HGLOBAL hGlobal = GlobalAlloc(GHND, headerSize + dataSize);
    if (hGlobal) {
        // Lock the memory and copy the data
        void* pGlobal = GlobalLock(hGlobal);
        if (pGlobal) {
            // Initialize the BITMAPINFOHEADER
            BITMAPINFOHEADER* bih = (BITMAPINFOHEADER*)pGlobal;
            bih->biSize = sizeof(BITMAPINFOHEADER);
            bih->biWidth = width;
            bih->biHeight = -height; // Negative height for top-down DIB
            bih->biPlanes = 1;
            bih->biBitCount = 32;
            bih->biCompression = BI_RGB;
            bih->biSizeImage = 0;
            bih->biXPelsPerMeter = 0;
            bih->biYPelsPerMeter = 0;
            bih->biClrUsed = 0;
            bih->biClrImportant = 0;

            // Copy the image data and convert BGRA to RGBA
            void* pData = (void*)((BYTE*)pGlobal + headerSize);
            memcpy(pData, data, dataSize);
            ConvertBGRAtoRGBA(pData, width, height);

            GlobalUnlock(hGlobal);

            // Open the clipboard and set the DIB
            if (OpenClipboard(nullptr)) {
                SetClipboardData(CF_DIB, hGlobal);
                CloseClipboard();
            }
        }

        // Free the global memory if it wasn't set to the clipboard
        if (GetClipboardData(CF_DIB) != hGlobal) {
            GlobalFree(hGlobal);
        }
    }
}

std::string Plot::CreateHTML(unsigned char* png, int len, std::string& filePath)
{
    // Base64 encode the image data
    std::string base64Image = "data:image/png;base64," + base64_encode(png, len);


    std::string preHtml = R"(Version:1.0
StartHTML:00000000
EndHTML:00000000
StartFragment:00000000
EndFragment:00000000
<!DOCTYPE html>
<html>
<body>
<!--StartFragment-->)";
    std::string postHtml = R"(<!--EndFragment-->
</body>
</html>)";
    std::string fragment = "<a href=\"" + filePath + "\"><img src=\"" + base64Image + "\" alt=\"Embedded Image\"></a>";

    size_t startHTML = preHtml.size() - 96; // Offset due to placeholder
    size_t startFragment = startHTML + preHtml.size();
    size_t endFragment = startFragment + fragment.size();
    size_t endHTML = endFragment + postHtml.size();

    std::string html = preHtml + fragment + postHtml;
    char number[9];
    sprintf_s(number, "%08llu", startHTML); html.replace(22, 8, number);
    sprintf_s(number, "%08llu", endHTML); html.replace(49, 8, number);
    sprintf_s(number, "%08llu", startFragment); html.replace(62, 8, number);
    sprintf_s(number, "%08llu", endFragment); html.replace(83, 8, number);

    return html;
}

std::string Plot::CreateRTF(const std::string& filePath, int width, int height, int len, unsigned char* png)
{
    std::string rtf = "{\\rtf1\\ansi\\deff0 ";
    rtf += "{\\field{\\fldinst HYPERLINK \"" + filePath + "\"}{\\fldrslt {\\pict\\pngblip ";
    rtf += "\\picw" + std::to_string(width) + "\\pich" + std::to_string(height);
    rtf += "\\picwgoal" + std::to_string(width * 20) + "\\pichgoal" + std::to_string(height * 20) + "\\picscalex100\\picscaley100";
    rtf += "\\bin ";
    for (int i = 0; i < len; i++)
    {
        char byteAsHex[3];
        sprintf_s(byteAsHex, "%02X", png[i]);
        rtf += byteAsHex;
    }

    rtf += "}}}\n";
    rtf += "}\n";
    return rtf;
}
