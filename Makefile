###############################################################################
#																			  #
# Makefile for OS project 6										     	      #
#																			  #	
# Gal Zohar ,galzo ,  gal.zohar1@mail.huji.ac.il				              #	
#																			  #
###############################################################################

SRC=clftp.cpp srftp.cpp

TAR=tar
TARFLAGS=-cvf
TARNAME=ex6.tar
TARSRC=$(SRC) Makefile README performance.jpg

REMOVE=rm
REMOVEFLAGS=-f

default: all
all: clftp srftp

clftp: clftp.cpp
	g++ -Wall -std=c++11 clftp.cpp -o clftp

srftp: srftp.cpp
	g++ -Wall -std=c++11 srftp.cpp -o srftp
	
tar:
	$(TAR) $(TARFLAGS) $(TARNAME) $(TARSRC)
	
clean:
	$(REMOVE) $(REMOVEFLAGS) $(TARNAME) clftp srftp
	
.PHONY: clean tar
