#ifndef GAME_CLIENT_COMPONENTS_YOUTUBE_SHORTS_CAPTURE_H
#define GAME_CLIENT_COMPONENTS_YOUTUBE_SHORTS_CAPTURE_H

#include <engine/graphics.h>

#include <game/client/component.h>

#include <string>

class CYoutubeShortsCapture : public CComponentInterfaces
{
public:
	enum class EScrollDirection
	{
		UP,
		DOWN,
	};

	~CYoutubeShortsCapture() override;

	// Call every frame the YouTube Shorts panel is visible. `Enabled` mirrors
	// the panel's checkbox: while false, no window search/capture happens and
	// any held texture is released.
	void Update(bool Enabled);
	void Shutdown();

	IGraphics::CTextureHandle Texture() const { return m_Texture; }
	bool HasWindow() const;
	const char *StatusText() const { return m_aStatusText; }

	void Scroll(EScrollDirection Direction);

private:
	void *m_pBrowserWindow = nullptr; // HWND on Windows, always nullptr elsewhere
	IGraphics::CTextureHandle m_Texture;
	int64_t m_LastWindowSearchTick = 0;
	int64_t m_LastCaptureTick = 0;
	char m_aStatusText[256] = "";

	void FindBrowserWindow();
	void CaptureFrame();
	void ReleaseTexture();
};

#endif
