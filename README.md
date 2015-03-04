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

On my very mediocre VM on my laptop, the average overhead per call was **400** microseconds,
and on an AWS [c1.medium](http://docs.aws.amazon.com/AWSEC2/latest/UserGuide/instance-types.html)
about **20** microseconds per call. Any high end server hardware should be able to perform
better than that.

I've written a companion module for [Varnish](http:/varnish-cache.org) as well called
[libvmod-statsd](https://github.com/jib/libvmod-statsd) in case you're running Varnish instead/also.

Installation
============

Source
------

Make sure you have **apxs2** and **perl** installed, which on Ubuntu you can get by running:

```
$ sudo apt-get install apache2-dev perl
```

From the checkout directory run:

```
$ sudo ./build.pl --install
```

This will build, install & enable the module on your system

Debian/Ubuntu
-------------

You can install a Debian package from the Krux OpenSource repository
by adding this to your apt sources.list (there is nothing lucid specific
in the packaging; it should work on any other Debian/Ubuntu install):

```
deb http://ops.krxd.net/apt/foss lucid production
deb-src http://ops.krxd.net/apt/foss lucid production
```

And then running the following installation command:

```
$ sudo apt-get install libapache2-mod-statsd
```

Building your own package
-------------------------

Make sure you have **dpkg-dev**, **cdbs** and **debhelper** installed, which on Ubuntu you can get by running:

```
$ sudo apt-get install dpkg-dev cdbs debhelper
```

Then build the package by first compiling the module, then running buildpackage:

```
$ perl build.pl
$ dpkg-buildpackage -d -b
```

Configuration
=============

See the [DOCUMENTATION](DOCUMENTATION) file in the same directory as this README for an
example configuration as well as all the documentation on the configuration directives
supported.


