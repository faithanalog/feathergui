// Copyright �2012 Black Sphere Studios
// For conditions of distribution and use, see copyright notice in "feathergui.h"

#ifndef __FG_CONTAINER_H__
#define __FG_CONTAINER_H__

#include "fgWindow.h"

#ifdef  __cplusplus
extern "C" {
#endif

// A container is a collection of fgChild objects that divide a window into areas.
typedef struct {
  fgWindow window;
  fgChild* regions;
} fgContainer;

FG_EXTERN void FG_FASTCALL fgContainer_Init(fgContainer* self);
FG_EXTERN void FG_FASTCALL fgContainer_Destroy(fgContainer* self);
FG_EXTERN fgChild* FG_FASTCALL fgContainer_AddRegion(fgContainer* self, fgElement* region);
FG_EXTERN void FG_FASTCALL fgContainer_RemoveRegion(fgContainer* self, fgChild* region);

#ifdef  __cplusplus
}
#endif

#endif