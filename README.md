tinybolo
========

tinybolo is a small-footprint daemon that runs metrics collectors
and reports data up to a central bolo daemon.

Installation
------------

Standard conventions apply:

    ./configure
    make
    make install

Options
-------

The following options are supported:

  - **-i** _30_ - How many seconds to sleep between runs.
  - **-c** _/etc/tinybolo.conf_ - Path to the configuration file.
  - **-e** _tcp://127.0.0.1:2999_ - Bolo endpoint to submit to.
  - **-F** - Don't daemonize; stay in the foreground.
  - **-D** - Enable debugging output, to standard error.
