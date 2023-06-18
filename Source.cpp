#pragma comment(linker,"\"/manifestdependency:type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

#pragma comment(lib, "d2d1")
#pragma comment(lib, "dwrite")

#include <windows.h>
#include <windowsx.h>
#include <d2d1_3.h>
#include <dwrite.h>
#include "resource.h"

#define DEFAULT_DPI 96
#define ZOOM_MAX 100.0
#define ZOOM_MIN 0.1
#define ZOOM_STEP 1.20

TCHAR szClassName[] = TEXT("Window");

template<class Interface> inline void SafeRelease(Interface** ppInterfaceToRelease)
{
	if (*ppInterfaceToRelease != NULL)
	{
		(*ppInterfaceToRelease)->Release();
		(*ppInterfaceToRelease) = NULL;
	}
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	static ID2D1Factory* m_pD2DFactory;
	static ID2D1HwndRenderTarget* m_pRenderTarget;
	static ID2D1SolidColorBrush* m_pNormalBrush;
	static ID2D1DeviceContext* m_pDeviceContext;
	static D2D1::Matrix3x2F matrix;
	static BOOL bDrag = FALSE;
	static POINT ptDragStart;
	static ID2D1RoundedRectangleGeometry* pRoundedRectangleGeometry;
	static HCURSOR hCursorOpenHand;
	static HCURSOR hCursorClosedHand;
	switch (msg)
	{
	case WM_CREATE:
	{
		hCursorOpenHand = LoadCursor(((LPCREATESTRUCT)lParam)->hInstance, MAKEINTRESOURCE(IDC_OPENHAND));
		hCursorClosedHand = LoadCursor(((LPCREATESTRUCT)lParam)->hInstance, MAKEINTRESOURCE(IDC_CLOSEDHAND));
		HRESULT hr = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &m_pD2DFactory);
		if (SUCCEEDED(hr)) {
			D2D1_ROUNDED_RECT roundedRect = D2D1::RoundedRect(D2D1::RectF(0, 0, 100, 100), 10, 10);
			hr = m_pD2DFactory->CreateRoundedRectangleGeometry(roundedRect, &pRoundedRectangleGeometry);
		}
		if (FAILED(hr)) {
			MessageBox(hWnd, L"Direct2D の初期化に失敗しました。", 0, 0);
		}
		RECT rect;
		GetClientRect(hWnd, &rect);

		matrix = D2D1::Matrix3x2F::Identity();
	}
	break;
	case WM_MOUSEWHEEL:
	{
		POINT point = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
		ScreenToClient(hWnd, &point);
		float zoom = 1.0f;
		int delta = GET_WHEEL_DELTA_WPARAM(wParam);
		if (delta < 0) {
			zoom = 0.9f;
			if (matrix.m11 < ZOOM_MIN) {
				zoom = 1.0f;
			}
		}
		else {
			zoom = 1.1f;
			if (matrix.m11 > ZOOM_MAX) {
				zoom = 1.0f;
			}
		}
		matrix = matrix *
			D2D1::Matrix3x2F::Scale((FLOAT)zoom, (FLOAT)zoom, D2D1::Point2F((FLOAT)(point.x), (FLOAT)(point.y)));
		InvalidateRect(hWnd, 0, 0);
	}
	break;
	case WM_LBUTTONDOWN:
	{
		int x = GET_X_LPARAM(lParam);
		int y = GET_Y_LPARAM(lParam);
		ptDragStart.x = x;
		ptDragStart.y = y;
		BOOL contains;
		if (pRoundedRectangleGeometry->FillContainsPoint(D2D1::Point2F((FLOAT)(x), (FLOAT)(y)), matrix, &contains) == S_OK && contains) {
			bDrag = TRUE;
			SetCursor(hCursorClosedHand);
			SetCapture(hWnd);
		}
	}
	break;
	case WM_MOUSEMOVE:
		if (bDrag) {
			int x = GET_X_LPARAM(lParam);
			int y = GET_Y_LPARAM(lParam);
			auto point2F2 = matrix.TransformPoint(D2D1::Point2F((FLOAT)(0), (FLOAT)(0)));
			matrix = matrix *
				D2D1::Matrix3x2F::Translation((FLOAT)(x - ptDragStart.x), (FLOAT)(y - ptDragStart.y));
			InvalidateRect(hWnd, 0, 0);
			ptDragStart.x = x;
			ptDragStart.y = y;
			SetCursor(hCursorClosedHand);
		}
		else {
			int x = GET_X_LPARAM(lParam);
			int y = GET_Y_LPARAM(lParam);
			BOOL contains;
			if (pRoundedRectangleGeometry->FillContainsPoint(D2D1::Point2F((FLOAT)(x), (FLOAT)(y)), matrix, &contains) == S_OK && contains) {
				SetCursor(hCursorOpenHand);
			}
			else {
				SetCursor(LoadCursor(0, IDC_ARROW));
			}
		}
		break;
	case WM_LBUTTONUP:
		if (bDrag) {
			ReleaseCapture();
			bDrag = FALSE;
		}
		break;
	case WM_PAINT:
	{
		ValidateRect(hWnd, NULL);
		HRESULT hr = S_OK;
		if (!m_pRenderTarget)
		{
			RECT rect;
			GetClientRect(hWnd, &rect);
			D2D1_SIZE_U CSize = D2D1::SizeU(rect.right, rect.bottom);
			hr = m_pD2DFactory->CreateHwndRenderTarget(D2D1::RenderTargetProperties(D2D1_RENDER_TARGET_TYPE_DEFAULT, D2D1::PixelFormat(), DEFAULT_DPI, DEFAULT_DPI, D2D1_RENDER_TARGET_USAGE_NONE, D2D1_FEATURE_LEVEL_DEFAULT), D2D1::HwndRenderTargetProperties(hWnd, CSize), &m_pRenderTarget);
			if (SUCCEEDED(hr))
				hr = m_pRenderTarget->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::Black), &m_pNormalBrush);
			if (SUCCEEDED(hr))
				hr = m_pRenderTarget->QueryInterface(&m_pDeviceContext);
		}
		if (SUCCEEDED(hr))
		{
			m_pRenderTarget->BeginDraw();
		}

		m_pRenderTarget->SetTransform(matrix);

		m_pRenderTarget->Clear(D2D1::ColorF(D2D1::ColorF::White));

		D2D1_POINT_2F s = { (float)0.0f, (float)0.0f };
		D2D1_POINT_2F e = { (float)100.0f, (float)100.0f };
		if (pRoundedRectangleGeometry) {
			m_pRenderTarget->DrawGeometry(pRoundedRectangleGeometry, m_pNormalBrush, 5.0F);
		}
		hr = m_pRenderTarget->EndDraw();
		if (hr == D2DERR_RECREATE_TARGET)
		{
			SafeRelease(&m_pRenderTarget);
			SafeRelease(&m_pNormalBrush);
			SafeRelease(&m_pDeviceContext);
		}
	}
	break;
	case WM_SIZE:
		if (m_pRenderTarget)
		{
			RECT rect;
			GetClientRect(hWnd, &rect);
			D2D1_SIZE_U CSize = { (UINT32)rect.right, (UINT32)rect.bottom };
			m_pRenderTarget->Resize(CSize);
		}
		break;
	case WM_DESTROY:
		SafeRelease(&pRoundedRectangleGeometry);
		SafeRelease(&m_pD2DFactory);
		SafeRelease(&m_pRenderTarget);
		SafeRelease(&m_pNormalBrush);
		SafeRelease(&m_pDeviceContext);
		PostQuitMessage(0);
		break;
	default:
		return DefWindowProc(hWnd, msg, wParam, lParam);
	}
	return 0;
}

int WINAPI wWinMain(_In_ HINSTANCE hInstance,
	_In_opt_ HINSTANCE hPrevInstance,
	_In_ LPWSTR lpCmdLine,
	_In_ int nShowCmd)
{
	MSG msg;
	WNDCLASS wndclass = {
		CS_HREDRAW | CS_VREDRAW,
		WndProc,
		0,
		0,
		hInstance,
		0,
		NULL,
		(HBRUSH)(COLOR_WINDOW + 1),
		0,
		szClassName
	};
	RegisterClass(&wndclass);
	HWND hWnd = CreateWindow(
		szClassName,
		L"Window",
		WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
		CW_USEDEFAULT,
		0,
		CW_USEDEFAULT,
		0,
		0,
		0,
		hInstance,
		0
	);
	ShowWindow(hWnd, SW_SHOWDEFAULT);
	UpdateWindow(hWnd);
	while (GetMessage(&msg, 0, 0, 0))
	{
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}
	return (int)msg.wParam;
}
