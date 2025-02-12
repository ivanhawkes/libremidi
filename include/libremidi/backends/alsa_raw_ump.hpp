#pragma once
#include <libremidi/backends/alsa_raw/observer.hpp>
#include <libremidi/backends/alsa_raw_ump/midi_in.hpp>
#include <libremidi/backends/dummy.hpp>

namespace libremidi::alsa_raw_ump
{
struct backend
{
  using midi_in = midi_in_impl;
  using midi_out = midi_out_dummy;
  using midi_observer = alsa_raw::observer_impl;
  using midi_in_configuration = alsa_raw_ump::input_configuration;
  using midi_out_configuration = dummy_configuration;
  using midi_observer_configuration = alsa_raw_observer_configuration;
  static const constexpr auto API = libremidi::API::ALSA_RAW_UMP;
  static const constexpr auto name = "alsa_raw_ump";
  static const constexpr auto display_name = "ALSA (raw UMP)";
};
}
