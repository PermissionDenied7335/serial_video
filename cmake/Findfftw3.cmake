cmake_minimum_required(VERSION 2.8.12)

find_package(PkgConfig REQUIRED)
if(PkgConfig_FOUND)

    pkg_check_modules(FFTW3 REQUIRED IMPORTED_TARGET fftw3)
    set(fftw3_LIBS PkgConfig::FFTW3)
    set(fftw3_FOUND TRUE)

endif()
