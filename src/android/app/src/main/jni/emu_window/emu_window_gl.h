// Copyright 2019 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <vector>

#include <EGL/egl.h>
#include <EGL/eglext.h>

#include "jni/emu_window/emu_window.h"

struct ANativeWindow;

class EmuWindow_Android_OpenGL : public EmuWindow_Android {
public:
    EmuWindow_Android_OpenGL(ANativeWindow* surface);
    ~EmuWindow_Android_OpenGL() override = default;

    void TryPresenting() override;
    void StopPresenting() override;

    std::unique_ptr<GraphicsContext> CreateSharedContext() const override;

private:
    bool CreateWindowSurface() override;
    void DestroyWindowSurface() override;
    void DestroyContext() override;

private:
    EGLConfig egl_config;
    EGLSurface egl_surface{};
    EGLContext egl_context{};
    EGLDisplay egl_display{};
};
