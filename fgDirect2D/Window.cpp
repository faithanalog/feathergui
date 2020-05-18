// Copyright (c)2020 Fundament Software
// For conditions of distribution and use, see copyright notice in "fgDirect2D.h"

#include "Window.h"
#include "Backend.h"
#include "win32_includes.h"
#include <d2d1_1.h>
#include <dwmapi.h>
#include <Windowsx.h>
#include <wincodec.h>
#include "RoundRect.h"
#include "Circle.h"
#include "Triangle.h"
#include "Modulation.h"
#include "util.h"

namespace D2D {
  __KHASH_IMPL(wic, , const Asset*, ID2D1Bitmap*, 1, kh_ptr_hash_func, kh_int_hash_equal);
}

using namespace D2D;

EXTERN_C IMAGE_DOS_HEADER __ImageBase; // Accesses the HINSTANCE for this module

FG_FORCEINLINE FG_Vec GetPoints(longptr_t lParam)
{
  return FG_Vec{ static_cast<float>GET_X_LPARAM(lParam), static_cast<float>GET_Y_LPARAM(lParam) };
}
FG_FORCEINLINE FG_Vec AdjustPoints(longptr_t lParam, HWND__* hWnd)
{
  RECT rect;
  GetWindowRect(hWnd, &rect);
  auto v = GetPoints(lParam);
  v.x += rect.left;

  return FG_Vec{ v.x + static_cast<float>(rect.left), v.y + static_cast<float>(rect.top) };
}

Window::Window(Backend* _backend, FG_Element* _element, const FG_Rect& area, uint64_t flags, const char* caption)
{
  target = 0;
  context = 0;
  color = 0;
  edgecolor = 0;
  roundrect = 0;
  triangle = 0;
  circle = 0;
  scale = 0;
  element = _element;
  backend = _backend;
  margin = { 0 };
  assets = kh_init_wic();
  inside = false;
  invalid = false;
  unsigned long style = (flags & FG_Window_NOBORDER) ? WS_POPUP : 0;
  unsigned long exstyle = (flags & FG_Window_NOCAPTION) ? WS_EX_TOOLWINDOW : WS_EX_APPWINDOW;
  if(flags & FG_Window_MINIMIZABLE)
    style |= WS_MINIMIZEBOX | WS_SYSMENU;
  if(flags & FG_Window_RESIZABLE)
    style |= WS_THICKFRAME;
  if(flags & FG_Window_MAXIMIZABLE)
    style |= WS_MAXIMIZEBOX | WS_SYSMENU;

  hWnd = Window::WndCreate(area, style, exstyle, this, Backend::WindowClass, caption, dpi);

  RECT rect;
  GetWindowRect(hWnd, &rect);
  ShowWindow(hWnd, SW_SHOW);
  UpdateWindow(hWnd);
}
Window::~Window()
{
  DiscardResources();
}

void Window::WndRegister(WNDPROC f, const wchar_t* name)
{
  // Register window class
  WNDCLASSEX wcex = { sizeof(WNDCLASSEX) };
  wcex.style = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
  wcex.lpfnWndProc = f;
  wcex.cbClsExtra = 0;
  wcex.cbWndExtra = sizeof(LONG_PTR);
  wcex.hInstance = ((HINSTANCE)&__ImageBase);
  wcex.hbrBackground = NULL;
  wcex.lpszMenuName = NULL;
  wcex.hCursor = NULL;
  wcex.lpszClassName = name;

  RegisterClassExW(&wcex);
}

longptr_t __stdcall Window::WndProc(HWND__* hWnd, unsigned int message, size_t wParam, longptr_t lParam)
{
  static const int BORDERWIDTH = 5;
  static tagTRACKMOUSEEVENT _trackingstruct = { sizeof(tagTRACKMOUSEEVENT), TME_LEAVE, 0, 0 };
  Window* self = reinterpret_cast<Window*>(GetWindowLongPtrW(hWnd, GWLP_USERDATA));

  switch(message)
  {
  case WM_CREATE:
  {
    CREATESTRUCT* info = reinterpret_cast<CREATESTRUCT*>(lParam);
    SetWindowPos(hWnd, HWND_TOP, info->x, info->y, info->cx, info->cy, SWP_FRAMECHANGED);
    SetWindowLongPtrW(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(info->lpCreateParams));
  }
  break;
  case WM_NCCALCSIZE:
    if(wParam == TRUE && (GetWindowLongPtrW(hWnd, GWL_STYLE) & WS_POPUP) != 0)
      return 0;
    break;
  case WM_LBUTTONDOWN:
  case WM_RBUTTONDOWN:
  case WM_MBUTTONDOWN:
    SetCapture(hWnd);
    break;
  case WM_LBUTTONUP:
  case WM_RBUTTONUP:
  case WM_MBUTTONUP:
    ReleaseCapture();
    break;
  }

  if(self)
  {
    switch(message)
    {
    case WM_SIZE:
      if(self->target)
        self->target->Resize(D2D1::SizeU(LOWORD(lParam), HIWORD(lParam)));
    case WM_MOVE:
    {
      FG_Msg msg = { FG_Kind_SETWINDOWFLAGS };
      msg.setWindowFlags.flags = self->backend->Behavior(self, FG_Msg{ FG_Kind_GETWINDOWFLAGS }).getWindowFlags;

      switch(wParam)
      {
      case SIZE_MAXIMIZED:
        msg.setWindowFlags.flags |= FG_Window_MAXIMIZED;
        self->backend->Behavior(self, msg);
        break;
      case SIZE_MINIMIZED:
        msg.setWindowFlags.flags &= ~FG_Window_MINIMIZED;
        self->backend->Behavior(self, msg);
      case SIZE_RESTORED:
        msg.setWindowFlags.flags &= ~(FG_Window_MAXIMIZED & FG_Window_MINIMIZED);
        self->backend->Behavior(self, msg);
        break;
      }
      self->ApplyWin32Size(hWnd);
      break;
    }
    case WM_NCHITTEST: // TODO: Make this a custom message passed to the window behavior
    {
      auto flags = self->backend->Behavior(self, FG_Msg{ FG_Kind_GETWINDOWFLAGS }).getWindowFlags;
      if(flags & (FG_Window_RESIZABLE | FG_Window_MAXIMIZABLE))
      {
        RECT WindowRect;
        GetWindowRect(hWnd, &WindowRect);
        int x = GET_X_LPARAM(lParam) - WindowRect.left;
        int y = GET_Y_LPARAM(lParam) - WindowRect.top;

        if((flags & FG_Window_RESIZABLE) && !(flags & FG_Window_MAXIMIZED))
        {
          if(x < BORDERWIDTH && y < BORDERWIDTH)
            return HTTOPLEFT;
          if(x > WindowRect.right - WindowRect.left - BORDERWIDTH && y < BORDERWIDTH)
            return HTTOPRIGHT;
          if(x > WindowRect.right - WindowRect.left - BORDERWIDTH && y > WindowRect.bottom - WindowRect.top - BORDERWIDTH)
            return HTBOTTOMRIGHT;
          if(x < BORDERWIDTH && y > WindowRect.bottom - WindowRect.top - BORDERWIDTH)
            return HTBOTTOMLEFT;
          if(x < BORDERWIDTH)
            return HTLEFT;
          if(y < BORDERWIDTH)
            return HTTOP;
          if(x > WindowRect.right - WindowRect.left - BORDERWIDTH)
            return HTRIGHT;
          if(y > WindowRect.bottom - WindowRect.top - BORDERWIDTH)
            return HTBOTTOM;
        }
        //if(y < (self->window->padding.top * (fgSingleton()->dpi.y / 96.0f)) + self->window->margin.top)
        //  return HTCAPTION;
        return HTCLIENT;
      }
      break;
    }
    case WM_DESTROY:
    {
      FG_Msg msg = { FG_Kind_SETWINDOWFLAGS };
      msg.setWindowFlags.flags = self->backend->Behavior(self, FG_Msg{ FG_Kind_GETWINDOWFLAGS }).getWindowFlags | FG_Window_CLOSED;
      self->backend->Behavior(self, msg);
      self->backend->Behavior(self, FG_Msg{ FG_Kind_ACTION, 1 }); // FG_WINDOW_ONCLOSE
      delete self;
      return 1;
    }
    case WM_MOUSEWHEEL:
    {
      POINTS pointstemp = { 0, GET_WHEEL_DELTA_WPARAM(wParam) };
      DWORD pos = GetMessagePos();
      self->SetMouse(GetPoints(pos), FG_Kind_MOUSESCROLL, 0, *(size_t*)&pointstemp, GetMessageTime());
    }
    break;
    case WM_DPICHANGED:
      self->dpi = FG_Vec{ static_cast<float>(LOWORD(wParam)), static_cast<float>(HIWORD(wParam)) };
      self->backend->RefreshMonitors();
      return 0;
    case WM_MOUSEHWHEEL:
    {
      POINTS pointstemp = { GET_WHEEL_DELTA_WPARAM(wParam), 0 };
      DWORD pos = GetMessagePos();
      self->SetMouse(GetPoints(pos), FG_Kind_MOUSESCROLL, 0, *(size_t*)&pointstemp, GetMessageTime());
    }
    break;
    case WM_MOUSEMOVE:
    {
      FG_Vec pt = AdjustPoints(lParam, hWnd); // Call this up here so we don't do it twice
      if(!self->inside)
      {
        _trackingstruct.hwndTrack = hWnd;
        BOOL result = TrackMouseEvent(&_trackingstruct);
        self->inside = true;
        self->SetMouse(pt, FG_Kind_MOUSEON, 0, wParam, GetMessageTime());
      }
      self->SetMouse(pt, FG_Kind_MOUSEMOVE, 0, wParam, GetMessageTime());
      break;
    }
    case WM_LBUTTONDOWN:
      self->SetMouse(AdjustPoints(lParam, hWnd), FG_Kind_MOUSEDOWN, FG_MouseButton_L, wParam, GetMessageTime());
      break;
    case WM_LBUTTONUP:
      self->SetMouse(AdjustPoints(lParam, hWnd), FG_Kind_MOUSEUP, FG_MouseButton_L, wParam, GetMessageTime());
      break;
    case WM_LBUTTONDBLCLK:
      self->SetMouse(AdjustPoints(lParam, hWnd), FG_Kind_MOUSEDBLCLICK, FG_MouseButton_L, wParam, GetMessageTime());
      break;
    case WM_RBUTTONDOWN:
      self->SetMouse(AdjustPoints(lParam, hWnd), FG_Kind_MOUSEDOWN, FG_MouseButton_R, wParam, GetMessageTime());
      break;
    case WM_RBUTTONUP:
      self->SetMouse(AdjustPoints(lParam, hWnd), FG_Kind_MOUSEUP, FG_MouseButton_R, wParam, GetMessageTime());
      break;
    case WM_RBUTTONDBLCLK:
      self->SetMouse(AdjustPoints(lParam, hWnd), FG_Kind_MOUSEDBLCLICK, FG_MouseButton_R, wParam, GetMessageTime());
      break;
    case WM_MBUTTONDOWN:
      self->SetMouse(AdjustPoints(lParam, hWnd), FG_Kind_MOUSEDOWN, FG_MouseButton_M, wParam, GetMessageTime());
      break;
    case WM_MBUTTONUP:
      self->SetMouse(AdjustPoints(lParam, hWnd), FG_Kind_MOUSEUP, FG_MouseButton_M, wParam, GetMessageTime());
      break;
    case WM_MBUTTONDBLCLK:
      self->SetMouse(AdjustPoints(lParam, hWnd), FG_Kind_MOUSEDBLCLICK, FG_MouseButton_M, wParam, GetMessageTime());
      break;
    case WM_XBUTTONDOWN:
      self->SetMouse(AdjustPoints(lParam, hWnd), FG_Kind_MOUSEDOWN, (GET_XBUTTON_WPARAM(wParam) == XBUTTON1 ? FG_MouseButton_X1 : FG_MouseButton_X2), wParam, GetMessageTime());
      break;
    case WM_XBUTTONUP:
      self->SetMouse(AdjustPoints(lParam, hWnd), FG_Kind_MOUSEUP, (GET_XBUTTON_WPARAM(wParam) == XBUTTON1 ? FG_MouseButton_X1 : FG_MouseButton_X2), wParam, GetMessageTime());
      break;
    case WM_XBUTTONDBLCLK:
      self->SetMouse(AdjustPoints(lParam, hWnd), FG_Kind_MOUSEDBLCLICK, (GET_XBUTTON_WPARAM(wParam) == XBUTTON1 ? FG_MouseButton_X1 : FG_MouseButton_X2), wParam, GetMessageTime());
      break;
    case WM_SYSKEYUP:
    case WM_SYSKEYDOWN:
      self->SetKey(static_cast<uint8_t>(wParam), message == WM_SYSKEYDOWN, (lParam & 0x40000000) != 0, GetMessageTime());
      break;
    case WM_KEYUP:
    case WM_KEYDOWN: // Windows return codes are the opposite of feathergui's - returning 0 means we accept, anything else rejects, so we invert the return code here.
      return !self->SetKey(static_cast<uint8_t>(wParam), message == WM_KEYDOWN, (lParam & 0x40000000) != 0, GetMessageTime());
    case WM_UNICHAR:
      if(wParam == UNICODE_NOCHAR) return TRUE;
    case WM_CHAR:
      self->SetChar(static_cast<int>(wParam), GetMessageTime());
      return 0;
    case WM_MOUSELEAVE:
    {
      DWORD pos = GetMessagePos();
      self->SetMouse(GetPoints(pos), FG_Kind_MOUSEOFF, 0, (size_t)~0, GetMessageTime());
      self->inside = false;
      break;
    }
    case WM_PAINT:
    {
      FG_Msg msg = { FG_Kind_DRAW };
      msg.draw.data = self;
      RECT WindowRect;
      GetWindowRect(hWnd, &WindowRect);
      msg.draw.area.left = 0;
      msg.draw.area.top = 0;
      msg.draw.area.right = static_cast<float>(WindowRect.right - WindowRect.left);
      msg.draw.area.bottom = static_cast<float>(WindowRect.bottom - WindowRect.top);
      self->backend->BeginDraw(self->backend, hWnd, &msg.draw.area);
      self->backend->Behavior(self, msg);
      self->backend->EndDraw(self->backend, hWnd);
      ValidateRect(self->hWnd, NULL);
      return 0;
    }
    case WM_DISPLAYCHANGE:
      self->backend->RefreshMonitors();
      InvalidateRect(hWnd, NULL, FALSE);
      return 0;
    }
  }
  return DefWindowProc(hWnd, message, wParam, lParam);
}

HWND__* Window::WndCreate(const FG_Rect& out, unsigned long style, uint32_t exflags, void* self, const wchar_t* cls, const char* caption, FG_Vec& dpi)
{
  exflags |= WS_EX_COMPOSITED | WS_EX_LAYERED;

  wchar_t* wcaption = L"";
  if(caption)
  {
    size_t len = UTF8toUTF16(caption, -1, 0, 0);
    if(len > 4096)
      return 0;
    wcaption = (wchar_t*)ALLOCA(sizeof(wchar_t) * len);
    UTF8toUTF16(caption, -1, wcaption, len);
  }

  RECT rsize = { (LONG)floor(out.left), (LONG)floor(out.top), (LONG)ceil(out.right), (LONG)ceil(out.bottom) };

  //AdjustWindowRectEx(&rsize, style, FALSE, exflags); // So long as we are drawing all over the nonclient area, we don't actually want to correct this
  int rwidth = rsize.right - rsize.left;
  int rheight = rsize.bottom - rsize.top;

  HWND handle = CreateWindowExW(exflags, cls, wcaption, style, (style & WS_POPUP) ? rsize.left : CW_USEDEFAULT, (style & WS_POPUP) ? rsize.top : CW_USEDEFAULT, INT(rwidth), INT(rheight), NULL, NULL, (HINSTANCE)&__ImageBase, self);
  HDC hdc = GetDC(handle);
  dpi = { static_cast<float>(GetDeviceCaps(hdc, LOGPIXELSX)), static_cast<float>(GetDeviceCaps(hdc, LOGPIXELSY)) };
  ReleaseDC(handle, hdc);


  if(backend->dwmblurbehind != 0)
  {
    //MARGINS margins = { -1,-1,-1,-1 };
    //(*dwmextend)(handle, &margins); //extends glass effect
    HRGN region = CreateRectRgn(-1, -1, 0, 0);
    DWM_BLURBEHIND blurbehind = { DWM_BB_ENABLE | DWM_BB_BLURREGION | DWM_BB_TRANSITIONONMAXIMIZED, TRUE, region, FALSE };
    (*backend->dwmblurbehind)(handle, &blurbehind);
    DeleteObject(region);
  }

  return handle;
}

FG_Err Window::PushClip(FG_Rect area)
{
  target->PushAxisAlignedClip(D2D1::RectF(area.left, area.top, area.right, area.bottom), D2D1_ANTIALIAS_MODE_ALIASED);
  return 0;
}
FG_Err Window::PopClip()
{
  target->PopAxisAlignedClip();
  return 0;
}

void Window::BeginDraw(HWND handle, const FG_Rect& area)
{
  invalid = true;
  CreateResources(handle);
  target->BeginDraw();
  target->Clear(D2D1::ColorF(0, 0));
  //target->SetTransform(D2D1::Matrix3x2F::Translation(-area.left, -area.top));
  PushClip(area);
}

void Window::EndDraw()
{
  PopClip();
  if(target->EndDraw() == D2DERR_RECREATE_TARGET)
    DiscardResources();
  invalid = false;
}

void Window::CreateResources(HWND handle)
{
  if(!target)
  {
    RECT rc;
    GetClientRect(handle, &rc);
    HRESULT hr = backend->CreateHWNDTarget(
      D2D1::RenderTargetProperties(D2D1_RENDER_TARGET_TYPE_DEFAULT, D2D1::PixelFormat(DXGI_FORMAT_UNKNOWN, D2D1_ALPHA_MODE_PREMULTIPLIED)),
      D2D1::HwndRenderTargetProperties(handle, D2D1::SizeU(rc.right - rc.left, rc.bottom - rc.top)),
      &target);

    if(SUCCEEDED(hr))
    {
      hr = target->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::White), &color);
      hr = target->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::White), &edgecolor);
      if(FAILED(hr))
        (*backend->_log)(backend->_root, FG_Level_ERROR, "CreateSolidColorBrush failed with error code %li", hr);
      hr = target->QueryInterface<ID2D1DeviceContext>(&context);
      if(SUCCEEDED(hr))
      {
        context->SetUnitMode(D2D1_UNIT_MODE_PIXELS); // Force direct2D to render in pixels because feathergui does the DPI conversion for us.
        hr = context->CreateEffect(CLSID_RoundRect, &roundrect);
        hr = context->CreateEffect(CLSID_Circle, &circle);
        hr = context->CreateEffect(CLSID_Triangle, &triangle);
        hr = context->CreateEffect(CLSID_D2D1Scale, &scale);
        if(FAILED(hr))
          (*backend->_log)(backend->_root, FG_Level_ERROR, "CreateEffect failed with error code %li", hr);
      }
      else
        (*backend->_log)(backend->_root, FG_Level_ERROR, "QueryInterface<ID2D1DeviceContext> failed with error code %li, custom effects won't be available. Make sure your device supports Direct2D 1.1", hr);
    }
    else
      (*backend->_log)(backend->_root, FG_Level_ERROR, "CreateHwndRenderTarget failed with error code %li", hr);
  }
}
void Window::DiscardResources()
{
  for(khint_t iter = kh_begin(assets); iter != kh_end(assets); ++iter)
  {
    if(kh_exist(assets, iter))
    {
      auto& a = kh_key(assets, iter)->instances;
      for(size_t i = 0; i < a.Length(); ++i)
        if(a[i] == this)
        {
          a.Remove(i);
          break;
        }

      kh_val(assets, iter)->Release();
    }
  }

  if(color)
    color->Release();
  if(edgecolor)
    edgecolor->Release();
  if(roundrect)
    roundrect->Release();
  if(triangle)
    triangle->Release();
  if(circle)
    circle->Release();
  if(scale)
    scale->Release();
  if(context)
    context->Release();
  if(target)
    target->Release();
  color = 0;
  edgecolor = 0;
  context = 0;
  roundrect = 0;
  triangle = 0;
  circle = 0;
  target = 0;
}
void Window::SetCaption(const char* caption)
{
  if(caption)
  {
    size_t len = UTF8toUTF16(caption, -1, 0, 0);
    if(len < 4096)
    {
      auto wcaption = reinterpret_cast<wchar_t*>(ALLOCA(sizeof(wchar_t) * len));
      UTF8toUTF16(caption, -1, wcaption, len);
      SetWindowTextW(hWnd, wcaption);
    }
  }
}
void Window::SetArea(const FG_Rect& r)
{
  SetWindowPos(hWnd, HWND_TOP, static_cast<int>(floorf(r.left)), static_cast<int>(floorf(r.top)), static_cast<int>(ceilf(r.right - r.left)), static_cast<int>(ceilf(r.bottom - r.top)), SWP_NOSENDCHANGING);
}
void Window::SetFlags(uint64_t flags)
{
  if((flags & FG_Window_MINIMIZED) && !IsIconic(hWnd))
    ShowWindow(hWnd, SW_MINIMIZE);
  if((flags & FG_Window_MAXIMIZED) && !IsZoomed(hWnd))
    ShowWindow(hWnd, SW_MAXIMIZE);
  if(!(flags & (FG_Window_MINIMIZED | FG_Window_MAXIMIZED)) && (IsIconic(hWnd) || IsZoomed(hWnd)))
    ShowWindow(hWnd, SW_RESTORE);

  if((flags & FG_Window_CLOSED) && IsWindowVisible(hWnd))
    ShowWindow(hWnd, SW_HIDE);
  if(!(flags & FG_Window_CLOSED) && !IsWindowVisible(hWnd))
    ShowWindow(hWnd, SW_SHOW);

  //LONG curstyle = GetWindowLongA(hWnd, GWL_STYLE);
  //LONG check = curstyle & 
}

size_t Window::SetKey(uint8_t keycode, bool down, bool held, unsigned long time)
{
  FG_Msg evt = { down ? FG_Kind_KEYDOWN : FG_Kind_KEYUP };
  evt.keyUp.code = keycode;
  evt.keyUp.sigkeys = 0;

  // GetKeyState only reflects keyboard state up to this current keyboard event, which is what we want
  if(GetKeyState(VK_SHIFT)) evt.keyUp.sigkeys = evt.keyUp.sigkeys | 1;
  if(GetKeyState(VK_CONTROL)) evt.keyUp.sigkeys = evt.keyUp.sigkeys | 2;
  if(GetKeyState(VK_MENU)) evt.keyUp.sigkeys = evt.keyUp.sigkeys | 4;
  if(held) evt.keyUp.sigkeys = evt.keyUp.sigkeys | 8;

  return backend->Behavior(this, evt).keyUp;
}

size_t Window::SetChar(int key, unsigned long time)
{
  FG_Msg evt = { FG_Kind_KEYCHAR };
  evt.keyChar.unicode = key;
  evt.keyChar.sigkeys = 0;

  // GetKeyState only reflects keyboard state up to this current keyboard event, which is what we want
  if(GetKeyState(VK_SHIFT)) evt.keyChar.sigkeys = evt.keyChar.sigkeys | 1;
  if(GetKeyState(VK_CONTROL)) evt.keyChar.sigkeys = evt.keyChar.sigkeys | 2;
  if(GetKeyState(VK_MENU)) evt.keyChar.sigkeys = evt.keyChar.sigkeys | 4;
  // if(held) evt.keyChar.sigkeys = evt.keyChar.sigkeys | 8;

  return backend->Behavior(this, evt).keyChar;
}

void Window::ApplyWin32Size(HWND__* handle)
{
  RECT r;

  if(SUCCEEDED(GetWindowRect(handle, &r)))
  {
    auto flags = backend->Behavior(this, FG_Msg{ FG_Kind_GETWINDOWFLAGS }).getWindowFlags;
    if(flags & FG_Window_MAXIMIZED)
    {
      RECT rsize = r;
      AdjustWindowRectEx(&rsize, GetWindowLong(handle, GWL_STYLE), FALSE, GetWindowLong(handle, GWL_EXSTYLE));
      margin = { static_cast<float>(rsize.right - r.right), static_cast<float>(rsize.bottom - r.bottom), static_cast<float>(rsize.right - r.right), static_cast<float>(rsize.bottom - r.bottom) };
    }
    else
      margin = { 0,0,0,0 };

    FG_Msg msg = { FG_Kind_SETWINDOWRECT };
    msg.setWindowRect.rect = { static_cast<float>(r.left), static_cast<float>(r.top), static_cast<float>(r.right), static_cast<float>(r.bottom) };
    backend->Behavior(this, msg);
  }
}

size_t Window::SetMouse(FG_Vec& points, FG_Kind type, unsigned char button, size_t wparam, unsigned long time)
{
  FG_Msg evt = { type };
  evt.mouseMove.x = points.x;
  evt.mouseMove.y = points.y;

  if(type == FG_Kind_MOUSESCROLL)
  {
    POINTS delta = MAKEPOINTS(wparam);
    evt.mouseScroll.delta = delta.y;
    evt.mouseScroll.hdelta = delta.x;
  }
  else
  {
    if(wparam != (size_t)-1)
    {
      evt.mouseMove.all = 0; // we must keep these bools accurate at all times
      evt.mouseMove.all |= FG_MouseButton_L & (-((wparam & MK_LBUTTON) != 0));
      evt.mouseMove.all |= FG_MouseButton_R & (-((wparam & MK_RBUTTON) != 0));
      evt.mouseMove.all |= FG_MouseButton_M & (-((wparam & MK_MBUTTON) != 0));
      evt.mouseMove.all |= FG_MouseButton_X1 & (-((wparam & MK_XBUTTON1) != 0));
      evt.mouseMove.all |= FG_MouseButton_X2 & (-((wparam & MK_XBUTTON2) != 0));
    }
    else // if wparam is -1 it signals that it is invalid, so we simply leave our assignments at their last known value.
    {
      evt.mouseMove.all |= ((GetKeyState(VK_LBUTTON) != 0) << 0);
      evt.mouseMove.all |= ((GetKeyState(VK_RBUTTON) != 0) << 1);
      evt.mouseMove.all |= ((GetKeyState(VK_MBUTTON) != 0) << 4);
      evt.mouseMove.all |= ((GetKeyState(VK_XBUTTON1) != 0) << 5);
      evt.mouseMove.all |= ((GetKeyState(VK_XBUTTON2) != 0) << 6);
    }
    evt.mouseMove.button = button;
  }

  switch(type)
  {
  case FG_Kind_MOUSEDBLCLICK: //L down
  case FG_Kind_MOUSEDOWN: //L down
    evt.mouseMove.all |= evt.mouseMove.button; // Ensure the correct button position is reflected in the allbtn parameter regardless of the current state of the mouse
    break;
  case FG_Kind_MOUSEUP: //L up
    evt.mouseMove.all &= ~evt.mouseMove.button;
    break;
  }

  return backend->Behavior(this, evt).mouseMove;
}

void Window::InvalidateHWND(HWND__* hWnd)
{
  if(!invalid)
  {
    InvalidateRect(hWnd, NULL, FALSE);
    invalid = true;
  }
}
ID2D1Bitmap* Window::GetBitmapFromSource(const Asset* p)
{
  ID2D1Bitmap* b = NULL;
  int r;
  khiter_t i = kh_put_wic(assets, p, &r);
  if(!r)
    b = kh_val(assets, i);
  else if(r == -1)
    return 0;
  else
  {
    IWICBitmapSource* tex;
    reinterpret_cast<IUnknown*>(p->data.data)->QueryInterface<IWICBitmapSource>(&tex);

    if(!tex)
      return 0;

    if(FAILED(target->CreateBitmapFromWicBitmap(tex, nullptr, &b)))
    {
      tex->Release();
      return 0;
    }

    p->instances.Add(this);
    tex->Release();
    kh_val(assets, i) = b;
  }
  b->AddRef();
  return b;
}
void Window::DiscardAsset(const Asset* p)
{
  auto iter = kh_get_wic(assets, p);
  if(iter < kh_end(assets) && kh_exist(assets, iter))
  {
    kh_val(assets, iter)->Release();
    kh_del_wic(assets, iter);
  }
}
