/*
 * Recording OpenGL stub — implementation.
 *
 * _GDI32_ is defined by the CMake target before Windows.h is reached. That is
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
#include <gl/GL.h>
#include <gl/GLU.h>

#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>

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
	// Setting GL_STUB_TRACE in the environment echoes every call to stderr as
	// it happens. The recorded trace is no help when a saver crashes mid-frame,
	// because the process dies before any assertion can read it; this shows
	// which call it got to. Off by default and checked once.
	//
	// GetEnvironmentVariable rather than getenv, which MSVC deprecates as
	// unsafe; Windows.h is already included and this is Windows-only code.
	if (static const bool echo = GetEnvironmentVariableA("GL_STUB_TRACE", nullptr, 0) != 0; echo) {
		std::fputs(name, stderr);
		std::fputc('\n', stderr);
		std::fflush(stderr);
	}
	trace().calls.emplace_back(name);
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

unsigned long long Trace::totalArrayVertices() const
{
	unsigned long long n = 0;
	for (const auto& p : arrayPrimitives) n += p.vertices;
	return n;
}

int Trace::netEnable(unsigned cap) const
{
	for (const auto& [capability, net] : enables) {
		if (capability == cap) return net;
	}
	return 0;
}

int Trace::countEnables(unsigned cap) const
{
	int n = 0;
	for (unsigned c : enabled) if (c == cap) n++;
	return n;
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

// ---------------------------------------------------------------------------
// Matrix stacks
//
// Fixed-function OpenGL, implemented for real rather than recorded. See the
// note in gl_stub.h for why three savers need this and why reset() leaves it
// alone.
//
// Storage is column-major exactly as OpenGL specifies: m[0..3] is the first
// column, so the translation lives at m[12..14].
// ---------------------------------------------------------------------------

// As with record() and bumpEnable() above, these sit in the namespace proper
// rather than an anonymous one so the extern "C" entry points can reach them.

struct Mat {
	float m[16];
};

// The project is C++17, so there is no <numbers> to take this from.
constexpr double kPi = 3.14159265358979323846;

Mat identityMat()
{
	Mat r = {};
	r.m[0] = 1.0f;
	r.m[5] = 1.0f;
	r.m[10] = 1.0f;
	r.m[15] = 1.0f;
	return r;
}

// c = a * b, the order glMultMatrixf applies: current = current * argument.
Mat multiply(const Mat& a, const Mat& b)
{
	Mat c = {};
	for (int col = 0; col < 4; ++col) {
		for (int row = 0; row < 4; ++row) {
			float sum = 0.0f;
			for (int k = 0; k < 4; ++k) {
				sum += a.m[row + 4 * k] * b.m[k + 4 * col];
			}
			c.m[row + 4 * col] = sum;
		}
	}
	return c;
}

struct State {
	std::vector<Mat> stacks[3];  // indexed by matrixIndex()
	int mode = 0;                // GL_MODELVIEW
	GLint viewport[4] = {0, 0, 1, 1};
};

State& state()
{
	static State instance = [] {
		State s;
		for (auto& stack : s.stacks) stack.push_back(identityMat());
		return s;
	}();
	return instance;
}

// GL_MODELVIEW is 0x1700, GL_PROJECTION 0x1701, GL_TEXTURE 0x1702. Anything
// else maps to modelview, which is what a driver would be left doing after
// rejecting the glMatrixMode call.
int matrixIndex(unsigned mode)
{
	switch (mode) {
		case GL_PROJECTION: return 1;
		case GL_TEXTURE:    return 2;
		default:            return 0;
	}
}

Mat& top() { return state().stacks[state().mode].back(); }

void applyToTop(const Mat& m) { top() = multiply(top(), m); }

void noteInvalidEnum(const char* fn, unsigned value)
{
	trace().invalidEnums.emplace_back(fn, value);
}

// Shared by glGetFloatv and glGetDoublev. Returns false when the enum is not
// one this stub answers, leaving it to the caller to record and write nothing.
bool readMatrix(unsigned pname, Mat* out)
{
	switch (pname) {
		case GL_MODELVIEW_MATRIX:  *out = state().stacks[0].back(); return true;
		case GL_PROJECTION_MATRIX: *out = state().stacks[1].back(); return true;
		case GL_TEXTURE_MATRIX:    *out = state().stacks[2].back(); return true;
		default:                   return false;
	}
}

// Degenerate arguments return identity rather than infinities. A saver that
// reaches one has a bug, but it should surface as a failed assertion on the
// call stream, not as NaN quietly poisoning every later number.

Mat rotation(float angleDegrees, float x, float y, float z)
{
	const float len = std::sqrt(x * x + y * y + z * z);
	if (len == 0.0f) return identityMat();
	x /= len;
	y /= len;
	z /= len;

	const float rad = angleDegrees * float(kPi) / 180.0f;
	const float c = std::cos(rad);
	const float s = std::sin(rad);
	const float t = 1.0f - c;

	Mat r = identityMat();
	r.m[0] = x * x * t + c;
	r.m[1] = y * x * t + z * s;
	r.m[2] = x * z * t - y * s;
	r.m[4] = x * y * t - z * s;
	r.m[5] = y * y * t + c;
	r.m[6] = y * z * t + x * s;
	r.m[8] = x * z * t + y * s;
	r.m[9] = y * z * t - x * s;
	r.m[10] = z * z * t + c;
	return r;
}

Mat orthoMat(double l, double r, double b, double t, double n, double f)
{
	if (r == l || t == b || f == n) return identityMat();
	Mat o = identityMat();
	o.m[0] = float(2.0 / (r - l));
	o.m[5] = float(2.0 / (t - b));
	o.m[10] = float(-2.0 / (f - n));
	o.m[12] = float(-(r + l) / (r - l));
	o.m[13] = float(-(t + b) / (t - b));
	o.m[14] = float(-(f + n) / (f - n));
	return o;
}

Mat frustumMat(double l, double r, double b, double t, double n, double f)
{
	if (r == l || t == b || f == n) return identityMat();
	Mat p = {};
	p.m[0] = float(2.0 * n / (r - l));
	p.m[5] = float(2.0 * n / (t - b));
	p.m[8] = float((r + l) / (r - l));
	p.m[9] = float((t + b) / (t - b));
	p.m[10] = float(-(f + n) / (f - n));
	p.m[11] = -1.0f;
	p.m[14] = float(-2.0 * f * n / (f - n));
	return p;
}

void pushCurrent()
{
	state().stacks[state().mode].push_back(top());
}

void popCurrent()
{
	auto& stack = state().stacks[state().mode];
	if (stack.size() > 1) stack.pop_back();
}

void currentMatrix(unsigned mode, float out[16])
{
	const Mat& m = state().stacks[matrixIndex(mode)].back();
	for (int i = 0; i < 16; ++i) out[i] = m.m[i];
}

int matrixStackDepth(unsigned mode)
{
	return static_cast<int>(state().stacks[matrixIndex(mode)].size());
}

void resetMatrices()
{
	for (auto& stack : state().stacks) {
		stack.clear();
		stack.push_back(identityMat());
	}
	state().mode = 0;
}

}  // namespace glstub

// ---------------------------------------------------------------------------
// Stubbed entry points. Signatures must match <gl/GL.h> exactly or the
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
	glstub::pushCurrent();
}

void APIENTRY glPopMatrix(void)
{
	REC(glPopMatrix);
	glstub::Trace& t = glstub::trace();
	t.matrixDepth--;
	t.pops++;
	if (t.matrixDepth < t.minMatrixDepth) t.minMatrixDepth = t.matrixDepth;
	// Real GL raises GL_STACK_UNDERFLOW and leaves the stack alone rather than
	// popping the last entry. minMatrixDepth already records that it happened.
	glstub::popCurrent();
}

void APIENTRY glEnable(GLenum cap)
{
	REC(glEnable);
	glstub::bumpEnable(cap, +1);
	glstub::trace().enabled.push_back(cap);
}

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
void APIENTRY glColor3fv(const GLfloat*)                     { REC(glColor3fv); }
void APIENTRY glColor4f(GLfloat, GLfloat, GLfloat, GLfloat)  { REC(glColor4f); }
void APIENTRY glColorMaterial(GLenum, GLenum)                { REC(glColorMaterial); }
void APIENTRY glEndList(void)                                { REC(glEndList); }
void APIENTRY glFrontFace(GLenum)                            { REC(glFrontFace); }
void APIENTRY glHint(GLenum, GLenum)                         { REC(glHint); }
void APIENTRY glLightfv(GLenum, GLenum, const GLfloat*)      { REC(glLightfv); }
void APIENTRY glLineWidth(GLfloat)                           { REC(glLineWidth); }
void APIENTRY glMaterialf(GLenum, GLenum, GLfloat)           { REC(glMaterialf); }
void APIENTRY glNewList(GLuint, GLenum)                      { REC(glNewList); }
void APIENTRY glPixelStorei(GLenum, GLint)                   { REC(glPixelStorei); }
void APIENTRY glPointSize(GLfloat)                           { REC(glPointSize); }
void APIENTRY glTexCoord2f(GLfloat, GLfloat)                 { REC(glTexCoord2f); }
void APIENTRY glTexEnvf(GLenum, GLenum, GLfloat)             { REC(glTexEnvf); }
void APIENTRY glTexEnvi(GLenum, GLenum, GLint)               { REC(glTexEnvi); }
void APIENTRY glTexImage2D(GLenum, GLint, GLint, GLsizei, GLsizei, GLint, GLenum, GLenum, const void*) { REC(glTexImage2D); }
void APIENTRY glTexParameteri(GLenum, GLenum, GLint)         { REC(glTexParameteri); }
void APIENTRY glTexSubImage2D(GLenum, GLint, GLint, GLint, GLsizei, GLsizei, GLenum, GLenum, const void*) { REC(glTexSubImage2D); }
void APIENTRY glBlendFunc(GLenum, GLenum)                    { REC(glBlendFunc); }
void APIENTRY glPushAttrib(GLbitfield)                       { REC(glPushAttrib); }
void APIENTRY glPopAttrib(void)                              { REC(glPopAttrib); }
void APIENTRY glBitmap(GLsizei, GLsizei, GLfloat, GLfloat, GLfloat, GLfloat, const GLubyte*) { REC(glBitmap); }

// Added for the second half of the rollout - flux, euphoria, helios, lattice,
// hyperspace, skyrocket and microcosm. All pure recorders; the ones that carry
// state are grouped separately below.
//
// SonarCloud reports cpp:S107 (too many parameters) on glCopyTexSubImage2D,
// glTexImage1D and gluProject. Those signatures are OpenGL's, not ours - a stub
// that took fewer arguments would not satisfy the calls it exists to satisfy.
// Left as findings rather than silenced, for the same reason as the S3630s
// further down.
void APIENTRY glClipPlane(GLenum, const GLdouble*)           { REC(glClipPlane); }
void APIENTRY glColor4fv(const GLfloat*)                     { REC(glColor4fv); }
void APIENTRY glCopyTexSubImage2D(GLenum, GLint, GLint, GLint, GLint, GLint, GLsizei, GLsizei) { REC(glCopyTexSubImage2D); }
void APIENTRY glCullFace(GLenum)                             { REC(glCullFace); }
void APIENTRY glDrawPixels(GLsizei, GLsizei, GLenum, GLenum, const void*) { REC(glDrawPixels); }
void APIENTRY glFlush(void)                                  { REC(glFlush); }
void APIENTRY glFogf(GLenum, GLfloat)                        { REC(glFogf); }
void APIENTRY glFogfv(GLenum, const GLfloat*)                { REC(glFogfv); }
void APIENTRY glLightModelfv(GLenum, const GLfloat*)         { REC(glLightModelfv); }
void APIENTRY glLightModeli(GLenum, GLint)                   { REC(glLightModeli); }
void APIENTRY glMaterialfv(GLenum, GLenum, const GLfloat*)   { REC(glMaterialfv); }
void APIENTRY glNormal3f(GLfloat, GLfloat, GLfloat)          { REC(glNormal3f); }
void APIENTRY glNormal3fv(const GLfloat*)                    { REC(glNormal3fv); }
void APIENTRY glRasterPos2i(GLint, GLint)                    { REC(glRasterPos2i); }
void APIENTRY glReadBuffer(GLenum)                           { REC(glReadBuffer); }
void APIENTRY glReadPixels(GLint, GLint, GLsizei, GLsizei, GLenum, GLenum, void*) { REC(glReadPixels); }
void APIENTRY glShadeModel(GLenum)                           { REC(glShadeModel); }
void APIENTRY glTexCoord2d(GLdouble, GLdouble)               { REC(glTexCoord2d); }
void APIENTRY glTexCoord2fv(const GLfloat*)                  { REC(glTexCoord2fv); }
void APIENTRY glTexGenfv(GLenum, GLenum, const GLfloat*)     { REC(glTexGenfv); }
void APIENTRY glTexGeni(GLenum, GLenum, GLint)               { REC(glTexGeni); }
void APIENTRY glTexImage1D(GLenum, GLint, GLint, GLsizei, GLint, GLenum, GLenum, const void*) { REC(glTexImage1D); }
void APIENTRY glTexSubImage1D(GLenum, GLint, GLint, GLsizei, GLenum, GLenum, const void*) { REC(glTexSubImage1D); }

// --- vertex arrays ----------------------------------------------------------
//
// libs/Implicit draws its marching-cubes mesh with glDrawElements rather than
// glBegin/glEnd, so helios and microcosm put most of their geometry through
// here. Recorded into arrayPrimitives so it stays out of the begin/end pairing
// counts while still being countable.

void APIENTRY glEnableClientState(GLenum)                    { REC(glEnableClientState); }
void APIENTRY glDisableClientState(GLenum)                   { REC(glDisableClientState); }
void APIENTRY glVertexPointer(GLint, GLenum, GLsizei, const void*)   { REC(glVertexPointer); }
void APIENTRY glNormalPointer(GLenum, GLsizei, const void*)          { REC(glNormalPointer); }
void APIENTRY glTexCoordPointer(GLint, GLenum, GLsizei, const void*) { REC(glTexCoordPointer); }
void APIENTRY glColorPointer(GLint, GLenum, GLsizei, const void*)    { REC(glColorPointer); }
void APIENTRY glInterleavedArrays(GLenum, GLsizei, const void*)      { REC(glInterleavedArrays); }

void APIENTRY glDrawElements(GLenum mode, GLsizei count, GLenum, const void*)
{
	REC(glDrawElements);
	glstub::Primitive p;
	p.mode = mode;
	p.vertices = unsigned(count < 0 ? 0 : count);
	glstub::trace().arrayPrimitives.push_back(p);
}

void APIENTRY glDrawArrays(GLenum mode, GLint, GLsizei count)
{
	REC(glDrawArrays);
	glstub::Primitive p;
	p.mode = mode;
	p.vertices = unsigned(count < 0 ? 0 : count);
	glstub::trace().arrayPrimitives.push_back(p);
}

// --- state the stub actually keeps -----------------------------------------

void APIENTRY glViewport(GLint x, GLint y, GLsizei width, GLsizei height)
{
	REC(glViewport);
	GLint* vp = glstub::state().viewport;
	vp[0] = x;
	vp[1] = y;
	vp[2] = width;
	vp[3] = height;
}

void APIENTRY glMatrixMode(GLenum mode)
{
	REC(glMatrixMode);
	glstub::state().mode = glstub::matrixIndex(mode);
}

void APIENTRY glLoadIdentity(void)
{
	REC(glLoadIdentity);
	glstub::top() = glstub::identityMat();
}

void APIENTRY glLoadMatrixf(const GLfloat* m)
{
	REC(glLoadMatrixf);
	for (int i = 0; i < 16; ++i) glstub::top().m[i] = m[i];
}

void APIENTRY glMultMatrixf(const GLfloat* m)
{
	REC(glMultMatrixf);
	glstub::Mat arg;
	for (int i = 0; i < 16; ++i) arg.m[i] = m[i];
	glstub::applyToTop(arg);
}

void APIENTRY glTranslatef(GLfloat x, GLfloat y, GLfloat z)
{
	REC(glTranslatef);
	glstub::Mat t = glstub::identityMat();
	t.m[12] = x;
	t.m[13] = y;
	t.m[14] = z;
	glstub::applyToTop(t);
}

void APIENTRY glScalef(GLfloat x, GLfloat y, GLfloat z)
{
	REC(glScalef);
	glstub::Mat s = glstub::identityMat();
	s.m[0] = x;
	s.m[5] = y;
	s.m[10] = z;
	glstub::applyToTop(s);
}

void APIENTRY glRotatef(GLfloat angle, GLfloat x, GLfloat y, GLfloat z)
{
	REC(glRotatef);
	glstub::applyToTop(glstub::rotation(angle, x, y, z));
}

void APIENTRY glOrtho(GLdouble left, GLdouble right, GLdouble bottom,
                      GLdouble top, GLdouble zNear, GLdouble zFar)
{
	REC(glOrtho);
	glstub::applyToTop(glstub::orthoMat(left, right, bottom, top, zNear, zFar));
}

void APIENTRY glFrustum(GLdouble left, GLdouble right, GLdouble bottom,
                        GLdouble top, GLdouble zNear, GLdouble zFar)
{
	REC(glFrustum);
	glstub::applyToTop(glstub::frustumMat(left, right, bottom, top, zNear, zFar));
}

// --- readback ---------------------------------------------------------------
//
// An enum this stub does not answer is recorded and the caller's buffer is left
// untouched, which is what a real driver does after raising GL_INVALID_ENUM.
// Tests assert on trace().invalidEnums rather than on whatever garbage the
// caller then reads.

void APIENTRY glGetFloatv(GLenum pname, GLfloat* params)
{
	REC(glGetFloatv);
	if (glstub::Mat m; glstub::readMatrix(pname, &m)) {
		for (int i = 0; i < 16; ++i) params[i] = m.m[i];
		return;
	}
	if (pname == GL_VIEWPORT) {
		for (int i = 0; i < 4; ++i) params[i] = float(glstub::state().viewport[i]);
		return;
	}
	glstub::noteInvalidEnum("glGetFloatv", pname);
}

void APIENTRY glGetDoublev(GLenum pname, GLdouble* params)
{
	REC(glGetDoublev);
	if (glstub::Mat m; glstub::readMatrix(pname, &m)) {
		for (int i = 0; i < 16; ++i) params[i] = double(m.m[i]);
		return;
	}
	if (pname == GL_VIEWPORT) {
		for (int i = 0; i < 4; ++i) params[i] = double(glstub::state().viewport[i]);
		return;
	}
	glstub::noteInvalidEnum("glGetDoublev", pname);
}

void APIENTRY glGetIntegerv(GLenum pname, GLint* params)
{
	REC(glGetIntegerv);
	if (pname == GL_VIEWPORT) {
		for (int i = 0; i < 4; ++i) params[i] = glstub::state().viewport[i];
		return;
	}
	if (glstub::Mat m; glstub::readMatrix(pname, &m)) {
		for (int i = 0; i < 16; ++i) params[i] = GLint(m.m[i]);
		return;
	}
	glstub::noteInvalidEnum("glGetIntegerv", pname);
}

const GLubyte* APIENTRY glGetString(GLenum name)
{
	REC(glGetString);
	// The extension string decides which path hyperspace and microcosm take,
	// and the three below are the ones their initExtensions asks for
	// (hyperspace/extensions.cpp:76, microcosm/extensions.cpp:76).
	//
	// Advertising them rather than reporting none is deliberate. Every GPU
	// since about 2002 has all three, so the shader path is the one that
	// actually ships - and the fallback is not merely less covered but broken:
	// hyperspace's draw() calls glActiveTextureARB unconditionally at
	// hyperspace.cpp:231-235, outside the if(dShaders) guard around every other
	// use, so with the pointers left null it crashes on its first frame. See
	// docs/MAINTENANCE.md.
	switch (name) {
		case GL_VENDOR:   return reinterpret_cast<const GLubyte*>("Really Slick Screensavers tests");
		case GL_RENDERER: return reinterpret_cast<const GLubyte*>("gl_stub");
		case GL_VERSION:  return reinterpret_cast<const GLubyte*>("1.3.0");
		case GL_EXTENSIONS:
			return reinterpret_cast<const GLubyte*>(
			    "GL_ARB_multitexture GL_ARB_texture_cube_map GL_ARB_shader_objects");
		default: return reinterpret_cast<const GLubyte*>("");
	}
}

// --- ARB entry points, reached only through wglGetProcAddress ---------------
//
// These are not linked against; the savers hold them as function pointers that
// their own extensions.cpp fills in. Shaders never compile here - the stub does
// no rendering - but the handles have to be non-zero, because a saver that got
// zero back would conclude the driver had failed.

void APIENTRY glActiveTextureARB(GLenum)                  { REC(glActiveTextureARB); }
void APIENTRY glCompileShaderARB(GLuint)                  { REC(glCompileShaderARB); }
void APIENTRY glAttachObjectARB(GLuint, GLuint)           { REC(glAttachObjectARB); }
void APIENTRY glLinkProgramARB(GLuint)                    { REC(glLinkProgramARB); }
void APIENTRY glUseProgramObjectARB(GLuint)               { REC(glUseProgramObjectARB); }
void APIENTRY glUniform1iARB(GLint, GLint)                { REC(glUniform1iARB); }
void APIENTRY glUniform3fARB(GLint, GLfloat, GLfloat, GLfloat) { REC(glUniform3fARB); }
void APIENTRY glShaderSourceARB(GLuint, GLsizei, const char**, const GLint*) { REC(glShaderSourceARB); }

GLuint APIENTRY glCreateShaderObjectARB(GLenum)
{
	REC(glCreateShaderObjectARB);
	static GLuint next = 1;
	return next++;
}

GLuint APIENTRY glCreateProgramObjectARB(void)
{
	REC(glCreateProgramObjectARB);
	static GLuint next = 1000;
	return next++;
}

GLint APIENTRY glGetUniformLocationARB(GLuint, const char*)
{
	REC(glGetUniformLocationARB);
	// Zero is a perfectly good uniform location; -1 would mean "not found".
	return 0;
}

BOOL WINAPI wglSwapIntervalEXT(int)
{
	REC(wglSwapIntervalEXT);
	return TRUE;
}

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

void APIENTRY gluDeleteQuadric(GLUquadric*)                  { REC(gluDeleteQuadric); }
void APIENTRY gluSphere(GLUquadric*, GLdouble, GLint, GLint) { REC(gluSphere); }

GLint APIENTRY gluBuild2DMipmaps(GLenum, GLint, GLsizei, GLsizei, GLenum, GLenum, const void*)
{
	REC(gluBuild2DMipmaps);
	return 0;  // GLU_NO_ERROR
}

GLint APIENTRY gluBuild1DMipmaps(GLenum, GLint, GLsizei, GLenum, GLenum, const void*)
{
	REC(gluBuild1DMipmaps);
	return 0;  // GLU_NO_ERROR
}

void APIENTRY gluPerspective(GLdouble fovy, GLdouble aspect, GLdouble zNear, GLdouble zFar)
{
	REC(gluPerspective);
	const double halfHeight = zNear * std::tan(fovy * glstub::kPi / 360.0);
	const double halfWidth = halfHeight * aspect;
	glstub::applyToTop(glstub::frustumMat(-halfWidth, halfWidth,
	                                      -halfHeight, halfHeight, zNear, zFar));
}

void APIENTRY gluOrtho2D(GLdouble left, GLdouble right, GLdouble bottom, GLdouble top)
{
	REC(gluOrtho2D);
	glstub::applyToTop(glstub::orthoMat(left, right, bottom, top, -1.0, 1.0));
}

// Object space -> window coordinates, the standard transform.
//
// Deliberate deviation from the spec: when the clip-space w is zero this
// writes zeros as well as returning GL_FALSE, where real GLU leaves the
// caller's buffers untouched. Neither call site in the tree checks the return
// value - skyrocket.cpp:341 and hyperspace/flare.cpp:187 both divide the
// outputs by the window size on the next line - so leaving them untouched would
// feed uninitialised stack into a float and make the tests flaky rather than
// wrong. Zero is at least deterministic.
GLint APIENTRY gluProject(GLdouble objx, GLdouble objy, GLdouble objz,
                          const GLdouble modelMatrix[16],
                          const GLdouble projMatrix[16],
                          const GLint viewport[4],
                          GLdouble* winx, GLdouble* winy, GLdouble* winz)
{
	REC(gluProject);

	const double in[4] = {objx, objy, objz, 1.0};

	double eye[4] = {0.0, 0.0, 0.0, 0.0};
	for (int row = 0; row < 4; ++row) {
		for (int k = 0; k < 4; ++k) eye[row] += modelMatrix[row + 4 * k] * in[k];
	}

	double clip[4] = {0.0, 0.0, 0.0, 0.0};
	for (int row = 0; row < 4; ++row) {
		for (int k = 0; k < 4; ++k) clip[row] += projMatrix[row + 4 * k] * eye[k];
	}

	if (clip[3] == 0.0) {
		*winx = 0.0;
		*winy = 0.0;
		*winz = 0.0;
		return GL_FALSE;
	}

	clip[0] /= clip[3];
	clip[1] /= clip[3];
	clip[2] /= clip[3];

	*winx = viewport[0] + viewport[2] * (clip[0] + 1.0) * 0.5;
	*winy = viewport[1] + viewport[3] * (clip[1] + 1.0) * 0.5;
	*winz = (clip[2] + 1.0) * 0.5;
	return GL_TRUE;
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

PROC WINAPI wglGetProcAddress(LPCSTR name)
{
	REC(wglGetProcAddress);

	// Resolves the entry points for the extensions glGetString advertises, and
	// nothing else. An unknown name still gets nullptr, which is what a driver
	// does and what every call site here null-checks.
	//
	// SonarCloud reports cpp:S3630 on every entry below and there is nothing to
	// do about it: wglGetProcAddress returns PROC, so handing back a function
	// pointer means converting between function pointer types, and
	// reinterpret_cast is the only cast in C++ that does that. static_cast and
	// friends do not apply. Left as findings rather than silenced.
	//
	// The alternative - answering nullptr for everything - is not the safe
	// default it looks like: hyperspace calls glActiveTextureARB outside its
	// own dShaders guard and would call through a null pointer on the first
	// frame. See the note on glGetString above.
	struct Entry {
		const char* name;
		PROC address;
	};
	static const Entry table[] = {
	    {"glActiveTextureARB",       reinterpret_cast<PROC>(glActiveTextureARB)},
	    {"glCreateShaderObjectARB",  reinterpret_cast<PROC>(glCreateShaderObjectARB)},
	    {"glShaderSourceARB",        reinterpret_cast<PROC>(glShaderSourceARB)},
	    {"glCompileShaderARB",       reinterpret_cast<PROC>(glCompileShaderARB)},
	    {"glCreateProgramObjectARB", reinterpret_cast<PROC>(glCreateProgramObjectARB)},
	    {"glAttachObjectARB",        reinterpret_cast<PROC>(glAttachObjectARB)},
	    {"glLinkProgramARB",         reinterpret_cast<PROC>(glLinkProgramARB)},
	    {"glUseProgramObjectARB",    reinterpret_cast<PROC>(glUseProgramObjectARB)},
	    {"glGetUniformLocationARB",  reinterpret_cast<PROC>(glGetUniformLocationARB)},
	    {"glUniform1iARB",           reinterpret_cast<PROC>(glUniform1iARB)},
	    {"glUniform3fARB",           reinterpret_cast<PROC>(glUniform3fARB)},
	    {"wglSwapIntervalEXT",       reinterpret_cast<PROC>(wglSwapIntervalEXT)},
	};

	if (name != nullptr) {
		for (const Entry& entry : table) {
			if (std::strcmp(entry.name, name) == 0) return entry.address;
		}
	}
	return nullptr;
}

}  // extern "C"
