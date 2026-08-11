/*
 * Shared scaffolding for the per-saver test binaries.
 *
 * Every saver is tested the same way - bring it up, draw a frame, assert the
 * command stream is coherent, then drive the dialog procedures - so without
 * this each suite repeated the same fixture and the same six invariant blocks.
 * With six savers that was already SonarCloud's duplication gate; with the
 * remaining seven it would have been twice as much.
 *
 * What stays in each suite is what actually differs: the settings, what the
 * saver is supposed to draw, and anything peculiar to it.
 */

#ifndef SAVER_TEST_COMMON_H
#define SAVER_TEST_COMMON_H

#include <gtest/gtest.h>

#include <Windows.h>

// Included once here rather than in each suite: every one needs the GL_* enums
// to name primitives, and SonarCloud cpp:S3806 fires on the spelling whichever
// case is used, because the tree has a 3rdparty/GL directory as well as the
// SDK's gl/GL.h. One finding is better than six.
#include <gl/GL.h>

#include "gl_stub.h"
#include "test_window.h"

// Declared by every saver with identical signatures, so this header can call
// them and each test binary links its own. setDefaults is deliberately absent:
// solarWinds takes a preset argument while the others take none.
void draw();
void idleProc();
void initSaver(HWND hwnd);
void cleanUp(HWND hwnd);

namespace savertest {

// Base fixture: brings the saver up, discards the first frame, and guarantees
// teardown.
//
// The warm-up matters because ctest runs every case in its own process, so
// anything a cold frame does once - lazy resource construction, first-call
// statics - would otherwise make a test pass in suite order and fail alone.
//
// Settings must only change while nothing is allocated, which is why stopping
// is a separate step rather than a restart helper. Some savers size their frees
// from the current globals rather than from what they allocated, so raising a
// count and then calling cleanUp walks off the end of an array; solarWinds does
// exactly that and blocks inside the heap. Always: stop, change, start.
class SaverFixture : public ::testing::Test {
protected:
    void TearDown() override { stop(); }

    void start() {
        initSaver(testsupport::hostWindow());
        started_ = true;
        draw();               // warm-up, discarded
        glstub::reset();
    }

    // Like start(), but leaves the trace holding what initSaver emitted instead
    // of clearing it, and draws no warm-up frame.
    //
    // Several savers build their geometry into display lists at startup and
    // only call those lists per frame, so setup is the one place those vertices
    // exist. Same for one-off state like fog parameters. Assertions about
    // either are impossible after start() has reset the trace.
    void startCapturingSetup() {
        glstub::reset();
        initSaver(testsupport::hostWindow());
        started_ = true;
    }

    void stop() {
        if (started_) {
            cleanUp(testsupport::hostWindow());
            started_ = false;
        }
    }

    static HWND hostWindow() { return testsupport::hostWindow(); }

    static int countPrimitives(unsigned mode) {
        int n = 0;
        for (const auto& p : glstub::trace().primitives) {
            if (p.mode == mode) n++;
        }
        return n;
    }

private:
    bool started_ = false;
};

// --- frame invariants ------------------------------------------------------
//
// AssertionResult rather than void so a failure is reported at the EXPECT_TRUE
// call site in the suite, not inside this header.

inline ::testing::AssertionResult MatrixStackBalanced() {
    const glstub::Trace& t = glstub::trace();
    if (t.matrixBalanced() && t.minMatrixDepth >= 0) {
        return ::testing::AssertionSuccess();
    }
    return ::testing::AssertionFailure()
        << "depth " << t.matrixDepth << ", " << t.pushes << " pushes vs "
        << t.pops << " pops, lowest depth " << t.minMatrixDepth;
}

inline ::testing::AssertionResult PrimitivesPaired() {
    const glstub::Trace& t = glstub::trace();
    if (t.nestedBeginSeen) {
        return ::testing::AssertionFailure() << "glBegin inside glBegin";
    }
    if (t.vertexOutsideBegin) {
        return ::testing::AssertionFailure() << "vertex emitted outside a glBegin block";
    }
    if (!t.primitivesBalanced()) {
        return ::testing::AssertionFailure()
            << t.begins << " glBegin vs " << t.ends << " glEnd";
    }
    return ::testing::AssertionSuccess();
}

inline ::testing::AssertionResult VertexCountsLegal() {
    std::string why;
    if (glstub::primitiveVertexCountsLegal(&why)) {
        return ::testing::AssertionSuccess();
    }
    return ::testing::AssertionFailure() << why;
}

// Every readback the stub could not answer. In a real driver each of these is
// GL_INVALID_ENUM and the caller's buffer is left holding whatever was on the
// stack, so a hit here means the saver is computing with uninitialised memory.
inline ::testing::AssertionResult NoInvalidEnums() {
    const glstub::Trace& t = glstub::trace();
    if (t.invalidEnums.empty()) {
        return ::testing::AssertionSuccess();
    }
    ::testing::AssertionResult failure = ::testing::AssertionFailure();
    for (const auto& [function, value] : t.invalidEnums) {
        failure << function << " was handed enum 0x" << std::hex << value << std::dec << "; ";
    }
    return failure;
}

// Anything enabled during a frame must be disabled again by the end of it.
// A leak here shows up as some later frame rendering wrong.
inline ::testing::AssertionResult NoEnableStateLeaked() {
    for (const auto& [capability, net] : glstub::trace().enables) {
        if (net != 0) {
            return ::testing::AssertionFailure()
                << "capability " << capability << " left with net enable " << net;
        }
    }
    return ::testing::AssertionSuccess();
}

// --- dialog procedures -----------------------------------------------------
//
// A dialog procedure is an ordinary message handler, so it can be driven
// directly. With a null HWND, GetDlgItem and SendDlgItemMessage return null or
// fail harmlessly, which is enough to walk the switch arms.
//
// IDOK is never sent by any of these: it calls writeRegistry and would rewrite
// the developer's real saver settings.

// Templated on the procedure rather than taking a function pointer: every
// caller passes a plain saver entry point, so the call is direct and there is
// nothing to indirect through.

// WM_CTLCOLORSTATIC returns a brush through an INT_PTR - the truncation PR #39
// fixed. lParam 0 matches GetDlgItem's null result so the branch is taken.
template <typename Proc>
::testing::AssertionResult AboutProcColoursTheWebPageLabel(Proc aboutProc) {
    if (aboutProc(nullptr, WM_CTLCOLORSTATIC, 0, 0) != 0) {
        return ::testing::AssertionSuccess();
    }
    return ::testing::AssertionFailure() << "expected a brush, not a fall-through";
}

template <typename Proc>
::testing::AssertionResult IgnoresUnhandledMessages(Proc proc) {
    if (proc(nullptr, WM_MOUSEMOVE, 0, 0) == FALSE) {
        return ::testing::AssertionSuccess();
    }
    return ::testing::AssertionFailure() << "an unhandled message should report FALSE";
}

template <typename Proc>
::testing::AssertionResult ConfigureDialogInitialisesAndCancels(Proc configureProc) {
    if (configureProc(nullptr, WM_INITDIALOG, 0, 0) != TRUE) {
        return ::testing::AssertionFailure() << "WM_INITDIALOG should report TRUE";
    }
    if (configureProc(nullptr, WM_COMMAND, IDCANCEL, 0) != TRUE) {
        return ::testing::AssertionFailure() << "IDCANCEL should report TRUE";
    }
    if (configureProc(nullptr, WM_HSCROLL, 0, 0) != TRUE) {
        return ::testing::AssertionFailure() << "WM_HSCROLL should report TRUE";
    }
    return ::testing::AssertionSuccess();
}

}  // namespace savertest

#endif
