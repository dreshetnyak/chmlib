#pragma once

#include <string>
#include <Windows.h>
#include <atlbase.h>
//#include <atlapp.h>
#include <atlimage.h>
#include <atlfile.h>
//#include <atlgdi.h>
#include <vector>
#include "GlobalMem.h"
#include <thumbcache.h>
#include <wincodec.h>

namespace Gfx
{
	using namespace std;
	//using namespace WTL;
	using namespace Gdiplus;

	enum class ImageFileType { Bmp, Png, Jpg };

	HRESULT LoadImageToHBitmap(const char* sourceImage, size_t size, HBITMAP& outBitmap, WTS_ALPHATYPE& outAlphaType, SIZE& imageSize);

	HBITMAP ToBitmap(CImage& sourceImage, LPSIZE bitmapSize);
	bool SaveImage(HBITMAP bitmapHandle, const wstring& fileName, ImageFileType fileType);
	bool GetEncoderClsid(const wstring& mimeType, CLSID* outClsid);
	const wchar_t* GetMimeType(ImageFileType fileType);
	bool ImageSizeSatisfiesCoverConstraints(const SIZE& imageSize);
}