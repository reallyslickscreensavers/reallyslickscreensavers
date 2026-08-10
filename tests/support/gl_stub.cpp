/*
 * Recording OpenGL stub — implementation.
 *
 * _GDI32_ is defined by the CMake target before windows.h is reached. That is
 * the switch wingdi.h itself uses:
 *
 *     #if !defined(_GDI32_)
 *     #define WINGDIAPI DECLSPEC_IMPORT
 *     #else
 *     #define WINGDIAPI
 *     #endif
 *
 * Without it every gl* function is declared __declspec(dllimport), calls go
 * indirect through __imp__gl*, and these definitions would satisfy nothing —
 * you cannot define a dllimport function. GLU needs no such treatment; its
 * header declares plain APIENTRY functions.
 *
 * opengl32.lib and glu32.lib are deliberately not linked.
 */

#include <Windows.h>
#include <GL/gl.h>
#include <GL/glu.h>

#include "gl_stub.h"

namespace glstub {

// Function-local rather than a namespace-scope global: one mutable object is
// unavoidable for a recorder, but it need not be visible to anything else.
Trace& trace()
{
	static Trace instance;
	return instance;
}

void reset() { trace() = Trace(); }

// Helpers live in the namespace proper, not an anonymous one, so the extern "C"
// entry points below can reach them.
void record(const char* name)
{
	trace().calls.push_back(name);
}

void bumpEnable(unsigned cap, int delta)
{
	for (auto& [capability, net] : trace().enables) {
		if (capability == cap) { net += delta; return; }
	}
	trace().enables.emplace_back(cap, delta);
}

void countVertex()
{
	if (!trace().insideBegin || trace().primitives.empty()) {
		trace().vertexOutsideBegin = true;
		return;
	}
	trace().primitives.back().vertices++;
}

unsigned long long Trace::totalVertices() const
{
	unsigned long long n = 0;
	for (const auto& p : primitives) n += p.vertices;
	return n;
}

int Trace::netEnable(unsigned cap) const
{
	for (const auto& [capability, net] : enables) {
		if (capability == cap) return net;
	}
	return 0;
}

int Trace::countCalls(const char* name) const
{
	int n = 0;
	for (const auto& c : calls) if (c == name) n++;
	return n;
}

bool primitiveVertexCountsLegal(std::string* why)
{
	for (size_t i = 0; i < trace().primitives.size(); ++i) {
		const Primitive& p = trace().primitives[i];
		unsigned need = 0;
		unsigned mult = 1;
		switch (p.mode) {
			case GL_POINTS:                               need = 1; mult = 1; break;
			case GL_LINES:                                need = 2; mult = 2; break;
			case GL_TRIANGLES:                            need = 3; mult = 3; break;
			case GL_QUADS:                                need = 4; mult = 4; break;
			case GL_LINE_STRIP:     case GL_LINE_LOOP:    need = 2; mult = 1; break;
			case GL_TRIANGLE_STRIP: case GL_TRIANGLE_FAN: need = 3; mult = 1; break;
			case GL_QUAD_STRIP:                           need = 4; mult = 2; break;
			case GL_POLYGON:                              need = 3; mult = 1; break;
			default:                                      need = 0; mult = 1; break;
		}
		// An empty block is legal: savers routinely begin/end with nothing to draw.
		if (p.vertices == 0) continue;
		if (p.vertices < need || (mult > 1 && p.vertices % mult != 0)) {
			if (why) {
				*why = "primitive #" + std::to_string(i) + " mode " +
				       std::to_string(p.mode) + " has " +
				       std::to_string(p.vertices) + " vertices";
			}
			return false;
		}
	}
	return true;
}

}  // namespace glstub

// ---------------------------------------------------------------------------
// Stubbed entry points. Signatures must match <GL/gl.h> exactly or the
// __stdcall decoration differs and the link fails — a useful self-check.
// ---------------------------------------------------------------------------

#define REC(name) glstub::record(#name)

extern "C" {

void APIENTRY glBegin(GLenum mode)
{
	REC(glBegin);
	glstub::Trace& t = glstub::trace();
	if (t.insideBegin) t.nestedBeginSeen = true;
	t.insideBegin = true;
	t.begins++;
	glstub::Primitive p;
	p.mode = mode;
	p.vertices = 0;
	t.primitives.push_back(p);
}

void APIENTRY glEnd(void)
{
	REC(glEnd);
	glstub::Trace& t = glstub::trace();
	t.insideBegin = false;
	t.ends++;
}

void APIENTRY glPushMatrix(void)
{
	REC(glPushMatrix);
	glstub::Trace& t = glstub::trace();
	t.matrixDepth++;
	t.pushes++;
	if (t.matrixDepth > t.maxMatrixDepth) t.maxMatrixDepth = t.matrixDepth;
}

void APIENTRY glPopMatrix(void)
{
	REC(glPopMatrix);
	glstub::Trace& t = glstub::trace();
	t.matrixDepth--;
	t.pops++;
	if (t.matrixDepth < t.minMatrixDepth) t.minMatrixDepth = t.matrixDepth;
}

void APIENTRY glEnable(GLenum cap)  { REC(glEnable);  glstub::bumpEnable(cap, +1); }
void APIENTRY glDisable(GLenum cap) { REC(glDisable); glstub::bumpEnable(cap, -1); }

void APIENTRY glVertex2f(GLfloat, GLfloat)          { REC(glVertex2f);  glstub::countVertex(); }
void APIENTRY glVertex3f(GLfloat, GLfloat, GLfloat) { REC(glVertex3f);  glstub::countVertex(); }
void APIENTRY glVertex3fv(const GLfloat*)           { REC(glVertex3fv); glstub::countVertex(); }

void APIENTRY glGenTextures(GLsizei n, GLuint* textures)
{
	REC(glGenTextures);
	static GLuint next = 1;
	for (GLsizei i = 0; i < n; ++i) textures[i] = next++;
	glstub::trace().texturesGenerated += n;
}

GLuint APIENTRY glGenLists(GLsizei range)
{
	REC(glGenLists);
	static GLuint next = 1;
	GLuint base = next;
	next += (range > 0 ? range : 1);
	glstub::trace().listsGenerated += range;
	return base;  // must be non-zero; 0 means failure to the caller
}

void APIENTRY glBindTexture(GLenum, GLuint)                  { REC(glBindTexture); }
void APIENTRY glCallList(GLuint)                             { REC(glCallList); }
void APIENTRY glClear(GLbitfield)                            { REC(glClear); }
void APIENTRY glClearColor(GLclampf, GLclampf, GLclampf, GLclampf) { REC(glClearColor); }
void APIENTRY glColor3f(GLfloat, GLfloat, GLfloat)           { REC(glColor3f); }
void APIENTRY glColorMaterial(GLenum, GLenum)                { REC(glColorMaterial); }
void APIENTRY glEndList(void)                                { REC(glEndList); }
void APIENTRY glFrontFace(GLenum)                            { REC(glFrontFace); }
void APIENTRY glHint(GLenum, GLenum)                         { REC(glHint); }
void APIENTRY glLightfv(GLenum, GLenum, const GLfloat*)      { REC(glLightfv); }
void APIENTRY glLineWidth(GLfloat)                           { REC(glLineWidth); }
void APIENTRY glLoadIdentity(void)                           { REC(glLoadIdentity); }
void APIENTRY glMaterialf(GLenum, GLenum, GLfloat)           { REC(glMaterialf); }
void APIENTRY glMatrixMode(GLenum)                           { REC(glMatrixMode); }
void APIENTRY glNewList(GLuint, GLenum)                      { REC(glNewList); }
void APIENTRY glOrtho(GLdouble, GLdouble, GLdouble, GLdouble, GLdouble, GLdouble) { REC(glOrtho); }
void APIENTRY glPixelStorei(GLenum, GLint)                   { REC(glPixelStorei); }
void APIENTRY glPointSize(GLfloat)                           { REC(glPointSize); }
void APIENTRY glRotatef(GLfloat, GLfloat, GLfloat, GLfloat)  { REC(glRotatef); }
void APIENTRY glScalef(GLfloat, GLfloat, GLfloat)            { REC(glScalef); }
void APIENTRY glTexCoord2f(GLfloat, GLfloat)                 { REC(glTexCoord2f); }
void APIENTRY glTexEnvf(GLenum, GLenum, GLfloat)             { REC(glTexEnvf); }
void APIENTRY glTexEnvi(GLenum, GLenum, GLint)               { REC(glTexEnvi); }
void APIENTRY glTexImage2D(GLenum, GLint, GLint, GLsizei, GLsizei, GLint, GLenum, GLenum, const void*) { REC(glTexImage2D); }
void APIENTRY glTexParameteri(GLenum, GLenum, GLint)         { REC(glTexParameteri); }
void APIENTRY glTexSubImage2D(GLenum, GLint, GLint, GLint, GLsizei, GLsizei, GLenum, GLenum, const void*) { REC(glTexSubImage2D); }
void APIENTRY glTranslatef(GLfloat, GLfloat, GLfloat)        { REC(glTranslatef); }
void APIENTRY glViewport(GLint, GLint, GLsizei, GLsizei)     { REC(glViewport); }
void APIENTRY glBlendFunc(GLenum, GLenum)                    { REC(glBlendFunc); }
void APIENTRY glPushAttrib(GLbitfield)                       { REC(glPushAttrib); }
void APIENTRY glPopAttrib(void)                              { REC(glPopAttrib); }
void APIENTRY glBitmap(GLsizei, GLsizei, GLfloat, GLfloat, GLfloat, GLfloat, const GLubyte*) { REC(glBitmap); }

}  // extern "C"

// --- GLU: its header declares plain APIENTRY functions, no dllimport --------

GLUquadric* APIENTRY gluNewQuadric(void)
{
	REC(gluNewQuadric);
	// GLUquadric is opaque in glu.h, so there is nothing to construct. The
	// handle is never dereferenced - gluSphere below ignores it - it only has to
	// be non-null and stable, which a function-local static gives us.
	static int quadric = 0;
	return static_cast<GLUquadric*>(static_cast<void*>(&quadric));
}

void APIENTRY gluDeleteQuadric(GLUquadric*)                          { REC(gluDeleteQuadric); }
void APIENTRY gluSphere(GLUquadric*, GLdouble, GLint, GLint)         { REC(gluSphere); }
void APIENTRY gluPerspective(GLdouble, GLdouble, GLdouble, GLdouble) { REC(gluPerspective); }
void APIENTRY gluOrtho2D(GLdouble, GLdouble, GLdouble, GLdouble)     { REC(gluOrtho2D); }

GLint APIENTRY gluBuild2DMipmaps(GLenum, GLint, GLsizei, GLsizei, GLenum, GLenum, const void*)
{
	REC(gluBuild2DMipmaps);
	return 0;  // GLU_NO_ERROR
}

// --- WGL -------------------------------------------------------------------

extern "C" {

HGLRC WINAPI wglCreateContext(HDC)
{
	REC(wglCreateContext);
	// DECLARE_HANDLE makes HGLRC a pointer to a real (if trivial) struct, so a
	// function-local instance gives a valid non-null handle with no cast at all.
	static HGLRC__ context = {};
	return &context;
}

BOOL WINAPI wglDeleteContext(HGLRC)        { REC(wglDeleteContext);    return TRUE; }
BOOL WINAPI wglMakeCurrent(HDC, HGLRC)     { REC(wglMakeCurrent);      return TRUE; }
BOOL WINAPI wglSwapLayerBuffers(HDC, UINT) { REC(wglSwapLayerBuffers); return TRUE; }

PROC WINAPI wglGetProcAddress(LPCSTR)
{
	REC(wglGetProcAddress);
	// nullptr means "extension unavailable", which every call site null-checks.
	// That exercises the fallback path rather than crashing.
	return nullptr;
}

}  // extern "C"
