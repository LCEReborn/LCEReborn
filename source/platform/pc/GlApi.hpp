#ifndef PLATFORM_PC_GLAPI_HPP
#define PLATFORM_PC_GLAPI_HPP

#include <stddef.h>

#if defined(_WIN32) && !defined(_WIN64)
#define LCE_GL_APIENTRY __stdcall
#else
#define LCE_GL_APIENTRY
#endif

typedef unsigned int GLenum;
typedef unsigned int GLuint;
typedef int GLint;
typedef int GLsizei;
typedef unsigned char GLboolean;
typedef float GLfloat;
typedef double GLdouble;
typedef unsigned int GLbitfield;
typedef char GLchar;
typedef unsigned char GLubyte;
typedef ptrdiff_t GLintptr;
typedef ptrdiff_t GLsizeiptr;

namespace Gl
{
const GLenum NONE = 0;
const GLboolean FALSE_VALUE = 0;
const GLboolean TRUE_VALUE = 1;

const GLbitfield DEPTH_BUFFER_BIT = 0x00000100;
const GLbitfield STENCIL_BUFFER_BIT = 0x00000400;
const GLbitfield COLOR_BUFFER_BIT = 0x00004000;

const GLenum LINES = 0x0001;
const GLenum LINE_STRIP = 0x0003;
const GLenum TRIANGLES = 0x0004;
const GLenum TRIANGLE_STRIP = 0x0005;
const GLenum TRIANGLE_FAN = 0x0006;

const GLenum NEVER = 0x0200;
const GLenum LESS = 0x0201;
const GLenum EQUAL = 0x0202;
const GLenum LEQUAL = 0x0203;
const GLenum GREATER = 0x0204;
const GLenum NOTEQUAL = 0x0205;
const GLenum GEQUAL = 0x0206;
const GLenum ALWAYS = 0x0207;

const GLenum ZERO = 0;
const GLenum ONE = 1;
const GLenum SRC_COLOR = 0x0300;
const GLenum ONE_MINUS_SRC_COLOR = 0x0301;
const GLenum SRC_ALPHA = 0x0302;
const GLenum ONE_MINUS_SRC_ALPHA = 0x0303;
const GLenum DST_ALPHA = 0x0304;
const GLenum ONE_MINUS_DST_ALPHA = 0x0305;
const GLenum DST_COLOR = 0x0306;
const GLenum ONE_MINUS_DST_COLOR = 0x0307;
const GLenum SRC_ALPHA_SATURATE = 0x0308;
const GLenum CONSTANT_COLOR = 0x8001;
const GLenum ONE_MINUS_CONSTANT_COLOR = 0x8002;
const GLenum CONSTANT_ALPHA = 0x8003;
const GLenum ONE_MINUS_CONSTANT_ALPHA = 0x8004;
const GLenum SRC1_ALPHA = 0x8589;
const GLenum SRC1_COLOR = 0x88F9;
const GLenum ONE_MINUS_SRC1_COLOR = 0x88FA;
const GLenum ONE_MINUS_SRC1_ALPHA = 0x88FB;

const GLenum FRONT = 0x0404;
const GLenum BACK = 0x0405;
const GLenum CW = 0x0900;
const GLenum CCW = 0x0901;

const GLenum CULL_FACE = 0x0B44;
const GLenum DEPTH_TEST = 0x0B71;
const GLenum STENCIL_TEST = 0x0B90;
const GLenum BLEND = 0x0BE2;
const GLenum SCISSOR_TEST = 0x0C11;
const GLenum POLYGON_OFFSET_FILL = 0x8037;

const GLenum KEEP = 0x1E00;
const GLenum REPLACE = 0x1E01;

const GLenum UNPACK_ROW_LENGTH = 0x0CF2;
const GLenum UNPACK_ALIGNMENT = 0x0CF5;
const GLenum PACK_ALIGNMENT = 0x0D05;
const GLenum MAX_TEXTURE_SIZE = 0x0D33;

const GLenum TEXTURE_2D = 0x0DE1;
const GLenum TEXTURE_MAG_FILTER = 0x2800;
const GLenum TEXTURE_MIN_FILTER = 0x2801;
const GLenum TEXTURE_WRAP_S = 0x2802;
const GLenum TEXTURE_WRAP_T = 0x2803;
const GLenum TEXTURE_BASE_LEVEL = 0x813C;
const GLenum TEXTURE_MAX_LEVEL = 0x813D;
const GLenum NEAREST = 0x2600;
const GLenum LINEAR = 0x2601;
const GLenum NEAREST_MIPMAP_NEAREST = 0x2700;
const GLenum LINEAR_MIPMAP_NEAREST = 0x2701;
const GLenum NEAREST_MIPMAP_LINEAR = 0x2702;
const GLenum LINEAR_MIPMAP_LINEAR = 0x2703;
const GLenum REPEAT = 0x2901;
const GLenum CLAMP_TO_EDGE = 0x812F;
const GLenum TEXTURE0 = 0x84C0;

const GLenum BYTE = 0x1400;
const GLenum UNSIGNED_BYTE = 0x1401;
const GLenum UNSIGNED_INT_8_8_8_8 = 0x8035;
const GLenum SHORT = 0x1402;
const GLenum UNSIGNED_SHORT = 0x1403;
const GLenum INT = 0x1404;
const GLenum UNSIGNED_INT = 0x1405;
const GLenum FLOAT = 0x1406;
const GLenum HALF_FLOAT = 0x140B;

const GLenum RED = 0x1903;
const GLenum RGBA = 0x1908;
const GLenum RGBA8 = 0x8058;
const GLenum RGBA16F = 0x881A;
const GLenum R16F = 0x822D;
const GLenum DEPTH24_STENCIL8 = 0x88F0;

const GLenum ARRAY_BUFFER = 0x8892;
const GLenum ELEMENT_ARRAY_BUFFER = 0x8893;
const GLenum UNIFORM_BUFFER = 0x8A11;
const GLenum STREAM_DRAW = 0x88E0;
const GLenum STATIC_DRAW = 0x88E4;
const GLenum UNIFORM_BUFFER_OFFSET_ALIGNMENT = 0x8A34;

const GLbitfield MAP_WRITE_BIT = 0x0002;
const GLbitfield MAP_INVALIDATE_RANGE_BIT = 0x0004;
const GLbitfield MAP_UNSYNCHRONIZED_BIT = 0x0020;

const GLenum FRAMEBUFFER = 0x8D40;
const GLenum READ_FRAMEBUFFER = 0x8CA8;
const GLenum DRAW_FRAMEBUFFER = 0x8CA9;
const GLenum RENDERBUFFER = 0x8D41;
const GLenum COLOR_ATTACHMENT0 = 0x8CE0;
const GLenum DEPTH_STENCIL_ATTACHMENT = 0x821A;
const GLenum FRAMEBUFFER_COMPLETE = 0x8CD5;

const GLenum FRAGMENT_SHADER = 0x8B30;
const GLenum VERTEX_SHADER = 0x8B31;
const GLenum COMPILE_STATUS = 0x8B81;
const GLenum LINK_STATUS = 0x8B82;
const GLenum INFO_LOG_LENGTH = 0x8B84;
const GLuint INVALID_INDEX = 0xFFFFFFFFu;

const GLenum VENDOR = 0x1F00;
const GLenum RENDERER = 0x1F01;
const GLenum VERSION = 0x1F02;
}

#define LCE_GL_FUNCTIONS(X)                                                                                            \
    X(const GLubyte*, GetString, (GLenum name))                                                                        \
    X(void, GetIntegerv, (GLenum pname, GLint * data))                                                                 \
    X(void, Enable, (GLenum cap))                                                                                      \
    X(void, Disable, (GLenum cap))                                                                                     \
    X(void, Viewport, (GLint x, GLint y, GLsizei width, GLsizei height))                                               \
    X(void, Scissor, (GLint x, GLint y, GLsizei width, GLsizei height))                                                \
    X(void, ClearColor, (GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha))                                     \
    X(void, ClearDepth, (GLdouble depth))                                                                              \
    X(void, ClearStencil, (GLint s))                                                                                   \
    X(void, Clear, (GLbitfield mask))                                                                                  \
    X(void, ColorMask, (GLboolean red, GLboolean green, GLboolean blue, GLboolean alpha))                              \
    X(void, ColorMaski, (GLuint index, GLboolean red, GLboolean green, GLboolean blue, GLboolean alpha))               \
    X(void, DepthMask, (GLboolean flag))                                                                               \
    X(void, DepthFunc, (GLenum func))                                                                                  \
    X(void, StencilFunc, (GLenum func, GLint ref, GLuint mask))                                                        \
    X(void, StencilOp, (GLenum fail, GLenum zfail, GLenum zpass))                                                      \
    X(void, StencilMask, (GLuint mask))                                                                                \
    X(void, BlendFunc, (GLenum sfactor, GLenum dfactor))                                                               \
    X(void, BlendFuncSeparate, (GLenum srcRGB, GLenum dstRGB, GLenum srcAlpha, GLenum dstAlpha))                       \
    X(void, BlendColor, (GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha))                                     \
    X(void, CullFace, (GLenum mode))                                                                                   \
    X(void, FrontFace, (GLenum mode))                                                                                  \
    X(void, PolygonOffset, (GLfloat factor, GLfloat units))                                                            \
    X(void, PixelStorei, (GLenum pname, GLint param))                                                                  \
    X(void, ReadPixels, (GLint x, GLint y, GLsizei width, GLsizei height, GLenum format, GLenum type, void* pixels))   \
    X(void, Finish, (void))                                                                                            \
    X(void, Flush, (void))                                                                                             \
    X(void, GenTextures, (GLsizei n, GLuint * textures))                                                               \
    X(void, DeleteTextures, (GLsizei n, const GLuint* textures))                                                       \
    X(void, BindTexture, (GLenum target, GLuint texture))                                                              \
    X(void, ActiveTexture, (GLenum texture))                                                                           \
    X(void, TexImage2D, (GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height,             \
                         GLint border, GLenum format, GLenum type, const void* pixels))                                \
    X(void, TexSubImage2D, (GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height,  \
                            GLenum format, GLenum type, const void* pixels))                                           \
    X(void, TexParameteri, (GLenum target, GLenum pname, GLint param))                                                 \
    X(void, GenSamplers, (GLsizei count, GLuint * samplers))                                                           \
    X(void, DeleteSamplers, (GLsizei count, const GLuint* samplers))                                                   \
    X(void, BindSampler, (GLuint unit, GLuint sampler))                                                                \
    X(void, SamplerParameteri, (GLuint sampler, GLenum pname, GLint param))                                            \
    X(void, GenBuffers, (GLsizei n, GLuint * buffers))                                                                 \
    X(void, DeleteBuffers, (GLsizei n, const GLuint* buffers))                                                         \
    X(void, BindBuffer, (GLenum target, GLuint buffer))                                                                \
    X(void, BufferData, (GLenum target, GLsizeiptr size, const void* data, GLenum usage))                              \
    X(void, BufferSubData, (GLenum target, GLintptr offset, GLsizeiptr size, const void* data))                        \
    X(void*, MapBufferRange, (GLenum target, GLintptr offset, GLsizeiptr length, GLbitfield access))                   \
    X(GLboolean, UnmapBuffer, (GLenum target))                                                                         \
    X(void, BindBufferRange, (GLenum target, GLuint index, GLuint buffer, GLintptr offset, GLsizeiptr size))           \
    X(void, GenVertexArrays, (GLsizei n, GLuint * arrays))                                                             \
    X(void, DeleteVertexArrays, (GLsizei n, const GLuint* arrays))                                                     \
    X(void, BindVertexArray, (GLuint array))                                                                           \
    X(void, EnableVertexAttribArray, (GLuint index))                                                                   \
    X(void, VertexAttribPointer, (GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride,        \
                                  const void* pointer))                                                                \
    X(void, VertexAttribIPointer, (GLuint index, GLint size, GLenum type, GLsizei stride, const void* pointer))       \
    X(void, DrawArrays, (GLenum mode, GLint first, GLsizei count))                                                     \
    X(void, DrawElementsBaseVertex, (GLenum mode, GLsizei count, GLenum type, const void* indices,                     \
                                     GLint basevertex))                                                                \
    X(GLuint, CreateShader, (GLenum type))                                                                             \
    X(void, ShaderSource, (GLuint shader, GLsizei count, const GLchar* const* string, const GLint* length))           \
    X(void, CompileShader, (GLuint shader))                                                                            \
    X(void, GetShaderiv, (GLuint shader, GLenum pname, GLint * params))                                                \
    X(void, GetShaderInfoLog, (GLuint shader, GLsizei bufSize, GLsizei * length, GLchar * infoLog))                   \
    X(void, DeleteShader, (GLuint shader))                                                                             \
    X(GLuint, CreateProgram, (void))                                                                                   \
    X(void, AttachShader, (GLuint program, GLuint shader))                                                             \
    X(void, LinkProgram, (GLuint program))                                                                             \
    X(void, GetProgramiv, (GLuint program, GLenum pname, GLint * params))                                              \
    X(void, GetProgramInfoLog, (GLuint program, GLsizei bufSize, GLsizei * length, GLchar * infoLog))                 \
    X(void, DeleteProgram, (GLuint program))                                                                           \
    X(void, UseProgram, (GLuint program))                                                                              \
    X(GLint, GetUniformLocation, (GLuint program, const GLchar* name))                                                 \
    X(void, Uniform1i, (GLint location, GLint v0))                                                                     \
    X(void, Uniform1f, (GLint location, GLfloat v0))                                                                   \
    X(void, Uniform4f, (GLint location, GLfloat v0, GLfloat v1, GLfloat v2, GLfloat v3))                               \
    X(GLuint, GetUniformBlockIndex, (GLuint program, const GLchar* uniformBlockName))                                  \
    X(void, UniformBlockBinding, (GLuint program, GLuint uniformBlockIndex, GLuint uniformBlockBinding))               \
    X(void, GenFramebuffers, (GLsizei n, GLuint * framebuffers))                                                       \
    X(void, DeleteFramebuffers, (GLsizei n, const GLuint* framebuffers))                                               \
    X(void, BindFramebuffer, (GLenum target, GLuint framebuffer))                                                      \
    X(void, FramebufferTexture2D, (GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level))  \
    X(void, FramebufferRenderbuffer, (GLenum target, GLenum attachment, GLenum renderbuffertarget,                     \
                                      GLuint renderbuffer))                                                            \
    X(GLenum, CheckFramebufferStatus, (GLenum target))                                                                 \
    X(void, GenRenderbuffers, (GLsizei n, GLuint * renderbuffers))                                                     \
    X(void, DeleteRenderbuffers, (GLsizei n, const GLuint* renderbuffers))                                             \
    X(void, BindRenderbuffer, (GLenum target, GLuint renderbuffer))                                                    \
    X(void, RenderbufferStorage, (GLenum target, GLenum internalformat, GLsizei width, GLsizei height))               \
    X(void, DrawBuffers, (GLsizei n, const GLenum* bufs))

namespace Gl
{
#define LCE_GL_DECLARE(returnType, name, parameters)                                                                   \
    typedef returnType(LCE_GL_APIENTRY* name##Function) parameters;                                                    \
    extern name##Function name;
LCE_GL_FUNCTIONS(LCE_GL_DECLARE)
#undef LCE_GL_DECLARE

bool LoadFunctions(void* (*getProcAddress)(const char* name));
}

#endif
