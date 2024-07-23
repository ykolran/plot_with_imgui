#pragma once

#include <d3d11.h>
#include <vector>
#include <string>
#include <set>
#include <map>
#include "Plot.h"

struct File
{
	std::string name;
	typedef std::vector<std::vector<std::string>> FileContent;
	FileContent content;
};

class PlotApp
{
public:
	static PlotApp& Instance() { static PlotApp instance; return instance; }
	int MainLoop();
	void AddPlot(const Plot& plot) { _plots.push_back(plot); }
	bool CaptureFramebuffer(int x, int y, int w, int h, unsigned int* pixels_rgba, void* user_data);
	void AddColToPlot(int idx, Plot& plot);

private:
	PlotApp();
	void ShowMainWindow();
	void CreateLists();

	// file manipulation
	File LoadCSV(const std::string& filename);
	static const char* FileNameGetter(void* user_data, int idx) { return PlotApp::Instance()._files[idx].name.c_str(); }

	static LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

	bool CreateDeviceD3D(HWND hWnd);
	void CreateRenderTarget();
	void CleanupDeviceD3D();
	void CleanupRenderTarget();

	ID3D11Device*			_pd3dDevice;
	ID3D11DeviceContext*	_pd3dDeviceContext;
	IDXGISwapChain*			_pSwapChain;
	UINT                    _ResizeWidth;
	UINT					_ResizeHeight;
	ID3D11RenderTargetView* _mainRenderTargetView;

	std::vector<Plot> _plots;
	bool _show_demo_window_implot;
	bool _show_demo_window_imgui;
	bool _show_main_window;

	size_t _currentFileIndex;
	std::set<int> _selectedFields;
	int _lastSelectedField;
	std::vector<File> _files;


};

