#pragma once
//*********************************************************************//
//  API: LINUX ALSA SEQUENCER
//*********************************************************************//

// API information found at:
//   - http://www.alsa-project.org/documentation.php#Library

// The ALSA Sequencer API is based on the use of a callback function for
// MIDI input.
//
// Thanks to Pedro Lopez-Cabanillas for help with the ALSA sequencer
// time stamps and other assorted fixes!!!

#include <libremidi/backends/alsa_seq/midi_in.hpp>
#include <libremidi/backends/alsa_seq/midi_out.hpp>
#include <libremidi/backends/alsa_seq/observer.hpp>

namespace libremidi::alsa_seq
{

struct backend
{
  using midi_in = midi_in_impl;
  using midi_out = midi_out_impl;
  using midi_observer = observer_impl;
  using midi_in_configuration = alsa_seq::input_configuration;
  using midi_out_configuration = alsa_seq::output_configuration;
  using midi_observer_configuration = alsa_seq::observer_configuration;
  static const constexpr auto API = libremidi::API::ALSA_SEQ;
  static const constexpr auto name = "alsa_seq";
  static const constexpr auto display_name = "ALSA (sequencer)";
};

}
