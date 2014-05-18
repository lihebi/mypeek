#include <stdio.h>
#include <sys/types.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/time.h>
#include <unistd.h>
#include <errno.h>

// #include <ndn/ndn.h>

// #include "ndn_client.h"
// #include "coding.h"
// #include "hashtb.h"
// #include "charbuf.h"
// #include "ndn.h"

#include <ndn/bloom.h>
#include <ndn/ndn.h>
#include <ndn/charbuf.h>
#include <ndn/uri.h>


int main(int argc, char** argv) {
  int res;
  struct ndn *h = NULL;
  struct ndn_charbuf *name = NULL;
  struct ndn_charbuf *resultbuf = NULL;
  struct ndn_charbuf *templ = NULL;
  int timeout_ms = 3000;
  struct ndn_parsed_ContentObject pcobuf = { 0 };
  int get_flags = 0;
  const unsigned char *ptr;
  size_t length;



  name = ndn_charbuf_create();
  ndn_name_from_uri(name, argv[1]);
  h = ndn_create();
  res = ndn_connect(h, NULL);
  resultbuf = ndn_charbuf_create();
  res = ndn_get(h, name, templ, timeout_ms, resultbuf, &pcobuf, NULL, get_flags);
  ptr = resultbuf->buf;
  length = resultbuf->length;
  ndn_content_get_value(ptr, length, &pcobuf, &ptr, &length);
  fwrite(ptr, length, 1, stdout) - 1;
}
