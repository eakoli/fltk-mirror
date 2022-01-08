#include "GdiInterop.h"

#include <new>
#include <stdlib.h>
#include <malloc.h>
#include <memory.h>

#include <dwrite.h>
#include <d2d1.h>
#include <d2d1helper.h>

// SafeRelease inline function.
template <class T> inline void SafeRelease(T **ppT)
{
    if (*ppT)
    {
        (*ppT)->Release();
        *ppT = NULL;
    }
}

class GdiInterop {
public:
    GdiInterop();
    ~GdiInterop();

    void draw(HDC gdiHdc, wchar_t *text, int wn, int x, int y);
    double text_width(HDC hdc, wchar_t *text, int wn);
    double text_height(HDC hdc, wchar_t *text, int wn);
private:
    HRESULT createResources(wchar_t *text, int wn, HFONT hfont);

    void calculateSize(HDC gdiHdc);

    long r_width;
    long r_height;

    IDWriteFactory* g_pDWriteFactory;
    ID2D1Factory* m_pDirect2dFactory;
    IDWriteGdiInterop* g_pGdiInterop;
    ID2D1DCRenderTarget *renderTarget;

    IDWriteTextFormat *createTextFormat(HDC gdiHdc, HFONT hfont, LOGFONTW *lf);
    IDWriteTextLayout *createTextLayout(HDC gdiHdc, wchar_t *text, UINT32 textLength, HFONT hfont);

};

namespace DwriteDll {
    bool validDll=false;
    HMODULE hModDwrite=NULL;
    typedef HRESULT (*DWriteCreateFactory_t) (
        DWRITE_FACTORY_TYPE factoryType,
        REFIID              iid,
        IUnknown            **factory
    );
    DWriteCreateFactory_t DWriteCreateFactory = NULL;

    HMODULE hModD2d1=NULL;
    typedef HRESULT (*D2D1CreateFactory_t)(
      D2D1_FACTORY_TYPE          factoryType,
      REFIID                     riid,
      const D2D1_FACTORY_OPTIONS *pFactoryOptions,
      void                       **ppIFactory
    );
    D2D1CreateFactory_t D2D1CreateFactory = NULL;
}

namespace DirectWriteGdiInterop {
    void init() {
        static bool inited=false;
        if (inited)
            return;
        inited = true;
        
        DwriteDll::hModDwrite = LoadLibrary("Dwrite.dll");
        if (!DwriteDll::hModDwrite) {
            return;
        }
        DwriteDll::DWriteCreateFactory = (DwriteDll::DWriteCreateFactory_t)GetProcAddress(DwriteDll::hModDwrite, "DWriteCreateFactory");
        if (!DwriteDll::DWriteCreateFactory) {
            return;
        }

        DwriteDll::hModD2d1 = LoadLibrary("D2d1.dll");
        if (!DwriteDll::hModD2d1) {
            return;
        }
        DwriteDll::D2D1CreateFactory = (DwriteDll::D2D1CreateFactory_t)GetProcAddress(DwriteDll::hModD2d1, "D2D1CreateFactory");
        if (!DwriteDll::D2D1CreateFactory) {
            return;
        }

        DwriteDll::validDll=true;
    }
    
    bool hasDirectWrite() {
        return DwriteDll::validDll;
    }

    GdiInterop& drawer() {
        static GdiInterop drawer;
        return drawer;
    }

    void draw_text(HDC hdc, int x, int y, wchar_t *text, int wn) {
        drawer().draw(hdc, text, wn, x, y);
    }
}

GdiInterop::GdiInterop():
    r_width(0),
    r_height(0),
    g_pDWriteFactory(NULL),
    m_pDirect2dFactory(NULL),
    g_pGdiInterop(NULL),
    renderTarget(NULL)
{
    HRESULT hr = S_OK;

    hr = DwriteDll::D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED,
                                      __uuidof(ID2D1Factory),
                                      NULL,
                                      reinterpret_cast<void**>(&m_pDirect2dFactory));
    if (SUCCEEDED(hr))
    {
        hr = DwriteDll::DWriteCreateFactory(
            DWRITE_FACTORY_TYPE_SHARED,
            __uuidof(IDWriteFactory),
            reinterpret_cast<IUnknown**>(&g_pDWriteFactory)
            );
    }

    if (SUCCEEDED(hr))
    {
        hr = g_pDWriteFactory->GetGdiInterop(&g_pGdiInterop);
    }

    if (SUCCEEDED(hr))
    {
        D2D1_RENDER_TARGET_PROPERTIES properties=D2D1::RenderTargetProperties();
        properties.pixelFormat.format=DXGI_FORMAT_B8G8R8A8_UNORM;
        properties.pixelFormat.alphaMode=D2D1_ALPHA_MODE_PREMULTIPLIED;

        hr = m_pDirect2dFactory->CreateDCRenderTarget(&properties, &renderTarget);
    }

    if (SUCCEEDED(hr))
    {
        renderTarget->SetTextAntialiasMode(D2D1_TEXT_ANTIALIAS_MODE_CLEARTYPE);
    }
}

GdiInterop::~GdiInterop()
{
    SafeRelease(&renderTarget);
    SafeRelease(&g_pGdiInterop);
    SafeRelease(&m_pDirect2dFactory);
    SafeRelease(&g_pDWriteFactory);
}

void GdiInterop::draw(HDC gdiHdc, wchar_t *text, int wn, int x, int y) {
    calculateSize(gdiHdc);

    HRESULT hr = S_OK;
    HFONT hfont = (HFONT)GetCurrentObject(gdiHdc, OBJ_FONT);

    IDWriteTextLayout* g_pTextLayout = createTextLayout(gdiHdc, text, wn, hfont);

    RECT rc;
    rc.top=0;
    rc.left=0;
    rc.bottom=r_height;
    rc.right=r_width;
    D2D1_POINT_2F origin;
    origin.x=x;
    origin.y=y;
    
    hr = renderTarget->BindDC(gdiHdc, &rc);

    // create solid color brush, use pen color if rect is completely filled with outline
    // TODO: Change font color
    ::D2D1_COLOR_F color;
    color.r = 0.0f;
    color.g = 0.0f;
    color.b = 0.0f;
    color.a = 1.0f;

    ::ID2D1SolidColorBrush* pBrush = NULL;
    renderTarget->CreateSolidColorBrush(color, &pBrush);

    renderTarget->BeginDraw();
    renderTarget->DrawTextLayout(
                origin,
                g_pTextLayout,
                pBrush,
                (D2D1_DRAW_TEXT_OPTIONS)4 // 4 = D2D1_DRAW_TEXT_OPTIONS_ENABLE_COLOR_FONT, but MinGW does not have the enum value.
                );
    renderTarget->EndDraw();

    SafeRelease(&g_pTextLayout);
    SafeRelease(&pBrush);
}

void GdiInterop::calculateSize(HDC gdiHdc)
{
    RECT wr;
    HWND hwnd = WindowFromDC(gdiHdc);
    if (hwnd) {
        GetWindowRect(hwnd, &wr);

        r_width = wr.right-wr.left+1;
        r_height = wr.bottom-wr.top+1;
    } else {
        BITMAP structBitmapHeader;
        memset( &structBitmapHeader, 0, sizeof(BITMAP) );

        HGDIOBJ hBitmap = GetCurrentObject(gdiHdc, OBJ_BITMAP);
        if (hBitmap) {
            GetObject(hBitmap, sizeof(BITMAP), &structBitmapHeader);

            r_width=structBitmapHeader.bmWidth;
            r_height=structBitmapHeader.bmHeight;
        } else {
            r_width = GetDeviceCaps(gdiHdc, HORZRES);
            r_height = GetDeviceCaps(gdiHdc, VERTRES);
        }
    }
}

IDWriteTextFormat *GdiInterop::createTextFormat(HDC gdiHdc, HFONT hfont, LOGFONTW *lf)
{
    HRESULT hr = S_OK;

    IDWriteFont* pFont = NULL;
    IDWriteFontFamily* pFontFamily = NULL;
    IDWriteLocalizedStrings* pFamilyNames = NULL;

    // Logical (GDI) font.
    if (SUCCEEDED(hr))
    {
        // Get a logical font from the font handle.
        GetObjectW(hfont, sizeof(LOGFONTW), lf);
    }

    // Convert to a DirectWrite font.
    if (SUCCEEDED(hr))
    {
        hr = g_pGdiInterop->CreateFontFromLOGFONT(lf, &pFont);
    }

    // Get the font family.
    if (SUCCEEDED(hr))
    {
        hr = pFont->GetFontFamily(&pFontFamily);
    }

    // Get a list of localized family names.
    if (SUCCEEDED(hr))
    {
        hr = pFontFamily->GetFamilyNames(&pFamilyNames);
    }

    // Select the first locale.  This is OK, because we are not displaying the family name.
    UINT32 index = 0;
    UINT32 length = 0;
    float fontSize = 0;

    // Get the length of the family name.
    if (SUCCEEDED(hr))
    {
        hr = pFamilyNames->GetStringLength(index, &length);
    }

    wchar_t *name = NULL;

    if (SUCCEEDED(hr))
    {
        // Allocate a new string.
        name = new (std::nothrow) wchar_t[length+1];
        if (name == NULL)
        {
            hr = E_OUTOFMEMORY;
        }
    }

    // Get the actual family name.
    if (SUCCEEDED(hr))
    {
        hr = pFamilyNames->GetString(index, name, length+1);
    }

    if (SUCCEEDED(hr))
    {
        // Calculate the font size.
        fontSize = (float) -MulDiv(lf->lfHeight, 96, GetDeviceCaps(gdiHdc, LOGPIXELSY));
    }

    IDWriteTextFormat* g_pTextFormat=NULL;

    // Create a text format using the converted font information.
    if (SUCCEEDED(hr))
    {
        hr = g_pDWriteFactory->CreateTextFormat(
            name,                // Font family name.
            NULL,
            pFont->GetWeight(),
            pFont->GetStyle(),
            pFont->GetStretch(),
            fontSize,
            L"en-us",
            &g_pTextFormat
            );
    }

    delete [] name;
    SafeRelease(&pFontFamily);
    SafeRelease(&pFont);
    SafeRelease(&pFamilyNames);

    return g_pTextFormat;
}

IDWriteTextLayout *GdiInterop::createTextLayout(HDC gdiHdc, wchar_t *text, UINT32 textLength, HFONT hfont)
{
    HRESULT hr = S_OK;
    LOGFONTW lf = {};

    IDWriteTextFormat* g_pTextFormat=createTextFormat(gdiHdc, hfont, &lf);
    if (!g_pTextFormat) {
        hr = E_POINTER;
    }

    IDWriteTextLayout* g_pTextLayout = NULL;

    // Create a text layout.
    if (SUCCEEDED(hr))
    {
        //Set horiz and vert alignment.
        g_pTextFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
        g_pTextFormat->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_NEAR);

        hr = g_pDWriteFactory->CreateTextLayout(
            text,
            textLength,
            g_pTextFormat,
            r_width,
            r_height,
            &g_pTextLayout
            );
    }

    if (SUCCEEDED(hr))
    {
        // Underline and strikethrough are part of a LOGFONT structure, but are not
        // part of a DWrite font object so we must set them using the text layout.
        if(lf.lfUnderline)
        {
            DWRITE_TEXT_RANGE textRange = {0, textLength};
            g_pTextLayout->SetUnderline(true, textRange);
        }

        if(lf.lfStrikeOut)
        {
            DWRITE_TEXT_RANGE textRange = {0, textLength};
            g_pTextLayout->SetStrikethrough(true, textRange);
        }
    }

    SafeRelease(&g_pTextFormat);

    return g_pTextLayout;
}
