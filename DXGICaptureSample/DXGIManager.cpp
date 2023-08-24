#include "stdafx.h"
#include "DXGIManager.h"
#include <gdiplus.h>

using namespace Gdiplus;

DXGIPointerInfo::DXGIPointerInfo(BYTE* pPointerShape, UINT uiPointerShapeBufSize, DXGI_OUTDUPL_FRAME_INFO fi, DXGI_OUTDUPL_POINTER_SHAPE_INFO psi)
    : m_pPointerShape(pPointerShape),
    m_uiPointerShapeBufSize(uiPointerShapeBufSize),
    m_FI(fi),
    m_PSI(psi)
{
}

DXGIPointerInfo::~DXGIPointerInfo()
{
    if (m_pPointerShape)
    {
        delete[] m_pPointerShape;
    }
}

BYTE* DXGIPointerInfo::GetBuffer()
{
    return m_pPointerShape;
}

UINT DXGIPointerInfo::GetBufferSize()
{
    return m_uiPointerShapeBufSize;
}

DXGI_OUTDUPL_FRAME_INFO& DXGIPointerInfo::GetFrameInfo()
{
    return m_FI;
}

DXGI_OUTDUPL_POINTER_SHAPE_INFO& DXGIPointerInfo::GetShapeInfo()
{
    return m_PSI;
}

DXGIOutputDuplication::DXGIOutputDuplication(IDXGIAdapter1* pAdapter,
    ID3D11Device* pD3DDevice,
    ID3D11DeviceContext* pD3DDeviceContext,
    IDXGIOutput1* pDXGIOutput1,
    IDXGIOutputDuplication* pDXGIOutputDuplication)
    : m_Adapter(pAdapter),
    m_D3DDevice(pD3DDevice),
    m_D3DDeviceContext(pD3DDeviceContext),
    m_DXGIOutput1(pDXGIOutput1),
    m_DXGIOutputDuplication(pDXGIOutputDuplication)
{
}

HRESULT DXGIOutputDuplication::GetDesc(DXGI_OUTPUT_DESC& desc)
{
    m_DXGIOutput1->GetDesc(&desc);
    return S_OK;
}

HRESULT DXGIOutputDuplication::AcquireNextFrame(IDXGISurface1** pDXGISurface, DXGIPointerInfo*& pDXGIPointer)
{
    DXGI_OUTDUPL_FRAME_INFO fi;
    ComPtr<IDXGIResource> spDXGIResource;
    HRESULT hr = m_DXGIOutputDuplication->AcquireNextFrame(20, &fi, &spDXGIResource);
    if (FAILED(hr))
    {
        __L_INFO("m_DXGIOutputDuplication->AcquireNextFrame failed with hr=0x%08x", hr);
        return hr;
    }

    ComPtr<ID3D11Texture2D> spTextureResource; spDXGIResource.As(&spTextureResource);

    D3D11_TEXTURE2D_DESC desc;
    spTextureResource->GetDesc(&desc);

    D3D11_TEXTURE2D_DESC texDesc;
    ZeroMemory(&texDesc, sizeof(texDesc));
    texDesc.Width = desc.Width;
    texDesc.Height = desc.Height;
    texDesc.MipLevels = 1;
    texDesc.ArraySize = 1;
    texDesc.SampleDesc.Count = 1;
    texDesc.SampleDesc.Quality = 0;
    texDesc.Usage = D3D11_USAGE_STAGING;
    texDesc.Format = desc.Format;
    texDesc.BindFlags = 0;
    texDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    texDesc.MiscFlags = 0;

    ComPtr<ID3D11Texture2D> spD3D11Texture2D = NULL;
    hr = m_D3DDevice->CreateTexture2D(&texDesc, NULL, &spD3D11Texture2D);
    if (FAILED(hr))
        return hr;

    m_D3DDeviceContext->CopyResource(spD3D11Texture2D.Get(), spTextureResource.Get());

    ComPtr<IDXGISurface1> spDXGISurface; spD3D11Texture2D.As(&spDXGISurface);

    *pDXGISurface = spDXGISurface.Detach();

    // Updating mouse pointer, if visible
    if (fi.PointerPosition.Visible)
    {
        BYTE* pPointerShape = new BYTE[fi.PointerShapeBufferSize];

        DXGI_OUTDUPL_POINTER_SHAPE_INFO psi = {};
        UINT uiPointerShapeBufSize = fi.PointerShapeBufferSize;
        hr = m_DXGIOutputDuplication->GetFramePointerShape(uiPointerShapeBufSize, pPointerShape, &uiPointerShapeBufSize, &psi);
        if (hr == DXGI_ERROR_MORE_DATA)
        {
            pPointerShape = new BYTE[uiPointerShapeBufSize];

            hr = m_DXGIOutputDuplication->GetFramePointerShape(uiPointerShapeBufSize, pPointerShape, &uiPointerShapeBufSize, &psi);
        }

        if (hr == S_OK)
        {
            __L_INFO("PointerPosition Visible=%d x=%d y=%d w=%d h=%d type=%d\n", fi.PointerPosition.Visible, fi.PointerPosition.Position.x, fi.PointerPosition.Position.y, psi.Width, psi.Height, psi.Type);

            if ((psi.Type == DXGI_OUTDUPL_POINTER_SHAPE_TYPE_MONOCHROME ||
                psi.Type == DXGI_OUTDUPL_POINTER_SHAPE_TYPE_COLOR ||
                psi.Type == DXGI_OUTDUPL_POINTER_SHAPE_TYPE_MASKED_COLOR) &&
                psi.Width <= 128 && psi.Height <= 128)
            {
                // Here we can obtain pointer shape
                if (pDXGIPointer)
                {
                    delete pDXGIPointer;
                }

                pDXGIPointer = new DXGIPointerInfo(pPointerShape, uiPointerShapeBufSize, fi, psi);

                pPointerShape = NULL;
            }

            DXGI_OUTPUT_DESC outDesc;
            GetDesc(outDesc);

            if (pDXGIPointer)
            {
                pDXGIPointer->GetFrameInfo().PointerPosition.Position.x = outDesc.DesktopCoordinates.left + fi.PointerPosition.Position.x;
                pDXGIPointer->GetFrameInfo().PointerPosition.Position.y = outDesc.DesktopCoordinates.top + fi.PointerPosition.Position.y;
            }
        }

        if (pPointerShape)
        {
            delete[] pPointerShape;
        }
    }

    return hr;
}

HRESULT DXGIOutputDuplication::ReleaseFrame()
{
    m_DXGIOutputDuplication->ReleaseFrame();
    return S_OK;
}

bool DXGIOutputDuplication::IsPrimary()
{
    DXGI_OUTPUT_DESC outdesc;
    m_DXGIOutput1->GetDesc(&outdesc);

    MONITORINFO mi;
    mi.cbSize = sizeof(MONITORINFO);
    GetMonitorInfo(outdesc.Monitor, &mi);
    if (mi.dwFlags & MONITORINFOF_PRIMARY)
    {
        return true;
    }
    return false;
}

DXGIManager::DXGIManager()
{
    m_windowHandle = nullptr;
    SetRect(&m_rcCurrentOutput, 0, 0, 0, 0);
    m_pBuf = NULL;
    m_pDXGIPointer = NULL;
    m_bInitialized = false;
}

DXGIManager::~DXGIManager()
{
    GdiplusShutdown(m_gdiplusToken);

    if (m_pBuf)
    {
        delete[] m_pBuf;
        m_pBuf = NULL;
    }

    if (m_pDXGIPointer)
    {
        delete m_pDXGIPointer;
        m_pDXGIPointer = NULL;
    }
}

HRESULT DXGIManager::setCaptureWindow(HWND windowHandle)
{
    m_windowHandle = windowHandle;
    return S_OK;
}

HRESULT DXGIManager::Init()
{
    if (m_bInitialized)
        return S_OK;

    GdiplusStartupInput gdiplusStartupInput;
    GdiplusStartup(&m_gdiplusToken, &gdiplusStartupInput, NULL);

    HRESULT hr = CreateDXGIFactory1(__uuidof(IDXGIFactory1), (void**)(&m_spDXGIFactory1));
    if (FAILED(hr))
    {
        __L_ERROR("Failed to CreateDXGIFactory1 hr=%08x", hr);
        return hr;
    }

    // Getting all adapters
    vector<ComPtr<IDXGIAdapter1>> vAdapters;

    ComPtr<IDXGIAdapter1> spAdapter;
    for (int i = 0; m_spDXGIFactory1->EnumAdapters1(i, &spAdapter) != DXGI_ERROR_NOT_FOUND; i++)
    {
        vAdapters.push_back(spAdapter);
        spAdapter.Reset();
    }

    // Iterating over all adapters to get all outputs
    for (vector<ComPtr<IDXGIAdapter1>>::iterator AdapterIter = vAdapters.begin();
        AdapterIter != vAdapters.end();
        AdapterIter++)
    {
        vector<ComPtr<IDXGIOutput>> vOutputs;

        ComPtr<IDXGIOutput> spDXGIOutput;
        for (int i = 0; (*AdapterIter)->EnumOutputs(i, &spDXGIOutput) != DXGI_ERROR_NOT_FOUND; i++)
        {
            DXGI_OUTPUT_DESC outputDesc;
            spDXGIOutput->GetDesc(&outputDesc);

            __L_INFO("Display output found. DeviceName=%ls  AttachedToDesktop=%d Rotation=%d DesktopCoordinates={(%d,%d),(%d,%d)}",
                outputDesc.DeviceName,
                outputDesc.AttachedToDesktop,
                outputDesc.Rotation,
                outputDesc.DesktopCoordinates.left,
                outputDesc.DesktopCoordinates.top,
                outputDesc.DesktopCoordinates.right,
                outputDesc.DesktopCoordinates.bottom);

            if (outputDesc.AttachedToDesktop)
            {
                vOutputs.push_back(spDXGIOutput);
            }

            spDXGIOutput.Reset();
        }

        if (vOutputs.size() == 0)
            continue;

        // Creating device for each adapter that has the output
        ComPtr<ID3D11Device> spD3D11Device;
        ComPtr<ID3D11DeviceContext> spD3D11DeviceContext;
        D3D_FEATURE_LEVEL fl = D3D_FEATURE_LEVEL_9_1;
        hr = D3D11CreateDevice((*AdapterIter).Get(), D3D_DRIVER_TYPE_UNKNOWN, NULL, 0, NULL, 0, D3D11_SDK_VERSION, &spD3D11Device, &fl, &spD3D11DeviceContext);
        if (FAILED(hr))
        {
            __L_ERROR("Failed to create D3D11CreateDevice hr=%08x", hr);
            return hr;
        }

        for (std::vector<ComPtr<IDXGIOutput>>::iterator OutputIter = vOutputs.begin();
            OutputIter != vOutputs.end();
            OutputIter++)
        {
            ComPtr<IDXGIOutput1> spDXGIOutput1; (*OutputIter).As(&spDXGIOutput1);
            if (!spDXGIOutput1)
            {
                __L_ERROR("spDXGIOutput1 is NULL");
                continue;
            }

            ComPtr<IDXGIDevice1> spDXGIDevice; spD3D11Device.As(&spDXGIDevice);
            if (!spDXGIDevice)
            {
                __L_ERROR("spDXGIDevice is NULL");
                continue;
            }

            ComPtr<IDXGIOutputDuplication> spDXGIOutputDuplication;
            hr = spDXGIOutput1->DuplicateOutput(spDXGIDevice.Get(), &spDXGIOutputDuplication);
            if (FAILED(hr))
            {
                __L_ERROR("Failed to duplicate output hr=%08x", hr);
                continue;
            }

            m_vOutputs.push_back(
                DXGIOutputDuplication((*AdapterIter).Get(),
                    spD3D11Device.Get(),
                    spD3D11DeviceContext.Get(),
                    spDXGIOutput1.Get(),
                    spDXGIOutputDuplication.Get()));
        }
    }
    hr = CoCreateInstance(
        CLSID_WICImagingFactory,
        nullptr,
        CLSCTX_INPROC_SERVER,
        __uuidof(IWICImagingFactory),
        (void**)&m_spWICFactory);
    //hr = m_spWICFactory.CoCreateInstance(CLSID_WICImagingFactory);
    if (FAILED(hr))
    {
        __L_ERROR("Failed to create WICImagingFactory hr=%08x", hr);
        return hr;
    }

    m_bInitialized = true;

    return S_OK;
}

HRESULT DXGIManager::GetOutputRect(RECT& rc)
{
    // Nulling rc just in case...
    SetRect(&rc, 0, 0, 0, 0);

    HRESULT hr = Init();
    if (hr != S_OK)
        return hr;

    vector<DXGIOutputDuplication> vOutputs = GetOutputDuplication();
    
    RECT rcShare;
    SetRect(&rcShare, 0, 0, 0, 0);
    
    for (vector<DXGIOutputDuplication>::iterator iter = vOutputs.begin(); iter != vOutputs.end(); iter++) {
        DXGIOutputDuplication& out = *iter;
    
        DXGI_OUTPUT_DESC outDesc;
        out.GetDesc(outDesc);
        RECT rcOutCoords = outDesc.DesktopCoordinates;
    
        UnionRect(&rcShare, &rcShare, &rcOutCoords);
    }
    CopyRect(&rc, &rcShare);

    return S_OK;
}

HRESULT DXGIManager::GetOutputBits(BYTE* pBits, RECT& rcDest)
{
    HRESULT hr = Init();
    if (hr != S_OK) return hr;


    DWORD destWidth = rcDest.right - rcDest.left;
    DWORD destHeight = rcDest.bottom - rcDest.top;

    RECT rcCapture;
    hr = GetClientRect(m_windowHandle, &rcCapture);
    POINT p = *LPPOINT(&rcCapture);
    if (FAILED(hr))  return hr;
    ClientToScreen(m_windowHandle, &p);
    OffsetRect(&rcCapture, p.x, p.y);

    DWORD captureWidth = rcCapture.right - rcCapture.left;
    DWORD captureHeight = rcCapture.bottom - rcCapture.top;

    BYTE* pBuf = NULL;
    if (rcCapture.right > (LONG)destWidth || rcCapture.bottom > (LONG)destHeight)
    {
        // Output is larger than pBits dimensions
        if (!m_pBuf || !EqualRect(&m_rcCurrentOutput, &rcCapture))
        {
            DWORD dwBufSize = captureWidth * captureHeight * 4;

            if (m_pBuf)
            {
                delete[] m_pBuf;
                m_pBuf = NULL;
            }

            m_pBuf = new BYTE[dwBufSize];

            CopyRect(&m_rcCurrentOutput, &rcCapture);
        }

        pBuf = m_pBuf;
    }
    else
    {
        // Output is smaller than pBits dimensions
        pBuf = pBits;
        captureWidth = destWidth;
        captureHeight = destHeight;
    }

    vector<DXGIOutputDuplication> vOutputs = GetOutputDuplication();

    //Iterate over the video outputs containing at least a part of the window
    for (vector<DXGIOutputDuplication>::iterator iter = vOutputs.begin(); iter != vOutputs.end(); iter++) {
        DXGIOutputDuplication& out = *iter;

        DXGI_OUTPUT_DESC outDesc;
        out.GetDesc(outDesc);

        //Get the video frame
        ComPtr<IDXGISurface1> spDXGISurface1;
        hr = out.AcquireNextFrame(&spDXGISurface1, m_pDXGIPointer);
        if (FAILED(hr))
            break;

        DXGI_MAPPED_RECT map;
        spDXGISurface1->Map(&map, DXGI_MAP_READ);

        RECT rcFrame = outDesc.DesktopCoordinates;
        DWORD frameWidth = rcFrame.right - rcFrame.left;
        DWORD frameHeight = rcFrame.bottom - rcFrame.top;

        DWORD dwMapPitchPixels = map.Pitch / 4;

        // Consider the intersetcion of the window to capture and the frame
        RECT rcIntersection;
        IntersectRect(&rcIntersection, &rcCapture, &rcFrame);
        DWORD intersectionWidth = rcIntersection.right - rcIntersection.left;
        DWORD intersectionHeight = rcIntersection.bottom - rcIntersection.top;

        OffsetRect(&rcIntersection, -rcFrame.left, -rcFrame.top);

        switch (outDesc.Rotation) {
        case DXGI_MODE_ROTATION_IDENTITY: {
            // Just copying
            DWORD intersectionStripe = intersectionWidth * 4; // width of the band to copy
            DWORD destinationStripe = destWidth * 4;          // width of a line in the destination buffer
            DWORD destinationOffsetH = 4 * (rcIntersection.left - (rcCapture.left - rcFrame.left));
            DWORD destinationOffsetV = (rcIntersection.top - (rcCapture.top - rcFrame.top));
            for (unsigned int i = 0; i < intersectionHeight; i++) {
                memcpy_s(pBuf + destinationOffsetH + ((i+ destinationOffsetV) * destinationStripe ), intersectionStripe,
                         map.pBits + (rcIntersection.left + (i + rcIntersection.top) * frameWidth) * 4, intersectionStripe);
            }
        }
        break;
        case DXGI_MODE_ROTATION_ROTATE90:
        {
            // Rotating at 90 degrees
            DWORD* pSrc = (DWORD*)map.pBits;
            DWORD* pDst = (DWORD*)pBuf;
            for (unsigned int j = 0; j < frameHeight; j++)
            {
                for (unsigned int i = 0; i < frameWidth; i++)
                {
                    *(pDst + (rcFrame.left + (j + rcFrame.top) * captureWidth) + i) = *(pSrc + j + dwMapPitchPixels * (frameWidth - i - 1));
                }
            }
        }
        break;
        case DXGI_MODE_ROTATION_ROTATE180:
        {
            // Rotating at 180 degrees
            DWORD* pSrc = (DWORD*)map.pBits;
            DWORD* pDst = (DWORD*)pBuf;
            for (unsigned int j = 0; j < frameHeight; j++)
            {
                for (unsigned int i = 0; i < frameWidth; i++)
                {
                    *(pDst + (rcFrame.left + (j + rcFrame.top) * captureWidth) + i) = *(pSrc + (frameWidth - i - 1) + dwMapPitchPixels * (frameHeight - j - 1));
                }
            }
        }
        break;
        case DXGI_MODE_ROTATION_ROTATE270:
        {
            // Rotating at 270 degrees
            DWORD* pSrc = (DWORD*)map.pBits;
            DWORD* pDst = (DWORD*)pBuf;
            for (unsigned int j = 0; j < frameHeight; j++)
            {
                for (unsigned int i = 0; i < frameWidth; i++)
                {
                    *(pDst + (rcFrame.left + (j + rcFrame.top) * captureWidth) + i) = *(pSrc + (frameHeight - j - 1) + dwMapPitchPixels * i);
                }
            }
        }
        break;
        }

        spDXGISurface1->Unmap();

        out.ReleaseFrame();
    }

    if (FAILED(hr))
        return hr;

    // Now pBits have the desktop. Let's paint mouse pointer!
    if (pBuf != pBits)
    {
        DrawMousePointer(pBuf, rcCapture, rcCapture);
    }
    else
    {
        DrawMousePointer(pBuf, rcCapture, rcDest);
    }

    // We have the pBuf filled with current desktop/monitor image.
    if (pBuf != pBits)
    {
        // pBuf contains the image that should be resized
        ComPtr<IWICBitmap> spBitmap = NULL;
        hr = m_spWICFactory->CreateBitmapFromMemory(captureWidth, captureHeight, GUID_WICPixelFormat32bppBGRA, captureWidth * 4, captureWidth * captureHeight * 4, (BYTE*)pBuf, &spBitmap);
        if (FAILED(hr))
            return hr;

        ComPtr<IWICBitmapScaler> spBitmapScaler = NULL;
        hr = m_spWICFactory->CreateBitmapScaler(&spBitmapScaler);
        if (FAILED(hr))
            return hr;

        captureWidth = rcCapture.right - rcCapture.left;
        captureHeight = rcCapture.bottom - rcCapture.top;

        double aspect = (double)captureWidth / (double)captureHeight;

        DWORD scaledWidth = destWidth;
        DWORD scaledHeight = destHeight;

        if (aspect > 1)
        {
            scaledWidth = destWidth;
            scaledHeight = (DWORD)(destWidth / aspect);
        }
        else
        {
            scaledWidth = (DWORD)(aspect * destHeight);
            scaledHeight = destHeight;
        }

        spBitmapScaler->Initialize(
            spBitmap.Get(), scaledWidth, scaledHeight, WICBitmapInterpolationModeNearestNeighbor);

        spBitmapScaler->CopyPixels(NULL, scaledWidth * 4, destWidth * destHeight * 4, pBits);
    }
    return hr;
}

void DXGIManager::DrawMousePointer(BYTE* pDesktopBits, RECT rcDesktop, RECT rcDest)
{
    if (!m_pDXGIPointer)
        return;

    DWORD dwDesktopWidth = rcDesktop.right - rcDesktop.left;
    DWORD dwDesktopHeight = rcDesktop.bottom - rcDesktop.top;

    DWORD dwDestWidth = rcDest.right - rcDest.left;
    DWORD dwDestHeight = rcDest.bottom - rcDest.top;

    int PtrX = m_pDXGIPointer->GetFrameInfo().PointerPosition.Position.x - rcDesktop.left;
    int PtrY = m_pDXGIPointer->GetFrameInfo().PointerPosition.Position.y - rcDesktop.top;
    switch (m_pDXGIPointer->GetShapeInfo().Type)
    {
    case DXGI_OUTDUPL_POINTER_SHAPE_TYPE_COLOR:
    {
        unique_ptr<Bitmap> bmpBitmap(new Bitmap(dwDestWidth, dwDestHeight, dwDestWidth * 4, PixelFormat32bppARGB, pDesktopBits));
        unique_ptr<Graphics> graphics(Graphics::FromImage(bmpBitmap.get()));
        unique_ptr<Bitmap> bmpPointer(new Bitmap(m_pDXGIPointer->GetShapeInfo().Width, m_pDXGIPointer->GetShapeInfo().Height, m_pDXGIPointer->GetShapeInfo().Width * 4, PixelFormat32bppARGB, m_pDXGIPointer->GetBuffer()));

        graphics->DrawImage(bmpPointer.get(), PtrX, PtrY);
    }
    break;
    case DXGI_OUTDUPL_POINTER_SHAPE_TYPE_MONOCHROME:
    case DXGI_OUTDUPL_POINTER_SHAPE_TYPE_MASKED_COLOR:
    {
        RECT rcPointer;

        if (m_pDXGIPointer->GetShapeInfo().Type == DXGI_OUTDUPL_POINTER_SHAPE_TYPE_MONOCHROME)
        {
            SetRect(&rcPointer, PtrX, PtrY, PtrX + m_pDXGIPointer->GetShapeInfo().Width, PtrY + m_pDXGIPointer->GetShapeInfo().Height / 2);
        }
        else
        {
            SetRect(&rcPointer, PtrX, PtrY, PtrX + m_pDXGIPointer->GetShapeInfo().Width, PtrY + m_pDXGIPointer->GetShapeInfo().Height);
        }

        RECT rcDesktopPointer;
        IntersectRect(&rcDesktopPointer, &rcPointer, &rcDesktop);

        CopyRect(&rcPointer, &rcDesktopPointer);
        OffsetRect(&rcPointer, -PtrX, -PtrY);

        BYTE* pShapeBuffer = m_pDXGIPointer->GetBuffer();
        UINT* pDesktopBits32 = (UINT*)pDesktopBits;

        if (m_pDXGIPointer->GetShapeInfo().Type == DXGI_OUTDUPL_POINTER_SHAPE_TYPE_MONOCHROME)
        {
            for (int j = rcPointer.top, jDP = rcDesktopPointer.top;
                j < rcPointer.bottom && jDP < rcDesktopPointer.bottom;
                j++, jDP++)
            {
                for (int i = rcPointer.left, iDP = rcDesktopPointer.left;
                    i < rcPointer.right && iDP < rcDesktopPointer.right;
                    i++, iDP++)
                {
                    BYTE Mask = 0x80 >> (i % 8);
                    BYTE AndMask = pShapeBuffer[i / 8 + (m_pDXGIPointer->GetShapeInfo().Pitch) * j] & Mask;
                    BYTE XorMask = pShapeBuffer[i / 8 + (m_pDXGIPointer->GetShapeInfo().Pitch) * (j + m_pDXGIPointer->GetShapeInfo().Height / 2)] & Mask;

                    UINT AndMask32 = (AndMask) ? 0xFFFFFFFF : 0xFF000000;
                    UINT XorMask32 = (XorMask) ? 0x00FFFFFF : 0x00000000;

                    pDesktopBits32[jDP * dwDestWidth + iDP] = (pDesktopBits32[jDP * dwDestWidth + iDP] & AndMask32) ^ XorMask32;
                }
            }
        }
        else
        {
            UINT* pShapeBuffer32 = (UINT*)pShapeBuffer;
            for (int j = rcPointer.top, jDP = rcDesktopPointer.top;
                j < rcPointer.bottom && jDP < rcDesktopPointer.bottom;
                j++, jDP++)
            {
                for (int i = rcPointer.left, iDP = rcDesktopPointer.left;
                    i < rcPointer.right && iDP < rcDesktopPointer.right;
                    i++, iDP++)
                {
                    // Set up mask
                    UINT MaskVal = 0xFF000000 & pShapeBuffer32[i + (m_pDXGIPointer->GetShapeInfo().Pitch / 4) * j];
                    if (MaskVal)
                    {
                        // Mask was 0xFF
                        pDesktopBits32[jDP * dwDestWidth + iDP] = (pDesktopBits32[jDP * dwDestWidth + iDP] ^ pShapeBuffer32[i + (m_pDXGIPointer->GetShapeInfo().Pitch / 4) * j]) | 0xFF000000;
                    }
                    else
                    {
                        // Mask was 0x00 - replacing pixel
                        pDesktopBits32[jDP * dwDestWidth + iDP] = pShapeBuffer32[i + (m_pDXGIPointer->GetShapeInfo().Pitch / 4) * j];
                    }
                }
            }
        }
    }
    break;
    }
}

vector<DXGIOutputDuplication> DXGIManager::GetOutputDuplication()
{
    vector<DXGIOutputDuplication> outputs;

    //find the window dimensions
    RECT rcWindow;
    GetClientRect(m_windowHandle, &rcWindow);

    //Find the window position on the desktop
    POINT p = *LPPOINT(&rcWindow);
    ClientToScreen(m_windowHandle, &p);
    OffsetRect(&rcWindow, p.x, p.y);
    
    
    RECT intersection; // needed but the value assigned is will not be used
    
    // Return all outputs containing at least a part of the window to capture
    for (vector<DXGIOutputDuplication>::iterator iter = m_vOutputs.begin(); iter != m_vOutputs.end(); iter++) {
        DXGIOutputDuplication& out = *iter;

        DXGI_OUTPUT_DESC outDesc;
        out.GetDesc(outDesc);
        RECT rcOut = outDesc.DesktopCoordinates;
        if (IntersectRect(&intersection, &rcWindow, &rcOut)) {
            outputs.push_back(out);
        }
    }
    return outputs;
}

BOOL CALLBACK MonitorEnumProc(HMONITOR hMonitor, HDC hdcMonitor, LPRECT lprcMonitor, LPARAM dwData)
{
    int* Count = (int*)dwData;
    (*Count)++;
    return TRUE;
}

int DXGIManager::GetMonitorCount()
{
    int Count = 0;
    if (EnumDisplayMonitors(NULL, NULL, MonitorEnumProc, (LPARAM)&Count))
        return Count;
    return -1;
}
