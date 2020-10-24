//
// Created by grant on 1/24/20.
//

#include "stms/audio.hpp"
#include "stb/stb_vorbis.c"

#include "stms/logging.hpp"
#include "stms/except.hpp"

namespace stms {

    ALenum handleAlError() {
        ALenum error = alGetError();
        if (error != AL_NO_ERROR) {
            STMS_ERROR("[** OpenAL ERROR **]: {}", error);
        }
        return error;
    }

    std::vector<const ALCchar *> enumerateAlDevices(bool cap) {
        std::vector<const ALCchar *> ret;

        if ((!cap) && alcIsExtensionPresent(nullptr, "ALC_ENUMERATE_ALL_EXT") == AL_TRUE) {
            const ALCchar *devices = alcGetString(nullptr, ALC_ALL_DEVICES_SPECIFIER);

            while (!(*devices == '\0' && *(devices + 1) == '\0')) {
                ret.emplace_back(devices);
                devices += std::strlen(devices) + 1;
            }
        } else if (alcIsExtensionPresent(nullptr, "ALC_ENUMERATION_EXT") == AL_TRUE) {
            const ALCchar *devices = alcGetString(nullptr, cap ? ALC_CAPTURE_DEVICE_SPECIFIER : ALC_DEVICE_SPECIFIER);

            while (!(*devices == '\0' && *(devices + 1) == '\0')) {
                ret.emplace_back(devices);
                devices += std::strlen(devices) + 1;
            }
        } else if (exceptionLevel > 0) {
            throw std::runtime_error("Cannot enumerate OpenAL devices: Both ALC_ENUMERATION_EXT and ALC_ENUMERATE_ALL_EXT are missing");
        } else {
            ret.emplace_back(nullptr);
        }

        return ret;
    }

    const ALCchar *getDefaultAlDeviceName(bool cap) {
        if ((!cap) && alcIsExtensionPresent(nullptr, "ALC_ENUMERATE_ALL_EXT") == AL_TRUE) {
            return alcGetString(nullptr, ALC_DEFAULT_ALL_DEVICES_SPECIFIER);
        } else if (alcIsExtensionPresent(nullptr, "ALC_ENUMERATION_EXT") == AL_TRUE) {
            return alcGetString(nullptr, cap ? ALC_CAPTURE_DEFAULT_DEVICE_SPECIFIER : ALC_DEFAULT_DEVICE_SPECIFIER);
        } else if (exceptionLevel > 0) {
            throw std::runtime_error("Cannot get default OpenAL device: Both ALC_ENUMERATION_EXT and ALC_ENUMERATE_ALL_EXT are missing");
        }
        return nullptr;
    }

    ALBuffer::ALBuffer() {
        alGenBuffers(1, &id);
    }

    ALBuffer::~ALBuffer() {
        if (freeOnDel) { alDeleteBuffers(1, &id); }
    }

    ALBuffer &ALBuffer::operator=(ALBuffer &&rhs) noexcept {
        if (&rhs == this) {
            return *this;
        }

        if (freeOnDel) { alDeleteBuffers(1, &id); }
        this->id = rhs.id;
        rhs.id = 0;

        return *this;
    }

    ALBuffer::ALBuffer(ALBuffer &&rhs) noexcept {
        *this = std::move(rhs);
    }

    void ALBuffer::loadFromFile(const char *filename) const {
        short *data;
        int channels;
        int sampleRate;

        int len = stb_vorbis_decode_filename(filename, &channels, &sampleRate, &data);
        if (len == 0 || channels == 0 || sampleRate == 0 || data == nullptr) {
            STMS_ERROR("Failed to load ALBuffer audio from {}!", filename);
            if (exceptionLevel > 0) {
                throw std::runtime_error(fmt::format("Cannot read .ogg data from {}", filename));
            }
            return;
        }

        if (channels > 1) {
            len *= 2 * sizeof(short);
            alBufferData(id, AL_FORMAT_STEREO16, data, len, sampleRate);
        } else {
            len *= sizeof(short);
            alBufferData(id, AL_FORMAT_MONO16, data, len, sampleRate);
        }

        free(data); // !VERY IMPORTANT!
    }

    ALBuffer::ALBuffer(const char *filename) {
        alGenBuffers(1, &id);
        loadFromFile(filename);
    }

    ALBuffer::ALBuffer(ALuint id) : freeOnDel(false), id(id) {}

    ALSource::ALSource() {
        alGenSources(1, &id);
    }

    ALSource::~ALSource() {
        alDeleteSources(1, &id);
    }

    ALSource::ALSource(ALSource &&rhs) noexcept {
        *this = std::move(rhs);
    }

    ALSource &ALSource::operator=(ALSource &&rhs) noexcept {
        if (this == &rhs) {
            return *this;
        }

        alDeleteSources(1, &id);

        this->id = rhs.id;
        rhs.id = 0;

        return *this;
    }

    ALint ALSource::dequeueAll() const {
        ALint ret = getProcessed();
        auto *bufsRemoved = new ALuint[ret];
        dequeueBufs(ret, bufsRemoved);
        delete[] bufsRemoved;
        return ret;
    }

    ALContext::~ALContext() {
        alcDestroyContext(id);
    }

    ALContext::ALContext(ALContext &&rhs) noexcept {
        *this = std::move(rhs);
    }

    ALContext &ALContext::operator=(ALContext &&rhs) noexcept {
        if (&rhs == this) {
            return *this;
        }

        alcDestroyContext(this->id);

        this->id = rhs.id;
        rhs.id = nullptr;

        return *this;
    }


    ALDevice::ALDevice(const ALCchar *devname) {
        id = alcOpenDevice(devname);
        if (id == nullptr) {
            STMS_ERROR("Failed to open ALDevice: {}", devname == nullptr ? "nullptr" : devname);
            if (exceptionLevel > 0) {
                throw std::runtime_error("Cannot open ALDevice!");
            }
        }
    }

    ALDevice::~ALDevice() {
        alcCloseDevice(id);
    }

    ALCenum _stms_GenericALDevice::handleError() {
        ALCenum error = alcGetError(id);
        if (error != ALC_NO_ERROR) {
            STMS_ERROR("[** ALC ERROR **] {}", error);
        }
        return error;
    }

    ALDevice::ALDevice(ALDevice &&rhs) noexcept {
        *this = std::move(rhs);
    }

    ALDevice &ALDevice::operator=(ALDevice &&rhs) noexcept {
        if (this == &rhs) {
            return *this;
        }

        alcCloseDevice(this->id);

        this->id = rhs.id;
        rhs.id = nullptr;

        return *this;
    }

    ALMicrophone::ALMicrophone(ALMicrophone &&rhs) noexcept {
        *this = std::move(rhs);
    }

    ALMicrophone &ALMicrophone::operator=(ALMicrophone &&rhs) noexcept {
        if (this == &rhs) {
            return *this;
        }

        alcCaptureCloseDevice(this->id);

        this->id = rhs.id;
        rhs.id = nullptr;

        return *this;
    }

    const ALCchar *ALDevice::getName() const {
        if (alcIsExtensionPresent(nullptr, "ALC_ENUMERATION_EXT") == AL_TRUE) {
            return alcGetString(id, ALC_DEVICE_SPECIFIER);
        } else if (exceptionLevel > 0) {
            throw std::runtime_error("Cannot get name of ALDevice: ALC_ENUMERATION_EXT not present!");
        } else {
            return nullptr;
        }
    }

    ALContext::ALContext(ALDevice *dev, ALCint *attribs) {
        id = alcCreateContext(dev->id, attribs);
    }

    ALMicrophone::ALMicrophone(const ALCchar *name, ALCuint freq, ALSoundFormat fmt, ALCsizei capbufSize) {
        id = alcCaptureOpenDevice(name, freq, fmt, capbufSize);
        if (id == nullptr) {
            STMS_ERROR("Failed to open ALMicrophone: {}", name == nullptr ? "nullptr" : name);
            if (exceptionLevel > 0) {
                throw std::runtime_error("Cannot open ALMicrophone!");
            }
        }
    }

    ALMicrophone::~ALMicrophone() {
        alcCaptureCloseDevice(id);
    }

    void ALMicrophone::capture(ALCvoid *dataBuf, ALCsizei numSamples) {
        alcCaptureSamples(id, dataBuf, numSamples);
    }

    const ALCchar *ALMicrophone::getName() const {
        if (alcIsExtensionPresent(nullptr, "ALC_ENUMERATION_EXT") == AL_TRUE) {
            return alcGetString(id, ALC_CAPTURE_DEVICE_SPECIFIER);
        } else if (exceptionLevel > 0) {
            throw std::runtime_error("Cannot get name of ALMicrophone: ALC_ENUMERATION_EXT not found.");
        } else {
            return nullptr;
        }
    }
}
