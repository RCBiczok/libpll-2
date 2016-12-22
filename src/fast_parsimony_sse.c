/*
    Copyright (C) 2016 Tomas Flouri

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU Affero General Public License as
    published by the Free Software Foundation, either version 3 of the
    License, or (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Affero General Public License for more details.

    You should have received a copy of the GNU Affero General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.

    Contact: Tomas Flouri <Tomas.Flouri@h-its.org>,
    Exelixis Lab, Heidelberg Instutute for Theoretical Studies
    Schloss-Wolfsbrunnenweg 35, D-69118 Heidelberg, Germany
*/

#include "pll.h"

PLL_EXPORT
unsigned int pll_fastparsimony_edge_score_4x4_sse(pll_parsimony_t * parsimony,
                                                  unsigned int node1_score_index,
                                                  unsigned int node2_score_index)
{
  unsigned int i;

  unsigned int bits[16] __attribute__ ((aligned(PLL_ALIGNMENT_SSE)));

  unsigned int * node1[4];
  unsigned int * node2[4];

  unsigned int ** vector = parsimony->packedvector;
  int vector_count = parsimony->packedvector_count;

  int score = 0;

  /* point to the parsimony vectors for each node and for each state */
  for (i = 0; i < 4; ++i)
  {
    node1[i] = vector[node1_score_index] + i*vector_count;
    node2[i] = vector[node2_score_index] + i*vector_count;
  }

  __m128i xmm0,xmm1,xmm2,xmm3,xmm4,xmm5,xmm6,xmm7;

  /* set all bits to one */
  xmm7 = _mm_set1_epi32(0xFFFFFFFF);

  for (i = 0; i < parsimony->packedvector_count; i += 4)
  {
    /* load, and, or bit vectors for state 0 */
    xmm0 = _mm_load_si128((__m128i *)(node1[0]+i));
    xmm1 = _mm_load_si128((__m128i *)(node2[0]+i));

    xmm2 = _mm_and_si128(xmm0,xmm1);

    /* load, and, or bit vectors for state 1 */
    xmm0 = _mm_load_si128((__m128i *)(node1[1]+i));
    xmm1 = _mm_load_si128((__m128i *)(node2[1]+i));

    xmm3 = _mm_and_si128(xmm0,xmm1);

    /* load, and, or bit vectors for state 2 */
    xmm0 = _mm_load_si128((__m128i *)(node1[2]+i));
    xmm1 = _mm_load_si128((__m128i *)(node2[2]+i));

    xmm4 = _mm_and_si128(xmm0,xmm1);

    /* load, and, or bit vectors for state 3 */
    xmm0 = _mm_load_si128((__m128i *)(node1[3]+i));
    xmm1 = _mm_load_si128((__m128i *)(node2[3]+i));

    xmm5 = _mm_and_si128(xmm0,xmm1);


    /* OR the ANDs of states 0 and 1 */
    xmm0 = _mm_or_si128(xmm2,xmm3);
    /* OR the ANDs of states 2 and 3 */
    xmm1 = _mm_or_si128(xmm4,xmm5);
    /* OR The two vectors */
    xmm6 = _mm_or_si128(xmm0,xmm1);


    xmm0 = _mm_andnot_si128(xmm6, xmm7);

    _mm_store_si128((__m128i *)bits, xmm0);

    score += __builtin_popcount(bits[0]);
    score += __builtin_popcount(bits[1]);
    score += __builtin_popcount(bits[2]);
    score += __builtin_popcount(bits[3]);
  }

  unsigned int score1 = parsimony->node_cost[node1_score_index];
  unsigned int score2 = parsimony->node_cost[node2_score_index];

  return score+score1+score2+parsimony->const_cost;
}

PLL_EXPORT
void pll_fastparsimony_update_vectors_4x4_sse(pll_parsimony_t * parsimony,
                                              const pll_pars_buildop_t * op,
                                              int count)
{
  unsigned int i;

  unsigned int bits[16] __attribute__ ((aligned(PLL_ALIGNMENT_SSE)));

  unsigned int * parent[4];
  unsigned int * child1[4];
  unsigned int * child2[4];

  unsigned int ** vector = parsimony->packedvector;
  int vector_count = parsimony->packedvector_count;

  int score = 0;

  /* point to the parsimony vectors for each node and for each state */
  for (i = 0; i < 4; ++i)
  {
    parent[i] = vector[op->parent_score_index] + i*vector_count;
    child1[i] = vector[op->child1_score_index] + i*vector_count;
    child2[i] = vector[op->child2_score_index] + i*vector_count;
  }

  __m128i xmm0,xmm1,xmm2,xmm3,xmm4,xmm5,xmm6,xmm7,xmm8,xmm9,xmm10,xmm11;

  /* set all bits to one */
  xmm11 = _mm_set_epi32(0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF);

  for (i = 0; i < parsimony->packedvector_count; i += 4)
  {
    /* load, and, or bit vectors for state 0 */
    xmm0 = _mm_load_si128((__m128i *)(child1[0]+i));
    xmm1 = _mm_load_si128((__m128i *)(child2[0]+i));

    xmm2 = _mm_and_si128(xmm0,xmm1);
    xmm3 = _mm_or_si128(xmm0,xmm1);

    /* load, and, or bit vectors for state 1 */
    xmm0 = _mm_load_si128((__m128i *)(child1[1]+i));
    xmm1 = _mm_load_si128((__m128i *)(child2[1]+i));

    xmm4 = _mm_and_si128(xmm0,xmm1);
    xmm5 = _mm_or_si128(xmm0,xmm1);

    /* load, and, or bit vectors for state 2 */
    xmm0 = _mm_load_si128((__m128i *)(child1[2]+i));
    xmm1 = _mm_load_si128((__m128i *)(child2[2]+i));

    xmm6 = _mm_and_si128(xmm0,xmm1);
    xmm7 = _mm_or_si128(xmm0,xmm1);

    /* load, and, or bit vectors for state 3 */
    xmm0 = _mm_load_si128((__m128i *)(child1[3]+i));
    xmm1 = _mm_load_si128((__m128i *)(child2[3]+i));

    xmm8 = _mm_and_si128(xmm0,xmm1);
    xmm9 = _mm_or_si128(xmm0,xmm1);

    
    /* OR the ANDs of states 0 and 1 */
    xmm0 = _mm_or_si128(xmm2,xmm4);
    /* OR the ANDs of states 2 and 3 */
    xmm1 = _mm_or_si128(xmm6,xmm8);
    /* OR The two vectors */
    xmm10 = _mm_or_si128(xmm0,xmm1);


    /* store them */
    xmm0 = _mm_andnot_si128(xmm10,xmm3);
    xmm1 = _mm_or_si128(xmm2,xmm0);
    _mm_store_si128((__m128i *)(parent[0]+i),xmm1);

    xmm0 = _mm_andnot_si128(xmm10,xmm5);
    xmm1 = _mm_or_si128(xmm4,xmm0);
    _mm_store_si128((__m128i *)(parent[1]+i),xmm1);

    xmm0 = _mm_andnot_si128(xmm10,xmm7);
    xmm1 = _mm_or_si128(xmm6,xmm0);
    _mm_store_si128((__m128i *)(parent[2]+i),xmm1);

    xmm0 = _mm_andnot_si128(xmm10,xmm9);
    xmm1 = _mm_or_si128(xmm8,xmm0);
    _mm_store_si128((__m128i *)(parent[3]+i),xmm1);


    xmm0 = _mm_andnot_si128(xmm10, xmm11);

    _mm_store_si128((__m128i *)bits, xmm0);

    score += __builtin_popcount(bits[0]);
    score += __builtin_popcount(bits[1]);
    score += __builtin_popcount(bits[2]);
    score += __builtin_popcount(bits[3]);
  }

  unsigned int score1 = parsimony->node_cost[op->child1_score_index];
  unsigned int score2 = parsimony->node_cost[op->child2_score_index];

  parsimony->node_cost[op->parent_score_index] = score+score1+score2;
}
