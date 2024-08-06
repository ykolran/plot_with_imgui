#pragma once
#include <vector>
#include <string>
#include "../implot/implot.h"
#include <limits>

#ifdef max
#undef max
#undef min
#endif

struct Column
{
	std::string label_id;
	std::vector<double> ys;
	ImVec4 color;
	float alpha;
	ImPlotMarker marker;
	float thickness;
	bool line;
	bool show;

	// histogram params
	bool histogram;
	bool cumulative;
	bool density;
	bool no_outliers;
	int bins;
};

class Plot
{
public:
	Plot()
	{
		Counter++;
		sprintf_s(_name, "Figure %d", Counter);
		_open = true;
		_initialized = false;
	}
	void AddCol(std::string name, std::vector<double> ys, ImVec4 color = ImVec4(0,0,0,-1), bool histogram = false) 
	{ 
		_columns.push_back({name, ys, color.w == -1 ? ImPlot::GetColormapColor(static_cast<int>(_columns.size())) : color,
			0.5f, ImPlotMarker_Circle,	1.0f, false, true, histogram, false, false, false, (int)ceil(1.0 + log2((double)ys.size())) });
	}
	void HandleKeyPressed();
	void AddDataTip();
	void Draw();

	bool* IsOpen() { return &_open; }
	const char* Name() { return _name; }

private:
	void ConvertBGRAtoRGBA(void* data, int width, int height);
	void CopyToClipboard(void* data, int width, int height, std::string filePath);
	void WriteDIBToClipboard(int width, int height, void* data);
	std::string CreateHTML(unsigned char* png, int len, std::string& filePath);
	std::string CreateRTF(const std::string& filePath, int width, int height, int len, unsigned char* png);
	void SaveTextureToPNG(const std::string& filename, const void* data, int width, int height);

	std::vector<Column> _columns;
	char _name[10];
	bool _open;
	bool _initialized;
    std::vector<std::pair<ImPlotPoint, ImVec4>> _annotations;
	ImVec2 _cursorPos;
	ImVec2 _extents;

	static int Counter;
};

