// Copyright �2016 Black Sphere Studios
// For conditions of distribution and use, see copyright notice in "feathergui.h"

#ifndef _FG_TEXTBOX_H__
#define _FG_TEXTBOX_H__

#include "fgScrollbar.h"
#include "fgText.h"

#ifdef  __cplusplus
extern "C" {
#endif

// A Textbox is really just a text static inside an optional Scrollbar. It can be single or multi-line with an optional validation regex.
// The textbox only understands single UTF codepoints, so an external library should be used to perform unicode normalization before setting it.
typedef struct {
  fgScrollbar window;
  fgText text; // Get or set the text using GETTEXT or SETTEXT messages
  char* validation; // validation regex
  int mask; // If not zero, stores a unicode character for password masking. 
  char* placeholder; // placeholder text displayed when textbox is empty. Use SETTEXT or GETTEXT with the second argument set to 1.
  fgColor selector; // Color of the selector rectangle
  fgColor placecolor; // placeholder text color. Use SETCOLOR with the second argument set to 1.
  size_t start; // start of text selection
  size_t end; // end of text selection (or just where the cursor is)
#ifdef  __cplusplus
  inline operator fgElement*() { return &window.control.element; }
  inline fgElement* operator->() { return operator fgElement*(); }
#endif
} fgTextbox;

FG_EXTERN fgControl* FG_FASTCALL fgTextbox_Create(fgControl* parent, fgFlag flags, const fgTransform* transform);
FG_EXTERN void FG_FASTCALL fgTextbox_Init(fgTextbox* self, fgControl* parent, fgFlag flags, const fgTransform* transform);
FG_EXTERN char FG_FASTCALL fgTextbox_Message(fgTextbox* self, const FG_Msg* msg);

#ifdef  __cplusplus
}
#endif

#endif