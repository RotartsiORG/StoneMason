
[comment]: # (This is a markdown document, but can still be read in plaintext.
              If you're seeing this, then you're reading the plaintext version.)

# StoneMason TODO list

### Physics
1. Learn Bullet Physics (bullet3: https://github.com/bulletphysics/bullet3) 

### Networking
1. ~~Learn OpenSSL (https://github.com/openssl/openssl)~~
2. Make DTLSServer/Client code protocol agnostic (work with DTLS/UDP or TLS/TCP )
3. Raw server with no OpenSSL encryption with same interface.
4. Make TCP-Over-UDP???? 
    - Implement delivery verification, anti-replay, anti-reordering, automatic fragmenting, heartbeats etc
 
### Audio
1. Device Enumeration support (both playback and capture devices)
2. Audio capturing

### Rendering
1. Basic OpenGL render
2. Font rendering, custom gui lib (or is ImGUI good enough?)
3. Vulkan rendering 
    - Use VMA?
    - Follow https://vulkan-tutorial.com/, then implement things from https://learnopengl.com/
3. DirectX 12? Metal? IDK.

### General TODO
- Add unit tests for timers (Stopwatch & TPSTimer)
- Add support for posting to urls /w libCurl.
- passwords /w libCurl
