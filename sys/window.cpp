#include "common.hpp"
#include "window.hpp"

using namespace winrt;
using namespace Windows::Foundation;
using namespace Windows::Web::UI::Interop;
using namespace WebView;

Window::Window(
	const std::wstring& title,
	const std::wstring& url,
	SIZE size,
	bool resizable) :
	m_hwnd(nullptr)
{
	HINSTANCE hInstance = GetModuleHandle(nullptr);
	if (hInstance == nullptr)
	{
		winrt::throw_last_error();
	}

	WNDCLASSEXW wcex;
	wcex.cbSize = sizeof(WNDCLASSEX);
	wcex.style = CS_HREDRAW | CS_VREDRAW;
	wcex.lpfnWndProc = Window::s_WndProc;
	wcex.cbClsExtra = 0;
	wcex.cbWndExtra = 0;
	wcex.hInstance = hInstance;
	wcex.hIcon = nullptr;
	wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
	wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
	wcex.lpszMenuName = nullptr;
	wcex.lpszClassName = L"WebViewControlWindow";
	wcex.hIconSm = nullptr;

	if (::RegisterClassExW(&wcex) == 0)
	{
		winrt::throw_last_error();
	}

	HWND hwnd = ::CreateWindowW(
		L"WebViewControlWindow", 
		title.c_str(),
		resizable ? WS_OVERLAPPEDWINDOW : (WS_OVERLAPPEDWINDOW ^ WS_THICKFRAME),
		CW_USEDEFAULT,
		CW_USEDEFAULT,
		size.cx,
		size.cy,
		nullptr,
		nullptr,
		hInstance,
		reinterpret_cast<LPVOID>(this));

	if (hwnd == nullptr)
	{
		winrt::throw_last_error();
	}

	ShowWindow(hwnd, SW_SHOW);
	UpdateWindow(hwnd);

	m_process = WebViewControlProcess();
	
	const Rect bounds = _GetBounds();
	auto op = m_process.CreateWebViewControlAsync(reinterpret_cast<int64_t>(hwnd), bounds);

	op.Completed([this, url](auto op, auto status)
	{
		m_control = op.GetResults();
		m_control.IsVisible(true);

		Uri uri(url.c_str());
		m_control.Navigate(uri);
		//m_control.NavigateToString(winrt::to_hstring("Hello World!"));
	});
}

Window::~Window()
{
	m_process.Terminate();
}

int Window::Run() noexcept
{
	winrt::init_apartment(winrt::apartment_type::single_threaded);

	MSG msg;
	while (::GetMessage(&msg, nullptr, 0, 0))
	{
		::TranslateMessage(&msg);
		::DispatchMessage(&msg);
	}

	return (int)msg.wParam;
}

/*static*/ LRESULT CALLBACK Window::s_WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	Window* window = nullptr;

	if (msg == WM_NCCREATE)
	{
		CREATESTRUCT* create = reinterpret_cast<CREATESTRUCT*>(lParam);
		window = reinterpret_cast<Window*>(create->lpCreateParams);
		::SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(window));

		window->m_hwnd = hwnd;
	}
	else
	{
		window = reinterpret_cast<Window*>(::GetWindowLongPtr(hwnd, GWLP_USERDATA));
	}

	if (window != nullptr)
	{
		return window->_HandleMessage(msg, wParam, lParam);
	}

	return ::DefWindowProc(hwnd, msg, wParam, lParam);
}

Rect Window::_GetBounds()
{
	RECT clientRect;
	::GetClientRect(m_hwnd, &clientRect);

	Rect bounds;
	bounds.X = static_cast<float>(clientRect.left);
	bounds.Y = static_cast<float>(clientRect.top);
	bounds.Width = static_cast<float>(clientRect.right - clientRect.left);
	bounds.Height = static_cast<float>(clientRect.bottom - clientRect.top);
	return bounds;
}

void Window::_UpdateBounds()
{
	if (m_control)
	{
		m_control.Bounds(_GetBounds());
	}
}

LRESULT Window::_HandleMessage(UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch (msg)
	{
	case WM_DESTROY:
		::PostQuitMessage(0);
		break;
	case WM_SIZE:
		_UpdateBounds();
		break;
	default:
		return DefWindowProc(m_hwnd, msg, wParam, lParam);
	}

	return 0;
}