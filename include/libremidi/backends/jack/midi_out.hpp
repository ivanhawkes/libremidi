#pragma once
#include <libremidi/backends/jack/config.hpp>
#include <libremidi/backends/jack/helpers.hpp>
#include <libremidi/detail/midi_out.hpp>

#include <semaphore>
namespace libremidi
{

class midi_out_jack final
    : public midi_out_api
    , private jack_helpers
    , public error_handler
{
public:
  struct
      : output_configuration
      , jack_output_configuration
  {
  } configuration;

  midi_out_jack(output_configuration&& conf, jack_output_configuration&& apiconf)
      : configuration{std::move(conf), std::move(apiconf)}
  {
    this->port = nullptr;
    this->client = nullptr;

    connect();
  }

  ~midi_out_jack() override
  {
    midi_out_jack::close_port();

    // Cleanup
    jack_ringbuffer_free(this->buffSize);
    jack_ringbuffer_free(this->buffMessage);
    if (this->client)
    {
      jack_client_close(this->client);
    }
  }

  void set_client_name(std::string_view) override
  {
    warning(configuration, "midi_out_jack: set_client_name unsupported");
  }

  libremidi::API get_current_api() const noexcept override { return libremidi::API::UNIX_JACK; }

  void open_port(unsigned int portNumber, std::string_view portName) override
  {
    if (!check_port_name_length(*this, configuration.client_name, portName))
      return;

    connect();

    // Creating new port
    if (!this->port)
      this->port = jack_port_register(
          this->client, portName.data(), JACK_DEFAULT_MIDI_TYPE, JackPortIsOutput, 0);

    if (!this->port)
    {
      error<driver_error>(configuration, "midi_out_jack::open_port: JACK error creating port");
      return;
    }

    // Connecting to the output
    std::string name = get_port_name(portNumber);
    jack_connect(this->client, jack_port_name(this->port), name.c_str());

    connected_ = true;
  }

  void open_virtual_port(std::string_view portName) override
  {
    if (!check_port_name_length(*this, configuration.client_name, portName))
      return;

    connect();
    if (this->port == nullptr)
      this->port = jack_port_register(
          this->client, portName.data(), JACK_DEFAULT_MIDI_TYPE, JackPortIsOutput, 0);

    if (this->port == nullptr)
    {
      error<driver_error>(
          configuration, "midi_out_jack::open_virtual_port: JACK error creating virtual port");
    }
  }

  void close_port() override
  {
    using namespace std::literals;
    if (this->port == nullptr)
      return;

    this->sem_needpost.release();
    this->sem_cleanup.try_acquire_for(1s);

    jack_port_unregister(this->client, this->port);
    this->port = nullptr;

    connected_ = false;
  }

  void set_port_name(std::string_view portName) override
  {
#if defined(LIBREMIDI_JACK_HAS_PORT_RENAME)
    jack_port_rename(this->client, this->port, portName.data());
#else
    jack_port_set_name(this->port, portName.data());
#endif
  }

  unsigned int get_port_count() const override
  {
    int count = 0;
    if (!this->client)
      return 0;

    // List of available ports
    const char** ports
        = jack_get_ports(this->client, nullptr, JACK_DEFAULT_MIDI_TYPE, JackPortIsInput);

    if (ports == nullptr)
      return 0;
    while (ports[count] != nullptr)
      count++;

    jack_free(ports);

    return count;
  }

  std::string get_port_name(unsigned int portNumber) const override
  {
    // List of available ports
    unique_handle<const char*, jack_free> ports{
        jack_get_ports(this->client, nullptr, JACK_DEFAULT_MIDI_TYPE, JackPortIsInput)};

    return jack_helpers::get_port_name(*this, ports.get(), portNumber);
  }

  void send_message(const unsigned char* message, size_t size) override
  {
    int nBytes = static_cast<int>(size);

    // Write full message to buffer
    jack_ringbuffer_write(this->buffMessage, (const char*)message, nBytes);
    jack_ringbuffer_write(this->buffSize, (char*)&nBytes, sizeof(nBytes));
  }

private:
  void connect()
  {
    if (this->client)
      return;

    // Initialize output ringbuffers
    this->buffSize = jack_ringbuffer_create(midi_out_jack::ringbuffer_size);
    this->buffMessage = jack_ringbuffer_create(midi_out_jack::ringbuffer_size);

    // Initialize JACK client
    this->client = jack_client_open(configuration.client_name.c_str(), JackNoStartServer, nullptr);
    if (this->client == nullptr)
    {
      warning(configuration, "midi_out_jack::initialize: JACK server not running?");
      return;
    }

    jack_set_process_callback(this->client, jackProcessOut, this);
    jack_activate(this->client);
  }

  static int jackProcessOut(jack_nframes_t nframes, void* arg)
  {
    auto& self = *(midi_out_jack*)arg;

    // Is port created?
    if (self.port == nullptr)
      return 0;

    void* buff = jack_port_get_buffer(self.port, nframes);
    jack_midi_clear_buffer(buff);

    while (jack_ringbuffer_read_space(self.buffSize) > 0)
    {
      int space{};
      jack_ringbuffer_read(self.buffSize, (char*)&space, sizeof(int));
      auto midiData = jack_midi_event_reserve(buff, 0, space);

      jack_ringbuffer_read(self.buffMessage, (char*)midiData, space);
    }

    if (!self.sem_needpost.try_acquire())
      self.sem_cleanup.release();

    return 0;
  }

private:
  static const constexpr auto ringbuffer_size = 16384;

  jack_client_t* client{};
  jack_port_t* port{};

  jack_ringbuffer_t* buffSize{};
  jack_ringbuffer_t* buffMessage{};

  std::counting_semaphore<> sem_cleanup{0};
  std::counting_semaphore<> sem_needpost{0};
};

}
