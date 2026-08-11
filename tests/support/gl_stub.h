/*
 * Recording OpenGL stub.
 *
 * The savers cannot be unit tested against a real GL context in CI, but their
 * draw paths are worth executing: they are where refactors break things. This
 * stub resolves every gl/glu/wgl symbol they use and records what was called,
 * so tests can assert structural invariants instead of merely "it did not
 * crash".
 *
 * What it deliberately does NOT do is render. Nothing here can tell you a saver
 * looks right, only that it issued a coherent sequence of commands.
 */

#ifndef GL_STUB_H
#define GL_STUB_H

#include <cstddef>
#include <string>
#include <vector>

namespace glstub {

struct Primitive {
	unsigned mode;      // GL_TRIANGLES, GL_LINES, ...
	unsigned vertices;  // vertices emitted between glBegin and glEnd
};

struct Trace {
	// Every gl* call, in order, by name. For ordering assertions.
	std::vector<std::string> calls;

	// Matrix stack
	int matrixDepth = 0;
	int minMatrixDepth = 0;
	int maxMatrixDepth = 0;
	int pushes = 0;
	int pops = 0;

	// glBegin / glEnd
	std::vector<Primitive> primitives;
	int begins = 0;
	int ends = 0;
	bool insideBegin = false;
	bool nestedBeginSeen = false;   // glBegin inside glBegin: always a bug
	bool vertexOutsideBegin = false;

	// Net enable/disable per capability. Non-zero at the end of a frame means
	// the saver leaked GL state, which shows up as another saver rendering wrong.
	std::vector<std::pair<unsigned, int>> enables;

	// Every capability passed to glEnable, in order. `enables` nets out to zero
	// for a well-behaved frame, which cannot answer "was this branch taken at
	// all" - a bracketed glEnable/glDisable pair is invisible there.
	std::vector<unsigned> enabled;

	// Vertex-array drawing, kept apart from `primitives` because it does not go
	// through glBegin/glEnd and must not disturb the pairing counts. The
	// Implicit library draws its marching-cubes mesh this way, so helios and
	// microcosm put most of their geometry here rather than in `primitives`.
	std::vector<Primitive> arrayPrimitives;

	// Resources
	int texturesGenerated = 0;
	int listsGenerated = 0;

	// Readback calls handed an enum this stub does not answer, as
	// {function name, enum}. Real GL raises GL_INVALID_ENUM and leaves the
	// caller's buffer untouched, so whatever the caller does next is reading
	// uninitialised memory. lattice did exactly that - see test_lattice.cpp.
	std::vector<std::pair<std::string, unsigned>> invalidEnums;

	unsigned long long totalVertices() const;
	unsigned long long totalArrayVertices() const;
	int netEnable(unsigned cap) const;
	int countEnables(unsigned cap) const;
	int countCalls(const char* name) const;
	bool matrixBalanced() const { return matrixDepth == 0 && pushes == pops; }
	bool primitivesBalanced() const { return begins == ends && !insideBegin; }
};

// The single trace. Tests reset() before exercising a saver, then assert.
Trace& trace();
void reset();

// True when every recorded primitive has a vertex count legal for its mode:
// GL_TRIANGLES divisible by 3, GL_QUADS by 4, GL_LINES by 2, and so on.
// Reports the first offender in `why` when it fails.
bool primitiveVertexCountsLegal(std::string* why = nullptr);

// --- matrix state ----------------------------------------------------------
//
// The stub keeps real 4x4 stacks for GL_MODELVIEW, GL_PROJECTION and
// GL_TEXTURE, because hyperspace, lattice and skyrocket read the matrices back
// and feed them into arithmetic that decides where things land on screen.
// Returning zeros there produces NaN or a divide by zero, so "record the call
// and move on" is not enough for those three.
//
// This state is deliberately NOT cleared by reset(): reset() clears the
// recording, not the context. A saver builds its projection in initSaver and
// relies on it in every later draw(), exactly as it would against a real
// driver.

// Copies the top of `mode`'s stack, column-major as OpenGL stores it.
void currentMatrix(unsigned mode, float out[16]);

// Entries on `mode`'s stack. One means nothing has been pushed.
int matrixStackDepth(unsigned mode);

// Clears all three stacks back to a single identity. For tests that want to
// start from a known context rather than inherit one.
void resetMatrices();

}  // namespace glstub

#endif
