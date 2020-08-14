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
        ALDevice() = delete;

        /**
         * @brief Create a new ALDevice from a device name.
         * @throw If this fails and `stms::exceptionLevel > 0`, a `GenericException` is thrown.
         * @param devname Name of the physical device to create the virtual device from. If this is nullptr,
         *                then the default speaker is used.
         */
        explicit ALDevice(const ALCchar *devname);

        virtual ~ALDevice();

        // Returns true if there was an error handled
        ALCenum handleError();

        inline void flushErrors() {
            while (handleError() != ALC_NO_ERROR);
        };

        ALDevice(ALDevice &rhs) = delete;

        ALDevice &operator=(ALDevice &rhs) = delete;

        ALDevice(ALDevice &&rhs) noexcept;

        ALDevice &operator=(ALDevice &&rhs) noexcept;
    };

    class ALContext {
    private:
        ALCcontext *id{};

    public:
        ALContext() = delete;

        explicit ALContext(ALDevice *dev, ALCint *attribs = nullptr);

        virtual ~ALContext();

        inline void bind() {
            alcMakeContextCurrent(id);
        }

        ALContext(ALContext &rhs) = delete;

        ALContext &operator=(ALContext &rhs) = delete;

        ALContext(ALContext &&rhs) noexcept;

        ALContext &operator=(ALContext &&rhs) noexcept;
    };

    enum ALSoundFormat {
        eStereo16 = AL_FORMAT_STEREO16,
        eMono16 = AL_FORMAT_MONO16,

        eStereo8 = AL_FORMAT_STEREO8,
        eMono8 = AL_FORMAT_MONO8
    };

    class ALBuffer {
    private:
        ALuint id{};

        friend class ALSource;

    public:
        ALBuffer();

        virtual ~ALBuffer();

        /**
         * @brief Load a .ogg file into a ALBuffer.
         * @param File to load
         * @throw See throws for `loadFromFile`.
         */
        explicit ALBuffer(const char *filename);

        /**
         * @brief Load a .ogg file into the ALBuffer
         * @throw If `stms::exceptionLevel > 0`, a `FileNotFoundException` will be thrown if `filename` cannot be read.
         *        This includes being unable to parse the data contained within the file.
         * @param filename .ogg file to load
         */
        void loadFromFile(const char *filename) const;

        inline void write(const void *dat, ALsizei size, ALsizei freq, ALSoundFormat fmt) const {
            alBufferData(id, fmt, dat, size, freq);
        }

        ALBuffer &operator=(ALBuffer &rhs) = delete;

        ALBuffer(ALBuffer &rhs) = delete;

        ALBuffer &operator=(ALBuffer &&rhs) noexcept;

        ALBuffer(ALBuffer &&rhs) noexcept;
    };

    class ALSource {
    private:
        ALuint id{};

    public:
        ALSource();

        virtual ~ALSource();

        inline void play() const {
            alSourcePlay(id);
        }

        inline void enqueueBuf(ALBuffer *buf) const {
            alSourceQueueBuffers(id, 1, &buf->id);
        }

        inline void stop() const {
            alSourceStop(id);
        }

        inline void pause() const {
            alSourcePause(id);
        }

        inline void rewind() const {
            alSourceRewind(id);
        }

        inline void dequeueBuf(ALBuffer *bufs) const {
            alSourceUnqueueBuffers(id, 1, &bufs->id);
        }

        inline void setLooping(bool looping) const {
            alSourcei(id, AL_LOOPING, looping ? AL_TRUE : AL_FALSE);
        }

        inline void setPitch(float pitch) const {
            alSourcef(id, AL_PITCH, pitch);
        }

        inline void setGain(float gain) const {
            alSourcef(id, AL_GAIN, gain);
        }

        inline void setVel(glm::vec3 vel) const {
            alSource3f(id, AL_VELOCITY, vel.x, vel.y, vel.z);
        }

        inline void setPos(glm::vec3 pos) const {
            alSource3f(id, AL_POSITION, pos.x, pos.y, pos.z);
        }

        inline void setOri(glm::vec3 forward, glm::vec3 up) const {
            float ori[6] = {forward.x, forward.y, forward.z, up.x, up.y, up.z};
            alSourcefv(id, AL_ORIENTATION, ori);
        }

        inline void bindBuffer(ALBuffer *buf) const {
            alSourcei(id, AL_BUFFER, buf->id);
        }

        [[nodiscard]] inline bool isPlaying() const {
            ALint state;
            alGetSourcei(id, AL_SOURCE_STATE, &state);
            return state == AL_PLAYING;
        }

        ALSource(ALSource &rhs) = delete;

        ALSource &operator=(ALSource &rhs) = delete;

        ALSource(ALSource &&rhs) noexcept;

        ALSource &operator=(ALSource &&rhs) noexcept;
    };

    class ALListener {
    public:
        /// This class is a singleton, delete the default constructor and do not allow construction of this type.
        ALListener() = delete;

        inline static void setPos(glm::vec3 pos) {
            alListener3f(AL_POSITION, pos.x, pos.y, pos.z);
        }

        inline static void setOri(glm::vec3 forward, glm::vec3 up) {
            float vec[6] = {forward.x, forward.y, forward.z, up.x, up.y, up.z};
            alListenerfv(AL_ORIENTATION, vec);
        }

        inline static void setVel(glm::vec3 vel) {
            alListener3f(AL_VELOCITY, vel.x, vel.y, vel.z);
        }
    };

    // Returns true if there was an error handled
    ALenum handleAlError();

    inline void flushAlErrors() {
        while (handleAlError() != AL_NO_ERROR);
    }

    inline ALDevice &defaultAlDevice() {
        static ALDevice alDevice = ALDevice(nullptr);
        return alDevice;
    }

    inline ALContext &defaultAlContext() {
        static ALContext alContext = ALContext(&defaultAlDevice(), nullptr);
        return alContext;
    }

    inline void refreshAlDefaults() {
        defaultAlDevice() = ALDevice(nullptr);
        defaultAlContext() = ALContext(&defaultAlDevice(), nullptr);
    }
}

#endif //__STONEMASON_AUDIO_BUFFER_HPP
