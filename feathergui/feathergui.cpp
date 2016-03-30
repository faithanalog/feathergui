// Copyright �2016 Black Sphere Studios
// For conditions of distribution and use, see copyright notice in "feathergui.h"

#include "feathergui.h"
#include "feathercpp.h"
#include "fgRoot.h"
#include <intrin.h>
#include <limits.h>

const fgElement fgElement_DEFAULT = { { 0, 0, 0, 0, 0, 1, 0, 1 }, 0, { 0, 0, 0, 0 } };
const fgElement fgElement_CENTER = { {0, 0.5, 0, 0.5, 0, 0.5, 0, 0.5 }, 0, {0, 0.5, 0, 0.5} };
const fgColor fgColor_NONE = { 0 };
const fgColor fgColor_BLACK = { 0xFF000000 };
const fgColor fgColor_WHITE = { 0xFFFFFFFF };
bss_util::cHash<std::pair<fgChild*, unsigned short>, void(FG_FASTCALL *)(struct _FG_CHILD*, const FG_Msg*)> fgListenerHash;

static_assert(sizeof(unsigned int) == sizeof(fgColor), "ERROR: fgColor not size of 32-bit int!");
static_assert(sizeof(FG_Msg) <= sizeof(uint64_t) * 3, "FG_Msg is too big!");

AbsVec FG_FASTCALL ResolveVec(const CVec* v, const AbsRect* last)
{
  AbsVec r = { v->x.abs, v->y.abs };
  assert(last!=0);
  r.x += lerp(last->left,last->right,v->x.rel);
  r.y += lerp(last->top,last->bottom,v->y.rel);
  return r; 
}

// This uses a standard inclusive-exclusive rectangle interpretation.
char FG_FASTCALL HitAbsRect(const AbsRect* r, FABS x, FABS y)
{
  assert(r!=0);
  return (x < r->right) && (x >= r->left) && (y < r->bottom) && (y >= r->top);
}

char FG_FASTCALL MsgHitAbsRect(const FG_Msg* msg, const AbsRect* r)
{
  assert(msg!=0 && r!=0);
  return HitAbsRect(r,(FABS)msg->x,(FABS)msg->y);
}

char FG_FASTCALL CompareAbsRects(const AbsRect* l, const AbsRect* r)
{
  assert(l != 0 && r != 0);
  return ((((l->left - r->left) != (l->right - r->right))) << 1)
    | ((((l->top - r->top) != (l->bottom - r->bottom))) << 2)
    | ((((l->left != r->left) || (l->right != r->right))) << 3)
    | ((((l->top != r->top) || (l->bottom != r->bottom))) << 4);
}
char FG_FASTCALL CompareCRects(const CRect* l, const CRect* r)
{
  assert(l!=0 && r!=0);
  return ((((l->left.abs - r->left.abs) != (l->right.abs - r->right.abs))) << 1)
    | ((((l->top.abs - r->top.abs) != (l->bottom.abs - r->bottom.abs))) << 2)
    | ((((l->left.abs != r->left.abs) || (l->right.abs != r->right.abs))) << 3)
    | ((((l->top.abs != r->top.abs) || (l->bottom.abs != r->bottom.abs))) << 4)
    | ((((l->left.rel - r->left.rel) != (l->right.rel - r->right.rel))) << 1)
    | ((((l->top.rel - r->top.rel) != (l->bottom.rel - r->bottom.rel))) << 2)
    | ((((l->left.rel != r->left.rel) || (l->right.rel != r->right.rel))) << 3)
    | ((((l->top.rel != r->top.rel) || (l->bottom.rel != r->bottom.rel))) << 4);
}
char FG_FASTCALL CompareElements(const fgElement* l, const fgElement* r)
{
  assert(l != 0 && r != 0);
  return CompareCRects(&l->area, &r->area)
    | (((l->center.x.abs != r->center.x.abs) || (l->center.x.rel != r->center.x.rel)) << 5)
    | (((l->center.y.abs != r->center.y.abs) || (l->center.y.rel != r->center.y.rel)) << 6)
    | ((l->rotation != r->rotation) << 7);
}

void FG_FASTCALL MoveCRect(AbsVec v, CRect* r)
{
  AbsVec d = { r->right.abs - r->left.abs, r->bottom.abs - r->top.abs };
  r->left.abs = v.x;
  r->top.abs = v.y;
  r->right.abs = v.x + d.x;
  r->bottom.abs = v.y + d.y;
}
void FG_FASTCALL MoveCRectInv(AbsVec v, CRect* r)
{
  AbsVec d = { r->bottom.abs - r->top.abs, r->right.abs - r->left.abs };
  r->left.abs = v.y;
  r->top.abs = v.x;
  r->right.abs = v.y + d.x;
  r->bottom.abs = v.x + d.y;
}

void FG_FASTCALL ToIntAbsRect(const AbsRect* r, int target[4])
{
  _mm_storeu_si128((__m128i*)target,_mm_cvttps_epi32(_mm_loadu_ps(&r->left)));
}

void FG_FASTCALL ToLongAbsRect(const AbsRect* r, long target[4])
{
#if ULONG_MAX==UINT_MAX
  _mm_storeu_si128((__m128i*)target,_mm_cvttps_epi32(_mm_loadu_ps(&r->left)));
#else
  int hold[4];
  _mm_storeu_si128((__m128i*)hold,_mm_cvtps_epi32(_mm_loadu_ps(&r->left)));
  target[0]=hold[0];
  target[1]=hold[1];
  target[2]=hold[2];
  target[3]=hold[3];
#endif

}

char* FG_FASTCALL fgCopyText(const char* text)
{
  if(!text) return 0;
  size_t len = strlen(text) + 1;
  char* ret = (char*)malloc(len);
  memcpy(ret, text, len);
  return ret;
}

void FG_FASTCALL fgUpdateMouseState(fgMouseState* state, const FG_Msg* msg)
{
  assert(msg != 0);
  switch(msg->type)
  {
  case FG_MOUSEDOWN:
    state->state |= FGMOUSE_HOVER;
    state->state |= FGMOUSE_INSIDE;
    break;
  case FG_MOUSEUP:
    state->state |= FGMOUSE_HOVER;
    state->state &= ~FGMOUSE_INSIDE;
    break;
  case FG_MOUSEMOVE:
    state->state |= FGMOUSE_HOVER;
    break;
  case FG_MOUSELEAVE:
    state->state &= ~FGMOUSE_HOVER;
    break;
  default:
    return;
  }
  state->x = msg->x;
  state->y = msg->y;
  state->buttons = msg->allbtn;
  if(fgroot_instance->drag != 0)
    state->state |= FGMOUSE_DRAG;
  if(msg->type == FG_MOUSELEAVE)
    state->state &= ~FGMOUSE_DRAG;
}