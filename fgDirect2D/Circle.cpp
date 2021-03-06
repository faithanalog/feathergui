// Copyright (c)2021 Fundament Software
// For conditions of distribution and use, see copyright notice in "fgDirect2D.h"

#include "win32_includes.h"
#include <d2d1effecthelpers.h>
#include <initguid.h>
#include "Circle.h"
#include "Direct2D_Circle.h"
#include <utility>
#include <memory>

#define XML(x) TEXT(#x)

using namespace D2D;

Circle::Circle() {}
Circle::~Circle() {}
IFACEMETHODIMP Circle::Initialize(_In_ ID2D1EffectContext* pContextInternal, _In_ ID2D1TransformGraph* pTransformGraph)
{
  HRESULT hr = pContextInternal->LoadPixelShader(CLSID_CirclePixelShader, Circle_main, sizeof(Circle_main));

  if(SUCCEEDED(hr))
    hr = pTransformGraph->SetSingleTransformNode(this);

  return hr;
}
HRESULT Circle::Register(_In_ ID2D1Factory1* pFactory)
{
  /* clang-format off */
  PCWSTR pszXml =
    XML(
      <?xml version='1.0'?>
      <Effect>
        <!-- System Properties -->
        <Property name = 'DisplayName' type = 'string' value = 'Circle' />
        <Property name = 'Author' type = 'string' value = 'Fundament Software' />
        <Property name = 'Category' type = 'string' value = 'Vector' />
        <Property name = 'Description' type = 'string' value = 'Draws a circle with an outline.' />
        <Inputs>
        </Inputs>
        <Property name='Rect' type='vector4'>
            <Property name='DisplayName' type='string' value='Output Rect'/>
            <Property name='Default' type='vector4' value='(0.0, 0.0, 0.0, 0.0)' />
        </Property>
        <Property name='InnerRadius' type='float'>
            <Property name='DisplayName' type='string' value='Inner Radius'/>
            <Property name='Default' type='float' value='0.0' />
        </Property>
        <Property name='InnerBorder' type='float'>
            <Property name='DisplayName' type='string' value='Inner Outline Width'/>
            <Property name='Default' type='float' value='0.0' />
        </Property>
        <Property name='Fill' type='uint32'>
            <Property name='DisplayName' type='string' value='Fill'/>
            <Property name='Default' type='uint32' value='0' />
        </Property>
        <Property name='Outline' type='uint32'>
            <Property name='DisplayName' type='string' value='Outline Color'/>
            <Property name='Default' type='uint32' value='0' />
        </Property>
        <Property name='Border' type='float'>
            <Property name='DisplayName' type='string' value='Outline Width'/>
            <Property name='Min' type='float' value='0.0' />
            <Property name='Default' type='float' value='0.0' />
        </Property>
        <Property name='Blur' type='float'>
            <Property name='DisplayName' type='string' value='Blur Amount'/>
            <Property name='Min' type='float' value='0.0' />
            <Property name='Default' type='float' value='0.0' />
        </Property>
      </Effect>
      );
  /* clang-format on */

  // This defines the bindings from specific properties to the callback functions
  // on the class that ID2D1Effect::SetValue() & GetValue() will call.
  const D2D1_PROPERTY_BINDING bindings[] = {
    D2D1_VALUE_TYPE_BINDING(L"Rect", &SetRect, &GetRect),
    D2D1_VALUE_TYPE_BINDING(L"InnerRadius", &SetInnerRadius, &GetInnerRadius),
    D2D1_VALUE_TYPE_BINDING(L"InnerBorder", &SetInnerBorder, &GetInnerBorder),
    D2D1_VALUE_TYPE_BINDING(L"Fill", &SetFill, &GetFill),
    D2D1_VALUE_TYPE_BINDING(L"Outline", &SetOutline, &GetOutline),
    D2D1_VALUE_TYPE_BINDING(L"Border", &SetBorder, &GetBorder),
    D2D1_VALUE_TYPE_BINDING(L"Blur", &SetBlur, &GetBlur),
  };

  return pFactory->RegisterEffectFromString(CLSID_Circle, pszXml, bindings, ARRAYSIZE(bindings), CreateEffect);
}

HRESULT Circle::SetInnerRadius(FLOAT r)
{
  _constants.corners.x = r;
  return S_OK;
}
FLOAT Circle::GetInnerRadius() const { return _constants.corners.x; }
HRESULT Circle::SetInnerBorder(FLOAT r)
{
  _constants.corners.y = r;
  return S_OK;
}
FLOAT Circle::GetInnerBorder() const { return _constants.corners.y; }

HRESULT __stdcall Circle::CreateEffect(_Outptr_ IUnknown** ppEffectImpl)
{
  // This code assumes that the effect class initializes its reference count to 1.
  *ppEffectImpl = static_cast<ID2D1EffectImpl*>(new Circle());

  if(*ppEffectImpl == nullptr)
    return E_OUTOFMEMORY;
  return S_OK;
}

IFACEMETHODIMP Circle::SetDrawInfo(_In_ ID2D1DrawInfo* pRenderInfo)
{
  _drawInfo = pRenderInfo;
  _drawInfo->AddRef();
  return _drawInfo->SetPixelShader(CLSID_CirclePixelShader);
}