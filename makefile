# mypeek: buf_decoder.c mypeek.c client.c signing.c coding.c sockaddr.c uri.c schedule.c name.c charbuf.c keystore.c
# 	cc buf_decoder.c client.c hashtb.c signing.c coding.c sockaddr.c uri.c schedule.c name.c charbuf.c keystore.c mypeek.c -lcrypto -o mypeek
#

CC=gcc
CFLAGS=-Wall
LDFLAGS=-lcrypto
# SOURCES=mypeek.c client.c signing.c coding.c hashtb.c sockaddr.c uri.c\
# 	schedule.c name.c charbuf.c keystore.c buf_decoder.c indexbuf.c\
# 	interest.c forwarding.c
# OBJECTS=$(SOURCES:.c=.o)
EXECUTABLE=mypeek
OBJ = mypeek.o hashtb.o ndn_bloom.o ndn_buf_decoder.o ndn_buf_encoder.o ndn_charbuf.o ndn_client.o ndn_coding.o ndn_digest.o\
	ndn_indexbuf.o ndn_interest.o ndn_keystore.o ndn_match.o ndn_name_util.o ndn_reg_mgmt.o\
	ndn_schedule.o ndn_setup_sockaddr_un.o ndn_signing.o ndn_sockaddrutil.o ndn_uri.o ndn_versioning.o

# all: $(SOURCES) $(EXECUTABLE)
#
# $(EXECUTABLE): $(OBJECTS)
# 	$(CC) $(LDFLAGS) $(OBJECTS) -o $@

# .c.o:
# 	$(CC) $(CFLAGS) $< -o $@

%.o: %.c
	$(CC) -c -o $@ $< $(CFLAGS)

$(EXECUTABLE): $(OBJ)
	$(CC) -o $(EXECUTABLE) $(OBJ) $(CFLAGS) $(LDFLAGS)

clean:
	rm -rf $(OBJ) $(EXECUTABLE)
