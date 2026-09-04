#include "rendering/backends/OpenGLExtensions.h"

#include <runtime/log/Log.hpp>

PFNGLACTIVETEXTUREPROC pglActiveTexture = nullptr;
PFNGLATTACHSHADERPROC pglAttachShader = nullptr;
PFNGLBINDATTRIBLOCATIONPROC pglBindAttribLocation = nullptr;
PFNGLBINDBUFFERPROC pglBindBuffer = nullptr;
PFNGLBINDFRAMEBUFFERPROC pglBindFramebuffer = nullptr;
PFNGLBINDRENDERBUFFERPROC pglBindRenderbuffer = nullptr;
PFNGLBINDVERTEXARRAYPROC pglBindVertexArray = nullptr;
PFNGLBLITFRAMEBUFFERPROC pglBlitFramebuffer = nullptr;
PFNGLBUFFERDATAPROC pglBufferData = nullptr;
PFNGLCHECKFRAMEBUFFERSTATUSPROC pglCheckFramebufferStatus = nullptr;
PFNGLCOMPILESHADERPROC pglCompileShader = nullptr;
PFNGLCREATEPROGRAMPROC pglCreateProgram = nullptr;
PFNGLCREATESHADERPROC pglCreateShader = nullptr;
PFNGLDELETEBUFFERSPROC pglDeleteBuffers = nullptr;
PFNGLDELETEFRAMEBUFFERSPROC pglDeleteFramebuffers = nullptr;
PFNGLDELETEPROGRAMPROC pglDeleteProgram = nullptr;
PFNGLDELETERENDERBUFFERSPROC pglDeleteRenderbuffers = nullptr;
PFNGLDELETESHADERPROC pglDeleteShader = nullptr;
PFNGLDISABLEVERTEXATTRIBARRAYPROC pglDisableVertexAttribArray = nullptr;
PFNGLENABLEVERTEXATTRIBARRAYPROC pglEnableVertexAttribArray = nullptr;
PFNGLFRAMEBUFFERRENDERBUFFERPROC pglFramebufferRenderbuffer = nullptr;
PFNGLFRAMEBUFFERTEXTURE2DPROC pglFramebufferTexture2D = nullptr;
PFNGLGENBUFFERSPROC pglGenBuffers = nullptr;
PFNGLGENFRAMEBUFFERSPROC pglGenFramebuffers = nullptr;
PFNGLGENRENDERBUFFERSPROC pglGenRenderbuffers = nullptr;
PFNGLGENVERTEXARRAYSPROC pglGenVertexArrays = nullptr;
PFNGLGETATTRIBLOCATIONPROC pglGetAttribLocation = nullptr;
PFNGLGETPROGRAMINFOLOGPROC pglGetProgramInfoLog = nullptr;
PFNGLGETPROGRAMIVPROC pglGetProgramiv = nullptr;
PFNGLGETSHADERINFOLOGPROC pglGetShaderInfoLog = nullptr;
PFNGLGETSHADERIVPROC pglGetShaderiv = nullptr;
PFNGLGETUNIFORMLOCATIONPROC pglGetUniformLocation = nullptr;
PFNGLLINKPROGRAMPROC pglLinkProgram = nullptr;
PFNGLRENDERBUFFERSTORAGEPROC pglRenderbufferStorage = nullptr;
PFNGLRENDERBUFFERSTORAGEMULTISAMPLEPROC pglRenderbufferStorageMultisample = nullptr;
PFNGLSHADERSOURCEPROC pglShaderSource = nullptr;
PFNGLUNIFORM1FPROC pglUniform1f = nullptr;
PFNGLUNIFORM1IPROC pglUniform1i = nullptr;
PFNGLUNIFORM1IVPROC pglUniform1iv = nullptr;
PFNGLUNIFORM2FPROC pglUniform2f = nullptr;
PFNGLUNIFORM4FPROC pglUniform4f = nullptr;
PFNGLUSEPROGRAMPROC pglUseProgram = nullptr;
PFNGLVERTEXATTRIBPOINTERPROC pglVertexAttribPointer = nullptr;

namespace {

template <typename T>
bool Load(T& function, const char* name, const char* fallback = nullptr) {
    function = reinterpret_cast<T>(wglGetProcAddress(name));
    if (function == nullptr && fallback != nullptr) {
        function = reinterpret_cast<T>(wglGetProcAddress(fallback));
    }
    if (function == nullptr) {
        WriteLog("Required OpenGL function is unavailable: {}", name);
        return false;
    }
    return true;
}

} // namespace

bool InitializeOpenGLExtensions() {
    bool ready = true;
    ready &= Load(pglActiveTexture, "glActiveTexture");
    ready &= Load(pglAttachShader, "glAttachShader");
    ready &= Load(pglBindAttribLocation, "glBindAttribLocation");
    ready &= Load(pglBindBuffer, "glBindBuffer");
    ready &= Load(pglBindFramebuffer, "glBindFramebuffer", "glBindFramebufferEXT");
    ready &= Load(pglBindRenderbuffer, "glBindRenderbuffer", "glBindRenderbufferEXT");
    ready &= Load(pglBlitFramebuffer, "glBlitFramebuffer", "glBlitFramebufferEXT");
    ready &= Load(pglBufferData, "glBufferData");
    ready &= Load(pglCheckFramebufferStatus, "glCheckFramebufferStatus", "glCheckFramebufferStatusEXT");
    ready &= Load(pglCompileShader, "glCompileShader");
    ready &= Load(pglCreateProgram, "glCreateProgram");
    ready &= Load(pglCreateShader, "glCreateShader");
    ready &= Load(pglDeleteBuffers, "glDeleteBuffers");
    ready &= Load(pglDeleteFramebuffers, "glDeleteFramebuffers", "glDeleteFramebuffersEXT");
    ready &= Load(pglDeleteProgram, "glDeleteProgram");
    ready &= Load(pglDeleteRenderbuffers, "glDeleteRenderbuffers", "glDeleteRenderbuffersEXT");
    ready &= Load(pglDeleteShader, "glDeleteShader");
    ready &= Load(pglDisableVertexAttribArray, "glDisableVertexAttribArray");
    ready &= Load(pglEnableVertexAttribArray, "glEnableVertexAttribArray");
    ready &= Load(pglFramebufferRenderbuffer, "glFramebufferRenderbuffer", "glFramebufferRenderbufferEXT");
    ready &= Load(pglFramebufferTexture2D, "glFramebufferTexture2D", "glFramebufferTexture2DEXT");
    ready &= Load(pglGenBuffers, "glGenBuffers");
    ready &= Load(pglGenFramebuffers, "glGenFramebuffers", "glGenFramebuffersEXT");
    ready &= Load(pglGenRenderbuffers, "glGenRenderbuffers", "glGenRenderbuffersEXT");
    ready &= Load(pglGetAttribLocation, "glGetAttribLocation");
    ready &= Load(pglGetProgramInfoLog, "glGetProgramInfoLog");
    ready &= Load(pglGetProgramiv, "glGetProgramiv");
    ready &= Load(pglGetShaderInfoLog, "glGetShaderInfoLog");
    ready &= Load(pglGetShaderiv, "glGetShaderiv");
    ready &= Load(pglGetUniformLocation, "glGetUniformLocation");
    ready &= Load(pglLinkProgram, "glLinkProgram");
    ready &= Load(pglRenderbufferStorage, "glRenderbufferStorage", "glRenderbufferStorageEXT");
    ready &= Load(pglRenderbufferStorageMultisample, "glRenderbufferStorageMultisample",
                  "glRenderbufferStorageMultisampleEXT");
    ready &= Load(pglShaderSource, "glShaderSource");
    ready &= Load(pglUniform1f, "glUniform1f");
    ready &= Load(pglUniform1i, "glUniform1i");
    ready &= Load(pglUniform1iv, "glUniform1iv");
    ready &= Load(pglUniform2f, "glUniform2f");
    ready &= Load(pglUniform4f, "glUniform4f");
    ready &= Load(pglUseProgram, "glUseProgram");
    ready &= Load(pglVertexAttribPointer, "glVertexAttribPointer");

    pglBindVertexArray = reinterpret_cast<PFNGLBINDVERTEXARRAYPROC>(wglGetProcAddress("glBindVertexArray"));
    pglGenVertexArrays = reinterpret_cast<PFNGLGENVERTEXARRAYSPROC>(wglGetProcAddress("glGenVertexArrays"));
    return ready;
}
