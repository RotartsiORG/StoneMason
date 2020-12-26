/**
 * @file stms/audio.hpp
 * @brief File containing all OpenAL wrappers. STMS audio engine.
 * Created by grant on 1/24/20.
 */

#pragma once

#ifndef __STONEMASON_AUDIO_BUFFER_HPP
#define __STONEMASON_AUDIO_BUFFER_HPP
//!< Include guard

#include <string>
#include <vector>

#include "glm/glm.hpp"
#include "al.h"
#include "alc.h"

namespace stms {
    /**
     * @brief Defines the layout of data in an `ALBuffer`. See OpenAL documentation for AL_FORMAT_XXXNN.
     */
    enum ALSoundFormat { // TODO: Are they signed or unsigned? Which channel is which? Is it LRLRLR or RLRLRL
        eStereo16 = AL_FORMAT_STEREO16, //!< 16-bit stereo. Expects a buffer of 16-bit ints, with the R and L channels interleaved.
        eMono16 = AL_FORMAT_MONO16, //!< 16-bit mono. Expects a buffer of 16-bit ints, only for 1 channel.

        eStereo8 = AL_FORMAT_STEREO8, //!< 8-bit stereo. Expects a buffer of 8-bit ints, with the R and L channels interleaved.
        eMono8 = AL_FORMAT_MONO8 //!< 8-bit mono. Expects a buffer of 8-bit ints, only for 1 channel.
    };

    class _stms_GenericALDevice {
    public:
        /**
        * @brief Process the next error in the queue. See OpenAL docs for `alcGetError`
        * @return The error that occurred. Will be 0 if error queue was empty.
        */
        ALCenum handleError();

        inline void flushErrors() { //!< Process all of the errors in this device's error queue. Repeatedly calls `handleError()`
            while (handleError() != ALC_NO_ERROR);
        };

    protected:
        ALCdevice *id{}; //!< Internal OpenAL deice id. Implementation detail, don't touch.
    };

    /**
     * @brief Virtually represents an output device. (e.i. a speaker)
     */
    class ALDevice : public _stms_GenericALDevice {
    private:

        friend class ALContext;

    public:
        ALDevice() = delete; //!< Forbidden default constructor to avoid confusion

        /**
         * @brief Create a new ALDevice from a device name.
         * @throw If this fails and `stms::exceptionLevel > 0`, a `std::runtime_error` is thrown.
         * @param devname Name of the device, fetched from `enumerateAlDevices(false)`. If this is nullptr,
         *                then the default speaker is used.
         */
        explicit ALDevice(const ALCchar *devname);

        /**
         * @brief Gets the human-readable device name as a string.
         * @throw If `stms::exceptionLevel > 0` and the required extension is not present, throws `std::runtime_error`
         * @return Device name as string, or `nullptr` if device-name querying isn't supported and exceptionLevel is 0
         */
        [[nodiscard]] const ALCchar *getName() const;

        ALDevice(ALDevice &rhs) = delete; //!< duh

        ALDevice &operator=(ALDevice &rhs) = delete; //!< duh

        ALDevice(ALDevice &&rhs) noexcept; //!< duh

        ALDevice &operator=(ALDevice &&rhs) noexcept; //!< duh

        virtual ~ALDevice(); //!< duh
    };

    /**
     * @brief Class similar to `ALDevice`, but represents an input device instead of an output. (e.i. a microphone)
     */
    class ALMicrophone : public _stms_GenericALDevice {
    public:
        ALMicrophone() = delete; //!< Forbidden default constructor to avoid confusion

        /**
         * @brief Construct a virtual rep. of a input device.
         * @throw If this fails and `stms::exceptionLevel > 0`, a `std::runtime_error` is thrown.
         * @param name Name of the device, fetched from `enumerateAlDevices(true)`. If this is `nullptr`, the default
         *             input device is used.
         * @param freq The sample rate the audio is to be captured at.
         * @param fmt The format the audio is to be captured at. See `ALSoundFormat`
         * @param capbufSize Size of the internal capture buffer to allocate, in bytes?
         */
        explicit ALMicrophone(const ALCchar *name = nullptr, ALCuint freq = 8000, ALSoundFormat fmt = eMono8, ALCsizei capbufSize = 32768);
        virtual ~ALMicrophone(); //!< duh

        ALMicrophone(ALMicrophone &rhs) = delete; //!< duh

        ALMicrophone &operator=(ALMicrophone &rhs) = delete; //!< duh

        ALMicrophone(ALMicrophone &&rhs) noexcept; //!< duh

        ALMicrophone &operator=(ALMicrophone &&rhs) noexcept; //!< duh

        /// Start capturing samples into the capture buffer.
        inline void start() {
            alcCaptureStart(id);
        }

        /// Stop capturing samples into the capture buffer.
        inline void stop() {
            alcCaptureStop(id);
        }

        /**
         * @brief Get the human-readable name of the microphone
         * @throw If `stms::exceptionLevel > 0` and the required extension is not present, throws `std::runtime_error`
         * @return Null-terminated string, might return nullptr if required extensions are missing and exceptionLevel is too low.
         */
        [[nodiscard]] const ALCchar *getName() const;

        /**
         * @brief Retrieve captured samples from the internal buffer.
         * @param dataBuf Buffer to write samples to. Must be big enough for `numSamples` samples
         * @param numSamples Number of samples to fetch
         */
        void capture(ALCvoid *dataBuf, ALCsizei numSamples);
    };

    /**
     * @brief Wrapper around OpenAL's ALCcontext.
     */
    class ALContext {
    private:
        ALCcontext *id{}; //!< OpenAL id. Internal implementation detail, no touchy

    public:
        ALContext() = delete; //!< Forbidden default constructor to avoid confusion.

        /**
         * @brief Construct a new context
         * @param dev ALDevice to output to
         * @param attribs Optional OpenAL flags. See OpenAL docs for `alcCreateContext`
         */
        explicit ALContext(ALDevice *dev, ALCint *attribs = nullptr);

        virtual ~ALContext(); //!< duh

        /**
         * @brief Bind the current context, making sources play using this context.
         */
        inline void bind() {
            alcMakeContextCurrent(id);
        }

        ALContext(ALContext &rhs) = delete; //!< duh

        ALContext &operator=(ALContext &rhs) = delete; //!< duh

        ALContext(ALContext &&rhs) noexcept; //!< duh

        ALContext &operator=(ALContext &&rhs) noexcept; //!< duh
    };

    /**
     * @brief ALBuffer contains the raw sound data to be played.
     */
    class ALBuffer {
    private:
        ALuint id{}; //!< Internal OpenAL id, implementation detail, dont touch, only read.

        friend class ALSource;
    public:

        bool freeOnDel = true;

        ALBuffer(); //!< Duh. Default constructor

        /**
         * @brief Recovers an ALBuffer object from the raw internal OpenAL id. Useful for manipulating the
         *        bufsRemoved retrieved from a ALSource with a dequeue operation.
         * @warning Constructing a buffer using this constructor will cause it to not be freed when the ALBuffer object
         *          goes out of scope in order to prevent pre-mature freeing or double freeing.
         * @param id Internal id of the buffer to recover.
         */
        explicit ALBuffer(ALuint id);

        virtual ~ALBuffer(); //!< duh

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

        /**
         * @brief Write your own raw data to the buffer.
         * @param dat Pointer to data to write
         * @param size Number of bytes to write from `dat`
         * @param freq Sample rate of the audio data (samples per second)
         * @param fmt Format of the audio data. See `ALSoundFormat`.
         */
        inline void write(const void *dat, ALsizei size, ALsizei freq, ALSoundFormat fmt) const {
            alBufferData(id, fmt, dat, size, freq);
        }

        ALBuffer &operator=(ALBuffer &rhs) = delete; //!< duh

        ALBuffer(ALBuffer &rhs) = delete; //!< duh

        ALBuffer &operator=(ALBuffer &&rhs) noexcept; //!< duh

        ALBuffer(ALBuffer &&rhs) noexcept; //!< duh
    };

    /**
     * @brief A virtual sound source to play sounds from. 3D sound!
     */
    class ALSource {
    private:
        ALuint id{}; //!< Internal OpenAL implementation detail, no touchy

    public:
        ALSource(); //!< Duh. Default constructor

        virtual ~ALSource(); //!< duh

        /**
         * @brief Gets the number of processed buffers ready to be dequeued
         * @return Integer of the max `num` that can be passed to `dequeueBufs`
         */
        [[nodiscard]] inline ALint getProcessed() const {
            ALint ret = 0;
            alGetSourcei(id, AL_BUFFERS_PROCESSED, &ret);
            return ret;
        }

        /**
         * @brief Dequeues all of the buffers that have been played.
         * @return The number of buffers that had been played. (i.e The number of buffers we tried to dequeue)
         */
        ALint dequeueAll() const;

        /// Start playback on this source.
        inline void play() const {
            alSourcePlay(id);
        }

        /**
         * @brief Add a buffer to the playback queue
         * @param buf Buffer to enqueue
         */
        inline void enqueueBuf(ALBuffer *buf) const {
            alSourceQueueBuffers(id, 1, &buf->id);
        }

        /// Stop playback on this source
        inline void stop() const {
            alSourceStop(id);
        }

        /// Pause playback on this source
        inline void pause() const {
            alSourcePause(id);
        }

        /// Rewind playback back to the start of the list of enqueued buffers on this source.
        inline void rewind() const {
            alSourceRewind(id);
        }

        /**
         * @brief Remove processed buffers from the playback queue
         * @param num Number of buffers to dequeue. Must be less than `getProcessed()`
         * @param bufsRemoved Pass in an array of at least `num` ALuints. This will be the place OpenAL uses to
         *                    store the internal handles of buffers removed. See OpenAL docs for `alSourceUnqueueBuffers`
         */
        inline void dequeueBufs(ALsizei num, ALuint *bufsRemoved) const {
            alSourceUnqueueBuffers(id, num, bufsRemoved);
        }

        /**
         * @brief Setting for if playback will loop back to the beginning of the enqueued buffers once it reaches the end.
         * @param looping If true, playback will loop, otherwise, playback will simply stop
         */
        inline void setLooping(bool looping) const {
            alSourcei(id, AL_LOOPING, looping ? AL_TRUE : AL_FALSE);
        }

        /**
         * @brief Set the pitch modifier.
         * @param pitch Pitch scalar float
         */
        inline void setPitch(float pitch) const {
            alSourcef(id, AL_PITCH, pitch);
        }

        /**
         * @brief Set the gain, or the volume of this source.
         * @param gain Gain scalar float
         */
        inline void setGain(float gain) const {
            alSourcef(id, AL_GAIN, gain);
        }

        /**
         * @brief  Set the velocity of this source, useful for doppler effect
         * @param vel Velocity vector.
         */
        inline void setVel(glm::vec3 vel) const {
            alSource3f(id, AL_VELOCITY, vel.x, vel.y, vel.z);
        }

        /**
         * @brief Set the position of this audio source in 3D space
         * @param pos Position vector
         */
        inline void setPos(glm::vec3 pos) const {
            alSource3f(id, AL_POSITION, pos.x, pos.y, pos.z);
        }

        /**
         * @brief Set the direction of the source, determined using two perpendicular vectors
         * @param forward Forward vector
         * @param up Up vector
         */
        inline void setOri(glm::vec3 forward, glm::vec3 up) const {
            float ori[6] = {forward.x, forward.y, forward.z, up.x, up.y, up.z};
            alSourcefv(id, AL_ORIENTATION, ori);
        }

        /**
         * @brief USE `enqueueBuf()` INSTEAD! This binds a SINGLE buffer to playback.
         * @warning Use of this is discouraged. One should use `enqueueBuf()` instead.
         * @param buf Buffer to bind
         */
        inline void bindBuffer(ALBuffer *buf) const {
            alSourcei(id, AL_BUFFER, buf->id);
        }

        /**
         * @brief Query if this source is currently playing audio
         * @return True if audio playback is in session
         */
        [[nodiscard]] inline bool isPlaying() const {
            ALint state;
            alGetSourcei(id, AL_SOURCE_STATE, &state);
            return state == AL_PLAYING;
        }

        ALSource(ALSource &rhs) = delete; //!< duh

        ALSource &operator=(ALSource &rhs) = delete; //!< duh

        ALSource(ALSource &&rhs) noexcept; //!< duh

        ALSource &operator=(ALSource &&rhs) noexcept; //!< duh
    };

    /// Singleton class representing the listener.
    class ALListener {
    public:
        /// This class is a singleton. The default constructor is deleted.
        ALListener() = delete;

        /**
         * @brief Set the position of the listener in 3D space.
         * @param pos Position vector
         */
        inline static void setPos(glm::vec3 pos) {
            alListener3f(AL_POSITION, pos.x, pos.y, pos.z);
        }

        /**
         * @brief Set the orientation of the listener in 3D space using 2 perpendicular vectors.
         * @param forward Forward vector
         * @param up Up vector
         */
        inline static void setOri(glm::vec3 forward, glm::vec3 up) {
            float vec[6] = {forward.x, forward.y, forward.z, up.x, up.y, up.z};
            alListenerfv(AL_ORIENTATION, vec);
        }

        /**
         * @brief Set the 3D velocity of the listener. Useful for the doppler effect.
         * @param vel Velocity vector.
         */
        inline static void setVel(glm::vec3 vel) {
            alListener3f(AL_VELOCITY, vel.x, vel.y, vel.z);
        }
    };

    /**
     * @brief Process a single error from the OpenAL error queue.
     * @return The error that was processed, 0 if no error was in the queue. See OpenAL docs for `alGetError`
     */
    ALenum handleAlError();

    /// Flush all errors from the global OpenAL error queue. Repeatedly calls `handleAlError()`.
    inline void flushAlErrors() {
        while (handleAlError() != AL_NO_ERROR);
    }

    /**
     * @return Reference to the static default ALDevice, which is created when you first call this function.
     */
    inline ALDevice &defaultAlDevice() {
        static ALDevice alDevice = ALDevice(nullptr);
        return alDevice;
    }

    /**
     * @return Reference to the static default ALContext, which is created when you first call this function.
     */
    inline ALContext &defaultAlContext() {
        static ALContext alContext = ALContext(&defaultAlDevice(), nullptr);
        return alContext;
    }

    /// Regenerates `defaultAlDevice` and `defaultAlContext`
    inline void refreshAlDefaults() {
        defaultAlDevice() = ALDevice(nullptr);
        defaultAlContext() = ALContext(&defaultAlDevice(), nullptr);
    }

    /**
     * @brief Returns a list of devices
     * @throw Throws `std::runtime_error` if `stms::exceptionLevel > 0` and required extensions are missing.
     * @param captureDevices If true, then this returns a list of capture devices (microphones) instead of input devices (speakers)
     * @return List of all devices if possible, otherwise just a list of most devices. If no enumeration extension is
     *         present, it will fall back to just a single element: nullptr (which will be interpreted as the default device).
     */
    std::vector<const ALCchar *> enumerateAlDevices(bool captureDevices = false);

    /**
     * @brief Returns the name of the default device.
     * @throw Throws `std::runtime_error` if `stms::exceptionLevel > 0` and required extensions are missing.
     * @param captureDevices If true, then this returns the name of the default capture device (microphone) instead of
     *                       the default input device (speaker)
     * @return Name of the default device, or nullptr if no enumeration extension is present.
     */
    const ALCchar *getDefaultAlDeviceName(bool captureDevices = false);
}

#endif //__STONEMASON_AUDIO_BUFFER_HPP
