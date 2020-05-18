// Copyright (c)2020 Fundament Software
// For conditions of distribution and use, see copyright notice in "fgDirect2D.h"

#include "util.h"
#include <initguid.h>
#include "EffectBase.h"
#include <utility>
#include <memory>

//int __cdecl TestHook1(int nReportType, char* szMsg, int* pnRet) { return -1; }

using namespace D2D;

EffectBase::EffectBase() : _ref(1), _drawInfo(0)
{ 
  fgassert(sizeof(_constants) == sizeof(float)*(4*4 + 2));
  memset(&_constants, 0, sizeof(_constants));
}
EffectBase::~EffectBase()
{
  if(_drawInfo)
    _drawInfo->Release();
}
IFACEMETHODIMP EffectBase::PrepareForRender(D2D1_CHANGE_TYPE changeType)
{
  return UpdateConstants();
}
IFACEMETHODIMP EffectBase::SetGraph(_In_ ID2D1TransformGraph* pGraph) { return E_NOTIMPL; }
IFACEMETHODIMP_(ULONG) EffectBase::AddRef() { return ++_ref; }
IFACEMETHODIMP_(ULONG) EffectBase::Release()
{
  if(--_ref > 0)
    return _ref;
  delete this;
  return 0;
}
IFACEMETHODIMP EffectBase::QueryInterface(_In_ REFIID riid, _Outptr_ void** ppOutput)
{
  *ppOutput = nullptr;
  HRESULT hr = S_OK;

  if(riid == __uuidof(ID2D1EffectImpl))
  {
    *ppOutput = reinterpret_cast<ID2D1EffectImpl*>(this);
  }
  else if(riid == __uuidof(ID2D1DrawTransform))
  {
    *ppOutput = static_cast<ID2D1DrawTransform*>(this);
  }
  else if(riid == __uuidof(ID2D1Transform))
  {
    *ppOutput = static_cast<ID2D1Transform*>(this);
  }
  else if(riid == __uuidof(ID2D1TransformNode))
  {
    *ppOutput = static_cast<ID2D1TransformNode*>(this);
  }
  else if(riid == __uuidof(IUnknown))
  {
    *ppOutput = this;
  }
  else
  {
    hr = E_NOINTERFACE;
  }

  if(*ppOutput != nullptr)
  {
    AddRef();
  }

  return hr;
}

HRESULT EffectBase::SetRect(D2D_VECTOR_4F rect)
{
  _rect = D2D1::RectL((INT32)floor(rect.x), (INT32)floor(rect.y), (INT32)ceil(rect.z), (INT32)ceil(rect.w));
  _constants.rect = rect;
  return S_OK;
}
D2D_VECTOR_4F EffectBase::GetRect() const { return _constants.rect; }

HRESULT EffectBase::SetCorners(D2D_VECTOR_4F corners) { _constants.corners = corners; return S_OK; }
D2D_VECTOR_4F EffectBase::GetCorners() const { return _constants.corners; }

HRESULT EffectBase::SetBorder(FLOAT border) { _constants.borderblur.x = border; return S_OK; }
FLOAT EffectBase::GetBorder() const { return _constants.borderblur.x; }

HRESULT EffectBase::SetBlur(FLOAT blur) { _constants.borderblur.y = blur; return S_OK; }
FLOAT EffectBase::GetBlur() const { return _constants.borderblur.y; }

HRESULT EffectBase::SetFill(UINT color)
{
  _fill = color;
  _constants.fill = ToD2Color(color);
  return S_OK;
}
UINT EffectBase::GetFill() const { return _fill; }

HRESULT EffectBase::SetOutline(UINT color)
{
  _outline = color;
  _constants.outline = ToD2Color(color);
  return S_OK;
}
UINT EffectBase::GetOutline() const { return _outline; }

IFACEMETHODIMP EffectBase::MapOutputRectToInputRects(_In_ const D2D1_RECT_L* pOutputRect, _Out_writes_(inputRectCount) D2D1_RECT_L* pInputRects, UINT32 inputRectCount) const
{
  return E_INVALIDARG;
}
IFACEMETHODIMP EffectBase::MapInputRectsToOutputRect(_In_reads_(inputRectCount) CONST D2D1_RECT_L* pInputRects, _In_reads_(inputRectCount) CONST D2D1_RECT_L* pInputOpaqueSubRects, UINT32 inputRectCount, _Out_ D2D1_RECT_L* pOutputRect, _Out_ D2D1_RECT_L* pOutputOpaqueSubRect)
{
  *pOutputRect = _rect;
  ZeroMemory(pOutputOpaqueSubRect, sizeof(*pOutputOpaqueSubRect)); // Entire image might be transparent
  return S_OK;
}
IFACEMETHODIMP EffectBase::MapInvalidRect(UINT32 inputIndex, D2D1_RECT_L invalidInputRect, _Out_ D2D1_RECT_L* pInvalidOutputRect) const { *pInvalidOutputRect = _rect; return S_OK; }
IFACEMETHODIMP_(UINT32) EffectBase::GetInputCount() const { return 0; }

HRESULT EffectBase::UpdateConstants()
{
  return _drawInfo->SetPixelShaderConstantBuffer(reinterpret_cast<BYTE*>(&_constants), sizeof(_constants));
}