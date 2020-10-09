//
// Created by grant on 1/24/20.
//

#pragma once

#ifndef __STONEMASON_AUDIO_BUFFER_HPP
#define __STONEMASON_AUDIO_BUFFER_HPP

#include <string>
#include <vector>

#include "glm/glm.hpp"
#include "al.h"
#include "alc.h"

namespace stms {
    enum ALSoundFormat {
        eStereo16 = AL_FORMAT_STEREO16,
        eMono16 = AL_FORMAT_MONO16,

        eStereo8 = AL_FORMAT_STEREO8,
        eMono8 = AL_FORMAT_MONO8
    };

    class ALDevice {
    private:
        ALCdevice *id{};

        friend class ALContext;

    public:
        ALDevice() = delete;

        /**
         * @brief Create a new ALDevice from a device name.
         * @throw If this fails and `stms::exceptionLevel > 0`, a `std::runtime_error` is thrown.
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

        /**
         * @brief Gets the human-readable device name as a string.
         * @return Device name as string, or `nullptr` if device-name querying isn't supported.
         */
        [[nodiscard]] const ALCchar *getName() const;

        ALDevice(ALDevice &rhs) = delete;

        ALDevice &operator=(ALDevice &rhs) = delete;

        ALDevice(ALDevice &&rhs) noexcept;

        ALDevice &operator=(ALDevice &&rhs) noexcept;
    };

    class ALMicrophone {
    private:
        ALCdevice *id{};
    public:
        explicit ALMicrophone(const ALCchar *name = nullptr, ALCuint freq = 8000, ALSoundFormat fmt = eMono8, ALCsizei capbufSize = 32768);
        virtual ~ALMicrophone();

        inline void start() {
            alcCaptureStart(id);
        }

        inline void stop() {
            alcCaptureStop(id);
        }

        [[nodiscard]] const ALCchar *getName() const;

        void capture(ALCvoid *dataBuf, ALCsizei numSamples);
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
         * @throw If `stms::exceptionLevel > 0`, a `std::runtime_error` will be thrown if `filename` cannot be read.
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

        [[nodiscard]] inline ALint getProcessed() const {
            ALint ret = 0;
            alGetSourcei(id, AL_BUFFERS_PROCESSED, &ret);
            return ret;
        }

        inline ALint dequeueAll(ALuint *bufsRemoved) const {
            ALint ret = getProcessed();
            dequeueBuf(ret, bufsRemoved);
            return ret;
        }

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

        inline void dequeueBuf(ALsizei num, ALuint *bufsRemoved) const {
            alSourceUnqueueBuffers(id, num, bufsRemoved);
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

    /**
     * @brief Returns a list of devices
     * @param captureDevices If true, then this returns a list of capture devices (microphones) instead of input devices (speakers)
     * @return List of all devices if possible, otherwise just a list of most devices. If no enumeration extension is
     *         present, it will fall back to just a single element: nullptr (which will be interpreted as the default device).
     */
    std::vector<const ALCchar *> enumerateAlDevices(bool captureDevices = false);

    /**
     * @brief Returns the name of the default device.
     * @param captureDevices If true, then this returns the name of the default capture device (microphone) instead of
     *                       the default input device (speaker)
     * @return Name of the default device, or nullptr if no enumeration extension is present.
     */
    const ALCchar *getDefaultAlDeviceName(bool captureDevices = false);
}

#endif //__STONEMASON_AUDIO_BUFFER_HPP
