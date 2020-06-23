
[comment]: # (This is a markdown document, but can still be read in plaintext.
              If you're seeing this, then you're reading the plaintext version.)


#THIS DOCUMENT PROBABLY WON'T BE UPDATED! EMAIL ME IF YOU HAVE TROUBLE (rotartsi0482@gmail.com)

#Building StoneMason
1. Download StoneMason and make sure that the submodules are cloned.
    - Method 1: Use `git clone --recursive https://www.github.com/RotartsiORG/StoneMason`
    - Method 2: If you already cloned the project, just run `git submodule init` followed by `git submodule update`
2. Ensure that OpenAL is installed and findable by CMake.
3. Ensure that the Vulkan SDK is installed and findable by CMake.
    - Download it from https://vulkan.lunarg.com/
    - Follow instructions on how to properly install it. 
        - On Linux, be sure to `source setup-env.sh` in your `~/.bashrc`
          or appropriate startup file in order to set the environment variables. 
4. Make a recent version of OpenSSL is installed and findable by CMake. 
    - I built a version of OpenSSL 1.1.1g for use. I used 
    `./config no-deprecated --release zlib threads enable-buildtest-c++`. See OpenSSL's `INSTALL` file for more info.
      I had a issue where CMake was finding an older version,
      which I solved by uninstalling the old version.
     - STMS is multi-threaded and uses OpenSSL. Please compile with `thread`
     - STMS requires an OpenSSL version of AT LEAST 1.1.1a. Of course, the newer the better.
5.  Install CMake. Preferably the newest version. I tested with CMake 3.17.1
    - You might have to modify `CMK_EXE` in `run.sh` to point to your CMake executable.
      For me, it is set to `/snap/bin/cmake`
5. `cd` to the StoneMason project root and run `bash run.sh` or equivalent.
6. _\[OPTIONAL\]_ If you wish to install StoneMason, simply run `sudo make install`



### Tested Operating Systems
1. *\[Ubuntu 19.10\]*: Main development platform, thoroughly tested. Works 100%
2. *\[Mac OSX Catalina 10.15\]*: Will test later. Should work with minor tweaks.
