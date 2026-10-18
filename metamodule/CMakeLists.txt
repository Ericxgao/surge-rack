cmake_minimum_required(VERSION 3.22)
if(NOT METAMODULE_SDK_DIR)
    if(DEFINED ENV{METAMODULE_SDK_DIR})
        set(METAMODULE_SDK_DIR "$ENV{METAMODULE_SDK_DIR}")
    else()
        set(METAMODULE_SDK_DIR "${CMAKE_CURRENT_LIST_DIR}/../../metamodule-plugin-sdk")
    endif()
endif()
include("${METAMODULE_SDK_DIR}/plugin.cmake")
# The SDK packages plugin.json verbatim. Keep the displayed and build versions
# together, and reconfigure automatically whenever the metadata changes.
file(READ "${CMAKE_CURRENT_LIST_DIR}/plugin.json" SURGE_MM_METADATA_JSON)
string(JSON SURGE_MM_VERSION GET "${SURGE_MM_METADATA_JSON}" version)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${CMAKE_CURRENT_LIST_DIR}/plugin.json")
project(SurgeXTNative VERSION "${SURGE_MM_VERSION}" LANGUAGES C CXX)
set(SURGE_DIR "${CMAKE_CURRENT_LIST_DIR}/../surge")
set(COMMON "${SURGE_DIR}/src/common")
# Checked-in embedded adaptations of the pinned Surge sources.
# No preparation scripts run at configure time.
set(SURGE_MM_RUNTIME "${CMAKE_CURRENT_LIST_DIR}/runtime")
set(ALIAS_SOURCE "${SURGE_MM_RUNTIME}/AliasOscillator.cpp")
set(STRING_SOURCE "${SURGE_MM_RUNTIME}/StringOscillator.cpp")
set(TWIST_SOURCE "${SURGE_MM_RUNTIME}/TwistOscillator.cpp")
set(EURORACK "${SURGE_DIR}/libs/eurorack/eurorack")
set(PLAITS_COMPAT "${SURGE_MM_RUNTIME}/plaits-compat")
set(TABLE_GENERATED "${SURGE_MM_RUNTIME}/table-generated")
set(FILTER_COMPAT "${SURGE_MM_RUNTIME}/filter-compat")
set(EFFECT_COMPAT "${SURGE_MM_RUNTIME}/effect-compat")
file(GLOB_RECURSE PLAITS_SOURCES CONFIGURE_DEPENDS "${EURORACK}/plaits/dsp/*.cc")
set(NIMBUS_SRC_SOURCES
    ${SURGE_DIR}/libs/libsamplerate/src/samplerate.c
    ${SURGE_DIR}/libs/libsamplerate/src/src_sinc.c
    ${SURGE_DIR}/libs/libsamplerate/src/src_linear.c
    ${SURGE_DIR}/libs/libsamplerate/src/src_zoh.c)
set_source_files_properties(${NIMBUS_SRC_SOURCES} PROPERTIES COMPILE_DEFINITIONS
    "HAVE_STDBOOL_H=1;ENABLE_SINC_FAST_CONVERTER;PACKAGE=\"libsamplerate\";VERSION=\"0.2.1\"")
add_library(surge-mm-dsp STATIC
    ${NIMBUS_SRC_SOURCES}
    ${EURORACK}/clouds/dsp/correlator.cc
    ${EURORACK}/clouds/dsp/granular_processor.cc
    ${EURORACK}/clouds/dsp/mu_law.cc
    ${EURORACK}/clouds/dsp/pvoc/frame_transformation.cc
    ${EURORACK}/clouds/dsp/pvoc/phase_vocoder.cc
    ${EURORACK}/clouds/dsp/pvoc/stft.cc
    ${EURORACK}/clouds/resources.cc
    dsp/lfo_dsp.cpp
    dsp/delay_dsp.cpp
    dsp/tuned_delay_plus_dsp.cpp
    dsp/tuned_delay_dsp.cpp
    dsp/nimbus_dsp.cpp
    dsp/reverb_dsp.cpp
    dsp/reverb2_dsp.cpp
    dsp/spring_dsp.cpp
    dsp/combulator_dsp.cpp
    dsp/resonator_dsp.cpp
    dsp/vocoder_dsp.cpp
    dsp/treemonster_dsp.cpp
    dsp/ringmod_dsp.cpp
    dsp/ringmod_sine.cpp
    dsp/frequency_shifter_dsp.cpp
    dsp/bonsai_dsp.cpp
    dsp/exciter_dsp.cpp
    dsp/chow_dsp.cpp
    dsp/neuron_dsp.cpp
    dsp/distortion_dsp.cpp
    dsp/ensemble_dsp.cpp
    dsp/chorus_dsp.cpp
    dsp/rotary_dsp.cpp
    dsp/flanger_dsp.cpp
    dsp/phaser_dsp.cpp
    dsp/engine.cpp
    dsp/table_data.cpp
    ${TABLE_GENERATED}/Wavetable.cpp
    ${TABLE_GENERATED}/WindowOscillator.cpp
    ${TABLE_GENERATED}/table_resources.cpp
    ${COMMON}/dsp/oscillators/WavetableOscillator.cpp
    ${ALIAS_SOURCE}
    ${STRING_SOURCE}
    ${TWIST_SOURCE}
    ${PLAITS_SOURCES}
    ${EURORACK}/plaits/resources.cc
    ${EURORACK}/stmlib/dsp/units.cc
    ${EURORACK}/stmlib/dsp/atan.cc
    ${PLAITS_COMPAT}/stmlib/utils/random.cc
    dsp/filter_dsp.cpp
    dsp/utility_dsp.cpp
    dsp/envelope_dsp.cpp
    dsp/quad_lfo_dsp.cpp
    dsp/eg_vca_dsp.cpp
    dsp/unison_dsp.cpp
    ${COMMON}/dsp/oscillators/ClassicOscillator.cpp
    ${COMMON}/dsp/oscillators/SampleAndHoldOscillator.cpp
    ${COMMON}/dsp/oscillators/FM2Oscillator.cpp
    ${COMMON}/dsp/oscillators/FM3Oscillator.cpp
    ${COMMON}/dsp/oscillators/ModernOscillator.cpp
    ${COMMON}/dsp/oscillators/SineOscillator.cpp)
set_target_properties(surge-mm-dsp PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED YES)
target_compile_definitions(surge-mm-dsp PRIVATE SURGE_COMPILE_BLOCK_SIZE=8 TEST=1)
target_include_directories(surge-mm-dsp PRIVATE
    ${SURGE_DIR}/libs/libsamplerate/include
    ${EFFECT_COMPAT} ${FILTER_COMPAT} ${PLAITS_COMPAT} ${EURORACK} ${SURGE_DIR}/libs/eurorack/special_include
    dsp/compat ${COMMON} ${COMMON}/dsp ${COMMON}/dsp/oscillators ${COMMON}/dsp/utilities
    ${COMMON}/dsp/vembertech ${COMMON}/dsp/filters ${SURGE_DIR}/src ${SURGE_DIR}/libs/simde
    ${SURGE_DIR}/libs/sst/sst-basic-blocks/include
    ${SURGE_DIR}/libs/sst/sst-effects/include
    ${SURGE_DIR}/libs/fmt/include
    ${SURGE_DIR}/libs/sst/sst-filters/include
    ${SURGE_DIR}/libs/sst/sst-waveshapers/include)
target_include_directories(surge-mm-dsp PUBLIC dsp)
add_library(SurgeXTNative STATIC plugin.cpp)
set_target_properties(SurgeXTNative PROPERTIES CXX_STANDARD 23 CXX_STANDARD_REQUIRED YES)
target_link_libraries(SurgeXTNative PRIVATE surge-mm-dsp)
if(NOT INSTALL_DIR)
    set(INSTALL_DIR "${CMAKE_BINARY_DIR}/metamodule-plugins")
endif()
create_plugin(SOURCE_LIB SurgeXTNative PLUGIN_NAME SurgeXTNative
    PLUGIN_JSON ${CMAKE_CURRENT_LIST_DIR}/plugin.json
    SOURCE_ASSETS ${CMAKE_CURRENT_LIST_DIR}/assets DESTINATION ${INSTALL_DIR})
if(TARGET arch_mp15x_a7)
    # The SDK's custom ELF link consumes surge-mm-dsp objects but only
    # declares the top-level archive as a file dependency. A DSP-only edit
    # must relink the ELF before packaging, even if plugin.cpp is unchanged.
    add_custom_command(OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/SurgeXTNative-debug.so"
        APPEND DEPENDS surge-mm-dsp "$<TARGET_FILE:surge-mm-dsp>")
endif()
