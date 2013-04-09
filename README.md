mini-torrent
============

A BitTorrent client implemented for CS 342 (Computer Networks)

### From Cynthia's instructions:

To compile the files:

1. You need first install OpenSSL and libcurl3-dev on your machine(use sudo apt-get)
2. Then go into bencode-tools-2011-03-15, run 
	"./configure" and run "make".
3. After that, go back to the hw4 folder and run make again. You don't need to change Makefile.

The first part of your work is to make this template work using 'select()'. 

After you have done that, you need to make it work with peers, handling handshakes, and 
other kind of requests. 

You can test on the torrents cs342files.torrent and Cute-Kittens-kitten.....torrent.  Both of these are being seeded from a machine running on the cs network.  In order to test these torrents, you will need to be running on a machine connected to the cs network, such as a lab machine.
