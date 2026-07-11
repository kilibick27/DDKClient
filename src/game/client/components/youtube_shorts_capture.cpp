#include "youtube_shorts_capture.h"

#include <base/mem.h>
#include <base/system.h>

#include <engine/gfx/image_manipulation.h>
#include <engine/graphics.h>
#include <engine/shared/config.h>

#include <game/localization.h>

#include <algorithm>
#include <cstring>
#include <vector>

#if defined(CONF_FAMILY_WINDOWS)
#undef NOGDI
#include <windows.h>
#endif

namespace
{
	constexpr int64_t WINDOW_SEARCH_INTERVAL_MS = 1500;
	constexpr int64_t CAPTURE_INTERVAL_MS = 200;
	constexpr int CAPTURE_MAX_DIMENSION = 480;
}

CYoutubeShortsCapture::~CYoutubeShortsCapture()
{
	ReleaseTexture();
}

void CYoutubeShortsCapture::ReleaseTexture()
{
	if(m_Texture.IsValid())
		Graphics()->UnloadTexture(&m_Texture);
}

void CYoutubeShortsCapture::Shutdown()
{
	ReleaseTexture();
	m_pBrowserWindow = nullptr;
}

bool CYoutubeShortsCapture::HasWindow() const
{
	return m_pBrowserWindow != nullptr;
}

#if defined(CONF_FAMILY_WINDOWS)

namespace
{
	struct SFindBrowserWindowContext
	{
		HWND m_Found = nullptr;
	};

	BOOL CALLBACK FindBrowserWindowProc(HWND Hwnd, LPARAM Param)
	{
		auto *pContext = reinterpret_cast<SFindBrowserWindowContext *>(Param);
		if(!IsWindowVisible(Hwnd))
			return TRUE;

		char aTitle[512];
		const int Length = GetWindowTextA(Hwnd, aTitle, sizeof(aTitle));
		if(Length <= 0)
			return TRUE;

		if(str_find_nocase(aTitle, "youtube") != nullptr)
		{
			pContext->m_Found = Hwnd;
			return FALSE;
		}
		return TRUE;
	}
}

void CYoutubeShortsCapture::FindBrowserWindow()
{
	HWND CurrentWindow = static_cast<HWND>(m_pBrowserWindow);
	if(CurrentWindow != nullptr && (!IsWindow(CurrentWindow) || !IsWindowVisible(CurrentWindow)))
	{
		CurrentWindow = nullptr;
		m_pBrowserWindow = nullptr;
		ReleaseTexture();
	}

	if(CurrentWindow != nullptr)
		return;

	SFindBrowserWindowContext Context;
	EnumWindows(FindBrowserWindowProc, reinterpret_cast<LPARAM>(&Context));
	m_pBrowserWindow = Context.m_Found;

	if(m_pBrowserWindow == nullptr)
		str_copy(m_aStatusText, Localize("No browser window with YouTube found. Open youtube.com/shorts in your browser."));
	else
		m_aStatusText[0] = '\0';
}

void CYoutubeShortsCapture::CaptureFrame()
{
	HWND Hwnd = static_cast<HWND>(m_pBrowserWindow);
	if(Hwnd == nullptr)
		return;

	RECT WindowRect;
	if(!GetClientRect(Hwnd, &WindowRect))
		return;
	const int SrcWidth = WindowRect.right - WindowRect.left;
	const int SrcHeight = WindowRect.bottom - WindowRect.top;
	if(SrcWidth <= 0 || SrcHeight <= 0)
		return;

	HDC WindowDc = GetDC(Hwnd);
	if(WindowDc == nullptr)
		return;

	HDC MemDc = CreateCompatibleDC(WindowDc);
	HBITMAP Bitmap = CreateCompatibleBitmap(WindowDc, SrcWidth, SrcHeight);
	HGDIOBJ OldObject = SelectObject(MemDc, Bitmap);

	// PW_RENDERFULLCONTENT is required to capture GPU-accelerated browser
	// content (Chrome/Edge/Firefox all composite via GPU by default).
	const BOOL Captured = PrintWindow(Hwnd, MemDc, 2 /* PW_RENDERFULLCONTENT */);

	bool Success = false;
	if(Captured)
	{
		BITMAPINFO BitmapInfo = {};
		BitmapInfo.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
		BitmapInfo.bmiHeader.biWidth = SrcWidth;
		BitmapInfo.bmiHeader.biHeight = -SrcHeight; // negative = top-down DIB
		BitmapInfo.bmiHeader.biPlanes = 1;
		BitmapInfo.bmiHeader.biBitCount = 32;
		BitmapInfo.bmiHeader.biCompression = BI_RGB;

		std::vector<uint8_t> vPixels(static_cast<size_t>(SrcWidth) * SrcHeight * 4);
		if(GetDIBits(MemDc, Bitmap, 0, SrcHeight, vPixels.data(), &BitmapInfo, DIB_RGB_COLORS) != 0)
		{
			// BGRA -> RGBA in place.
			for(size_t Pixel = 0; Pixel < vPixels.size(); Pixel += 4)
			{
				std::swap(vPixels[Pixel], vPixels[Pixel + 2]);
				vPixels[Pixel + 3] = 0xFF;
			}

			// Crop out the browser chrome (tabs/address bar) and isolate the
			// centered 9:16 shorts player instead of the whole tab content.
			const int CropTop = std::clamp(g_Config.m_DdkYoutubeShortsCropTop, 0, SrcHeight - 1);
			const int ContentHeight = SrcHeight - CropTop;
			const int FramedWidth = std::clamp((int)(ContentHeight * 9 / 16), 1, SrcWidth);

			// Zoom in further on the centered player, cropping away the
			// surrounding YouTube page UI (side buttons, up-next panel, etc.)
			// that still leaks into a plain 9:16 crop of the page content.
			const int Zoom = std::max(g_Config.m_DdkYoutubeShortsZoom, 100);
			const int TargetHeight = std::max(1, ContentHeight * 100 / Zoom);
			const int TargetWidth = std::max(1, FramedWidth * 100 / Zoom);
			const int CropTopZoomed = CropTop + (ContentHeight - TargetHeight) / 2;
			const int CropLeft = (SrcWidth - TargetWidth) / 2;

			std::vector<uint8_t> vCropped(static_cast<size_t>(TargetWidth) * TargetHeight * 4);
			for(int Row = 0; Row < TargetHeight; Row++)
			{
				const uint8_t *pSrcRow = vPixels.data() + (static_cast<size_t>(CropTopZoomed + Row) * SrcWidth + CropLeft) * 4;
				uint8_t *pDstRow = vCropped.data() + static_cast<size_t>(Row) * TargetWidth * 4;
				mem_copy(pDstRow, pSrcRow, (size_t)TargetWidth * 4);
			}

			CImageInfo Image;
			Image.m_Width = TargetWidth;
			Image.m_Height = TargetHeight;
			Image.m_Format = CImageInfo::FORMAT_RGBA;
			Image.m_pData = static_cast<uint8_t *>(malloc(vCropped.size()));
			mem_copy(Image.m_pData, vCropped.data(), vCropped.size());

			if(TargetWidth > CAPTURE_MAX_DIMENSION || TargetHeight > CAPTURE_MAX_DIMENSION)
			{
				const float Scale = (float)CAPTURE_MAX_DIMENSION / (float)std::max(TargetWidth, TargetHeight);
				const int NewWidth = std::max(1, (int)(TargetWidth * Scale));
				const int NewHeight = std::max(1, (int)(TargetHeight * Scale));
				ResizeImage(Image, NewWidth, NewHeight);
			}

			ReleaseTexture();
			m_Texture = Graphics()->LoadTextureRawMove(Image, 0, "youtube_shorts_capture");
			Success = m_Texture.IsValid();
		}
	}

	SelectObject(MemDc, OldObject);
	DeleteObject(Bitmap);
	DeleteDC(MemDc);
	ReleaseDC(Hwnd, WindowDc);

	if(!Success)
		str_copy(m_aStatusText, Localize("Failed to capture the browser window."));
}

namespace
{
	struct SFindRenderWidgetContext
	{
		HWND m_Found = nullptr;
	};

	// Chromium (Chrome/Edge/Brave/Opera all share this) routes real input to
	// an inner child window of class "Chrome_RenderWidgetHostHWND", not the
	// top-level frame window. EnumChildWindows walks the whole descendant
	// tree, so this finds it regardless of nesting depth.
	BOOL CALLBACK FindRenderWidgetProc(HWND Hwnd, LPARAM Param)
	{
		auto *pContext = reinterpret_cast<SFindRenderWidgetContext *>(Param);
		char aClassName[64];
		if(GetClassNameA(Hwnd, aClassName, sizeof(aClassName)) > 0 && strcmp(aClassName, "Chrome_RenderWidgetHostHWND") == 0)
		{
			pContext->m_Found = Hwnd;
			return FALSE;
		}
		return TRUE;
	}

	HWND FindRenderWidgetHost(HWND Root)
	{
		SFindRenderWidgetContext Context;
		EnumChildWindows(Root, FindRenderWidgetProc, reinterpret_cast<LPARAM>(&Context));
		return Context.m_Found;
	}
}

void CYoutubeShortsCapture::Scroll(EScrollDirection Direction)
{
	HWND Hwnd = static_cast<HWND>(m_pBrowserWindow);
	if(Hwnd == nullptr)
		return;

	const WORD VirtualKey = (Direction == EScrollDirection::UP) ? VK_UP : VK_DOWN;

	// 1) Post the arrow key YouTube Shorts natively binds to "next/previous
	// video" directly to the Chromium render surface. This needs neither
	// cursor movement nor OS focus/foreground, so it also works when the
	// browser window is fully hidden behind the game on the same monitor -
	// but Chromium may still ignore synthetic key messages on some builds.
	HWND RenderWidget = FindRenderWidgetHost(Hwnd);
	if(RenderWidget != nullptr)
	{
		const WORD ScanCode = (WORD)MapVirtualKeyA(VirtualKey, MAPVK_VK_TO_VSC);
		const LPARAM LParamDown = 1 | (ScanCode << 16) | (1 << 24);
		const LPARAM LParamUp = 1 | (ScanCode << 16) | (1 << 24) | (1 << 30) | (1 << 31);
		PostMessageA(RenderWidget, WM_KEYDOWN, VirtualKey, LParamDown);
		PostMessageA(RenderWidget, WM_KEYUP, VirtualKey, LParamUp);
	}

	// 2) Also fire a genuine wheel event via SendInput after briefly hovering
	// the real cursor over the browser (without SetForegroundWindow). Windows
	// routes real wheel input to whatever window is actually visible under
	// the cursor even without focus, which reliably covers the case where
	// the browser sits on another monitor and is fully visible there.
	RECT ClientRect;
	if(GetClientRect(Hwnd, &ClientRect))
	{
		POINT Center = {(ClientRect.right - ClientRect.left) / 2, (ClientRect.bottom - ClientRect.top) / 2};
		if(ClientToScreen(Hwnd, &Center))
		{
			POINT OriginalCursorPos;
			GetCursorPos(&OriginalCursorPos);

			SetCursorPos(Center.x, Center.y);

			INPUT Input = {};
			Input.type = INPUT_MOUSE;
			Input.mi.dwFlags = MOUSEEVENTF_WHEEL;
			Input.mi.mouseData = (Direction == EScrollDirection::UP) ? (DWORD)(WHEEL_DELTA * 3) : (DWORD)(-(WHEEL_DELTA * 3));
			SendInput(1, &Input, sizeof(INPUT));

			SetCursorPos(OriginalCursorPos.x, OriginalCursorPos.y);
		}
	}
}

#else // !CONF_FAMILY_WINDOWS

void CYoutubeShortsCapture::FindBrowserWindow()
{
	str_copy(m_aStatusText, Localize("Browser capture is only supported on Windows."));
}

void CYoutubeShortsCapture::CaptureFrame()
{
}

void CYoutubeShortsCapture::Scroll(EScrollDirection Direction)
{
	(void)Direction;
}

#endif

void CYoutubeShortsCapture::Update(bool Enabled)
{
	if(!Enabled)
	{
		if(HasWindow() || m_Texture.IsValid())
		{
			m_pBrowserWindow = nullptr;
			ReleaseTexture();
		}
		m_aStatusText[0] = '\0';
		return;
	}

	const int64_t Now = time_get();
	const int64_t NowMs = Now * 1000 / time_freq();

	if(!HasWindow() && NowMs - m_LastWindowSearchTick >= WINDOW_SEARCH_INTERVAL_MS)
	{
		FindBrowserWindow();
		m_LastWindowSearchTick = NowMs;
	}

	if(HasWindow() && NowMs - m_LastCaptureTick >= CAPTURE_INTERVAL_MS)
	{
		CaptureFrame();
		m_LastCaptureTick = NowMs;
	}
}
