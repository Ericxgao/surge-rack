// SPDX-License-Identifier: GPL-3.0-or-later
#include "CoreModules/CoreProcessor.hh"
#include "CoreModules/register_module.hh"
#include "engine.h"
#include <array>
#include <string>

#include "oscillator_processor.h"
#include "oscillator_panel.h"
#include "utility_panels.h"
#include "modulation_panels.h"
#include "quad_lfo_panel.h"
#include "eg_vca_panel.h"
#include "filter_panels.h"
#include "phaser_panel.h"
#include "flanger_panel.h"
#include "rotary_panel.h"
#include "chorus_panel.h"
#include "ensemble_panel.h"
#include "distortion_panel.h"
#include "neuron_panel.h"
#include "chow_panel.h"
#include "exciter_panel.h"
#include "bonsai_panel.h"
#include "frequency_shifter_panel.h"
#include "ringmod_panel.h"
#include "treemonster_panel.h"
#include "vocoder_panel.h"
#include "resonator_panel.h"
#include "combulator_panel.h"
#include "spring_panel.h"
#include "nimbus_panel.h"
#include "tuned_delay_panel.h"
#include "tuned_delay_plus_panel.h"
#include "delay_panel.h"
#include "lfo_panel.h"
#include "reverb_panel.h"
#include "reverb2_panel.h"
#include "unison_panels.h"
extern "C" void init()
{
    register_direct_oscillator<surge_mm::Model::Classic>("Classic", "SurgeXTNative/Classic.png");
    register_direct_oscillator<surge_mm::Model::Modern>("Modern", "SurgeXTNative/Modern.png");
    register_direct_oscillator<surge_mm::Model::Sine>("Sine", "SurgeXTNative/Sine.png");
    register_direct_oscillator<surge_mm::Model::FM2>("FM2", "SurgeXTNative/FM2.png");
    register_direct_oscillator<surge_mm::Model::FM3>("FM3", "SurgeXTNative/FM3.png");
    register_direct_oscillator<surge_mm::Model::SHNoise>("SHNoise", "SurgeXTNative/SHNoise.png");
    register_direct_oscillator<surge_mm::Model::Alias>("Alias", "SurgeXTNative/Alias.png");
    register_direct_oscillator<surge_mm::Model::String>("String", "SurgeXTNative/String.png");
    register_direct_oscillator<surge_mm::Model::Twist>("Twist", "SurgeXTNative/Twist.png");
    register_direct_oscillator<surge_mm::Model::Wavetable>("Wavetable",
                                                           "SurgeXTNative/Wavetable.png");
    register_direct_oscillator<surge_mm::Model::Window>("Window", "SurgeXTNative/Window.png");
    register_mixer<false>();
    register_mixer<true>();
    register_digital_ring();
    register_mod_matrix();
    register_quad_ad();
    register_quad_lfo();
    register_eg_vca();
    register_filter<surge_mm::FilterModule::VCF>();
    register_filter<surge_mm::FilterModule::Waveshaper>();
    register_phaser();
    register_flanger();
    register_rotary();
    register_chorus();
    register_ensemble();
    register_distortion();
    register_neuron();
    register_chow();
    register_exciter();
    register_bonsai();
    register_frequency_shifter();
    register_ringmod();
    register_treemonster();
    register_vocoder();
    register_resonator();
    register_combulator();
    register_spring();
    register_nimbus();
    register_tuned_delay();
    register_tuned_delay_plus();
    register_delay();
    register_lfo();
    register_reverb();
    register_reverb2();
    register_unison_helper();
    register_unison_expander();
}
