//
// Created by grant on 1/24/20.
//

#pragma once

#ifndef __STONEMASON_AUDIO_BUFFER_HPP
#define __STONEMASON_AUDIO_BUFFER_HPP

#include <string>

#include "glm/glm.hpp"
#include "al.h"
#include "alc.h"

namespace stms {
    class ALDevice {
    private:
        ALCdevice *id{};

        friend class ALContext;

    public:
        explicit ALDevice(const ALCchar *devname = nullptr) noexcept;

        virtual ~ALDevice();

        // Returns true if there was an error handled
        bool handleError();

        inline void flushErrors() {
            while (handleError());
        };
    };

    class ALContext {
    private:
        ALCcontext *id{};
    public:
        ALContext() = default;

        explicit ALContext(ALDevice *dev, ALCint *attribs = nullptr) noexcept;

        virtual ~ALContext();

        inline void bind() {
            alcMakeContextCurrent(id);
        }
    };

    class ALBuffer {
    private:
        short *data{};
        int len{};
        int channels{};
        int sampleRate{};

        ALuint id{};

        friend class ALSource;

    public:
        ALBuffer() = default;

        explicit ALBuffer(const char *filename);

        virtual ~ALBuffer();

        ALBuffer &operator=(ALBuffer const &rhs) noexcept;

        ALBuffer(ALBuffer &rhs);
    };

    class ALSource {
    private:
        ALuint id{};

    public:
        ALSource();

        virtual ~ALSource();

        inline void play() {
            alSourcePlay(id);
        }

        inline void enqueueBuf(ALBuffer *buf) {
            alSourceQueueBuffers(id, 1, &buf->id);
        }

        inline void stop() {
            alSourceStop(id);
        }

        inline void pause() {
            alSourcePause(id);
        }

        inline void rewind() {
            alSourceRewind(id);
        }

        inline void dequeueBuf(ALBuffer *bufs) {
            alSourceUnqueueBuffers(id, 1, &bufs->id);
        }

        inline void setLooping(bool looping) {
            alSourcei(id, AL_LOOPING, looping ? AL_TRUE : AL_FALSE);
        }

        inline void setPitch(float pitch) {
            alSourcef(id, AL_PITCH, pitch);
        }

        inline void setGain(float gain) {
            alSourcef(id, AL_GAIN, gain);
        }

        inline void setVel(glm::vec3 vel) {
            alSource3f(id, AL_VELOCITY, vel.x, vel.y, vel.z);
        }

        inline void setPos(glm::vec3 pos) {
            alSource3f(id, AL_POSITION, pos.x, pos.y, pos.z);
        }

        inline void setOri(glm::vec3 forward, glm::vec3 up) {
            float ori[6] = {forward.x, forward.y, forward.z, up.x, up.y, up.z};
            alSourcefv(id, AL_ORIENTATION, ori);
        }

        inline void bindBuffer(ALBuffer *buf) {
            alSourcei(id, AL_BUFFER, buf->id);
        }

        inline bool isPlaying() {
            ALint state;
            alGetSourcei(id, AL_SOURCE_STATE, &state);
            return state == AL_PLAYING;
        }

        ALSource(ALSource &rhs);

        ALSource &operator=(ALSource const &rhs) noexcept;
    };

    namespace ALListener {
        inline void setPos(glm::vec3 pos) {
            alListener3f(AL_POSITION, pos.x, pos.y, pos.z);
        }

        inline void setOri(glm::vec3 forward, glm::vec3 up) {
            float vec[6] = {forward.x, forward.y, forward.z, up.x, up.y, up.z};
            alListenerfv(AL_ORIENTATION, vec);
        }

        inline void setVel(glm::vec3 vel) {
            alListener3f(AL_VELOCITY, vel.x, vel.y, vel.z);
        }
    }

    // Returns true if there was an error handled
    bool handleAlError();

    inline void flushAlErrors() {
        while (handleAlError());
    }

    // Initiate stuff
    extern ALDevice defaultAlDevice;
    extern ALContext defaultAlContext;

    inline void refreshAlDefaults() {
        defaultAlDevice = ALDevice();
        defaultAlContext = ALContext(&defaultAlDevice, nullptr);
    }
}

#endif //__STONEMASON_AUDIO_BUFFER_HPP
