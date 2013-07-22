Introduction
============

This module enables the sending of [Statsd](http:/github.com/etsy/statsd) statistics directly
from Apache, without the need for a CustomLog processor. It will
send one counter and one timer per request received.

For a request to _www.example.com/foo/bar?baz=42_, the stat format
would be:

```
  <prefix.>foo.bar.GET.200<.suffix>
```

Where \<prefix> and \<suffix> are optionally configured (see [DOCUMENTATION](DOCUMENTATION)).
The path gets converted from **/foo/bar** to **foo.bar** and the HTTP method
(GET) and the response code (200) are also part the stat.

The module is implemented as a logging hook into Apache's runtime,
meaning it runs after your request backend request is completed
and data is already being sent to the client.

On my very mediocre VM on my laptop, the average overhead per call
was **0.4** milliseconds, so any server grade hardware you have should
be able to do better than that.

I've written a companion module for [Varnish](http:/varnish-cache.org) as well called
[libvmod-statsd](https://github.com/jib/libvmod-statsd) in case you're running Varnish instead/also.

Installation
============

Make sure you have **apxs2** and **perl** installed, which on Ubuntu you can get by running:

```
$ sudo apt-get install apache2-dev perl
```

From the checkout directory run:

```
$ sudo ./build.pl
```

This will build, install & enable the module on your system

Configuration
=============

See the [DOCUMENTATION](DOCUMENTATION) file in the same directory as this README for an
example configuration as well as all the documentation on the configuration directives 
supported.


