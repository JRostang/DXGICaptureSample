// DXGICaptureSample.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include "DXGIManager.h"

DXGIManager g_DXGIManager;

int _tmain(int argc, _TCHAR* argv[])
{
	printf("DXGICaptureSample. Fast windows screen capture\n");
	printf("Capturing desktop to: capture.bmp\n");
	printf("Log: logfile.log\n");

	CoInitialize(NULL);

	HWND window = FindWindow(nullptr, L"Media Player");

	g_DXGIManager.setCaptureWindow(window);

	//Dimensions of the window to grab
	RECT rcDim;
	GetClientRect(window, &rcDim);

	DWORD dwWidth = rcDim.right - rcDim.left;
	DWORD dwHeight = rcDim.bottom - rcDim.top;

	printf("dwWidth=%d dwHeight=%d\n", dwWidth, dwHeight);

	DWORD dwBufSize = dwWidth*dwHeight*4;

	BYTE* pBuf = new BYTE[dwBufSize];

	ComPtr<IWICImagingFactory> spWICFactory = NULL;
	HRESULT hr = CoCreateInstance( CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, __uuidof(IWICImagingFactory), (void**)&spWICFactory);
	if(FAILED(hr)) return hr;

	int i=0;
	do {
		hr = g_DXGIManager.GetOutputBits(pBuf, rcDim);
		i++;
	}
	while (hr == DXGI_ERROR_WAIT_TIMEOUT || i < 2);

	//////////////////////////////////////////////////////////////////////////////////////
	if(FAILED(hr)) { printf("GetOutputBits failed with hr=0x%08x\n", hr); return hr; }

	printf("Saving capture to file\n");

	ComPtr<IWICBitmap> spBitmap = NULL;
	hr = spWICFactory->CreateBitmapFromMemory(dwWidth, dwHeight, GUID_WICPixelFormat32bppBGRA, dwWidth*4, dwBufSize, (BYTE*)pBuf, &spBitmap);
	if( FAILED(hr) )
		return hr;
		
	ComPtr<IWICStream> spStream = NULL;

	hr = spWICFactory->CreateStream(&spStream);
	if (SUCCEEDED(hr)) {
		hr = spStream->InitializeFromFilename(L"D:/Barco/Dev/DXGICaptureSample-master/Debug/capture.bmp", GENERIC_WRITE);
	}

	ComPtr<IWICBitmapEncoder> spEncoder = NULL;
	if (SUCCEEDED(hr)) {
		hr = spWICFactory->CreateEncoder(GUID_ContainerFormatBmp, NULL, &spEncoder);
	}

	if (SUCCEEDED(hr)) {
		hr = spEncoder->Initialize(spStream.Get(), WICBitmapEncoderNoCache);
	}

	ComPtr<IWICBitmapFrameEncode> spFrame = NULL;
	if (SUCCEEDED(hr)) {
		hr = spEncoder->CreateNewFrame(&spFrame, NULL);
	}

	if (SUCCEEDED(hr)) {
		hr = spFrame->Initialize(NULL);
	}

	if (SUCCEEDED(hr)) {
		hr = spFrame->SetSize(dwWidth, dwHeight);
	}

	WICPixelFormatGUID format;
	spBitmap->GetPixelFormat(&format);

	if (SUCCEEDED(hr)) {
		hr = spFrame->SetPixelFormat(&format);
	}

	if (SUCCEEDED(hr)) {
		hr = spFrame->WriteSource(spBitmap.Get(), NULL);
	}
		
	if (SUCCEEDED(hr)) {
		hr = spFrame->Commit();
	}

	if (SUCCEEDED(hr)) {
		hr = spEncoder->Commit();
	}

	delete[] pBuf;

	return 0;
}

