/*
 * Ideas for screen management extension to EGL.
 *
 * Each EGLDisplay has one or more screens (CRTs, Flat Panels, etc).
 * The screens' handles can be obtained with eglGetScreensMESA().
 *
 * A new kind of EGLSurface is possible- one which can be directly scanned
 * out on a screen.  Such a surface is created with eglCreateScreenSurface().
 *
 * To actually display a screen surface on a screen, the eglShowSurface()
 * function is called.
 */

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "egldisplay.h"
#include "eglglobals.h"
#include "eglmode.h"
#include "eglconfig.h"
#include "eglsurface.h"
#include "eglscreen.h"


/**
 * Initialize an _EGLScreen object to default values.
 */
void
_eglInitScreen(_EGLScreen *screen)
{
   /* just init to zero for now */
   memset(screen, 0, sizeof(_EGLScreen));
}


/**
 * Given a public screen handle, return the internal _EGLScreen object.
 */
_EGLScreen *
_eglLookupScreen(EGLDisplay dpy, EGLScreenMESA screen)
{
   EGLint i;
   _EGLDisplay *display = _eglLookupDisplay(dpy);

   if (!display)
      return NULL;

   for (i = 0; i < display->NumScreens; i++) {
      if (display->Screens[i]->Handle == screen)
         return display->Screens[i];
   }
   return NULL;
}


/**
 * Add the given _EGLScreen to the display's list of screens.
 */
void
_eglAddScreen(_EGLDisplay *display, _EGLScreen *screen)
{
   EGLint n;

   assert(display);
   assert(screen);

   screen->Handle = _eglAllocScreenHandle();
   n = display->NumScreens;
   display->Screens = realloc(display->Screens, (n+1) * sizeof(_EGLScreen *));
   display->Screens[n] = screen;
   display->NumScreens++;
}



EGLBoolean
_eglGetScreensMESA(_EGLDriver *drv, EGLDisplay dpy, EGLScreenMESA *screens,
                   EGLint max_screens, EGLint *num_screens)
{
   _EGLDisplay *display = _eglLookupDisplay(dpy);
   EGLint n;

   if (!display) {
      _eglError(EGL_BAD_DISPLAY, "eglGetScreensMESA");
      return EGL_FALSE;
   }

   if (display->NumScreens > max_screens) {
      n = max_screens;
   }
   else {
      n = display->NumScreens;
   }

   if (screens) {
      EGLint i;
      for (i = 0; i < n; i++)
         screens[i] = display->Screens[i]->Handle;
   }
   if (num_screens)
      *num_screens = n;

   return EGL_TRUE;
}


/**
 * Initialize the given _EGLSurface object.  Do error checking.
 * Assign it an EGLSurface handle and insert into hash table.
 * \return EGLSurface handle or EGL_NO_SURFACE if error.
 */
EGLSurface
_eglInitScreenSurface(_EGLSurface *surf, _EGLDriver *drv, EGLDisplay dpy,
                      EGLConfig config, const EGLint *attrib_list)
{
   EGLint width = 0, height = 0;
   EGLint i;

   for (i = 0; attrib_list && attrib_list[i] != EGL_NONE; i++) {
      switch (attrib_list[i]) {
      case EGL_WIDTH:
         width = attrib_list[++i];
         break;
      case EGL_HEIGHT:
         height = attrib_list[++i];
         break;
      default:
         _eglError(EGL_BAD_ATTRIBUTE, "eglCreateScreenSurfaceMESA");
         return EGL_NO_SURFACE;
      }
   }

   if (width <= 0 || height <= 0) {
      _eglError(EGL_BAD_ATTRIBUTE,
                "eglCreateScreenSurfaceMESA(width or height)");
      return EGL_NO_SURFACE;
   }

   _eglInitSurface(surf);
   surf->Width = width;
   surf->Height = height;
   surf->Type = EGL_SCREEN_BIT_MESA;
   surf->Config =  _eglLookupConfig(drv, dpy, config);

   /* insert into hash table */
   _eglSaveSurface(surf);
   assert(surf->Handle);

   return surf->Handle;
}


/**
 * Create a drawing surface which can be directly displayed on a screen.
 */
EGLSurface
_eglCreateScreenSurfaceMESA(_EGLDriver *drv, EGLDisplay dpy, EGLConfig config,
                            const EGLint *attrib_list)
{
   _EGLSurface *surf;
   EGLSurface surface;
   
   surf = (_EGLSurface *) malloc(sizeof(_EGLSurface));
   surface = _eglInitScreenSurface(surf, drv, dpy, config, attrib_list);
   if (surface == EGL_NO_SURFACE)
      free(surf);

   return surface;
}


/**
 * Show the given surface on the named screen.
 * If surface is EGL_NO_SURFACE, disable the screen's output.
 * 
 * This is just a placeholder function; drivers will always override
 * this with code that _really_ shows the surface.
 */
EGLBoolean
_eglShowSurfaceMESA(_EGLDriver *drv, EGLDisplay dpy, EGLScreenMESA screen,
                    EGLSurface surface, EGLModeMESA m)
{
   _EGLScreen *scrn = _eglLookupScreen(dpy, screen);
   _EGLMode *mode = _eglLookupMode(dpy, m);

   if (!scrn) {
      _eglError(EGL_BAD_SCREEN_MESA, "eglShowSurfaceMESA");
      return EGL_FALSE;
   }
   if (!mode && (m != EGL_NO_MODE_MESA )) {
      _eglError(EGL_BAD_MODE_MESA, "eglShowSurfaceMESA");
      return EGL_FALSE;
   }

   if (surface == EGL_NO_SURFACE) {
      scrn->CurrentSurface = NULL;
   } else {
      _EGLSurface *surf = _eglLookupSurface(surface);
      if (!surf || surf->Type != EGL_SCREEN_BIT_MESA) {
         _eglError(EGL_BAD_SURFACE, "eglShowSurfaceMESA");
         return EGL_FALSE;
      }
      if (surf->Width < mode->Width || surf->Height < mode->Height) {
         _eglError(EGL_BAD_SURFACE,
                   "eglShowSurfaceMESA(surface smaller than screen size)");
         return EGL_FALSE;
      }

      scrn->CurrentSurface = surf;
      scrn->CurrentMode = mode;
   }
   return EGL_TRUE;
}


/**
 * Set a screen's current display mode.
 * Note: mode = EGL_NO_MODE is valid (turns off the screen)
 *
 * This is just a placeholder function; drivers will always override
 * this with code that _really_ sets the mode.
 */
EGLBoolean
_eglScreenModeMESA(_EGLDriver *drv, EGLDisplay dpy, EGLScreenMESA screen,
                   EGLModeMESA mode)
{
   _EGLScreen *scrn = _eglLookupScreen(dpy, screen);

   if (!scrn) {
      _eglError(EGL_BAD_SCREEN_MESA, "eglScreenModeMESA");
      return EGL_FALSE;
   }

   scrn->CurrentMode = _eglLookupMode(dpy, mode);

   return EGL_TRUE;
}


/**
 * Set a screen's surface origin.
 */
EGLBoolean
_eglScreenPositionMESA(_EGLDriver *drv, EGLDisplay dpy,
                       EGLScreenMESA screen, EGLint x, EGLint y)
{
   _EGLScreen *scrn = _eglLookupScreen(dpy, screen);
   if (!scrn) {
      _eglError(EGL_BAD_SCREEN_MESA, "eglScreenPositionMESA");
      return EGL_FALSE;
   }

   scrn->OriginX = x;
   scrn->OriginY = y;

   return EGL_TRUE;
}


/**
 * Query a screen's current surface.
 */
EGLBoolean
_eglQueryScreenSurfaceMESA(_EGLDriver *drv, EGLDisplay dpy,
                           EGLScreenMESA screen, EGLSurface *surface)
{
   const _EGLScreen *scrn = _eglLookupScreen(dpy, screen);
   if (scrn->CurrentSurface)
      *surface = scrn->CurrentSurface->Handle;
   else
      *surface = EGL_NO_SURFACE;
   return EGL_TRUE;
}


/**
 * Query a screen's current mode.
 */
EGLBoolean
_eglQueryScreenModeMESA(_EGLDriver *drv, EGLDisplay dpy, EGLScreenMESA screen,
                        EGLModeMESA *mode)
{
   const _EGLScreen *scrn = _eglLookupScreen(dpy, screen);
   if (scrn->CurrentMode)
      *mode = scrn->CurrentMode->Handle;
   else
      *mode = EGL_NO_MODE_MESA;
   return EGL_TRUE;
}


EGLBoolean
_eglQueryScreenMESA(_EGLDriver *drv, EGLDisplay dpy, EGLScreenMESA screen,
                    EGLint attribute, EGLint *value)
{
   const _EGLScreen *scrn = _eglLookupScreen(dpy, screen);

   if (!scrn) {
      _eglError(EGL_BAD_SCREEN_MESA, "eglQueryScreenMESA");
      return EGL_FALSE;
   }

   switch (attribute) {
   case EGL_SCREEN_POSITION_MESA:
      value[0] = scrn->OriginX;
      value[1] = scrn->OriginY;
      break;
   default:
      _eglError(EGL_BAD_ATTRIBUTE, "eglQueryScreenMESA");
      return EGL_FALSE;
   }

   return EGL_TRUE;
}


void
_eglDestroyScreenModes(_EGLScreen *scrn)
{
   free(scrn->Modes);
}

      
/**
 * Default fallback routine - drivers should usually override this.
 */
void
_eglDestroyScreen(_EGLScreen *scrn)
{
   _eglDestroyScreenModes(scrn);
   free(scrn);
}

