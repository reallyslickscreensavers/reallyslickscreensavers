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

	// Resources
	int texturesGenerated = 0;
	int listsGenerated = 0;

	unsigned long long totalVertices() const;
	int netEnable(unsigned cap) const;
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

}  // namespace glstub

#endif
