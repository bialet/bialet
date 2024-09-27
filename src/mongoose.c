// Copyright (c) 2004-2013 Sergey Lyubka
// Copyright (c) 2013-2022 Cesanta Software Limited
// All rights reserved
//
// This software is dual-licensed: you can redistribute it and/or modify
// it under the terms of the GNU General Public License version 2 as
// published by the Free Software Foundation. For the terms of this
// license, see http://www.gnu.org/licenses/
//
// You are free to use this software under the terms of the GNU General
// Public License, but WITHOUT ANY WARRANTY; without even the implied
// warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
// See the GNU General Public License for more details.
//
// Alternatively, you can license this software under a commercial
// license, as set out in https://www.mongoose.ws/licensing/
//
// SPDX-License-Identifier: GPL-2.0-only or commercial
#include "bialet.h"
#include "favicon.h"

extern const unsigned char favicon_data[];
extern const size_t        favicon_data_len;

#define MG_ENABLE_DIRLIST 0

#include "bialet_wren.h"
#include "mongoose.h"

#ifdef MG_ENABLE_LINES
#line 1 "src/base64.c"
#endif

static int mg_base64_encode_single(int c) {
  if(c < 26) {
    return c + 'A';
  } else if(c < 52) {
    return c - 26 + 'a';
  } else if(c < 62) {
    return c - 52 + '0';
  } else {
    return c == 62 ? '+' : '/';
  }
}

static int mg_base64_decode_single(int c) {
  if(c >= 'A' && c <= 'Z') {
    return c - 'A';
  } else if(c >= 'a' && c <= 'z') {
    return c + 26 - 'a';
  } else if(c >= '0' && c <= '9') {
    return c + 52 - '0';
  } else if(c == '+') {
    return 62;
  } else if(c == '/') {
    return 63;
  } else if(c == '=') {
    return 64;
  } else {
    return -1;
  }
}

size_t mg_base64_update(unsigned char ch, char* to, size_t n) {
  unsigned long rem = (n & 3) % 3;
  if(rem == 0) {
    to[n] = (char)mg_base64_encode_single(ch >> 2);
    to[++n] = (char)((ch & 3) << 4);
  } else if(rem == 1) {
    to[n] = (char)mg_base64_encode_single(to[n] | (ch >> 4));
    to[++n] = (char)((ch & 15) << 2);
  } else {
    to[n] = (char)mg_base64_encode_single(to[n] | (ch >> 6));
    to[++n] = (char)mg_base64_encode_single(ch & 63);
    n++;
  }
  return n;
}

size_t mg_base64_final(char* to, size_t n) {
  size_t saved = n;
  // printf("---[%.*s]\n", n, to);
  if(n & 3)
    n = mg_base64_update(0, to, n);
  if((saved & 3) == 2)
    n--;
  // printf("    %d[%.*s]\n", n, n, to);
  while(n & 3)
    to[n++] = '=';
  to[n] = '\0';
  return n;
}

size_t mg_base64_encode(const unsigned char* p, size_t n, char* to, size_t dl) {
  size_t i, len = 0;
  if(dl > 0)
    to[0] = '\0';
  if(dl < ((n / 3) + (n % 3 ? 1 : 0)) * 4 + 1)
    return 0;
  for(i = 0; i < n; i++)
    len = mg_base64_update(p[i], to, len);
  len = mg_base64_final(to, len);
  return len;
}

size_t mg_base64_decode(const char* src, size_t n, char* dst, size_t dl) {
  const char* end = src == NULL ? NULL : src + n; // Cannot add to NULL
  size_t      len = 0;
  if(dl > 0)
    dst[0] = '\0';
  if(dl < n / 4 * 3 + 1)
    return 0;
  while(src != NULL && src + 3 < end) {
    int a = mg_base64_decode_single(src[0]), b = mg_base64_decode_single(src[1]),
        c = mg_base64_decode_single(src[2]), d = mg_base64_decode_single(src[3]);
    if(a == 64 || a < 0 || b == 64 || b < 0 || c < 0 || d < 0)
      return 0;
    dst[len++] = (char)((a << 2) | (b >> 4));
    if(src[2] != '=') {
      dst[len++] = (char)((b << 4) | (c >> 2));
      if(src[3] != '=')
        dst[len++] = (char)((c << 6) | d);
    }
    src += 4;
  }
  dst[len] = '\0';
  return len;
}

#ifdef MG_ENABLE_LINES
#line 1 "src/dns.c"
#endif

struct dns_data {
  struct dns_data*      next;
  struct mg_connection* c;
  uint64_t              expire;
  uint16_t              txnid;
};

static void mg_sendnsreq(struct mg_connection*, struct mg_str*, int, struct mg_dns*,
                         bool);

static void mg_dns_free(struct mg_connection* c, struct dns_data* d) {
  LIST_DELETE(struct dns_data, (struct dns_data**)&c->mgr->active_dns_requests, d);
  free(d);
}

void mg_resolve_cancel(struct mg_connection* c) {
  struct dns_data *tmp, *d = (struct dns_data*)c->mgr->active_dns_requests;
  for(; d != NULL; d = tmp) {
    tmp = d->next;
    if(d->c == c)
      mg_dns_free(c, d);
  }
}

static size_t mg_dns_parse_name_depth(const uint8_t* s, size_t len, size_t ofs,
                                      char* to, size_t tolen, size_t j, int depth) {
  size_t i = 0;
  if(tolen > 0 && depth == 0)
    to[0] = '\0';
  if(depth > 5)
    return 0;
  // MG_INFO(("ofs %lx %x %x", (unsigned long) ofs, s[ofs], s[ofs + 1]));
  while(ofs + i + 1 < len) {
    size_t n = s[ofs + i];
    if(n == 0) {
      i++;
      break;
    }
    if(n & 0xc0) {
      size_t ptr = (((n & 0x3f) << 8) | s[ofs + i + 1]); // 12 is hdr len
      // MG_INFO(("PTR %lx", (unsigned long) ptr));
      if(ptr + 1 < len && (s[ptr] & 0xc0) == 0 &&
         mg_dns_parse_name_depth(s, len, ptr, to, tolen, j, depth + 1) == 0)
        return 0;
      i += 2;
      break;
    }
    if(ofs + i + n + 1 >= len)
      return 0;
    if(j > 0) {
      if(j < tolen)
        to[j] = '.';
      j++;
    }
    if(j + n < tolen)
      memcpy(&to[j], &s[ofs + i + 1], n);
    j += n;
    i += n + 1;
    if(j < tolen)
      to[j] = '\0'; // Zero-terminate this chunk
    // MG_INFO(("--> [%s]", to));
  }
  if(tolen > 0)
    to[tolen - 1] = '\0'; // Make sure make sure it is nul-term
  return i;
}

static size_t mg_dns_parse_name(const uint8_t* s, size_t n, size_t ofs, char* dst,
                                size_t dstlen) {
  return mg_dns_parse_name_depth(s, n, ofs, dst, dstlen, 0, 0);
}

size_t mg_dns_parse_rr(const uint8_t* buf, size_t len, size_t ofs, bool is_question,
                       struct mg_dns_rr* rr) {
  const uint8_t *s = buf + ofs, *e = &buf[len];

  memset(rr, 0, sizeof(*rr));
  if(len < sizeof(struct mg_dns_header))
    return 0; // Too small
  if(len > 512)
    return 0; //  Too large, we don't expect that
  if(s >= e)
    return 0; //  Overflow

  if((rr->nlen = (uint16_t)mg_dns_parse_name(buf, len, ofs, NULL, 0)) == 0)
    return 0;
  s += rr->nlen + 4;
  if(s > e)
    return 0;
  rr->atype = (uint16_t)(((uint16_t)s[-4] << 8) | s[-3]);
  rr->aclass = (uint16_t)(((uint16_t)s[-2] << 8) | s[-1]);
  if(is_question)
    return (size_t)(rr->nlen + 4);

  s += 6;
  if(s > e)
    return 0;
  rr->alen = (uint16_t)(((uint16_t)s[-2] << 8) | s[-1]);
  if(s + rr->alen > e)
    return 0;
  return (size_t)(rr->nlen + rr->alen + 10);
}

bool mg_dns_parse(const uint8_t* buf, size_t len, struct mg_dns_message* dm) {
  const struct mg_dns_header* h = (struct mg_dns_header*)buf;
  struct mg_dns_rr            rr;
  size_t                      i, n, ofs = sizeof(*h);
  memset(dm, 0, sizeof(*dm));

  if(len < sizeof(*h))
    return 0; // Too small, headers dont fit
  if(mg_ntohs(h->num_questions) > 1)
    return 0; // Sanity
  if(mg_ntohs(h->num_answers) > 10)
    return 0; // Sanity
  dm->txnid = mg_ntohs(h->txnid);

  for(i = 0; i < mg_ntohs(h->num_questions); i++) {
    if((n = mg_dns_parse_rr(buf, len, ofs, true, &rr)) == 0)
      return false;
    // MG_INFO(("Q %lu %lu %hu/%hu", ofs, n, rr.atype, rr.aclass));
    ofs += n;
  }
  for(i = 0; i < mg_ntohs(h->num_answers); i++) {
    if((n = mg_dns_parse_rr(buf, len, ofs, false, &rr)) == 0)
      return false;
    // MG_INFO(("A -- %lu %lu %hu/%hu %s", ofs, n, rr.atype, rr.aclass,
    // dm->name));
    mg_dns_parse_name(buf, len, ofs, dm->name, sizeof(dm->name));
    ofs += n;

    if(rr.alen == 4 && rr.atype == 1 && rr.aclass == 1) {
      dm->addr.is_ip6 = false;
      memcpy(&dm->addr.ip, &buf[ofs - 4], 4);
      dm->resolved = true;
      break; // Return success
    } else if(rr.alen == 16 && rr.atype == 28 && rr.aclass == 1) {
      dm->addr.is_ip6 = true;
      memcpy(&dm->addr.ip, &buf[ofs - 16], 16);
      dm->resolved = true;
      break; // Return success
    }
  }
  return true;
}

static void dns_cb(struct mg_connection* c, int ev, void* ev_data, void* fn_data) {
  struct dns_data *d, *tmp;
  if(ev == MG_EV_POLL) {
    uint64_t now = *(uint64_t*)ev_data;
    for(d = (struct dns_data*)c->mgr->active_dns_requests; d != NULL; d = tmp) {
      tmp = d->next;
      // MG_DEBUG ("%lu %lu dns poll", d->expire, now));
      if(now > d->expire)
        mg_error(d->c, "DNS timeout");
    }
  } else if(ev == MG_EV_READ) {
    struct mg_dns_message dm;
    int                   resolved = 0;
    if(mg_dns_parse(c->recv.buf, c->recv.len, &dm) == false) {
      MG_ERROR(("Unexpected DNS response:"));
      mg_hexdump(c->recv.buf, c->recv.len);
    } else {
      // MG_VERBOSE(("%s %d", dm.name, dm.resolved));
      for(d = (struct dns_data*)c->mgr->active_dns_requests; d != NULL; d = tmp) {
        tmp = d->next;
        // MG_INFO(("d %p %hu %hu", d, d->txnid, dm.txnid));
        if(dm.txnid != d->txnid)
          continue;
        if(d->c->is_resolving) {
          if(dm.resolved) {
            dm.addr.port = d->c->rem.port; // Save port
            d->c->rem = dm.addr;           // Copy resolved address
            MG_DEBUG(("%lu %s is %M", d->c->id, dm.name, mg_print_ip, &d->c->rem));
            mg_connect_resolved(d->c);
#if MG_ENABLE_IPV6
          } else if(dm.addr.is_ip6 == false && dm.name[0] != '\0' &&
                    c->mgr->use_dns6 == false) {
            struct mg_str x = mg_str(dm.name);
            mg_sendnsreq(d->c, &x, c->mgr->dnstimeout, &c->mgr->dns6, true);
#endif
          } else {
            mg_error(d->c, "%s DNS lookup failed", dm.name);
          }
        } else {
          MG_ERROR(("%lu already resolved", d->c->id));
        }
        mg_dns_free(c, d);
        resolved = 1;
      }
    }
    if(!resolved)
      MG_ERROR(("stray DNS reply"));
    c->recv.len = 0;
  } else if(ev == MG_EV_CLOSE) {
    for(d = (struct dns_data*)c->mgr->active_dns_requests; d != NULL; d = tmp) {
      tmp = d->next;
      mg_error(d->c, "DNS error");
      mg_dns_free(c, d);
    }
  }
  (void)fn_data;
}

static bool mg_dns_send(struct mg_connection* c, const struct mg_str* name,
                        uint16_t txnid, bool ipv6) {
  struct {
    struct mg_dns_header header;
    uint8_t              data[256];
  } pkt;
  size_t i, n;
  memset(&pkt, 0, sizeof(pkt));
  pkt.header.txnid = mg_htons(txnid);
  pkt.header.flags = mg_htons(0x100);
  pkt.header.num_questions = mg_htons(1);
  for(i = n = 0; i < sizeof(pkt.data) - 5; i++) {
    if(name->ptr[i] == '.' || i >= name->len) {
      pkt.data[n] = (uint8_t)(i - n);
      memcpy(&pkt.data[n + 1], name->ptr + n, i - n);
      n = i + 1;
    }
    if(i >= name->len)
      break;
  }
  memcpy(&pkt.data[n], "\x00\x00\x01\x00\x01", 5); // A query
  n += 5;
  if(ipv6)
    pkt.data[n - 3] = 0x1c; // AAAA query
  // memcpy(&pkt.data[n], "\xc0\x0c\x00\x1c\x00\x01", 6);  // AAAA query
  // n += 6;
  return mg_send(c, &pkt, sizeof(pkt.header) + n);
}

static void mg_sendnsreq(struct mg_connection* c, struct mg_str* name, int ms,
                         struct mg_dns* dnsc, bool ipv6) {
  struct dns_data* d = NULL;
  if(dnsc->url == NULL) {
    mg_error(c, "DNS server URL is NULL. Call mg_mgr_init()");
  } else if(dnsc->c == NULL) {
    dnsc->c = mg_connect(c->mgr, dnsc->url, NULL, NULL);
    if(dnsc->c != NULL) {
      dnsc->c->pfn = dns_cb;
      // dnsc->c->is_hexdumping = 1;
    }
  }
  if(dnsc->c == NULL) {
    mg_error(c, "resolver");
  } else if((d = (struct dns_data*)calloc(1, sizeof(*d))) == NULL) {
    mg_error(c, "resolve OOM");
  } else {
    struct dns_data* reqs = (struct dns_data*)c->mgr->active_dns_requests;
    d->txnid = reqs ? (uint16_t)(reqs->txnid + 1) : 1;
    d->next = (struct dns_data*)c->mgr->active_dns_requests;
    c->mgr->active_dns_requests = d;
    d->expire = mg_millis() + (uint64_t)ms;
    d->c = c;
    c->is_resolving = 1;
    MG_VERBOSE(("%lu resolving %.*s @ %s, txnid %hu", c->id, (int)name->len,
                name->ptr, dnsc->url, d->txnid));
    if(!mg_dns_send(dnsc->c, name, d->txnid, ipv6)) {
      mg_error(dnsc->c, "DNS send");
    }
  }
}

void mg_resolve(struct mg_connection* c, const char* url) {
  struct mg_str host = mg_url_host(url);
  c->rem.port = mg_htons(mg_url_port(url));
  if(mg_aton(host, &c->rem)) {
    // host is an IP address, do not fire name resolution
    mg_connect_resolved(c);
  } else {
    // host is not an IP, send DNS resolution request
    struct mg_dns* dns = c->mgr->use_dns6 ? &c->mgr->dns6 : &c->mgr->dns4;
    mg_sendnsreq(c, &host, c->mgr->dnstimeout, dns, c->mgr->use_dns6);
  }
}

#ifdef MG_ENABLE_LINES
#line 1 "src/event.c"
#endif

void mg_call(struct mg_connection* c, int ev, void* ev_data) {
  // Run user-defined handler first, in order to give it an ability
  // to intercept processing (e.g. clean input buffer) before the
  // protocol handler kicks in
  if(c->fn != NULL)
    c->fn(c, ev, ev_data, c->fn_data);
  if(c->pfn != NULL)
    c->pfn(c, ev, ev_data, c->pfn_data);
}

void mg_error(struct mg_connection* c, const char* fmt, ...) {
  char    buf[64];
  va_list ap;
  va_start(ap, fmt);
  mg_vsnprintf(buf, sizeof(buf), fmt, &ap);
  va_end(ap);
  MG_ERROR(("%lu %p %s", c->id, c->fd, buf));
  c->is_closing = 1;            // Set is_closing before sending MG_EV_CALL
  mg_call(c, MG_EV_ERROR, buf); // Let user handler to override it
}

#ifdef MG_ENABLE_LINES
#line 1 "src/fmt.c"
#endif

static bool is_digit(int c) {
  return c >= '0' && c <= '9';
}

static int addexp(char* buf, int e, int sign) {
  int n = 0;
  buf[n++] = 'e';
  buf[n++] = (char)sign;
  if(e > 400)
    return 0;
  if(e < 10)
    buf[n++] = '0';
  if(e >= 100)
    buf[n++] = (char)(e / 100 + '0'), e -= 100 * (e / 100);
  if(e >= 10)
    buf[n++] = (char)(e / 10 + '0'), e -= 10 * (e / 10);
  buf[n++] = (char)(e + '0');
  return n;
}

static int xisinf(double x) {
  union {
    double   f;
    uint64_t u;
  } ieee754 = {x};
  return ((unsigned)(ieee754.u >> 32) & 0x7fffffff) == 0x7ff00000 &&
         ((unsigned)ieee754.u == 0);
}

static int xisnan(double x) {
  union {
    double   f;
    uint64_t u;
  } ieee754 = {x};
  return ((unsigned)(ieee754.u >> 32) & 0x7fffffff) + ((unsigned)ieee754.u != 0) >
         0x7ff00000;
}

static size_t mg_dtoa(char* dst, size_t dstlen, double d, int width, bool tz) {
  char   buf[40];
  int    i, s = 0, n = 0, e = 0;
  double t, mul, saved;
  if(d == 0.0)
    return mg_snprintf(dst, dstlen, "%s", "0");
  if(xisinf(d))
    return mg_snprintf(dst, dstlen, "%s", d > 0 ? "inf" : "-inf");
  if(xisnan(d))
    return mg_snprintf(dst, dstlen, "%s", "nan");
  if(d < 0.0)
    d = -d, buf[s++] = '-';

  // Round
  saved = d;
  mul = 1.0;
  while(d >= 10.0 && d / mul >= 10.0)
    mul *= 10.0;
  while(d <= 1.0 && d / mul <= 1.0)
    mul /= 10.0;
  for(i = 0, t = mul * 5; i < width; i++)
    t /= 10.0;
  d += t;
  // Calculate exponent, and 'mul' for scientific representation
  mul = 1.0;
  while(d >= 10.0 && d / mul >= 10.0)
    mul *= 10.0, e++;
  while(d < 1.0 && d / mul < 1.0)
    mul /= 10.0, e--;
  // printf(" --> %g %d %g %g\n", saved, e, t, mul);

  if(e >= width && width > 1) {
    n = (int)mg_dtoa(buf, sizeof(buf), saved / mul, width, tz);
    // printf(" --> %.*g %d [%.*s]\n", 10, d / t, e, n, buf);
    n += addexp(buf + s + n, e, '+');
    return mg_snprintf(dst, dstlen, "%.*s", n, buf);
  } else if(e <= -width && width > 1) {
    n = (int)mg_dtoa(buf, sizeof(buf), saved / mul, width, tz);
    // printf(" --> %.*g %d [%.*s]\n", 10, d / mul, e, n, buf);
    n += addexp(buf + s + n, -e, '-');
    return mg_snprintf(dst, dstlen, "%.*s", n, buf);
  } else {
    for(i = 0, t = mul; t >= 1.0 && s + n < (int)sizeof(buf); i++) {
      int ch = (int)(d / t);
      if(n > 0 || ch > 0)
        buf[s + n++] = (char)(ch + '0');
      d -= ch * t;
      t /= 10.0;
    }
    // printf(" --> [%g] -> %g %g (%d) [%.*s]\n", saved, d, t, n, s + n, buf);
    if(n == 0)
      buf[s++] = '0';
    while(t >= 1.0 && n + s < (int)sizeof(buf))
      buf[n++] = '0', t /= 10.0;
    if(s + n < (int)sizeof(buf))
      buf[n + s++] = '.';
    // printf(" 1--> [%g] -> [%.*s]\n", saved, s + n, buf);
    for(i = 0, t = 0.1; s + n < (int)sizeof(buf) && n < width; i++) {
      int ch = (int)(d / t);
      buf[s + n++] = (char)(ch + '0');
      d -= ch * t;
      t /= 10.0;
    }
  }
  while(tz && n > 0 && buf[s + n - 1] == '0')
    n--; // Trim trailing zeroes
  if(n > 0 && buf[s + n - 1] == '.')
    n--; // Trim trailing dot
  n += s;
  if(n >= (int)sizeof(buf))
    n = (int)sizeof(buf) - 1;
  buf[n] = '\0';
  return mg_snprintf(dst, dstlen, "%s", buf);
}

static size_t mg_lld(char* buf, int64_t val, bool is_signed, bool is_hex) {
  const char* letters = "0123456789abcdef";
  uint64_t    v = (uint64_t)val;
  size_t      s = 0, n, i;
  if(is_signed && val < 0)
    buf[s++] = '-', v = (uint64_t)(-val);
  // This loop prints a number in reverse order. I guess this is because we
  // write numbers from right to left: least significant digit comes last.
  // Maybe because we use Arabic numbers, and Arabs write RTL?
  if(is_hex) {
    for(n = 0; v; v >>= 4)
      buf[s + n++] = letters[v & 15];
  } else {
    for(n = 0; v; v /= 10)
      buf[s + n++] = letters[v % 10];
  }
  // Reverse a string
  for(i = 0; i < n / 2; i++) {
    char t = buf[s + i];
    buf[s + i] = buf[s + n - i - 1], buf[s + n - i - 1] = t;
  }
  if(val == 0)
    buf[n++] = '0'; // Handle special case
  return n + s;
}

static size_t scpy(void (*out)(char, void*), void* ptr, char* buf, size_t len) {
  size_t i = 0;
  while(i < len && buf[i] != '\0')
    out(buf[i++], ptr);
  return i;
}

size_t mg_xprintf(void (*out)(char, void*), void* ptr, const char* fmt, ...) {
  size_t  len = 0;
  va_list ap;
  va_start(ap, fmt);
  len = mg_vxprintf(out, ptr, fmt, &ap);
  va_end(ap);
  return len;
}

size_t mg_vxprintf(void (*out)(char, void*), void* param, const char* fmt,
                   va_list* ap) {
  size_t i = 0, n = 0;
  while(fmt[i] != '\0') {
    if(fmt[i] == '%') {
      size_t j, k, x = 0, is_long = 0, w = 0 /* width */, pr = ~0U /* prec */;
      char   pad = ' ', minus = 0, c = fmt[++i];
      if(c == '#')
        x++, c = fmt[++i];
      if(c == '-')
        minus++, c = fmt[++i];
      if(c == '0')
        pad = '0', c = fmt[++i];
      while(is_digit(c))
        w *= 10, w += (size_t)(c - '0'), c = fmt[++i];
      if(c == '.') {
        c = fmt[++i];
        if(c == '*') {
          pr = (size_t)va_arg(*ap, int);
          c = fmt[++i];
        } else {
          pr = 0;
          while(is_digit(c))
            pr *= 10, pr += (size_t)(c - '0'), c = fmt[++i];
        }
      }
      while(c == 'h')
        c = fmt[++i]; // Treat h and hh as int
      if(c == 'l') {
        is_long++, c = fmt[++i];
        if(c == 'l')
          is_long++, c = fmt[++i];
      }
      if(c == 'p')
        x = 1, is_long = 1;
      if(c == 'd' || c == 'u' || c == 'x' || c == 'X' || c == 'p' || c == 'g' ||
         c == 'f') {
        bool   s = (c == 'd'), h = (c == 'x' || c == 'X' || c == 'p');
        char   tmp[40];
        size_t xl = x ? 2 : 0;
        if(c == 'g' || c == 'f') {
          double v = va_arg(*ap, double);
          if(pr == ~0U)
            pr = 6;
          k = mg_dtoa(tmp, sizeof(tmp), v, (int)pr, c == 'g');
        } else if(is_long == 2) {
          int64_t v = va_arg(*ap, int64_t);
          k = mg_lld(tmp, v, s, h);
        } else if(is_long == 1) {
          long v = va_arg(*ap, long);
          k = mg_lld(tmp, s ? (int64_t)v : (int64_t)(unsigned long)v, s, h);
        } else {
          int v = va_arg(*ap, int);
          k = mg_lld(tmp, s ? (int64_t)v : (int64_t)(unsigned)v, s, h);
        }
        for(j = 0; j < xl && w > 0; j++)
          w--;
        for(j = 0; pad == ' ' && !minus && k < w && j + k < w; j++)
          n += scpy(out, param, &pad, 1);
        n += scpy(out, param, (char*)"0x", xl);
        for(j = 0; pad == '0' && k < w && j + k < w; j++)
          n += scpy(out, param, &pad, 1);
        n += scpy(out, param, tmp, k);
        for(j = 0; pad == ' ' && minus && k < w && j + k < w; j++)
          n += scpy(out, param, &pad, 1);
      } else if(c == 'm' || c == 'M') {
        mg_pm_t f = va_arg(*ap, mg_pm_t);
        if(c == 'm')
          out('"', param);
        n += f(out, param, ap);
        if(c == 'm')
          n += 2, out('"', param);
      } else if(c == 'c') {
        int ch = va_arg(*ap, int);
        out((char)ch, param);
        n++;
      } else if(c == 's') {
        char* p = va_arg(*ap, char*);
        if(pr == ~0U)
          pr = p == NULL ? 0 : strlen(p);
        for(j = 0; !minus && pr < w && j + pr < w; j++)
          n += scpy(out, param, &pad, 1);
        n += scpy(out, param, p, pr);
        for(j = 0; minus && pr < w && j + pr < w; j++)
          n += scpy(out, param, &pad, 1);
      } else if(c == '%') {
        out('%', param);
        n++;
      } else {
        out('%', param);
        out(c, param);
        n += 2;
      }
      i++;
    } else {
      out(fmt[i], param), n++, i++;
    }
  }
  return n;
}

#ifdef MG_ENABLE_LINES
#line 1 "src/fs.c"
#endif

struct mg_fd* mg_fs_open(struct mg_fs* fs, const char* path, int flags) {
  struct mg_fd* fd = (struct mg_fd*)calloc(1, sizeof(*fd));
  if(fd != NULL) {
    fd->fd = fs->op(path, flags);
    fd->fs = fs;
    if(fd->fd == NULL) {
      free(fd);
      fd = NULL;
    }
  }
  return fd;
}

void mg_fs_close(struct mg_fd* fd) {
  if(fd != NULL) {
    fd->fs->cl(fd->fd);
    free(fd);
  }
}

char* mg_file_read(struct mg_fs* fs, const char* path, size_t* sizep) {
  struct mg_fd* fd;
  char*         data = NULL;
  size_t        size = 0;
  fs->st(path, &size, NULL);
  if((fd = mg_fs_open(fs, path, MG_FS_READ)) != NULL) {
    data = (char*)calloc(1, size + 1);
    if(data != NULL) {
      if(fs->rd(fd->fd, data, size) != size) {
        free(data);
        data = NULL;
      } else {
        data[size] = '\0';
        if(sizep != NULL)
          *sizep = size;
      }
    }
    mg_fs_close(fd);
  }
  return data;
}

bool mg_file_write(struct mg_fs* fs, const char* path, const void* buf, size_t len) {
  bool          result = false;
  struct mg_fd* fd;
  char          tmp[MG_PATH_MAX];
  mg_snprintf(tmp, sizeof(tmp), "%s..%d", path, rand());
  if((fd = mg_fs_open(fs, tmp, MG_FS_WRITE)) != NULL) {
    result = fs->wr(fd->fd, buf, len) == len;
    mg_fs_close(fd);
    if(result) {
      fs->rm(path);
      fs->mv(tmp, path);
    } else {
      fs->rm(tmp);
    }
  }
  return result;
}

bool mg_file_printf(struct mg_fs* fs, const char* path, const char* fmt, ...) {
  va_list ap;
  char*   data;
  bool    result = false;
  va_start(ap, fmt);
  data = mg_vmprintf(fmt, &ap);
  va_end(ap);
  result = mg_file_write(fs, path, data, strlen(data));
  free(data);
  return result;
}

#ifdef MG_ENABLE_LINES
#line 1 "src/fs_posix.c"
#endif

#if MG_ENABLE_FILE

#ifndef MG_STAT_STRUCT
#define MG_STAT_STRUCT stat
#endif

#ifndef MG_STAT_FUNC
#define MG_STAT_FUNC stat
#endif

static int p_stat(const char* path, size_t* size, time_t* mtime) {
#if !defined(S_ISDIR)
  MG_ERROR(("stat() API is not supported. %p %p %p", path, size, mtime));
  return 0;
#else
#if MG_ARCH == MG_ARCH_WIN32
  struct _stati64 st;
  wchar_t         tmp[MG_PATH_MAX];
  MultiByteToWideChar(CP_UTF8, 0, path, -1, tmp, sizeof(tmp) / sizeof(tmp[0]));
  if(_wstati64(tmp, &st) != 0)
    return 0;
  // If path is a symlink, windows reports 0 in st.st_size.
  // Get a real file size by opening it and jumping to the end
  if(st.st_size == 0 && (st.st_mode & _S_IFREG)) {
    FILE* fp = _wfopen(tmp, L"rb");
    if(fp != NULL) {
      fseek(fp, 0, SEEK_END);
      if(ftell(fp) > 0)
        st.st_size = ftell(fp); // Use _ftelli64 on win10+
      fclose(fp);
    }
  }
#else
  struct MG_STAT_STRUCT st;
  if(MG_STAT_FUNC(path, &st) != 0)
    return 0;
#endif
  if(size)
    *size = (size_t)st.st_size;
  if(mtime)
    *mtime = st.st_mtime;
  return MG_FS_READ | MG_FS_WRITE | (S_ISDIR(st.st_mode) ? MG_FS_DIR : 0);
#endif
}

#if MG_ARCH == MG_ARCH_WIN32
struct dirent {
  char d_name[MAX_PATH];
};

typedef struct win32_dir {
  HANDLE           handle;
  WIN32_FIND_DATAW info;
  struct dirent    result;
} DIR;

int gettimeofday(struct timeval* tv, void* tz) {
  FILETIME         ft;
  unsigned __int64 tmpres = 0;

  if(tv != NULL) {
    GetSystemTimeAsFileTime(&ft);
    tmpres |= ft.dwHighDateTime;
    tmpres <<= 32;
    tmpres |= ft.dwLowDateTime;
    tmpres /= 10; // convert into microseconds
    tmpres -= (int64_t)11644473600000000;
    tv->tv_sec = (long)(tmpres / 1000000UL);
    tv->tv_usec = (long)(tmpres % 1000000UL);
  }
  (void)tz;
  return 0;
}

static int to_wchar(const char* path, wchar_t* wbuf, size_t wbuf_len) {
  int  ret;
  char buf[MAX_PATH * 2], buf2[MAX_PATH * 2], *p;
  strncpy(buf, path, sizeof(buf));
  buf[sizeof(buf) - 1] = '\0';
  // Trim trailing slashes. Leave backslash for paths like "X:\"
  p = buf + strlen(buf) - 1;
  while(p > buf && p[-1] != ':' && (p[0] == '\\' || p[0] == '/'))
    *p-- = '\0';
  memset(wbuf, 0, wbuf_len * sizeof(wchar_t));
  ret = MultiByteToWideChar(CP_UTF8, 0, buf, -1, wbuf, (int)wbuf_len);
  // Convert back to Unicode. If doubly-converted string does not match the
  // original, something is fishy, reject.
  WideCharToMultiByte(CP_UTF8, 0, wbuf, (int)wbuf_len, buf2, sizeof(buf2), NULL,
                      NULL);
  if(strcmp(buf, buf2) != 0) {
    wbuf[0] = L'\0';
    ret = 0;
  }
  return ret;
}

DIR* opendir(const char* name) {
  DIR*    d = NULL;
  wchar_t wpath[MAX_PATH];
  DWORD   attrs;

  if(name == NULL) {
    SetLastError(ERROR_BAD_ARGUMENTS);
  } else if((d = (DIR*)calloc(1, sizeof(*d))) == NULL) {
    SetLastError(ERROR_NOT_ENOUGH_MEMORY);
  } else {
    to_wchar(name, wpath, sizeof(wpath) / sizeof(wpath[0]));
    attrs = GetFileAttributesW(wpath);
    if(attrs != 0Xffffffff && (attrs & FILE_ATTRIBUTE_DIRECTORY)) {
      (void)wcscat(wpath, L"\\*");
      d->handle = FindFirstFileW(wpath, &d->info);
      d->result.d_name[0] = '\0';
    } else {
      free(d);
      d = NULL;
    }
  }
  return d;
}

int closedir(DIR* d) {
  int result = 0;
  if(d != NULL) {
    if(d->handle != INVALID_HANDLE_VALUE)
      result = FindClose(d->handle) ? 0 : -1;
    free(d);
  } else {
    result = -1;
    SetLastError(ERROR_BAD_ARGUMENTS);
  }
  return result;
}

struct dirent* readdir(DIR* d) {
  struct dirent* result = NULL;
  if(d != NULL) {
    memset(&d->result, 0, sizeof(d->result));
    if(d->handle != INVALID_HANDLE_VALUE) {
      result = &d->result;
      WideCharToMultiByte(CP_UTF8, 0, d->info.cFileName, -1, result->d_name,
                          sizeof(result->d_name), NULL, NULL);
      if(!FindNextFileW(d->handle, &d->info)) {
        FindClose(d->handle);
        d->handle = INVALID_HANDLE_VALUE;
      }
    } else {
      SetLastError(ERROR_FILE_NOT_FOUND);
    }
  } else {
    SetLastError(ERROR_BAD_ARGUMENTS);
  }
  return result;
}
#endif

static void p_list(const char* dir, void (*fn)(const char*, void*), void* userdata) {
#if MG_ENABLE_DIRLIST
  struct dirent* dp;
  DIR*           dirp;
  if((dirp = (opendir(dir))) == NULL)
    return;
  while((dp = readdir(dirp)) != NULL) {
    if(!strcmp(dp->d_name, ".") || !strcmp(dp->d_name, ".."))
      continue;
    fn(dp->d_name, userdata);
  }
  closedir(dirp);
#else
  (void)dir, (void)fn, (void)userdata;
#endif
}

static void* p_open(const char* path, int flags) {
#if MG_ARCH == MG_ARCH_WIN32
  const char* mode = flags == MG_FS_READ ? "rb" : "a+b";
  wchar_t     b1[MG_PATH_MAX], b2[10];
  MultiByteToWideChar(CP_UTF8, 0, path, -1, b1, sizeof(b1) / sizeof(b1[0]));
  MultiByteToWideChar(CP_UTF8, 0, mode, -1, b2, sizeof(b2) / sizeof(b2[0]));
  return (void*)_wfopen(b1, b2);
#else
  const char* mode = flags == MG_FS_READ ? "rbe" : "a+be"; // e for CLOEXEC
  return (void*)fopen(path, mode);
#endif
}

static void p_close(void* fp) {
  fclose((FILE*)fp);
}

static size_t p_read(void* fp, void* buf, size_t len) {
  return fread(buf, 1, len, (FILE*)fp);
}

static size_t p_write(void* fp, const void* buf, size_t len) {
  return fwrite(buf, 1, len, (FILE*)fp);
}

static size_t p_seek(void* fp, size_t offset) {
#if(defined(_FILE_OFFSET_BITS) && _FILE_OFFSET_BITS == 64) ||                       \
    (defined(_POSIX_C_SOURCE) && _POSIX_C_SOURCE >= 200112L) ||                     \
    (defined(_XOPEN_SOURCE) && _XOPEN_SOURCE >= 600)
  if(fseeko((FILE*)fp, (off_t)offset, SEEK_SET) != 0)
    (void)0;
#else
  if(fseek((FILE*)fp, (long)offset, SEEK_SET) != 0)
    (void)0;
#endif
  return (size_t)ftell((FILE*)fp);
}

static bool p_rename(const char* from, const char* to) {
  return rename(from, to) == 0;
}

static bool p_remove(const char* path) {
  return remove(path) == 0;
}

static bool p_mkdir(const char* path) {
  return mkdir(path, 0775) == 0;
}

#else

static int p_stat(const char* path, size_t* size, time_t* mtime) {
  (void)path, (void)size, (void)mtime;
  return 0;
}
static void p_list(const char* path, void (*fn)(const char*, void*),
                   void*       userdata) {
  (void)path, (void)fn, (void)userdata;
}
static void* p_open(const char* path, int flags) {
  (void)path, (void)flags;
  return NULL;
}
static void p_close(void* fp) {
  (void)fp;
}
static size_t p_read(void* fd, void* buf, size_t len) {
  (void)fd, (void)buf, (void)len;
  return 0;
}
static size_t p_write(void* fd, const void* buf, size_t len) {
  (void)fd, (void)buf, (void)len;
  return 0;
}
static size_t p_seek(void* fd, size_t offset) {
  (void)fd, (void)offset;
  return (size_t)~0;
}
static bool p_rename(const char* from, const char* to) {
  (void)from, (void)to;
  return false;
}
static bool p_remove(const char* path) {
  (void)path;
  return false;
}
static bool p_mkdir(const char* path) {
  (void)path;
  return false;
}
#endif

struct mg_fs mg_fs_posix = {p_stat,  p_list, p_open,   p_close,  p_read,
                            p_write, p_seek, p_rename, p_remove, p_mkdir};

#ifdef MG_ENABLE_LINES
#line 1 "src/http.c"
#endif

bool mg_to_size_t(struct mg_str str, size_t* val);
bool mg_to_size_t(struct mg_str str, size_t* val) {
  size_t i = 0, max = (size_t)-1, max2 = max / 10, result = 0, ndigits = 0;
  while(i < str.len && (str.ptr[i] == ' ' || str.ptr[i] == '\t'))
    i++;
  if(i < str.len && str.ptr[i] == '-')
    return false;
  while(i < str.len && str.ptr[i] >= '0' && str.ptr[i] <= '9') {
    size_t digit = (size_t)(str.ptr[i] - '0');
    if(result > max2)
      return false; // Overflow
    result *= 10;
    if(result > max - digit)
      return false; // Overflow
    result += digit;
    i++, ndigits++;
  }
  while(i < str.len && (str.ptr[i] == ' ' || str.ptr[i] == '\t'))
    i++;
  if(ndigits == 0)
    return false; // #2322: Content-Length = 1 * DIGIT
  if(i != str.len)
    return false; // Ditto
  *val = (size_t)result;
  return true;
}

// Chunk deletion marker is the MSB in the "processed" counter
#define MG_DMARK ((size_t)1 << (sizeof(size_t) * 8 - 1))

// Multipart POST example:
// --xyz
// Content-Disposition: form-data; name="val"
//
// abcdef
// --xyz
// Content-Disposition: form-data; name="foo"; filename="a.txt"
// Content-Type: text/plain
//
// hello world
//
// --xyz--
size_t mg_http_next_multipart(struct mg_str body, size_t ofs,
                              struct mg_http_part* part) {
  struct mg_str cd = mg_str_n("Content-Disposition", 19);
  const char*   s = body.ptr;
  size_t        b = ofs, h1, h2, b1, b2, max = body.len;

  // Init part params
  if(part != NULL)
    part->name = part->filename = part->body = mg_str_n(0, 0);

  // Skip boundary
  while(b + 2 < max && s[b] != '\r' && s[b + 1] != '\n')
    b++;
  if(b <= ofs || b + 2 >= max)
    return 0;
  // MG_INFO(("B: %zu %zu [%.*s]", ofs, b - ofs, (int) (b - ofs), s));

  // Skip headers
  h1 = h2 = b + 2;
  for(;;) {
    while(h2 + 2 < max && s[h2] != '\r' && s[h2 + 1] != '\n')
      h2++;
    if(h2 == h1)
      break;
    if(h2 + 2 >= max)
      return 0;
    // MG_INFO(("Header: [%.*s]", (int) (h2 - h1), &s[h1]));
    if(part != NULL && h1 + cd.len + 2 < h2 && s[h1 + cd.len] == ':' &&
       mg_ncasecmp(&s[h1], cd.ptr, cd.len) == 0) {
      struct mg_str v = mg_str_n(&s[h1 + cd.len + 2], h2 - (h1 + cd.len + 2));
      part->name = mg_http_get_header_var(v, mg_str_n("name", 4));
      part->filename = mg_http_get_header_var(v, mg_str_n("filename", 8));
    }
    h1 = h2 = h2 + 2;
  }
  b1 = b2 = h2 + 2;
  while(b2 + 2 + (b - ofs) + 2 < max &&
        !(s[b2] == '\r' && s[b2 + 1] == '\n' && memcmp(&s[b2 + 2], s, b - ofs) == 0))
    b2++;

  if(b2 + 2 >= max)
    return 0;
  if(part != NULL)
    part->body = mg_str_n(&s[b1], b2 - b1);
  // MG_INFO(("Body: [%.*s]", (int) (b2 - b1), &s[b1]));
  return b2 + 2;
}

void mg_http_bauth(struct mg_connection* c, const char* user, const char* pass) {
  struct mg_str u = mg_str(user), p = mg_str(pass);
  size_t        need = c->send.len + 36 + (u.len + p.len) * 2;
  if(c->send.size < need)
    mg_iobuf_resize(&c->send, need);
  if(c->send.size >= need) {
    size_t i, n = 0;
    char*  buf = (char*)&c->send.buf[c->send.len];
    memcpy(buf, "Authorization: Basic ", 21); // DON'T use mg_send!
    for(i = 0; i < u.len; i++) {
      n = mg_base64_update(((unsigned char*)u.ptr)[i], buf + 21, n);
    }
    if(p.len > 0) {
      n = mg_base64_update(':', buf + 21, n);
      for(i = 0; i < p.len; i++) {
        n = mg_base64_update(((unsigned char*)p.ptr)[i], buf + 21, n);
      }
    }
    n = mg_base64_final(buf + 21, n);
    c->send.len += 21 + (size_t)n + 2;
    memcpy(&c->send.buf[c->send.len - 2], "\r\n", 2);
  } else {
    MG_ERROR(("%lu oom %d->%d ", c->id, (int)c->send.size, (int)need));
  }
}

struct mg_str mg_http_var(struct mg_str buf, struct mg_str name) {
  struct mg_str k, v, result = mg_str_n(NULL, 0);
  while(mg_split(&buf, &k, &v, '&')) {
    if(name.len == k.len && mg_ncasecmp(name.ptr, k.ptr, k.len) == 0) {
      result = v;
      break;
    }
  }
  return result;
}

int mg_http_get_var(const struct mg_str* buf, const char* name, char* dst,
                    size_t dst_len) {
  int len;
  if(dst == NULL || dst_len == 0) {
    len = -2; // Bad destination
  } else if(buf->ptr == NULL || name == NULL || buf->len == 0) {
    len = -1; // Bad source
    dst[0] = '\0';
  } else {
    struct mg_str v = mg_http_var(*buf, mg_str(name));
    if(v.ptr == NULL) {
      len = -4; // Name does not exist
    } else {
      len = mg_url_decode(v.ptr, v.len, dst, dst_len, 1);
      if(len < 0)
        len = -3; // Failed to decode
    }
  }
  return len;
}

static bool isx(int c) {
  return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
}

int mg_url_decode(const char* src, size_t src_len, char* dst, size_t dst_len,
                  int is_form_url_encoded) {
  size_t i, j;
  for(i = j = 0; i < src_len && j + 1 < dst_len; i++, j++) {
    if(src[i] == '%') {
      // Use `i + 2 < src_len`, not `i < src_len - 2`, note small src_len
      if(i + 2 < src_len && isx(src[i + 1]) && isx(src[i + 2])) {
        mg_unhex(src + i + 1, 2, (uint8_t*)&dst[j]);
        i += 2;
      } else {
        return -1;
      }
    } else if(is_form_url_encoded && src[i] == '+') {
      dst[j] = ' ';
    } else {
      dst[j] = src[i];
    }
  }
  if(j < dst_len)
    dst[j] = '\0'; // Null-terminate the destination
  return i >= src_len && j < dst_len ? (int)j : -1;
}

static bool isok(uint8_t c) {
  return c == '\n' || c == '\r' || c >= ' ';
}

int mg_http_get_request_len(const unsigned char* buf, size_t buf_len) {
  size_t i;
  for(i = 0; i < buf_len; i++) {
    if(!isok(buf[i]))
      return -1;
    if((i > 0 && buf[i] == '\n' && buf[i - 1] == '\n') ||
       (i > 3 && buf[i] == '\n' && buf[i - 1] == '\r' && buf[i - 2] == '\n'))
      return (int)i + 1;
  }
  return 0;
}
struct mg_str* mg_http_get_header(struct mg_http_message* h, const char* name) {
  size_t i, n = strlen(name), max = sizeof(h->headers) / sizeof(h->headers[0]);
  for(i = 0; i < max && h->headers[i].name.len > 0; i++) {
    struct mg_str *k = &h->headers[i].name, *v = &h->headers[i].value;
    if(n == k->len && mg_ncasecmp(k->ptr, name, n) == 0)
      return v;
  }
  return NULL;
}

// Get character length. Used to parse method, URI, headers
static size_t clen(const char* s) {
  uint8_t c = *(uint8_t*)s;
  if(c > ' ' && c < '~')
    return 1; // Usual ascii printed char
  if((c & 0xe0) == 0xc0)
    return 2; // 2-byte UTF8
  if((c & 0xf0) == 0xe0)
    return 3; // 3-byte UTF8
  if((c & 0xf8) == 0xf0)
    return 4; // 4-byte UTF8
  return 0;
}

// Skip until the newline. Return advanced `s`, or NULL on error
static const char* skiptorn(const char* s, const char* end, struct mg_str* v) {
  v->ptr = s;
  while(s < end && s[0] != '\n' && s[0] != '\r')
    s++, v->len++; // To newline
  if(s >= end || (s[0] == '\r' && s[1] != '\n'))
    return NULL; // Stray \r
  if(s < end && s[0] == '\r')
    s++; // Skip \r
  if(s >= end || *s++ != '\n')
    return NULL; // Skip \n
  return s;
}

static bool mg_http_parse_headers(const char* s, const char* end,
                                  struct mg_http_header* h, size_t max_hdrs) {
  size_t i, n;
  for(i = 0; i < max_hdrs; i++) {
    struct mg_str k = {NULL, 0}, v = {NULL, 0};
    if(s >= end)
      return false;
    if(s[0] == '\n' || (s[0] == '\r' && s[1] == '\n'))
      break;
    k.ptr = s;
    while(s < end && s[0] != ':' && (n = clen(s)) > 0)
      s += n, k.len += n;
    if(k.len == 0)
      return false; // Empty name
    if(s >= end || *s++ != ':')
      return false; // Invalid, not followed by :
    while(s < end && s[0] == ' ')
      s++; // Skip spaces
    if((s = skiptorn(s, end, &v)) == NULL)
      return false;
    while(v.len > 0 && v.ptr[v.len - 1] == ' ')
      v.len--; // Trim spaces
    // MG_INFO(("--HH [%.*s] [%.*s]", (int) k.len, k.ptr, (int) v.len, v.ptr));
    h[i].name = k, h[i].value = v; // Success. Assign values
  }
  return true;
}

int mg_http_parse(const char* s, size_t len, struct mg_http_message* hm) {
  int         is_response, req_len = mg_http_get_request_len((unsigned char*)s, len);
  const char *end = s == NULL ? NULL : s + req_len, *qs; // Cannot add to NULL
  struct mg_str* cl;
  size_t         n;

  memset(hm, 0, sizeof(*hm));
  if(req_len <= 0)
    return req_len;

  hm->message.ptr = hm->head.ptr = s;
  hm->body.ptr = end;
  hm->head.len = (size_t)req_len;
  hm->chunk.ptr = end;
  hm->message.len = hm->body.len = (size_t)~0; // Set body length to infinite

  // Parse request line
  hm->method.ptr = s;
  while(s < end && (n = clen(s)) > 0)
    s += n, hm->method.len += n;
  while(s < end && s[0] == ' ')
    s++; // Skip spaces
  hm->uri.ptr = s;
  while(s < end && (n = clen(s)) > 0)
    s += n, hm->uri.len += n;
  while(s < end && s[0] == ' ')
    s++; // Skip spaces
  if((s = skiptorn(s, end, &hm->proto)) == NULL)
    return false;

  // If URI contains '?' character, setup query string
  if((qs = (const char*)memchr(hm->uri.ptr, '?', hm->uri.len)) != NULL) {
    hm->query.ptr = qs + 1;
    hm->query.len = (size_t)(&hm->uri.ptr[hm->uri.len] - (qs + 1));
    hm->uri.len = (size_t)(qs - hm->uri.ptr);
  }

  // Sanity check. Allow protocol/reason to be empty
  // Do this check after hm->method.len and hm->uri.len are finalised
  if(hm->method.len == 0 || hm->uri.len == 0)
    return -1;

  if(!mg_http_parse_headers(s, end, hm->headers,
                            sizeof(hm->headers) / sizeof(hm->headers[0])))
    return -1; // error when parsing
  if((cl = mg_http_get_header(hm, "Content-Length")) != NULL) {
    if(mg_to_size_t(*cl, &hm->body.len) == false)
      return -1;
    hm->message.len = (size_t)req_len + hm->body.len;
  }

  // mg_http_parse() is used to parse both HTTP requests and HTTP
  // responses. If HTTP response does not have Content-Length set, then
  // body is read until socket is closed, i.e. body.len is infinite (~0).
  //
  // For HTTP requests though, according to
  // http://tools.ietf.org/html/rfc7231#section-8.1.3,
  // only POST and PUT methods have defined body semantics.
  // Therefore, if Content-Length is not specified and methods are
  // not one of PUT or POST, set body length to 0.
  //
  // So, if it is HTTP request, and Content-Length is not set,
  // and method is not (PUT or POST) then reset body length to zero.
  is_response = mg_ncasecmp(hm->method.ptr, "HTTP/", 5) == 0;
  if(hm->body.len == (size_t)~0 && !is_response) {
    hm->body.len = 0;
    hm->message.len = (size_t)req_len;
  }

  // The 204 (No content) responses also have 0 body length
  if(hm->body.len == (size_t)~0 && is_response &&
     mg_vcasecmp(&hm->uri, "204") == 0) {
    hm->body.len = 0;
    hm->message.len = (size_t)req_len;
  }
  if(hm->message.len < (size_t)req_len)
    return -1; // Overflow protection

  return req_len;
}

static void mg_http_vprintf_chunk(struct mg_connection* c, const char* fmt,
                                  va_list* ap) {
  size_t len = c->send.len;
  mg_send(c, "        \r\n", 10);
  mg_vxprintf(mg_pfn_iobuf, &c->send, fmt, ap);
  if(c->send.len >= len + 10) {
    mg_snprintf((char*)c->send.buf + len, 9, "%08lx", c->send.len - len - 10);
    c->send.buf[len + 8] = '\r';
    if(c->send.len == len + 10)
      c->is_resp = 0; // Last chunk, reset marker
  }
  mg_send(c, "\r\n", 2);
}

void mg_http_printf_chunk(struct mg_connection* c, const char* fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  mg_http_vprintf_chunk(c, fmt, &ap);
  va_end(ap);
}

void mg_http_write_chunk(struct mg_connection* c, const char* buf, size_t len) {
  mg_printf(c, "%lx\r\n", (unsigned long)len);
  mg_send(c, buf, len);
  mg_send(c, "\r\n", 2);
  if(len == 0)
    c->is_resp = 0;
}

// clang-format off
static const char *mg_http_status_code_str(int status_code) {
  switch (status_code) {
    case 100: return "Continue";
    case 101: return "Switching Protocols";
    case 102: return "Processing";
    case 200: return "OK";
    case 201: return "Created";
    case 202: return "Accepted";
    case 203: return "Non-authoritative Information";
    case 204: return "No Content";
    case 205: return "Reset Content";
    case 206: return "Partial Content";
    case 207: return "Multi-Status";
    case 208: return "Already Reported";
    case 226: return "IM Used";
    case 300: return "Multiple Choices";
    case 301: return "Moved Permanently";
    case 302: return "Found";
    case 303: return "See Other";
    case 304: return "Not Modified";
    case 305: return "Use Proxy";
    case 307: return "Temporary Redirect";
    case 308: return "Permanent Redirect";
    case 400: return "Bad Request";
    case 401: return "Unauthorized";
    case 402: return "Payment Required";
    case 403: return "Forbidden";
    case 404: return "Not Found";
    case 405: return "Method Not Allowed";
    case 406: return "Not Acceptable";
    case 407: return "Proxy Authentication Required";
    case 408: return "Request Timeout";
    case 409: return "Conflict";
    case 410: return "Gone";
    case 411: return "Length Required";
    case 412: return "Precondition Failed";
    case 413: return "Payload Too Large";
    case 414: return "Request-URI Too Long";
    case 415: return "Unsupported Media Type";
    case 416: return "Requested Range Not Satisfiable";
    case 417: return "Expectation Failed";
    case 418: return "I'm a teapot";
    case 421: return "Misdirected Request";
    case 422: return "Unprocessable Entity";
    case 423: return "Locked";
    case 424: return "Failed Dependency";
    case 426: return "Upgrade Required";
    case 428: return "Precondition Required";
    case 429: return "Too Many Requests";
    case 431: return "Request Header Fields Too Large";
    case 444: return "Connection Closed Without Response";
    case 451: return "Unavailable For Legal Reasons";
    case 499: return "Client Closed Request";
    case 500: return "Internal Server Error";
    case 501: return "Not Implemented";
    case 502: return "Bad Gateway";
    case 503: return "Service Unavailable";
    case 504: return "Gateway Timeout";
    case 505: return "HTTP Version Not Supported";
    case 506: return "Variant Also Negotiates";
    case 507: return "Insufficient Storage";
    case 508: return "Loop Detected";
    case 510: return "Not Extended";
    case 511: return "Network Authentication Required";
    case 599: return "Network Connect Timeout Error";
    default: return "";
  }
}
// clang-format on

void mg_http_reply(struct mg_connection* c, int code, const char* headers,
                   const char* fmt, ...) {
  va_list ap;
  size_t  len;
  mg_printf(c, "HTTP/1.1 %d %s\r\n%sContent-Length:            \r\n\r\n", code,
            mg_http_status_code_str(code), headers == NULL ? "" : headers);
  len = c->send.len;
  va_start(ap, fmt);
  mg_vxprintf(mg_pfn_iobuf, &c->send, fmt, &ap);
  va_end(ap);
  if(c->send.len > 16) {
    size_t n = mg_snprintf((char*)&c->send.buf[len - 15], 11, "%-10lu",
                           (unsigned long)(c->send.len - len));
    c->send.buf[len - 15 + n] = ' '; // Change ending 0 to space
  }
  c->is_resp = 0;
}

static void http_cb(struct mg_connection*, int, void*, void*);
static void restore_http_cb(struct mg_connection* c) {
  mg_fs_close((struct mg_fd*)c->pfn_data);
  c->pfn_data = NULL;
  c->pfn = http_cb;
  c->is_resp = 0;
}

char* mg_http_etag(char* buf, size_t len, size_t size, time_t mtime);
char* mg_http_etag(char* buf, size_t len, size_t size, time_t mtime) {
  mg_snprintf(buf, len, "\"%lld.%lld\"", (int64_t)mtime, (int64_t)size);
  return buf;
}

static void static_cb(struct mg_connection* c, int ev, void* ev_data,
                      void* fn_data) {
  if(ev == MG_EV_WRITE || ev == MG_EV_POLL) {
    struct mg_fd* fd = (struct mg_fd*)fn_data;
    // Read to send IO buffer directly, avoid extra on-stack buffer
    size_t  n, max = MG_IO_SIZE, space;
    size_t* cl = (size_t*)&c->data[(sizeof(c->data) - sizeof(size_t)) /
                                   sizeof(size_t) * sizeof(size_t)];
    if(c->send.size < max)
      mg_iobuf_resize(&c->send, max);
    if(c->send.len >= c->send.size)
      return; // Rate limit
    if((space = c->send.size - c->send.len) > *cl)
      space = *cl;
    n = fd->fs->rd(fd->fd, c->send.buf + c->send.len, space);
    c->send.len += n;
    *cl -= n;
    if(n == 0)
      restore_http_cb(c);
  } else if(ev == MG_EV_CLOSE) {
    restore_http_cb(c);
  }
  (void)ev_data;
}

// Known mime types. Keep it outside guess_content_type() function, since
// some environments don't like it defined there.
// clang-format off
static struct mg_str s_known_types[] = {
    MG_C_STR("html"), MG_C_STR("text/html; charset=utf-8"),
    MG_C_STR("htm"), MG_C_STR("text/html; charset=utf-8"),
    MG_C_STR("css"), MG_C_STR("text/css; charset=utf-8"),
    MG_C_STR("js"), MG_C_STR("text/javascript; charset=utf-8"),
    MG_C_STR("gif"), MG_C_STR("image/gif"),
    MG_C_STR("png"), MG_C_STR("image/png"),
    MG_C_STR("jpg"), MG_C_STR("image/jpeg"),
    MG_C_STR("jpeg"), MG_C_STR("image/jpeg"),
    MG_C_STR("woff"), MG_C_STR("font/woff"),
    MG_C_STR("ttf"), MG_C_STR("font/ttf"),
    MG_C_STR("svg"), MG_C_STR("image/svg+xml"),
    MG_C_STR("txt"), MG_C_STR("text/plain; charset=utf-8"),
    MG_C_STR("avi"), MG_C_STR("video/x-msvideo"),
    MG_C_STR("csv"), MG_C_STR("text/csv"),
    MG_C_STR("doc"), MG_C_STR("application/msword"),
    MG_C_STR("exe"), MG_C_STR("application/octet-stream"),
    MG_C_STR("gz"), MG_C_STR("application/gzip"),
    MG_C_STR("ico"), MG_C_STR("image/x-icon"),
    MG_C_STR("json"), MG_C_STR("application/json"),
    MG_C_STR("mov"), MG_C_STR("video/quicktime"),
    MG_C_STR("mp3"), MG_C_STR("audio/mpeg"),
    MG_C_STR("mp4"), MG_C_STR("video/mp4"),
    MG_C_STR("mpeg"), MG_C_STR("video/mpeg"),
    MG_C_STR("pdf"), MG_C_STR("application/pdf"),
    MG_C_STR("shtml"), MG_C_STR("text/html; charset=utf-8"),
    MG_C_STR("tgz"), MG_C_STR("application/tar-gz"),
    MG_C_STR("wav"), MG_C_STR("audio/wav"),
    MG_C_STR("webp"), MG_C_STR("image/webp"),
    MG_C_STR("zip"), MG_C_STR("application/zip"),
    MG_C_STR("3gp"), MG_C_STR("video/3gpp"),
    {0, 0},
};
// clang-format on

struct mg_str guess_content_type(struct mg_str path, const char* extra) {
  struct mg_str k, v, s = mg_str(extra);
  size_t        i = 0;

  // Shrink path to its extension only
  while(i < path.len && path.ptr[path.len - i - 1] != '.')
    i++;
  path.ptr += path.len - i;
  path.len = i;

  // Process user-provided mime type overrides, if any
  while(mg_commalist(&s, &k, &v)) {
    if(mg_strcmp(path, k) == 0)
      return v;
  }

  // Process built-in mime types
  for(i = 0; s_known_types[i].ptr != NULL; i += 2) {
    if(mg_strcmp(path, s_known_types[i]) == 0)
      return s_known_types[i + 1];
  }

  return mg_str("text/plain; charset=utf-8");
}

static int getrange(struct mg_str* s, size_t* a, size_t* b) {
  size_t i, numparsed = 0;
  for(i = 0; i + 6 < s->len; i++) {
    struct mg_str k, v = mg_str_n(s->ptr + i + 6, s->len - i - 6);
    if(memcmp(&s->ptr[i], "bytes=", 6) != 0)
      continue;
    if(mg_split(&v, &k, NULL, '-')) {
      if(mg_to_size_t(k, a))
        numparsed++;
      if(v.len > 0 && mg_to_size_t(v, b))
        numparsed++;
    } else {
      if(mg_to_size_t(v, a))
        numparsed++;
    }
    break;
  }
  return (int)numparsed;
}

void mg_http_serve_file(struct mg_connection* c, struct mg_http_message* hm,
                        const char* path, const struct mg_http_serve_opts* opts) {
  char           etag[64], tmp[MG_PATH_MAX];
  struct mg_fs*  fs = opts->fs == NULL ? &mg_fs_posix : opts->fs;
  struct mg_fd*  fd = NULL;
  size_t         size = 0;
  time_t         mtime = 0;
  struct mg_str* inm = NULL;
  struct mg_str  mime = guess_content_type(mg_str(path), opts->mime_types);
  bool           gzip = false;

  if(path != NULL) {
    // If a browser sends us "Accept-Encoding: gzip", try to open .gz first
    struct mg_str* ae = mg_http_get_header(hm, "Accept-Encoding");
    if(ae != NULL && mg_strstr(*ae, mg_str("gzip")) != NULL) {
      mg_snprintf(tmp, sizeof(tmp), "%s.gz", path);
      fd = mg_fs_open(fs, tmp, MG_FS_READ);
      if(fd != NULL)
        gzip = true, path = tmp;
    }
    // No luck opening .gz? Open what we've told to open
    if(fd == NULL)
      fd = mg_fs_open(fs, path, MG_FS_READ);
  }

  // Failed to open, and page404 is configured? Open it, then
  if(fd == NULL && opts->page404 != NULL) {
    fd = mg_fs_open(fs, opts->page404, MG_FS_READ);
    mime = guess_content_type(mg_str(path), opts->mime_types);
    path = opts->page404;
  }

  if(fd == NULL || fs->st(path, &size, &mtime) == 0) {
    if(mg_vcmp(&hm->uri, "/favicon.ico") == 0) {
      /* Send default favicon */
      mg_printf(c,
                "HTTP/1.1 200 OK\r\nContent-Type: "
                "image/x-icon\r\nContent-Length: %d\r\n\r\n",
                (int)favicon_data_len);
      mg_send(c, favicon_data, favicon_data_len);
    } else {
      mg_http_reply(c, 404, BIALET_HEADERS, BIALET_NOT_FOUND_PAGE);
    }
    mg_fs_close(fd);
    // NOTE: mg_http_etag() call should go first!
  } else if(mg_http_etag(etag, sizeof(etag), size, mtime) != NULL &&
            (inm = mg_http_get_header(hm, "If-None-Match")) != NULL &&
            mg_vcasecmp(inm, etag) == 0) {
    mg_fs_close(fd);
    mg_http_reply(c, 304, opts->extra_headers, "");
  } else {
    int    n, status = 200;
    char   range[100];
    size_t r1 = 0, r2 = 0, cl = size;

    // Handle Range header
    struct mg_str* rh = mg_http_get_header(hm, "Range");
    range[0] = '\0';
    if(rh != NULL && (n = getrange(rh, &r1, &r2)) > 0) {
      // If range is specified like "400-", set second limit to content len
      if(n == 1)
        r2 = cl - 1;
      if(r1 > r2 || r2 >= cl) {
        status = 416;
        cl = 0;
        mg_snprintf(range, sizeof(range), "Content-Range: bytes */%lld\r\n",
                    (int64_t)size);
      } else {
        status = 206;
        cl = r2 - r1 + 1;
        mg_snprintf(range, sizeof(range), "Content-Range: bytes %llu-%llu/%llu\r\n",
                    (uint64_t)r1, (uint64_t)(r1 + cl - 1), (uint64_t)size);
        fs->sk(fd->fd, r1);
      }
    }
    mg_printf(c,
              "HTTP/1.1 %d %s\r\n"
              "Content-Type: %.*s\r\n"
              "Etag: %s\r\n"
              "Content-Length: %llu\r\n"
              "%s%s%s\r\n",
              status, mg_http_status_code_str(status), (int)mime.len, mime.ptr, etag,
              (uint64_t)cl, gzip ? "Content-Encoding: gzip\r\n" : "", range,
              opts->extra_headers ? opts->extra_headers : "");
    if(mg_vcasecmp(&hm->method, "HEAD") == 0) {
      c->is_draining = 1;
      c->is_resp = 0;
      mg_fs_close(fd);
    } else {
      // Track to-be-sent content length at the end of c->data, aligned
      size_t* clp = (size_t*)&c->data[(sizeof(c->data) - sizeof(size_t)) /
                                      sizeof(size_t) * sizeof(size_t)];
      c->pfn = static_cb;
      c->pfn_data = fd;
      *clp = cl;
    }
  }
}

struct printdirentrydata {
  struct mg_connection*            c;
  struct mg_http_message*          hm;
  const struct mg_http_serve_opts* opts;
  const char*                      dir;
};

#if MG_ENABLE_DIRLIST
static void printdirentry(const char* name, void* userdata) {
  struct printdirentrydata* d = (struct printdirentrydata*)userdata;
  struct mg_fs*             fs = d->opts->fs == NULL ? &mg_fs_posix : d->opts->fs;
  size_t                    size = 0;
  time_t                    t = 0;
  char                      path[MG_PATH_MAX], sz[40], mod[40];
  int                       flags, n = 0;

  // MG_DEBUG(("[%s] [%s]", d->dir, name));
  if(mg_snprintf(path, sizeof(path), "%s%c%s", d->dir, '/', name) > sizeof(path)) {
    MG_ERROR(("%s truncated", name));
  } else if((flags = fs->st(path, &size, &t)) == 0) {
    MG_ERROR(("%lu stat(%s): %d", d->c->id, path, errno));
  } else {
    const char* slash = flags & MG_FS_DIR ? "/" : "";
    if(flags & MG_FS_DIR) {
      mg_snprintf(sz, sizeof(sz), "%s", "[DIR]");
    } else {
      mg_snprintf(sz, sizeof(sz), "%lld", (uint64_t)size);
    }
#if defined(MG_HTTP_DIRLIST_TIME_FMT)
    {
      char       time_str[40];
      struct tm* time_info = localtime(&t);
      strftime(time_str, sizeof time_str, "%Y/%m/%d %H:%M:%S", time_info);
      mg_snprintf(mod, sizeof(mod), "%s", time_str);
    }
#else
    mg_snprintf(mod, sizeof(mod), "%lu", (unsigned long)t);
#endif
    n = (int)mg_url_encode(name, strlen(name), path, sizeof(path));
    mg_printf(d->c,
              "  <tr><td><a href=\"%.*s%s\">%s%s</a></td>"
              "<td name=%lu>%s</td><td name=%lld>%s</td></tr>\n",
              n, path, slash, name, slash, (unsigned long)t, mod,
              flags & MG_FS_DIR ? (int64_t)-1 : (int64_t)size, sz);
  }
}

static void listdir(struct mg_connection* c, struct mg_http_message* hm,
                    const struct mg_http_serve_opts* opts, char* dir) {
  const char* sort_js_code =
      "<script>function srt(tb, sc, so, d) {"
      "var tr = Array.prototype.slice.call(tb.rows, 0),"
      "tr = tr.sort(function (a, b) { var c1 = a.cells[sc], c2 = b.cells[sc],"
      "n1 = c1.getAttribute('name'), n2 = c2.getAttribute('name'), "
      "t1 = a.cells[2].getAttribute('name'), "
      "t2 = b.cells[2].getAttribute('name'); "
      "return so * (t1 < 0 && t2 >= 0 ? -1 : t2 < 0 && t1 >= 0 ? 1 : "
      "n1 ? parseInt(n2) - parseInt(n1) : "
      "c1.textContent.trim().localeCompare(c2.textContent.trim())); });";
  const char* sort_js_code2 =
      "for (var i = 0; i < tr.length; i++) tb.appendChild(tr[i]); "
      "if (!d) window.location.hash = ('sc=' + sc + '&so=' + so); "
      "};"
      "window.onload = function() {"
      "var tb = document.getElementById('tb');"
      "var m = /sc=([012]).so=(1|-1)/.exec(window.location.hash) || [0, 2, 1];"
      "var sc = m[1], so = m[2]; document.onclick = function(ev) { "
      "var c = ev.target.rel; if (c) {if (c == sc) so *= -1; srt(tb, c, so); "
      "sc = c; ev.preventDefault();}};"
      "srt(tb, sc, so, true);"
      "}"
      "</script>";
  struct mg_fs*            fs = opts->fs == NULL ? &mg_fs_posix : opts->fs;
  struct printdirentrydata d = {c, hm, opts, dir};
  char                     tmp[10], buf[MG_PATH_MAX];
  size_t                   off, n;
  int           len = mg_url_decode(hm->uri.ptr, hm->uri.len, buf, sizeof(buf), 0);
  struct mg_str uri = len > 0 ? mg_str_n(buf, (size_t)len) : hm->uri;

  mg_printf(c,
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: text/html; charset=utf-8\r\n"
            "%s"
            "Content-Length:         \r\n\r\n",
            opts->extra_headers == NULL ? "" : opts->extra_headers);
  off = c->send.len; // Start of body
  mg_printf(c,
            "<!DOCTYPE html><html><head><title>Index of %.*s</title>%s%s"
            "<style>th,td {text-align: left; padding-right: 1em; "
            "font-family: monospace; }</style></head>"
            "<body><h1>Index of %.*s</h1><table cellpadding=\"0\"><thead>"
            "<tr><th><a href=\"#\" rel=\"0\">Name</a></th><th>"
            "<a href=\"#\" rel=\"1\">Modified</a></th>"
            "<th><a href=\"#\" rel=\"2\">Size</a></th></tr>"
            "<tr><td colspan=\"3\"><hr></td></tr>"
            "</thead>"
            "<tbody id=\"tb\">\n",
            (int)uri.len, uri.ptr, sort_js_code, sort_js_code2, (int)uri.len,
            uri.ptr);
  mg_printf(c, "%s",
            "  <tr><td><a href=\"..\">..</a></td>"
            "<td name=-1></td><td name=-1>[DIR]</td></tr>\n");

  fs->ls(dir, printdirentry, &d);
  mg_printf(c,
            "</tbody><tfoot><tr><td colspan=\"3\"><hr></td></tr></tfoot>"
            "</table><address>Mongoose v.%s</address></body></html>\n",
            MG_VERSION);
  n = mg_snprintf(tmp, sizeof(tmp), "%lu", (unsigned long)(c->send.len - off));
  if(n > sizeof(tmp))
    n = 0;
  memcpy(c->send.buf + off - 12, tmp, n); // Set content length
  c->is_resp = 0;                         // Mark response end
}
#endif

// Resolve requested file into `path` and return its fs->st() result
static int uri_to_path2(struct mg_connection* c, struct mg_http_message* hm,
                        struct mg_fs* fs, struct mg_str url, struct mg_str dir,
                        char* path, size_t path_size) {
  int flags, tmp;
  // Append URI to the root_dir, and sanitize it
  size_t n = mg_snprintf(path, path_size, "%.*s", (int)dir.len, dir.ptr);
  if(n + 2 >= path_size) {
    mg_http_reply(c, 400, "", "Exceeded path size");
    return -1;
  }
  path[path_size - 1] = '\0';
  // Terminate root dir with slash
  if(n > 0 && path[n - 1] != '/')
    path[n++] = '/', path[n] = '\0';
  if(url.len < hm->uri.len) {
    mg_url_decode(hm->uri.ptr + url.len, hm->uri.len - url.len, path + n,
                  path_size - n, 0);
  }
  path[path_size - 1] = '\0'; // Double-check
  if(!mg_path_is_sane(path)) {
    mg_http_reply(c, 403, BIALET_HEADERS, BIALET_FORBIDDEN_PAGE);
    return -1;
  }
  n = strlen(path);
  while(n > 1 && path[n - 1] == '/')
    path[--n] = 0; // Trim trailing slashes
  flags = mg_vcmp(&hm->uri, "/") == 0 ? MG_FS_DIR : fs->st(path, NULL, NULL);
  MG_VERBOSE(
      ("%lu %.*s -> %s %d", c->id, (int)hm->uri.len, hm->uri.ptr, path, flags));
  if(flags == 0) {
    // Check routing wren files without extension
    if((mg_snprintf(path + n, path_size - n, BIALET_EXTENSION) > 0 &&
        (tmp = fs->st(path, NULL, NULL)) != 0)) {
      flags = tmp;
    }
  } else if((flags & MG_FS_DIR) && hm->uri.len > 0 &&
            hm->uri.ptr[hm->uri.len - 1] != '/') {
    mg_printf(c,
              "HTTP/1.1 301 Moved\r\n"
              "Location: %.*s/\r\n"
              "Content-Length: 0\r\n"
              "\r\n",
              (int)hm->uri.len, hm->uri.ptr);
    c->is_resp = 0;
    flags = -1;
  } else if(flags & MG_FS_DIR) {
    if(((mg_snprintf(path + n, path_size - n, "/" MG_HTTP_INDEX) > 0 &&
         (tmp = fs->st(path, NULL, NULL)) != 0) ||
        (mg_snprintf(path + n, path_size - n, BIALET_INDEX_FILE) > 0 &&
         (tmp = fs->st(path, NULL, NULL)) != 0))) {
      flags = tmp;
    } else if((mg_snprintf(path + n, path_size - n, "/" MG_HTTP_INDEX ".gz") > 0 &&
               (tmp = fs->st(path, NULL, NULL)) != 0)) { // check for gzipped index
      flags = tmp;
      path[n + 1 + strlen(MG_HTTP_INDEX)] =
          '\0'; // Remove appended .gz in index file name
    } else {
      path[n] = '\0'; // Remove appended index file name
    }
  }
  return flags;
}

static int uri_to_path(struct mg_connection* c, struct mg_http_message* hm,
                       const struct mg_http_serve_opts* opts, char* path,
                       size_t path_size) {
  struct mg_fs* fs = opts->fs == NULL ? &mg_fs_posix : opts->fs;
  struct mg_str k, v, s = mg_str(opts->root_dir), u = {0, 0}, p = {0, 0};
  while(mg_commalist(&s, &k, &v)) {
    if(v.len == 0)
      v = k, k = mg_str("/"), u = k, p = v;
    if(hm->uri.len < k.len)
      continue;
    if(mg_strcmp(k, mg_str_n(hm->uri.ptr, k.len)) != 0)
      continue;
    u = k, p = v;
  }
  return uri_to_path2(c, hm, fs, u, p, path, path_size);
}

void mg_http_serve_dir(struct mg_connection* c, struct mg_http_message* hm,
                       const struct mg_http_serve_opts* opts) {
  char        path[MG_PATH_MAX];
  const char* sp = opts->ssi_pattern;
  int         flags = uri_to_path(c, hm, opts, path, sizeof(path));
  if(flags < 0) {
    // Do nothing: the response has already been sent by uri_to_path()
  } else if(flags & MG_FS_DIR) {
#if MG_ENABLE_DIRLIST
    listdir(c, hm, opts, path);
#else
    /* If the home not exists, show Bialet welcome page */
    if(mg_vcmp(&hm->uri, "/") == 0) {
      mg_http_reply(c, 200, BIALET_HEADERS, BIALET_WELCOME_PAGE);
    } else {
      mg_http_reply(c, 404, BIALET_HEADERS, BIALET_NOT_FOUND_PAGE);
    }
#endif
  } else if(flags && sp != NULL &&
            mg_globmatch(sp, strlen(sp), path, strlen(path))) {
    mg_http_serve_ssi(c, hm, opts->root_dir, path);
  } else {
    mg_http_serve_file(c, hm, path, opts);
  }
}

static bool mg_is_url_safe(int c) {
  return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') ||
         (c >= 'A' && c <= 'Z') || c == '.' || c == '_' || c == '-' || c == '~';
}

size_t mg_url_encode(const char* s, size_t sl, char* buf, size_t len) {
  size_t i, n = 0;
  for(i = 0; i < sl; i++) {
    int c = *(unsigned char*)&s[i];
    if(n + 4 >= len)
      return 0;
    if(mg_is_url_safe(c)) {
      buf[n++] = s[i];
    } else {
      buf[n++] = '%';
      mg_hex(&s[i], 1, &buf[n]);
      n += 2;
    }
  }
  if(len > 0 && n < len - 1)
    buf[n] = '\0'; // Null-terminate the destination
  if(len > 0)
    buf[len - 1] = '\0'; // Always.
  return n;
}

void mg_http_creds(struct mg_http_message* hm, char* user, size_t userlen,
                   char* pass, size_t passlen) {
  struct mg_str* v = mg_http_get_header(hm, "Authorization");
  user[0] = pass[0] = '\0';
  if(v != NULL && v->len > 6 && memcmp(v->ptr, "Basic ", 6) == 0) {
    char        buf[256];
    size_t      n = mg_base64_decode(v->ptr + 6, v->len - 6, buf, sizeof(buf));
    const char* p = (const char*)memchr(buf, ':', n > 0 ? n : 0);
    if(p != NULL) {
      mg_snprintf(user, userlen, "%.*s", p - buf, buf);
      mg_snprintf(pass, passlen, "%.*s", n - (size_t)(p - buf) - 1, p + 1);
    }
  } else if(v != NULL && v->len > 7 && memcmp(v->ptr, "Bearer ", 7) == 0) {
    mg_snprintf(pass, passlen, "%.*s", (int)v->len - 7, v->ptr + 7);
  } else if((v = mg_http_get_header(hm, "Cookie")) != NULL) {
    struct mg_str t = mg_http_get_header_var(*v, mg_str_n("access_token", 12));
    if(t.len > 0)
      mg_snprintf(pass, passlen, "%.*s", (int)t.len, t.ptr);
  } else {
    mg_http_get_var(&hm->query, "access_token", pass, passlen);
  }
}

static struct mg_str stripquotes(struct mg_str s) {
  return s.len > 1 && s.ptr[0] == '"' && s.ptr[s.len - 1] == '"'
             ? mg_str_n(s.ptr + 1, s.len - 2)
             : s;
}

struct mg_str mg_http_get_header_var(struct mg_str s, struct mg_str v) {
  size_t i;
  for(i = 0; v.len > 0 && i + v.len + 2 < s.len; i++) {
    if(s.ptr[i + v.len] == '=' && memcmp(&s.ptr[i], v.ptr, v.len) == 0) {
      const char *p = &s.ptr[i + v.len + 1], *b = p, *x = &s.ptr[s.len];
      int         q = p < x && *p == '"' ? 1 : 0;
      while(p < x && (q ? p == b || *p != '"' : *p != ';' && *p != ' ' && *p != ','))
        p++;
      // MG_INFO(("[%.*s] [%.*s] [%.*s]", (int) s.len, s.ptr, (int) v.len,
      // v.ptr, (int) (p - b), b));
      return stripquotes(mg_str_n(b, (size_t)(p - b + q)));
    }
  }
  return mg_str_n(NULL, 0);
}

bool mg_http_match_uri(const struct mg_http_message* hm, const char* glob) {
  return mg_match(hm->uri, mg_str(glob), NULL);
}

long mg_http_upload(struct mg_connection* c, struct mg_http_message* hm,
                    struct mg_fs* fs, const char* path, size_t max_size) {
  char buf[20] = "0";
  long res = 0, offset;
  mg_http_get_var(&hm->query, "offset", buf, sizeof(buf));
  offset = strtol(buf, NULL, 0);
  if(hm->body.len == 0) {
    mg_http_reply(c, 200, "", "%ld", res); // Nothing to write
  } else {
    struct mg_fd* fd;
    size_t        current_size = 0;
    MG_DEBUG(("%s -> %d bytes @ %ld", path, (int)hm->body.len, offset));
    if(offset == 0)
      fs->rm(path); // If offset if 0, truncate file
    fs->st(path, &current_size, NULL);
    if(offset < 0) {
      mg_http_reply(c, 400, "", "offset required");
      res = -1;
    } else if(offset > 0 && current_size != (size_t)offset) {
      mg_http_reply(c, 400, "", "%s: offset mismatch", path);
      res = -2;
    } else if((size_t)offset + hm->body.len > max_size) {
      mg_http_reply(c, 400, "", "%s: over max size of %lu", path,
                    (unsigned long)max_size);
      res = -3;
    } else if((fd = mg_fs_open(fs, path, MG_FS_WRITE)) == NULL) {
      mg_http_reply(c, 400, "", "open(%s): %d", path, errno);
      res = -4;
    } else {
      res = offset + (long)fs->wr(fd->fd, hm->body.ptr, hm->body.len);
      mg_fs_close(fd);
      mg_http_reply(c, 200, "", "%ld", res);
    }
  }
  return res;
}

int mg_http_status(const struct mg_http_message* hm) {
  return atoi(hm->uri.ptr);
}

// If a server sends data to the client using chunked encoding, Mongoose strips
// off the chunking prefix (hex length and \r\n) and suffix (\r\n), appends the
// stripped data to the body, and fires the MG_EV_HTTP_CHUNK event.  When zero
// chunk is received, we fire MG_EV_HTTP_MSG, and the body already has all
// chunking prefixes/suffixes stripped.
//
// If a server sends data without chunked encoding, we also fire a series of
// MG_EV_HTTP_CHUNK events for every received piece of data, and then we fire
// MG_EV_HTTP_MSG event in the end.
//
// We track total processed length in the c->pfn_data, which is a void *
// pointer: we store a size_t value there.
static bool getchunk(struct mg_str s, size_t* prefixlen, size_t* datalen) {
  size_t i = 0, n;
  while(i < s.len && s.ptr[i] != '\r' && s.ptr[i] != '\n')
    i++;
  n = mg_unhexn(s.ptr, i);
  // MG_INFO(("%d %d", (int) (i + n + 4), (int) s.len));
  if(s.len < i + n + 4)
    return false; // Chunk not yet fully buffered
  if(s.ptr[i] != '\r' || s.ptr[i + 1] != '\n')
    return false;
  if(s.ptr[i + n + 2] != '\r' || s.ptr[i + n + 3] != '\n')
    return false;
  *prefixlen = i + 2;
  *datalen = n;
  return true;
}

static bool mg_is_chunked(struct mg_http_message* hm) {
  const char*    needle = "chunked";
  struct mg_str* te = mg_http_get_header(hm, "Transfer-Encoding");
  return te != NULL && mg_vcasecmp(te, needle) == 0;
}

void mg_http_delete_chunk(struct mg_connection* c, struct mg_http_message* hm) {
  size_t ofs = (size_t)(hm->chunk.ptr - (char*)c->recv.buf);
  mg_iobuf_del(&c->recv, ofs, hm->chunk.len);
  c->pfn_data = (void*)((size_t)c->pfn_data | MG_DMARK);
}

static void deliver_chunked_chunks(struct mg_connection* c, size_t hlen,
                                   struct mg_http_message* hm, bool* next) {
  //  |  ... headers ... | HEXNUM\r\n ..data.. \r\n | ......
  //  +------------------+--------------------------+----
  //  |      hlen        |           chunk1         | ......
  char * buf = (char*)&c->recv.buf[hlen], *p = buf;
  size_t len = c->recv.len - hlen;
  size_t processed = ((size_t)c->pfn_data) & ~MG_DMARK;
  size_t mark, pl, dl, del = 0, ofs = 0;
  bool   last = false;
  if(processed <= len)
    len -= processed, buf += processed;
  while(!last && getchunk(mg_str_n(buf + ofs, len - ofs), &pl, &dl)) {
    size_t saved = c->recv.len;
    memmove(p + processed, buf + ofs + pl, dl);
    // MG_INFO(("P2 [%.*s]", (int) (processed + dl), p));
    hm->chunk = mg_str_n(p + processed, dl);
    mg_call(c, MG_EV_HTTP_CHUNK, hm);
    ofs += pl + dl + 2, del += pl + 2; // 2 is for \r\n suffix
    processed += dl;
    if(c->recv.len != saved)
      processed -= dl, buf -= dl;
    // mg_hexdump(c->recv.buf, hlen + processed);
    last = (dl == 0);
  }
  mg_iobuf_del(&c->recv, hlen + processed, del);
  mark = ((size_t)c->pfn_data) & MG_DMARK;
  c->pfn_data = (void*)(processed | mark);
  if(last) {
    hm->body.len = processed;
    hm->message.len = hlen + processed;
    c->pfn_data = NULL;
    if(mark)
      mg_iobuf_del(&c->recv, 0, hlen), *next = true;
    // MG_INFO(("LAST, mark: %lx", mark));
    // mg_hexdump(c->recv.buf, c->recv.len);
  }
}

static void deliver_normal_chunks(struct mg_connection* c, size_t hlen,
                                  struct mg_http_message* hm, bool* next) {
  size_t left, processed = ((size_t)c->pfn_data) & ~MG_DMARK;
  size_t deleted = ((size_t)c->pfn_data) & MG_DMARK;
  hm->chunk = mg_str_n((char*)&c->recv.buf[hlen], c->recv.len - hlen);
  if(processed <= hm->chunk.len && !deleted) {
    hm->chunk.len -= processed;
    hm->chunk.ptr += processed;
  }
  left = hm->body.len < processed ? 0 : hm->body.len - processed;
  if(hm->chunk.len > left)
    hm->chunk.len = left;
  if(hm->chunk.len > 0)
    mg_call(c, MG_EV_HTTP_CHUNK, hm);
  processed += hm->chunk.len;
  deleted = ((size_t)c->pfn_data) & MG_DMARK; // Re-evaluate after user call
  if(processed >= hm->body.len) {             // Last, 0-len chunk
    hm->chunk.len = 0;                        // Reset length
    mg_call(c, MG_EV_HTTP_CHUNK, hm);         // Call user handler
    c->pfn_data = NULL;                       // Reset processed counter
    if(processed && deleted)
      mg_iobuf_del(&c->recv, 0, hlen), *next = true;
  } else {
    c->pfn_data = (void*)(processed | deleted); // if it is set
  }
}

static void http_cb(struct mg_connection* c, int ev, void* evd, void* fnd) {
  if(ev == MG_EV_READ || ev == MG_EV_CLOSE) {
    struct mg_http_message hm;
    while(c->recv.buf != NULL && c->recv.len > 0) {
      bool next = false;
      int  hlen = mg_http_parse((char*)c->recv.buf, c->recv.len, &hm);
      if(hlen < 0) {
        mg_error(c, "HTTP parse:\n%.*s", (int)c->recv.len, c->recv.buf);
        break;
      }
      if(c->is_resp)
        break; // Response is still generated
      if(hlen == 0)
        break;                        // Request is not buffered yet
      if(ev == MG_EV_CLOSE) {         // If client did not set Content-Length
        hm.message.len = c->recv.len; // and closes now, deliver a MSG
        hm.body.len = hm.message.len - (size_t)(hm.body.ptr - hm.message.ptr);
      }
      if(mg_is_chunked(&hm)) {
        deliver_chunked_chunks(c, (size_t)hlen, &hm, &next);
      } else {
        deliver_normal_chunks(c, (size_t)hlen, &hm, &next);
      }
      if(next)
        continue; // Chunks & request were deleted
      //  Chunk events are delivered. If we have full body, deliver MSG
      if(c->recv.len < hm.message.len)
        break;
      if(c->is_accepted)
        c->is_resp = 1;                // Start generating response
      mg_call(c, MG_EV_HTTP_MSG, &hm); // User handler can clear is_resp
      mg_iobuf_del(&c->recv, 0, hm.message.len);
    }
  }
  (void)evd, (void)fnd;
}

struct mg_connection* mg_http_connect(struct mg_mgr* mgr, const char* url,
                                      mg_event_handler_t fn, void* fn_data) {
  struct mg_connection* c = mg_connect(mgr, url, fn, fn_data);
  if(c != NULL)
    c->pfn = http_cb;
  return c;
}

struct mg_connection* mg_http_listen(struct mg_mgr* mgr, const char* url,
                                     mg_event_handler_t fn, void* fn_data) {
  struct mg_connection* c = mg_listen(mgr, url, fn, fn_data);
  if(c != NULL)
    c->pfn = http_cb;
  return c;
}

#ifdef MG_ENABLE_LINES
#line 1 "src/iobuf.c"
#endif

// Not using memset for zeroing memory, cause it can be dropped by compiler
// See https://github.com/cesanta/mongoose/pull/1265
static void zeromem(volatile unsigned char* buf, size_t len) {
  if(buf != NULL) {
    while(len--)
      *buf++ = 0;
  }
}

static size_t roundup(size_t size, size_t align) {
  return align == 0 ? size : (size + align - 1) / align * align;
}

int mg_iobuf_resize(struct mg_iobuf* io, size_t new_size) {
  int ok = 1;
  new_size = roundup(new_size, io->align);
  if(new_size == 0) {
    zeromem(io->buf, io->size);
    free(io->buf);
    io->buf = NULL;
    io->len = io->size = 0;
  } else if(new_size != io->size) {
    // NOTE(lsm): do not use realloc here. Use calloc/free only, to ease the
    // porting to some obscure platforms like FreeRTOS
    void* p = calloc(1, new_size);
    if(p != NULL) {
      size_t len = new_size < io->len ? new_size : io->len;
      if(len > 0 && io->buf != NULL)
        memmove(p, io->buf, len);
      zeromem(io->buf, io->size);
      free(io->buf);
      io->buf = (unsigned char*)p;
      io->size = new_size;
    } else {
      ok = 0;
      MG_ERROR(("%lld->%lld", (uint64_t)io->size, (uint64_t)new_size));
    }
  }
  return ok;
}

int mg_iobuf_init(struct mg_iobuf* io, size_t size, size_t align) {
  io->buf = NULL;
  io->align = align;
  io->size = io->len = 0;
  return mg_iobuf_resize(io, size);
}

size_t mg_iobuf_add(struct mg_iobuf* io, size_t ofs, const void* buf, size_t len) {
  size_t new_size = roundup(io->len + len, io->align);
  mg_iobuf_resize(io, new_size); // Attempt to resize
  if(new_size != io->size)
    len = 0; // Resize failure, append nothing
  if(ofs < io->len)
    memmove(io->buf + ofs + len, io->buf + ofs, io->len - ofs);
  if(buf != NULL)
    memmove(io->buf + ofs, buf, len);
  if(ofs > io->len)
    io->len += ofs - io->len;
  io->len += len;
  return len;
}

size_t mg_iobuf_del(struct mg_iobuf* io, size_t ofs, size_t len) {
  if(ofs > io->len)
    ofs = io->len;
  if(ofs + len > io->len)
    len = io->len - ofs;
  if(io->buf)
    memmove(io->buf + ofs, io->buf + ofs + len, io->len - ofs - len);
  if(io->buf)
    zeromem(io->buf + io->len - len, len);
  io->len -= len;
  return len;
}

void mg_iobuf_free(struct mg_iobuf* io) {
  mg_iobuf_resize(io, 0);
}

#ifdef MG_ENABLE_LINES
#line 1 "src/log.c"
#endif

static int      s_level = MG_LL_INFO;
static mg_pfn_t s_log_func = mg_pfn_stdout;
static void*    s_log_func_param = NULL;

void mg_log_set_fn(mg_pfn_t fn, void* param) {
  s_log_func = fn;
  s_log_func_param = param;
}

static void logc(unsigned char c) {
  s_log_func((char)c, s_log_func_param);
}

static void logs(const char* buf, size_t len) {
  size_t i;
  for(i = 0; i < len; i++)
    logc(((unsigned char*)buf)[i]);
}

void mg_log_set(int log_level) {
  MG_DEBUG(("Setting log level to %d", log_level));
  s_level = log_level;
}

bool mg_log_prefix(int level, const char* file, int line, const char* fname) {
  if(level <= s_level) {
    const char* p = strrchr(file, '/');
    char        buf[41];
    size_t      n;
    if(p == NULL)
      p = strrchr(file, '\\');
    n = mg_snprintf(buf, sizeof(buf), "%-6llx %d %s:%d:%s", mg_millis(), level,
                    p == NULL ? file : p + 1, line, fname);
    if(n > sizeof(buf) - 2)
      n = sizeof(buf) - 2;
    while(n < sizeof(buf))
      buf[n++] = ' ';
    logs(buf, n - 1);
    return true;
  } else {
    return false;
  }
}

void mg_log(const char* fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  mg_vxprintf(s_log_func, s_log_func_param, fmt, &ap);
  va_end(ap);
  logs("\r\n", 2);
}

static unsigned char nibble(unsigned c) {
  return (unsigned char)(c < 10 ? c + '0' : c + 'W');
}

#define ISPRINT(x) ((x) >= ' ' && (x) <= '~')
void mg_hexdump(const void* buf, size_t len) {
  const unsigned char* p = (const unsigned char*)buf;
  unsigned char        ascii[16], alen = 0;
  size_t               i;
  for(i = 0; i < len; i++) {
    if((i % 16) == 0) {
      // Print buffered ascii chars
      if(i > 0)
        logs("  ", 2), logs((char*)ascii, 16), logc('\n'), alen = 0;
      // Print hex address, then \t
      logc(nibble((i >> 12) & 15)), logc(nibble((i >> 8) & 15)),
          logc(nibble((i >> 4) & 15)), logc('0'), logs("   ", 3);
    }
    logc(nibble(p[i] >> 4)), logc(nibble(p[i] & 15)); // Two nibbles, e.g. c5
    logc(' ');                                        // Space after hex number
    ascii[alen++] = ISPRINT(p[i]) ? p[i] : '.';       // Add to the ascii buf
  }
  while(alen < 16)
    logs("   ", 3), ascii[alen++] = ' ';
  logs("  ", 2), logs((char*)ascii, 16), logc('\n');
}

#ifdef MG_ENABLE_LINES
#line 1 "src/md5.c"
#endif

#if defined(MG_ENABLE_MD5) && MG_ENABLE_MD5

static void mg_byte_reverse(unsigned char* buf, unsigned longs) {
  if(MG_BIG_ENDIAN) {
    do {
      uint32_t t = (uint32_t)((unsigned)buf[3] << 8 | buf[2]) << 16 |
                   ((unsigned)buf[1] << 8 | buf[0]);
      *(uint32_t*)buf = t;
      buf += 4;
    } while(--longs);
  } else {
    (void)buf, (void)longs; // Little endian. Do nothing
  }
}

#define F1(x, y, z) (z ^ (x & (y ^ z)))
#define F2(x, y, z) F1(z, x, y)
#define F3(x, y, z) (x ^ y ^ z)
#define F4(x, y, z) (y ^ (x | ~z))

#define MD5STEP(f, w, x, y, z, data, s)                                             \
  (w += f(x, y, z) + data, w = w << s | w >> (32 - s), w += x)

/*
 * Start MD5 accumulation.  Set bit count to 0 and buffer to mysterious
 * initialization constants.
 */
void mg_md5_init(mg_md5_ctx* ctx) {
  ctx->buf[0] = 0x67452301;
  ctx->buf[1] = 0xefcdab89;
  ctx->buf[2] = 0x98badcfe;
  ctx->buf[3] = 0x10325476;

  ctx->bits[0] = 0;
  ctx->bits[1] = 0;
}

static void mg_md5_transform(uint32_t buf[4], uint32_t const in[16]) {
  uint32_t a, b, c, d;

  a = buf[0];
  b = buf[1];
  c = buf[2];
  d = buf[3];

  MD5STEP(F1, a, b, c, d, in[0] + 0xd76aa478, 7);
  MD5STEP(F1, d, a, b, c, in[1] + 0xe8c7b756, 12);
  MD5STEP(F1, c, d, a, b, in[2] + 0x242070db, 17);
  MD5STEP(F1, b, c, d, a, in[3] + 0xc1bdceee, 22);
  MD5STEP(F1, a, b, c, d, in[4] + 0xf57c0faf, 7);
  MD5STEP(F1, d, a, b, c, in[5] + 0x4787c62a, 12);
  MD5STEP(F1, c, d, a, b, in[6] + 0xa8304613, 17);
  MD5STEP(F1, b, c, d, a, in[7] + 0xfd469501, 22);
  MD5STEP(F1, a, b, c, d, in[8] + 0x698098d8, 7);
  MD5STEP(F1, d, a, b, c, in[9] + 0x8b44f7af, 12);
  MD5STEP(F1, c, d, a, b, in[10] + 0xffff5bb1, 17);
  MD5STEP(F1, b, c, d, a, in[11] + 0x895cd7be, 22);
  MD5STEP(F1, a, b, c, d, in[12] + 0x6b901122, 7);
  MD5STEP(F1, d, a, b, c, in[13] + 0xfd987193, 12);
  MD5STEP(F1, c, d, a, b, in[14] + 0xa679438e, 17);
  MD5STEP(F1, b, c, d, a, in[15] + 0x49b40821, 22);

  MD5STEP(F2, a, b, c, d, in[1] + 0xf61e2562, 5);
  MD5STEP(F2, d, a, b, c, in[6] + 0xc040b340, 9);
  MD5STEP(F2, c, d, a, b, in[11] + 0x265e5a51, 14);
  MD5STEP(F2, b, c, d, a, in[0] + 0xe9b6c7aa, 20);
  MD5STEP(F2, a, b, c, d, in[5] + 0xd62f105d, 5);
  MD5STEP(F2, d, a, b, c, in[10] + 0x02441453, 9);
  MD5STEP(F2, c, d, a, b, in[15] + 0xd8a1e681, 14);
  MD5STEP(F2, b, c, d, a, in[4] + 0xe7d3fbc8, 20);
  MD5STEP(F2, a, b, c, d, in[9] + 0x21e1cde6, 5);
  MD5STEP(F2, d, a, b, c, in[14] + 0xc33707d6, 9);
  MD5STEP(F2, c, d, a, b, in[3] + 0xf4d50d87, 14);
  MD5STEP(F2, b, c, d, a, in[8] + 0x455a14ed, 20);
  MD5STEP(F2, a, b, c, d, in[13] + 0xa9e3e905, 5);
  MD5STEP(F2, d, a, b, c, in[2] + 0xfcefa3f8, 9);
  MD5STEP(F2, c, d, a, b, in[7] + 0x676f02d9, 14);
  MD5STEP(F2, b, c, d, a, in[12] + 0x8d2a4c8a, 20);

  MD5STEP(F3, a, b, c, d, in[5] + 0xfffa3942, 4);
  MD5STEP(F3, d, a, b, c, in[8] + 0x8771f681, 11);
  MD5STEP(F3, c, d, a, b, in[11] + 0x6d9d6122, 16);
  MD5STEP(F3, b, c, d, a, in[14] + 0xfde5380c, 23);
  MD5STEP(F3, a, b, c, d, in[1] + 0xa4beea44, 4);
  MD5STEP(F3, d, a, b, c, in[4] + 0x4bdecfa9, 11);
  MD5STEP(F3, c, d, a, b, in[7] + 0xf6bb4b60, 16);
  MD5STEP(F3, b, c, d, a, in[10] + 0xbebfbc70, 23);
  MD5STEP(F3, a, b, c, d, in[13] + 0x289b7ec6, 4);
  MD5STEP(F3, d, a, b, c, in[0] + 0xeaa127fa, 11);
  MD5STEP(F3, c, d, a, b, in[3] + 0xd4ef3085, 16);
  MD5STEP(F3, b, c, d, a, in[6] + 0x04881d05, 23);
  MD5STEP(F3, a, b, c, d, in[9] + 0xd9d4d039, 4);
  MD5STEP(F3, d, a, b, c, in[12] + 0xe6db99e5, 11);
  MD5STEP(F3, c, d, a, b, in[15] + 0x1fa27cf8, 16);
  MD5STEP(F3, b, c, d, a, in[2] + 0xc4ac5665, 23);

  MD5STEP(F4, a, b, c, d, in[0] + 0xf4292244, 6);
  MD5STEP(F4, d, a, b, c, in[7] + 0x432aff97, 10);
  MD5STEP(F4, c, d, a, b, in[14] + 0xab9423a7, 15);
  MD5STEP(F4, b, c, d, a, in[5] + 0xfc93a039, 21);
  MD5STEP(F4, a, b, c, d, in[12] + 0x655b59c3, 6);
  MD5STEP(F4, d, a, b, c, in[3] + 0x8f0ccc92, 10);
  MD5STEP(F4, c, d, a, b, in[10] + 0xffeff47d, 15);
  MD5STEP(F4, b, c, d, a, in[1] + 0x85845dd1, 21);
  MD5STEP(F4, a, b, c, d, in[8] + 0x6fa87e4f, 6);
  MD5STEP(F4, d, a, b, c, in[15] + 0xfe2ce6e0, 10);
  MD5STEP(F4, c, d, a, b, in[6] + 0xa3014314, 15);
  MD5STEP(F4, b, c, d, a, in[13] + 0x4e0811a1, 21);
  MD5STEP(F4, a, b, c, d, in[4] + 0xf7537e82, 6);
  MD5STEP(F4, d, a, b, c, in[11] + 0xbd3af235, 10);
  MD5STEP(F4, c, d, a, b, in[2] + 0x2ad7d2bb, 15);
  MD5STEP(F4, b, c, d, a, in[9] + 0xeb86d391, 21);

  buf[0] += a;
  buf[1] += b;
  buf[2] += c;
  buf[3] += d;
}

void mg_md5_update(mg_md5_ctx* ctx, const unsigned char* buf, size_t len) {
  uint32_t t;

  t = ctx->bits[0];
  if((ctx->bits[0] = t + ((uint32_t)len << 3)) < t)
    ctx->bits[1]++;
  ctx->bits[1] += (uint32_t)len >> 29;

  t = (t >> 3) & 0x3f;

  if(t) {
    unsigned char* p = (unsigned char*)ctx->in + t;

    t = 64 - t;
    if(len < t) {
      memcpy(p, buf, len);
      return;
    }
    memcpy(p, buf, t);
    mg_byte_reverse(ctx->in, 16);
    mg_md5_transform(ctx->buf, (uint32_t*)ctx->in);
    buf += t;
    len -= t;
  }

  while(len >= 64) {
    memcpy(ctx->in, buf, 64);
    mg_byte_reverse(ctx->in, 16);
    mg_md5_transform(ctx->buf, (uint32_t*)ctx->in);
    buf += 64;
    len -= 64;
  }

  memcpy(ctx->in, buf, len);
}

void mg_md5_final(mg_md5_ctx* ctx, unsigned char digest[16]) {
  unsigned       count;
  unsigned char* p;
  uint32_t*      a;

  count = (ctx->bits[0] >> 3) & 0x3F;

  p = ctx->in + count;
  *p++ = 0x80;
  count = 64 - 1 - count;
  if(count < 8) {
    memset(p, 0, count);
    mg_byte_reverse(ctx->in, 16);
    mg_md5_transform(ctx->buf, (uint32_t*)ctx->in);
    memset(ctx->in, 0, 56);
  } else {
    memset(p, 0, count - 8);
  }
  mg_byte_reverse(ctx->in, 14);

  a = (uint32_t*)ctx->in;
  a[14] = ctx->bits[0];
  a[15] = ctx->bits[1];

  mg_md5_transform(ctx->buf, (uint32_t*)ctx->in);
  mg_byte_reverse((unsigned char*)ctx->buf, 4);
  memcpy(digest, ctx->buf, 16);
  memset((char*)ctx, 0, sizeof(*ctx));
}
#endif

#ifdef MG_ENABLE_LINES
#line 1 "src/net.c"
#endif

size_t mg_vprintf(struct mg_connection* c, const char* fmt, va_list* ap) {
  size_t old = c->send.len;
  mg_vxprintf(mg_pfn_iobuf, &c->send, fmt, ap);
  return c->send.len - old;
}

size_t mg_printf(struct mg_connection* c, const char* fmt, ...) {
  size_t  len = 0;
  va_list ap;
  va_start(ap, fmt);
  len = mg_vprintf(c, fmt, &ap);
  va_end(ap);
  return len;
}

static bool mg_atonl(struct mg_str str, struct mg_addr* addr) {
  uint32_t localhost = mg_htonl(0x7f000001);
  if(mg_vcasecmp(&str, "localhost") != 0)
    return false;
  memcpy(addr->ip, &localhost, sizeof(uint32_t));
  addr->is_ip6 = false;
  return true;
}

static bool mg_atone(struct mg_str str, struct mg_addr* addr) {
  if(str.len > 0)
    return false;
  memset(addr->ip, 0, sizeof(addr->ip));
  addr->is_ip6 = false;
  return true;
}

static bool mg_aton4(struct mg_str str, struct mg_addr* addr) {
  uint8_t data[4] = {0, 0, 0, 0};
  size_t  i, num_dots = 0;
  for(i = 0; i < str.len; i++) {
    if(str.ptr[i] >= '0' && str.ptr[i] <= '9') {
      int octet = data[num_dots] * 10 + (str.ptr[i] - '0');
      if(octet > 255)
        return false;
      data[num_dots] = (uint8_t)octet;
    } else if(str.ptr[i] == '.') {
      if(num_dots >= 3 || i == 0 || str.ptr[i - 1] == '.')
        return false;
      num_dots++;
    } else {
      return false;
    }
  }
  if(num_dots != 3 || str.ptr[i - 1] == '.')
    return false;
  memcpy(&addr->ip, data, sizeof(data));
  addr->is_ip6 = false;
  return true;
}

static bool mg_v4mapped(struct mg_str str, struct mg_addr* addr) {
  int      i;
  uint32_t ipv4;
  if(str.len < 14)
    return false;
  if(str.ptr[0] != ':' || str.ptr[1] != ':' || str.ptr[6] != ':')
    return false;
  for(i = 2; i < 6; i++) {
    if(str.ptr[i] != 'f' && str.ptr[i] != 'F')
      return false;
  }
  // struct mg_str s = mg_str_n(&str.ptr[7], str.len - 7);
  if(!mg_aton4(mg_str_n(&str.ptr[7], str.len - 7), addr))
    return false;
  memcpy(&ipv4, addr->ip, sizeof(ipv4));
  memset(addr->ip, 0, sizeof(addr->ip));
  addr->ip[10] = addr->ip[11] = 255;
  memcpy(&addr->ip[12], &ipv4, 4);
  addr->is_ip6 = true;
  return true;
}

static bool mg_aton6(struct mg_str str, struct mg_addr* addr) {
  size_t i, j = 0, n = 0, dc = 42;
  if(str.len > 2 && str.ptr[0] == '[')
    str.ptr++, str.len -= 2;
  if(mg_v4mapped(str, addr))
    return true;
  for(i = 0; i < str.len; i++) {
    if((str.ptr[i] >= '0' && str.ptr[i] <= '9') ||
       (str.ptr[i] >= 'a' && str.ptr[i] <= 'f') ||
       (str.ptr[i] >= 'A' && str.ptr[i] <= 'F')) {
      unsigned long val;
      if(i > j + 3)
        return false;
      // MG_DEBUG(("%zu %zu [%.*s]", i, j, (int) (i - j + 1), &str.ptr[j]));
      val = mg_unhexn(&str.ptr[j], i - j + 1);
      addr->ip[n] = (uint8_t)((val >> 8) & 255);
      addr->ip[n + 1] = (uint8_t)(val & 255);
    } else if(str.ptr[i] == ':') {
      j = i + 1;
      if(i > 0 && str.ptr[i - 1] == ':') {
        dc = n; // Double colon
        if(i > 1 && str.ptr[i - 2] == ':')
          return false;
      } else if(i > 0) {
        n += 2;
      }
      if(n > 14)
        return false;
      addr->ip[n] = addr->ip[n + 1] = 0; // For trailing ::
    } else {
      return false;
    }
  }
  if(n < 14 && dc == 42)
    return false;
  if(n < 14) {
    memmove(&addr->ip[dc + (14 - n)], &addr->ip[dc], n - dc + 2);
    memset(&addr->ip[dc], 0, 14 - n);
  }

  addr->is_ip6 = true;
  return true;
}

bool mg_aton(struct mg_str str, struct mg_addr* addr) {
  // MG_INFO(("[%.*s]", (int) str.len, str.ptr));
  return mg_atone(str, addr) || mg_atonl(str, addr) || mg_aton4(str, addr) ||
         mg_aton6(str, addr);
}

struct mg_connection* mg_alloc_conn(struct mg_mgr* mgr) {
  struct mg_connection* c =
      (struct mg_connection*)calloc(1, sizeof(*c) + mgr->extraconnsize);
  if(c != NULL) {
    c->mgr = mgr;
    c->send.align = c->recv.align = MG_IO_SIZE;
    c->id = ++mgr->nextid;
  }
  return c;
}

void mg_close_conn(struct mg_connection* c) {
  mg_resolve_cancel(c); // Close any pending DNS query
  LIST_DELETE(struct mg_connection, &c->mgr->conns, c);
  if(c == c->mgr->dns4.c)
    c->mgr->dns4.c = NULL;
  if(c == c->mgr->dns6.c)
    c->mgr->dns6.c = NULL;
  // Order of operations is important. `MG_EV_CLOSE` event must be fired
  // before we deallocate received data, see #1331
  mg_call(c, MG_EV_CLOSE, NULL);
  MG_DEBUG(("%lu %p closed", c->id, c->fd));

  mg_tls_free(c);
  mg_iobuf_free(&c->recv);
  mg_iobuf_free(&c->send);
  memset(c, 0, sizeof(*c));
  free(c);
}

struct mg_connection* mg_connect(struct mg_mgr* mgr, const char* url,
                                 mg_event_handler_t fn, void* fn_data) {
  struct mg_connection* c = NULL;
  if(url == NULL || url[0] == '\0') {
    MG_ERROR(("null url"));
  } else if((c = mg_alloc_conn(mgr)) == NULL) {
    MG_ERROR(("OOM"));
  } else {
    LIST_ADD_HEAD(struct mg_connection, &mgr->conns, c);
    c->is_udp = (strncmp(url, "udp:", 4) == 0);
    c->fd = (void*)(size_t)MG_INVALID_SOCKET;
    c->fn = fn;
    c->is_client = true;
    c->fn_data = fn_data;
    MG_DEBUG(("%lu %p %s", c->id, c->fd, url));
    mg_call(c, MG_EV_OPEN, (void*)url);
    mg_resolve(c, url);
    if(mg_url_is_ssl(url)) {
      struct mg_str host = mg_url_host(url);
      mg_tls_init(c, host);
    }
  }
  return c;
}

struct mg_connection* mg_listen(struct mg_mgr* mgr, const char* url,
                                mg_event_handler_t fn, void* fn_data) {
  struct mg_connection* c = NULL;
  if((c = mg_alloc_conn(mgr)) == NULL) {
    MG_ERROR(("OOM %s", url));
  } else if(!mg_open_listener(c, url)) {
    MG_ERROR(("Failed: %s, errno %d", url, errno));
    free(c);
    c = NULL;
  } else {
    c->is_listening = 1;
    c->is_udp = strncmp(url, "udp:", 4) == 0;
    LIST_ADD_HEAD(struct mg_connection, &mgr->conns, c);
    c->fn = fn;
    c->fn_data = fn_data;
    mg_call(c, MG_EV_OPEN, NULL);
    if(mg_url_is_ssl(url))
      c->is_tls = 1; // Accepted connection must
    MG_DEBUG(("%lu %p %s", c->id, c->fd, url));
  }
  return c;
}

struct mg_connection* mg_wrapfd(struct mg_mgr* mgr, int fd, mg_event_handler_t fn,
                                void* fn_data) {
  struct mg_connection* c = mg_alloc_conn(mgr);
  if(c != NULL) {
    c->fd = (void*)(size_t)fd;
    c->fn = fn;
    c->fn_data = fn_data;
    MG_EPOLL_ADD(c);
    mg_call(c, MG_EV_OPEN, NULL);
    LIST_ADD_HEAD(struct mg_connection, &mgr->conns, c);
  }
  return c;
}

struct mg_timer* mg_timer_add(struct mg_mgr* mgr, uint64_t milliseconds,
                              unsigned flags, void (*fn)(void*), void* arg) {
  struct mg_timer* t = (struct mg_timer*)calloc(1, sizeof(*t));
  if(t != NULL) {
    mg_timer_init(&mgr->timers, t, milliseconds, flags, fn, arg);
    t->id = mgr->timerid++;
  }
  return t;
}

void mg_mgr_free(struct mg_mgr* mgr) {
  struct mg_connection* c;
  struct mg_timer *     tmp, *t = mgr->timers;
  while(t != NULL)
    tmp = t->next, free(t), t = tmp;
  mgr->timers = NULL; // Important. Next call to poll won't touch timers
  for(c = mgr->conns; c != NULL; c = c->next)
    c->is_closing = 1;
  mg_mgr_poll(mgr, 0);
#if MG_ENABLE_FREERTOS_TCP
  FreeRTOS_DeleteSocketSet(mgr->ss);
#endif
  MG_DEBUG(("All connections closed"));
#if MG_ENABLE_EPOLL
  if(mgr->epoll_fd >= 0)
    close(mgr->epoll_fd), mgr->epoll_fd = -1;
#endif
  mg_tls_ctx_free(mgr);
}

void mg_mgr_init(struct mg_mgr* mgr) {
  memset(mgr, 0, sizeof(*mgr));
#if MG_ENABLE_EPOLL
  if((mgr->epoll_fd = epoll_create1(EPOLL_CLOEXEC)) < 0)
    MG_ERROR(("epoll_create1 errno %d", errno));
#else
  mgr->epoll_fd = -1;
#endif
#if MG_ARCH == MG_ARCH_WIN32 && MG_ENABLE_WINSOCK
  // clang-format off
  { WSADATA data; WSAStartup(MAKEWORD(2, 2), &data); }
  // clang-format on
#elif MG_ENABLE_FREERTOS_TCP
  mgr->ss = FreeRTOS_CreateSocketSet();
#elif defined(__unix) || defined(__unix__) || defined(__APPLE__)
  // Ignore SIGPIPE signal, so if client cancels the request, it
  // won't kill the whole process.
  signal(SIGPIPE, SIG_IGN);
#endif
  mgr->dnstimeout = 3000;
  mgr->dns4.url = "udp://8.8.8.8:53";
  mgr->dns6.url = "udp://[2001:4860:4860::8888]:53";
}

#ifdef MG_ENABLE_LINES
#line 1 "src/printf.c"
#endif

static void mg_pfn_iobuf_private(char ch, void* param, bool expand) {
  struct mg_iobuf* io = (struct mg_iobuf*)param;
  if(expand && io->len + 2 > io->size)
    mg_iobuf_resize(io, io->len + 2);
  if(io->len + 2 <= io->size) {
    io->buf[io->len++] = (uint8_t)ch;
    io->buf[io->len] = 0;
  } else if(io->len < io->size) {
    io->buf[io->len++] = 0; // Guarantee to 0-terminate
  }
}

static void mg_putchar_iobuf_static(char ch, void* param) {
  mg_pfn_iobuf_private(ch, param, false);
}

void mg_pfn_iobuf(char ch, void* param) {
  mg_pfn_iobuf_private(ch, param, true);
}

size_t mg_vsnprintf(char* buf, size_t len, const char* fmt, va_list* ap) {
  struct mg_iobuf io = {(uint8_t*)buf, len, 0, 0};
  size_t          n = mg_vxprintf(mg_putchar_iobuf_static, &io, fmt, ap);
  if(n < len)
    buf[n] = '\0';
  return n;
}

size_t mg_snprintf(char* buf, size_t len, const char* fmt, ...) {
  va_list ap;
  size_t  n;
  va_start(ap, fmt);
  n = mg_vsnprintf(buf, len, fmt, &ap);
  va_end(ap);
  return n;
}

char* mg_vmprintf(const char* fmt, va_list* ap) {
  struct mg_iobuf io = {0, 0, 0, 256};
  mg_vxprintf(mg_pfn_iobuf, &io, fmt, ap);
  return (char*)io.buf;
}

char* mg_mprintf(const char* fmt, ...) {
  char*   s;
  va_list ap;
  va_start(ap, fmt);
  s = mg_vmprintf(fmt, &ap);
  va_end(ap);
  return s;
}

void mg_pfn_stdout(char c, void* param) {
  putchar(c);
  (void)param;
}

static size_t print_ip4(void (*out)(char, void*), void* arg, uint8_t* p) {
  return mg_xprintf(out, arg, "%d.%d.%d.%d", p[0], p[1], p[2], p[3]);
}

static size_t print_ip6(void (*out)(char, void*), void* arg, uint16_t* p) {
  return mg_xprintf(out, arg, "[%x:%x:%x:%x:%x:%x:%x:%x]", mg_ntohs(p[0]),
                    mg_ntohs(p[1]), mg_ntohs(p[2]), mg_ntohs(p[3]), mg_ntohs(p[4]),
                    mg_ntohs(p[5]), mg_ntohs(p[6]), mg_ntohs(p[7]));
}

size_t mg_print_ip4(void (*out)(char, void*), void* arg, va_list* ap) {
  uint8_t* p = va_arg(*ap, uint8_t*);
  return print_ip4(out, arg, p);
}

size_t mg_print_ip6(void (*out)(char, void*), void* arg, va_list* ap) {
  uint16_t* p = va_arg(*ap, uint16_t*);
  return print_ip6(out, arg, p);
}

size_t mg_print_ip(void (*out)(char, void*), void* arg, va_list* ap) {
  struct mg_addr* addr = va_arg(*ap, struct mg_addr*);
  if(addr->is_ip6)
    return print_ip6(out, arg, (uint16_t*)addr->ip);
  return print_ip4(out, arg, (uint8_t*)&addr->ip);
}

size_t mg_print_ip_port(void (*out)(char, void*), void* arg, va_list* ap) {
  struct mg_addr* a = va_arg(*ap, struct mg_addr*);
  return mg_xprintf(out, arg, "%M:%hu", mg_print_ip, a, mg_ntohs(a->port));
}

size_t mg_print_mac(void (*out)(char, void*), void* arg, va_list* ap) {
  uint8_t* p = va_arg(*ap, uint8_t*);
  return mg_xprintf(out, arg, "%02x:%02x:%02x:%02x:%02x:%02x", p[0], p[1], p[2],
                    p[3], p[4], p[5]);
}

static char mg_esc(int c, bool esc) {
  const char *p, *esc1 = "\b\f\n\r\t\\\"", *esc2 = "bfnrt\\\"";
  for(p = esc ? esc1 : esc2; *p != '\0'; p++) {
    if(*p == c)
      return esc ? esc2[p - esc1] : esc1[p - esc2];
  }
  return 0;
}

static char mg_escape(int c) {
  return mg_esc(c, true);
}

static size_t qcpy(void (*out)(char, void*), void* ptr, char* buf, size_t len) {
  size_t i = 0, extra = 0;
  for(i = 0; i < len && buf[i] != '\0'; i++) {
    char c = mg_escape(buf[i]);
    if(c) {
      out('\\', ptr), out(c, ptr), extra++;
    } else {
      out(buf[i], ptr);
    }
  }
  return i + extra;
}

static size_t bcpy(void (*out)(char, void*), void* arg, uint8_t* buf, size_t len) {
  size_t      i, j, n = 0;
  const char* t = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  for(i = 0; i < len; i += 3) {
    uint8_t c1 = buf[i], c2 = i + 1 < len ? buf[i + 1] : 0,
            c3 = i + 2 < len ? buf[i + 2] : 0;
    char tmp[4] = {t[c1 >> 2], t[(c1 & 3) << 4 | (c2 >> 4)], '=', '='};
    if(i + 1 < len)
      tmp[2] = t[(c2 & 15) << 2 | (c3 >> 6)];
    if(i + 2 < len)
      tmp[3] = t[c3 & 63];
    for(j = 0; j < sizeof(tmp) && tmp[j] != '\0'; j++)
      out(tmp[j], arg);
    n += j;
  }
  return n;
}

size_t mg_print_hex(void (*out)(char, void*), void* arg, va_list* ap) {
  size_t      bl = (size_t)va_arg(*ap, int);
  uint8_t*    p = va_arg(*ap, uint8_t*);
  const char* hex = "0123456789abcdef";
  size_t      j;
  for(j = 0; j < bl; j++) {
    out(hex[(p[j] >> 4) & 0x0F], arg);
    out(hex[p[j] & 0x0F], arg);
  }
  return 2 * bl;
}
size_t mg_print_base64(void (*out)(char, void*), void* arg, va_list* ap) {
  size_t   len = (size_t)va_arg(*ap, int);
  uint8_t* buf = va_arg(*ap, uint8_t*);
  return bcpy(out, arg, buf, len);
}

size_t mg_print_esc(void (*out)(char, void*), void* arg, va_list* ap) {
  size_t len = (size_t)va_arg(*ap, int);
  char*  p = va_arg(*ap, char*);
  if(len == 0)
    len = p == NULL ? 0 : strlen(p);
  return qcpy(out, arg, p, len);
}

#ifdef MG_ENABLE_LINES
#line 1 "src/sock.c"
#endif

#if MG_ENABLE_SOCKET

#ifndef closesocket
#define closesocket(x) close(x)
#endif

#define FD(c_) ((MG_SOCKET_TYPE)(size_t)(c_)->fd)
#define S2PTR(s_) ((void*)(size_t)(s_))

#ifndef MSG_NONBLOCKING
#define MSG_NONBLOCKING 0
#endif

#ifndef AF_INET6
#define AF_INET6 10
#endif

#ifndef MG_SOCK_ERR
#define MG_SOCK_ERR(errcode) ((errcode) < 0 ? errno : 0)
#endif

#ifndef MG_SOCK_INTR
#define MG_SOCK_INTR(fd) (fd == MG_INVALID_SOCKET && MG_SOCK_ERR(-1) == EINTR)
#endif

#ifndef MG_SOCK_PENDING
#define MG_SOCK_PENDING(errcode)                                                    \
  (((errcode) < 0) && (errno == EINPROGRESS || errno == EWOULDBLOCK))
#endif

#ifndef MG_SOCK_RESET
#define MG_SOCK_RESET(errcode)                                                      \
  (((errcode) < 0) && (errno == EPIPE || errno == ECONNRESET))
#endif

union usa {
  struct sockaddr    sa;
  struct sockaddr_in sin;
#if MG_ENABLE_IPV6
  struct sockaddr_in6 sin6;
#endif
};

static socklen_t tousa(struct mg_addr* a, union usa* usa) {
  socklen_t len = sizeof(usa->sin);
  memset(usa, 0, sizeof(*usa));
  usa->sin.sin_family = AF_INET;
  usa->sin.sin_port = a->port;
  memcpy(&usa->sin.sin_addr, a->ip, sizeof(uint32_t));
#if MG_ENABLE_IPV6
  if(a->is_ip6) {
    usa->sin.sin_family = AF_INET6;
    usa->sin6.sin6_port = a->port;
    memcpy(&usa->sin6.sin6_addr, a->ip, sizeof(a->ip));
    len = sizeof(usa->sin6);
  }
#endif
  return len;
}

static void tomgaddr(union usa* usa, struct mg_addr* a, bool is_ip6) {
  a->is_ip6 = is_ip6;
  a->port = usa->sin.sin_port;
  memcpy(&a->ip, &usa->sin.sin_addr, sizeof(uint32_t));
#if MG_ENABLE_IPV6
  if(is_ip6) {
    memcpy(a->ip, &usa->sin6.sin6_addr, sizeof(a->ip));
    a->port = usa->sin6.sin6_port;
  }
#endif
}

static void setlocaddr(MG_SOCKET_TYPE fd, struct mg_addr* addr) {
  union usa usa;
  socklen_t n = sizeof(usa);
  if(getsockname(fd, &usa.sa, &n) == 0) {
    tomgaddr(&usa, addr, n != sizeof(usa.sin));
  }
}

static void iolog(struct mg_connection* c, char* buf, long n, bool r) {
  if(n == MG_IO_WAIT) {
    // Do nothing
  } else if(n <= 0) {
    c->is_closing = 1; // Termination. Don't call mg_error(): #1529
  } else if(n > 0) {
    if(c->is_hexdumping) {
      union usa usa;
      socklen_t slen = sizeof(usa.sin);
      if(getsockname(FD(c), &usa.sa, &slen) < 0)
        (void)0; // Ignore result
      MG_INFO(("\n-- %lu %M %s %M %ld", c->id, mg_print_ip_port, &c->loc,
               r ? "<-" : "->", mg_print_ip_port, &c->rem, n));

      mg_hexdump(buf, (size_t)n);
    }
    if(r) {
      c->recv.len += (size_t)n;
      mg_call(c, MG_EV_READ, &n);
    } else {
      mg_iobuf_del(&c->send, 0, (size_t)n);
      // if (c->send.len == 0) mg_iobuf_resize(&c->send, 0);
      if(c->send.len == 0) {
        MG_EPOLL_MOD(c, 0);
      }
      mg_call(c, MG_EV_WRITE, &n);
    }
  }
}

long mg_io_send(struct mg_connection* c, const void* buf, size_t len) {
  long n;
  if(c->is_udp) {
    union usa usa;
    socklen_t slen = tousa(&c->rem, &usa);
    n = sendto(FD(c), (char*)buf, len, 0, &usa.sa, slen);
    if(n > 0)
      setlocaddr(FD(c), &c->loc);
  } else {
    n = send(FD(c), (char*)buf, len, MSG_NONBLOCKING);
  }
  if(MG_SOCK_PENDING(n))
    return MG_IO_WAIT;
  if(MG_SOCK_RESET(n))
    return MG_IO_RESET;
  if(n <= 0)
    return MG_IO_ERR;
  return n;
}

bool mg_send(struct mg_connection* c, const void* buf, size_t len) {
  if(c->is_udp) {
    long n = mg_io_send(c, buf, len);
    MG_DEBUG(("%lu %p %d:%d %ld err %d", c->id, c->fd, (int)c->send.len,
              (int)c->recv.len, n, MG_SOCK_ERR(n)));
    iolog(c, (char*)buf, n, false);
    return n > 0;
  } else {
    return mg_iobuf_add(&c->send, c->send.len, buf, len);
  }
}

static void mg_set_non_blocking_mode(MG_SOCKET_TYPE fd) {
#if defined(MG_CUSTOM_NONBLOCK)
  MG_CUSTOM_NONBLOCK(fd);
#elif MG_ARCH == MG_ARCH_WIN32 && MG_ENABLE_WINSOCK
  unsigned long on = 1;
  ioctlsocket(fd, FIONBIO, &on);
#elif MG_ENABLE_RL
  unsigned long on = 1;
  ioctlsocket(fd, FIONBIO, &on);
#elif MG_ENABLE_FREERTOS_TCP
  const BaseType_t off = 0;
  if(setsockopt(fd, 0, FREERTOS_SO_RCVTIMEO, &off, sizeof(off)) != 0)
    (void)0;
  if(setsockopt(fd, 0, FREERTOS_SO_SNDTIMEO, &off, sizeof(off)) != 0)
    (void)0;
#elif MG_ENABLE_LWIP
  lwip_fcntl(fd, F_SETFL, O_NONBLOCK);
#elif MG_ARCH == MG_ARCH_AZURERTOS
  fcntl(fd, F_SETFL, O_NONBLOCK);
#elif MG_ARCH == MG_ARCH_TIRTOS
  int val = 0;
  setsockopt(fd, SOL_SOCKET, SO_BLOCKING, &val, sizeof(val));
  // SPRU524J section 3.3.3 page 63, SO_SNDLOWAT
  int sz = sizeof(val);
  getsockopt(fd, SOL_SOCKET, SO_SNDBUF, &val, &sz);
  val /= 2; // set send low-water mark at half send buffer size
  setsockopt(fd, SOL_SOCKET, SO_SNDLOWAT, &val, sizeof(val));
#else
  fcntl(fd, F_SETFL, fcntl(fd, F_GETFL, 0) | O_NONBLOCK); // Non-blocking mode
  fcntl(fd, F_SETFD, FD_CLOEXEC);                         // Set close-on-exec
#endif
}

bool mg_open_listener(struct mg_connection* c, const char* url) {
  MG_SOCKET_TYPE fd = MG_INVALID_SOCKET;
  bool           success = false;
  c->loc.port = mg_htons(mg_url_port(url));
  if(!mg_aton(mg_url_host(url), &c->loc)) {
    MG_ERROR(("invalid listening URL: %s", url));
  } else {
    union usa usa;
    socklen_t slen = tousa(&c->loc, &usa);
    int       rc, on = 1, af = c->loc.is_ip6 ? AF_INET6 : AF_INET;
    int       type = strncmp(url, "udp:", 4) == 0 ? SOCK_DGRAM : SOCK_STREAM;
    int       proto = type == SOCK_DGRAM ? IPPROTO_UDP : IPPROTO_TCP;
    (void)on;

    if((fd = socket(af, type, proto)) == MG_INVALID_SOCKET) {
      MG_ERROR(("socket: %d", MG_SOCK_ERR(-1)));
#if defined(SO_EXCLUSIVEADDRUSE)
    } else if((rc = setsockopt(fd, SOL_SOCKET, SO_EXCLUSIVEADDRUSE, (char*)&on,
                               sizeof(on))) != 0) {
      // "Using SO_REUSEADDR and SO_EXCLUSIVEADDRUSE"
      MG_ERROR(("setsockopt(SO_EXCLUSIVEADDRUSE): %d %d", on, MG_SOCK_ERR(rc)));
#elif defined(SO_REUSEADDR) && (!defined(LWIP_SOCKET) || SO_REUSE)
    } else if((rc = setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (char*)&on,
                               sizeof(on))) != 0) {
      // 1. SO_REUSEADDR semantics on UNIX and Windows is different.  On
      // Windows, SO_REUSEADDR allows to bind a socket to a port without error
      // even if the port is already open by another program. This is not the
      // behavior SO_REUSEADDR was designed for, and leads to hard-to-track
      // failure scenarios.
      //
      // 2. For LWIP, SO_REUSEADDR should be explicitly enabled by defining
      // SO_REUSE = 1 in lwipopts.h, otherwise the code below will compile but
      // won't work! (setsockopt will return EINVAL)
      MG_ERROR(("setsockopt(SO_REUSEADDR): %d", MG_SOCK_ERR(rc)));
#endif
#if defined(IPV6_V6ONLY)
    } else if(c->loc.is_ip6 && (rc = setsockopt(fd, IPPROTO_IPV6, IPV6_V6ONLY,
                                                (char*)&on, sizeof(on))) != 0) {
      // See #2089. Allow to bind v4 and v6 sockets on the same port
      MG_ERROR(("setsockopt(IPV6_V6ONLY): %d", MG_SOCK_ERR(rc)));
#endif
    } else if((rc = bind(fd, &usa.sa, slen)) != 0) {
      MG_ERROR(("bind: %d", MG_SOCK_ERR(rc)));
    } else if((type == SOCK_STREAM &&
               (rc = listen(fd, MG_SOCK_LISTEN_BACKLOG_SIZE)) != 0)) {
      // NOTE(lsm): FreeRTOS uses backlog value as a connection limit
      // In case port was set to 0, get the real port number
      MG_ERROR(("listen: %d", MG_SOCK_ERR(rc)));
    } else {
      setlocaddr(fd, &c->loc);
      mg_set_non_blocking_mode(fd);
      c->fd = S2PTR(fd);
      MG_EPOLL_ADD(c);
      success = true;
    }
  }
  if(success == false && fd != MG_INVALID_SOCKET)
    closesocket(fd);
  return success;
}

long mg_io_recv(struct mg_connection* c, void* buf, size_t len) {
  long n = 0;
  if(c->is_udp) {
    union usa usa;
    socklen_t slen = tousa(&c->rem, &usa);
    n = recvfrom(FD(c), (char*)buf, len, 0, &usa.sa, &slen);
    if(n > 0)
      tomgaddr(&usa, &c->rem, slen != sizeof(usa.sin));
  } else {
    n = recv(FD(c), (char*)buf, len, MSG_NONBLOCKING);
  }
  if(MG_SOCK_PENDING(n))
    return MG_IO_WAIT;
  if(MG_SOCK_RESET(n))
    return MG_IO_RESET;
  if(n <= 0)
    return MG_IO_ERR;
  return n;
}

// NOTE(lsm): do only one iteration of reads, cause some systems
// (e.g. FreeRTOS stack) return 0 instead of -1/EWOULDBLOCK when no data
static void read_conn(struct mg_connection* c) {
  long n = -1;
  if(c->recv.len >= MG_MAX_RECV_SIZE) {
    mg_error(c, "max_recv_buf_size reached");
  } else if(c->recv.size <= c->recv.len &&
            !mg_iobuf_resize(&c->recv, c->recv.size + MG_IO_SIZE)) {
    mg_error(c, "oom");
  } else {
    char*  buf = (char*)&c->recv.buf[c->recv.len];
    size_t len = c->recv.size - c->recv.len;
    n = c->is_tls ? mg_tls_recv(c, buf, len) : mg_io_recv(c, buf, len);
    MG_DEBUG(("%lu %p snd %ld/%ld rcv %ld/%ld n=%ld err=%d", c->id, c->fd,
              (long)c->send.len, (long)c->send.size, (long)c->recv.len,
              (long)c->recv.size, n, MG_SOCK_ERR(n)));
    iolog(c, buf, n, true);
  }
}

static void write_conn(struct mg_connection* c) {
  char*  buf = (char*)c->send.buf;
  size_t len = c->send.len;
  long   n = c->is_tls ? mg_tls_send(c, buf, len) : mg_io_send(c, buf, len);
  MG_DEBUG(("%lu %p snd %ld/%ld rcv %ld/%ld n=%ld err=%d", c->id, c->fd,
            (long)c->send.len, (long)c->send.size, (long)c->recv.len,
            (long)c->recv.size, n, MG_SOCK_ERR(n)));
  iolog(c, buf, n, false);
}

static void close_conn(struct mg_connection* c) {
  if(FD(c) != MG_INVALID_SOCKET) {
#if MG_ENABLE_EPOLL
    epoll_ctl(c->mgr->epoll_fd, EPOLL_CTL_DEL, FD(c), NULL);
#endif
    closesocket(FD(c));
#if MG_ENABLE_FREERTOS_TCP
    FreeRTOS_FD_CLR(c->fd, c->mgr->ss, eSELECT_ALL);
#endif
  }
  mg_close_conn(c);
}

static void connect_conn(struct mg_connection* c) {
  union usa usa;
  socklen_t n = sizeof(usa);
  // Use getpeername() to test whether we have connected
  if(getpeername(FD(c), &usa.sa, &n) == 0) {
    c->is_connecting = 0;
    mg_call(c, MG_EV_CONNECT, NULL);
    MG_EPOLL_MOD(c, 0);
    if(c->is_tls_hs)
      mg_tls_handshake(c);
  } else {
    mg_error(c, "socket error");
  }
}

static void setsockopts(struct mg_connection* c) {
#if MG_ENABLE_FREERTOS_TCP || MG_ARCH == MG_ARCH_AZURERTOS ||                       \
    MG_ARCH == MG_ARCH_TIRTOS
  (void)c;
#else
  int on = 1;
#if !defined(SOL_TCP)
#define SOL_TCP IPPROTO_TCP
#endif
  if(setsockopt(FD(c), SOL_TCP, TCP_NODELAY, (char*)&on, sizeof(on)) != 0)
    (void)0;
  if(setsockopt(FD(c), SOL_SOCKET, SO_KEEPALIVE, (char*)&on, sizeof(on)) != 0)
    (void)0;
#endif
}

void mg_connect_resolved(struct mg_connection* c) {
  int type = c->is_udp ? SOCK_DGRAM : SOCK_STREAM;
  int rc, af = c->rem.is_ip6 ? AF_INET6 : AF_INET; // c->rem has resolved IP
  c->fd = S2PTR(socket(af, type, 0));              // Create outbound socket
  c->is_resolving = 0;                             // Clear resolving flag
  if(FD(c) == MG_INVALID_SOCKET) {
    mg_error(c, "socket(): %d", MG_SOCK_ERR(-1));
  } else if(c->is_udp) {
    MG_EPOLL_ADD(c);
#if MG_ARCH == MG_ARCH_TIRTOS
    union usa usa; // TI-RTOS NDK requires binding to receive on UDP sockets
    socklen_t slen = tousa(&c->loc, &usa);
    if((rc = bind(c->fd, &usa.sa, slen)) != 0)
      MG_ERROR(("bind: %d", MG_SOCK_ERR(rc)));
#endif
    mg_call(c, MG_EV_RESOLVE, NULL);
    mg_call(c, MG_EV_CONNECT, NULL);
  } else {
    union usa usa;
    socklen_t slen = tousa(&c->rem, &usa);
    mg_set_non_blocking_mode(FD(c));
    setsockopts(c);
    MG_EPOLL_ADD(c);
    mg_call(c, MG_EV_RESOLVE, NULL);
    rc = connect(FD(c), &usa.sa, slen); // Attempt to connect
    if(rc == 0) {                       // Success
      mg_call(c, MG_EV_CONNECT, NULL);  // Send MG_EV_CONNECT to the user
    } else if(MG_SOCK_PENDING(rc)) {    // Need to wait for TCP handshake
      MG_DEBUG(("%lu %p -> %M pend", c->id, c->fd, mg_print_ip_port, &c->rem));
      c->is_connecting = 1;
    } else {
      mg_error(c, "connect: %d", MG_SOCK_ERR(rc));
    }
  }
}

static MG_SOCKET_TYPE raccept(MG_SOCKET_TYPE sock, union usa* usa, socklen_t* len) {
  MG_SOCKET_TYPE fd = MG_INVALID_SOCKET;
  do {
    memset(usa, 0, sizeof(*usa));
    fd = accept(sock, &usa->sa, len);
  } while(MG_SOCK_INTR(fd));
  return fd;
}

static void accept_conn(struct mg_mgr* mgr, struct mg_connection* lsn) {
  struct mg_connection* c = NULL;
  union usa             usa;
  socklen_t             sa_len = sizeof(usa);
  MG_SOCKET_TYPE        fd = raccept(FD(lsn), &usa, &sa_len);
  if(fd == MG_INVALID_SOCKET) {
#if MG_ARCH == MG_ARCH_AZURERTOS
    // AzureRTOS, in non-block socket mode can mark listening socket readable
    // even it is not. See comment for 'select' func implementation in
    // nx_bsd.c That's not an error, just should try later
    if(errno != EAGAIN)
#endif
      MG_ERROR(("%lu accept failed, errno %d", lsn->id, MG_SOCK_ERR(-1)));
#if(MG_ARCH != MG_ARCH_WIN32) && !MG_ENABLE_FREERTOS_TCP &&                         \
    (MG_ARCH != MG_ARCH_TIRTOS) && !MG_ENABLE_POLL && !MG_ENABLE_EPOLL
  } else if((long)fd >= FD_SETSIZE) {
    MG_ERROR(("%ld > %ld", (long)fd, (long)FD_SETSIZE));
    closesocket(fd);
#endif
  } else if((c = mg_alloc_conn(mgr)) == NULL) {
    MG_ERROR(("%lu OOM", lsn->id));
    closesocket(fd);
  } else {
    tomgaddr(&usa, &c->rem, sa_len != sizeof(usa.sin));
    LIST_ADD_HEAD(struct mg_connection, &mgr->conns, c);
    c->fd = S2PTR(fd);
    MG_EPOLL_ADD(c);
    mg_set_non_blocking_mode(FD(c));
    setsockopts(c);
    c->is_accepted = 1;
    c->is_hexdumping = lsn->is_hexdumping;
    c->loc = lsn->loc;
    c->pfn = lsn->pfn;
    c->pfn_data = lsn->pfn_data;
    c->fn = lsn->fn;
    c->fn_data = lsn->fn_data;
    MG_DEBUG(("%lu %p accepted %M -> %M", c->id, c->fd, mg_print_ip_port, &c->rem,
              mg_print_ip_port, &c->loc));
    mg_call(c, MG_EV_OPEN, NULL);
    mg_call(c, MG_EV_ACCEPT, NULL);
    if(lsn->is_tls)
      mg_tls_init(c, mg_str(""));
  }
}

static bool can_read(const struct mg_connection* c) {
  return c->is_full == false;
}

static bool can_write(const struct mg_connection* c) {
  return c->is_connecting || (c->send.len > 0 && c->is_tls_hs == 0);
}

static bool skip_iotest(const struct mg_connection* c) {
  return (c->is_closing || c->is_resolving || FD(c) == MG_INVALID_SOCKET) ||
         (can_read(c) == false && can_write(c) == false);
}

static void mg_iotest(struct mg_mgr* mgr, int ms) {
#if MG_ENABLE_FREERTOS_TCP
  struct mg_connection* c;
  for(c = mgr->conns; c != NULL; c = c->next) {
    c->is_readable = c->is_writable = 0;
    if(skip_iotest(c))
      continue;
    if(can_read(c))
      FreeRTOS_FD_SET(c->fd, mgr->ss, eSELECT_READ | eSELECT_EXCEPT);
    if(can_write(c))
      FreeRTOS_FD_SET(c->fd, mgr->ss, eSELECT_WRITE);
  }
  FreeRTOS_select(mgr->ss, pdMS_TO_TICKS(ms));
  for(c = mgr->conns; c != NULL; c = c->next) {
    EventBits_t bits = FreeRTOS_FD_ISSET(c->fd, mgr->ss);
    c->is_readable = bits & (eSELECT_READ | eSELECT_EXCEPT) ? 1U : 0;
    c->is_writable = bits & eSELECT_WRITE ? 1U : 0;
    if(c->fd != MG_INVALID_SOCKET)
      FreeRTOS_FD_CLR(c->fd, mgr->ss, eSELECT_READ | eSELECT_EXCEPT | eSELECT_WRITE);
  }
#elif MG_ENABLE_EPOLL
  size_t max = 1;
  for(struct mg_connection* c = mgr->conns; c != NULL; c = c->next) {
    c->is_readable = c->is_writable = 0;
    if(mg_tls_pending(c) > 0)
      ms = 1, c->is_readable = 1;
    if(can_write(c))
      MG_EPOLL_MOD(c, 1);
    max++;
  }
  struct epoll_event* evs = (struct epoll_event*)alloca(max * sizeof(evs[0]));
  int                 n = epoll_wait(mgr->epoll_fd, evs, (int)max, ms);
  for(int i = 0; i < n; i++) {
    struct mg_connection* c = (struct mg_connection*)evs[i].data.ptr;
    if(evs[i].events & EPOLLERR) {
      mg_error(c, "socket error");
    } else if(c->is_readable == 0) {
      bool rd = evs[i].events & (EPOLLIN | EPOLLHUP);
      bool wr = evs[i].events & EPOLLOUT;
      c->is_readable = can_read(c) && rd ? 1U : 0;
      c->is_writable = can_write(c) && wr ? 1U : 0;
    }
  }
  (void)skip_iotest;
#elif MG_ENABLE_POLL
  nfds_t n = 0;
  for(struct mg_connection* c = mgr->conns; c != NULL; c = c->next)
    n++;
  struct pollfd* fds = (struct pollfd*)alloca(n * sizeof(fds[0]));
  memset(fds, 0, n * sizeof(fds[0]));
  n = 0;
  for(struct mg_connection* c = mgr->conns; c != NULL; c = c->next) {
    c->is_readable = c->is_writable = 0;
    if(skip_iotest(c)) {
      // Socket not valid, ignore
    } else if(mg_tls_pending(c) > 0) {
      ms = 1; // Don't wait if TLS is ready
    } else {
      fds[n].fd = FD(c);
      if(can_read(c))
        fds[n].events |= POLLIN;
      if(can_write(c))
        fds[n].events |= POLLOUT;
      n++;
    }
  }

  // MG_INFO(("poll n=%d ms=%d", (int) n, ms));
  if(poll(fds, n, ms) < 0) {
#if MG_ARCH == MG_ARCH_WIN32
    if(n == 0)
      Sleep(ms); // On Windows, poll fails if no sockets
#endif
    memset(fds, 0, n * sizeof(fds[0]));
  }
  n = 0;
  for(struct mg_connection* c = mgr->conns; c != NULL; c = c->next) {
    if(skip_iotest(c)) {
      // Socket not valid, ignore
    } else if(mg_tls_pending(c) > 0) {
      c->is_readable = 1;
    } else {
      if(fds[n].revents & POLLERR) {
        mg_error(c, "socket error");
      } else {
        c->is_readable = (unsigned)(fds[n].revents & (POLLIN | POLLHUP) ? 1 : 0);
        c->is_writable = (unsigned)(fds[n].revents & POLLOUT ? 1 : 0);
      }
      n++;
    }
  }
#else
  struct timeval        tv = {ms / 1000, (ms % 1000) * 1000}, tv_zero = {0, 0}, *tvp;
  struct mg_connection* c;
  fd_set                rset, wset, eset;
  MG_SOCKET_TYPE        maxfd = 0;
  int                   rc;

  FD_ZERO(&rset);
  FD_ZERO(&wset);
  FD_ZERO(&eset);
  tvp = ms < 0 ? NULL : &tv;
  for(c = mgr->conns; c != NULL; c = c->next) {
    c->is_readable = c->is_writable = 0;
    if(skip_iotest(c))
      continue;
    FD_SET(FD(c), &eset);
    if(can_read(c))
      FD_SET(FD(c), &rset);
    if(can_write(c))
      FD_SET(FD(c), &wset);
    if(mg_tls_pending(c) > 0)
      tvp = &tv_zero;
    if(FD(c) > maxfd)
      maxfd = FD(c);
  }

  if((rc = select((int)maxfd + 1, &rset, &wset, &eset, tvp)) < 0) {
#if MG_ARCH == MG_ARCH_WIN32
    if(maxfd == 0)
      Sleep(ms); // On Windows, select fails if no sockets
#else
    MG_ERROR(("select: %d %d", rc, MG_SOCK_ERR(rc)));
#endif
    FD_ZERO(&rset);
    FD_ZERO(&wset);
    FD_ZERO(&eset);
  }

  for(c = mgr->conns; c != NULL; c = c->next) {
    if(FD(c) != MG_INVALID_SOCKET && FD_ISSET(FD(c), &eset)) {
      mg_error(c, "socket error");
    } else {
      c->is_readable = FD(c) != MG_INVALID_SOCKET && FD_ISSET(FD(c), &rset);
      c->is_writable = FD(c) != MG_INVALID_SOCKET && FD_ISSET(FD(c), &wset);
      if(mg_tls_pending(c) > 0)
        c->is_readable = 1;
    }
  }
#endif
}

void mg_mgr_poll(struct mg_mgr* mgr, int ms) {
  struct mg_connection *c, *tmp;
  uint64_t              now;

  mg_iotest(mgr, ms);
  now = mg_millis();
  mg_timer_poll(&mgr->timers, now);

  for(c = mgr->conns; c != NULL; c = tmp) {
    bool is_resp = c->is_resp;
    tmp = c->next;
    mg_call(c, MG_EV_POLL, &now);
    if(is_resp && !c->is_resp) {
      long n = 0;
      mg_call(c, MG_EV_READ, &n);
    }
    MG_VERBOSE(("%lu %c%c %c%c%c%c%c", c->id, c->is_readable ? 'r' : '-',
                c->is_writable ? 'w' : '-', c->is_tls ? 'T' : 't',
                c->is_connecting ? 'C' : 'c', c->is_tls_hs ? 'H' : 'h',
                c->is_resolving ? 'R' : 'r', c->is_closing ? 'C' : 'c'));
    if(c->is_resolving || c->is_closing) {
      // Do nothing
    } else if(c->is_listening && c->is_udp == 0) {
      if(c->is_readable)
        accept_conn(mgr, c);
    } else if(c->is_connecting) {
      if(c->is_readable || c->is_writable)
        connect_conn(c);
    } else if(c->is_tls_hs) {
      if((c->is_readable || c->is_writable))
        mg_tls_handshake(c);
    } else {
      if(c->is_readable)
        read_conn(c);
      if(c->is_writable)
        write_conn(c);
    }

    if(c->is_draining && c->send.len == 0)
      c->is_closing = 1;
    if(c->is_closing)
      close_conn(c);
  }
}
#endif

#ifdef MG_ENABLE_LINES
#line 1 "src/ssi.c"
#endif

#ifndef MG_MAX_SSI_DEPTH
#define MG_MAX_SSI_DEPTH 5
#endif

#ifndef MG_SSI_BUFSIZ
#define MG_SSI_BUFSIZ 1024
#endif

void mg_http_serve_ssi(struct mg_connection* c, struct mg_http_message* hm,
                       const char* root, const char* fullpath) {
  char* code = readFile(fullpath);
  if(code) {
    struct BialetResponse r = bialetRun((char*)fullpath, code, hm);
    // When there is a length, the response is a file
    if(r.length > 0) {
      mg_printf(c, "HTTP/1.1 200 OK\r\n%sContent-Length: %d\r\n\r\n", r.header,
                (int)r.length);
      mg_send(c, r.body, r.length);
      c->is_resp = 0;
      // Free the response
      free(r.body);
      r.body = NULL;
    } else {
      mg_http_reply(c, r.status, r.header, r.body, MG_ESC("status"));
    }
  } else {
    MG_ERROR(("Error reading file: %s", fullpath));
  }
}

#ifdef MG_ENABLE_LINES
#line 1 "src/str.c"
#endif

struct mg_str mg_str_s(const char* s) {
  struct mg_str str = {s, s == NULL ? 0 : strlen(s)};
  return str;
}

struct mg_str mg_str_n(const char* s, size_t n) {
  struct mg_str str = {s, n};
  return str;
}

int mg_lower(const char* s) {
  int c = *s;
  if(c >= 'A' && c <= 'Z')
    c += 'a' - 'A';
  return c;
}

int mg_ncasecmp(const char* s1, const char* s2, size_t len) {
  int diff = 0;
  if(len > 0)
    do {
      diff = mg_lower(s1++) - mg_lower(s2++);
    } while(diff == 0 && s1[-1] != '\0' && --len > 0);
  return diff;
}

int mg_casecmp(const char* s1, const char* s2) {
  return mg_ncasecmp(s1, s2, (size_t)~0);
}

int mg_vcmp(const struct mg_str* s1, const char* s2) {
  size_t n2 = strlen(s2), n1 = s1->len;
  int    r = strncmp(s1->ptr, s2, (n1 < n2) ? n1 : n2);
  if(r == 0)
    return (int)(n1 - n2);
  return r;
}

int mg_vcasecmp(const struct mg_str* str1, const char* str2) {
  size_t n2 = strlen(str2), n1 = str1->len;
  int    r = mg_ncasecmp(str1->ptr, str2, (n1 < n2) ? n1 : n2);
  if(r == 0)
    return (int)(n1 - n2);
  return r;
}

struct mg_str mg_strdup(const struct mg_str s) {
  struct mg_str r = {NULL, 0};
  if(s.len > 0 && s.ptr != NULL) {
    char* sc = (char*)calloc(1, s.len + 1);
    if(sc != NULL) {
      memcpy(sc, s.ptr, s.len);
      sc[s.len] = '\0';
      r.ptr = sc;
      r.len = s.len;
    }
  }
  return r;
}

int mg_strcmp(const struct mg_str str1, const struct mg_str str2) {
  size_t i = 0;
  while(i < str1.len && i < str2.len) {
    int c1 = str1.ptr[i];
    int c2 = str2.ptr[i];
    if(c1 < c2)
      return -1;
    if(c1 > c2)
      return 1;
    i++;
  }
  if(i < str1.len)
    return 1;
  if(i < str2.len)
    return -1;
  return 0;
}

const char* mg_strstr(const struct mg_str haystack, const struct mg_str needle) {
  size_t i;
  if(needle.len > haystack.len)
    return NULL;
  if(needle.len == 0)
    return haystack.ptr;
  for(i = 0; i <= haystack.len - needle.len; i++) {
    if(memcmp(haystack.ptr + i, needle.ptr, needle.len) == 0) {
      return haystack.ptr + i;
    }
  }
  return NULL;
}

static bool is_space(int c) {
  return c == ' ' || c == '\r' || c == '\n' || c == '\t';
}

struct mg_str mg_strstrip(struct mg_str s) {
  while(s.len > 0 && is_space((int)*s.ptr))
    s.ptr++, s.len--;
  while(s.len > 0 && is_space((int)*(s.ptr + s.len - 1)))
    s.len--;
  return s;
}

bool mg_match(struct mg_str s, struct mg_str p, struct mg_str* caps) {
  size_t i = 0, j = 0, ni = 0, nj = 0;
  if(caps)
    caps->ptr = NULL, caps->len = 0;
  while(i < p.len || j < s.len) {
    if(i < p.len && j < s.len && (p.ptr[i] == '?' || s.ptr[j] == p.ptr[i])) {
      if(caps == NULL) {
      } else if(p.ptr[i] == '?') {
        caps->ptr = &s.ptr[j], caps->len = 1;    // Finalize `?` cap
        caps++, caps->ptr = NULL, caps->len = 0; // Init next cap
      } else if(caps->ptr != NULL && caps->len == 0) {
        caps->len = (size_t)(&s.ptr[j] - caps->ptr); // Finalize current cap
        caps++, caps->len = 0, caps->ptr = NULL;     // Init next cap
      }
      i++, j++;
    } else if(i < p.len && (p.ptr[i] == '*' || p.ptr[i] == '#')) {
      if(caps && !caps->ptr)
        caps->len = 0, caps->ptr = &s.ptr[j]; // Init cap
      ni = i++, nj = j + 1;
    } else if(nj > 0 && nj <= s.len && (p.ptr[ni] == '#' || s.ptr[j] != '/')) {
      i = ni, j = nj;
      if(caps && caps->ptr == NULL && caps->len == 0) {
        caps--, caps->len = 0; // Restart previous cap
      }
    } else {
      return false;
    }
  }
  if(caps && caps->ptr && caps->len == 0) {
    caps->len = (size_t)(&s.ptr[j] - caps->ptr);
  }
  return true;
}

bool mg_globmatch(const char* s1, size_t n1, const char* s2, size_t n2) {
  return mg_match(mg_str_n(s2, n2), mg_str_n(s1, n1), NULL);
}

static size_t mg_nce(const char* s, size_t n, size_t ofs, size_t* koff, size_t* klen,
                     size_t* voff, size_t* vlen, char delim) {
  size_t kvlen, kl;
  for(kvlen = 0; ofs + kvlen < n && s[ofs + kvlen] != delim;)
    kvlen++;
  for(kl = 0; kl < kvlen && s[ofs + kl] != '=';)
    kl++;
  if(koff != NULL)
    *koff = ofs;
  if(klen != NULL)
    *klen = kl;
  if(voff != NULL)
    *voff = kl < kvlen ? ofs + kl + 1 : 0;
  if(vlen != NULL)
    *vlen = kl < kvlen ? kvlen - kl - 1 : 0;
  ofs += kvlen + 1;
  return ofs > n ? n : ofs;
}

bool mg_split(struct mg_str* s, struct mg_str* k, struct mg_str* v, char sep) {
  size_t koff = 0, klen = 0, voff = 0, vlen = 0, off = 0;
  if(s->ptr == NULL || s->len == 0)
    return 0;
  off = mg_nce(s->ptr, s->len, 0, &koff, &klen, &voff, &vlen, sep);
  if(k != NULL)
    *k = mg_str_n(s->ptr + koff, klen);
  if(v != NULL)
    *v = mg_str_n(s->ptr + voff, vlen);
  *s = mg_str_n(s->ptr + off, s->len - off);
  return off > 0;
}

bool mg_commalist(struct mg_str* s, struct mg_str* k, struct mg_str* v) {
  return mg_split(s, k, v, ',');
}

char* mg_hex(const void* buf, size_t len, char* to) {
  const unsigned char* p = (const unsigned char*)buf;
  const char*          hex = "0123456789abcdef";
  size_t               i = 0;
  for(; len--; p++) {
    to[i++] = hex[p[0] >> 4];
    to[i++] = hex[p[0] & 0x0f];
  }
  to[i] = '\0';
  return to;
}

static unsigned char mg_unhex_nimble(unsigned char c) {
  return (c >= '0' && c <= '9')   ? (unsigned char)(c - '0')
         : (c >= 'A' && c <= 'F') ? (unsigned char)(c - '7')
                                  : (unsigned char)(c - 'W');
}

unsigned long mg_unhexn(const char* s, size_t len) {
  unsigned long i = 0, v = 0;
  for(i = 0; i < len; i++)
    v <<= 4, v |= mg_unhex_nimble(((uint8_t*)s)[i]);
  return v;
}

void mg_unhex(const char* buf, size_t len, unsigned char* to) {
  size_t i;
  for(i = 0; i < len; i += 2) {
    to[i >> 1] = (unsigned char)mg_unhexn(&buf[i], 2);
  }
}

bool mg_path_is_sane(const char* path) {
  const char* s = path;
  for(; s[0] != '\0'; s++) {
    if(s == path || s[0] == '/' || s[0] == '\\') { // Subdir?
      if(s[1] == '.' || s[1] == '_')
        return false; // Starts with . or _
    }
  }
  return true;
}

#ifdef MG_ENABLE_LINES
#line 1 "src/timer.c"
#endif

#define MG_TIMER_CALLED 4

void mg_timer_init(struct mg_timer** head, struct mg_timer* t, uint64_t ms,
                   unsigned flags, void (*fn)(void*), void* arg) {
  t->id = 0, t->period_ms = ms, t->expire = 0;
  t->flags = flags, t->fn = fn, t->arg = arg, t->next = *head;
  *head = t;
}

void mg_timer_free(struct mg_timer** head, struct mg_timer* t) {
  while(*head && *head != t)
    head = &(*head)->next;
  if(*head)
    *head = t->next;
}

// t: expiration time, prd: period, now: current time. Return true if expired
bool mg_timer_expired(uint64_t* t, uint64_t prd, uint64_t now) {
  if(now + prd < *t)
    *t = 0; // Time wrapped? Reset timer
  if(*t == 0)
    *t = now + prd; // Firt poll? Set expiration
  if(*t > now)
    return false;                               // Not expired yet, return
  *t = (now - *t) > prd ? now + prd : *t + prd; // Next expiration time
  return true;                                  // Expired, return true
}

void mg_timer_poll(struct mg_timer** head, uint64_t now_ms) {
  struct mg_timer *t, *tmp;
  for(t = *head; t != NULL; t = tmp) {
    bool once = t->expire == 0 && (t->flags & MG_TIMER_RUN_NOW) &&
                !(t->flags & MG_TIMER_CALLED); // Handle MG_TIMER_NOW only once
    bool expired = mg_timer_expired(&t->expire, t->period_ms, now_ms);
    tmp = t->next;
    if(!once && !expired)
      continue;
    if((t->flags & MG_TIMER_REPEAT) || !(t->flags & MG_TIMER_CALLED)) {
      t->fn(t->arg);
    }
    t->flags |= MG_TIMER_CALLED;
  }
}

#ifdef MG_ENABLE_LINES
#line 1 "src/tls_dummy.c"
#endif

#if MG_TLS == MG_TLS_NONE
void mg_tls_init(struct mg_connection* c, struct mg_str hostname) {
  (void)hostname;
  mg_error(c, "TLS is not enabled");
}
void mg_tls_handshake(struct mg_connection* c) {
  (void)c;
}
void mg_tls_free(struct mg_connection* c) {
  (void)c;
}
long mg_tls_recv(struct mg_connection* c, void* buf, size_t len) {
  return c == NULL || buf == NULL || len == 0 ? 0 : -1;
}
long mg_tls_send(struct mg_connection* c, const void* buf, size_t len) {
  return c == NULL || buf == NULL || len == 0 ? 0 : -1;
}
size_t mg_tls_pending(struct mg_connection* c) {
  (void)c;
  return 0;
}
void mg_tls_ctx_free(struct mg_mgr* mgr) {
  mgr->tls_ctx = NULL;
}
void mg_tls_ctx_init(struct mg_mgr* mgr, const struct mg_tls_opts* opts) {
  (void)opts, (void)mgr;
}
#endif

#ifdef MG_ENABLE_LINES
#line 1 "src/tls_openssl.c"
#endif

#if MG_TLS == MG_TLS_OPENSSL
static int mg_tls_err(struct mg_tls* tls, int res) {
  int err = SSL_get_error(tls->ssl, res);
  // We've just fetched the last error from the queue.
  // Now we need to clear the error queue. If we do not, then the following
  // can happen (actually reported):
  //  - A new connection is accept()-ed with cert error (e.g. self-signed cert)
  //  - Since all accept()-ed connections share listener's context,
  //  - *ALL* SSL accepted connection report read error on the next poll cycle.
  //    Thus a single errored connection can close all the rest, unrelated ones.
  // Clearing the error keeps the shared SSL_CTX in an OK state.

  if(err != 0)
    ERR_print_errors_fp(stderr);
  ERR_clear_error();
  if(err == SSL_ERROR_WANT_READ)
    return 0;
  if(err == SSL_ERROR_WANT_WRITE)
    return 0;
  return err;
}

static STACK_OF(X509_INFO) * load_ca_certs(const char* ca, int ca_len) {
  BIO* ca_bio = BIO_new_mem_buf(ca, ca_len);
  if(!ca_bio)
    return NULL;
  STACK_OF(X509_INFO)* certs = PEM_X509_INFO_read_bio(ca_bio, NULL, NULL, NULL);
  BIO_free(ca_bio);
  return certs;
}

static bool add_ca_certs(SSL_CTX* ctx, STACK_OF(X509_INFO) * certs) {
  X509_STORE* cert_store = SSL_CTX_get_cert_store(ctx);
  for(int i = 0; i < sk_X509_INFO_num(certs); i++) {
    X509_INFO* cert_info = sk_X509_INFO_value(certs, i);
    if(cert_info->x509 && !X509_STORE_add_cert(cert_store, cert_info->x509))
      return false;
  }
  return true;
}

static EVP_PKEY* load_key(const char* key, int key_len) {
  BIO* key_bio = BIO_new_mem_buf(key, key_len);
  if(!key_bio)
    return NULL;
  EVP_PKEY* priv_key = PEM_read_bio_PrivateKey(key_bio, NULL, 0, NULL);
  BIO_free(key_bio);
  return priv_key;
}

static X509* load_cert(const char* cert, int cert_len) {
  BIO* cert_bio = BIO_new_mem_buf(cert, cert_len);
  if(!cert_bio)
    return NULL;
  X509* x509 = PEM_read_bio_X509(cert_bio, NULL, 0, NULL);
  BIO_free(cert_bio);
  return x509;
}

void mg_tls_init(struct mg_connection* c, struct mg_str hostname) {
  struct mg_tls_ctx* ctx = (struct mg_tls_ctx*)c->mgr->tls_ctx;
  struct mg_tls*     tls = (struct mg_tls*)calloc(1, sizeof(*tls));

  if(ctx == NULL) {
    mg_error(c, "TLS context not initialized");
    goto fail;
  }

  if(tls == NULL) {
    mg_error(c, "TLS OOM");
    goto fail;
  }

  tls->ctx = c->is_client ? SSL_CTX_new(TLS_client_method())
                          : SSL_CTX_new(TLS_server_method());
  if((tls->ssl = SSL_new(tls->ctx)) == NULL) {
    mg_error(c, "SSL_new");
    goto fail;
  }

  SSL_set_min_proto_version(tls->ssl, TLS1_2_VERSION);

#ifdef MG_ENABLE_OPENSSL_NO_COMPRESSION
  SSL_set_options(tls->ssl, SSL_OP_NO_COMPRESSION);
#endif
#ifdef MG_ENABLE_OPENSSL_CIPHER_SERVER_PREFERENCE
  SSL_set_options(tls->ssl, SSL_OP_CIPHER_SERVER_PREFERENCE);
#endif

  if(c->is_client) {
    if(ctx->client_ca) {
      SSL_set_verify(tls->ssl, SSL_VERIFY_PEER | SSL_VERIFY_FAIL_IF_NO_PEER_CERT,
                     NULL);
      if(!add_ca_certs(tls->ctx, ctx->client_ca))
        goto fail;
    }
    if(ctx->client_cert && ctx->client_key) {
      if(SSL_use_certificate(tls->ssl, ctx->client_cert) != 1) {
        mg_error(c, "SSL_CTX_use_certificate");
        goto fail;
      }
      if(SSL_use_PrivateKey(tls->ssl, ctx->client_key) != 1) {
        mg_error(c, "SSL_CTX_use_PrivateKey");
        goto fail;
      }
    }
  } else {
    if(ctx->server_ca) {
      SSL_set_verify(tls->ssl, SSL_VERIFY_PEER | SSL_VERIFY_FAIL_IF_NO_PEER_CERT,
                     NULL);
      if(!add_ca_certs(tls->ctx, ctx->server_ca))
        goto fail;
    }
    if(ctx->server_cert && ctx->server_key) {
      if(SSL_use_certificate(tls->ssl, ctx->server_cert) != 1) {
        mg_error(c, "SSL_CTX_use_certificate");
        goto fail;
      }
      if(SSL_use_PrivateKey(tls->ssl, ctx->server_key) != 1) {
        mg_error(c, "SSL_CTX_use_PrivateKey");
        goto fail;
      }
    }
  }

  SSL_set_mode(tls->ssl, SSL_MODE_ACCEPT_MOVING_WRITE_BUFFER);
#if OPENSSL_VERSION_NUMBER > 0x10002000L
  SSL_set_ecdh_auto(tls->ssl, 1);
#endif

#if OPENSSL_VERSION_NUMBER >= 0x10100000L
  if(c->is_client && hostname.ptr && hostname.ptr[0] != '\0') {
    char* s = mg_mprintf("%.*s", (int)hostname.len, hostname.ptr);
    SSL_set1_host(tls->ssl, s);
    SSL_set_tlsext_host_name(tls->ssl, s);
    free(s);
  }
#endif

  c->tls = tls;
  c->is_tls = 1;
  c->is_tls_hs = 1;
  if(c->is_client && c->is_resolving == 0 && c->is_connecting == 0) {
    mg_tls_handshake(c);
  }
  MG_DEBUG(("%lu SSL %s OK", c->id, c->is_accepted ? "accept" : "client"));
  return;

fail:
  c->is_closing = 1;
  free(tls);
}

void mg_tls_handshake(struct mg_connection* c) {
  struct mg_tls* tls = (struct mg_tls*)c->tls;
  int            rc;
  SSL_set_fd(tls->ssl, (int)(size_t)c->fd);
  rc = c->is_client ? SSL_connect(tls->ssl) : SSL_accept(tls->ssl);
  if(rc == 1) {
    MG_DEBUG(("%lu success", c->id));
    c->is_tls_hs = 0;
    mg_call(c, MG_EV_TLS_HS, NULL);
  } else {
    int code = mg_tls_err(tls, rc);
    if(code != 0)
      mg_error(c, "tls hs: rc %d, err %d", rc, code);
  }
}

void mg_tls_free(struct mg_connection* c) {
  struct mg_tls* tls = (struct mg_tls*)c->tls;
  if(tls == NULL)
    return;
  SSL_free(tls->ssl);
  SSL_CTX_free(tls->ctx);
  free(tls);
  c->tls = NULL;
}

size_t mg_tls_pending(struct mg_connection* c) {
  struct mg_tls* tls = (struct mg_tls*)c->tls;
  return tls == NULL ? 0 : (size_t)SSL_pending(tls->ssl);
}

long mg_tls_recv(struct mg_connection* c, void* buf, size_t len) {
  struct mg_tls* tls = (struct mg_tls*)c->tls;
  int            n = SSL_read(tls->ssl, buf, (int)len);
  if(n < 0 && mg_tls_err(tls, n) == 0)
    return MG_IO_WAIT;
  if(n <= 0)
    return MG_IO_ERR;
  return n;
}

long mg_tls_send(struct mg_connection* c, const void* buf, size_t len) {
  struct mg_tls* tls = (struct mg_tls*)c->tls;
  int            n = SSL_write(tls->ssl, buf, (int)len);
  if(n < 0 && mg_tls_err(tls, n) == 0)
    return MG_IO_WAIT;
  if(n <= 0)
    return MG_IO_ERR;
  return n;
}

void mg_tls_ctx_free(struct mg_mgr* mgr) {
  struct mg_tls_ctx* ctx = (struct mg_tls_ctx*)mgr->tls_ctx;
  if(ctx) {
    if(ctx->server_cert)
      X509_free(ctx->server_cert);
    if(ctx->server_key)
      EVP_PKEY_free(ctx->server_key);
    if(ctx->server_ca)
      sk_X509_INFO_pop_free(ctx->server_ca, X509_INFO_free);
    if(ctx->client_cert)
      X509_free(ctx->client_cert);
    if(ctx->client_key)
      EVP_PKEY_free(ctx->client_key);
    if(ctx->client_ca)
      sk_X509_INFO_pop_free(ctx->client_ca, X509_INFO_free);
    free(ctx);
    mgr->tls_ctx = NULL;
  }
}

void mg_tls_ctx_init(struct mg_mgr* mgr, const struct mg_tls_opts* opts) {
  static unsigned char s_initialised = 0;
  if(!s_initialised) {
    SSL_library_init();
    s_initialised++;
  }

  struct mg_tls_ctx* ctx = (struct mg_tls_ctx*)calloc(1, sizeof(*ctx));
  if(ctx == NULL)
    return;

  if(opts->server_cert.ptr && opts->server_cert.ptr[0] != '\0') {
    struct mg_str key = opts->server_key;
    if(!key.ptr)
      key = opts->server_cert;
    if(!(ctx->server_cert =
             load_cert(opts->server_cert.ptr, (int)opts->server_cert.len)))
      goto fail;
    if(!(ctx->server_key = load_key(key.ptr, (int)key.len)))
      goto fail;
  }

  if(opts->server_ca.ptr && opts->server_ca.ptr[0] != '\0') {
    if(!(ctx->server_ca =
             load_ca_certs(opts->server_ca.ptr, (int)opts->server_ca.len)))
      goto fail;
  }

  if(opts->client_cert.ptr && opts->client_cert.ptr[0] != '\0') {
    struct mg_str key = opts->client_key;
    if(!key.ptr)
      key = opts->client_cert;
    if(!(ctx->client_cert =
             load_cert(opts->client_cert.ptr, (int)opts->client_cert.len)))
      goto fail;
    if(!(ctx->client_key = load_key(key.ptr, (int)key.len)))
      goto fail;
  }

  if(opts->client_ca.ptr && opts->client_ca.ptr[0] != '\0') {
    if(!(ctx->client_ca =
             load_ca_certs(opts->client_ca.ptr, (int)opts->client_ca.len)))
      goto fail;
  }

  mgr->tls_ctx = ctx;
  return;
fail:
  MG_ERROR(("TLS ctx init error"));
  mg_tls_ctx_free(mgr);
}

#endif

#ifdef MG_ENABLE_LINES
#line 1 "src/url.c"
#endif

struct url {
  size_t key, user, pass, host, port, uri, end;
};

int mg_url_is_ssl(const char* url) {
  return strncmp(url, "wss:", 4) == 0 || strncmp(url, "https:", 6) == 0 ||
         strncmp(url, "mqtts:", 6) == 0 || strncmp(url, "ssl:", 4) == 0 ||
         strncmp(url, "tls:", 4) == 0;
}

static struct url urlparse(const char* url) {
  size_t     i;
  struct url u;
  memset(&u, 0, sizeof(u));
  for(i = 0; url[i] != '\0'; i++) {
    if(url[i] == '/' && i > 0 && u.host == 0 && url[i - 1] == '/') {
      u.host = i + 1;
      u.port = 0;
    } else if(url[i] == ']') {
      u.port = 0; // IPv6 URLs, like http://[::1]/bar
    } else if(url[i] == ':' && u.port == 0 && u.uri == 0) {
      u.port = i + 1;
    } else if(url[i] == '@' && u.user == 0 && u.pass == 0 && u.uri == 0) {
      u.user = u.host;
      u.pass = u.port;
      u.host = i + 1;
      u.port = 0;
    } else if(url[i] == '/' && u.host && u.uri == 0) {
      u.uri = i;
    }
  }
  u.end = i;
#if 0
  printf("[%s] %d %d %d %d %d\n", url, u.user, u.pass, u.host, u.port, u.uri);
#endif
  return u;
}

struct mg_str mg_url_host(const char* url) {
  struct url u = urlparse(url);
  size_t n = u.port ? u.port - u.host - 1 : u.uri ? u.uri - u.host : u.end - u.host;
  struct mg_str s = mg_str_n(url + u.host, n);
  return s;
}

const char* mg_url_uri(const char* url) {
  struct url u = urlparse(url);
  return u.uri ? url + u.uri : "/";
}

unsigned short mg_url_port(const char* url) {
  struct url     u = urlparse(url);
  unsigned short port = 0;
  if(strncmp(url, "http:", 5) == 0 || strncmp(url, "ws:", 3) == 0)
    port = 80;
  if(strncmp(url, "wss:", 4) == 0 || strncmp(url, "https:", 6) == 0)
    port = 443;
  if(strncmp(url, "mqtt:", 5) == 0)
    port = 1883;
  if(strncmp(url, "mqtts:", 6) == 0)
    port = 8883;
  if(u.port)
    port = (unsigned short)atoi(url + u.port);
  return port;
}

struct mg_str mg_url_user(const char* url) {
  struct url    u = urlparse(url);
  struct mg_str s = mg_str("");
  if(u.user && (u.pass || u.host)) {
    size_t n = u.pass ? u.pass - u.user - 1 : u.host - u.user - 1;
    s = mg_str_n(url + u.user, n);
  }
  return s;
}

struct mg_str mg_url_pass(const char* url) {
  struct url    u = urlparse(url);
  struct mg_str s = mg_str_n("", 0UL);
  if(u.pass && u.host) {
    size_t n = u.host - u.pass - 1;
    s = mg_str_n(url + u.pass, n);
  }
  return s;
}

#ifdef MG_ENABLE_LINES
#line 1 "src/util.c"
#endif

#if MG_ENABLE_CUSTOM_RANDOM
#else
void mg_random(void* buf, size_t len) {
  bool           done = false;
  unsigned char* p = (unsigned char*)buf;
#if MG_ARCH == MG_ARCH_ESP32
  while(len--)
    *p++ = (unsigned char)(esp_random() & 255);
  done = true;
#elif MG_ARCH == MG_ARCH_WIN32
#elif MG_ARCH == MG_ARCH_UNIX
  FILE* fp = fopen("/dev/urandom", "rb");
  if(fp != NULL) {
    if(fread(buf, 1, len, fp) == len)
      done = true;
    fclose(fp);
  }
#endif
  // If everything above did not work, fallback to a pseudo random generator
  while(!done && len--)
    *p++ = (unsigned char)(rand() & 255);
}
#endif

char* mg_random_str(char* buf, size_t len) {
  size_t i;
  mg_random(buf, len);
  for(i = 0; i < len; i++) {
    uint8_t c = ((uint8_t*)buf)[i] % 62U;
    buf[i] = i == len - 1 ? (char)'\0'           // 0-terminate last byte
             : c < 26     ? (char)('a' + c)      // lowercase
             : c < 52     ? (char)('A' + c - 26) // uppercase
                          : (char)('0' + c - 52);    // numeric
  }
  return buf;
}

uint32_t mg_ntohl(uint32_t net) {
  uint8_t data[4] = {0, 0, 0, 0};
  memcpy(&data, &net, sizeof(data));
  return (((uint32_t)data[3]) << 0) | (((uint32_t)data[2]) << 8) |
         (((uint32_t)data[1]) << 16) | (((uint32_t)data[0]) << 24);
}

uint16_t mg_ntohs(uint16_t net) {
  uint8_t data[2] = {0, 0};
  memcpy(&data, &net, sizeof(data));
  return (uint16_t)((uint16_t)data[1] | (((uint16_t)data[0]) << 8));
}

uint32_t mg_crc32(uint32_t crc, const char* buf, size_t len) {
  static const uint32_t crclut[16] = {
      // table for polynomial 0xEDB88320 (reflected)
      0x00000000, 0x1DB71064, 0x3B6E20C8, 0x26D930AC, 0x76DC4190, 0x6B6B51F4,
      0x4DB26158, 0x5005713C, 0xEDB88320, 0xF00F9344, 0xD6D6A3E8, 0xCB61B38C,
      0x9B64C2B0, 0x86D3D2D4, 0xA00AE278, 0xBDBDF21C};
  crc = ~crc;
  while(len--) {
    uint8_t byte = *(uint8_t*)buf++;
    crc = crclut[(crc ^ byte) & 0x0F] ^ (crc >> 4);
    crc = crclut[(crc ^ (byte >> 4)) & 0x0F] ^ (crc >> 4);
  }
  return ~crc;
}

static int isbyte(int n) {
  return n >= 0 && n <= 255;
}

static int parse_net(const char* spec, uint32_t* net, uint32_t* mask) {
  int n, a, b, c, d, slash = 32, len = 0;
  if((sscanf(spec, "%d.%d.%d.%d/%d%n", &a, &b, &c, &d, &slash, &n) == 5 ||
      sscanf(spec, "%d.%d.%d.%d%n", &a, &b, &c, &d, &n) == 4) &&
     isbyte(a) && isbyte(b) && isbyte(c) && isbyte(d) && slash >= 0 && slash < 33) {
    len = n;
    *net =
        ((uint32_t)a << 24) | ((uint32_t)b << 16) | ((uint32_t)c << 8) | (uint32_t)d;
    *mask = slash ? (uint32_t)(0xffffffffU << (32 - slash)) : (uint32_t)0;
  }
  return len;
}

int mg_check_ip_acl(struct mg_str acl, struct mg_addr* remote_ip) {
  struct mg_str k, v;
  int      allowed = acl.len == 0 ? '+' : '-'; // If any ACL is set, deny by default
  uint32_t remote_ip4;
  if(remote_ip->is_ip6) {
    return -1; // TODO(): handle IPv6 ACL and addresses
  } else {     // IPv4
    memcpy((void*)&remote_ip4, remote_ip->ip, sizeof(remote_ip4));
    while(mg_commalist(&acl, &k, &v)) {
      uint32_t net, mask;
      if(k.ptr[0] != '+' && k.ptr[0] != '-')
        return -1;
      if(parse_net(&k.ptr[1], &net, &mask) == 0)
        return -2;
      if((mg_ntohl(remote_ip4) & mask) == net)
        allowed = k.ptr[0];
    }
  }
  return allowed == '+';
}

#if MG_ENABLE_CUSTOM_MILLIS
#else
uint64_t mg_millis(void) {
#if MG_ARCH == MG_ARCH_WIN32
  return GetTickCount();
#elif MG_ARCH == MG_ARCH_RP2040
  return time_us_64() / 1000;
#elif MG_ARCH == MG_ARCH_ESP32
  return esp_timer_get_time() / 1000;
#elif MG_ARCH == MG_ARCH_ESP8266 || MG_ARCH == MG_ARCH_FREERTOS
  return xTaskGetTickCount() * portTICK_PERIOD_MS;
#elif MG_ARCH == MG_ARCH_AZURERTOS
  return tx_time_get() * (1000 /* MS per SEC */ / TX_TIMER_TICKS_PER_SECOND);
#elif MG_ARCH == MG_ARCH_TIRTOS
  return (uint64_t)Clock_getTicks();
#elif MG_ARCH == MG_ARCH_ZEPHYR
  return (uint64_t)k_uptime_get();
#elif MG_ARCH == MG_ARCH_CMSIS_RTOS1
  return (uint64_t)rt_time_get();
#elif MG_ARCH == MG_ARCH_CMSIS_RTOS2
  return (uint64_t)((osKernelGetTickCount() * 1000) / osKernelGetTickFreq());
#elif MG_ARCH == MG_ARCH_RTTHREAD
  return (uint64_t)((rt_tick_get() * 1000) / RT_TICK_PER_SECOND);
#elif MG_ARCH == MG_ARCH_UNIX && defined(__APPLE__)
  // Apple CLOCK_MONOTONIC_RAW is equivalent to CLOCK_BOOTTIME on linux
  // Apple CLOCK_UPTIME_RAW is equivalent to CLOCK_MONOTONIC_RAW on linux
  return clock_gettime_nsec_np(CLOCK_UPTIME_RAW) / 1000000;
#elif MG_ARCH == MG_ARCH_UNIX
  struct timespec ts = {0, 0};
  // See #1615 - prefer monotonic clock
#if defined(CLOCK_MONOTONIC_RAW)
  // Raw hardware-based time that is not subject to NTP adjustment
  clock_gettime(CLOCK_MONOTONIC_RAW, &ts);
#elif defined(CLOCK_MONOTONIC)
  // Affected by the incremental adjustments performed by adjtime and NTP
  clock_gettime(CLOCK_MONOTONIC, &ts);
#else
  // Affected by discontinuous jumps in the system time and by the incremental
  // adjustments performed by adjtime and NTP
  clock_gettime(CLOCK_REALTIME, &ts);
#endif
  return ((uint64_t)ts.tv_sec * 1000 + (uint64_t)ts.tv_nsec / 1000000);
#elif defined(ARDUINO)
  return (uint64_t)millis();
#else
  return (uint64_t)(time(NULL) * 1000);
#endif
}
#endif
