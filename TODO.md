
[comment]: # (This is a markdown document, but can still be read in plaintext.
              If you're seeing this, then you're reading the plaintext version.)

# StoneMason TODO list

### Physics
1. Learn Bullet Physics (bullet3: https://github.com/bulletphysics/bullet3) 

### Networking
1. Make DTLSServer/Client code protocol agnostic (work with DTLS/UDP or TLS/TCP )
    - Add ability to kick any client based on uuid
2. Raw server with no OpenSSL encryption with same interface.
3. Add support for kicking clients & list of all clients (by uuid). Also add support for blockUntilMessageFrom

### Audio
1. Device Enumeration support (both playback and capture devices)
2. Audio capturing

### Rendering
1. Basic OpenGL render
    - Multi colored text in 1 draw call (freetype module)
2. Is font rendering API complete enough?
2. Custom gui lib (or is ImGUI good enough?)
3. Vulkan rendering 
    - Use VMA.
    - Follow https://vulkan-tutorial.com/, then implement things from https://learnopengl.com/

### Curl
1. Make libcurl interface into a class (based on easy_handler thing)
2. Support for passwords, POSTing, etc. etc. (https://curl.haxx.se/libcurl/c/libcurl-tutorial.html)

### Tests
- Add unit tests for timers (Stopwatch & TPSTimer)
- Operator overload tests for UUID & other types


####Probably wont
1. Make TCP-Over-UDP???? 
    - Implement delivery verification, anti-replay, anti-reordering, automatic fragmenting, heartbeats etc
2. Procedural sound generation (pure tones of any Hz /w square, sine, sawtooth waves).
3. Classes for AudioResource, TexResource etc to represent files instead of having GLTexture & shader, & al audio directly load file?
4. DirectX 12? Metal? IDK.