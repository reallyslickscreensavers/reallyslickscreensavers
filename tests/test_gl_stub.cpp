/*
 * Tests for the GL stub's own maths.
 *
 * Everything the saver suites assert about flare positions, culling and
 * billboard orientation is computed by the matrix stack in support/gl_stub.cpp.
 * A transposed multiply there would not fail any saver test - it would just
 * quietly make every downstream assertion meaningless - so the stack is checked
 * here against values that can be worked out by hand.
 */

#include <gtest/gtest.h>

#include <Windows.h>
#include <gl/GL.h>
#include <gl/GLU.h>

#include "support/gl_stub.h"

namespace {

// OpenGL stores matrices column-major: m[row + 4 * column].
constexpr int at(int row, int column) { return row + 4 * column; }

class GlStubMatrix : public ::testing::Test {
protected:
    void SetUp() override {
        glstub::resetMatrices();
        glstub::reset();
        glMatrixMode(GL_MODELVIEW);
    }

    static void modelview(float out[16]) { glstub::currentMatrix(GL_MODELVIEW, out); }
};

}  // namespace

TEST_F(GlStubMatrix, StartsAtIdentity) {
    float m[16];
    modelview(m);
    for (int row = 0; row < 4; ++row) {
        for (int col = 0; col < 4; ++col) {
            EXPECT_FLOAT_EQ(m[at(row, col)], row == col ? 1.0f : 0.0f)
                << "at (" << row << ", " << col << ")";
        }
    }
}

TEST_F(GlStubMatrix, TranslationLandsInTheFourthColumn) {
    glTranslatef(1.0f, 2.0f, 3.0f);

    float m[16];
    modelview(m);
    EXPECT_FLOAT_EQ(m[12], 1.0f);
    EXPECT_FLOAT_EQ(m[13], 2.0f);
    EXPECT_FLOAT_EQ(m[14], 3.0f);
    EXPECT_FLOAT_EQ(m[15], 1.0f);
}

TEST_F(GlStubMatrix, TransformsCompoundInTheOrderGlApplies) {
    // glTranslate then glScale means the scale is applied first to the point,
    // so the translation is NOT scaled. Getting the multiply argument order
    // backwards would put 2 and 4 in the fourth column instead.
    glTranslatef(1.0f, 2.0f, 3.0f);
    glScalef(2.0f, 2.0f, 2.0f);

    float m[16];
    modelview(m);
    EXPECT_FLOAT_EQ(m[at(0, 0)], 2.0f);
    EXPECT_FLOAT_EQ(m[12], 1.0f) << "translation must not pick up the scale";
    EXPECT_FLOAT_EQ(m[13], 2.0f);
    EXPECT_FLOAT_EQ(m[14], 3.0f);
}

TEST_F(GlStubMatrix, RotationAboutZMatchesTheHandWorkedResult) {
    glRotatef(90.0f, 0.0f, 0.0f, 1.0f);

    // The x axis rotates onto the y axis: first column becomes (0, 1, 0, 0).
    float m[16];
    modelview(m);
    EXPECT_NEAR(m[at(0, 0)], 0.0f, 1e-6f);
    EXPECT_NEAR(m[at(1, 0)], 1.0f, 1e-6f);
    EXPECT_NEAR(m[at(0, 1)], -1.0f, 1e-6f);
    EXPECT_NEAR(m[at(1, 1)], 0.0f, 1e-6f);
}

TEST_F(GlStubMatrix, ZeroLengthRotationAxisIsIdentityRatherThanNaN) {
    glRotatef(45.0f, 0.0f, 0.0f, 0.0f);

    float m[16];
    modelview(m);
    for (int i = 0; i < 16; ++i) EXPECT_FALSE(std::isnan(m[i])) << "element " << i;
    EXPECT_FLOAT_EQ(m[at(0, 0)], 1.0f);
}

TEST_F(GlStubMatrix, PushAndPopRestoreTheMatrix) {
    glTranslatef(5.0f, 0.0f, 0.0f);
    glPushMatrix();
    glTranslatef(7.0f, 0.0f, 0.0f);

    float inner[16];
    modelview(inner);
    EXPECT_FLOAT_EQ(inner[12], 12.0f);
    EXPECT_EQ(glstub::matrixStackDepth(GL_MODELVIEW), 2);

    glPopMatrix();

    float outer[16];
    modelview(outer);
    EXPECT_FLOAT_EQ(outer[12], 5.0f);
    EXPECT_EQ(glstub::matrixStackDepth(GL_MODELVIEW), 1);
}

TEST_F(GlStubMatrix, PoppingAnEmptyStackLeavesTheLastEntryAlone) {
    // Real GL raises GL_STACK_UNDERFLOW rather than discarding the matrix. The
    // trace still records the imbalance through minMatrixDepth.
    glTranslatef(5.0f, 0.0f, 0.0f);
    glPopMatrix();

    float m[16];
    modelview(m);
    EXPECT_FLOAT_EQ(m[12], 5.0f);
    EXPECT_EQ(glstub::matrixStackDepth(GL_MODELVIEW), 1);
    EXPECT_LT(glstub::trace().minMatrixDepth, 0) << "the underflow must still be visible";
}

TEST_F(GlStubMatrix, EachModeHasItsOwnStack) {
    glMatrixMode(GL_MODELVIEW);
    glTranslatef(1.0f, 0.0f, 0.0f);
    glMatrixMode(GL_PROJECTION);
    glTranslatef(9.0f, 0.0f, 0.0f);

    float mv[16];
    float proj[16];
    glstub::currentMatrix(GL_MODELVIEW, mv);
    glstub::currentMatrix(GL_PROJECTION, proj);
    EXPECT_FLOAT_EQ(mv[12], 1.0f);
    EXPECT_FLOAT_EQ(proj[12], 9.0f);
}

TEST_F(GlStubMatrix, PerspectiveMatchesTheStandardProjection) {
    // fovy 90 gives f = 1/tan(45) = 1, so with aspect 1 the first two diagonal
    // entries are 1. zNear 1, zFar 101 gives m10 = -(f+n)/(f-n) = -1.02 and
    // m14 = -2fn/(f-n) = -2.02.
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective(90.0, 1.0, 1.0, 101.0);

    float m[16];
    glstub::currentMatrix(GL_PROJECTION, m);
    EXPECT_NEAR(m[at(0, 0)], 1.0f, 1e-5f);
    EXPECT_NEAR(m[at(1, 1)], 1.0f, 1e-5f);
    EXPECT_NEAR(m[at(2, 2)], -1.02f, 1e-5f);
    EXPECT_NEAR(m[at(3, 2)], -1.0f, 1e-5f);
    EXPECT_NEAR(m[at(2, 3)], -2.02f, 1e-5f);
}

TEST_F(GlStubMatrix, LoadMatrixReplacesRatherThanCompounds) {
    glTranslatef(4.0f, 4.0f, 4.0f);

    float replacement[16] = {};
    replacement[0] = replacement[5] = replacement[10] = replacement[15] = 1.0f;
    replacement[12] = 8.0f;
    glLoadMatrixf(replacement);

    float m[16];
    modelview(m);
    EXPECT_FLOAT_EQ(m[12], 8.0f);
}

// --- readback ---------------------------------------------------------------

TEST_F(GlStubMatrix, GetFloatvAnswersTheMatrixQuery) {
    glTranslatef(0.0f, 6.0f, 0.0f);

    float m[16] = {};
    glGetFloatv(GL_MODELVIEW_MATRIX, m);

    EXPECT_FLOAT_EQ(m[13], 6.0f);
    EXPECT_TRUE(glstub::trace().invalidEnums.empty());
}

TEST_F(GlStubMatrix, GetFloatvLeavesTheBufferAloneForAnEnumItCannotAnswer) {
    // GL_MODELVIEW is the mode, not the matrix query - the mistake lattice made.
    float m[16];
    for (int i = 0; i < 16; ++i) m[i] = -1.0f;

    glGetFloatv(GL_MODELVIEW, m);

    for (int i = 0; i < 16; ++i) EXPECT_FLOAT_EQ(m[i], -1.0f) << "element " << i;
    ASSERT_EQ(glstub::trace().invalidEnums.size(), 1u);
    EXPECT_EQ(glstub::trace().invalidEnums[0].first, "glGetFloatv");
    EXPECT_EQ(glstub::trace().invalidEnums[0].second, unsigned(GL_MODELVIEW));
}

TEST_F(GlStubMatrix, ViewportReadsBackWhatWasSet) {
    glViewport(0, 0, 640, 480);

    GLint viewport[4] = {};
    glGetIntegerv(GL_VIEWPORT, viewport);

    EXPECT_EQ(viewport[2], 640);
    EXPECT_EQ(viewport[3], 480);
    EXPECT_TRUE(glstub::trace().invalidEnums.empty());
}

// --- gluProject -------------------------------------------------------------

TEST_F(GlStubMatrix, ProjectPutsTheOriginAtTheCentreOfTheViewport) {
    const GLdouble identity[16] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
    const GLint viewport[4] = {0, 0, 640, 480};

    GLdouble winx = 0.0;
    GLdouble winy = 0.0;
    GLdouble winz = 0.0;
    EXPECT_EQ(gluProject(0.0, 0.0, 0.0, identity, identity, viewport, &winx, &winy, &winz),
              GL_TRUE);

    EXPECT_DOUBLE_EQ(winx, 320.0);
    EXPECT_DOUBLE_EQ(winy, 240.0);
    EXPECT_DOUBLE_EQ(winz, 0.5);
}

TEST_F(GlStubMatrix, ProjectMovesWithTheModelviewMatrix) {
    // Shifting the model right by one unit under an identity projection moves
    // the point a quarter of the way across a 640-wide viewport, because clip
    // space spans -1..1.
    const GLdouble identity[16] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
    const GLdouble translated[16] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 1, 0, 0, 1};
    const GLint viewport[4] = {0, 0, 640, 480};

    GLdouble winx = 0.0;
    GLdouble winy = 0.0;
    GLdouble winz = 0.0;
    gluProject(0.0, 0.0, 0.0, translated, identity, viewport, &winx, &winy, &winz);

    EXPECT_DOUBLE_EQ(winx, 640.0);
    EXPECT_DOUBLE_EQ(winy, 240.0);
}

TEST_F(GlStubMatrix, ProjectWritesZerosRatherThanLeakingNaNWhenWIsZero) {
    // Deliberate deviation from the spec, because neither call site in the tree
    // checks the return value before dividing the outputs - see the comment on
    // gluProject in support/gl_stub.cpp.
    const GLdouble identity[16] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
    const GLdouble degenerate[16] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0};
    const GLint viewport[4] = {0, 0, 640, 480};

    GLdouble winx = 7.0;
    GLdouble winy = 7.0;
    GLdouble winz = 7.0;
    EXPECT_EQ(gluProject(0.0, 0.0, 0.0, identity, degenerate, viewport, &winx, &winy, &winz),
              GL_FALSE);

    EXPECT_DOUBLE_EQ(winx, 0.0);
    EXPECT_DOUBLE_EQ(winy, 0.0);
    EXPECT_DOUBLE_EQ(winz, 0.0);
}

// --- extension probing ------------------------------------------------------

TEST(GlStubExtensions, ReportsNoExtensionsWithoutReturningNull) {
    // hyperspace and microcosm walk this string with strstr; a null would crash
    // rather than send them down their no-shader fallback.
    const GLubyte* extensions = glGetString(GL_EXTENSIONS);
    ASSERT_NE(extensions, nullptr);
    EXPECT_STREQ(reinterpret_cast<const char*>(extensions), "");
}

TEST(GlStubExtensions, ProcAddressLookupFails) {
    EXPECT_EQ(wglGetProcAddress("glActiveTextureARB"), nullptr);
}
