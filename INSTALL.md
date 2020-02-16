
[comment]: # (This is a markdown document, but can still be read in plaintext.
              If you're seeing this, then you're reading the plaintext version.)
              
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
4. `cd` to the StoneMason project root and run `bash run.sh` or equivalent.
5. _\[OPTIONAL\]_ If you wish to install StoneMason, simply run `sudo make install`



### Tested Operating Systems
1. *\[Ubuntu 19.10\]*: Main development platform, thoroughly tested. Works 100%
2. *\[Mac OSX Catalina 10.15\]*: Will test later. Should work with minor tweaks.
3. *\[Windows 10\]*: Hopeless. I gave up. Windows is a godforsaken wasteland.