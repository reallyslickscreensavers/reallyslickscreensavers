/*
 * OpenAL stub.
 *
 * skyrocket is the only saver with sound. Its dSound setting gates whether a
 * SoundEngine is constructed at all (skyrocket.cpp:955), so most of its tests
 * never reach OpenAL - but soundEngine.cpp still has to link, and the sound-on
 * path is worth exercising. This resolves the nineteen entry points it uses.
 *
 * AL_BUILD_LIBRARY is defined by the CMake target before al.h is reached, and
 * is the exact analogue of _GDI32_ for the GL stub:
 *
 *     #if defined(AL_BUILD_LIBRARY) || defined (_OPENAL32LIB)
 *      #define AL_API __declspec(dllexport)
 *     #else
 *      #define AL_API __declspec(dllimport)
 *     #endif
 *
 * Without it every al* function is declared dllimport and these definitions
 * would satisfy nothing. OpenAL32.lib is deliberately not linked.
 *
 * Nothing here makes a sound. Handles are non-null and ids increase, which is
 * all the engine checks before deciding it initialised successfully.
 */

#include <al.h>
#include <alc.h>

// ALCdevice and ALCcontext are opaque in alc.h, so there is nothing to
// construct. The handles are never dereferenced by the caller - they are only
// passed back in - so they need to be non-null and stable, which a
// function-local static gives us.

extern "C" {

ALCdevice* ALC_APIENTRY alcOpenDevice(const ALCchar*)
{
	static int device = 0;
	return static_cast<ALCdevice*>(static_cast<void*>(&device));
}

ALCboolean ALC_APIENTRY alcCloseDevice(ALCdevice*) { return ALC_TRUE; }

ALCcontext* ALC_APIENTRY alcCreateContext(ALCdevice*, const ALCint*)
{
	static int context = 0;
	return static_cast<ALCcontext*>(static_cast<void*>(&context));
}

ALCboolean ALC_APIENTRY alcMakeContextCurrent(ALCcontext*) { return ALC_TRUE; }
void ALC_APIENTRY alcDestroyContext(ALCcontext*) { /* intentionally empty */ }

void AL_APIENTRY alGenBuffers(ALsizei n, ALuint* buffers)
{
	static ALuint next = 1;
	for (ALsizei i = 0; i < n; ++i) buffers[i] = next++;
}

void AL_APIENTRY alGenSources(ALsizei n, ALuint* sources)
{
	static ALuint next = 1;
	for (ALsizei i = 0; i < n; ++i) sources[i] = next++;
}

void AL_APIENTRY alGetSourcei(ALuint, ALenum, ALint* value)
{
	// Reported as "not playing", so the engine always considers a source free
	// and takes the branch that actually starts one.
	*value = 0;
}

void AL_APIENTRY alDeleteBuffers(ALsizei, const ALuint*) { /* intentionally empty */ }
void AL_APIENTRY alDeleteSources(ALsizei, const ALuint*) { /* intentionally empty */ }
void AL_APIENTRY alBufferData(ALuint, ALenum, const ALvoid*, ALsizei, ALsizei) { /* intentionally empty */ }
void AL_APIENTRY alDistanceModel(ALenum) { /* intentionally empty */ }
void AL_APIENTRY alDopplerVelocity(ALfloat) { /* intentionally empty */ }
void AL_APIENTRY alListenerf(ALenum, ALfloat) { /* intentionally empty */ }
void AL_APIENTRY alListenerfv(ALenum, const ALfloat*) { /* intentionally empty */ }
void AL_APIENTRY alSourcePlay(ALuint) { /* intentionally empty */ }
void AL_APIENTRY alSourcef(ALuint, ALenum, ALfloat) { /* intentionally empty */ }
void AL_APIENTRY alSourcefv(ALuint, ALenum, const ALfloat*) { /* intentionally empty */ }
void AL_APIENTRY alSourcei(ALuint, ALenum, ALint) { /* intentionally empty */ }

}  // extern "C"
