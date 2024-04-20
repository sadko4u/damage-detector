# Audio Damage Detector

This GStreamer plugin detects short audio level drops in the stream, computes the number of drops among the
specified period and sends notification when the number of drops exceeds the specified threshold.

## Algorithm

The plugin estimates RMS of the input signal for a period defined by the 'reactivity' parameter.
After that, it looks at the level of the RMS signal.

At the start, the plugin is in CLOSED state. It analyzes the computed RMS value and ff the RMS level
is above the specified threshold (see `threshold` parameter), it waits for 1/10 of reactivity time to
ensure that the signal went above the threshold, and turns into OPEN state. The last timestamp when the
plugin has turned into OPEN state is recorded. When the RMS level drops below the specified threshold,
the plugin waits 1/10 of reacitvity time to ensure that the signal went below the threshold, and turns
into CLOSED state. The timestamp when the plugin has turned into CLOSED state is also recorded.

When the plugin turnes into CLOSED state, it computes the difference between the time of last turn into
CLOSED state and the time of last turn into OPEN state. If the difference is less than `d_time` parameter,
it is considered to be a potential stream corruption problem, additional event is pushed into the event
queue.

After that, the plugin computes the overall number of events in the queue for a period specified by the
`e_time` parameter. If the number of events is above the specified threshold (see `ev_threshold`
parameter), then the plugin starts to periodically send GStreamer messages about the stream corruption
with the time interval specified by the `ev_period` parameter. The plugin also emits GStreamer message
when the number of events goes below the threshold, but these messages are generated once until
the number of events exceeds the specified threshold again.

## Properties

Properties available for reading/writing:

* threshold - RMS signal trigger threshold (dB);
* reactivity - The time period of the RMS value calculation (ms);
* d_time - Audio signal corruption detection time (s);
* e_time - Estimation time window for calculating number of corruption events (s);
* ev_threshold - The number of events that trigger notifications;
* ev_period - Notification send period (s).

Properties available for reading:
* events - the current number of corruption events.

## Messages

The plugin generates one type of the GStreamer message - `stream-corruption-state` with the following
fields:
  * corrupted - the indicator that the plugin detected stream corruption (boolean);
  * timestamp - the time stamp (in samples) relative to the start of the plugin when the corruption was detected.

## Usage

Simple usage case when processing audio files in RIFF format:

```
gst-launch-1.0 filesrc location=input.wav ! wavparse ! audioconvert ! damage_detector ! wavenc ! filesink location=output.wav
```

## Building

To build the plugin, perform the following commands:

```bash
make config # Configure the build
make fetch # Fetch dependencies from Git repository
make
sudo make install
```

To get more build options, run:

```bash
make help
```

To uninstall plugin, simply issue:

```bash
make uninstall
```

To clean all binary files, run:

```bash
make clean
```

To clean the whole project tree including configuration files, run:

```bash
make prune
```

To fetch all possible dependencies and make the source code tree portable between
different architectures and platforms, run:

```bash
make tree
```

To build source code archive with all possible dependencies, run:

```bash
make distsrc
```


