mini-torrent
============

A BitTorrent client implemented for CS 342 (Computer Networks)

### From Cynthia's instructions:

To compile the files:

1. You need first install OpenSSL and libcurl3-dev on your machine(use
sudo apt-get)
2. Then go into bencode-tools-2011-03-15, run 
	"./configure" and run "make".
3. After that, go back to the hw4 folder and run make again. You don't
need to change Makefile.

The first part of your work is to make this template work using
'select()'.

After you have done that, you need to make it work with peers,
handling handshakes, and other kind of requests.

You can test on the torrents cs342files.torrent and
Cute-Kittens-kitten.....torrent.  Both of these are being seeded from
a machine running on the cs network.  In order to test these torrents,
you will need to be running on a machine connected to the cs network,
such as a lab machine.

### Bugs and Implementation Notes

1. From time to time, downloading will hang for awhile. This could
just due to being choked by the one peer I need to download the next
chunk. In any case, fear not! Downloading usually resumes in a few
seconds; just be patient.
2. If you start seeing a lot of "connection refused" messages, don't
have too many hopes of your download completing. This has happened
only while downloading the cs342_partial.torrent, where I get refused
sometimes by EVERY SINGLE PEER. I don't know why this is. It could be
my code. Or it could be bullying by peers.

### Honor Code

I adhered to the Honor Code in this assignment.

Luke Lovett
