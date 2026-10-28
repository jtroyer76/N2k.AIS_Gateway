// Body of GET /api/log?since=N (SPEC 6.2). Platform-free so it runs in the host tests.
//
//   {"lines":[[seq,ms,"I","text"],...],"next":S,"more":B,"lost":L,"reset":R}
//
// next:  pass as `since` on the next request.
// more:  more lines are waiting (line limit or buffer full); ask again straight away.
// lost:  lines after `since` that the ring had already dropped.
// reset: `since` was ahead of the device's latest line, so it restarted; lines start from the
//        oldest kept.
#pragma once

#include <stddef.h>
#include <stdint.h>

#include "JsonOut.h"
#include "LogRing.h"

// Longest possible tail: ],"next":<u32>,"more":false,"lost":<u32>,"reset":false} plus the NUL.
static const size_t kLogJsonTail = 66;

template <size_t N, size_t Len>
void writeLogJson(JsonOut &j, const LogRing<N, Len> &ring, uint32_t since, size_t maxLines) {
  bool reset = since > ring.latest();
  if (reset) since = 0;
  uint32_t start = since + 1 < ring.oldest() ? ring.oldest() : since + 1;
  uint32_t lost = start - 1 - since;
  uint32_t next = start - 1;
  bool more = false;

  j.raw("{\"lines\":[");
  j.setReserve(kLogJsonTail);
  size_t written = 0;
  for (uint32_t seq = start; seq <= ring.latest(); seq++) {
    if (written == maxLines) {
      more = true;
      break;
    }
    const typename LogRing<N, Len>::Line *l = ring.find(seq);
    size_t m = j.mark();
    if (written) j.raw(",");
    j.raw("[");
    j.num(l->seq);
    j.raw(",");
    j.num(l->ms);
    char level[2] = {l->level, 0};
    j.raw(",");
    j.str(level);
    j.raw(",");
    j.str(l->text);
    j.raw("]");
    if (!j.ok()) {
      j.rewind(m);
      more = true;
      break;
    }
    next = seq;
    written++;
  }
  j.setReserve(0);
  j.raw("],\"next\":");
  j.num(next);
  j.raw(",\"more\":");
  j.boolean(more);
  j.raw(",\"lost\":");
  j.num(lost);
  j.raw(",\"reset\":");
  j.boolean(reset);
  j.raw("}");
}
