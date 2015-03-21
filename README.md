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

  - _-i_ **30** - How many seconds to sleep between runs.
  - _-c_ **/etc/tinybolo.conf** - Path to the configuration file.
  - _-e_ **tcp://127.0.0.1:2999** - Bolo endpoint to submit to.
  - _-F_ - Don't daemonize; stay in the foreground.
  - _-D_ - Enable debugging output, to standard error.
