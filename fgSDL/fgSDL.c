// Copyright �2016 Black Sphere Studios
// For conditions of distribution and use, see copyright notice in "feathergui.h"

#include "fgSDL.h"
#include "fgText.h"
#include "fgResource.h"
#include "fgRoot.h"
#include "fgButton.h"
#include "fgMenu.h"
#include "fgTopWindow.h"

#if defined(BSS_DEBUG) && defined(BSS_CPU_x86_64)
#pragma comment(lib, "../bin/feathergui_d.lib")
#pragma comment(lib, "lib/SDL2_d.lib")
#elif defined(BSS_CPU_x86_64)
#pragma comment(lib, "../bin/feathergui.lib")
#pragma comment(lib, "lib/SDL2.lib")
#elif defined(BSS_DEBUG)
#pragma comment(lib, "../bin32/feathergui32_d.lib")
#pragma comment(lib, "lib/SDL2_32_d.lib")
#else
#pragma comment(lib, "../bin32/feathergui32.lib")
#pragma comment(lib, "lib/SDL2_32.lib")
#endif

fgRootSDL* _fgroot = 0; // fgRoot singleton variable

typedef struct __TEX_WRAP {
  SDL_Texture* texture;
  int w, h, ref;
} fgTexWrap;

void* FG_FASTCALL fgCreateFont(fgFlag flags, const char* font, unsigned int fontsize, float lineheight, float letterspacing) { return (void*)0xFFFFFFFF; }
void* FG_FASTCALL fgCloneFont(void* font) { return (void*)0xFFFFFFFF; }
void FG_FASTCALL fgDestroyFont(void* font) { }
void FG_FASTCALL fgDrawFont(void* font, const char* text, unsigned int color, const AbsRect* area, FABS rotation, AbsVec* center, fgFlag flags) { }
void FG_FASTCALL fgFontSize(void* font, const char* text, AbsRect* area, fgFlag flags) { }

void* FG_FASTCALL fgCreateResource(fgFlag flags, const char* data, size_t length) {
  fgTexWrap* r = (fgTexWrap*)malloc(sizeof(fgTexWrap));
  //r->texture = IMG_LoadTexture(_fgroot->render, file.c_str());
  r->ref = 1;

  Uint32 format;
  int access;
  SDL_QueryTexture(r->texture, &format, &access, &r->w, &r->h);
  return r;
}
void* FG_FASTCALL fgCloneResource(void* res) { ((fgTexWrap*)res)->ref++; return res; }
void FG_FASTCALL fgDestroyResource(void* res)
{ 
  fgTexWrap* r = (fgTexWrap*)res;
  if(--r->ref <= 0)
  {
    SDL_DestroyTexture(r->texture);
    free(r);
  }
}
void FG_FASTCALL fgDrawResource(void* res, const CRect* uv, unsigned int color, const AbsRect* area, FABS rotation, AbsVec* center, fgFlag flags)
{
  fgTexWrap* r = (fgTexWrap*)res;

  SDL_Rect uvresolve = { uv->left.rel + (uv->left.abs / r->w),
    uv->top.rel + (uv->top.abs / r->h),
    uv->right.rel + (uv->right.abs / r->w),
    uv->bottom.rel + (uv->bottom.abs / r->h) };
  uvresolve.w -= uvresolve.x;
  uvresolve.h -= uvresolve.y;

  SDL_Rect dest = { area->left, area->top, area->right - area->left, area->bottom - area->top };
  SDL_Point c = { center->x, center->y };
  dest.x -= c.x; // move the image so it's actually centered around the center, instead of just rotating around it.
  dest.y -= c.y;
  if(rotation == 0.0)
    SDL_RenderCopy(_fgroot->render, r->texture, &uvresolve, &dest);
  else
    SDL_RenderCopyEx(_fgroot->render, r->texture, &uvresolve, &dest, rotation, &c, SDL_FLIP_NONE);
}
void FG_FASTCALL fgResourceSize(void* res, const CRect* uv, AbsVec* dim, fgFlag flags)
{
  fgTexWrap* r = (fgTexWrap*)res;
  dim->x = r->w;
  dim->y = r->h;
}

#define DEFAULT_CREATE(type, init, ...) \
  type* r = (type*)malloc(sizeof(type)); \
  init(r, __VA_ARGS__); \
  ((fgChild*)r)->free = &free; \
  return (fgChild*)r

fgChild* FG_FASTCALL fgResource_Create(void* res, const CRect* uv, unsigned int color, fgFlag flags, fgChild* BSS_RESTRICT parent, fgChild* BSS_RESTRICT prev, const fgElement* element)
{
  DEFAULT_CREATE(fgResource, fgResource_Init, res, uv, color, flags, parent, prev, element);
}
fgChild* FG_FASTCALL fgText_Create(char* text, void* font, unsigned int color, fgFlag flags, fgChild* BSS_RESTRICT parent, fgChild* BSS_RESTRICT prev, const fgElement* element)
{
  DEFAULT_CREATE(fgText, fgText_Init, text, font, color, flags, parent, prev, element);
}
fgChild* FG_FASTCALL fgButton_Create(fgChild* item, fgFlag flags, fgChild* BSS_RESTRICT parent, fgChild* BSS_RESTRICT prev, const fgElement* element)
{
  DEFAULT_CREATE(fgButton, fgButton_Init, flags, parent, prev, element);
}
fgChild* FG_FASTCALL fgTopWindow_Create(const char* caption, fgFlag flags, const fgElement* element)
{
  fgTopWindow* r = (fgTopWindow*)malloc(sizeof(fgTopWindow));
  fgTopWindow_Init(r, flags, element);
  fgChild_VoidMessage((fgChild*)r, FG_SETPARENT, fgSingleton());
  fgChild_VoidMessage((fgChild*)r, FG_SETTEXT, (void*)caption);
  return (fgChild*)r;
}

fgRoot* FG_FASTCALL fgInitialize()
{
  fgRootSDL* r = (fgRootSDL*)malloc(sizeof(fgRootSDL));
  assert(!_fgroot);

  if(SDL_Init(SDL_INIT_VIDEO) != 0) {
    free(r);
    return 0;
  }

  r->window = SDL_CreateWindow("", 100, 100, 640, 480, SDL_WINDOW_SHOWN);
  if(!r->window) {
    SDL_Quit();
    free(r);
    return 0;
  }

  r->render = SDL_CreateRenderer(r->window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
  if(!r->render) {
    SDL_DestroyWindow(r->window);
    SDL_Quit();
    free(r);
    return 0;
  }

  fgRoot_Init(&r->root);
  r->cliplength = 0;
  r->clipcapacity = 4;
  r->clipstack = malloc(sizeof(SDL_Rect)*r->clipcapacity);
  return _fgroot = r;
}
char FG_FASTCALL fgLoadExtension(void* fg, const char* extname) { return -1; }
void fgPushClipRect(AbsRect* clip)
{
  if(_fgroot->cliplength >= _fgroot->clipcapacity)
  {
    _fgroot->clipcapacity *= 2;
    _fgroot->clipstack = realloc(_fgroot->clipstack, _fgroot->clipcapacity);
  }

  SDL_Rect r = { clip->left, clip->top, clip->right - clip->left, clip->bottom - clip->top };
  _fgroot->clipstack[_fgroot->cliplength] = r;
  SDL_RenderSetClipRect(_fgroot->render, _fgroot->clipstack + (_fgroot->cliplength++));
}
void fgPopClipRect()
{
  if(_fgroot->cliplength <= 1)
  {
    _fgroot->cliplength = 0;
    SDL_RenderSetClipRect(_fgroot->render, 0);
  }
  else
    SDL_RenderSetClipRect(_fgroot->render, _fgroot->clipstack + (--_fgroot->cliplength) - 1);
}