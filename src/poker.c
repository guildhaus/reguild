/* poker.c -- the card poker game of the tavern against two NPCs (0x4a7140..0x4a8760): the game record 0xf83654,
   the deck 0xf83660 and the two opponents 0xf837e0. The am unit ends before it (its last .data string is
   "%sgebaeude/*%s_%c.ogr" 0x600478); this unit's .data block is the hand names 0x600490, then its strings (the
   NPC models 0x600d90..0x600e10, 0x4a7140's); the next object (aip) starts with ai_LeisureActions. Names are ours.
   unit of rea 8cef7d5b (src/game/unk_4a7140.c); boundaries in decl/text.dcl */
#include "txt_views.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "poker.h"

#define PARTNER_FLAGS_SEX0                                                     \
  0xf00502 /* the partner-chain flags 0x4a8ad0 asks for a character with sex 0 (reasoned: the other sex; the low byte 2 against 1) */
#define PARTNER_FLAGS_SEX1 0xf00501 /* ditto for sex != 0 */
#define PARTNER_FLAGS_ANY                                                      \
  0xf00500 /* the same flags with the low byte 0, either sex (0x4a8f20; reasoned) */
#define PARTNER_FLAGS_ANY_OFFICE                                               \
  0xf00d00 /* PARTNER_FLAGS_ANY | 0x800: either sex and an office holder (0x5c33c0: a candidate whose +0x166 officeKind / +0x169 secondOffice is set), 0x4a95d0 */
#define SUITOR_FLAGS_SEX0                                                      \
  0xc0140a /* the candidate flags 0x4a91a0 passes ai_FindCandidates for a character with sex 0 (reasoned: the other sex, as PARTNER_FLAGS_SEX0) */
#define SUITOR_FLAGS_SEX1 0xc01409 /* ditto for sex != 0 */
#define AIPER_F1CC_DISTRUST                                                    \
  0x400000 /* the flags1cc bit that costs a candidate 0.2 trust in 0x4aad30 (reasoned name: its only reader) */
/* full rebuild: the symbols whose original addresses the code above spells literally */
#include "cm_types.h"
#include "ahm.h"
#include "amt.h"
#include "anm.h"
#include "cm.h"
#include "f3gm.h"
#include "gv.h"
#include "he.h"
#include "tr.h"
#include "lc.h"
/* clang-format off */
unsigned short util_Random(unsigned short n);  /* 0x42ec00, @original: tr.c defines it returning int; this unit's callers read the 16-bit result (mov si,ax / movzx): the original's units saw a 16-bit declaration */
/* clang-format on */

/* 0x4a7140, starts a game of character p in g: the stake from p's money, the deck, two random opponents */

int ai_PokerStart(AICHAR *p, G10GAME *g) {
  double d;
  int i, j, k;
  G10CARD *c;
  AICHAR *n;

  ai_PokerTable = 0;
  if (p == 0 || p->kind != 6 || g == 0)
    return 0;
  memset(ai_PokerOpponents, 0, sizeof(ai_PokerOpponents));
  ai_PokerTable = g;
  memset(g, 0, sizeof(G10GAME));
  ai_PokerTable->seat[0].who = p;
  k = gm_ConvertMoney(gm_SumListAmounts(p->inventory, ai_OwnPlayer),
                      ai_OwnPlayer);
  d = k;
  if (10000.0 > d)
    d = 10000.0;
  ai_PokerTable->money =
      gm_PlayerFactor((int)(d / (log10(d) + 1.0) * 0.1f), ai_OwnPlayer);
  ai_PokerTable->stake = ai_PokerTable->money / (util_Random(3) + 7);
  ai_PokerTable->stake2 = ai_PokerTable->stake;
  ai_PokerTable->winner = 4;
  for (k = 0; k < 6; k++) {
    for (j = 0; j < 4; j++) {
      ai_PokerDeck[k * 4 + j].suit = k;
      ai_PokerDeck[k * 4 + j].rank = j;
    }
  }
  for (i = 0; i < 2; i++) {
    n = &ai_PokerOpponents[i];
    if (i != 0)
      n->sex = ai_PokerOpponents[0].sex == 0;
    else
      n->sex = (unsigned char)util_Random(2);
    n->level = (unsigned char)util_Random(4) + 1;
    strncpy(n->family, txt_Ptrs[TXT_FAMILY_NAMES + util_Random(0x95)], 15);
    if (n->sex == 0) {
      strncpy(n->first, txt_Ptrs[TXT_MALE_FIRST_NAMES + util_Random(0xbf)], 15);
      switch (util_Random(4)) {
      case 0:
        ai_PokerTable->seat[i + 1].model =
            gm_HeadIconNumber("buerger_MANN") + 0x639;
        break;
      case 1:
        ai_PokerTable->seat[i + 1].model =
            gm_HeadIconNumber("Handwerker_MANN") + 0x639;
        break;
      case 2:
        ai_PokerTable->seat[i + 1].model =
            gm_HeadIconNumber("buerger3_DICKER") + 0x639;
        break;
      case 3:
        ai_PokerTable->seat[i + 1].model =
            gm_HeadIconNumber("schneider1_MANN") + 0x639;
        break;
      }
    } else {
      strncpy(n->first, txt_Ptrs[TXT_FEMALE_FIRST_NAMES + util_Random(0x70)],
              15);
      switch (util_Random(4)) {
      case 0:
        ai_PokerTable->seat[i + 1].model =
            gm_HeadIconNumber("buergerin_frau") + 0x639;
        break;
      case 1:
        ai_PokerTable->seat[i + 1].model =
            gm_HeadIconNumber("buergerin2_frau") + 0x639;
        break;
      case 2:
        ai_PokerTable->seat[i + 1].model =
            gm_HeadIconNumber("minerin_frau") + 0x639;
        break;
      case 3:
        ai_PokerTable->seat[i + 1].model =
            gm_HeadIconNumber("arbeiterin_frau") + 0x639;
        break;
      }
    }
    n->kind = 10;
    ai_PokerTable->seat[i + 1].who = n;
  }
  return ai_PokerDealRound(g);
}
/* 0x4a7430, deals a new round: all cards back, the three seats cleared */

int ai_PokerDealRound(G10GAME *g) {
  G10SEAT *s;
  int i;

  if (g == 0)
    return 0;
  g->left = 0x18;
  g->dealt = 0;
  g->pot = 0;
  g->phase = 0;
  g->turn = util_Random(3);
  g->winHand = 0;
  for (i = 0; i < 24; i++) {
    ai_PokerDeck[i].owner = 3;
    ai_PokerDeck[i].shown = 0;
  }
  s = g->seat;
  i = 3;
  do {
    memset(s->card, 0xff, sizeof(s->card));
    s->bet = 0;
    s->strategy = 0;
    s->swapped = -1;
    s->bold = util_Random(5) == 1;
    s->strategy = 0;
    s->foldedAt = -1;
    s++;
  } while (--i);
  return 1;
}
/* 0x4a74e0, one step of the game: returns the button mask ai_PokerGame shows (1 first button, 8 second, 0x10 fourth, 0x20 bet slider) or 0 */

unsigned short ai_PokerStep(void) {
  switch (ai_PokerTable->phase) {
  case 0:
    ai_PokerTable->phase = 1;
    return ai_PokerTable->turn != 0 ? 0x10 : 0x21;
  case 1:
    ai_PokerAnte();
    ai_PokerTable->phase++;
    ai_PokerDealThree();
    ai_PokerPickStrategies();
    ai_PokerNpcSetStake();
    return 0;
  case 2:
    ai_PokerTable->phase = 3;
    return (ai_PokerTable->turn != 0 ? 0 : 0x20) | 9;
  case 3:
    ai_PokerPickStrategies();
    ai_PokerNpcFold();
    ai_PokerTable->phase++;
    ai_PokerDealFourth();
    ai_PokerPayStakes();
    return 0;
  case 4:
    ai_PokerTable->phase = 5;
    ai_PokerPickStrategies();
    ai_PokerNpcSetStake();
    return (ai_PokerTable->turn != 0 ? 0 : 0x20) | 9;
  case 5:
    ai_PokerNpcFold();
    ai_PokerPayStakes();
    ai_PokerTable->phase++;
    return 0x1c;
  case 6:
    ai_PokerNpcExchange();
    ai_PokerTable->phase++;
    return 0x10;
  case 7:
    ai_PokerDealFifth();
    ai_PokerTable->phase++;
    break;
  case 8:
    ai_PokerPickStrategies();
    ai_PokerTable->phase++;
    return (ai_PokerTable->turn != 0 ? 0 : 0x20) | 9;
  case 9:
    ai_PokerNpcFold();
    ai_PokerPayStakes();
    ai_PokerTable->phase++;
    return 0;
  case 10:
    ai_PokerTable->phase = 11;
    ai_PokerEndRound();
    return 0x10;
  }
  return 0;
}
/* 0x4a7650, finds a pair: the first suit (the deck's cards 4 by 4, suit 0..5) other than skip of which seat 2 holds
   two or more cards (only open ones if shown): the suit into out, the count as the result, 0 if none. The seat loop
   runs 1..2 and keeps only seat 2's counts; hand is not read */

int ai_PokerFindPair(int *hand, int shown, int skip, int *out) {
  int cnt[6];
  int s, g, i, k, n, best;

  for (s = 1; s < 3; s++) {
    best = 0;
    g = 0;
    for (k = 0; k < 24; k += 4) {
      n = 0;
      for (i = 0; i < 4; i++) {
        if (ai_PokerDeck[k + i].owner == s &&
            (shown == 0 || ai_PokerDeck[k + i].shown != 0))
          n++;
      }
      if (n > best)
        best = n;
      cnt[g++] = n;
    }
  }
  for (g = 0; g < 6; g++) {
    if (g != skip && cnt[g] > 1) {
      *out = g;
      return cnt[g];
    }
  }
  return 0;
}
/* 0x4a76d0, the highest card of a hand (group, then rank): of cards 0..4, or 2..4 if high */

int ai_PokerHighCard(int *hand, int high) {
  int i, j, best, g, r;

  i = high != 0 ? 2 : 0;
  best = hand[i];
  if (best < 0)
    return -1;
  j = i + 1;
  if (hand[j] < 0)
    return best;
  g = ai_PokerDeck[best].suit;
  r = ai_PokerDeck[best].rank;
  for (; j < 5; j++) {
    if (hand[j] < 0)
      break;
    if (ai_PokerDeck[hand[j]].suit > g) {
      best = hand[j];
      g = ai_PokerDeck[best].suit;
      r = ai_PokerDeck[best].rank;
    } else if (ai_PokerDeck[hand[j]].suit == g &&
               ai_PokerDeck[hand[j]].rank > r) {
      best = hand[j];
      r = ai_PokerDeck[best].rank;
    }
  }
  return best;
}
/* 0x4a7760, deals a card from the deck to owner (the first one when 10 or fewer are left, else a random one) */

int ai_PokerDrawCard(int owner) {
  int i;

  if (ai_PokerTable->left == 0)
    return -1;
  if (ai_PokerTable->left > 10) {
    do
      i = util_Random(24);
    while (ai_PokerDeck[i].owner != 3);
  } else {
    for (i = 0; i < 24; i++)
      if (ai_PokerDeck[i].owner == 3)
        break;
  }
  ai_PokerDeck[i].owner = owner;
  ai_PokerTable->left--;
  return i;
}
/* 0x4a77e0, the NPCs exchange a card by their strategy (seat->strategy): keep pairs (1), the most common rank (2) */

void ai_PokerNpcExchange(void) {
  int cnt[6];
  int s, g, j, i, n, k;
  int pairs, maxn, first, second, maxr, bestr;
  int c0;
  G10SEAT *st;

  first = 0;
  second = 0;
  bestr = 0;
  k = -1;
  for (s = 1; s < 3; s++) {
    maxn = 0;
    pairs = 0;
    maxr = 0;
    g = 0;
    for (c0 = 0; c0 < 24; c0 += 4) {
      n = 0;
      for (i = 0; i < 4; i++) {
        if (ai_PokerDeck[c0 + i].owner == s)
          n++;
      }
      if (n > 1)
        pairs++;
      if (n > maxn) {
        second = first;
        maxn = n;
        first = g;
      }
      cnt[g] = n;
      g++;
    }
    for (j = 0; j < 4; j++) {
      n = 0;
      for (i = 0; i < 24; i += 4) {
        if (ai_PokerDeck[i + j].owner == s)
          n++;
      }
      if (n > maxr) {
        maxr = n;
        bestr = j;
      }
      cnt[j] = n;
    }
    st = &ai_PokerTable->seat[s];
    switch (st->strategy) {
    case 0:
      k = util_Random(3);
      break;
    case 1:
      if (pairs == 1) {
        if (ai_PokerDeck[st->card[0]].suit != first)
          k = 0;
        else if (ai_PokerDeck[st->card[1]].suit != first)
          k = 1;
      } else if (pairs == 2) {
        if (ai_PokerDeck[st->card[0]].suit != first &&
            ai_PokerDeck[st->card[0]].suit != second)
          k = 0;
        else if (ai_PokerDeck[st->card[1]].suit != first &&
                 ai_PokerDeck[st->card[1]].suit != second)
          k = 1;
      }
      break;
    case 2:
      if (ai_PokerDeck[st->card[0]].rank != bestr)
        k = 0;
      else if (ai_PokerDeck[st->card[1]].rank != bestr)
        k = 1;
      break;
    case 3:
      if (pairs == 1) {
        if (ai_PokerDeck[st->card[0]].suit == first)
          k = 0;
        else if (ai_PokerDeck[st->card[1]].suit != first)
          k = 1;
      }
      break;
    }
    if (k >= 0)
      ai_PokerSwapCard(st, k);
  }
}
/* 0x4a79d0, seat s puts card k (0 or 1) away and draws a new one in its place */

void ai_PokerSwapCard(G10SEAT *s, int k) {
  int o, n;

  if (k < 2) {
    o = ai_PokerDeck[s->card[k]].owner;
    ai_PokerDeck[s->card[k]].owner = 4;
    s->swapped = s->card[k];
    n = ai_PokerDrawCard(o);
    s->card[k] = n;
    ai_PokerDeck[n].shown = 0;
  }
}
/* 0x4a7a20, every seat still in (foldedAt < 0) pays the stake into the pot; the player's from p's money */

void ai_PokerPayStakes(void) {
  int i;

  for (i = 0; i < 3; i++) {
    if (ai_PokerTable->seat[i].foldedAt < 0) {
      ai_PokerTable->pot += ai_PokerTable->stake2;
      if (i == 0) {
        gm_PlayerFactor(ai_PokerTable->stake2, ai_OwnPlayer);
        cm_SendMoveGoodsUnchecked(-1, ai_PokerTable->seat[0].who->netId,
                                  ai_OwnPlayer, ai_PokerTable->stake2);
      }
      ai_PokerTable->seat[i].bet += ai_PokerTable->stake2;
    }
  }
}
/* 0x4a7ac0, picks each NPC's strategy (seat->strategy) from its hand: 3 nothing yet, 2 go for the rank, 1 pairs,
   0 random */

void ai_PokerPickStrategies(void) {
  G10SEAT *st;
  int cnt[6];
  int s, g, j, i, k, pairs, first;
  int n, maxn, maxr;

  first = 0;
  for (s = 1; s < 3; s++) {
    st = &ai_PokerTable->seat[s];
    pairs = 0;
    maxn = 0;
    maxr = 0;
    g = 0;
    for (k = 0; k < 24; k += 4) {
      n = 0;
      for (i = 0; i < 4; i++) {
        if (ai_PokerDeck[k + i].owner == s)
          n++;
      }
      if (n > 1)
        pairs++;
      if (n > maxn)
        maxn = n;
      cnt[g++] = n;
    }
    for (j = 0; j < 4; j++) {
      n = 0;
      for (i = 0; i < 24; i += 4) {
        if (ai_PokerDeck[i + j].owner == s)
          n++;
      }
      if (n > maxr) {
        maxr = n;
        first = j;
      }
      cnt[j] = n;
    }
    if (ai_PokerTable->dealt < 5 && pairs <= 1 && maxn < 2) {
      st->strategy = 3;
    } else if (ai_PokerTable->dealt == maxr) {
      st->strategy = 2;
    } else {
      if (ai_PokerTable->phase < 6 && ai_PokerTable->dealt - maxr <= 1) {
        for (j = 0; j < 6; j++)
          if (ai_PokerDeck[j * 4 + first].owner != s &&
              ai_PokerDeck[j * 4 + first].shown == 0)
            break;
        if (j == 6) {
          st->strategy = 2;
          continue;
        }
      }
      if (ai_PokerTable->dealt == 5 && maxn < 2)
        st->strategy = 0;
      else
        st->strategy = 1;
    }
  }
}
/* 0x4a7c20, each NPC decides whether to fold (seat->foldedAt = the phase) against the open cards of the others */

void ai_PokerNpcFold(void) {
  int s, j, suitN, suit, m, mySuit;
  int rankN, rank;
  G10SEAT *st, *o;

  for (s = 1; s < 3; s++) {
    st = &ai_PokerTable->seat[s];
    if (st->strategy == 0 && st->bold == 0 && ai_PokerTable->dealt >= 4)
      st->foldedAt = ai_PokerTable->phase;
    for (j = 0; j < 2; j++) {
      if (j != s && (o = &ai_PokerTable->seat[j])->foldedAt < 0) {
        ai_PokerOpenCounts(o, &suitN, &suit, &rankN, &rank);
        switch (st->strategy) {
        case 0:
          if (st->bold != 0) {
            if (suitN > 2)
              st->foldedAt = ai_PokerTable->phase;
          } else if (suitN > 1 || util_Random(4) == 1) {
            st->foldedAt = ai_PokerTable->phase;
          }
          break;
        case 1:
          if (st->bold != 0) {
            int any;

            m = ai_PokerFindPair(st->card, 0, 7, &any);
            if (m < suitN && ai_PokerTable->phase >= 6 &&
                ai_PokerTable->dealt >= 5)
              st->foldedAt = ai_PokerTable->phase;
          } else {
            m = ai_PokerFindPair(st->card, 0, 7, &mySuit);
            if (ai_PokerTable->phase >= 6 && m <= suitN &&
                (m < suitN || mySuit < suit))
              st->foldedAt = ai_PokerTable->phase;
          }
          break;
        case 2:
          if (st->bold == 0) {
            int any;

            m = ai_PokerFindPair(st->card, 0, 7, &any);
            if (ai_PokerTable->dealt < 5 && m == 3 && util_Random(4) == 1)
              st->foldedAt = ai_PokerTable->phase;
          }
          break;
        case 3:
          if (st->bold == 0) {
            int any;

            m = ai_PokerFindPair(st->card, 0, 7, &any);
            if (ai_PokerTable->dealt < 5 && m == 3 && util_Random(5) == 1)
              st->foldedAt = ai_PokerTable->phase;
          }
          break;
        }
      }
    }
  }
}
/* 0x4a7e70, counts the open cards (2 .. dealt-1) of seat s per suit and rank: the most common suit (count n, suit g)
   and rank (count m, rank r) */

void ai_PokerOpenCounts(G10SEAT *s, int *n, int *g, int *m, int *r) {
  int rcnt[4] = {0};
  int cnt[6] = {0};
  int i, k, dealt;

  dealt = ai_PokerTable->dealt;
  for (i = 2; i < dealt; i++) {
    k = s->card[i];
    rcnt[ai_PokerDeck[k].rank]++;
    cnt[ai_PokerDeck[k].suit]++;
  }
  *n = cnt[0];
  *g = 0;
  for (i = 1; i < 6; i++) {
    if (cnt[i] > *n) {
      *n = cnt[i];
      *g = i;
    }
  }
  *m = rcnt[0];
  *r = 0;
  for (i = 1; i < 4; i++) {
    if (rcnt[i] > *m) {
      *m = rcnt[i];
      *r = i;
    }
  }
}
/* 0x4a7f50, rates the hand of owner: 0 nothing, 1 pair, 2 two pairs, 3 three, 4 straight, 5 full house, 6 flush,
   7 poker, 8 straight flush (the names at 0x600490); the groups into g1 / g2, the highest ranks into r1 / r2 */

int ai_PokerRateHand(int owner, int *g1, int *g2, int *r1, int *r2) {
  int cnt[6];
  int rcnt[4];
  int g, r, n, i;
  int pairs, rpairs, maxn, maxr, bestr, kind, a, b;

  pairs = 0;
  rpairs = 0;
  maxn = 0;
  maxr = 0;
  bestr = 0;
  b = -1;
  kind = 0;
  a = 0;
  for (g = 0; g < 6; g++) {
    n = 0;
    for (i = 0; i < 4; i++) {
      if (ai_PokerDeck[g * 4 + i].owner == owner)
        n++;
    }
    cnt[g] = n;
    if (n > 1) {
      pairs++;
      if (n > maxn) {
        b = a;
        maxn = n;
        a = g;
      } else if (b < 0) {
        b = g;
      }
    }
  }
  for (r = 0; r < 4; r++) {
    n = 0;
    for (g = 0; g < 6; g++) {
      if (ai_PokerDeck[g * 4 + r].owner == owner)
        n++;
    }
    rcnt[r] = n;
    if (n > 1)
      rpairs++;
    if (n > maxr) {
      maxr = n;
      bestr = r;
    }
  }
  if (rpairs == 1 && maxr == 5) {
    kind = 6;
    *r1 = bestr;
  }
  if (pairs == 0) {
    if (cnt[0] == 0 || cnt[5] == 0)
      return kind == 6 ? 8 : 4;
    if (kind == 6)
      return 6;
  } else if (pairs == 1 && maxn == 4) {
    *g1 = a;
    return 7;
  } else if (pairs == 2 && maxn == 3) {
    *g1 = a;
    *g2 = b;
    return 5;
  } else if (pairs == 1 && maxn == 3) {
    *g1 = a;
    return 3;
  } else if (pairs == 2 && maxn == 2) {
    *g1 = a;
    *g2 = b;
    for (r = 3; r >= 0; r--) {
      if (ai_PokerDeck[a * 4 + r].owner == owner)
        *r1 = r;
      if (ai_PokerDeck[b * 4 + r].owner == owner)
        *r2 = r;
    }
    return 2;
  } else if (pairs == 1 && maxn == 2) {
    *g1 = a;
    for (r = 3; r >= 0; r--) {
      if (ai_PokerDeck[a * 4 + r].owner == owner) {
        *r1 = r;
        return 1;
      }
    }
    return 1;
  }
  return 0;
}
/* 0x4a8190, the showdown: the winning seat (seat 0 against 1, the better of them against 2), its hand into winHand;
   two empty hands against an empty seat 2 compare the high card of the winner with seat 1's (as the original does)
   @deep targeted check of the new C2 findings (source unchanged, 158 ins): none applies to the swap. (1)
   Id sort: the seat access is already the original's operand order, IV first, table second (c2ids:
   temp#21a key 18680 over ai_PokerTable#20 key 10400; ours [edi+ebx+0x2c], the original [ebx+edi+0x2c]
   with the registers swapped), so no id window is involved. (2) Forwarding: no sum with a block-dead local
   in the race. (3) The allocation queue (c2trace --raw): w 345 takes esi, the IV k#21a 76 (tie 2b) takes
   edi, the table 12 (tie 11) ebx, kind[2] ebp; 76 against 12 is no tie, so neither the tie field nor the
   declaration set reaches it. Measured: 23 probe counts (16..1072 dead 'if (0) ai_PokerHighCard(..);'
   lines in front of the inits) = control, one code. Two ways give the original (table edi, IV ebx), the
   second new: (A) the table above the IV (about +65: s74 proved it with four table reads in the init
   block, 118 ins left); (B) the IV still served first but with a score on edi: the choice takes the lowest
   score, ties esi, edi, ebx, ebp, and a waiting candidate that interferes and PREFERS edi adds one, so the
   IV would take ebx and the table edi. The table has prefs[ ] here; a preference comes only from a copy
   with a physical register or an assigned candidate (edi: the destination of an inline rep stos / movs, or
   a copy with a candidate already in edi), and the original function has no string instruction, so (B)
   needs a copy that leaves no code: not found. What the original's init block shows and ours lacks (under
   either way): xor ebp (kind[2]); xor ebx (the IV, in front of the stores); xor eax (kind[0]); mov ecx,ebp
   (kind[1] copied from kind[2]'s register, not a fresh xor); xor edx for out[2] and out[1]; out[0] stored
   from the IV's ebx; the table load last. Ours: one eax for kind[0] and all three out stores, xor ecx for
   kind[1], the IV's xor behind the stores. So the zero constant's pieces are spread by preferences the
   original's text gives (kind[1] from kind[2], out[0] from the IV) and ours does not. Kept facts of
   s72..s76: the first switch is the original's instruction for instruction with cases 1 / 2 as 'if (g1[0]
   > g1[1]) w = 0; else if (g1[1] > g1[0]) w = 1;' and the case order 1, 2, 3/7, 6, 8, 5, 4; = control or
   worse: seat pointer, hand store chained, arms swapped, loop over k or a block local i (154, frame 0x4c),
   while, w = 0 positions, out[0] = w, zero-init chains / loop / memset / = {0}, a local t = ai_PokerTable
   (155-158), 720 init orders and 5040 case orders under the probes (118), shared-compare forms (192-207),
   climb 4 rounds. Next: a text for the init block whose zero stores carry the original's copies (kind[1]
   <- kind[2], out[0] <- the seat offset), then the table / IV order again on that base. prior: s73 (158
   ins, source unchanged): the register swap is an order-of-service question. c2trace: w#1 (345) takes esi,
   the seat-offset IV k#21a (76: +18 def in the init block P9, +12 foldedAt, +28 the hand store in the
   post-call block P7, +32 latch) takes edi, the table ai_PokerTable#1 (12) takes ebx, kind[2] ebp; the
   choice order is esi, edi, ebx, ebp (probe: five '{ static G10GAME *pm; pm = ai_PokerTable; }' in the
   first loop lift the table to 61, it is served before the IV and takes edi). The table is at +40 after
   the first loop and loses about 58 over the ~27 unreferenced blocks of the first switch (-1..-3 each);
   the original pays the same (one table load before the loop, one reload after RateHand, none after the
   HighCard calls: same-unit callee known not to write it). So in the original the IV ranks below the
   table, or the table has references the text here lacks. = control / worse this session: a seat pointer
   st (top / folded arm / hand store: folds into the IV), the hand store chained either way or via k (=
   control / 170), the arms swapped (164), the folded arm as 'else if (foldedAt >= 0)' (= control), two ifs
   (192), out[w] as the flag (270), the loop over k (154 but frame 0x4c, same swap), 'w <= 2' / 'w != 3'
   (160), while (= control), w = 0 at every position among the inits with 'for (; w < 3; w++)' and 'out[0]
   = w' (= control), a label / do { } while (0) around the inits (= control). flip --want 'mov edi, DWORD
   PTR _ai_PokerTable': only meaning changes (a second RateHand call, a winHand store moved above 'if
   (out[2] != 0)' or 'if (k == 0)'); flip --want 'add ebx, 52': no hit. climb 4 rounds (move, swap, arms,
   ifelse, init): no move. Next: what lowers the IV by ~65 (its def / hand-store / latch blocks) or what
   plain text gives the table more weight in the first loop. s74: the service order is now PROVEN: four '{
   static G10GAME *pm; pm = ai_PokerTable; }' in the init block lift the table to 89 against the IV's 79,
   and the table takes edi, the IV ebx, as the original (158 -> 118, the probes' own code left; probes are
   no final form). Each table reference in the init block is worth about +19 (P9), one in the loop +12 /
   +28; the original needs about +65 for the table or as much less for the IV (def +18 in the init block,
   foldedAt +12, hand store +28 in the post-call block P7, latch +32). The table's foldedAt read in the
   loop head is not credited to ai_PokerTable#1 (it pays -8 there). = control / no swap this session:
   'out[w] = foldedAt >= 0' in the else arm, foldedAt into k first (= control); case 6 / 8 grouped (195,
   table 13); hand stored first (170: IV 99). Next: an ordinary statement before the first loop (or in it)
   that reads the table, or a loop form whose IV has no def in the init block. s75: the swap is not the
   only difference: with the init-block probes (table edi, IV ebx) 118 ins remain (besides the probes' four
   stores): the zero sources of the inits (original: kind[1] = mov ecx,ebp, out[0] from the IV's xor ebx,
   out[2] / out[1] from a fresh edx; ours all from eax), the both-zero HighCard path's setle cl (ours dl),
   and the first switch's 'w = g1[0] <= g1[1]' / 'r1' cases loading and jumping into ONE shared 'xor edx;
   cmp eax,ecx; setle dl; mov esi,edx' tail. Under the probes: all 720 init orders 118 (none better), all
   5040 orders of the seven first-switch case groups 118. Natural table forms tried: a local 't =
   ai_PokerTable' through the whole function / the first loop / after the loop (155-158: no reload after
   RateHand, the original reloads), the loop counter as a block local i (154, frame 0x4c, same swap), while
   (= control), the bound as sizeof(seat) / sizeof(seat[0]) (160). G10GAME has no seat count. s76: the
   shared-compare form (the g1 / r1 cases set 'x = g1[0]; y = g1[1]; cmp = 1;', one 'if (cmp != 0) w = x <=
   y;' after the switch or inside its else, cmp = 0 at the top or before the switch): 192-207, the table
   drops to prio 0 (ebp), the IV stays 76 (edi): no lever, and so the init / case-order rounds were not
   rerun on it. The original's one shared 'cmp; setle; mov esi' tail is VC6's cross-jumping of the separate
   'w = g1[0] <= g1[1]' / 'w = r1[0] <= r1[1]' cases, which needs equal scratch registers in every copy: it
   follows from the allocation, not from the text. Next: the table / IV order remains the key (with it the
   init zeros and the HighCard setle register are next); look for a plain statement in the init block or
   first loop that reads the table (worth +19 / +12..28 each, about +65 needed). */

int ai_PokerShowdown(void) {
  int g1[3], r1[3], kind[3], out[3], g2[3];
  int r2[3];
  int w, k; /* w: the seat, first the loop over the seats, then the winner */

  kind[0] = 0;
  kind[2] = 0;
  kind[1] = 0;
  out[0] = 0;
  out[2] = 0;
  out[1] = 0;
  for (w = 0; w < 3; w++) {
    if (ai_PokerTable->seat[w].foldedAt < 0) {
      kind[w] = ai_PokerRateHand(w, &g1[w], &g2[w], &r1[w], &r2[w]);
      ai_PokerTable->seat[w].hand = kind[w];
    } else {
      out[w] = 1;
    }
  }
  if (out[0] != 0) {
    w = 1;
  } else if (out[1] != 0) {
    w = 0;
  } else if (kind[0] == 0) {
    if (kind[1] == 0)
      w = ai_PokerHighCard(ai_PokerTable->seat[0].card, 0) <=
          ai_PokerHighCard(ai_PokerTable->seat[1].card, 0);
    else
      w = 1;
  } else if (kind[1] == 0) {
    w = 0;
  } else if (kind[1] < kind[0]) {
    w = 0;
  } else if (kind[1] > kind[0]) {
    w = 1;
  } else {
    switch (kind[0]) {
    case 1:
      if (g1[0] > g1[1])
        w = 0;
      else if (g1[1] > g1[0])
        w = 1;
      else
        w = r1[0] <= r1[1];
      break;
    case 2:
      if (g1[0] > g1[1])
        w = 0;
      else if (g1[1] > g1[0])
        w = 1;
      else if (g2[0] > g2[1])
        w = 0;
      else if (g2[1] > g2[0])
        w = 1;
      else
        w = r1[0] <= r1[1];
      break;
    case 3:
    case 7:
      w = g1[0] <= g1[1];
      break;
    case 6:
      w = r1[0] <= r1[1];
      break;
    case 8:
      w = r1[0] <= r1[1];
      break;
    case 5:
      w = g1[0] <= g1[1];
      break;
    case 4:
      w = ai_PokerHighCard(ai_PokerTable->seat[0].card, 0) <=
          ai_PokerHighCard(ai_PokerTable->seat[1].card, 0);
      break;
    }
  }
  if (out[2] != 0) {
    ai_PokerTable->winHand = kind[w];
    return w;
  }
  if (kind[w] == 0 && kind[2] == 0) {
    if (ai_PokerHighCard(ai_PokerTable->seat[w].card, 0) <=
        ai_PokerHighCard(ai_PokerTable->seat[1].card, 0))
      w = 1;
    return w;
  }
  k = kind[w];
  if (k == 0) {
    ai_PokerTable->winHand = kind[2];
    return 2;
  }
  if (kind[2] != 0 && kind[2] >= k) {
    if (kind[2] > k) {
      w = 2;
      ai_PokerTable->winHand = kind[w];
      return w;
    }
    switch (k) {
    case 1:
      if (g1[2] >= g1[w]) {
        if (g1[2] > g1[w]) {
          ai_PokerTable->winHand = kind[2];
          return 2;
        }
        if (r1[w] <= r1[2]) {
          w = 2;
          ai_PokerTable->winHand = kind[w];
          return w;
        }
      }
      break;
    case 2:
      if (g1[2] >= g1[w]) {
        if (g1[2] > g1[w]) {
          ai_PokerTable->winHand = kind[2];
          return 2;
        }
        if (g2[2] >= g2[w]) {
          if (g2[2] > g2[w]) {
            ai_PokerTable->winHand = kind[2];
            return 2;
          }
          if (r1[w] <= r1[2]) {
            w = 2;
            ai_PokerTable->winHand = kind[w];
            return w;
          }
        }
      }
      break;
    case 3:
    case 5:
    case 7:
      if (g1[w] <= g1[2]) {
        ai_PokerTable->winHand = kind[2];
        return 2;
      }
      break;
    case 4:
      if (ai_PokerHighCard(ai_PokerTable->seat[w].card, 0) <=
          ai_PokerHighCard(ai_PokerTable->seat[2].card, 0))
        w = 2;
      ai_PokerTable->winHand = kind[w];
      return w;
    case 6:
    case 8:
      if (r1[w] <= r1[2]) {
        w = 2;
        ai_PokerTable->winHand = kind[w];
        return w;
      }
      break;
    default:
      return w;
    }
  }
  ai_PokerTable->winHand = k;
  return w;
}
/* 0x4a8520, deals three cards to each seat (the first two hidden, the third open) */

void ai_PokerDealThree(void) {
  int k, i;

  for (k = 0; k < 3; k++) {
    for (i = 0; i < 3; i++) {
      ai_PokerTable->seat[i].card[k] = ai_PokerDrawCard(i);
      if (k > 1)
        ai_PokerDeck[ai_PokerTable->seat[i].card[k]].shown = 1;
      else
        ai_PokerDeck[ai_PokerTable->seat[i].card[k]].shown = 0;
    }
  }
  ai_PokerTable->dealt += 3;
}
/* 0x4a8590, deals the fourth card to each seat, open */

void ai_PokerDealFourth(void) {
  int i;

  for (i = 0; i < 3; i++) {
    ai_PokerTable->seat[i].card[3] = ai_PokerDrawCard(i);
    ai_PokerDeck[ai_PokerTable->seat[i].card[3]].shown = 1;
  }
  ai_PokerTable->dealt++;
}
/* 0x4a85e0, deals the fifth card to each seat, open */

void ai_PokerDealFifth(void) {
  int i;

  for (i = 0; i < 3; i++) {
    ai_PokerTable->seat[i].card[4] = ai_PokerDrawCard(i);
    ai_PokerDeck[ai_PokerTable->seat[i].card[4]].shown = 1;
  }
  ai_PokerTable->dealt++;
}
/* 0x4a8630, the player exchanges card k */

void ai_PokerExchange(int k) { ai_PokerSwapCard(&ai_PokerTable->seat[0], k); }
/* 0x4a8650, the ante: three stakes into the pot, the player's from p's money */

void ai_PokerAnte(void) {
  ai_PokerTable->pot = ai_PokerTable->stake2 * 3;
  cm_SendMoveGoodsUnchecked(-1, ai_PokerTable->seat[0].who->netId, ai_OwnPlayer,
                            ai_PokerTable->stake2);
}
/* 0x4a8690, ends the round: the winner into winner, the pot to the player if he won, all cards back */

void ai_PokerEndRound(void) {
  int i;

  ai_PokerTable->winner = (short)ai_PokerShowdown();
  if (ai_PokerTable->winner == 0)
    cm_SendMoveGoodsUnchecked(ai_PokerTable->seat[0].who->netId, -1,
                              ai_OwnPlayer, ai_PokerTable->pot);
  for (i = 0; i < 24; i++) {
    ai_PokerDeck[i].owner = 3;
    ai_PokerDeck[i].shown = 0;
  }
}
/* 0x4a86f0, the NPC whose turn it is (turn) sets the stake: up to half the margin above the stake, or all of it
   if it bluffs */

void ai_PokerNpcSetStake(void) {
  if (ai_PokerTable->turn != 0) {
    if (ai_PokerTable->seat[ai_PokerTable->turn].bold != 0)
      ai_PokerTable->stake2 =
          util_Random(ai_PokerTable->money - ai_PokerTable->stake) +
          ai_PokerTable->stake;
    else
      ai_PokerTable->stake2 =
          util_Random(ai_PokerTable->money - ai_PokerTable->stake >> 1) +
          ai_PokerTable->stake;
  }
}

/* the poker unit's one .rdata constant: it precedes the unit's literals (77.0f at 0x5ed1c0, ...), after am's */
const int ai_CardBack =
    0x18; /* 0x5ed1bc, the picture offset of the card back (d2_Pics[0x775 + ai_CardBack], ai_PokerSeatPanel) */
char ai_PokerHandNames[9][0x100] = {
    "H\xf6"
    "chste Karte",
    "Paar",
    "2 Paar",
    "Dreier",
    "Strasse",
    "Fullhouse",
    "Flush",
    "Poker",
    "Royal Flush"}; /* 0x600490, the hand names by ai_PokerRateHand's rank 0 .. 8; no reader in the image: the first datum of the unit's .data block, after am's strings */
G10GAME *ai_PokerTable;      /* 0xf83654 */
G10CARD ai_PokerDeck[24];    /* 0xf83660 */
AICHAR ai_PokerOpponents[2]; /* 0xf837e0 */
