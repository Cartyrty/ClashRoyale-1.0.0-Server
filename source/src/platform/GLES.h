#pragma once
// OpenGL ES 2.0 on desktop OpenGL (compatibility profile). Object names pass through unchanged; client-side arrays
// and pixel pointers are guest addresses, which are host-addressable through the guest memory mapping.
#include <functional>
#include <string>
namespace gles {
void init();                     // load entry points (main context current)
void ensureContext();            // a GL context on the calling thread (shared with the main one)
// EAGLContext support: -renderbufferStorage:fromDrawable: allocates the bound renderbuffer as the screen;
// -presentRenderbuffer: shows it in the window.
bool storageFromDrawable(int width, int height);
bool present();
// Saves the next presented frame as a BMP.
void requestScreenshot(const std::string &path);
// Drawn over each presented frame (window pixels, origin bottom-left): e.g. the typing bar.
void setOverlay(std::function<void(int windowWidth, int windowHeight)> draw);
} // namespace gles
