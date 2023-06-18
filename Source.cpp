#pragma comment(linker,"\"/manifestdependency:type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

#pragma comment(lib, "d2d1")
#pragma comment(lib, "dwrite")

#include <windows.h>
#include <windowsx.h>
#include <d2d1_3.h>
#include <dwrite.h>
#include <vector>
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

class CObject {
public:
	BOOL bSelected;
	D2D1::Matrix3x2F local;
	ID2D1RoundedRectangleGeometry* pRoundedRectangleGeometry;
	CObject(ID2D1Factory* m_pD2DFactory) : bSelected(FALSE) {
		local = D2D1::Matrix3x2F::Identity();
		D2D1_ROUNDED_RECT roundedRect = D2D1::RoundedRect(D2D1::RectF(-50, -50, 50, 50), 10, 10);
		m_pD2DFactory->CreateRoundedRectangleGeometry(roundedRect, &pRoundedRectangleGeometry);
	}
	~CObject() {
		SafeRelease(&pRoundedRectangleGeometry);
	}
	void Draw(ID2D1RenderTarget* m_pRenderTarget, ID2D1SolidColorBrush* m_pNormalBrush, const D2D1::Matrix3x2F& global) {
		m_pRenderTarget->SetTransform(local * global);
		m_pRenderTarget->FillGeometry(pRoundedRectangleGeometry, m_pNormalBrush);
	}
	void SetPosition(const D2D1_POINT_2F& pt) {
		local.dx = pt.x * (1.0f / local.m11);
		local.dy = pt.y * (1.0f / local.m11);
		//local = D2D1::Matrix3x2F::Translation(pt.x, pt.y);
	}
	D2D1_POINT_2F GetPosition() const {
		return D2D1::Point2F(local.dx, local.dy);
	}
	BOOL HitTest(const D2D1_POINT_2F& pt, const D2D1::Matrix3x2F& global) {
		D2D1_POINT_2F pt2 = D2D1::Point2F(pt.x, pt.y);
		BOOL bHitTest = FALSE;
		pRoundedRectangleGeometry->FillContainsPoint(pt2, local * global, &bHitTest);
		return bHitTest;
	}
	void SetSelected(BOOL bSelected) {
		this->bSelected = bSelected;
	}
	BOOL GetSelected() {
		return bSelected;
	}
	BOOL HitTestInRect(const D2D1_RECT_F& rect, const D2D1::Matrix3x2F& global) {
		D2D1_RECT_F rect2;
		pRoundedRectangleGeometry->GetBounds(local * global, &rect2);
		if (rect.left <= rect2.left && rect2.left <= rect.right &&
			rect.left <= rect2.right && rect2.right <= rect.right &&
			rect.top <= rect2.top && rect2.top <= rect.bottom &&
			rect.top <= rect2.bottom && rect2.bottom <= rect.bottom
			) {
			return TRUE;
		}
		else {
			return FALSE;
		}
	}
};

LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	static ID2D1Factory* m_pD2DFactory;
	static ID2D1HwndRenderTarget* m_pRenderTarget;
	static ID2D1SolidColorBrush* m_pNormalBrush;
	static ID2D1SolidColorBrush* m_pSelectedBrush;
	static D2D1::Matrix3x2F matrix;
	static BOOL bDrag = FALSE;
	static D2D1_POINT_2F ptDragObject;
	static POINT ptDragStart;
	static POINT ptDragEnd;
	static HCURSOR hCursorOpenHand;
	static HCURSOR hCursorClosedHand;
	static BOOL bRectangleSelectMode = FALSE;
	static ID2D1StrokeStyle* pStrokeStyle;
	static std::vector<CObject*> objectlist;
	static std::vector<CObject*> dragobjectlist;
	switch (msg)
	{
	case WM_CREATE:
		{
			hCursorOpenHand = LoadCursor(((LPCREATESTRUCT)lParam)->hInstance, MAKEINTRESOURCE(IDC_OPENHAND));
			hCursorClosedHand = LoadCursor(((LPCREATESTRUCT)lParam)->hInstance, MAKEINTRESOURCE(IDC_CLOSEDHAND));
			HRESULT hr = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &m_pD2DFactory);
			if (FAILED(hr)) {
				MessageBox(hWnd, L"Direct2D の初期化に失敗しました。", 0, 0);
			}
			RECT rect;
			GetClientRect(hWnd, &rect);
			matrix = D2D1::Matrix3x2F::Identity();
			D2D1_POINT_2F ptObject;
			CObject* pObject1 = new CObject(m_pD2DFactory);
			ptObject.x = (FLOAT)((rect.right - rect.left) / 2.0F - 100.0F / 2.0F - 100.0f);
			ptObject.y = (FLOAT)((rect.bottom - rect.top) / 2.0F - 100.0F / 2.0F);
			pObject1->SetPosition(ptObject);
			objectlist.push_back(pObject1);
			CObject* pObject2 = new CObject(m_pD2DFactory);
			ptObject.x = (FLOAT)((rect.right - rect.left) / 2.0F - 100.0F / 2.0F + 100.0f);
			ptObject.y = (FLOAT)((rect.bottom - rect.top) / 2.0F - 100.0F / 2.0F);
			pObject2->SetPosition(ptObject);
			objectlist.push_back(pObject2);
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
			ptDragEnd.x = x;
			ptDragEnd.y = y;
			D2D1_POINT_2F pt2 = D2D1::Point2F((FLOAT)x, (FLOAT)y);
			for (auto i : objectlist) {
				i->SetSelected(FALSE);
			}
			dragobjectlist.clear();
			for (auto i : objectlist) {
				if (i->HitTest(pt2, matrix)) {
					ptDragObject = i->GetPosition(); // ドラッグ開始時のオブジェクトの位置を記憶
					i->SetSelected(TRUE);
					dragobjectlist.push_back(i);
					break;
				}
			}
			if (dragobjectlist.size() > 0) {
				bDrag = TRUE;
				SetCursor(hCursorClosedHand);
				SetCapture(hWnd);
			} else {
				bRectangleSelectMode = TRUE;
				SetCapture(hWnd);
			}
			InvalidateRect(hWnd, 0, 0);
		}
		break;
	case WM_MOUSEMOVE:
		{
			int x = GET_X_LPARAM(lParam);
			int y = GET_Y_LPARAM(lParam);
			if (bDrag) {
				D2D1_POINT_2F ptObject;
				ptObject.x = ptDragObject.x + (FLOAT)((x - ptDragStart.x) * 1.0f / matrix.m11);
				ptObject.y = ptDragObject.y + (FLOAT)((y - ptDragStart.y) * 1.0f / matrix.m11);
				for (auto& i : dragobjectlist) {
					i->SetPosition(ptObject);
				}
				InvalidateRect(hWnd, 0, 0);
				SetCursor(hCursorClosedHand);
			}
			else if (bRectangleSelectMode) {
				ptDragEnd.x = x;
				ptDragEnd.y = y;

				for (auto& i : objectlist) {
					D2D1_RECT_F rect;
					rect.left = (FLOAT)min(ptDragStart.x, ptDragEnd.x);
					rect.top = (FLOAT)min(ptDragStart.y, ptDragEnd.y);
					rect.right = (FLOAT)max(ptDragStart.x, ptDragEnd.x);
					rect.bottom = (FLOAT)max(ptDragStart.y, ptDragEnd.y);
					if (i->HitTestInRect(rect, matrix) == TRUE) {
						i->SetSelected(TRUE);
					}
					else {
						i->SetSelected(FALSE);
					}
				}

				InvalidateRect(hWnd, 0, 0);
			} 
			else {
				D2D1_POINT_2F pt2 = D2D1::Point2F((FLOAT)x, (FLOAT)y);
				BOOL bHit = FALSE;
				for (auto i : objectlist) {
					if (i->HitTest(pt2, matrix)) {
						bHit = TRUE;
						SetCursor(hCursorOpenHand);
						break;
					}
				}
				if (bHit == FALSE) {
					SetCursor(LoadCursor(0, IDC_ARROW));
				}
			}
		}
		break;
	case WM_LBUTTONUP:
		if (bDrag) {
			dragobjectlist.clear();
			ReleaseCapture();
			bDrag = FALSE;
		}
		if (bRectangleSelectMode) {
			ReleaseCapture();
			bRectangleSelectMode = FALSE;
		}
		InvalidateRect(hWnd, 0, 0);
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
					hr = m_pRenderTarget->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::Red), &m_pSelectedBrush);
				if (SUCCEEDED(hr)) {
					hr = m_pD2DFactory->CreateStrokeStyle(
						D2D1::StrokeStyleProperties(
							D2D1_CAP_STYLE_FLAT,
							D2D1_CAP_STYLE_FLAT,
							D2D1_CAP_STYLE_FLAT,
							D2D1_LINE_JOIN_MITER,
							10.0f,
							D2D1_DASH_STYLE_DASH,
							0.0f),
						0,
						0,
						&pStrokeStyle
					);
				}
			}
			if (SUCCEEDED(hr))
			{
				m_pRenderTarget->BeginDraw();
			}
			m_pRenderTarget->Clear(D2D1::ColorF(D2D1::ColorF::White));
			for (auto i : objectlist) {
				i->Draw(m_pRenderTarget, i->GetSelected()? m_pSelectedBrush : m_pNormalBrush, matrix);
			}
			if (bRectangleSelectMode && m_pNormalBrush && pStrokeStyle) {
				m_pRenderTarget->SetTransform(D2D1::Matrix3x2F::Identity());
				m_pRenderTarget->DrawRectangle(D2D1::RectF((FLOAT)(ptDragStart.x), (FLOAT)(ptDragStart.y), (FLOAT)(ptDragEnd.x), (FLOAT)(ptDragEnd.y)), m_pNormalBrush, 1.0F, pStrokeStyle);
			}
			hr = m_pRenderTarget->EndDraw();
			if (hr == D2DERR_RECREATE_TARGET)
			{
				SafeRelease(&m_pRenderTarget);
				SafeRelease(&m_pNormalBrush);
				SafeRelease(&m_pSelectedBrush);
				SafeRelease(&pStrokeStyle);
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
		for (auto i : objectlist) {
			delete i;
		}
		objectlist.clear();
		SafeRelease(&m_pD2DFactory);
		SafeRelease(&m_pRenderTarget);
		SafeRelease(&m_pNormalBrush);
		SafeRelease(&m_pSelectedBrush);
		SafeRelease(&pStrokeStyle);
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
