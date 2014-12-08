ps3-cloud-drive
===============

An open source cloud sync utility for Playstation 3 games saves.

You will need the ps3toolchain setup and PSL1GHT working to compile this.
I have committed the required libraries including the SSL libraries to the
ps3toolchain.

You will need to create a Google Drive application using the Google Services
console. Enter the client_secret and app_id into the main.cpp file before 
attempting compilation.

A note about the root certificate mentioned in the source code. The root certificate
was taken from the polarssl library. The newer distributions do not carry a root
certificate, so you can either take one from the ps3 hard drive or from an available
openssl package. I cannot package it into the repo.

Portlibs required to compile:
(look for this commit on ps3libraries https://github.com/mhaqs/ps3libraries/commit/bc2de847dad3e79677aa2acdab6ea07256ef2a66)

- Updated libcurl to the latest version 7.31.0.
- Ported polarssl 1.2.8 to psl1ght.
- Reorganized ps3libraries script order to compile curl with polarssl support. SSL support is enabled in curl.
- Ported lib json-c to psl1ght.
- Updated patches for libcurl, polarssl for compilation.
- Updated execution rights on new scripts.
- zlib script execution fix introduced in last commit


For usage see: http://blog.tabinda.net/playstation-3/playstation-3-cloud-drive/
